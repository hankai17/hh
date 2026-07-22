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
#include <string.h>
#include <unordered_map>
#include <functional>

//#define DEBUG_ON 0

static std::map<std::pair<dev_t, ino_t>, Module> inode2module;
static std::unordered_map<DefineStmt*, std::vector<DefineStmt*>> depended_by;

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

    puts("defined:");
    for (auto &x : mod.defined) {
        printf("    %s\n", x.first.c_str());
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
        if (mod.named_action.count(stmt.ident)) {
            n_errors++;
            mod.locfile.locate(stmt.loc, "Redefined '%s'\n", stmt.ident.c_str());
        }
        mod.named_action[stmt.ident] = stmt.code;
    }

    void visit(DefineStmt &stmt) override {                             // 2.1 接普通语句
#ifdef DEBUG_ON
        std::cout << "visit ModuleImportDef:PreorderStmtVisitor DefineStmt\n" << std::endl;
#endif
        mod.defined.emplace(stmt.lhs, &stmt);                           // 2.1.1 copy AST到 ModuleImportDef
        stmt.module = &mod;
        depended_by[&stmt];
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
        if (stmt.qualified.size()) {                                           // 2.2.2 加载新文件 保存到当前mod里
            mod.qualified_import[stmt.qualified] = m;                   // 2.2.3 有限定符 保存到当前mod的qualified_import里 eg: import file as f
        } else if (std::count(ALL(mod.unqualified_import), m) == 0) {   // 2.2.3 无限定符 保存到当前mod的unqualified_import里 eg: import file
            mod.unqualified_import.push_back(m);
        }
    }
};

struct ModuleUse : PreorderActionExprStmtVisitor {
    Module &mod;
    long &n_errors;
    DefineStmt *define_stmt = NULL;

    ModuleUse(Module &mod, long &n_errors) :
        mod(mod),
        n_errors(n_errors) {
    }

    void visit(DefineStmt &stmt) override {
#ifdef DEBUG_ON
        std::cout << "visit ModuleUse:PreorderActionExprStmtVisitor DefineStmt\n" << std::endl;
#endif
        define_stmt = &stmt;
        stmt.rhs->accept(*this);
        define_stmt = NULL;
    }
    
    void visit(BracketExpr &expr) override {
#ifdef DEBUG_ON
        std::cout << "visit ModuleUse:PreorderActionExprStmtVisitor BracketExpr\n" << std::endl;
#endif
    }

    void visit(ClosureExpr &expr) override {
#ifdef DEBUG_ON
        std::cout << "visit ModuleUse:PreorderActionExprStmtVisitor ClosureExpr\n" << std::endl;
#endif
        expr.inner->accept(*this);
    }

    void visit(CollapseExpr &expr) override {
#ifdef DEBUG_ON
        std::cout << "visit ModuleUse:PreorderActionExprStmtVisitor CollapseExpr\n" << std::endl;
#endif
        if (expr.qualified.size()) {
            if (!mod.qualified_import.count(expr.qualified)) {
                n_errors++;
                mod.locfile.locate(expr.loc, "Unknown module '%s'\n", expr.qualified.c_str());
            } else {
                auto it = mod.qualified_import[expr.qualified]->defined.find(expr.ident);
                if (it == mod.qualified_import[expr.qualified]->defined.end()) {
                    n_errors++;
                    mod.locfile.locate(expr.loc, "'%s::%s': Undefined \n", expr.qualified.c_str(), expr.ident.c_str());
                } else {
                    expr.define_stmt = it->second;
                }
            }
        } else {
            auto it = mod.defined.find(expr.ident);
            bool found = it != mod.defined.end();
            for (auto &import : mod.unqualified_import) {
                auto it2 = import->defined.find(expr.ident);
                if (it2 != import->defined.end()) {
                    if (found) {
                        n_errors++;
                        mod.locfile.locate(expr.loc, "'%s' redefined in unqualified import %s\n",
                                expr.ident.c_str(),
                                import->filename.c_str());
                    } else {
                        it = it2;
                        found = true;
                    }
                }
            }
            if (!found) {
                n_errors++;
                mod.locfile.locate(expr.loc, "'%s' Undefined\n", expr.ident.c_str());
            } else {
                expr.define_stmt = it->second;
            }
        }
    }

    void visit(EmbedExpr &expr) override {
#ifdef DEBUG_ON
        std::cout << "visit ModuleUse:PreorderActionExprStmtVisitor EmbedExpr\n" << std::endl;
#endif
        if (expr.qualified.size()) {                                   // 限定符场景
            if (!mod.qualified_import.count(expr.qualified)) {
                n_errors++;
                mod.locfile.locate(expr.loc, "Unknown module '%s'\n", expr.qualified.c_str());
            } else {
                auto it = mod.qualified_import[expr.qualified]->defined.find(expr.ident);
                if (it == mod.qualified_import[expr.qualified]->defined.end()) {
                    n_errors++;
                    mod.locfile.locate(expr.loc, "'%s::%s': Undefined \n", expr.qualified.c_str(), expr.ident.c_str());
                } else {
                    depended_by[it->second].push_back(define_stmt);
                    expr.define_stmt = it->second;
                }
            }
        } else {                                                // 无限定符场景 或 一般场景
            auto it = mod.defined.find(expr.ident);             // 一般场景: 检查 全局是否 已经存储(ModuleImportDef:PreorderStmtVisitor::visit(DefineStmt &))了这个ident
            bool found = it != mod.defined.end();
            for (auto &import : mod.unqualified_import) {
                auto it2 = import->defined.find(expr.ident);
                if (it2 != import->defined.end()) {
                    if (found) {
                        n_errors++;
                        mod.locfile.locate(expr.loc, "'%s' redefined in unqualified import %s\n",
                                expr.ident.c_str(), import->filename.c_str());
                    } else {
                        it = it2;
                        found = true;
                    }
                }
            }
            if (!found) {
                n_errors++;
                mod.locfile.locate(expr.loc, "'%s' Undefined\n", expr.ident.c_str());
            } else {
                depended_by[it->second].push_back(define_stmt); // 被依赖
                expr.define_stmt = it->second;
            }
        }
    }

    void visit(MaybeExpr &expr) override {
#ifdef DEBUG_ON
        std::cout << "visit ModuleUse:PreorderActionExprStmtVisitor MaybeExpr\n" << std::endl;
#endif
        expr.inner->accept(*this);
    }

    void visit(PlusExpr &expr) override {
#ifdef DEBUG_ON
        std::cout << "visit ModuleUse:PreorderActionExprStmtVisitor PlusExpr\n" << std::endl;
#endif
        expr.inner->accept(*this);
    }

    void visit(UnionExpr &expr) override {
#ifdef DEBUG_ON
        std::cout << "visit ModuleUse:PreorderActionExprStmtVisitor UnionExpr\n" << std::endl;
#endif
        expr.lhs->accept(*this);
        expr.rhs->accept(*this);
    }
};

Module *load_module(long &n_errors, const std::string &filename) {
    std::pair<dev_t, ino_t> inode {0, 0};
    FILE *file = filename != "-" ? fopen(filename.c_str(), "r") : stdin;
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
        return &inode2module[inode];
    }

    Module &mod = inode2module[inode];
    std::string module { file != stdin ? filename : "main" };
    std::string::size_type t = module.find('.');

    if (t != std::string::npos) {
        module.erase(t, module.size() - t);
    }

    long r;
    char buf[BUF_SIZE];
    std::string data;

    while ((r = fread(buf, 1, sizeof(buf), file)) > 0) {
        data += std::string(buf, buf + r);
        if (r < sizeof(buf)) break;
    }

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
        return NULL;
    }
    mod.toplevel = toplevel;                                    // 1.1 toplevel 可能是 parser.y 中 stmt: 下 定义的那三种stmt
    return &mod;
}

static std::vector<DefineStmt *> topo_define_stmts(long &n_errors) {
    std::vector<DefineStmt*> topo;
    std::vector<DefineStmt*> st;
    std::unordered_map<DefineStmt*, i8> vis;
    std::function<bool(DefineStmt *)> dfs = [&] (DefineStmt *u) {
        if (vis[u] == 2) {
            return false;
        }
        if (vis[u] == 1) {
            u->module->locfile.locate(u->loc, "'%s': circular embedding", u->lhs.c_str());
            long i = st.size();
            while (st[i - 1] != u) {
                i--;
            }
            st.push_back(st[i - 1]);
            for (; i < st.size(); i++) {
                st[i]->module->locfile.locate(st[i]->loc, "required by %s",
                        st[i]->lhs.c_str());
            }
            fputs("\n", stderr);
            return true;
        }
        vis[u] = 1;
        st.push_back(u);
        for (auto v : depended_by[u]) {
            if (dfs(v)) {
                return true;
            }
        }
        st.pop_back();
        vis[u] = 2;
        topo.push_back(u);
        return false;
    };
    for (auto &d : depended_by) {
        if (dfs(d.first)) {
            n_errors++;
            return topo;
        }
    }
    std::reverse(ALL(topo));
    return topo;
}

long load(const std::string &filename) {
    long n_errors = 0;

    Module *mod = load_module(n_errors, filename);
    if (!mod) {                 // 1 加载首文件 构建基本的(第一个)AST
        err_exit(EX_OSFILE, "open", filename.c_str());
        return n_errors;
    }

    printf("\nProcessing import & def\n");
    for (;;) {
        bool done = true;
        for (auto &it : inode2module) {                     // 2 ModuleImportDef(能接所有语句) 根据基本的AST 构建完整的AST以及依赖关系
            if (!it.second.processed) {
                done = false;
                Module &mod = it.second;
                mod.processed = true;
                ModuleImportDef p { mod, n_errors };
                for (Stmt *s = mod.toplevel; s; s= s->next) {
                    s->accept(p);
                }
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
        Module &mod = it.second;
        ModuleUse p { mod, n_errors };                      // 3 检查变量以及引用 是否正确
        for (Stmt *s = mod.toplevel; s; s= s->next) {
            s->accept(p);
        }
    }
    if (n_errors) {
        return n_errors; 
    }

    if (opt_module_info) {
        for (auto &it : inode2module) {
            Module &mod = it.second;
            print_module_info(mod);
        }
    }

    if (opt_dump_tree) {
        StmtPrinter p;
        for (auto &it : inode2module) {
            Module &mod = it.second;
            printf("=== %s\n", mod.filename.c_str());
            for (Stmt *s = mod.toplevel; s; s= s->next) {
                s->accept(p);
            }
        }
    }

    printf("\nTopological sorting\n");
    std::vector<DefineStmt*> topo = topo_define_stmts(n_errors);
    if (n_errors) {
        return n_errors; 
    }

    printf("\nCompiling DefineStmt\n");
    for (auto stmt : topo) {
        printf("%s->%s\n", stmt->module->filename.c_str(), stmt->lhs.c_str());
        compile(stmt);
    }

    printf("\nOutput\n");
    for (Stmt *x = mod->toplevel; x; x = x->next) {
        if (auto xx = dynamic_cast<DefineStmt*>(x)) {
            if (xx->export_) {
                export_statement(xx);
            }
        }
    }
    return n_errors;
}

void unload_all() {
    for (auto &it : inode2module) {
        Module &mod = it.second;
        stmt_free(mod.toplevel) ;
    }
}

