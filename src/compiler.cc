#include "compiler.hh"
#include "common.hh"
#include <map>
#include <stack>
#include <iostream>
#include <functional>
#include <unordered_map>

//#define DEBUG_COMP 1

static std::map<DefineStmt *, FsaAnno> compiled;

static void print_fsa(const Fsa &fsa) {
    printf("start: %ld\n", fsa.start);
    printf("finals:");
    for (long i : fsa.finals) {
        printf(" %ld", i);
    }
    puts("");
    puts("edges:");
    REP (i, fsa.n()) {
        printf("%ld: ", i);
        for (auto &x : fsa.adj[i]) {
            printf(" (%ld, %ld)", x.first, x.second);
        }
        puts("");
    }
}

struct Compiler : Visitor<Expr> {
    std::stack<FsaAnno> st;
    std::stack<Expr*> path;

    void pre(Expr &expr) {
        expr.depth = path.size();
        if (path.size()) {
            expr.anc.assign(1, &expr);
        }
    }

    void visit(Expr &expr) override {
        expr.accept(*this);
    }

    void visit(BracketExpr &expr) override {
#ifdef DEBUG_COMP
        std::cout << "Compiler visit BracketExpr" << std::endl;
#endif
        pre(expr);
        st.push(FsaAnno::bracket(expr));
    }

    void visit(ClosureExpr &expr) override {
#ifdef DEBUG_COMP
        std::cout << "Compiler visit ClosureExpr" << std::endl;
#endif
        pre(expr);
        expr.inner->accept(*this);
        st.top().star(expr);
    }

    void visit(CollapseExpr &expr) override {
#ifdef DEBUG_COMP
        std::cout << "Compiler visit CollapseExpr" << std::endl;
#endif
        pre(expr);
        st.push(FsaAnno::collapse(expr));
    }

    void visit(ConcatExpr &expr) override {
#ifdef DEBUG_COMP
        std::cout << "Compiler visit ConcatExpr" << std::endl;
#endif
        pre(expr);
        path.push(&expr);
        expr.rhs->accept(*this);
        FsaAnno rhs = std::move(st.top());
        expr.lhs->accept(*this);
        path.pop();
        st.top().concat(rhs);
    }

    void visit(DifferenceExpr &expr) override {
#ifdef DEBUG_COMP
        std::cout << "Compiler visit DifferenceExpr" << std::endl;
#endif
        pre(expr);
        path.push(&expr);
        expr.rhs->accept(*this);
        FsaAnno rhs = std::move(st.top());
        expr.lhs->accept(*this);
        path.pop();
        st.top().difference(rhs);
    }

    void visit(DotExpr &expr) override {
#ifdef DEBUG_COMP
        std::cout << "Compiler visit DotExpr" << std::endl;
#endif
        pre(expr);
        st.push(FsaAnno::dot(expr));
    }

    void visit(EmbedExpr &expr) override {
#ifdef DEBUG_COMP
        std::cout << "Compiler visit EmbedExpr" << std::endl;
#endif
        pre(expr);
        st.push(compiled[expr.define_stmt]);
    }

    void visit(IntersectExpr &expr) override {
#ifdef DEBUG_COMP
        std::cout << "Compiler visit IntersectExpr" << std::endl;
#endif
        pre(expr);
        path.push(&expr);
        expr.rhs->accept(*this);
        FsaAnno rhs = std::move(st.top());
        expr.lhs->accept(*this);
        path.pop();
        st.top().intersect(rhs);
    }

    void visit(LiteralExpr &expr) override {
#ifdef DEBUG_COMP
        std::cout << "Compiler visit LiteralExpr" << std::endl;
#endif
        pre(expr);
        st.push(FsaAnno::literal(expr));
    }

    void visit(MaybeExpr &expr) override {
#ifdef DEBUG_COMP
        std::cout << "Compiler visit MaybeExpr" << std::endl;
#endif
        pre(expr);
        path.push(&expr);
        expr.inner->accept(*this);
        path.pop();
        st.top().question(expr);
    }

    void visit(PlusExpr &expr) override {
#ifdef DEBUG_COMP
        std::cout << "Compiler visit PlusExpr" << std::endl;
#endif
        pre(expr);
        path.push(&expr);
        expr.inner->accept(*this);
        path.pop();
        st.top().plus();
    }

    void visit(UnionExpr &expr) override {
#ifdef DEBUG_COMP
        std::cout << "Compiler visit UnionExpr" << std::endl;
#endif
        pre(expr);
        path.push(&expr);
        expr.rhs->accept(*this);
        FsaAnno rhs = std::move(st.top());
        expr.lhs->accept(*this);
        path.pop();
        st.top().union_(rhs, expr);
    }
};

void compile(DefineStmt *stmt) {
    if (compiled.count(stmt)) {
        return;
    }
    FsaAnno &anno = compiled[stmt];
    Compiler comp;
    stmt->rhs->accept(comp);
    anno = std::move(comp.st.top());
}

void export_statement(DefineStmt *stmt) {
    printf("Exporting %s\n", stmt->lhs.c_str());
    FsaAnno &anno = compiled[stmt];

    printf("Construct automato with all referenced CollapseExpr's DefineStmt\n");
    std::vector<std::vector<std::pair<long, long>>> adj;
    std::vector<std::vector<Expr*>> assoc;
    std::vector<std::vector<DefineStmt*>> cllps;
    long allo = 0;
    std::unordered_map<DefineStmt*, long> stmt2offset;
    std::function<void(DefineStmt*)> allocate_collapse = [&] (DefineStmt *stmt) {
        if (stmt2offset.count(stmt))  {
            return;
        }
        printf("Allocate %ld to %s\n", allo, stmt->lhs.c_str());
        FsaAnno &anno = compiled[stmt];
        long old = stmt2offset[stmt] = allo;
        allo += anno.fsa.n() + 1;
        adj.insert(adj.end(), ALL(anno.fsa.adj));
        REP (i, anno.fsa.n()) {
            for (auto &e : adj[old + i]) {
                e.second += old;
            }
        }
        adj.emplace_back();
        assoc.insert(assoc.end(), ALL(anno.assoc));
        assoc.emplace_back();
        FOR (i, old, old + anno.fsa.n()) {
            if (anno.fsa.has(i - old, 256)) {
                for (auto a : assoc[i]) {
                    if (auto e = dynamic_cast<CollapseExpr *>(a)) {
                        DefineStmt *v = e->define_stmt;
                        allocate_collapse(v);
                        sorted_insert(adj[i],
                                std::make_pair(-1L, stmt2offset[v] + compiled[v].fsa.start));
                    }
                }
                long j = adj[i].size();
                while (j && adj[i][j - 1].first == 256) {
                    long v = adj[i][--j].second;
                    for (auto a : assoc[v]) {
                        if (auto e = dynamic_cast<CollapseExpr*>(a)) {
                            DefineStmt *w = e->define_stmt;
                            allocate_collapse(w);
                            for (long f : compiled[w].fsa.finals) {
                                long g = stmt2offset[w] + f;
                                sorted_insert(adj[g], std::make_pair(-1L, v));
                                if (g == i) {
                                    j++;
                                }
                            }
                        }
                    }
                }
                adj[i].resize(j);
            }
        }
    };
    allocate_collapse(stmt);
    anno.fsa.adj = std::move(adj);
    anno.assoc = std::move(assoc);
    anno.determinize();
    anno.minimize();

    allo = 0;
    auto relate = [&] (long x) {
        if (allo != x) {
            anno.fsa.adj[allo] = std::move(anno.fsa.adj[x]);
            anno.assoc[allo] = std::move(anno.assoc[x]);
        }
        allo++;
    };
    anno.fsa.remove_dead(relate);
    print_fsa(anno.fsa);
}


