#include "compiler.hh"
#include "common.hh"
#include "loader.hh"

#include <map>
#include <stack>
#include <iostream>
#include <functional>
#include <unordered_map>
#include <algorithm>

//#define DEBUG_COMP 1

static std::map<DefineStmt *, FsaAnno> compiled;

static void print_assoc(const FsaAnno &anno) {
    REP (i, anno.fsa.n()) {
        printf("%ld: ", i);
        for (auto a : anno.assoc[i]) {
            const char *name = typeid(*a).name();
            while (name && isdigit(name[0])) {
                name++;
            }
            std::string t = name;
            t = t.substr(t.size() - 4);
            printf(" %s(%ld-%ld)", t.c_str(),
                    a->loc.start, a->loc.end);
        }
    }
    puts("");
}

static void print_fsa(const Fsa &fsa) {
    printf("\nstart: %ld\n", fsa.start);
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
    for (long k = 63 - __builtin_clzl(v->depth); k >= 0; k--) {
        if (u->anc[k] != v->anc[k]) {
            u = u->anc[k];
            v = v->anc[k];
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
    }

    void post_expr(Expr &expr) {
        path.pop();
        expr.post = tick;
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

    void visit(ClosureExpr &expr) override {
#ifdef DEBUG_COMP
        std::cout << "Compiler visit ClosureExpr" << std::endl;
#endif
        visit(*expr.inner);
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
        visit(*expr.rhs);
        FsaAnno rhs = std::move(st.top());
        visit(*expr.lhs);
        path.pop();
        st.top().concat(rhs);
    }

    void visit(DifferenceExpr &expr) override {
#ifdef DEBUG_COMP
        std::cout << "Compiler visit DifferenceExpr" << std::endl;
#endif
        visit(*expr.rhs);
        FsaAnno rhs = std::move(st.top());
        visit(*expr.lhs);
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
        visit(*expr.rhs);
        FsaAnno rhs = std::move(st.top());
        visit(*expr.lhs);
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
        visit(*expr.inner);
        st.top().question(expr);
    }

    void visit(PlusExpr &expr) override {
#ifdef DEBUG_COMP
        std::cout << "Compiler visit PlusExpr" << std::endl;
#endif
        visit(*expr.inner);
        st.top().plus();
    }

    void visit(UnionExpr &expr) override {
#ifdef DEBUG_COMP
        std::cout << "Compiler visit UnionExpr" << std::endl;
#endif
        visit(*expr.rhs);
        FsaAnno rhs = std::move(st.top());
        visit(*expr.lhs);
        st.top().union_(rhs, expr);
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
}

void compile_actions(FsaAnno &anno) {
    REP (i, anno.fsa.n()) {
        std::sort(ALL(anno.assoc[i]), [] (const Expr *x, const Expr *y) {
            return x->pre < y->pre;
        });
    }
    REP (u, anno.fsa.n()) {
        for (auto &e : anno.fsa.adj[u]) {
            long v = e.second;
            if (anno.fsa.is_final(v)) {
                Expr *last = NULL;
                for (auto a : anno.assoc[v]) {
                    Expr *stop = last ? find_lca(last, a) : NULL;
                    last = a;
                    for (Expr *x = a; a != stop; a = a->anc[0]) {
                        for (auto action : x->finishing) {
                            if (auto t = dynamic_cast<InlineAction*>(action)) {
                                printf("%ld %ld %ld %s\n",
                                        u, e.first, v,
                                        t->code.c_str());
                            } else if (auto t = dynamic_cast<RefAction*>(action)) {
                                printf("%ld %ld %ld %s\n",
                                        u, e.first, v,
                                        t->define_module->defined_action[t->ident].c_str());
                            }
                        }
                    }
                }
            }
        }
    }
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
            anno.assoc[allo] = std::move(anno.assoc[x]);
        }
        allo++;
    };
    anno.fsa.remove_dead(relate);
    if (anno.fsa.finals.empty()) {
        anno.assoc.assign(1, {});
    } else {
        anno.assoc.resize(allo);
    }

    print_fsa(anno.fsa);
    print_assoc(anno);

    compile_actions(anno);
}


