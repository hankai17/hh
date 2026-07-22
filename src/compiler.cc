#include "compiler.hh"
#include <map>
#include <stack>
#include <iostream>

#define DEBUG_COMP 1

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

    void visit(Expr &expr) override {
        expr.accept(*this);
    }

    void visit(BracketExpr &expr) override {
#ifdef DEBUG_COMP
        std::cout << "Compiler visit BracketExpr" << std::endl;
#endif
        st.push(FsaAnno::bracket(expr));
    }

    void visit(ClosureExpr &expr) override {
#ifdef DEBUG_COMP
        std::cout << "Compiler visit ClosureExpr" << std::endl;
#endif
        expr.inner->accept(*this);
        st.top().star(expr);
    }

    void visit(CollapseExpr &expr) override {
#ifdef DEBUG_COMP
        std::cout << "Compiler visit CollapseExpr" << std::endl;
#endif
        st.push(FsaAnno::collapse(expr));
    }

    void visit(ConcatExpr &expr) override {
#ifdef DEBUG_COMP
        std::cout << "Compiler visit ConcatExpr" << std::endl;
#endif
        expr.rhs->accept(*this);
        FsaAnno rhs = std::move(st.top());
        expr.lhs->accept(*this);
        st.top().concat(rhs);
    }

    void visit(DifferenceExpr &expr) override {
#ifdef DEBUG_COMP
        std::cout << "Compiler visit DifferenceExpr" << std::endl;
#endif
        expr.rhs->accept(*this);
        FsaAnno rhs = std::move(st.top());
        expr.lhs->accept(*this);
        st.top().difference(rhs);
    }

    void visit(DotExpr &expr) override {
#ifdef DEBUG_COMP
        std::cout << "Compiler visit DotExpr" << std::endl;
#endif
        st.push(FsaAnno::dot(expr));
    }

    void visit(EmbedExpr &expr) override {
#ifdef DEBUG_COMP
        std::cout << "Compiler visit EmbedExpr" << std::endl;
#endif
        st.push(compiled[expr.define_stmt]);
    }

    void visit(IntersectExpr &expr) override {
#ifdef DEBUG_COMP
        std::cout << "Compiler visit IntersectExpr" << std::endl;
#endif
        expr.rhs->accept(*this);
        FsaAnno rhs = std::move(st.top());
        expr.lhs->accept(*this);
        st.top().intersect(rhs);
    }

    void visit(LiteralExpr &expr) override {
#ifdef DEBUG_COMP
        std::cout << "Compiler visit LiteralExpr" << std::endl;
#endif
        st.push(FsaAnno::literal(expr));
    }

    void visit(MaybeExpr &expr) override {
#ifdef DEBUG_COMP
        std::cout << "Compiler visit MaybeExpr" << std::endl;
#endif
        expr.inner->accept(*this);
        st.top().question(expr);
    }

    void visit(PlusExpr &expr) override {
#ifdef DEBUG_COMP
        std::cout << "Compiler visit PlusExpr" << std::endl;
#endif
        expr.inner->accept(*this);
        st.top().plus();
    }

    void visit(UnionExpr &expr) override {
#ifdef DEBUG_COMP
        std::cout << "Compiler visit UnionExpr" << std::endl;
#endif
        expr.rhs->accept(*this);
        FsaAnno rhs = std::move(st.top());
        expr.lhs->accept(*this);
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
    printf("Exporting %s", stmt->lhs);
    FsaAnno &anno = compiled[stmt];
    anno.determinize();
    anno.minimize();
    print_fsa(anno.fsa);
}

