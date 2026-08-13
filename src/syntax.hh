#pragma once

#include "location.hh"
#include <bitset>
#include <vector>
#include <string>
#include <sstream>
#include <memory>
#include <typeinfo>
#include <cxxabi.h>

//#define DEBUG_CLS 1

template <class T>
struct Visitor;

template <class T>
struct VisitableBase {
    virtual void accept(Visitor<T> &visitor) = 0;
};

template <class Base, class Derived>
struct Visitable : Base {
    void accept(Visitor<Base> &visitor) override {
        visitor.visit(static_cast<Derived &>(*this));
    }
};

// Action
struct Action;
struct InlineAction;
struct RefAction;

template <>
struct Visitor<Action> {
    virtual void visit(Action &) = 0;
    virtual void visit(InlineAction &) = 0;
    virtual void visit(RefAction &) = 0;
};

// Expr
struct Expr;
struct BracketExpr;
struct CallExpr;
struct ClosureExpr;
struct CollapseExpr;
struct ComplementExpr;
struct ConcatExpr;
struct DifferenceExpr;
struct DotExpr;
struct EmbedExpr;
struct EpsilonExpr;
struct IntersectExpr;
struct LiteralExpr;
struct MaybeExpr;
struct PlusExpr;
struct RepeatExpr;
struct UnionExpr;

template <>
struct Visitor<Expr> {                              // 1 visitor虚基类定义expr各接口
    virtual void visit(Expr &) = 0;
    virtual void visit(BracketExpr &) = 0;
    virtual void visit(CallExpr &) = 0;
    virtual void visit(ClosureExpr &) = 0;
    virtual void visit(CollapseExpr &) = 0;
    virtual void visit(ComplementExpr &) = 0;
    virtual void visit(ConcatExpr &) = 0;
    virtual void visit(DifferenceExpr &) = 0;
    virtual void visit(DotExpr &) = 0;
    virtual void visit(EmbedExpr &) = 0;
    virtual void visit(EpsilonExpr &) = 0;
    virtual void visit(IntersectExpr &) = 0;
    virtual void visit(LiteralExpr &) = 0;
    virtual void visit(MaybeExpr &) = 0;
    virtual void visit(PlusExpr &) = 0;
    virtual void visit(RepeatExpr &) = 0;
    virtual void visit(UnionExpr &) = 0;
};

// Stmt
struct Stmt;
struct ActionStmt;
struct CppStmt;
struct DefineStmt;          // 赋值语句
struct EmptyStmt;           // 空语句
struct ImportStmt;
struct PreprocessDefineStmt;

template <>
struct Visitor<Stmt> {                              // 同上 visitor虚基类定义stmt各接口
    virtual void visit(Stmt &) = 0;
    virtual void visit(ActionStmt &) = 0;
    virtual void visit(CppStmt &) = 0;
    virtual void visit(DefineStmt &) = 0;
    virtual void visit(EmptyStmt &) = 0;
    virtual void visit(ImportStmt &) = 0;
    virtual void visit(PreprocessDefineStmt &) = 0;
};

///////////////////////////////////////////////////// CRTP

// Action
struct Action: VisitableBase<Action> {
    Location loc;
    virtual ~Action() = default;
};

struct InlineAction: Visitable<Action, InlineAction> {
    std::string code;
    InlineAction(std::string &code) : code(std::move(code)) {
#ifdef DEBUG_CLS 
        printf("new InlineAction\n");
#endif
    }
    ~InlineAction() {
    }
};

struct Module;
struct RefAction : Visitable<Action, RefAction> {
    std::string qualified;
    std::string ident;
    ActionStmt *define_stmt;
    RefAction(std::string &qualified, std::string &ident) :
        qualified(std::move(qualified)),
        ident(std::move(ident)) {
#ifdef DEBUG_CLS 
        printf("new RefAction\n");
#endif
    }
    ~RefAction() {
    }
};

// Expr
struct Expr : VisitableBase<Expr> {                 // 2 Expr 白嫖accept接口 (CRTP典型做法继承自身)
    Location loc;
    long depth;
    long pre;
    long post;
    std::vector<Expr *> anc;
    std::vector<std::pair<Action *, long>> entering;
    std::vector<std::pair<Action *, long>> finishing;
    std::vector<std::pair<Action *, long>> leaving;
    std::vector<std::pair<Action *, long>> transiting;
    DefineStmt *stmt = NULL;
    virtual ~Expr() {
        for (auto a : entering) {
            delete a.first;
        }
        for (auto a : finishing) {
            delete a.first;
        }
        for (auto a : leaving) {
            delete a.first;
        }
        for (auto a : transiting) {
            delete a.first;
        }
    }
    bool no_action() const {
        return entering.empty() &&
                finishing.empty() &&
                leaving.empty() &&
                transiting.empty();
    }
    std::string name() const {
        int status;
        std::unique_ptr<char, void(*)(void*)> r {
            abi::__cxa_demangle(typeid(*this).name(), NULL, NULL, &status),
            free
        };
        std::string t = r.get();
        t = t.substr(0, t.size() - 4);
        return t;
    }
    std::string dump_info() {
        std::ostringstream oss;
        oss << "depth: " << depth
            << ", pre: " << pre
            << ", post: " << post
            << ", stmt_ptr:" << (void*)stmt;
        return oss.str();
    }
};

struct BracketExpr : Visitable<Expr, BracketExpr> { // 2.1 CRTP 但还没有实现expr中 该类的visit接口
    DisjointIntervals intervals;
    BracketExpr(DisjointIntervals *intervals) :
        intervals(std::move(*intervals)) {
        delete intervals;
#ifdef DEBUG_CLS 
        printf("new BracketExpr\n");
#endif
    }
};

struct CallExpr : Visitable<Expr, CallExpr> {
    std::string qualified;
    std::string ident;
    DefineStmt *define_stmt = NULL;

    CallExpr(std::string &qualified, std::string ident) :
        qualified(std::move(qualified)),
        ident(std::move(ident)) {
#ifdef DEBUG_CLS 
        printf("new CallExpr\n");
#endif
    }

};

struct CollapseExpr : Visitable<Expr, CollapseExpr> {
    std::string qualified;
    std::string ident;
    DefineStmt *define_stmt = NULL;

    CollapseExpr(std::string &qualified, std::string &ident) :
        qualified(std::move(qualified)),
        ident(std::move(ident)) {
#ifdef DEBUG_CLS 
        printf("new CollapseExpr: qualified: %s, ident: %s\n",
                qualified, ident);
#endif
    }

    ~CollapseExpr() {
    }
};

struct ClosureExpr : Visitable<Expr, ClosureExpr> {
    Expr *inner;

    ClosureExpr(Expr *inner) :
        inner(inner) {
#ifdef DEBUG_CLS 
        printf("new ClosureExpr\n");
#endif
    }

    ~ClosureExpr() { delete inner; }
};

struct ComplementExpr : Visitable<Expr, ComplementExpr> {
    Expr *inner;

    ComplementExpr(Expr *inner) :
        inner(inner) {
#ifdef DEBUG_CLS 
        printf("new ComplementExpr\n");
#endif
    }

    ~ComplementExpr() { delete inner; }
};

struct ConcatExpr : Visitable<Expr, ConcatExpr> {
    Expr *lhs, *rhs;

    ConcatExpr(Expr *lhs, Expr *rhs) :
        lhs(lhs), rhs(rhs) {
#ifdef DEBUG_CLS 
        printf("new ConcatExpr\n");
#endif
    }

    ~ConcatExpr() { delete lhs; delete rhs; }
};

struct DifferenceExpr : Visitable<Expr, DifferenceExpr> {
    Expr *lhs, *rhs;

    DifferenceExpr(Expr *lhs, Expr *rhs) :
        lhs(lhs), rhs(rhs) {
#ifdef DEBUG_CLS 
        printf("new DifferenceExpr\n");
#endif
    }

    ~DifferenceExpr() { delete lhs; delete rhs; }

};

struct DotExpr : Visitable<Expr, DotExpr> {
    // TODO
};

struct EmbedExpr : Visitable<Expr, EmbedExpr> {
    std::string qualified;
    std::string ident;
    DefineStmt *define_stmt = NULL;
    long macro_value;
    EmbedExpr(std::string &qualified, std::string &ident) :
        qualified(std::move(qualified)),
        ident(std::move(ident)) {
#ifdef DEBUG_CLS 
        printf("new EmbedExpr: qualified: %s, ident: %s\n",
                qualified, ident);
#endif
    }

    ~EmbedExpr() {
    }
};

struct EpsilonExpr : Visitable<Expr, EpsilonExpr> {
};

struct IntersectExpr : Visitable<Expr, IntersectExpr> {
    Expr *lhs;
    Expr *rhs;
    IntersectExpr(Expr *lhs, Expr *rhs) :
        lhs(lhs), rhs(rhs) {
#ifdef DEBUG_CLS 
        printf("new IntersectExpr\n");
#endif
    }
    ~IntersectExpr() {
        delete lhs;
        delete rhs;
    }
};

struct LiteralExpr : Visitable<Expr, LiteralExpr> {
    std::string literal;

    LiteralExpr(std::string &literal) :
        literal(std::move(literal)) {
#ifdef DEBUG_CLS 
        printf("new LiteralExpr: literal: %s\n",
                literal);
#endif
    }

    ~LiteralExpr() {
    }
};

struct MaybeExpr : Visitable<Expr, MaybeExpr> {
    Expr *inner;

    MaybeExpr(Expr *inner) :
        inner(inner) {
#ifdef DEBUG_CLS 
        printf("new MaybeExpr\n");
#endif
    }
    ~MaybeExpr() {
        delete inner;
    }
};

struct PlusExpr : Visitable<Expr, PlusExpr> {
    Expr *inner;

    PlusExpr(Expr *inner) :
        inner(inner) {
#ifdef DEBUG_CLS 
        printf("new PlusExpr\n");
#endif
    }
    ~PlusExpr() { delete inner; }
};

struct RepeatExpr : Visitable<Expr, RepeatExpr> {
    Expr *inner;
    long low;
    long high;

    RepeatExpr(Expr *inner, long low, long high) :
        inner(inner),
        low(low),
        high(high) {
#ifdef DEBUG_CLS 
        printf("new RepeatExpr\n");
#endif
    }
    ~RepeatExpr() { delete inner; }
};

struct UnionExpr : Visitable<Expr, UnionExpr> {
    Expr *lhs, *rhs;

    UnionExpr(Expr *lhs, Expr *rhs) :
        lhs(lhs), rhs(rhs) {
#ifdef DEBUG_CLS 
        printf("new UnionExpr\n");
#endif
    }
    ~UnionExpr() { delete lhs; delete rhs; }
};

// Stmt
struct Stmt {
    Location loc;
    Stmt *prev = NULL;
    Stmt *next = NULL;
    virtual ~Stmt() = default;
    virtual void accept(Visitor<Stmt> &visitor) = 0;
};

struct EmptyStmt : Visitable<Stmt, EmptyStmt> {};                   // 常用于占位或分号单独成句的情况 eg: ;

struct ActionStmt : Visitable<Stmt, ActionStmt> {
    std::string ident;
    std::string code;

    ActionStmt(std::string &ident, std::string &code) :
        ident(std::move(ident)),
        code(std::move(code)) {
#ifdef DEBUG_CLS 
        printf("new ActionStmt: ident: %s, code: %s\n",
                ident, code);
#endif
    }

    ~ActionStmt() {
    }
};

struct CppStmt : Visitable<Stmt, CppStmt> {
    std::string code;
    CppStmt(std::string &code) :
        code(std::move(code)) {
#ifdef DEBUG_CLS 
        printf("new CppStmt\n");
#endif
    }
};

struct DefineStmt : Visitable<Stmt, DefineStmt> {
    bool export_ = false;
    bool intact = false;
    std::string export_params;
    std::string lhs;
    Expr *rhs;
    Module *module;

    DefineStmt(std::string &lhs, Expr *rhs) :
        lhs(std::move(lhs)),
        rhs(rhs) {
#ifdef DEBUG_CLS 
        printf("new DefineStmt: lhs: %s, rhs: TODO\n",
                lhs);
#endif
    }

    ~DefineStmt() {
        delete rhs;
    }
};

struct ImportStmt : Visitable<Stmt, ImportStmt> {
    std::string filename;
    std::string qualified;

    ImportStmt(std::string &filename, std::string &qualified) :
        filename(std::move(filename)),
        qualified(std::move(qualified)) {
#ifdef DEBUG_CLS 
        printf("new ImportStmt: filename: %s, qualified: %s\n",
                filename, qualified);
#endif
    }

    ~ImportStmt() {
    }
};

struct PreprocessDefineStmt : Visitable<Stmt, PreprocessDefineStmt> {
    std::string ident;   
    long value;
    PreprocessDefineStmt(std::string &ident, long value) :
        ident(std::move(ident)),
        value(value) {
#ifdef DEBUG_CLS 
        printf("new PreprocessDefineStmt: ident: %s, value: %ld\n",
                ident.c_str(), value);
#endif
    }
};

void stmt_free(Stmt *stmt);

// Visitor imp
struct StmtPrinter : Visitor<Action>, Visitor<Expr>, Visitor<Stmt> {
    int depth = 0;

    // action
    void visit(Action &action) override {
        action.accept(*this);
    }

    void visit(InlineAction &action) override {
        printf("%*s%s\n", 2 * depth, "", "InlineAction");
        printf("%*s%s\n", 2 * (depth + 1), "", action.code.c_str());
    }

    void visit(RefAction &action) override {
        printf("%*s%s\n", 2 * depth, "", "RefAction");
        printf("%*s%s\n", 2 * (depth + 1), "", action.ident.c_str());
    }

    // stmt
    void visit(Stmt &stmt) override {
        stmt.accept(*this);
    }

    void visit(ActionStmt &stmt) override {
        printf("%*s%s\n", 2 * depth, "", "ActionStmt");
        printf("%*s%s\n", 2 * (depth + 1), "", stmt.ident.c_str());
        printf("%*s%s\n", 2 * (depth + 1), "", stmt.code.c_str());
    }

    void visit(CppStmt &stmt) override {
        printf("%*s%s\n", 2 * depth, "", "CppStmt");
        printf("%*s%s\n", 2 * (depth + 1), "", stmt.code.c_str());
    }

    void visit(DefineStmt &stmt) override {
        printf("%*s%s%s\n", 2 * depth, "", "DefineStmt", stmt.export_ ? " export" : "");
        depth++;
        ident(stdout, depth);
        if (stmt.export_params.size()) {
            printf("(%s) ", stmt.export_params.c_str());
        }
        printf("%s\n", stmt.lhs.c_str());
        visit(*stmt.rhs);
        depth--;
    }

    void visit(EmptyStmt &) override {
        printf("%*s%s\n", 2 * depth, "", "EmptyStmt");
    }

    void visit(ImportStmt &stmt) override {
        printf("%*s%s\n", 2 * depth, "", "ImportStmt");
        printf("%*s%s\n", 2 * (depth + 1), "", stmt.filename.c_str());
        if (stmt.qualified.size()) {
            printf("%*sas %s\n", 2 * (depth + 1), "", stmt.qualified.c_str());
        }
    }

    void visit(PreprocessDefineStmt &stmt) override {
        printf("%*s%s\n", 2 * depth, "", "PreprocessDefineStmt");
        printf("%*s%s %ld\n", 2 * (depth + 1), "", stmt.ident.c_str(), stmt.value);
    }

    // expr
    void visit(Expr &expr) override {
        if (expr.entering.size()) {
            printf("%*s%s\n", 2 * depth, "", "@entering");
            depth++;
            for (auto a : expr.entering) {
                ident(stdout, depth);
                printf("%ld\n", a.second);
                a.first->accept(*this);
            }
            depth--;
        }
        if (expr.finishing.size()) {
            printf("%*s%s\n", 2 * depth, "", "@finishing");
            depth++;
            for (auto a : expr.finishing) {
                ident(stdout, depth);
                printf("%ld\n", a.second);
                a.first->accept(*this);
            }
            depth--;
        }
        if (expr.leaving.size()) {
            printf("%*s%s\n", 2 * depth, "", "@leaving");
            depth++;
            for (auto a : expr.leaving) {
                ident(stdout, depth);
                printf("%ld\n", a.second);
                a.first->accept(*this);
            }
            depth--;
        }
        if (expr.transiting.size()) {
            printf("%*s%s\n", 2 * depth, "", "@transiting");
            depth++;
            for (auto a : expr.transiting) {
                ident(stdout, depth);
                printf("%ld\n", a.second);
                a.first->accept(*this);
            }
            depth--;
        }
        expr.accept(*this);
    }

    void visit(BracketExpr &expr) override {
        std::string info = expr.dump_info();
        printf("%*s%s%s\n", 2 * depth, "", "BracketExpr: ", info.c_str());
        printf("%*s", 2 * (depth + 1), "");
        for (auto &x : expr.intervals.to) {
            printf("(%ld,%ld)", x.first, x.second);
        }
        puts("");
    }

    void visit(CallExpr &expr) override {
        printf("%*s%s\n", 2 * depth, "", "CallExpr");
        printf("%*s", 2 * (depth + 1), "");
        if (expr.qualified.size()) {
            printf("%s::%s\n", expr.qualified.c_str(), expr.ident.c_str());
        } else {
            printf("%s\n", expr.ident.c_str());
        }
    }
    
    void visit(ClosureExpr &expr) override {
        printf("%*s%s\n", 2 * depth, "", "ClosureExpr");
        depth++;
        visit(*expr.inner);
        depth--;
    }

    void visit(CollapseExpr &expr) override {
        std::string info = expr.dump_info();
        printf("%*s%s%s\n", 2 * depth, "", "CollapseExpr: ", info.c_str());
        printf("%*s", 2 * (depth + 1), "");
        if (expr.qualified.size()) {
            printf("%s.%s\n", expr.qualified.c_str(), expr.ident.c_str());
        } else {
            printf("%s\n", expr.ident.c_str());
        }
    }

    void visit(ComplementExpr &expr) override {
        printf("%*s%s\n", 2 * depth, "", "ComplementExpr");
        depth++;
        visit(*expr.inner);
        depth--;
    }

    void visit(ConcatExpr &expr) override {
        printf("%*s%s\n", 2 * depth, "", "ConcatExpr");
        depth++;
        visit(*expr.lhs);
        visit(*expr.rhs);
        depth--;
    }

    void visit(DifferenceExpr &expr) override {
        printf("%*s%s\n", 2 * depth, "", "DifferenceExpr");
        depth++;
        visit(*expr.lhs);
        visit(*expr.rhs);
        depth--;
    }

    void visit(DotExpr &) override {
        printf("%*s%s\n", 2 * depth, "", "DotExpr");
    }

    void visit(EmbedExpr &expr) override {
        printf("%*s%s\n", 2 * depth, "", "EmbedExpr");
        printf("%*s", 2 * (depth + 1), "");
        if (expr.qualified.size()) {
            printf("%s.%s\n", expr.qualified.c_str(), expr.ident.c_str());
        } else {
            printf("%s\n", expr.ident.c_str());
        }
    }

    void visit(EpsilonExpr &) override {
        printf("%*s%s\n", 2 * depth, "", "EpsilonExpr");
    }

    void visit(IntersectExpr &expr) override {
        printf("%*s%s\n", 2 * depth, "", "IntersectExpr");
        depth++;
        visit(*expr.lhs);
        visit(*expr.rhs);
        depth--;
    }

    void visit(LiteralExpr &expr) override {
        printf("%*s%s\n", 2 * depth, "", "LiteralExpr");
        printf("%*s%s\n", 2 * (depth + 1), "", expr.literal.c_str());
    }

    void visit(MaybeExpr &expr) override {
        printf("%*s%s\n", 2 * depth, "", "MaybeExpr");
        depth++;
        visit(*expr.inner);
        depth--;
    }

    void visit(PlusExpr &expr) override {
        printf("%*s%s\n", 2 * depth, "", "PlusExpr");
        depth++;
        visit(*expr.inner);
        depth--;
    }

    void visit(RepeatExpr &expr) override {
        printf("%*s%s\n", 2 * depth, "", "RepeatExpr");
        printf("%*s%ld,%ld\n", 2 * (depth + 1), "", expr.low, expr.high);
        depth++;
        visit(*expr.inner);
        depth--;
    }

    void visit(UnionExpr &expr) override {
        printf("%*s%s\n", 2 * depth, "", "UnionExpr");
        depth++;
        visit(*expr.lhs);
        visit(*expr.rhs);
        depth--;
    }
};

struct PreorderStmtVisitor : Visitor<Stmt> {
    void visit(Stmt &stmt) override {
        stmt.accept(*this);
    }

    void visit(ActionStmt &) override {
#ifdef DEBUG_CLS 
        printf("visit PreorderStmtVisitor ActionStmt\n");
#endif
    }

    void visit(CppStmt &) override {
#ifdef DEBUG_CLS 
        printf("visit PreorderStmtVisitor CppStmt\n");
#endif
    }

    void visit(DefineStmt &) override {
#ifdef DEBUG_CLS 
        printf("visit PreorderStmtVisitor DefineStmt\n");
#endif
    }

    void visit(EmptyStmt &) override {
#ifdef DEBUG_CLS 
        printf("visit PreorderStmtVisitor EmptyStmt\n");
#endif
    }

    void visit(ImportStmt &) override {
#ifdef DEBUG_CLS 
        printf("visit PreorderStmtVisitor ImportStmt\n");
#endif
    }

    void visit(PreprocessDefineStmt &) override {
#ifdef DEBUG_CLS 
        printf("visit PreorderStmtVisitor PreprocessDefineStmt\n");
#endif
    }
};

struct PrePostActionExprStmtVisitor : Visitor<Action>, Visitor<Expr>, Visitor<Stmt> {

    virtual void pre_action(Action &) {
#ifdef DEBUG_CLS 
        printf("visit PreorderActionExprStmtVisitor pre action\n");
#endif
    }

    virtual void post_action(Action &) {
#ifdef DEBUG_CLS 
        printf("visit PreorderActionExprStmtVisitor post action\n");
#endif
    }

    virtual void pre_expr(Expr &) {
#ifdef DEBUG_CLS 
        printf("visit PreorderActionExprStmtVisitor pre expr\n");
#endif
    }

    virtual void post_expr(Expr &) {
#ifdef DEBUG_CLS 
        printf("visit PreorderActionExprStmtVisitor post expr\n");
#endif
    }

    virtual void pre_stmt(Stmt &) {
#ifdef DEBUG_CLS 
        printf("visit PreorderActionExprStmtVisitor pre stmt\n");
#endif
    }

    virtual void post_stmt(Stmt &) {
#ifdef DEBUG_CLS 
        printf("visit PreorderActionExprStmtVisitor post stmt\n");
#endif
    }
    
    // action
    void visit(Action &action) override {
#ifdef DEBUG_CLS 
        printf("visit PreorderActionExprStmtVisitor Action\n");
#endif
        pre_action(action);
        action.accept(*this);
        post_action(action);
    }

    void visit(InlineAction &) override {
#ifdef DEBUG_CLS 
        printf("visit PreorderActionExprStmtVisitor InlineAction\n");
#endif
    }

    void visit(RefAction &) override {
#ifdef DEBUG_CLS 
        printf("visit PreorderActionExprStmtVisitor RefAction\n");
#endif
    }

    // expr
    void visit(Expr &expr) override {
#ifdef DEBUG_CLS 
        printf("visit PreorderActionExprStmtVisitor Expr\n");
#endif
        pre_expr(expr);
        expr.accept(*this);
        post_expr(expr);
    }

    void visit(BracketExpr &) override {
#ifdef DEBUG_CLS 
        printf("visit PreorderActionExprStmtVisitor BracketExpr\n");
#endif
    }

    void visit(ClosureExpr &expr) override {
#ifdef DEBUG_CLS 
        printf("visit PreorderActionExprStmtVisitor ClosureExpr\n");
#endif
        visit(*expr.inner);
    }

    void visit(CollapseExpr &) override {
#ifdef DEBUG_CLS 
        printf("visit PreorderActionExprStmtVisitor CollapseExpr\n");
#endif
    }

    void visit(ComplementExpr &expr) override {
#ifdef DEBUG_CLS 
        printf("visit PreorderActionExprStmtVisitor ComplementExpr\n");
#endif
        visit(*expr.inner);
    }

    void visit(ConcatExpr &expr) override {
#ifdef DEBUG_CLS 
        printf("visit PreorderActionExprStmtVisitor ConcatExpr\n");
#endif
        visit(*expr.lhs);
        visit(*expr.rhs);
    }

    void visit(DifferenceExpr &expr) override {
#ifdef DEBUG_CLS 
        printf("visit PreorderActionExprStmtVisitor DifferenceExpr\n");
#endif
        visit(*expr.lhs);
        visit(*expr.rhs);
    }

    void visit(DotExpr &) override {
#ifdef DEBUG_CLS 
        printf("visit PreorderActionExprStmtVisitor DotExpr\n");
#endif
    }

    void visit(EmbedExpr &) override {
#ifdef DEBUG_CLS 
        printf("visit PreorderActionExprStmtVisitor EmbedExpr\n");
#endif
    }

    void visit(EpsilonExpr &) override {
#ifdef DEBUG_CLS 
        printf("visit PreorderActionExprStmtVisitor EpsilonExpr\n");
#endif
    }

    void visit(IntersectExpr &expr) override {
#ifdef DEBUG_CLS 
        printf("visit PreorderActionExprStmtVisitor IntersectExpr\n");
#endif
        visit(*expr.lhs);
        visit(*expr.rhs);
    }

    void visit(LiteralExpr &) override {
#ifdef DEBUG_CLS 
        printf("visit PreorderActionExprStmtVisitor LiteralExpr\n");
#endif
    }

    void visit(MaybeExpr &expr) override {
#ifdef DEBUG_CLS 
        printf("visit PreorderActionExprStmtVisitor MaybeExpr\n");
#endif
        visit(*expr.inner);
    }

    void visit(PlusExpr &expr) override {
#ifdef DEBUG_CLS 
        printf("visit PreorderActionExprStmtVisitor PlusExpr\n");
#endif
        visit(*expr.inner);
    }

    void visit(RepeatExpr &expr) override {
#ifdef DEBUG_CLS 
        printf("visit PreorderActionExprStmtVisitor RepeatExpr\n");
#endif
        visit(*expr.inner);
    }

    void visit(UnionExpr &expr) override {
#ifdef DEBUG_CLS 
        printf("visit PreorderActionExprStmtVisitor UnionExpr\n");
#endif
        visit(*expr.lhs);
        visit(*expr.rhs);
    }


    // stmt
    void visit(Stmt &stmt) override {
#ifdef DEBUG_CLS 
        printf("visit PreorderActionExprStmtVisitor Stmt\n");
#endif
        pre_stmt(stmt);
        stmt.accept(*this);
        post_stmt(stmt);
    }

    void visit(ActionStmt &) override {
#ifdef DEBUG_CLS 
        printf("visit PreorderActionExprStmtVisitor ActionStmt\n");
#endif
    }

    void visit(CppStmt &) override {
#ifdef DEBUG_CLS 
        printf("visit PreorderActionExprStmtVisitor CppStmt\n");
#endif
    }

    void visit(DefineStmt &stmt) override {
#ifdef DEBUG_CLS 
        printf("visit PreorderActionExprStmtVisitor DefineStmt\n");
#endif
        stmt.rhs->accept(*this);
    }

    void visit(EmptyStmt &) override {
#ifdef DEBUG_CLS 
        printf("visit PreorderActionExprStmtVisitor EmptyStmt\n");
#endif
    }

    void visit(ImportStmt &) override {
#ifdef DEBUG_CLS 
        printf("visit PreorderActionExprStmtVisitor ImportStmt\n");
#endif
    }

    void visit(PreprocessDefineStmt &) override {
#ifdef DEBUG_CLS 
        printf("visit PreorderActionExprStmtVisitor PreprocessDefineStmt\n");
#endif
    }

};

