#include "fsa_anno.hh"

#include <algorithm>
#include <utility>
#include <cassert>
#include <string.h>
#include <limits.h>
#include <map>
#include <unicode/utf8.h>

bool operator<(ExprTag x, ExprTag y) {
    return long(x) < long(y);
}

bool assoc_has_expr(std::vector<std::pair<Expr*, ExprTag>> &as, Expr *x) {
    auto it = std::lower_bound(ALL(as),
            std::make_pair(x, ExprTag(0)));
    return it != as.end() && it->first == x;
}

FsaAnno FsaAnno::bracket(BracketExpr &expr) {
    FsaAnno r;
    r.fsa.start = 0;
    r.fsa.finals = {1};
    r.fsa.adj.resize(2);
    REP (c, 256) {
        if (expr.charset[c]) {
            r.fsa.adj[0].emplace_back(c, 1);
        }
    }
    r.assoc.resize(2);
    r.add_assoc(expr);
    r.deterministic = true;
    return r;
}

FsaAnno FsaAnno::collapse(CollapseExpr &expr) {
    FsaAnno r;
    r.fsa.start = 0;
    r.fsa.finals = {1};
    r.fsa.adj.resize(2);
    r.fsa.adj[0].emplace_back(256, 1);
    r.assoc.resize(2);
    r.add_assoc(expr);
    r.deterministic = true;
    return r;
}

FsaAnno FsaAnno::dot(DotExpr *expr) {
    FsaAnno r;
    r.fsa.start = 0;
    r.fsa.finals = {1};
    r.fsa.adj.resize(2);
    REP (c, 256) {
        r.fsa.adj[0].emplace_back(c, 1);
    }
    r.assoc.resize(2);
    if (expr) {
        r.add_assoc(*expr);
    }
    return r;
}

FsaAnno FsaAnno::epsilon(EpsilonExpr *expr) {
    FsaAnno r;
    r.fsa.start = 0;
    r.fsa.finals.push_back(0);
    r.fsa.adj.resize(1);
    r.assoc.resize(1);
    if (expr) {
        r.add_assoc(*expr);
    }
    r.deterministic = true;
    return r;
}


FsaAnno FsaAnno::literal(LiteralExpr &expr) {
    FsaAnno r;
    r.fsa.start = 0;
    r.fsa.finals.assign(1, expr.literal.size());
    r.fsa.adj.resize(expr.literal.size() + 1);
    REP (i, expr.literal.size()) {
        r.fsa.adj[i].emplace_back((unsigned char)expr.literal[i], i + 1);
    }
    r.assoc.resize(expr.literal.size() + 1);
    r.add_assoc(expr);
    r.deterministic = true;
    return r;
}

FsaAnno FsaAnno::unicode_range(UnicodeRangeExpr &expr) {
    FsaAnno r;
    long n = 0;
    struct Trie {
        long id = 1;
        long refcnt = 1;
        std::map<int, Trie*> ch;
        ~Trie() {
            for (auto c : ch) {
                if (!--c.second->refcnt) {
                    delete c.second;
                }
            }
        }
    } root;
    root.id = n++;
    r.fsa.start = 0;

    Trie *last = NULL;
    FOR (i, expr.start, expr.end) {
        u8 s[4];
        long len = 0;
        U8_APPEND_UNSAFE(s, len, i);
        Trie *x = &root;
        Trie *y;
        REP (j, len) {
            auto it = x->ch.find(s[j]);
            if (it == x->ch.end()) {
                if (j == len - 1 && last) {
                    y = last;
                    y->refcnt++;
                } else {
                    y = new Trie;
                    y->id = n++;
                }
                x->ch[s[j]] = y;
                x = y;
            } else {
                x = it->second;
            }
        }
        if (!last) {
            last = x;
        }
        r.fsa.finals.push_back(x->id);
    }

    r.fsa.adj.resize(n);
    std::function<void(Trie*)> dfs = [&] (Trie *x) {
        for (auto &c : x->ch) {
            r.fsa.adj[x->id].emplace_back(c.first, c.second->id);
            dfs(c.second);
        }
    };
    dfs(&root);
    std::sort(ALL(r.fsa.finals));
    r.assoc.resize(n);
    r.add_assoc(expr);
    return r;
}

void FsaAnno::accessible() {
    long allo = 0;
    auto relate = [&] (long x) {
        if (allo != x) {
            assoc[allo] = std::move(assoc[x]);
        }
        allo++;
    };
    fsa.accessible(relate);
    assoc.resize(allo);
}

void FsaAnno::co_accessible() {
    long allo = 0;
    auto relate = [&] (long x) {
        if (allo != x) {
            assoc[allo] = std::move(assoc[x]);
        }
        allo++;
    };
    fsa.co_accessible(relate);
    if (fsa.finals.empty()) {
        assoc.assign(1, {});
        deterministic = true;
        return;
    }
    if (!deterministic) {
        REP (i, fsa.n()) {
            std::sort(ALL(fsa.adj[i]));
        }
    }
    assoc.resize(allo);
}

/*
四种基本的状态类型(bracket/collapse/dot/literal) 通过复合操作(concat/union/intersect/difference/start/plus/question) 构建自动机
eg: ident = [a-z] [0-9]*
    1 [a-z] → bracket()
    2 [0-9]* → bracket() + star()
    两个部分 concat() 连接起来
    最终得到一个复杂的 FsaAnno，内部状态关联了多个 Expr。
*/

void FsaAnno::add_assoc(Expr &expr) {
    if (expr.no_action() &&
            !expr.stmt->intact &&
            !dynamic_cast<CollapseExpr*>(&expr)) {
        return;
    }
    auto j = fsa.finals.begin();
    REP (i, fsa.n()) {
        ExprTag tag = ExprTag(0);
        if (i == fsa.start) {
            tag = ExprTag::start;
        }
        while (j != fsa.finals.end() && *j < i) {
            ++j;
        }
        if (j != fsa.finals.end() && *j == i) {
            tag = ExprTag(long(tag) | long(ExprTag::final));
        }
        if (tag == ExprTag(0)) {
            tag = ExprTag::inner;
        }
        sorted_insert(assoc[i], std::make_pair(&expr, tag));
    }
}

void FsaAnno::complement(ComplementExpr *expr) {
    fsa = ~fsa;
    assoc.assign(fsa.n(), {});
}

void FsaAnno::concat(FsaAnno& rhs, ConcatExpr *expr) {
    long ln = fsa.n();
    long rn = rhs.fsa.n();
    for (long f : fsa.finals) {
        fsa.adj[f].emplace(fsa.adj[f].begin(), -1, ln + rhs.fsa.start);
    }
    for (auto& es: rhs.fsa.adj) {
        for (auto& e: es) {
            e.second += ln;
        }
        fsa.adj.emplace_back(std::move(es));
    }
    fsa.finals = std::move(rhs.fsa.finals);
    for (long& f: fsa.finals) {
        f += ln;
    }
    assoc.resize(fsa.n());
    REP (i, rhs.fsa.n()) {
        assoc[ln + i] = std::move(rhs.assoc[i]);
    }
    if (expr) {
        add_assoc(*expr);
    }
    deterministic = false;
}

void sort_assoc(std::vector<std::pair<Expr*, ExprTag>> &as) {
    std::sort(ALL(as));
    auto i = as.begin();
    auto j = i;
    auto k = i;
    for (; i != as.end(); i = j) {
        while (++j != as.end() && i->first == j->first) {
            i->second = ExprTag(long(i->second) | long(j->second));
        }
        *k++ = *i;
    }
    as.erase(k, as.end());
}

void FsaAnno::determinize() {
    if (deterministic)  {
        return;
    }
    decltype(assoc) new_assoc;
    auto relate = [&] (const std::vector<long> &xs) {
        new_assoc.emplace_back();
        auto &as = new_assoc.back();
        for (long x : xs) {
            as.insert(as.end(), ALL(assoc[x]));
        }
        std::sort(ALL(as));
    };
    fsa = fsa.determinize(relate);
    assoc = std::move(new_assoc);
    deterministic = true;
}

void FsaAnno::difference(FsaAnno& rhs, DifferenceExpr *expr) {
    std::vector<std::vector<long>> rel0;
    decltype(rhs.assoc) new_assoc;
    auto relate0 = [&](const std::vector<long>& xs) {
        rel0.emplace_back(xs);
    };
    auto relate = [&](long x) {
        if (rel0.empty()) {
            new_assoc.emplace_back(assoc[x]);
        } else {
            new_assoc.emplace_back();
            auto &as = new_assoc.back();
            for (long u: rel0[x]) {
                as.insert(as.end(), ALL(assoc[u]));
            }
            sort_assoc(as);
        }
    };
    if (!deterministic) {
        fsa = fsa.determinize(relate0);
    }
    if (!rhs.deterministic) {
        rhs.fsa = rhs.fsa.determinize([](const std::vector<long>&) {});
    }
    fsa = fsa.difference(rhs.fsa, relate);
    assoc = std::move(new_assoc);
    if (expr) {
        add_assoc(*expr);
    }
    deterministic = true;
}

void FsaAnno::embed(EmbedExpr &expr) {
    // TODO
    /*
    fsa.start = 0;
    fsa.finals = {0};
    fsa.adj.assign(1, {});
    assoc.clear();
    assoc.emplace_back(1, &expr);
    */
}

void FsaAnno::intersect(FsaAnno& rhs, IntersectExpr *expr) {
    decltype(rhs.assoc) new_assoc;
    std::vector<std::vector<long>> rel0, rel1;
    auto relate0 = [&](const std::vector<long>& xs) {
        rel0.emplace_back(xs);
    };
    auto relate1 = [&](const std::vector<long>& xs) {
        rel1.emplace_back(xs);
    };
    auto relate = [&](long x, long y) {
        new_assoc.emplace_back();
        auto &as = new_assoc.back();
        if (rel0.empty())
            as.insert(as.end(), ALL(assoc[x]));
        else
            for (long u: rel0[x])
                as.insert(as.end(), ALL(assoc[u]));
        if (rel1.empty())
            as.insert(as.end(), ALL(rhs.assoc[y]));
        else
            for (long v: rel1[y])
                as.insert(as.end(), ALL(rhs.assoc[v]));
        sort_assoc(as);
    };
    if (! deterministic)
        fsa = fsa.determinize(relate0);
    if (! rhs.deterministic)
        rhs.fsa = rhs.fsa.determinize(relate1);
    fsa = fsa.intersect(rhs.fsa, relate);
    assoc = std::move(new_assoc);
    if (expr) {
        add_assoc(*expr);
    }
    deterministic = true;
}

void FsaAnno::minimize() {
    assert(deterministic);
    decltype(assoc) new_assoc;
    auto relate = [&] (std::vector<long> & xs) {
        new_assoc.emplace_back();
        auto &as = new_assoc.back();
        for (long x : xs) {
            as.insert(as.end(), ALL(assoc[x]));
        }
        sort_assoc(as);
    };
    fsa = fsa.hopcroft_minimize(relate);
    assoc = std::move(new_assoc);
}

void FsaAnno::union_(FsaAnno &rhs, UnionExpr *expr) {
    long ln = fsa.n();
    long rn = rhs.fsa.n();
    long src = ln + rn;
    long old_lsrc = fsa.start;
    fsa.start = src;
    for (long f: rhs.fsa.finals) {
        fsa.finals.push_back(ln + f);
    }
    for (auto &es : rhs.fsa.adj) {
        for (auto &e : es) {
            e.second += ln;
        }
        fsa.adj.emplace_back(std::move(es));
    }
    fsa.adj.emplace_back();
    fsa.adj[src].emplace_back(-1, old_lsrc);
    fsa.adj[src].emplace_back(-1, ln + rhs.fsa.start);
    assoc.resize(fsa.n());
    REP (i, rhs.fsa.n()) {
        assoc[ln + i] = std::move(rhs.assoc[i]);
    }
    if (expr) {
        add_assoc(*expr);
    }
    deterministic = false;
}

void FsaAnno::plus(PlusExpr *expr) {
    for (long f: fsa.finals) {
        sorted_insert(fsa.adj[f], std::make_pair(-1L, fsa.start));
    }
    if (expr) {
        add_assoc(*expr);
    }
    deterministic = false;
}

void FsaAnno::question(MaybeExpr *expr) {
    long src = fsa.n();
    long sink = src + 1;
    long old_src = fsa.start;

    fsa.start = src;
    fsa.adj.emplace_back();
    fsa.adj.emplace_back();
    fsa.adj[src].emplace_back(-1, old_src);
    fsa.adj[src].emplace_back(-1, sink);
    fsa.finals.push_back(sink);
    assoc.resize(fsa.n());
    if (expr) {
        add_assoc(*expr);
    }
    deterministic = false;
}

void FsaAnno::repeat(RepeatExpr &expr) {
    FsaAnno r = epsilon(NULL);
    REP (i, expr.low) {
        FsaAnno t = *this;
        r.concat(t, NULL);
    }
    if (expr.high == LONG_MAX) {
        star(NULL);
        r.concat(*this, NULL);
    } else if (expr.low < expr.high) {
        FsaAnno rhs = epsilon(NULL);
        FsaAnno x = *this;
        ROF (i, 0, expr.high - expr.low) {
            FsaAnno t = x;
            rhs.union_(t, NULL);
            if (i) {
                t = *this;
                x.concat(t, NULL);
            }
        }
        r.concat(rhs, NULL);
    }
    r.deterministic = false;
    *this = std::move(r);
}


void FsaAnno::star(ClosureExpr *expr) {
    long src = fsa.n();
    long sink = src + 1;
    long old_src = fsa.start;

    fsa.start = src;
    fsa.adj.emplace_back();
    fsa.adj.emplace_back();
    fsa.adj[src].emplace_back(-1, old_src);
    fsa.adj[src].emplace_back(-1, sink);
    for (long f: fsa.finals) {
        sorted_insert(fsa.adj[f], std::make_pair(-1L, old_src));
        sorted_insert(fsa.adj[f], std::make_pair(-1L, sink));
    }
    fsa.finals.assign(1, sink);
    assoc.resize(fsa.n());
    if (expr) {
        add_assoc(*expr);
    }
    deterministic = false;
}

void FsaAnno::substring_grammar() {
    long src = fsa.n();
    long sink = src + 1;
    long old_src = fsa.start;
    fsa.start = src;
    fsa.adj.emplace_back();
    fsa.adj.emplace_back();
    REP (i, src) {
        bool ok = true;
        for (auto aa : assoc[i]) {
            if (auto e = dynamic_cast<CollapseExpr*>(aa.first)) {
                if (e->define_stmt->intact && long(aa.second) & long(ExprTag::inner)) {
                    ok = false;
                    break;
                }
            } else if (aa.first->stmt->intact && long(aa.second) & long(ExprTag::inner)) {
                ok = false;
                break;
            }
        }
        if (ok || i == old_src) {
            fsa.adj[src].emplace_back(-1, i);
        }
        if (ok || fsa.is_final(i)) {
            sorted_insert(fsa.adj[i], std::make_pair(-1L, sink));
        }
    }
    fsa.finals.assign(1, sink);
    assoc.resize(fsa.n());
    deterministic = false;
}

