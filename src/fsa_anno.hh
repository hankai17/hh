#pragma once
#include "fsa.hh"
#include "syntax.hh"

struct FsaAnno {
  bool deterministic;
  Fsa fsa;
  std::vector<std::vector<Expr*> > assoc;
  //FsaAnno& operator=(FsaAnno&&) = default;
  void collapse(CollapseExpr& expr);
  void concat(FsaAnno& rhs);
  void determinize();
  void difference(FsaAnno& rhs);
  void embed(EmbedExpr& expr);
  void intersect(FsaAnno& rhs);
  void minimize();
  void plus(PlusExpr* expr);
  void question(MaybeExpr* expr);
  void star(ClosureExpr* expr);
  void union_(FsaAnno& rhs, UnionExpr* expr);
};

