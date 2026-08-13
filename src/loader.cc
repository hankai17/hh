#include "loader.hh"
#include "common.hh"
#include "location.hh"
#include "parser.hh"
#include "syntax.hh"
#include "option.hh"
#include "compiler.hh"

#include <algorithm>
#include <sysexits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <iostream>
#include <cassert>
#include <string.h>
#include <unordered_map>
#include <functional>
#include <time.h>

//#define DEBUG_ON 0

static std::map<std::pair<dev_t, ino_t>, Module> inode2module;
static std::unordered_map<DefineStmt*, std::vector<DefineStmt*>> depended_by;
std::map<DefineStmt*, std::vector<Expr*>> used_as_call;
std::map<DefineStmt*, std::vector<Expr*>> used_as_collapse;
std::map<DefineStmt*, std::vector<Expr*>> used_as_embed;
static DefineStmt *main_export;
Module *main_module;
FILE *output;
FILE *output_header;

long load(const char *filename);
Module *load_module(const char *filename);
void unload_all();

void print_module_info(Module &mod) {
    printf("filename: %s\n", mod.filename.c_str());
    puts("qualified imports:");
    for (auto &x : mod.qualified_import) {
        printf("    %s as %s\n",
            x.second->filename.c_str(),
            x.first.c_str());
    }
    puts("unqualified imports:");
    for (auto &x : mod.unqualified_import) {
        printf("    %s\n", x->filename.c_str());
    }
    puts("defined action:");
    for (auto &x : mod.defined_action) {
        printf("    %s\n", x.first.c_str());
    }
    puts("defined:");
    for (auto &x : mod.defined) {
        printf("    %s\n", x.first.c_str());
    }
}

Stmt *resolve(Module &mod, const std::string qualified,
        const std::string& ident) {
    if (qualified.size())  {
        if (!mod.qualified_import.count(qualified)) {
            return NULL;
        }
        auto it = mod.qualified_import[qualified]->defined.find(ident);
        if (it == mod.qualified_import[qualified]->defined.end()) {
            return NULL;
        }
        return it->second;
    } else {
        Stmt *r = NULL;
        if (mod.macro.count(ident)) {
            r = mod.macro[ident];
        }
        if (mod.defined.count(ident)) {
            if (r) {
                return (Stmt*)1;
            }
            r = mod.defined[ident];
        }
        for (auto *import : mod.unqualified_import) {
            if (import->macro.count(ident)) {
                if (r) {
                    return (Stmt*)1;
                }
                r = import->macro[ident];
            }
            if (import->defined.count(ident)) {
                if (r) {
                    return (Stmt*)1;
                }
                r = import->defined[ident];
            }
        }
        return r;
    }
}

ActionStmt *resolve_action(Module &mod, const std::string qualified,
        const std::string &ident) {
    if (qualified.size()) {
        if (!mod.qualified_import.count(qualified)) {
            return NULL;
        }
        auto it = mod.qualified_import[qualified]->defined_action.find(ident);
        if (it == mod.qualified_import[qualified]->defined_action.end()) {
            return NULL;
        }
        return it->second;
    } else {
        ActionStmt *r = NULL;
        if (mod.defined_action.count(ident)) {
            r = mod.defined_action[ident];
        }
        for (auto *import : mod.unqualified_import) {
            if (import->defined_action.count(ident)) {
                if (r) {
                    return (ActionStmt*)1;
                }
                r = import->defined_action[ident];
            }
        }
        return r;
    }
}

struct ModuleImportDef : PreorderStmtVisitor {
    Module &mod;
    long &n_errors;

    ModuleImportDef(Module &mod, long &n_errors) :
        mod(mod),
        n_errors(n_errors) {
    }

    void visit(ActionStmt &stmt) override {
#ifdef DEBUG_ON
        std::cout << "visit ModuleImportDef:PreorderStmtVisitor ActionStmt\n" << std::endl;
#endif
        if (mod.defined_action.count(stmt.ident)) {
            n_errors++;
            mod.locfile.locate(stmt.loc, "Redefined '%s'\n", stmt.ident.c_str());
        }
        mod.defined_action[stmt.ident] = &stmt;
    }

    void visit(DefineStmt &stmt) override {                             // 2.1 接普通语句
#ifdef DEBUG_ON
        std::cout << "visit ModuleImportDef:PreorderStmtVisitor DefineStmt\n" << std::endl;
#endif
        if (mod.defined.count(stmt.lhs) || mod.macro.count(stmt.lhs)) {
            n_errors++;
            mod.locfile.locate(stmt.loc, "Redefined '%s'\n", stmt.lhs.c_str());
        } else {
            mod.defined.emplace(stmt.lhs, &stmt);                       // 2.1.1
            stmt.module = &mod;
            depended_by[&stmt];
        }
    }

    void visit(ImportStmt &stmt) override {                             // 2.2 接import语句
#ifdef DEBUG_ON
        std::cout << "visit ModuleImportDef:PreorderStmtVisitor ImportStmt\n" << std::endl;
        std::cout << "before load_module...\n" << std::endl;
#endif
        Module *m = load_module(n_errors, stmt.filename);               // 2.2.1 边消费边生产模型
#ifdef DEBUG_ON
        std::cout << "after load_module...\n" << std::endl;
#endif
        if (!m) {
            n_errors++;
            mod.locfile.locate(stmt.loc, "'%s' : %s", stmt.filename.c_str(),
                    errno ? strerror(errno) : "parse error");
            return;
        }
        if (stmt.qualified.size()) {                                    // 2.2.2 加载新文件 保存到当前mod里
            mod.qualified_import[stmt.qualified] = m;
        } else if (std::count(ALL(mod.unqualified_import), m) == 0) {
            mod.unqualified_import.push_back(m);
        }
    }

    void visit(PreprocessDefineStmt &stmt) override {
#ifdef DEBUG_ON
        std::cout << "visit ModuleImportDef:PreorderStmtVisitor PreprocessDefineStmt\n" << std::endl;
#endif
        if (mod.defined.count(stmt.ident) || mod.macro.count(stmt.ident)) {
            n_errors++;
            mod.locfile.locate(stmt.loc, "Redefined '%s'\n", stmt.ident.c_str());
        } else {
            mod.macro[stmt.ident] = &stmt;
        }
    }
};

struct ModuleUse : PrePostActionExprStmtVisitor {
    Module &mod;
    long &n_errors;
    DefineStmt *stmt = NULL;

    ModuleUse(Module &mod, long &n_errors) :
        mod(mod),
        n_errors(n_errors) {
    }

    void pre_expr(Expr &expr) override {
        expr.stmt = stmt;
    }

    void post_expr(Expr &expr) override {
        for (auto a : expr.entering) {
            PrePostActionExprStmtVisitor::visit(*a.first);
        }
        for (auto a : expr.finishing) {
            PrePostActionExprStmtVisitor::visit(*a.first);
        }
        for (auto a : expr.leaving) {
            PrePostActionExprStmtVisitor::visit(*a.first);
        }
        for (auto a : expr.transiting) {
            PrePostActionExprStmtVisitor::visit(*a.first);
        }
    }

    void visit(RefAction &action) override {
        ActionStmt *r = resolve_action(mod, action.qualified, action.ident);
        if (!r) {
            n_errors++;
            if (action.qualified.size()) {
                mod.locfile.locate(action.loc, "'%s::%s': Undefined \n", action.qualified.c_str(), action.ident.c_str());
            } else {
                mod.locfile.locate(action.loc, "'%s': Undefined \n", action.ident.c_str());
            }
        } else if (r == (Stmt*)1) {
            n_errors++;
            mod.locfile.locate(action.loc, "'%s' redefined\n", action.ident.c_str());
        } else {
            action.define_stmt = r;
        }
    }

    void visit(BracketExpr &expr) override {
        for (auto &x : expr.intervals.to) {
            AB = std::max(AB, x.second);
        }
    }

    void visit(CallExpr &expr) override {
#ifdef DEBUG_ON
        std::cout << "visit ModuleUse:PreorderActionExprStmtVisitor CallExpr\n" << std::endl;
#endif
        Stmt *r = resolve(mod, expr.qualified, expr.ident);
        if (!r) {
            error_undefined(expr.loc, expr.qualified, expr.ident) ;
        } else if (r == (Stmt*)1) {
            error_ambiguous(expr.loc, expr.ident);
        } else if (dynamic_cast<PreprocessDefineStmt*>(r)) {
            error_misuse_macro("CallExpr", expr.loc, expr.qualified, expr.ident);
        } else if (auto d = dynamic_cast<DefineStmt*>(r)) {
            used_as_call[d].push_back(&expr);               // 跟depended_by 类似
            expr.define_stmt = d;
        } else {
            assert(0);
        }
    }

    void visit(CollapseExpr &expr) override {
#ifdef DEBUG_ON
        std::cout << "visit ModuleUse:PreorderActionExprStmtVisitor CollapseExpr\n" << std::endl;
#endif
        Stmt *r = resolve(mod, expr.qualified, expr.ident);
        if (!r) {
            error_undefined(expr.loc, expr.qualified, expr.ident) ;
        } else if (r == (Stmt*)1) {
            error_ambiguous(expr.loc, expr.ident);
        } else if (dynamic_cast<PreprocessDefineStmt*>(r)) {
            error_misuse_macro("CollapseExpr", expr.loc, expr.qualified, expr.ident);
        } else if (auto d = dynamic_cast<DefineStmt*>(r)) {
            used_as_collapse[d].push_back(&expr);
            expr.define_stmt = d;
        } else {
            assert(0);
        }
    }

    void visit(DefineStmt &stmt) override {
#ifdef DEBUG_ON
        std::cout << "visit ModuleUse:PreorderActionExprStmtVisitor DefineStmt\n" << std::endl;
#endif
        this->stmt = &stmt;
        PrePostActionExprStmtVisitor::visit(*stmt.rhs);
        this->stmt = NULL;
    }

    void visit(EmbedExpr &expr) override {
#ifdef DEBUG_ON
        std::cout << "visit ModuleUse:PreorderActionExprStmtVisitor EmbedExpr\n" << std::endl;
#endif
        Stmt *r = resolve(mod, expr.qualified, expr.ident);
        if (!r) {
            error_undefined(expr.loc, expr.qualified, expr.ident) ;
        } else if (r == (Stmt*)1) {
            error_ambiguous(expr.loc, expr.ident);
        } else if (auto d = dynamic_cast<PreprocessDefineStmt*>(r)) {
            expr.define_stmt = NULL;
            expr.macro_value = d->value;
            AB = std::max(AB, d->value + 1);
        } else if (auto d = dynamic_cast<DefineStmt*>(r)) {
            depended_by[d].push_back(stmt);
            used_as_embed[d].push_back(&expr);
            expr.define_stmt = d;
        } else {
            assert(0);
        }
    }
private:
    void error_undefined(const Location &loc,
            const std::string &qualified, const std::string &ident) {
        n_errors++;
        if (qualified.size()) {
            mod.locfile.locate(loc, "%s::%s undefined", qualified.c_str(), ident.c_str());
        } else {
            mod.locfile.locate(loc, "%s undefined", ident.c_str());
        }
    }
    void error_ambiguous(const Location &loc, const std::string &ident) {
        n_errors++;
        mod.locfile.locate(loc, "%s ambiguous", ident.c_str());
    }
    void error_misuse_macro(const char *name, const Location &loc,
            const std::string &qualified, const std::string &ident) {
        n_errors++;
        if (qualified.size()) {
            mod.locfile.locate(loc, "macro %s::%s used as %s", qualified.c_str(), ident.c_str(), name);
        } else {
            mod.locfile.locate(loc, "macro %s used as %s", ident.c_str(), name);
        }
    }
};

Module *load_module(long &n_errors, const std::string &filename) {
    FILE *file = stdin;
    std::pair<dev_t, ino_t> inode {0, 0};

    if (filename != "-") {
        file = fopen(filename.c_str(), "r");
    }
    if (!file) {
        n_errors++;
        return NULL;
    }
    if (file != stdin) {
        struct stat st;
        if (fstat(fileno(file), &st) < 0) {
            err_exit(EX_OSFILE, "fstat  '%s'", filename.c_str());
        }
        inode = { st.st_dev, st.st_ino };
    }
    if (inode2module.count(inode)) {
        fclose(file);
        return &inode2module[inode];
    }

    Module &mod = inode2module[inode];
    std::string module { file != stdin ? filename : "main" };
    std::string::size_type t = module.find('.');
    if (t != std::string::npos) {
        module.erase(t, module.size() - t);
    }

    size_t r;
    char buf[BUF_SIZE];
    std::string data;

    while ((r = fread(buf, 1, sizeof(buf), file)) > 0) {
        data += std::string(buf, buf + r);
        if (r < sizeof(buf)) break;
    }
    fclose(file);
    if (data.empty() || data.back() != '\n') {
        data.push_back('\n');
    }

    LocationFile locfile(filename, data);
    mod.locfile = locfile;
    mod.filename = filename;
    Stmt *toplevel = NULL;
    long errors = parse(locfile, toplevel);
    if (!toplevel) {
        n_errors += errors;
        mod.toplevel = NULL;
        mod.status = BAD;
        return &mod;
    }
    mod.toplevel = toplevel;
    return &mod;
}

static std::vector<DefineStmt *> topo_define_stmts(long &n_errors) {
    std::vector<DefineStmt*> topo;
    std::vector<DefineStmt*> st;
    std::unordered_map<DefineStmt*, i8> vis;
    std::unordered_map<DefineStmt*, long> cnt;
    std::function<bool(DefineStmt *)> dfs = [&] (DefineStmt *u) {
        if (vis[u] == 2) {  // visited
            return false;
        }
        if (vis[u] == 3) {  // in cycle
            return true;
        }
        if (vis[u] == 1) {  // in stack
            u->module->locfile.locate(u->loc, "'%s': circular embedding", u->lhs.c_str());
            size_t i = st.size();
            while (st[i - 1] != u) {
                i--;
            }
            st.push_back(st[i - 1]);
            for (; i < st.size(); i++) {
                vis[st[i]] = 3;
                st[i]->module->locfile.locate(st[i]->loc, "required by %s",
                        st[i]->lhs.c_str());
            }
            fputs("\n", stderr);
            return true;
        }
        cnt[u] = u->export_ ? 1 : 0;
        vis[u] = 1;
        st.push_back(u);
        bool cycle = false;
        for (auto v : depended_by[u]) {
            if (dfs(v)) {
                cycle = true;
            } else {
                cnt[u] += cnt[v];
            }
        }
        st.pop_back();
        vis[u] = 2;
        topo.push_back(u);
        return cycle;
    };
    for (auto &d : depended_by) {
        if (!d.second.size() &&
                !d.first->export_ &&
                !used_as_collapse[d.first].size()) {
            printf("stmt: %s maybe not used\n", d.first->lhs.c_str());
        }
        if (!vis[d.first] && dfs(d.first)) {
            n_errors++;
        }
    }
    std::reverse(ALL(topo));
    if (opt_dump_embed) {
        printf("\n====== Embed size: %ld\n", depended_by.size());
        for (auto stmt : topo) {
            if (cnt[stmt] > 0) {
                printf("count(%s::%s) = %ld\n",
                        stmt->module->filename.c_str(),
                        stmt->lhs.c_str(),
                        cnt[stmt]);
            }
        }
    }
    return topo;
}

long load(const std::string &filename) {
    long n_errors = 0;

    Module *mod = load_module(n_errors, filename);
    if (!mod) {
        err_exit(EX_OSFILE, "fopen", filename.c_str());
        return n_errors;
    }
    main_module = mod;

    printf("\nProcessing import & def\n");
    for (;;) {
        bool done = true;
        for (auto &it : inode2module) {                     // hh2 ModuleImportDef(能接所有语句) 根据main的AST 构建所有文件的AST
            if (it.second.status == UNPROCESSED) {
                done = false;
                Module &mod = it.second;
                mod.status = GOOD;
                long old = n_errors;
                ModuleImportDef p { mod, n_errors };
                for (Stmt *s = mod.toplevel; s; s= s->next) {
                    s->accept(p);
                }
                mod.status = old == n_errors ? GOOD : BAD;
            }
        }
        if (done) {
            break;
        }
    }
    if (n_errors) {
        return n_errors; 
    }

    printf("\nProcessing use\n");
    for (auto &it : inode2module) {
        if (it.second.status == GOOD) {
            Module &mod = it.second;
            ModuleUse p { mod, n_errors };                  // hh3 检查变量以及引用 是否正确
            for (Stmt *s = mod.toplevel; s; s= s->next) {
                s->accept(p);
            }
        }
    }
    if (n_errors) {
        return n_errors; 
    }

    {
        auto it0 = used_as_call.begin();
        auto it0e = used_as_call.end();
        auto it1 = used_as_collapse.begin();
        auto it1e = used_as_collapse.end();
        auto it2 = used_as_embed.begin();
        auto it2e = used_as_embed.end();
        while (it0 != it0e || it1 != it1e || it2 != it2e) { // 确保一个 DefineStmt 只能被以下三种方式之一使用： CallExpr（普通调用） CollapseExpr EmbedExpr
            long c = 0;
            DefineStmt *x = NULL;
            if (it0 != it0e &&
                    (!x || it0->first < x)) {
                x = it0->first;
            }
            if (it1 != it1e &&
                    (!x || it1->first < x)) {
                x = it1->first;
            }
            if (it2 != it2e &&
                    (!x || it2->first < x)) {
                x = it2->first;
            }
            if (it0 != it0e && it0->first == x) {
                c++;
            }
            if (it1 != it1e && it1->first == x) {
                c++;
            }
            if (it2 != it2e && it2->first == x) {
                c++;
            }
            if (c > 1) {
                x->module->locfile.locate(x->loc, "%s is not used solely as CallExpr, CollapseExpr or EmbedExpr", 
                        x->lhs.c_str());
                if (it0 != it0e && it0->first == x) {
                    for (auto *y : it0->second) {
                        fputs("    ", stderr);
                        y->stmt->module->locfile.locate(y->loc, "required by %s", y->stmt->lhs.c_str());
                    }
                }
                if (it1 != it1e && it1->first == x) {
                    for (auto *y : it1->second) {
                        fputs("    ", stderr);
                        y->stmt->module->locfile.locate(y->loc, "required by %s", y->stmt->lhs.c_str());
                    }
                }
                if (it2 != it2e && it2->first == x) {
                    for (auto *y : it2->second) {
                        fputs("    ", stderr);
                        y->stmt->module->locfile.locate(y->loc, "required by %s", y->stmt->lhs.c_str());
                    }
                }
            }
            if (it0 != it0e && it0->first == x) {
                ++it0;
            }
            if (it1 != it1e && it1->first == x) {
                ++it1;
            }
            if (it2 != it2e && it2->first == x) {
                ++it2;
            }
        }
    }

    if (opt_dump_module) {
        printf("\n====== Module\n");
        for (auto &it : inode2module) {
            if (it.second.status == GOOD) {
                Module &mod = it.second;
                print_module_info(mod);
            }
        }
    }

    if (0 && opt_dump_tree) {
        printf("\n====== Tree\n");
        StmtPrinter p;
        for (auto &it : inode2module) {
            Module &mod = it.second;
            printf("filename: %s\n", mod.filename.c_str());
            for (Stmt *s = mod.toplevel; s; s= s->next) {
                s->accept(p);
            }
        }
    }

    printf("\n====== Topological sorting\n");
    std::vector<DefineStmt*> topo = topo_define_stmts(n_errors);
    if (n_errors) {
        return n_errors; 
    }

    action_label_base = action_label = AB;
    call_label_base = call_label = action_label + 1000000;
    collapse_label_base = collapse_label = call_label + 1000000;

    printf("\n====== Compiling DefineStmt\n");
    for (auto stmt : topo) {
        int s = (int)time(NULL);
        printf("%s======================> %s\n", stmt->module->filename.c_str(), stmt->lhs.c_str());
        compile(stmt);
        int e = (int)time(NULL);
        printf("%s<====================== %s compiled done. elapse %d\n",
                stmt->module->filename.c_str(),
                stmt->lhs.c_str(),
                e - s
                );
    }

    output = stdout;

    std::unordered_map<DefineStmt*, std::vector<std::pair<long, long>>> stmt2call_addr;
    printf("Compiling exporting DefineStmt (coalescing referenced CallExpr/CollapseExpr)\n");
    for (Stmt *x = main_module->toplevel; x; x = x->next) {
        if (auto xx = dynamic_cast<DefineStmt*>(x))  {
            if (xx->export_ && !compile_export(xx)) {
                n_errors++;
            }
        }
    }
    if (n_errors) {
        return n_errors;
    }
    for (Stmt *x = main_module->toplevel; x; x = x->next) {
        if (auto xx = dynamic_cast<DefineStmt*>(x))  {
            if (xx->export_) {
                FsaAnno &anno = compiled[xx];
                if (opt_dump_fsa) {
                    print_fsa(anno.fsa);
                }
                if (opt_dump_assoc) {
                    print_assoc(anno);
                }
            }
        }
    }

    if (opt_mode == Mode::cxx) {
        printf("Generating C++\n");
        generate_cxx(mod);
    } else if (opt_mode == Mode::graphviz) {
        printf("Generating Graphviz dot\n");
        generate_graphviz(mod);
    } else if (opt_mode == Mode::interactive) {
        printf("Testing given string\n");
        DefineStmt *main_export = NULL; 
        for (Stmt *x = main_module->toplevel; x; x = x->next) {
            if (auto xx = dynamic_cast<DefineStmt*>(x))  {
                if (xx->export_) {
                    main_export = xx;
                    break;
                }
            }
        }
        if (!main_export) {
            printf("no exporting DefineStmt\n");
        } else {
            printf("Testing %s\n", main_export->lhs.c_str());
        }
    }


    fclose(output);
    return n_errors;
}

void unload_all() {
    for (auto &it : inode2module) {
        Module &mod = it.second;
        stmt_free(mod.toplevel) ;
    }
}

