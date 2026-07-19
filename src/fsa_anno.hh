#pragma once
#include "fsa.hh"
#include "syntax.hh"

struct FsaAnno {
    bool deterministic;
    Fsa fsa;
    std::vector<std::vector<Expr*> > assoc;

    static FsaAnno bracket(BracketExpr &expr);
    static FsaAnno collapse(CollapseExpr &expr);
    static FsaAnno dot(DotExpr &expr);
    static FsaAnno literal(LiteralExpr &expr);

    void concat(FsaAnno &rhs);
    void determinize();
    void difference(FsaAnno &rhs);
    void embed(EmbedExpr &expr);
    void intersect(FsaAnno &rhs);
    void minimize();
    void plus();
    void question(MaybeExpr &expr);
    void star(ClosureExpr &expr);
    void union_(FsaAnno &rhs, UnionExpr &expr);
};

