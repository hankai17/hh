#include "compiler.hh"
#include "common.hh"
#include "loader.hh"

#include <map>
#include <stack>
#include <iostream>
#include <functional>
#include <unordered_map>
#include <algorithm>

#define DEBUG_COMP 1

static std::map<DefineStmt *, FsaAnno> compiled;

static void print_assoc(const FsaAnno &anno) {
    printf("====== Associated Expr of each state\n");
    REP (i, anno.fsa.n()) {
        printf("%ld: ", i);
        for (auto aa : anno.assoc[i]) {
            auto a = aa.first;
            auto tag = aa.second;
            const char *name = typeid(*a).name();
            while (name && isdigit(name[0])) {
                name++;
            }
            std::string t = name;
            t = t.substr(t.size() - 4);
            std::string st_name;
            if (a->stmt) {
                st_name = a->stmt->lhs;
            }
            printf(" %s %s%d(%ld-%ld", st_name.c_str(),
                    t.c_str(),
                    (int)tag,
                    a->loc.start,
                    a->loc.end);
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

static void print_fsa(const Fsa &fsa) {
    printf("====== Automato\n");
    printf("start: %ld\n", fsa.start);
    printf("finals:");
    for (long i : fsa.finals) {
        printf(" %ld", i);
    }
    puts("");
    puts("edges:");
    REP (i, fsa.n()) {
        printf("%ld: ", i);
        /*
        for (auto &x : fsa.adj[i]) {
            printf(" (%ld, %ld)", x.first, x.second);
        }
        */
        for (auto it = fsa.adj[i].begin(); it != fsa.adj[i].end();) {
            long from = it->first;
            long to = from;
            long v = it->second;
            while (++it != fsa.adj[i].end() &&
                    it->first == to + 1 &&
                    it->second == v) {
                to++;
            }
            if (from == to) {
                printf(" (%ld,%ld)", from, v);
            } else {
                printf(" (%ld-%ld,%ld)", from, to, v);
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
        printf("expr:(%ld-%ld), depth:%ld, stmt:%s\n",
                expr.loc.start,
                expr.loc.end,
                expr.depth,
                expr.stmt->lhs.c_str());
#endif
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
        st.top().star(&expr);
    }

    void visit(CollapseExpr &expr) override {
#ifdef DEBUG_COMP
        std::cout << "Compiler visit CollapseExpr" << std::endl;
#endif
        //st.push(FsaAnno::collapse(expr));
        auto anno = FsaAnno::collapse(expr);
        st.push(anno);
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
        FsaAnno anno = compiled[expr.define_stmt];
        anno.add_assoc(expr);
        st.push(anno);
    }

    void visit(EpsilonExpr &expr) override {
#ifdef DEBUG_COMP
        std::cout << "Compiler visit EpsilonExpr" << std::endl;
#endif
        st.push(FsaAnno::epsilon(&expr));
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

    void visit(UnicodeRangeExpr &expr) override {
#ifdef DEBUG_COMP
        std::cout << "Compiler visit UnicodeRangeExpr" << std::endl;
#endif
        st.push(FsaAnno::unicode_range(expr));
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
    anno.determinize();
    anno.minimize();
}

void compile_actions(DefineStmt *stmt) {
    FsaAnno &anno = compiled[stmt];
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
            return t->define_module->defined_action[t->ident];
        }
        return std::string();
    };
#define D(S)                                                            \
    if (auto t = dynamic_cast<InlineAction*>(action)) {                 \
        printf(" %ld, %ld, %ld, %s\n", u, e.first, v, t->code.c_str()); \
    } else if (auto t = dynamic_cast<RefAction*>(action)) {             \
        printf(" %ld, %ld, %ld, %s\n", u, e.first, v,                   \
            t->define_module->defined_action[t->ident].c_str());        \
    }
    fprintf(output, "long hh_%s_transit(long u, long c)\n", stmt->lhs.c_str());
    fprintf(output, "{\n");
    ident(output, 1);
    fprintf(output, "long v = -1;\n");
    ident(output, 1);
    fprintf(output, "switch (u) {\n");
    REP (u, anno.fsa.n()) {
        if (anno.fsa.adj[u].empty()) {
            continue;
        }
        ident(output, 1);
        fprintf(output, "case %ld:\n", u);
        ident(output, 2);
        fprintf(output, "switch (c) {\n");
        for (auto it = anno.fsa.adj[u].begin(); it != anno.fsa.adj[u].end();) {
            long from = it->first;
            long to = from;
            long v = it->second;
            while (++it != anno.fsa.adj[u].end() &&
                    it->first == to + 1 &&
                    it->second == v) {
                to++;
            }
            ident(output, 2);
            if (from == to) {
                fprintf(output, "case %ld:\n", from);
            } else {
                fprintf(output, "case %ld ... %ld:\n", from, to);
            }
            ident(output, 3);
            fprintf(output, "v = %ld;\n", v);
            auto ie = withins[u].end();
            auto je = withins[v].end();
            for (auto i = withins[u].begin(), j = withins[v].begin(); i != ie; ++i) {
                while (j != je && i->first > j->first) {
                    ++j;
                }
                if (j == je || i->first != j->first) {
                    for (auto action : i->first->leaving) {
                        ident(output, 3);
                        fprintf(output, "{%s}\n", get_code(action).c_str());
                    }
                }
            }
            for (auto i = withins[u].begin(), j = withins[v].begin(); j != je; ++j) {
                while (i != ie && i->first < j->first) {
                    ++i;
                }
                if (i == ie || i->first != j->first) {
                    for (auto action : j->first->entering) {
                        ident(output, 3);
                        fprintf(output, "{%s}\n", get_code(action).c_str());
                    }
                }
            }
            for (auto j = withins[v].begin(); j != je; ++j) {
                for (auto action : j->first->transiting) {
                    ident(output, 3);
                    fprintf(output, "{%s}\n", get_code(action).c_str());
                }
            }
            for (auto j = withins[v].begin(); j != je; ++j) {
                if (long(j->second) & long(ExprTag::final)) {
                    for (auto action : j->first->finishing) {
                        ident(output, 3);
                        fprintf(output, "{%s}\n", get_code(action).c_str());
                    }
                }
            }
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
    fprintf(output, "}\n");
}

void compile_export(DefineStmt *stmt) {                                // 展开所有 & 引用（CollapseExpr） 把被引用的自动机状态合并进来
    printf("Exporting %s\n", stmt->lhs.c_str());                        //  用 ε 转移连接引用点 最终构造一个完整的、不依赖其他定义的状态机
    FsaAnno &anno = compiled[stmt];                                     //  注意这里只修改 anno 不会改变 stmt 语法树

    printf("Construct automato with all referenced CollapseExpr's DefineStmt\n");
    std::vector<std::vector<std::pair<long, long>>> adj;
    decltype(anno.assoc) assoc;
    std::vector<std::vector<DefineStmt*>> cllps;
    long allo = 0;
    std::unordered_map<DefineStmt*, long> stmt2offset;
    std::function<void(DefineStmt*)> allocate_collapse = [&] (DefineStmt *stmt) {
        if (stmt2offset.count(stmt))  {
            return;
        }
        printf("Allocate %ld to %s\n", allo, stmt->lhs.c_str());
        FsaAnno &anno = compiled[stmt];

        printf("---------------------->\n");
        print_fsa(anno.fsa);
        print_assoc(anno);
        printf("<----------------------\n");

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
            if (anno.fsa.has(i - old, 256)) {                               // 这个状态有引用转移
                for (auto aa : assoc[i]) {
                    if (auto e = dynamic_cast<CollapseExpr *>(aa.first)) {  // 找到 引用转移的边
                        DefineStmt *v = e->define_stmt;                     // 找到 最原始处的定义
                        allocate_collapse(v);
                        sorted_insert(adj[i],
                                std::make_pair(-1L, stmt2offset[v] + compiled[v].fsa.start));
                    }
                }
                long j = adj[i].size();
                while (j && adj[i][j - 1].first == 256) {
                    long v = adj[i][--j].second;
                    for (auto aa : assoc[v]) {
                        if (auto e = dynamic_cast<CollapseExpr*>(aa.first)) {
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
    anno.deterministic = false;

    printf("last---------------------->\n");
    print_fsa(anno.fsa);
    print_assoc(anno);
    printf("last<----------------------\n");

    if (1 && !stmt->intact) {
        printf("Constructing substring grammar\n");
        anno.substring_grammar();
    }

    printf("Determinize minimize, remove dead states\n");
    anno.determinize();
    anno.minimize();

    anno.accessible();
    anno.co_accessible();

    /*
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
    */

    if (1) {
        print_fsa(anno.fsa);
        print_assoc(anno);
    }
}

void generate_cxx_export(DefineStmt *stmt) {
    compile_export(stmt);
    FsaAnno &anno = compiled[stmt];

    fprintf(output, "void hh_%s_init(long &start, std::vector<long> &finals)\n", stmt->lhs.c_str());
    fprintf(output, "{\n");
    ident(output, 1);
    fprintf(output, "start = %ld;\n", anno.fsa.start);
    ident(output, 1);
    fprintf(output, "finals = {");
    bool first = true;
    for (long f : anno.fsa.finals)  {
        if (first) {
            first = false;
        } else {
            fprintf(output, ",");
        }
        fprintf(output, "%ld", f);
    }
    fprintf(output, "};\n");
    fprintf(output, "};\n\n");

    printf("Compiling actions\n");
    compile_actions(stmt);
}

void generate_graphviz(Module *mod) {
    fprintf(output, "Generate by hh, %s\n", mod->filename.c_str());
    for (Stmt *x = mod->toplevel; x; x = x->next) {
        if (auto stmt = dynamic_cast<DefineStmt*>(x)) {
            if (stmt->export_) {
                compile_export(stmt);
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
                    auto it2 = it;
                    auto ite = anno.fsa.adj[u].end();
                    for (; it != ite; it = it2) {
                        long v = it->first;
                        while (++it2 != ite && it->second == it2->second) {
                            v = it2->first;
                        }
                        std::stringstream &lb = labels[it->second];
                        if (!lb.str().empty()) {
                            lb << ",";
                        }
                        if (it->first == v) {
                            lb << v;
                        } else {
                            lb << it->first << "-" << v;
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
    fprintf(output, "#include <vector>\n");
    fputs(
        "#include <algorithm>\n"
        "#include <cstdio>\n",
        output
    );
    fprintf(output, "\n");
    for (Stmt *x = mod->toplevel; x; x = x->next) {
        if (auto xx = dynamic_cast<DefineStmt*>(x)) {
            if (xx->export_) {
                generate_cxx_export(xx);
            } else if (auto xx = dynamic_cast<CppStmt*>(x)) {
                fprintf(output, "%s", xx->code.c_str());
                fputs(
                    "\n"
                    "int main(int argc, char **argv)\n"
                    "{\n"
                    "   long u = 0;\n"
                    "   long len = 0;\n"
                    "   std::vector<long> finals;\n"
                    "   hh_main_init(u, finals);\n"
                    "   if (argc > 1) {\n"
                    "       for (char *c = argv[1]; *c; c++) {\n"
                    "           u = hh_main_transit(u, *(unsigned char*)c);\n"
                    "           if (u < 0) {\n"
                    "               break;\n"
                    "           }\n"
                    "           len++;\n"
                    "       }\n"
                    "   } else {\n"
                    "       int c;\n"
                    "       while (u >= 0 && (c = getchar()) != EOF) {\n"
                    "           u = hh_main_transit(u, c;)\n"
                    "           if (u < 0) {\n"
                    "               break;\n"
                    "           }\n"
                    "           len++;\n"
                    "       }\n"
                    "   }\n"
                    "   printf(\"len: %ld\\nfinal: %s\\n\", len, u, binary_search(finals.begin(), finals.end(), u) ? \"true\", : \"false\");\n"
                    , output
                );
            }
        }
    }
}

