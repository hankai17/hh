#pragma once

#include <bitset>

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

struct Expr;
struct BracketExpr;     // 字符集表达式: 最常见的字符类，比如正则中的 [a-z0-9] 这里的括号是[]
struct ClosureExpr;     // 闭包表达式: 对应正则中的 * 重复操作 0次或多次
struct CollapseExpr;    // 折叠表达式: 使用 ! 操作符引用的非终结符 不记账递归
struct ConcatExpr;      // 连接表达式: 隐式默认存在 常见的组合方式 
struct DifferenceExpr;  // 差集表达式: [a-z] - [aeiou] 可以匹配辅音字母
struct EmbedExpr;       // 嵌入表达式: 普通引用一个非终结符（没有 ! 或 & 修饰符）
struct PlusExpr;        // 正闭包表达式: 对应正则中的 +
struct UnionExpr;       // 并集表达式: 或关系 |

struct ExprVisitor {
    virtual void visit(Expr &) = 0;
    virtual void visit(BracketExpr &) = 0;
    virtual void visit(ClosureExpr &) = 0;
    virtual void visit(CollapseExpr &) = 0;
    virtual void visit(ConcatExpr &) = 0;
    virtual void visit(DifferenceExpr &) = 0;
    virtual void visit(EmbedExpr &) = 0;
    virtual void visit(PlusExpr &) = 0;
    virtual void visit(UnionExpr &) = 0;
};

template <> struct Visitor<Expr> : ExprVisitor {};

struct Stmt;
struct AssignStmt;          // 赋值语句
struct EmptyStmt;           // 空语句
struct InstantiationStmt;   // 实例化 / 声明语句
struct StmtVisitor {
    virtual void visit(Stmt &) = 0;
    virtual void visit(AssignStmt &) = 0;
    virtual void visit(EmptyStmt &) = 0;
    virtual void visit(InstantiationStmt &) = 0;
};

template <> struct Visitor<Stmt> : StmtVisitor {};

// Expr
struct Expr : VisitableBase<Expr> {
    virtual ~Expr() = default;
};

struct BracketExpr : Visitable<Expr, BracketExpr> {
    std::bitset<256> charset;
    BracketExpr(std::bitset<256> *charset) : charset(*charset) {}
};

struct CollapseExpr : Visitable<Expr, CollapseExpr> {
    char *ident;
    CollapseExpr(char *ident) : ident(ident) {}
};

struct ClosureExpr : Visitable<Expr, ClosureExpr> {
    Expr *inner;
    ClosureExpr(Expr *inner) : inner(inner) {}
    ~ClosureExpr() { delete inner; }
};

struct ConcatExpr : Visitable<Expr, ConcatExpr> {
    Expr *lhs, *rhs;
    ConcatExpr(Expr *lhs, Expr *rhs) : lhs(lhs), rhs(rhs) {}
    ~ConcatExpr() { delete lhs; delete rhs; }
};

struct DifferenceExpr : Visitable<Expr, DifferenceExpr> {
    Expr *lhs, *rhs;
    DifferenceExpr(Expr *lhs, Expr *rhs) : lhs(lhs), rhs(rhs) {}
    ~DifferenceExpr() { delete lhs; delete rhs; }

};

struct EmbedExpr : Visitable<Expr, EmbedExpr> {
    char *ident;
    EmbedExpr(char *ident) : ident(ident) {}
    ~EmbedExpr() { free(ident); }
};

struct PlusExpr : Visitable<Expr, PlusExpr> {
    Expr *inner;
    PlusExpr(Expr *inner) : inner(inner) {}
    ~PlusExpr() { delete inner; }
};

struct UnionExpr : Visitable<Expr, UnionExpr> {
    Expr *lhs, *rhs;
    UnionExpr(Expr *lhs, Expr *rhs) : lhs(lhs), rhs(rhs) {}
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

struct AssignStmt : Visitable<Stmt, AssignStmt> {                   // x = 5 + 3;
    char *lhs;  // 左值（变量名）
    Expr *rhs;  // 右值（表达式
    AssignStmt(char *lhs, Expr *rhs) : lhs(lhs), rhs(rhs) {}
    ~AssignStmt() { free(lhs); delete rhs; }
};

struct InstantiationStmt : Visitable<Stmt, InstantiationStmt> {     // int x = 10
    char *lhs;
    Expr *rhs;
    InstantiationStmt(char *lhs, Expr *rhs) : lhs(lhs), rhs(rhs) {}
    ~InstantiationStmt() { free(lhs); delete rhs; }
};

void stmt_free(Stmt *stmt);

// Visitor imp
struct ExprPrinter : Visitor<Expr> {
    void visit(Expr &expr) {}
};

struct StmtPrinter : Visitor<Expr>, Visitor<Stmt> {
    int depth = 0;

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

    void visit(EmptyStmt &stmt) override {
        printf("%*s%s\n", 2 * depth, "", "EmptyStmt");
    }

    void visit(InstantiationStmt &stmt) override {
        printf("%*s%s\n", 2 * depth, "", "InstantiationStmt");
    }

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
        printf("%*s%s\n", 2 * (depth + 1), "", expr.ident);
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

    void visit(EmbedExpr &expr) override {
        printf("%*s%s\n", 2 * depth, "", "EmbedExpr");
        printf("%*s%s\n", 2 * (depth + 1), "", expr.ident);
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

