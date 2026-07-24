#pragma once
#include "fsa.hh"
#include "syntax.hh"

enum class ExprTag {
    start = 1,
    inner = 2,
    final = 4,
};

bool operator<(ExprTag x, ExprTag y);
bool assoc_has_expr(std::vector<std::pair<Expr*, ExprTag>> &as, Expr *x);

struct FsaAnno {
    bool deterministic;
    Fsa fsa;
    std::vector<std::vector<std::pair<Expr*, ExprTag>>> assoc;

    static FsaAnno bracket(BracketExpr &expr);
    static FsaAnno collapse(CollapseExpr &expr);
    static FsaAnno dot(DotExpr &expr);
    static FsaAnno literal(LiteralExpr &expr);

    void add_assoc(Expr &expr);
    void concat(FsaAnno &rhs, ConcatExpr &expr);
    void determinize();
    void difference(FsaAnno &rhs, DifferenceExpr &expr);
    void embed(EmbedExpr &expr);
    void intersect(FsaAnno &rhs, IntersectExpr &expr);
    void minimize();
    void plus(PlusExpr &expr);
    void question(MaybeExpr &expr);
    void star(ClosureExpr &expr);
    void substring_grammar();
    void union_(FsaAnno &rhs, UnionExpr &expr);
};

