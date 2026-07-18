#include "fsa_anno.hh"

#include <algorithm>
#include <utility>

//using namespace std;

void FsaAnno::collapse(CollapseExpr& expr) {
  // TODO
  fsa.start = 0;
  fsa.finals = {0};
  fsa.adj.assign(1, {});
  assoc.clear();
  assoc.emplace_back(1, &expr);
}

void FsaAnno::concat(FsaAnno& rhs) {
  long ln = fsa.n(), rn = rhs.fsa.n();
  for (long f: fsa.finals)
    fsa.adj[f].emplace(fsa.adj[f].begin(), -1, ln+rhs.fsa.start);
  for (auto& es: rhs.fsa.adj) {
    for (auto& e: es)
      e.second += ln;
    fsa.adj.emplace_back(std::move(es));
  }
  fsa.finals = std::move(rhs.fsa.finals);
  for (long& f: fsa.finals)
    f += ln;
  for (auto& a: rhs.assoc)
    assoc.emplace_back(std::move(a));
  deterministic = false;
}

void FsaAnno::difference(FsaAnno& rhs) {
  std::vector<std::vector<long>> rel0, rel1;
  std::vector<std::vector<Expr*>> new_assoc;
  auto relate0 = [&](std::vector<long>& xs) {
    rel0.emplace_back(std::move(xs));
  };
  auto relate1 = [&](std::vector<long>& xs) {
    rel1.emplace_back(std::move(xs));
  };
  auto relate = [&](long x, long _y) {
    if (rel0.empty())
      new_assoc.emplace_back(assoc[x]);
    else {
      new_assoc.emplace_back();
      std::vector<Expr*>& a = new_assoc.back();
      for (long u: rel0[x])
        a.insert(a.end(), ALL(assoc[u]));
      std::sort(ALL(a));
      a.erase(std::unique(ALL(a)), a.end());
    }
  };
  if (! deterministic)
    fsa = fsa.determinize(relate0);
  if (! rhs.deterministic)
    rhs.fsa = rhs.fsa.determinize(relate1);
  fsa = fsa.difference(rhs.fsa, relate);
  assoc = std::move(new_assoc);
  deterministic = true;
}

void FsaAnno::embed(EmbedExpr& expr) {
  // TODO
  fsa.start = 0;
  fsa.finals = {0};
  fsa.adj.assign(1, {});
  assoc.clear();
  assoc.emplace_back(1, &expr);
}

void FsaAnno::intersect(FsaAnno& rhs) {
  std::vector<std::vector<Expr*>> new_assoc;
  std::vector<std::vector<long>> rel0, rel1;
  auto relate0 = [&](std::vector<long>& xs) {
    rel0.emplace_back(std::move(xs));
  };
  auto relate1 = [&](std::vector<long>& xs) {
    rel1.emplace_back(std::move(xs));
  };
  auto relate = [&](long x, long y) {
    new_assoc.emplace_back();
    std::vector<Expr*>& a = new_assoc.back();
    if (rel0.empty())
      a.insert(a.end(), ALL(assoc[x]));
    else
      for (long u: rel0[x])
        a.insert(a.end(), ALL(assoc[u]));
    if (rel1.empty())
      a.insert(a.end(), ALL(rhs.assoc[y]));
    else
      for (long v: rel1[y])
        a.insert(a.end(), ALL(assoc[v]));
    std::sort(ALL(a));
    a.erase(std::unique(ALL(a)), a.end());
  };
  if (! deterministic)
    fsa = fsa.determinize(relate0);
  if (! rhs.deterministic)
    rhs.fsa = rhs.fsa.determinize(relate1);
  fsa = fsa.intersect(rhs.fsa, relate);
  assoc = std::move(new_assoc);
  deterministic = true;
}

void FsaAnno::union_(FsaAnno& rhs, UnionExpr* expr) {
  std::vector<std::vector<Expr*>> new_assoc;
  std::vector<std::vector<long>> rel0, rel1;
  //auto relate0 = [&](vector<long>& xs) {
  //  rel0.emplace_back(move(xs));
  //};
  //auto relate1 = [&](vector<long>& xs) {
  //  rel1.emplace_back(move(xs));
  //};
  //auto relate = [&](vector<long>& xs, vector<long>& ys, bool flag) {
  //  new_assoc.emplace_back();
  //  vector<Expr*>& a = new_assoc.back();
  //  for (long x: xs)
  //    a.insert(a.end(), ALL(assoc[x]));
  //  for (long y: ys)
  //    a.insert(a.end(), ALL(assoc[y]));
  //  sort(ALL(a));
  //  a.erase(unique(ALL(a)), a.end());
  //};

  long ln = fsa.n(), rn = rhs.fsa.n(), src = ln+rn,
       old_lsrc = fsa.start;
  fsa.start = src;
  for (long f: rhs.fsa.finals)
    fsa.finals.push_back(ln+f);
  for (auto& es: rhs.fsa.adj)
    fsa.adj.emplace_back(std::move(es));
  fsa.adj.emplace_back();
  fsa.adj[src].emplace_back(-1, old_lsrc);
  fsa.adj[src].emplace_back(-1, ln+rhs.fsa.start);
  for (auto& a: rhs.assoc)
    assoc.emplace_back(std::move(a));
  assoc.emplace_back(1, expr);
  deterministic = false;
}

void FsaAnno::plus(PlusExpr* expr) {
  for (long f: fsa.finals)
    sorted_insert(fsa.adj[f], std::make_pair(-1L, fsa.start));
  deterministic = false;
}

void FsaAnno::question(MaybeExpr* expr) {
  long src = fsa.n(), sink = src+1, old_src = fsa.start;
  fsa.start = src;
  fsa.adj.emplace_back();
  fsa.adj.emplace_back();
  fsa.adj[src].emplace_back(-1, old_src);
  fsa.adj[src].emplace_back(-1, sink);
  fsa.finals.push_back(sink);
  assoc.emplace_back(1, expr);
  assoc.emplace_back(1, expr);
  deterministic = false;
}

void FsaAnno::star(ClosureExpr* expr) {
  long src = fsa.start = fsa.n(), sink = src+1, old_src = fsa.start;
  fsa.adj.emplace_back();
  fsa.adj.emplace_back();
  fsa.adj[src].emplace_back(-1, old_src);
  fsa.adj[src].emplace_back(-1, sink);
  for (long f: fsa.finals) {
    sorted_insert(fsa.adj[f], std::make_pair(-1L, old_src));
    sorted_insert(fsa.adj[f], std::make_pair(-1L, sink));
  }
  fsa.finals.assign(1, sink);
  assoc.emplace_back(1, expr);
  assoc.emplace_back(1, expr);
  deterministic = false;
}

