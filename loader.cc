#include "loader.hh"
#include "common.hh"
#include "location.hh"
#include "parser.hh"
#include "syntax.hh"
#include "option.hh"

#include <algorithm>
#include <sysexits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <iostream>
#include <string.h>

static std::map<std::pair<dev_t, ino_t>, Module> inode2module;

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
        printf("    %s\n", x.c_str());
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
        std::cout << "visit ModuleImportDef:PreorderStmtVisitor ActionStmt\n" << std::endl;
        if (mod.named_action.count(stmt.ident)) {
            n_errors++;
            mod.locfile.locate(stmt.loc, "Redefined '%s'\n", stmt.ident);
        }
        mod.named_action[stmt.ident] = stmt.code;
    }

    void visit(DefineStmt &stmt) override {                             // 1.3 接普通语句
        std::cout << "visit ModuleImportDef:PreorderStmtVisitor DefineStmt\n" << std::endl;
        mod.defined.insert(stmt.lhs);
    }

    void visit(ImportStmt &stmt) override {                             // 1.3 接import语句
        std::cout << "visit ModuleImportDef:PreorderStmtVisitor ImportStmt\n" << std::endl;
        std::cout << "before load_module...\n" << std::endl;
        Module *m = load_module(n_errors, stmt.filename);                         // 1.3.1 边消费边生产模型
        std::cout << "after load_module...\n" << std::endl;
        if (!m) {
            n_errors++;
            mod.locfile.locate(stmt.loc, "'%s' : %s", stmt.filename,
                    errno ? strerror(errno) : "parse error");
            return;
        }
        if (stmt.qualified) {                                           // 1.3.2 加载新文件 保存到当前mod里
            mod.qualified_import[stmt.qualified] = m;                   // 1.3.3 有限定符 保存到当前mod的qualified_import里 eg: import file as f
        } else if (std::count(ALL(mod.unqualified_import), m) == 0) {   // 1.3.3 无限定符 保存到当前mod的unqualified_import里 eg: import file
            mod.unqualified_import.push_back(m);
        }
    }
};

struct ModuleUse : PreorderActionExprStmtVisitor {
    Module &mod;
    long &n_errors;

    ModuleUse(Module &mod, long &n_errors) :
        mod(mod),
        n_errors(n_errors) {
    }
    
    void visit(BracketExpr &expr) override {
        std::cout << "visit ModuleUse:PreorderActionExprStmtVisitor BracketExpr\n" << std::endl;
    }

    void visit(ClosureExpr &expr) override {
        std::cout << "visit ModuleUse:PreorderActionExprStmtVisitor ClosureExpr\n" << std::endl;
        expr.inner->accept(*this);
    }

    void visit(CollapseExpr &expr) override {
        std::cout << "visit ModuleUse:PreorderActionExprStmtVisitor CollapseExpr\n" << std::endl;
        if (expr.qualified) {
            if (!mod.qualified_import.count(expr.qualified)) {
                n_errors++;
                mod.locfile.locate(expr.loc, "Unknown module '%s'\n", expr.qualified);
            } else if (!mod.qualified_import[expr.qualified]->defined.count(expr.ident)) {
                n_errors++;
                mod.locfile.locate(expr.loc, "'%s::%s': Undefined \n", expr.qualified, expr.ident);
            }
        } else {
            long c = mod.defined.count(expr.ident);
            for (auto &it : mod.unqualified_import) {
                c += it->defined.count(expr.ident);
            }
            if (!c) {
                n_errors++;
                mod.locfile.locate(expr.loc, "'%s' Undefined\n", expr.ident);
            }
        }
    }

    void visit(EmbedExpr &expr) override {
        std::cout << "visit ModuleUse:PreorderActionExprStmtVisitor EmbedExpr\n" << std::endl;
        if (expr.qualified) {                                   // 限定符场景
            if (!mod.qualified_import.count(expr.qualified)) {
                n_errors++;
                mod.locfile.locate(expr.loc, "Unknown module '%s'\n", expr.qualified);
            } else if (!mod.qualified_import[expr.qualified]->defined.count(expr.ident)) {
                n_errors++;
                mod.locfile.locate(expr.loc, "'%s::%s': Undefined \n", expr.qualified, expr.ident);
            }
        } else {                                                // 无限定符场景 或 一般场景
            long c = mod.defined.count(expr.ident);             // 一般场景: 检查 全局是否 已经存储(ModuleImportDef:PreorderStmtVisitor::visit(DefineStmt &))了这个ident
            for (auto &it : mod.unqualified_import) {           // 无限定符场景
                c += it->defined.count(expr.ident);
            }
            if (!c) {
                n_errors++;
                mod.locfile.locate(expr.loc, "'%s' Undefined\n", expr.ident);
            }
        }
    }

    void visit(MaybeExpr &expr) override {
        std::cout << "visit ModuleUse:PreorderActionExprStmtVisitor MaybeExpr\n" << std::endl;
        expr.inner->accept(*this);
    }

    void visit(PlusExpr &expr) override {
        std::cout << "visit ModuleUse:PreorderActionExprStmtVisitor PlusExpr\n" << std::endl;
        expr.inner->accept(*this);
    }

    void visit(UnionExpr &expr) override {
        std::cout << "visit ModuleUse:PreorderActionExprStmtVisitor UnionExpr\n" << std::endl;
        expr.lhs->accept(*this);
        expr.rhs->accept(*this);
    }
};

Module *load_module(long &n_errors, const char *filename) {
    std::pair<dev_t, ino_t> inode {0, 0};
    FILE *file = strcmp(filename, "-") ? fopen(filename, "r") : stdin;
    if (!file) {
        return NULL;
    }

    if (file != stdin) {
        struct stat st;
        if (fstat(fileno(file), &st) < 0) {
            err_exit(EX_OSFILE, "fstat  '%s'", filename);
        }
        inode = { st.st_dev, st.st_ino };
    }

    if (inode2module.count(inode)) {
        return &inode2module[inode];
    }

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
    Stmt *toplevel = NULL;
    long errors = parse(locfile, toplevel);
    if (!toplevel) {
        n_errors += errors;
        return NULL;
    }

    Module &mod = inode2module[inode];
    mod.locfile = locfile;
    mod.filename = filename;
    mod.toplevel = toplevel;                                // 1.1 toplevel 可能是 parser.y 中 stmt: 下 定义的那三种stmt
    return &mod;
}

long load(const char *filename) {
    long n_errors = 0;

    if (!load_module(n_errors, filename)) {                 // 1 加载首文件 构建AST
        return n_errors;
    }

    if (!n_errors) {
        for (auto &it : inode2module) {                     // 1.2 ModuleImportDef 能接所有语句
            Module &mod = it.second;
            ModuleImportDef p { mod, n_errors };
            for (Stmt *s = mod.toplevel; s; s= s->next) {
                s->accept(p);                               // 边消费 边生产
            }
        }
    }

    if (!n_errors) {
        for (auto &it : inode2module) {
            Module &mod = it.second;                        // 这里mod 是"全局的" 意思是 ModuleImportDef ModuleUse 公用同一个
            ModuleUse p { mod, n_errors };                  // 2 遍历表达式 既遍历stmt等号右边的expr
            for (Stmt *s = mod.toplevel; s; s= s->next) {
                s->accept(p);
            }
        }
    }

    if (opt_module_info && !n_errors) {
        for (auto &it : inode2module) {
            Module &mod = it.second;
            print_module_info(mod);
        }
    }

    if (opt_dump_tree && !n_errors) {
        StmtPrinter p;
        for (auto &it : inode2module) {
            Module &mod = it.second;
            for (Stmt *s = mod.toplevel; s; s= s->next) {
                s->accept(p);
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

