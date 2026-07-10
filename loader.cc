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

static std::map<std::pair<dev_t, ino_t>, Module> inode2module;

void load(const char *filename);
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

    void visit(ActionStmt &stmt)  override {
        if (mod.named_action.count(stmt.ident)) {
            err_msg("Redefined '%s'\n", stmt.ident);
        }
        mod.named_action[stmt.ident] = stmt.code;
    }

    void visit(DefineStmt &stmt) override {
        mod.defined.insert(stmt.lhs);
    }

    void visit(ImportStmt &stmt) override {
        Module *m = load_module(stmt.filename);
        if (stmt.qualified) {
            mod.qualified_import[stmt.qualified] = m;
        } else if (std::count(ALL(mod.unqualified_import), m) == 0) {
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
    
    void visit(BracketExpr &expr) override {}

    void visit(ClosureExpr &expr) override {
        expr.inner->accept(*this);
    }

    void visit(CollapseExpr &expr) override {
        if (expr.qualified) {
            if (!mod.qualified_import.count(expr.qualified)) {
                err_msg("%s: Unknown module '%s'\n",
                    mod.filename.c_str(), expr.qualified);
            } else if (!mod.qualified_import[expr.qualified]->defined.count(expr.ident)) {
                err_msg("%s: '%s::%s': Undefined \n",
                    mod.filename.c_str(),
                    expr.qualified,
                    expr.ident);
                //mod.locfile.locate();
                //err_msg("Redefined '%s'\n", stmt.ident);
            }
        } else {
            long c = mod.defined.count(expr.ident);
            for (auto &it : mod.unqualified_import) {
                c += it->defined.count(expr.ident);
            }
            if (!c) {
                err_msg("%s: '%s' Undefined\n",
                    mod.filename.c_str(),
                    expr.ident);
            }
        }
    }

    void visit(EmbedExpr &expr) override {
        if (expr.qualified) {
            if (!mod.qualified_import.count(expr.qualified)) {
                err_msg("%s: Unknown module '%s'\n",
                    mod.filename.c_str(), expr.qualified);
            } else if (!mod.qualified_import[expr.qualified]->defined.count(expr.ident)) {
                err_msg("%s: '%s::%s': Undefined \n",
                    mod.filename.c_str(),
                    expr.qualified,
                    expr.ident);
                //mod.locfile.locate();
                //err_msg("Redefined '%s'\n", stmt.ident);
            }
        } else {
            long c = mod.defined.count(expr.ident);
            for (auto &it : mod.unqualified_import) {
                c += it->defined.count(expr.ident);
            }
            if (!c) {
                err_msg("%s: '%s' Undefined\n",
                    mod.filename.c_str(),
                    expr.ident);
            }
        }
    }

    void visit(MaybeExpr &expr) override {
        expr.inner->accept(*this);
    }

    void visit(PlusExpr &expr) override {
        expr.inner->accept(*this);
    }

    void visit(UnionExpr &expr) override {
        expr.lhs->accept(*this);
        expr.rhs->accept(*this);
    }
};

Module *load_module(const char *filename) {
    std::pair<dev_t, ino_t> inode {0, 0};
    FILE *file = filename ? fopen(filename, "r") : stdin;

    if (!file) {
        err_exit(EX_OSFILE, "fopen '%s'", filename);
    }

    if (filename) {
        struct stat st;
        if (fstat(fileno(file), &st) < 0) {
            err_exit(EX_OSFILE, "fstat  '%s'", filename);
        }
        inode = { st.st_dev, st.st_ino };
    }

    if (inode2module.count(inode)) {
        return &inode2module[inode];
    }

    std::string module { filename ? filename : "main" };
    std::string::size_type t = module.find('.');

    if (t != std::string::npos) {
        module.erase(t, module.size() - t);
    }

    long r;
    char buf[BUF_SIZE];
    std::string data;

    while ((r = fread(buf, 1, sizeof(buf), file)) > 0) {
        data += string(buf, buf + r);
        if (r < sizeof(buf)) break;
    }

    LocationFile locfile("-", data);
    Stmt *toplevel = NULL;
    long errors = parse(locfile, toplevel);
    if (!toplevel) {
        err_exit(EX_DATAERR, "Failed to laod '%s'", filename);
    }

    Module &mod = inode2module[inode];
    mod.filename = filename;
    mod.toplevel = toplevel;
    return &mod;
}

void load(const char *filename) {
    long n_errors = 0;

    load_module(filename);

    if (!n_errors) {
        for (auto &it : inode2module) {
            Module &mod = it.second;
            ModuleImportDef p { mod, n_errors };
            for (Stmt *s = mod.toplevel; s; s= s->next) {
                s->accept(p);
            }
        }
    }

    if (!n_errors) {
        for (auto &it : inode2module) {
            Module &mod = it.second;
            ModuleUse p { mod, n_errors };
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
}

void unload_all() {
    for (auto &it : inode2module) {
        Module &mod = it.second;
        stmt_free(mod.toplevel) ;
    }
}

