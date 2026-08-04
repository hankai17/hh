#include "compiler.hh"
#include "common.hh"
#include "loader.hh"

#include <map>
#include <stack>
#include <iostream>
#include <functional>
#include <unordered_map>
#include <algorithm>
#include <cassert>
#include <climits>

#define DEBUG_COMP 1

std::unordered_map<DefineStmt*, FsaAnno> compiled;
static std::unordered_map<DefineStmt*, std::vector<std::pair<long, long>>> stmt2call_addr;
static std::unordered_map<DefineStmt*, std::vector<bool>> stmt2final;

void print_assoc(const FsaAnno &anno) {
    printf("====== FSA Associated Expr of each state\n");
    REP (i, anno.fsa.n()) {
        printf("%ld: ", i);
        for (auto aa : anno.assoc[i]) {
            auto a = aa.first;
            printf(" %s%s%s%s(%ld-%ld)",
                    a->name().c_str(),
                    has_start(aa.second) ? "^" : "",
                    has_inner(aa.second) ? "." : "",
                    has_final(aa.second) ? "$" : "",
                    a->loc.start, a->loc.end);
            if (a->entering.size()) {
                printf(",>%zd", a->entering.size());
            }
            if (a->leaving.size()) {
                printf(",%%%zd", a->leaving.size());
            }
            if (a->finishing.size()) {
                printf(",@%zd", a->finishing.size());
            }
            if (a->transiting.size()) {
                printf(",$%zd", a->transiting.size());
            }
            printf(")");
        }
        puts("");
    }
    puts("");
}

void print_fsa(const Fsa &fsa) {
    printf("====== FSA Automaton\n");
    printf("start: %ld\n", fsa.start);
    printf("finals:");
    for (long i : fsa.finals) {
        printf(" %ld", i);
    }
    puts("");
    puts("edges:");
    REP (i, fsa.n()) {
        printf("%ld: ", i);
        for (auto it = fsa.adj[i].begin(); it != fsa.adj[i].end();) {
            long from = it->first.first;
            long to = it->first.second;
            long v = it->second;
            while (++it != fsa.adj[i].end() &&
                    to == it->first.first &&
                    it->second == v) {
                to = it->first.second;
            }
            if (from == to - 1) {
                printf(" (%ld,%ld)", from, v);
            } else {
                printf(" (%ld-%ld,%ld)", from, to - 1, v);
            }
        }
        puts("");
    }
    puts("");
}

Expr *find_lca(Expr *u, Expr *v) {
    if (u->depth > v->depth) {
        std::swap(u, v);
    }
    if (u->depth < v->depth) {
        for (long k = 63 - __builtin_clzl(v->depth - u->depth); k >= 0; k--) {
            if (u->depth <= v->depth - (1L << k)) {
                v = v->anc[k];
            }
        }
    }
    if (u == v) {
        return u;
    }
    if (v->depth) {
        for (long k = 63 - __builtin_clzl(v->depth); k >= 0; k--) {
            if (k < u->anc.size() &&
                    u->anc[k] != v->anc[k]) {
                u = u->anc[k];
                v = v->anc[k];
            }
        }
    }
    return u->anc[0];
}

struct Compiler : Visitor<Expr> {
    std::stack<FsaAnno> st;
    std::stack<Expr*> path;
    long tick = 0;

    void pre_expr(Expr &expr) {
        expr.pre = tick++;
        expr.depth = path.size();
        if (path.size()) {
            expr.anc.assign(1, path.top());
            for (long k = 1; 1L << k <= expr.depth; k++) {
                expr.anc.push_back(expr.anc[k - 1]->anc[k - 1]);
            }
        } else {
            expr.anc.assign(1, nullptr);
        }
        path.push(&expr);
#ifdef DEBUG_COMP
        printf("stmt: %s, expr:%s(%ld-%ld), depth:%ld\n",
                expr.stmt->lhs.c_str(),
                expr.name().c_str(),
                expr.loc.start,
                expr.loc.end,
                expr.depth);
#endif
    }

    void post_expr(Expr &expr) {
        path.pop();
        expr.post = tick;
        st.top().fsa.check();
    }

    void visit(Expr &expr) override {
        pre_expr(expr);
        expr.accept(*this);
        post_expr(expr);
    }

    void visit(BracketExpr &expr) override {
#ifdef DEBUG_COMP
        std::cout << "Compiler visit BracketExpr" << std::endl;
#endif
        st.push(FsaAnno::bracket(expr));
    }

    void visit(CallExpr &expr) override {
#ifdef DEBUG_COMP
        std::cout << "Compiler visit CallExpr" << std::endl;
#endif
        st.push(FsaAnno::call(expr));
    }

    void visit(ClosureExpr &expr) override {
#ifdef DEBUG_COMP
        std::cout << "Compiler visit ClosureExpr" << std::endl;
#endif
        visit(*expr.inner);
        st.top().star(&expr);
    }

    void visit(CollapseExpr &expr) override {
#ifdef DEBUG_COMP
        std::cout << "Compiler visit CollapseExpr" << std::endl;
#endif
        st.push(FsaAnno::collapse(expr));
    }

    void visit(ComplementExpr &expr) override {
#ifdef DEBUG_COMP
        std::cout << "Compiler visit ComplementExpr" << std::endl;
#endif
        visit(*expr.inner);
        st.top().complement(&expr);
    }

    void visit(ConcatExpr &expr) override {
#ifdef DEBUG_COMP
        std::cout << "Compiler visit ConcatExpr" << std::endl;
#endif
        visit(*expr.rhs);
        FsaAnno rhs = std::move(st.top());
        visit(*expr.lhs);
        st.top().concat(rhs, &expr);
    }

    void visit(DifferenceExpr &expr) override {
#ifdef DEBUG_COMP
        std::cout << "Compiler visit DifferenceExpr" << std::endl;
#endif
        visit(*expr.rhs);
        FsaAnno rhs = std::move(st.top());
        visit(*expr.lhs);
        st.top().difference(rhs, &expr);
    }

    void visit(DotExpr &expr) override {
#ifdef DEBUG_COMP
        std::cout << "Compiler visit DotExpr" << std::endl;
#endif
        st.push(FsaAnno::dot(&expr));
    }

    void visit(EmbedExpr &expr) override {
#ifdef DEBUG_COMP
        std::cout << "Compiler visit EmbedExpr" << std::endl;
#endif
        st.push(FsaAnno::embed(expr));
    }

    void visit(EpsilonExpr &expr) override {
#ifdef DEBUG_COMP
        std::cout << "Compiler visit EpsilonExpr" << std::endl;
#endif
        st.push(FsaAnno::epsilon_fsa(&expr));
    }

    void visit(IntersectExpr &expr) override {
#ifdef DEBUG_COMP
        std::cout << "Compiler visit IntersectExpr" << std::endl;
#endif
        visit(*expr.rhs);
        FsaAnno rhs = std::move(st.top());
        visit(*expr.lhs);
        st.top().intersect(rhs, &expr);
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
        visit(*expr.inner);
        st.top().question(&expr);
    }

    void visit(PlusExpr &expr) override {
#ifdef DEBUG_COMP
        std::cout << "Compiler visit PlusExpr" << std::endl;
#endif
        visit(*expr.inner);
        st.top().plus(&expr);
    }

    void visit(RepeatExpr &expr) override {
#ifdef DEBUG_COMP
        std::cout << "Compiler visit RepeatExpr" << std::endl;
#endif
        visit(*expr.inner);
        st.top().repeat(expr);
    }

    void visit(UnionExpr &expr) override {
#ifdef DEBUG_COMP
        std::cout << "Compiler visit UnionExpr" << std::endl;
#endif
        visit(*expr.rhs);
        FsaAnno rhs = std::move(st.top());
        visit(*expr.lhs);
        st.top().union_(rhs, &expr);
    }
};

void compile(DefineStmt *stmt) {
    if (compiled.count(stmt)) {
        return;
    }
    FsaAnno &anno = compiled[stmt];
    Compiler comp;
    comp.visit(*stmt->rhs);
    anno = std::move(comp.st.top());
    anno.determinize(NULL, NULL);
    anno.minimize(NULL);
    printf("compile, determinize, minimize done, size(%s::%s) = %ld\n",
            stmt->module->filename.c_str(),
            stmt->lhs.c_str(),
            anno.fsa.n());
}

void generate_transitions(DefineStmt *stmt) {
    FsaAnno &anno = compiled[stmt];
    auto &call_addr = stmt2call_addr[stmt];
    auto &sub_final = stmt2final[stmt];
    auto find_within = [&] (long u) {
        std::vector<std::pair<Expr*, ExprTag>> within;
        Expr *last = NULL;
        std::sort(ALL(anno.assoc[u]), [](const std::pair<Expr*, ExprTag> &x,
                const std::pair<Expr*, ExprTag> &y) {
            if (x.first->pre != y.first->pre) {
                return x.first->pre < y.first->pre;
            }
            return x.second < y.second;
        });
        for (auto aa : anno.assoc[u]) {
            Expr *stop = last ? find_lca(last, aa.first) : NULL;
            last = aa.first;
            for (Expr *x = aa.first; x != stop; x = x->anc[0]) {
                within.emplace_back(x, aa.second);
            }
        }
        std::sort(ALL(within));
        auto j = within.begin();
        for (auto i = within.begin(); i != within.end();) {
            Expr *x = i->first;
            long t = long(i->second);
            while (++i != within.end() && x == i->first) {
                t |= long(i->second);
            }
            *j++ = { x, ExprTag(t) };
        }
        within.erase(j, within.end());
        return within;
    };
    decltype(anno.assoc) withins(anno.fsa.n());
    REP (i, anno.fsa.n()) {
        withins[i] = std::move(find_within(i));
    }
    auto get_code = [] (Action *action) {
        if (auto t = dynamic_cast<InlineAction*>(action)) {
            return t->code;
        } else if (auto t = dynamic_cast<RefAction*>(action)) {
            return t->define_stmt->code;
        } else {
            assert(0);
        }
        return std::string();
    };
#define D(S)                                                                    \
    if (auto t = dynamic_cast<InlineAction*>(action.first)) {                   \
        if (from == to - 1) {                                                   \
            printf(" %ld, %ld, %ld, %s\n", u, from, v, t->code.c_str());        \
        } else {                                                                \
            printf(" %ld %ld-%ld %ld %s\n",                                     \
                u, from, to - 1, v, t->code.c_str());                           \
        }                                                                       \
    } else if (auto t = dynamic_cast<RefAction*>(action.first)) {               \
        if (from == to - 1) {                                                   \
            printf(" %ld, %ld, %ld, %s\n", u, from, v,                          \
                t->define_stmt->code.c_str());                                  \
        } else {                                                                \
            printf(" %ld %ld-%ld %ld %s\n",                                     \
                u, from, to - 1, v, t->define_stmt->code.c_str());              \
        }                                                                       \
    }

    fprintf(output, "long hh_%s_transit(std::vector<long> &ret_stack, long u, long c)\n", stmt->lhs.c_str());
    if (stmt->export_params.size()) {
        fprintf(output, ", %s);\n", stmt->export_params.c_str());
    }
    fprintf(output,
            "{\n"
            "   long v = -1;\n"
            "again:\n"
            "   switch (u) {\n"
    );
    REP (u, anno.fsa.n()) {
        if (call_addr[u].first >= 0) {
            fprintf(output,
                    "   case %ld:\n"
                    "       u = %ld;\n",
            u, call_addr[u].first);

            fprintf(output,
                    "   ret_stack.push_back(%ld);\n"
                    "   goto again;\n",
            call_addr[u].second);
            continue;
        }
        if (anno.fsa.adj[u].empty() && !sub_final[u]) {
            continue;
        }
        ident(output, 1);
        fprintf(output, "case %ld:\n", u);
        ident(output, 2);
        fprintf(output, "switch (c) {\n");
        std::unordered_map<
            long,
            std::pair<
                    std::vector<std::pair<long, long>>,
                    std::vector<std::pair<Action*, long>>
            >
        > v2case;
        for (auto it = anno.fsa.adj[u].begin(); it != anno.fsa.adj[u].end();) {
            long from = it->first.first;
            long to = it->first.second;
            long v = it->second;
            while (++it != anno.fsa.adj[u].end() &&
                    to == it->first.first &&
                    it->second == v) {
                to = it->first.second;
            }
            v2case[v].first.emplace_back(from, to);
            auto &body = v2case[v].second;
            auto ie = withins[u].end();
            auto je = withins[v].end();
            for (auto i = withins[u].begin(), j = withins[v].begin(); i != ie; ++i) {
                while (j != je && i->first > j->first) {
                    ++j;
                }
                if (j == je || i->first != j->first) {
                    for (auto action : i->first->leaving) {
                        body.push_back(action);
                    }
                }
            }
            for (auto i = withins[u].begin(), j = withins[v].begin(); j != je; ++j) {
                while (i != ie && i->first < j->first) {
                    ++i;
                }
                if (i == ie || i->first != j->first) {
                    for (auto action : j->first->entering) {
                        body.push_back(action);
                    }
                }
            }
            for (auto i = withins[u].begin(), j = withins[v].begin(); j != je; ++j) {
                while (i != ie && i->first < j->first) {
                    ++i;
                }
                if (i != ie && i->first == j->first) {
                    for (auto action : j->first->transiting) {
                        body.push_back(action);
                    }
                }
            }
            for (auto i = withins[u].begin(), j = withins[v].begin(); j != je; ++j) {
                while (i != ie && i->first < j->first) {
                    ++i;
                }
                if (i != ie && i->first == j->first &&
                        has_final(j->second)) {
                    for (auto action : j->first->finishing) {
                        body.push_back(action);
                    }
                }
            }
        }
        for (auto & x : v2case) {
            for (auto &y : x.second.first) {
                ident(output, 2);
                if (y.first == y.second - 1) {
                    fprintf(output, "case %ld:\n", y.first);
                } else {
                    fprintf(output, "case %ld ... %ld:\n", y.first, y.second - 1);
                }
            }
            ident(output, 3);
            fprintf(output, "v = %ld;\n", x.first);
            
            std:;sort(ALL(x.second.second), [] (const std::pair<Action*, long> &a0,
                    const std::pair<Action*, long> &a1) {
                return a0.second != a1.second ?
                        a0.second < a1.second : a0.first < a1.first;
            });
            x.second.second.erase(std::unique(ALL(x.second.second)), x.second.second.end());
            for (auto a : x.second.second) {
                fprintf(output, "{%s}\n", get_code(a.first).c_str());
            }
            ident(output, 3);
            fprintf(output, "break;\n");
        }
        if (sub_final[u]) {
            ident(output, 2);
            fprintf(output, "default;\n");
            ident(output, 3);
            fprintf(output,
                    "if (ret_stack.size()) { u = ret_stack.back(); ret_stack.pop_back(); goto again; }\n"
            );
            ident(output, 3);
            fprintf(output, "break;\n");
        }
        ident(output, 2);
        fprintf(output, "}\n");
        ident(output, 2);
        fprintf(output, "break;\n");
    }
    ident(output, 1);
    fprintf(output, "}\n");
    ident(output, 1);
    fprintf(output, "return v;\n");
    fprintf(output, "}\n\n");
}

static void print_adj(std::vector<std::vector<Edge>> &adj) {
    REP (i, adj.size()) {
        printf("%ld: ", i);
        for (auto it = adj[i].begin(); it != adj[i].end();) {
            long from = it->first.first;
            long to = it->first.second;
            long v = it->second;
            while (++it != adj[i].end() &&
                    to == it->first.first &&
                    it->second == v) {
                to = it->first.second;
            }
            if (from == to - 1) {
                printf(" (%ld,%ld)", from, v);
            } else {
                printf(" (%ld-%ld,%ld)", from, to - 1, v);
            }
        }
        puts("");
    }
    return;
}

bool compile_export(DefineStmt *stmt) {                                // 展开所有 & 引用（CollapseExpr） 把被引用的自动机状态合并进来
    printf("Exporting %s\n", stmt->lhs.c_str());                        //  用 ε 转移连接引用点 最终构造一个完整的、不依赖其他定义的状态机
    FsaAnno &anno = compiled[stmt];                                     //  注意这里只修改 anno 不会改变 stmt 语法树

    printf("Construct automato with all referenced CollapseExpr's DefineStmt\n");
    std::vector<std::vector<Edge>> adj;
    decltype(anno.assoc) assoc;
    std::vector<std::vector<DefineStmt*>> cllps;
    long allo = 0;
    std::unordered_map<DefineStmt*, long> stmt2offset;
    std::unordered_map<DefineStmt*, long> stmt2start;
    std::unordered_map<long, DefineStmt*> start2stmt;
    std::vector<long> starts;
    std::vector<bool> sub_final;;

    std::function<void(DefineStmt*)> allocate = [&] (DefineStmt *stmt) {
        if (stmt2offset.count(stmt))  {
            printf("stmt: %s already in stmt2offset\n", stmt->lhs.c_str());
            return;
        }
        printf("Allocate %ld to %s\n", allo, stmt->lhs.c_str());
        FsaAnno &anno = compiled[stmt];
        long base = stmt2offset[stmt] = allo;
        printf("---------------------->\n");
        print_fsa(anno.fsa);
        print_assoc(anno);
        printf("<----------------------\n");
        allo += anno.fsa.n();
        sub_final.resize(allo);
        if (used_as_call.count(stmt)) {
            stmt2start[stmt] = base + anno.fsa.start;
            start2stmt[base + anno.fsa.start] = stmt;
            starts.push_back(base + anno.fsa.start);
            for (long f : anno.fsa.finals) {
                sub_final[base + f] = true;
            }
        }
        adj.insert(adj.end(), ALL(anno.fsa.adj));
        REP (i, anno.fsa.n()) {
            for (auto &e : adj[base + i]) {
                e.second += base;
            }
        }
        assoc.insert(assoc.end(), ALL(anno.assoc));
        FOR (i, base, base + anno.fsa.n()) {
            printf("==========> begin foreach anno to find collapse/callexpr\n");
            for (auto aa : assoc[i]) {
                if (has_start(aa.second)) {
                    if (auto e = dynamic_cast<CallExpr *>(aa.first)) {
                        DefineStmt *v = e->define_stmt;
                        allocate(v);
                    } else if (auto e = dynamic_cast<CollapseExpr *>(aa.first)) {   // 这个状态有引用转移
                        printf("found collapse, allocate\n");
                        DefineStmt *v = e->define_stmt;                             // v是e这个表达式所引用的句子
                        allocate(v);
        printf("after allocate, print adj---------------------->\n");
        print_adj(adj);
                        sorted_emplace(adj[i],
                                epsilon, stmt2offset[v] + compiled[v].fsa.start);   // 指向 所引用的句子的开头
        printf("after allocate, sorted_emplaced, print adj---------------------->\n");
        print_adj(adj);
                    }
                }
            }
            long j = adj[i].size();
            while (j && collapse_label_base < adj[i][j - 1].first.second) {         // 遍历这个状态引用的边(包含start inner final类型)
                long v = adj[i][j-1].second;
                if (adj[i][j - 1].first.first < collapse_label_base) {
                    adj[i][j - 1].first.second = collapse_label_base;
                } else {
                    j--;
                }
                CollapseExpr *e;
                for (auto aa : assoc[v]) {                                          // 引用的引用
                    if (has_final(aa.second) &&
                            (e = dynamic_cast<CollapseExpr*>(aa.first))) {
                        DefineStmt *w = e->define_stmt;
                        allocate(w);
                        for (long f : compiled[w].fsa.finals) {
                            long g = stmt2offset[w] + f;
                            sorted_emplace(adj[g], epsilon, v);
                            if (g == i) {
                                j++;
                            }
                        }
                    }
                }
            }
            adj[i].resize(j);                                                       // 删除老的引用转移边
            printf("<========== begin foreach End anno to find collapse/callexpr\n");
        }
    };
    allocate(stmt);
    anno.fsa.adj = std::move(adj);
    anno.assoc = std::move(assoc);
    anno.deterministic = false;
    printf(" of states: %ld\n", anno.fsa.n());

    printf("last allocate done---------------------->\n");
    print_fsa(anno.fsa);
    print_assoc(anno);
    printf("last allocate done<----------------------\n");

    if (0 && !stmt->intact) {
        printf("Constructing substring grammar\n");
        anno.substring_grammar();
        printf(" of states: %ld\n", anno.fsa.n());
    }

    printf("Determinize\n");
    std::vector<std::vector<long>> map0;
    anno.determinize(&starts, &map0);
    std::vector<bool> sub_final2(anno.fsa.n());
    REP (i, anno.fsa.n()) {
        for (long u : map0[i]) {
            if (sub_final[u]) {
                sub_final2[i] = true;
            }
            if (start2stmt.count(u)) {
                DefineStmt *stmt = start2stmt[u];
                if (stmt2start[stmt] < 0) {
                    stmt->module->locfile.locate(stmt->loc,
                            "the start has been included in multiple DFA states");
                    return false;
                }
                stmt2start[stmt] = ~i;
            }
        }
    }
    sub_final = std::move(sub_final2);
    start2stmt.clear();
    for (auto &it : stmt2start) {
        it.second = ~ it.second;
        start2stmt[it.second] = it.first;
    }
    printf(" of states: %ld", anno.fsa.n());

    printf("Minimize\n");
    map0.clear();
    anno.minimize(&map0);
    sub_final2.assign(anno.fsa.n(), false);
    REP (i, anno.fsa.n()) {
        for (long u : map0[i]) {
            if (sub_final[u]) {
                sub_final2[i] = true;
            }
            if (start2stmt.count(u)) {
                DefineStmt * stmt = start2stmt[u];
                stmt2start[stmt] = i;
            }
        }
    }
    sub_final = std::move(sub_final2);
    start2stmt.clear();
    for (auto & it : stmt2start) {
        start2stmt[it.second] = it.first;
    }
    printf(" of states: %ld", anno.fsa.n());
     
    if (0) {
        printf("Keep accessible states\n");
        starts.clear();
        for (auto &it : stmt2start) {
            starts.push_back(it.second);
        }
        std::vector<long> map1;
        anno.accessible(&starts, map1);
        sub_final2.assign(anno.fsa.n(), false);
        REP (i, anno.fsa.n()) {
            long u = map1[i];
            sub_final2[i] = sub_final[u];
            if (start2stmt.count(u)) {
                stmt2start[start2stmt[u]] = i;
            }
        }
        sub_final = std::move(sub_final2);
        start2stmt.clear();
        for (auto &it : stmt2start) {
            start2stmt[it.second] = it.first;
        }
        printf(" of states: %ld", anno.fsa.n());
    
        printf("Keep co-accessible states\n");
        map1.clear();
        anno.co_accessible(&sub_final, map1);
        sub_final2.assign(anno.fsa.n(), false);
        REP (i, anno.fsa.n()) {
            long u = map1[i];
            sub_final2[i] = sub_final[u];
            if (start2stmt.count(u)) {
                stmt2start[start2stmt[u]] = i;
            }
        }
        sub_final = std::move(sub_final2);
        start2stmt.clear();
        for (auto &it : stmt2start) {
            start2stmt[it.second] = it.first;
        }
        printf(" of states: %ld", anno.fsa.n());
    }

    stmt2final[stmt] = sub_final;
    auto &call_addr = stmt2call_addr[stmt];
    call_addr.assign(anno.fsa.n(), std::make_pair(-1L, -1L));
    printf("CallExpr");
    REP (i, anno.fsa.n()) {
        if (anno.fsa.has_call(i)) {
            if (anno.fsa.adj[i].size() != 1 ||
                    anno.fsa.adj[i][0].first.second - anno.fsa.adj[i][0].first.first > 1) {
                stmt->module->locfile.locate(stmt->loc,
                        "state %ld: CallExpr cannot coexist with other transitions", i);
                for (auto it = anno.fsa.adj[i].begin(); it != anno.fsa.adj[i].end();) {
                    long from = it->first.first;
                    long to = it->first.second;
                    long v = it->second;
                    while (++it != anno.fsa.adj[i].end() &&
                            to == it->first.first &&
                            it->second == v) {
                        to = it->first.second;
                    }
                    printf("    (%ld,%ld)\n", from, to - 1);
                }
                return false;
            }
            for (auto aa : anno.assoc[i]) {
                if (has_start(aa.second)) {
                    if (auto *e = dynamic_cast<CallExpr*>(aa.first)) {
                        call_addr[i] = {
                            stmt2start[e->define_stmt],
                            anno.fsa.adj[i][0].second
                        };
                    }
                }
            }
        }
    }

    printf("Removing action/CallExpr labels");
    REP (i, anno.fsa.n()) {
        long j = anno.fsa.adj[i].size();
        while (j && action_label_base < anno.fsa.adj[i][j - 1].first.second) {
            if (anno.fsa.adj[i][j - 1].first.first < action_label_base) {
                anno.fsa.adj[i][j - 1].first.second = action_label_base;
            } else {
                j--;
            }
        }
        anno.fsa.adj[i].resize(j);
    }

    //if (1) {
    //    print_fsa(anno.fsa);
    //    print_assoc(anno);
    //}
    return true;
}

static void generate_final(const char *name, const std::vector<bool> &final) {
    fprintf(output,
            "   //static const long %sfinals[] = {",
        name
    );
    bool first = true;
    REP (i, final.size()) {
        if (final[i]) {
            if (first) {
                first = false;
            } else {
                fprintf(output, ",");
            }
            fprintf(output, "%ld", i);
        }
    }
    fprintf(output, "};\n");

    first = true;
    fprintf(output, "   static const unsigned long %sfinal[] = {", name);
    for (long j = 0, i = 0; i < final.size(); i += CHAR_BIT * sizeof(long)) {
        ulong mask = 0;
        for (; j < final.size() && j < i + CHAR_BIT * sizeof(long); j++) {
            if (final[j]) {
                mask |= 1uL << (j - i);
            }
        }
        if (i) {
            fprintf(output, ",");
        }
        fprintf(output, "%#lx", mask);
    }
    fprintf(output, "};\n");
}

void generate_cxx_export(DefineStmt *stmt) {
    FsaAnno &anno = compiled[stmt];

    fprintf(output,
            "long hh_%s_start = %ld;\n\n\n",
        stmt->lhs.c_str(), anno.fsa.start);
    fprintf(output,
            "bool hh_%s_is_final(const std::vector<long> &ret_stack, long u)\n",
        stmt->lhs.c_str()
    );
    fprintf(output, "{\n");
    std::vector<bool> final(anno.fsa.n());
    for (long f : anno.fsa.finals)  {
        final[f] = true;
    }
    generate_final("", final);
    generate_final("sub_", stmt2final[stmt]);
    fprintf(output,
            "   for (auto i = ret_stack.size(); i; u = ret_stack[--i])\n"
            "      if (!(0 <= u && u < %ld && sub_final[u/(CHAR_BIT * sizeof(long))] >> (u%%(CHAR_BIT * sizeof(long))) & 1))\n"
            "          return false;\n"
            "      return 0 <= u && u < %ld && final[u/(CHAR_BIT*sizeof(long))] >> (u%%(CHAR_BIT*sizeof(long))) & 1;\n"
            "   };\n\n",
        anno.fsa.n(),
        anno.fsa.n()
    );
    generate_transitions(stmt);
}

void generate_graphviz(Module *mod) {
    fprintf(output, "Generate by hh, %s\n", mod->filename.c_str());
    for (Stmt *x = mod->toplevel; x; x = x->next) {
        if (auto stmt = dynamic_cast<DefineStmt*>(x)) {
            if (stmt->export_) {
                FsaAnno &anno = compiled[stmt];
                fprintf(output, "digraph \"%s\" {\n", mod->filename.c_str());
                bool start_is_final = false;
                ident(output, 1);
                fprintf(output, "node[shape=doublecircle,color=olivedrab1,style=filled,fontname=Monospace];");
                for (long f : anno.fsa.finals) {
                    if (f == anno.fsa.start) {
                        start_is_final = true;
                    } else {
                        fprintf(output, " %ld", f);
                    }
                }
                fprintf(output, "\n");

                ident(output, 1);
                if (start_is_final) {
                    fprintf(output, "node[shape=doublecircle,color=orchid];");
                } else {
                    fprintf(output, "node[shape=circle,color=orchid];");
                }
                fprintf(output, " %ld\n", anno.fsa.start);

                ident(output, 1);
                fprintf(output, "node[shape=circle,color=black,stype=\"\"]\n");

                REP (u, anno.fsa.n()) {
                    std::unordered_map<long, std::stringstream> labels;
                    bool first = true;
                    auto it = anno.fsa.adj[u].begin();
                    for (; it != anno.fsa.adj[u].end(); ++it) {
                        std::stringstream &lb = labels[it->second];
                        if (!lb.str().empty()) {
                            lb << ",";
                        }
                        if (it->first.first == it->first.second - 1) {
                            lb << it->first.first;
                        } else {
                            lb << it->first.first << "-" << it->first.second - 1;
                        }
                    }
                    for (auto &lb : labels) {
                        ident(output, 1);
                        fprintf(output, "%ld -> %ld[label=\"%s\"]\n",
                                u, lb.first, lb.second.str().c_str());
                    }
                }
            }
        }
    }
    fprintf(output, "}\n");
}

void generate_cxx(Module *mod) {
    fprintf(output, "// Generate by hh, %s\n", mod->filename.c_str());
    fprintf(output, "#include <limits.h>\n");
    fprintf(output, "#include <vector>\n");
    fputs(
        "#include <algorithm>\n"
        "#include <cinttypes>\n"
        "#include <clocale>\n"
        "#include <codecvt>\n"
        "#include <cstdint>\n"
        "#include <cstdio>\n"
        "#include <cstring>\n"
        "#include <cwctype>\n"
        "#include <iostream>\n"
        "#include <locale>\n"
        "#include <string>\n"
        "#include <cstdio>\n",
        output
    );
    fprintf(output, "\n");
    DefineStmt *main_export = NULL;
    for (Stmt *x = mod->toplevel; x; x = x->next) {
        if (auto xx = dynamic_cast<DefineStmt*>(x)) {
            if (xx->export_) {
                if (!main_export) {
                    main_export = xx;
                }
                generate_cxx_export(xx);
            } else if (auto xx = dynamic_cast<CppStmt*>(x)) {
                fprintf(output, "%s", xx->code.c_str());
            }
        }
    }

    if (main_export) {
        fprintf(output,
                "\n"
                "int main(int argc, char **argv) {\n"
                "   setlocale(LC_ALL, \"\");\n"
                "   std::string utf8;\n"
                "   const char *p;\n"
                "   long c, u = hh_%s_start, pref = 0;\n",
            main_export->lhs.c_str()
        );
        fprintf(output, "   std::vector<long> ret_stack;\n");
        fprintf(output,
                "   if (argc == 2) utf8 = argv[1];\n"
                "   else {\n"
                "       FILE *f = argc == 1 ? stdin : fopen(argv[1]), \"r\");\n"
                "       while ((c = fgetc(f)) != EOF)\n"
                "           utf8 += c;\n"
                "       fclose(f);\n"
                "   }\n"
                "   std::u32string utf32 = wstring_convert<codecvt_utf8<char32_t>, char32_t>{}.from_bytes(utf8);\n"
        );

        fprintf(output,
                "hh_%s_is_final(ret_stack, u)",
            main_export->lhs.c_str()
        );

        fprintf(output,
                "   for (char32_t c : utf32) {\n"
                "       u = hh_%s_transit(ret_stack, u, c);\n",
            main_export->lhs.c_str()
        );

        fprintf(output,
                "       if (c > WCHAR_MAX || iswcntrl(c)) printf(\"%%\" PRIuLEAST32 \" \", c);\n"
                "       else std::cout << wstring_convert<codecvt_utf8<char32_t>, char32_t>{}.to_bytes(c) << ' ';\n"
                "       hh_%s_is_final(ret_stack, u);\n",
            main_export->lhs.c_str()
        );
        fprintf(output,
                "   if (u < 0) break;\n"
                "   pref++;\n"
                "   }\n"
                "   printf(\"\\nlen: %%zd\\npref: %%ld\\nstate: %%ld\\nfinal: %%s\\n\", utf32.size(), pref, u, hh_%s_is_final(ret_stack, u) ? \"true\" : \"false\");\n"
                "}\n",
            main_export->lhs.c_str()
        );
    }
}

