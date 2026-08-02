#include "fsa_anno.hh"
#include "compiler.hh"

#include <algorithm>
#include <utility>
#include <cassert>
#include <string.h>
#include <limits.h>
#include <map>
#include <unicode/utf8.h>

#define DEBUG_FSA 1

bool operator<(ExprTag x, ExprTag y) {
    return long(x) < long(y);
}

bool assoc_has_expr(std::vector<std::pair<Expr*, ExprTag>> &as, Expr *x) {
    auto it = std::lower_bound(ALL(as),
            std::make_pair(x, ExprTag(0)));
    return it != as.end() && it->first == x;
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

FsaAnno FsaAnno::bracket(BracketExpr &expr) {
    FsaAnno r;
    r.fsa.start = 0;
    r.fsa.finals = {1};
    r.fsa.adj.resize(2);
    for (auto &x : expr.intervals.to) {
        r.fsa.adj[0].emplace_back(x, 1);
    }
    r.assoc.resize(2);
    r.add_assoc(expr);
    r.deterministic = true;
#ifdef DEBUG_FSA
    printf("print BracketExpr fsa\n");
    print_fsa(r.fsa);
    print_assoc(r);
    printf("print BracketExpr fsa done\n");
#endif
    return r;
}

FsaAnno FsaAnno::call(CallExpr &expr) {
    FsaAnno r;
    r.fsa.start = 0;
    r.fsa.finals = {1};
    r.fsa.adj.resize(2);
    r.fsa.adj[0].emplace_back(std::make_pair(call_label, call_label + 1), 1);
    call_label++;
    r.assoc.resize(2);
    r.add_assoc(expr);
    r.deterministic = true;
#ifdef DEBUG_FSA
    printf("print CallExpr fsa\n");
    print_fsa(r.fsa);
    print_assoc(r);
    printf("print CallExpr fsa done\n");
#endif
    return r;
}

FsaAnno FsaAnno::collapse(CollapseExpr &expr) {
    FsaAnno r;
    r.fsa.start = 0;
    r.fsa.finals = {1};
    r.fsa.adj.resize(2);
    r.fsa.adj[0].emplace_back(std::make_pair(collapse_label, collapse_label + 1), 1);
    collapse_label++;
    r.assoc.resize(2);
    r.add_assoc(expr);
    r.deterministic = true;
#ifdef DEBUG_FSA
    printf("print CollapseExpr fsa\n");
    print_fsa(r.fsa);
    print_assoc(r);
    printf("print CollapseExpr fsa done\n");
#endif
    return r;
}

FsaAnno FsaAnno::dot(DotExpr *expr) {
    FsaAnno r;
    r.fsa.start = 0;
    r.fsa.finals = {1};
    r.fsa.adj.resize(2);
    r.fsa.adj[0].emplace_back(std::make_pair(0L, AB), 1);
    r.assoc.resize(2);
    if (expr) {
        r.add_assoc(*expr);
    }
    r.deterministic = true;
#ifdef DEBUG_FSA
    printf("print DotExpr fsa\n");
    print_fsa(r.fsa);
    print_assoc(r);
    printf("print DotExpr fsa done\n");
#endif
    return r;
}

FsaAnno FsaAnno::embed(EmbedExpr &expr) {
    if (expr.define_stmt) {
        FsaAnno r = compiled[expr.define_stmt];
        REP (i, r.fsa.n()) {
            auto it = upper_bound(ALL(r.fsa.adj[i]),
                    std::make_pair(
                        std::make_pair(call_label_base, LONG_MAX),
                        LONG_MAX
                    )
            );
            if (it != r.fsa.adj[i].begin() &&
                    call_label_base < (it - 1)->first.second) {
                --it;
            }
            for (; it != r.fsa.adj[i].end() && it->first.first < call_label; ++it) {
                assert(call_label_base <= it->first.first);
                long t = it->first.second - it->first.first;
                it->first.first = call_label;
                call_label += t;
                it->first.second = call_label;
                assert(it->first.second <= call_label);
            }
        }
        r.add_assoc(expr);
#ifdef DEBUG_FSA
        printf("print embedExpr fsa\n");
        print_fsa(r.fsa);
        print_assoc(r);
        printf("print embedExpr fsa done\n");
#endif
        return r;
    } else {
        FsaAnno r;
        r.fsa.start = 0;
        r.fsa.finals = {1};
        r.fsa.adj.resize(2);
        r.fsa.adj[0].emplace_back(std::make_pair(expr.macro_value, expr.macro_value + 1), 1);
        r.assoc.resize(2);
        r.add_assoc(expr);
        r.deterministic = true;
#ifdef DEBUG_FSA
        printf("print embedExpr fsa\n");
        print_fsa(r.fsa);
        print_assoc(r);
        printf("print embedExpr fsa done\n");
#endif
        return r;
    }
}

FsaAnno FsaAnno::epsilon_fsa(EpsilonExpr *expr) {
    FsaAnno r;
    r.fsa.start = 0;
    r.fsa.finals.push_back(0);
    r.fsa.adj.resize(1);
    r.assoc.resize(1);
    if (expr) {
        r.add_assoc(*expr);
    }
    r.deterministic = true;
#ifdef DEBUG_FSA
    printf("print EpsilonExpr fsa\n");
    print_fsa(r.fsa);
    print_assoc(r);
    printf("print EpsilonExpr fsa done\n");
#endif
    return r;
}

FsaAnno FsaAnno::literal(LiteralExpr &expr) {
    FsaAnno r;
    r.fsa.start = 0;
    long len = 0;
    for (i32 c = 0, i = 0; i < expr.literal.size(); len++) {
        U8_NEXT_OR_FFFD(expr.literal.c_str(), i, expr.literal.size(), c);
        r.fsa.adj.emplace_back();
        r.fsa.adj[len].emplace_back(std::make_pair(c, c + 1), len + 1);
    }
    r.fsa.adj.emplace_back();
    r.fsa.finals.push_back(len);
    r.assoc.resize(len + 1);
    r.add_assoc(expr);
    r.deterministic = true;
#ifdef DEBUG_FSA
    printf("print LiteralExpr fsa\n");
    print_fsa(r.fsa);
    print_assoc(r);
    printf("print LiteralExpr fsa done\n");
#endif
    return r;
}

void FsaAnno::accessible(const std::vector<long> *start, std::vector<long> &mapping) {
    long allo = 0;
    auto relate = [&] (long x) {
        if (allo != x) {
            assoc[allo] = std::move(assoc[x]);
        }
        allo++;
    };
    fsa.accessible(start, relate);
    assoc.resize(allo);
}

void FsaAnno::co_accessible(const std::vector<bool> *final, std::vector<long> &mapping) {
    long allo = 0;
    auto relate = [&] (long x) {
        if (allo != x) {
            assoc[allo] = std::move(assoc[x]);
        }
        allo++;
    };
    fsa.co_accessible(final, relate);
    if (fsa.finals.empty()) {
        assoc.assign(1, {});
        mapping.assign(1, 0);
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
    if (expr.no_action() &&             // 无action 且无intact 且一般表达式
            !expr.stmt->intact &&
            !dynamic_cast<CallExpr*>(&expr) &&
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
    if (expr.leaving.size() || expr.entering.size() || expr.transiting.size()) {
        for (auto action : expr.transiting) {
            REP (i, fsa.n()) {
                fsa.adj[i].emplace_back(std::make_pair(action_label, action_label + 1), i);
                action_label++;
            }
        }
    } else if (expr.finishing.size()) {
        for (long f : fsa.finals)  {
            fsa.adj[f].emplace_back(std::make_pair(action_label, action_label + 1), f);
            action_label++;
        }
    }
}

void FsaAnno::complement(ComplementExpr *expr) {
    if (!deterministic) {
        fsa = fsa.determinize(NULL, [&](long, const std::vector<long>&) {});
    }
    fsa = ~fsa;
    assoc.assign(fsa.n(), {});
    deterministic = true;
}

void FsaAnno::concat(FsaAnno& rhs, ConcatExpr *expr) {
    long ln = fsa.n();
    long rn = rhs.fsa.n();
    for (long f : fsa.finals) {
        emplace_front(fsa.adj[f], epsilon, ln + rhs.fsa.start);
    }
    for (auto& es: rhs.fsa.adj) {
        for (auto& e: es) {
            e.second += ln;
        }
        fsa.adj.emplace_back(std::move(es));
    }
    fsa.finals = std::move(rhs.fsa.finals);
    for (long &f : fsa.finals) {
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
#ifdef DEBUG_FSA
    printf("print concat fsa\n");
    print_fsa((*this).fsa);
    print_assoc(*this);
    printf("print concat fsa done\n");
#endif
}

void FsaAnno::determinize(const std::vector<long> *starts, std::vector<std::vector<long>> *mapping) {
    if (deterministic)  {
        return;
    }
    decltype(assoc) new_assoc;
    auto relate = [&] (long id, const std::vector<long> &xs) {
        if (id + 1 > new_assoc.size()) {
            new_assoc.resize(id + 1);
            if (mapping) {
                mapping->resize(id + 1);
            }
        }
        auto &as = new_assoc[id];
        for (long x : xs) {
            as.insert(as.end(), ALL(assoc[x]));
        }
        sort_assoc(as);
        if (mapping) {
            (*mapping)[id] = xs;
        }
    };
    fsa = fsa.determinize(starts, relate);
    assoc = std::move(new_assoc);
    deterministic = true;
#ifdef DEBUG_FSA
    printf("print determinize fsa\n");
    print_fsa((*this).fsa);
    print_assoc(*this);
    printf("print determinize fsa done\n");
#endif
}

void FsaAnno::difference(FsaAnno& rhs, DifferenceExpr *expr) {
    std::vector<std::vector<long>> rel0;
    decltype(rhs.assoc) new_assoc;
    auto relate0 = [&](long id, const std::vector<long>& xs) {
        if (id + 1 > rel0.size()) {
            rel0.resize(id + 1);
        }
        rel0[id] = xs;
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
        fsa = fsa.determinize(NULL, relate0);
    }
    if (!rhs.deterministic) {
        rhs.fsa = rhs.fsa.determinize(NULL, [](long, const std::vector<long>&) {});
    }
    fsa = fsa.difference(rhs.fsa, relate);
    assoc = std::move(new_assoc);
    if (expr) {
        add_assoc(*expr);
    }
    deterministic = true;
#ifdef DEBUG_FSA
    printf("print difference fsa\n");
    print_fsa((*this).fsa);
    print_assoc(*this);
    printf("print difference fsa done\n");
#endif
}

void FsaAnno::intersect(FsaAnno& rhs, IntersectExpr *expr) {
    decltype(rhs.assoc) new_assoc;
    std::vector<std::vector<long>> rel0, rel1;
    auto relate0 = [&](long id, const std::vector<long>& xs) {
        if (id + 1 > rel0.size()) {
            rel0.resize(id + 1);
        }
        rel0[id] = xs;
    };
    auto relate1 = [&](long id, const std::vector<long>& xs) {
        if (id + 1 > rel1.size()) {
            rel1.resize(id + 1);
        }
        rel1[id] = xs;
    };
    auto relate = [&](long x, long y) {
        new_assoc.emplace_back();
        auto &as = new_assoc.back();
        if (rel0.empty()) {
            as.insert(as.end(), ALL(assoc[x]));
        } else {
            for (long u: rel0[x]) {
                as.insert(as.end(), ALL(assoc[u]));
            }
        }
        if (rel1.empty()) {
            as.insert(as.end(), ALL(rhs.assoc[y]));
        } else {
            for (long v: rel1[y]) {
                as.insert(as.end(), ALL(rhs.assoc[v]));
            }
        }
        sort_assoc(as);
    };
    if (!deterministic) {
        fsa = fsa.determinize(NULL, relate0);
    }
    if (!rhs.deterministic) {
        rhs.fsa = rhs.fsa.determinize(NULL, relate1);
    }
    fsa = fsa.intersect(rhs.fsa, relate);
    assoc = std::move(new_assoc);
    if (expr) {
        add_assoc(*expr);
    }
    deterministic = true;
#ifdef DEBUG_FSA
    printf("print intersect fsa\n");
    print_fsa((*this).fsa);
    print_assoc(*this);
    printf("print intersect fsa done\n");
#endif
}

void FsaAnno::minimize(std::vector<std::vector<long>> *mapping) {
    assert(deterministic);
    decltype(assoc) new_assoc;
    auto relate = [&] (std::vector<long> & xs) {
        new_assoc.emplace_back();
        auto &as = new_assoc.back();
        for (long x : xs) {
            as.insert(as.end(), ALL(assoc[x]));
        }
        sort_assoc(as);
        if (mapping) {
            mapping->push_back(xs);
        }
    };
    fsa = fsa.hopcroft_minimize(relate);
    assoc = std::move(new_assoc);
#ifdef DEBUG_FSA
    printf("print minimize fsa\n");
    print_fsa((*this).fsa);
    print_assoc(*this);
    printf("print minimize fsa done\n");
#endif
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
    fsa.adj[src].emplace_back(epsilon, old_lsrc);
    fsa.adj[src].emplace_back(epsilon, ln + rhs.fsa.start);
    assoc.resize(fsa.n());
    REP (i, rhs.fsa.n()) {
        assoc[ln + i] = std::move(rhs.assoc[i]);
    }
    if (expr) {
        add_assoc(*expr);
    }
    deterministic = false;
#ifdef DEBUG_FSA
    printf("print union fsa\n");
    print_fsa((*this).fsa);
    print_assoc(*this);
    printf("print union fsa done\n");
#endif
}

void FsaAnno::plus(PlusExpr *expr) {
    for (long f: fsa.finals) {
        emplace_front(fsa.adj[f], epsilon, fsa.start);
    }
    if (expr) {
        add_assoc(*expr);
    }
    deterministic = false;
#ifdef DEBUG_FSA
    printf("print plus fsa\n");
    print_fsa((*this).fsa);
    print_assoc(*this);
    printf("print plus fsa done\n");
#endif
}

void FsaAnno::question(MaybeExpr *expr) {
    long src = fsa.n();
    long sink = src + 1;
    long old_src = fsa.start;

    fsa.start = src;
    fsa.adj.emplace_back();
    fsa.adj.emplace_back();
    fsa.adj[src].emplace_back(epsilon, old_src);
    fsa.adj[src].emplace_back(epsilon, sink);
    fsa.finals.push_back(sink);
    assoc.resize(fsa.n());
    if (expr) {
        add_assoc(*expr);
    }
    deterministic = false;
#ifdef DEBUG_FSA
    printf("print question fsa\n");
    print_fsa((*this).fsa);
    print_assoc(*this);
    printf("print question fsa done\n");
#endif
}

void FsaAnno::repeat(RepeatExpr &expr) {
    FsaAnno r = epsilon_fsa(NULL);
    REP (i, expr.low) {
        FsaAnno t = *this;
        r.concat(t, NULL);
    }
    if (expr.high == LONG_MAX) {
        star(NULL);
        r.concat(*this, NULL);
    } else if (expr.low < expr.high) {
        FsaAnno rhs = epsilon_fsa(NULL);
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
#ifdef DEBUG_FSA
    printf("print repeate fsa\n");
    print_fsa((*this).fsa);
    print_assoc(*this);
    printf("print repeate fsa done\n");
#endif
}

void FsaAnno::star(ClosureExpr *expr) {
    long src = fsa.n();
    long sink = src + 1;
    long old_src = fsa.start;

    fsa.start = src;
    fsa.adj.emplace_back();
    fsa.adj.emplace_back();
    fsa.adj[src].emplace_back(epsilon, old_src);
    fsa.adj[src].emplace_back(epsilon, sink);
    for (long f: fsa.finals) {
        sorted_emplace(fsa.adj[f], epsilon, old_src);
        sorted_emplace(fsa.adj[f], epsilon, sink);
    }
    fsa.finals.assign(1, sink);
    assoc.resize(fsa.n());
    if (expr) {
        add_assoc(*expr);
    }
    deterministic = false;
#ifdef DEBUG_FSA
    printf("print star fsa\n");
    print_fsa((*this).fsa);
    print_assoc(*this);
    printf("print star fsa done\n");
#endif
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
                if (e->define_stmt->intact && has_inner(aa.second)) {
                    ok = false;
                    break;
                }
            } else if (aa.first->stmt->intact && has_inner(aa.second)) {
                ok = false;
                break;
            }
        }
        if (ok || i == old_src) {
            fsa.adj[src].emplace_back(epsilon, i);
        }
        if (ok || fsa.is_final(i)) {
            emplace_front(fsa.adj[i], epsilon, sink);
        }
    }
    fsa.finals.assign(1, sink);
    assoc.resize(fsa.n());
    deterministic = false;
#ifdef DEBUG_FSA
    printf("print substring fsa\n");
    print_fsa((*this).fsa);
    print_assoc(*this);
    printf("print substring fsa done\n");
#endif
}

