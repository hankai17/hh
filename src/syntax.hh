#pragma once

#include "location.hh"
#include <bitset>
#include <vector>

#define DEBUG_CLS 1

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
struct BracketExpr;     // 字符集表达式: 最常见的字符类，比如正则中的 [a-z0-9] 这里的括号是[]
struct ClosureExpr;     // 闭包表达式: 对应正则中的 * 重复操作 0次或多次
struct CollapseExpr;    // 折叠表达式: 使用 ! 操作符引用的非终结符 不记账递归
struct ConcatExpr;      // 连接表达式: 隐式默认存在 常见的组合方式 
struct DifferenceExpr;  // 差集表达式: [a-z] - [aeiou] 可以匹配辅音字母
struct DotExpr;
struct EmbedExpr;       // 嵌入表达式: 普通引用一个非终结符（没有 ! 或 & 修饰符） // 用于表示一个标识符（变量名、子模式名等）直接作为表达式使用
struct IntersectExpr;
struct LiteralExpr;
struct MaybeExpr;
struct PlusExpr;        // 正闭包表达式: 对应正则中的 +
struct UnionExpr;       // 并集表达式: 或关系 |

template <>
struct Visitor<Expr> {                              // 1 visitor虚基类定义expr各接口
    virtual void visit(Expr &) = 0;
    virtual void visit(BracketExpr &) = 0;
    virtual void visit(ClosureExpr &) = 0;
    virtual void visit(CollapseExpr &) = 0;
    virtual void visit(ConcatExpr &) = 0;
    virtual void visit(DifferenceExpr &) = 0;
    virtual void visit(DotExpr &) = 0;
    virtual void visit(EmbedExpr &) = 0;
    virtual void visit(IntersectExpr &) = 0;
    virtual void visit(LiteralExpr &) = 0;
    virtual void visit(MaybeExpr &) = 0;
    virtual void visit(PlusExpr &) = 0;
    virtual void visit(UnionExpr &) = 0;
};

// Stmt
struct Stmt;
struct ActionStmt;
struct DefineStmt;          // 赋值语句
struct EmptyStmt;           // 空语句
struct ImportStmt;

template <>
struct Visitor<Stmt> {                              // 同上 visitor虚基类定义stmt各接口
    virtual void visit(Stmt &) = 0;
    virtual void visit(ActionStmt &) = 0;
    virtual void visit(DefineStmt &) = 0;
    virtual void visit(EmptyStmt &) = 0;
    virtual void visit(ImportStmt &) = 0;
};

///////////////////////////////////////////////////// CRTP

// Action
struct Action: VisitableBase<Action> {
    Location loc;
    virtual ~Action() = default;
};

struct InlineAction: Visitable<Action, InlineAction> {
    char *code;
    InlineAction(char *code) : code(code) {
#ifdef DEBUG_CLS 
        printf("new InlineAction\n");
#endif
    }
    ~InlineAction() {
        free(code);
    }
};

struct RefAction : Visitable<Action, RefAction> {
    char *ident;
    RefAction(char *ident) : ident(ident) {
#ifdef DEBUG_CLS 
        printf("new RefAction\n");
#endif
    }
    ~RefAction() {
        free(ident);
    }
};

// Expr
struct Expr : VisitableBase<Expr> {                 // 2 Expr 白嫖accept接口 (CRTP典型做法继承自身)
    Location loc;
    std::vector<Action *> entering;
    std::vector<Action *> finishing;
    std::vector<Action *> leaving;
    std::vector<Action *> transiting;
    virtual ~Expr() = default;
};

struct BracketExpr : Visitable<Expr, BracketExpr> { // 2.1 CRTP 但还没有实现expr中 该类的visit接口
    std::bitset<256> charset;

    BracketExpr(std::bitset<256> *in_charset) :
        charset(*in_charset) {
        delete in_charset;
#ifdef DEBUG_CLS 
        printf("new BracketExpr\n");
#endif
    }
};

struct CollapseExpr : Visitable<Expr, CollapseExpr> {
    char *qualified;
    char *ident;

    CollapseExpr(char *qualified, char *ident) :
        qualified(qualified),
        ident(ident) {
#ifdef DEBUG_CLS 
        printf("new CollapseExpr: qualified: %s, ident: %s\n",
                qualified, ident);
#endif
    }

    ~CollapseExpr() {
        free(qualified);
        free(ident);
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
    char *qualified;
    char *ident;
    DefineStmt *define_stmt = NULL;

    EmbedExpr(char *qualified, char *ident) :
        qualified(qualified),
        ident(ident) {
#ifdef DEBUG_CLS 
        printf("new EmbedExpr: qualified: %s, ident: %s\n",
                qualified, ident);
#endif
    }

    ~EmbedExpr() {
        free(qualified);
        free(ident);
    }
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
    char *literal;

    LiteralExpr(char *literal) :
        literal(literal) {
#ifdef DEBUG_CLS 
        printf("new LiteralExpr: literal: %s\n",
                literal);
#endif
    }

    ~LiteralExpr() {
        free(literal);
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
    char *ident;
    char *code;

    ActionStmt(char *ident, char *code) :
        ident(ident),
        code(code) {
#ifdef DEBUG_CLS 
        printf("new ActionStmt: ident: %s, code: %s\n",
                ident, code);
#endif
    }

    ~ActionStmt() {
        free(ident);
        free(code);
    }
};

struct Module;
struct DefineStmt : Visitable<Stmt, DefineStmt> {                   // x = 5 + 3;
    bool export_;
    char *lhs;  // 左值（变量名）
    Expr *rhs;  // 右值（表达式
    Module *module;

    DefineStmt(bool export_, char *lhs, Expr *rhs) :
        export_(export_),
        lhs(lhs),
        rhs(rhs) {
#ifdef DEBUG_CLS 
        printf("new DefineStmt: lhs: %s, rhs: TODO\n",
                lhs);
#endif
    }

    ~DefineStmt() {
        free(lhs);
        delete rhs;
    }
};

struct ImportStmt : Visitable<Stmt, ImportStmt> {
    char *filename;
    char *qualified;

    ImportStmt(char *filename, char *qualified) :
        filename(filename),
        qualified(qualified) {
#ifdef DEBUG_CLS 
        printf("new ImportStmt: filename: %s, qualified: %s\n",
                filename, qualified);
#endif
    }

    ~ImportStmt() {
        free(filename);
        free(qualified);
    }
};

void stmt_free(Stmt *stmt);

// Visitor imp
struct ExprPrinter : Visitor<Expr> {
    void visit(Expr &expr) {}
};

struct StmtPrinter : Visitor<Action>, Visitor<Expr>, Visitor<Stmt> {
    int depth = 0;

    // action
    void visit(Action &action) override {
        action.accept(*this);
    }

    void visit(InlineAction &action) override {
        printf("%*s%s\n", 2 * depth, "", "InlineAction");
        printf("%*s%s\n", 2 * (depth + 1), "", action.code);
    }

    void visit(RefAction &action) override {
        printf("%*s%s\n", 2 * depth, "", "RefAction");
        printf("%*s%s\n", 2 * (depth + 1), "", action.ident);
    }

    // stmt
    void visit(Stmt &stmt) override {
        stmt.accept(*this);
    }

    void visit(DefineStmt &stmt) override {
        printf("%*s%s\n", 2 * depth, "", "DefineStmt");
        depth++;
        printf("%*s%s\n", 2 * depth, "", stmt.lhs);
        visit(*stmt.rhs);
        depth--;
    }

    void visit(ActionStmt &stmt) override {
        printf("%*s%s\n", 2 * depth, "", "ActionStmt");
        printf("%*s%s\n", 2 * (depth + 1), "", stmt.ident);
        printf("%*s%s\n", 2 * (depth + 1), "", stmt.code);
    }

    void visit(EmptyStmt &stmt) override {
        printf("%*s%s\n", 2 * depth, "", "EmptyStmt");
    }

    void visit(ImportStmt &stmt) override {
        printf("%*s%s\n", 2 * depth, "", "ImportStmt");
        printf("%*s%s\n", 2 * (depth + 1), "", stmt.filename);
        if (stmt.qualified) {
            printf("%*sas %s\n", 2 * (depth + 1), "", stmt.qualified);
        }
    }

    // expr
    void visit(Expr &expr) override {
        expr.accept(*this);
    }

    void visit(BracketExpr &expr) override {
        printf("%*s%s\n", 2 * depth, "", "BracketExpr");
        printf("%*s", 2 * (depth + 1), "");
        for (long i = 0, j; i < expr.charset.size(); ) {
            if (!expr.charset[i]) {
                i++;
            } else {
                for (j = i; j < expr.charset.size() && expr.charset[j]; j++) {
                    printf(" %ld-%ld", i, j - 1);
                    i = j;
                }
            }
        }
        puts("");
    }
    
    void visit(ClosureExpr &expr) override {
        printf("%*s%s\n", 2 * depth, "", "ClosureExpr");
        depth++;
        expr.inner->accept(*this);
        depth--;
    }

    void visit(CollapseExpr &expr) override {
        printf("%*s%s\n", 2 * depth, "", "CollapseExpr");
        printf("%*s\n", 2 * (depth + 1), "");
        if (expr.qualified) {
            printf("%s.%s\n", expr.qualified, expr.ident);
        } else {
            printf("%s\n", expr.ident);
        }
    }

    void visit(ConcatExpr &expr) override {
        printf("%*s%s\n", 2 * depth, "", "ConcatExpr");
        depth++;
        expr.lhs->accept(*this);
        expr.rhs->accept(*this);
        depth--;
    }

    void visit(DifferenceExpr &expr) override {
        printf("%*s%s\n", 2 * depth, "", "DifferenceExpr");
        depth++;
        expr.lhs->accept(*this);
        expr.rhs->accept(*this);
        depth--;
    }

    void visit(DotExpr &expr) override {
        printf("%*s%s\n", 2 * depth, "", "DotExpr");
    }

    void visit(EmbedExpr &expr) override {
        printf("%*s%s\n", 2 * depth, "", "EmbedExpr");
        printf("%*s", 2 * (depth + 1), "");
        if (expr.qualified) {
            printf("%s.%s\n", expr.qualified, expr.ident);
        } else {
            printf("%s\n", expr.ident);
        }
    }

    void visit(IntersectExpr &expr) override {
        printf("%*s%s\n", 2 * depth, "", "IntersectExpr");
        depth++;
        expr.lhs->accept(*this);
        expr.rhs->accept(*this);
        depth--;
    }

    void visit(LiteralExpr &expr) override {
        printf("%*s%s\n", 2 * depth, "", "LiteralExpr");
        printf("%*s%s\n", 2 * (depth + 1), "", expr.literal);
    }

    void visit(MaybeExpr &expr) override {
        printf("%*s%s\n", 2 * depth, "", "MaybeExpr");
        depth++;
        expr.inner->accept(*this);
        depth--;
    }

    void visit(PlusExpr &expr) override {
        printf("%*s%s\n", 2 * depth, "", "PlusExpr");
        depth++;
        expr.inner->accept(*this);
        depth--;
    }

    void visit(UnionExpr &expr) override {
        printf("%*s%s\n", 2 * depth, "", "UnionExpr");
        depth++;
        expr.lhs->accept(*this);
        expr.rhs->accept(*this);
        depth--;
    }
};

struct PreorderStmtVisitor : Visitor<Stmt> {
    void visit(Stmt &stmt) override {
        stmt.accept(*this);
    }

    void visit(ActionStmt &stmt) override {
#ifdef DEBUG_CLS 
        printf("visit PreorderStmtVisitor ActionStmt\n");
#endif
    }

    void visit(DefineStmt &stmt) override {
#ifdef DEBUG_CLS 
        printf("visit PreorderStmtVisitor DefineStmt\n");
#endif
    }

    void visit(EmptyStmt &stmt) override {
#ifdef DEBUG_CLS 
        printf("visit PreorderStmtVisitor EmptyStmt\n");
#endif
    }

    void visit(ImportStmt &stmt) override {
#ifdef DEBUG_CLS 
        printf("visit PreorderStmtVisitor ImportStmt\n");
#endif
    }
};

struct PreorderActionExprStmtVisitor : Visitor<Action>, Visitor<Expr>, Visitor<Stmt> {
    // action
    void visit(Action &action) override {
#ifdef DEBUG_CLS 
        printf("visit PreorderActionExprStmtVisitor Action\n");
#endif
        action.accept(*this);
    }

    void visit(InlineAction &action) override {
#ifdef DEBUG_CLS 
        printf("visit PreorderActionExprStmtVisitor InlineAction\n");
#endif
    }

    void visit(RefAction &action) override {
#ifdef DEBUG_CLS 
        printf("visit PreorderActionExprStmtVisitor RefAction\n");
#endif
    }

    // expr
    void visit(Expr &expr) override {
#ifdef DEBUG_CLS 
        printf("visit PreorderActionExprStmtVisitor Expr\n");
#endif
        expr.accept(*this);
    }

    void visit(BracketExpr &expr) override {
#ifdef DEBUG_CLS 
        printf("visit PreorderActionExprStmtVisitor BracketExpr\n");
#endif
    }

    void visit(ClosureExpr &expr) override {
#ifdef DEBUG_CLS 
        printf("visit PreorderActionExprStmtVisitor ClosureExpr\n");
#endif
    }

    void visit(CollapseExpr &expr) override {
#ifdef DEBUG_CLS 
        printf("visit PreorderActionExprStmtVisitor CollapseExpr\n");
#endif
    }

    void visit(ConcatExpr &expr) override {
#ifdef DEBUG_CLS 
        printf("visit PreorderActionExprStmtVisitor ConcatExpr\n");
#endif
        expr.lhs->accept(*this);
        expr.rhs->accept(*this);
    }

    void visit(DifferenceExpr &expr) override {
#ifdef DEBUG_CLS 
        printf("visit PreorderActionExprStmtVisitor DifferenceExpr\n");
#endif
        expr.lhs->accept(*this);
        expr.rhs->accept(*this);
    }

    void visit(DotExpr &expr) override {
#ifdef DEBUG_CLS 
        printf("visit PreorderActionExprStmtVisitor DotExpr\n");
#endif
    }

    void visit(EmbedExpr &expr) override {
#ifdef DEBUG_CLS 
        printf("visit PreorderActionExprStmtVisitor EmbedExpr\n");
#endif
    }

    void visit(IntersectExpr &expr) override {
#ifdef DEBUG_CLS 
        printf("visit PreorderActionExprStmtVisitor IntersectExpr\n");
#endif
        expr.lhs->accept(*this);
        expr.rhs->accept(*this);
    }

    void visit(LiteralExpr &expr) override {
#ifdef DEBUG_CLS 
        printf("visit PreorderActionExprStmtVisitor LiteralExpr\n");
#endif
    }

    void visit(MaybeExpr &expr) override {
#ifdef DEBUG_CLS 
        printf("visit PreorderActionExprStmtVisitor MaybeExpr\n");
#endif
    }

    void visit(PlusExpr &expr) override {
#ifdef DEBUG_CLS 
        printf("visit PreorderActionExprStmtVisitor PlusExpr\n");
#endif
    }

    void visit(UnionExpr &expr) override {
#ifdef DEBUG_CLS 
        printf("visit PreorderActionExprStmtVisitor UnionExpr\n");
#endif
    }


    // stmt
    void visit(Stmt &stmt) override {
#ifdef DEBUG_CLS 
        printf("visit PreorderActionExprStmtVisitor Stmt\n");
#endif
        stmt.accept(*this);
    }

    void visit(ActionStmt &stmt) override {
#ifdef DEBUG_CLS 
        printf("visit PreorderActionExprStmtVisitor ActionStmt\n");
#endif
    }

    void visit(DefineStmt &stmt) override {
#ifdef DEBUG_CLS 
        printf("visit PreorderActionExprStmtVisitor DefineStmt\n");
#endif
        stmt.rhs->accept(*this);
    }

    void visit(EmptyStmt &stmt) override {
#ifdef DEBUG_CLS 
        printf("visit PreorderActionExprStmtVisitor EmptyStmt\n");
#endif
    }

    void visit(ImportStmt &stmt) override {
#ifdef DEBUG_CLS 
        printf("visit PreorderActionExprStmtVisitor ImportStmt\n");
#endif
    }

};

