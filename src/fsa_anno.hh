#pragma once
#include "fsa.hh"
#include "syntax.hh"

enum class ExprTag {
    start = 1,
    inner = 2,
    final = 4,
};

extern inline bool has_start(ExprTag x) {
    return long(x) & long(ExprTag::start);
}
extern inline bool has_inner(ExprTag x) {
    return long(x) & long(ExprTag::inner);
}
extern inline bool has_final(ExprTag x) {
    return long(x) & long(ExprTag::final);
}

bool operator<(ExprTag x, ExprTag y);
bool assoc_has_expr(std::vector<std::pair<Expr*, ExprTag>> &as, Expr *x);

struct FsaAnno {
    bool deterministic;
    Fsa fsa;
    std::vector<std::vector<std::pair<Expr*, ExprTag>>> assoc;

    static FsaAnno bracket(BracketExpr &expr);
    static FsaAnno call(CallExpr &expr);
    static FsaAnno collapse(CollapseExpr &expr);
    static FsaAnno dot(DotExpr *expr);
    static FsaAnno embed(EmbedExpr &expr);
    static FsaAnno epsilon_fsa(EpsilonExpr *expr);
    static FsaAnno literal(LiteralExpr &expr);

    void accessible(const std::vector<long> *start, std::vector<long> &mapping);
    void co_accessible(const std::vector<bool> *final, std::vector<long> &mapping);
    void add_assoc(Expr &expr);
    void complement(ComplementExpr *expr);
    void concat(FsaAnno &rhs, ConcatExpr *expr);
    void determinize(const std::vector<long> *start, std::vector<std::vector<long>> *mapping);
    void difference(FsaAnno &rhs, DifferenceExpr *expr);
    void intersect(FsaAnno &rhs, IntersectExpr *expr);
    void minimize(std::vector<std::vector<long>> *mapping);
    void plus(PlusExpr *expr);
    void question(MaybeExpr *expr);
    void repeat(RepeatExpr &expr);
    void star(ClosureExpr *expr);
    void substring_grammar();
    void union_(FsaAnno &rhs, UnionExpr *expr);
};

