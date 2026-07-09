#pragma once

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

struct Action;
struct InlineAction;
struct RefAction;
struct ActionVisitor {
    virtual void visit(Action &) = 0;
    virtual void visit(InlineAction &) = 0;
    virtual void visit(RefAction &) = 0;
};
template <> struct Visitor<Action> : ActionVisitor {};

struct Expr;
struct BracketExpr;     // 字符集表达式: 最常见的字符类，比如正则中的 [a-z0-9] 这里的括号是[]
struct ClosureExpr;     // 闭包表达式: 对应正则中的 * 重复操作 0次或多次
struct CollapseExpr;    // 折叠表达式: 使用 ! 操作符引用的非终结符 不记账递归
struct ConcatExpr;      // 连接表达式: 隐式默认存在 常见的组合方式 
struct DifferenceExpr;  // 差集表达式: [a-z] - [aeiou] 可以匹配辅音字母
struct DotExpr;
struct EmbedExpr;       // 嵌入表达式: 普通引用一个非终结符（没有 ! 或 & 修饰符） // 用于表示一个标识符（变量名、子模式名等）直接作为表达式使用
struct LiteralExpr;
struct MaybeExpr;
struct PlusExpr;        // 正闭包表达式: 对应正则中的 +
struct UnionExpr;       // 并集表达式: 或关系 |

struct ExprVisitor {
    virtual void visit(Expr &) = 0;
    virtual void visit(BracketExpr &) = 0;
    virtual void visit(ClosureExpr &) = 0;
    virtual void visit(CollapseExpr &) = 0;
    virtual void visit(ConcatExpr &) = 0;
    virtual void visit(DifferenceExpr &) = 0;
    virtual void visit(DotExpr &) = 0;
    virtual void visit(EmbedExpr &) = 0;
    virtual void visit(LiteralExpr &) = 0;
    virtual void visit(MaybeExpr &) = 0;
    virtual void visit(PlusExpr &) = 0;
    virtual void visit(UnionExpr &) = 0;
};

template <> struct Visitor<Expr> : ExprVisitor {};  // 1 visitor虚基类定义expr各接口

struct Stmt;
struct ActionStmt;
struct AssignStmt;          // 赋值语句
struct EmptyStmt;           // 空语句
struct ImportStmt;
struct InstantiationStmt;   // 实例化 / 声明语句
struct StmtVisitor {
    virtual void visit(Stmt &) = 0;
    virtual void visit(ActionStmt &) = 0;
    virtual void visit(AssignStmt &) = 0;
    virtual void visit(EmptyStmt &) = 0;
    virtual void visit(ImportStmt &) = 0;
    virtual void visit(InstantiationStmt &) = 0;
};

template <> struct Visitor<Stmt> : StmtVisitor {};  // 同上 visitor虚基类定义stmt各接口

// Action
struct Action: VisitableBase<Action> {
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
    char *module;
    char *ident;
    CollapseExpr(char *module, char *ident) :
        module(module),
        ident(ident) {
#ifdef DEBUG_CLS 
        printf("new CollapseExpr\n");
#endif
    }
    ~CollapseExpr() {
        free(module);
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
    char *module;
    char *ident;
    EmbedExpr(char *module, char *ident) :
        module(module),
        ident(ident) {
#ifdef DEBUG_CLS 
        printf("new EmbedExpr\n");
#endif
    }
    ~EmbedExpr() {
        free(module);
        free(ident);
    }
};

struct LiteralExpr : Visitable<Expr, LiteralExpr> {
    char *literal;
    LiteralExpr(char *literal) :
        literal(literal) {
#ifdef DEBUG_CLS 
        printf("new LiteralExpr\n");
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
        printf("new ActionStmt\n");
#endif
    }
    ~ActionStmt() {
        free(ident);
        free(code);
    }
};

struct AssignStmt : Visitable<Stmt, AssignStmt> {                   // x = 5 + 3;
    bool export_;
    char *lhs;  // 左值（变量名）
    Expr *rhs;  // 右值（表达式
    AssignStmt(bool export_, char *lhs, Expr *rhs) :
        export_(export_),
        lhs(lhs),
        rhs(rhs) {
#ifdef DEBUG_CLS 
        printf("new AssignStmt\n");
#endif
    }
    ~AssignStmt() {
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
        printf("new ImportStmt\n");
#endif
    }
    ~ImportStmt() {
        free(filename);
        free(qualified);
    }
};

struct InstantiationStmt : Visitable<Stmt, InstantiationStmt> {     // int x = 10
    char *lhs;
    Expr *rhs;
    InstantiationStmt(char *lhs, Expr *rhs) :
        lhs(lhs), rhs(rhs) {
#ifdef DEBUG_CLS 
        printf("new InstantiationStmt\n");
#endif
    }
    ~InstantiationStmt() { free(lhs); delete rhs; }
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

    void visit(AssignStmt &stmt) override {
        printf("%*s%s\n", 2 * depth, "", "AssignStmt");
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

    void visit(InstantiationStmt &stmt) override {
        printf("%*s%s\n", 2 * depth, "", "InstantiationStmt");
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
        if (expr.module) {
            printf("%s.%s\n", expr.module, expr.ident);
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
        printf("%*s\n", 2 * (depth + 1), "");
        if (expr.module) {
            printf("%s.%s\n", expr.module, expr.ident);
        } else {
            printf("%s\n", expr.ident);
        }
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

