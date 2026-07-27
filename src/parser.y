%code requires {
#include "location.hh"
#include "syntax.hh"
#include <limits.h>
#include <unicode/utf8.h>

using std::bitset;

#define YYINITDEPTH 1000
#define YYLTYPE Location
#define YYLLOC_DEFAULT(Loc, Rhs, N)                 \
    do {                                            \
        if (N) {                                    \
            (Loc).start = YYRHSLOC(Rhs, 1).start;   \
            (Loc).end = YYRHSLOC(Rhs, N).end;       \
        } else {                                    \
            (Loc).start = YYRHSLOC(Rhs, 0).start;   \
            (Loc).end = YYRHSLOC(Rhs, 0).end;       \
        }                                           \
    } while (0)

int parse(const LocationFile &locfile, Stmt *&res);
}

                                                    // bison 声明
                                                    // 启用位置跟踪（@1、@2 等）。
%locations
                                                    // 错误信息更详细
%define parse.error verbose
                                                    // 生成可重入（纯）解析器
%define api.pure
                                                    // 给 yyparse() 和 yylex() 传递额外参数（res、errors、locfile、lexer）
%parse-param {Stmt *&res}
%parse-param {long &errors}
%parse-param {const LocationFile &locfile}
%parse-param {void **lexer}

%lex-param {Stmt *&res}
%lex-param {long &errors}
%lex-param {const LocationFile &locfile}
%lex-param {void **lexer}
                                                    // 值类型 %union // yylval 可以携带的不同数据类型
%union {
    long integer;                                   // yylval可存储 INTEGER、CHAR
    std::string *str;                                   // yylval可存储 IDENT、STRING_LITERAL、BRACED_CODE
    bitset<256> *charset;                           // yylval可存储 字符集 [...]
    Action *action;
    Expr *expr;                                     // yylval可存储 抽象语法树节点
    Stmt *stmt;
    char *errmsg;
}

%destructor { delete $$; } <str>
%destructor { delete $$; } <action>
%destructor { delete $$; } <expr>
%destructor { delete $$; } <stmt>
%destructor { delete $$; } <charset>

                                                    // Token(lexer解析而得) 声明 // 告诉 Bison 哪些 token(终结符) 有值，以及值的类型
%token ACTION AS CPP DOTDOT EPSILON EXPORT IMPORT INTACT INVALID_CHARACTER SEMISEMI
%token <integer> CHAR INTEGER
%token <str> IDENT
%token <str> STRING_LITERAL
%token <str> BRACED_CODE

%nonassoc IDENT                                     // 声明这些 Token 没有结合性（Non-associative) eg: 不允许 ab cd 这种隐式连接
%nonassoc '.'                                       // 限制点运算符不能连续（如 a..b 是非法的）

                                                    // 非终结符类型 // 指定不同产生式返回值的类型
%type <action> action
%type <stmt> define_stmt stmt stmt_list
%type <expr> concat_expr difference_expr factor repeat intersect_expr union_expr unop_expr
%type <charset> bracket bracket_items
                                                    // 用户代码段
%{
#include "lexer.hh"
#define FAIL(loc, errmsg)                                   \
    do {                                                    \
        Location l = loc;                                   \
        yyerror(&l, res, errors, locfile, lexer, errmsg);   \
    } while (0)

void yyerror(YYLTYPE *loc, Stmt *&res, long& errors,const LocationFile &locfile,
        yyscan_t *lexer, const char *errmsg) {
    errors++;
    locfile.locate(*loc, "%s", errmsg);
}

int yylex(YYSTYPE *yylval, YYLTYPE *loc, Stmt *&res, long &errors,
        const LocationFile &locfile, yyscan_t *lexer) {
    int token = raw_yylex(yylval, loc, *lexer);
    if (token == INVALID_CHARACTER) {
        FAIL(*loc, yylval->errmsg ? yylval->errmsg : "Invalid character");
        free(yylval->errmsg);
    }
   /* 
    if (token == YYEMPTY) {
        locfile.locate(*loc, "EMPTY");
    }
    if (token == YYEOF) {
        locfile.locate(*loc, "YYEOF");
    }
    if (token == YYerror) {
        locfile.locate(*loc, "YYerror");
    }
    if (token == YYUNDEF) {
        locfile.locate(*loc, "YYUNDEF");
    }
    if (token == ACTION) {
        locfile.locate(*loc, "ACTION: \n");
    }
    if (token == AS) {
        locfile.locate(*loc, "AS \n");
    }
    if (token == CPP) {
        locfile.locate(*loc, "CPP \n");
    }
    if (token == DOTDOT) {
        locfile.locate(*loc, "DOTDOT \n");
    }
    if (token == EPSILON) {
        locfile.locate(*loc, "EPSILON \n");
    }
    if (token == EXPORT) {
        locfile.locate(*loc, "EXPORT \n");
    }
    if (token == IMPORT) {
        locfile.locate(*loc, "IMPORT \n");
    }
    if (token == INTACT) {
        locfile.locate(*loc, "INTACT \n");
    }
    if (token == INVALID_CHARACTER) {
        locfile.locate(*loc, "INVALID_CHARACTER");
    }
    if (token == SEMISEMI) {
        locfile.locate(*loc, "SEMISEMI \n");
    }
    if (token == CHAR) {
        locfile.locate(*loc, "CHAR: %c\n", yylval->integer);
    }
    if (token == INTEGER) {
        locfile.locate(*loc, "INTEGER: %d\n", yylval->integer);
    }
    if (token == IDENT) {
        locfile.locate(*loc, "IDENT: %s\n", yylval->str->c_str());
    }
    if (token == STRING_LITERAL) {
        locfile.locate(*loc, "STRING_LITERAL: %s\n", yylval->str->c_str());
    }
    if (token == BRACED_CODE) {
        locfile.locate(*loc, "BRACED_CODE: %s\n", yylval->str->c_str());
    }
    */
    return token;
}

#define gen_repeat(x, inner, low, high) \
    if (low < 0) {                      \
        FAIL(yyloc, "negative");        \
    }                                   \
    if (low > high) {                   \
        FAIL(yyloc, "low > high");      \
    }                                   \
    x = new RepeatExpr(inner, low, high)
%}
                                                    
                                                    // 语法规则
                                                    // $$: 代表左边的非终结符 用于构建AST 就是.h中定义的各种类
                                                    // $1: 代表右边第1个符号的值
                                                    // Reduce: 规约: 把右边合并成左边 ---> token($x)作为输入匹配某个规则后 分配对象 合并成一个expr
%%
toplevel:
    stmt_list { res = $1; }                         // 整个输入是语句列表
                                                    // stmt_list 的结果赋值给 res // res 是parse函数的 in-out参数
                                                    // 把解析完的所有语句交给外部调用者

stmt_list:                                          // 支持空列表 + 链表式多条语句（双向链表）
    %empty { $$ = new EmptyStmt; }                  // 创建一个 EmptyStmt 对象，作为链表的结束标志
    | '\n' stmt_list {                              // 接把后面的 stmt_list 传递上去（$$ = $2） // 允许文件开头有空行，或者连续多个换行
        $$ = $2;
    }
    | stmt '\n' stmt_list {                         // 正常语句 + 换行 
        $1->next = $3;
        $3->prev = $1;
        $$ = $1;
    }
    | error '\n' stmt_list {
        $$ = $3;
    }

stmt:
    define_stmt { $$ = $1; }
    | IMPORT STRING_LITERAL AS IDENT { $$ = new ImportStmt(*$2, *$4); delete $2; delete $4; $$->loc = yyloc; }
    | IMPORT STRING_LITERAL { std::string t; $$ = new ImportStmt(*$2, t); delete $2; $$->loc = yyloc; }
    | ACTION IDENT BRACED_CODE { $$ = new ActionStmt(*$2, *$3); delete $2; delete $3; $$->loc = yyloc; }
    | CPP BRACED_CODE { $$ = new CppStmt(*$2); delete $2; $$->loc = yyloc; }
    | error {}

define_stmt:
    IDENT '=' union_expr { $$ = new DefineStmt(*$1, $3); delete $1; $$->loc = yyloc; }
    | IDENT ':' union_expr { $$ = new DefineStmt(*$1, $3); delete $1; $$->loc = yyloc; }
    | IDENT ':' '|' union_expr { $$ = new DefineStmt(*$1, $4); delete $1; $$->loc = yyloc; }
    | EXPORT define_stmt { $$ = $2; ((DefineStmt*)$$)->export_ = true; $$->loc = yyloc; }
    | INTACT define_stmt { $$ = $2; ((DefineStmt*)$$)->intact = true; $$->loc = yyloc; }

union_expr:                                         // 并集  ab即a后边跟着b即ab就是并集
    intersect_expr { $$ = $1; }
    | union_expr '|' intersect_expr { $$ = new UnionExpr($1, $3); $$->loc = yyloc; }

intersect_expr:
    difference_expr { $$ = $1; }
    | intersect_expr '&' difference_expr { $$ = new IntersectExpr($1, $3); $$->loc = yyloc; }

difference_expr:                                    // 差集
    concat_expr { $$ = $1; }
    | difference_expr '-' concat_expr { $$ = new DifferenceExpr($1, $3); $$->loc = yyloc; }


concat_expr:                                        // 链接
    unop_expr { $$ = $1; }
    | concat_expr unop_expr { $$ = new ConcatExpr($1, $2); $$->loc = yyloc; }

unop_expr:                                        // 链接
    factor { $$ = $1; }
    | '~' unop_expr { $$ = new ComplementExpr($2); $$->loc = yyloc; }

factor:                                             // 基础因子
    EPSILON { $$ = new EpsilonExpr; $$->loc = yyloc; }
    | IDENT { std::string t; $$ = new EmbedExpr(t, *$1); delete $1; $$->loc = yyloc; }         // IDENT类型 创建的AST实例类型是EmbedExpr       eg: abc
    | IDENT SEMISEMI IDENT { $$ = new EmbedExpr(*$1, *$3); delete $1; delete $3; $$->loc = yyloc; }
    | '!' IDENT { std::string t; $$ = new CollapseExpr(t, *$2); delete $2; $$->loc = yyloc; }// &IDENT ...                                   eg: &ref
    | '!' IDENT SEMISEMI IDENT { $$ = new CollapseExpr(*$2, *$4); delete $2; delete $4; $$->loc = yyloc; }   // ?
    | STRING_LITERAL { $$ = new LiteralExpr(*$1); delete $1; $$->loc = yyloc; }
    | '.' { $$ = new DotExpr(); $$->loc = yyloc; }
    | bracket { $$ = new BracketExpr($1); $$->loc = yyloc; }         // bracket类型 创建的AST实例类型是BracketExpr   eg: [a-z]
    | STRING_LITERAL DOTDOT STRING_LITERAL {
        i32 c0;
        i32 c1;
        i32 i = 0;
        i32 j = 0;
        U8_NEXT($1->c_str(), i, $1->size(), c0);
        U8_NEXT($3->c_str(), j, $3->size(), c1);
        delete $1;
        delete $3;
        if (i != $1->size() || j != $3->size()) {
            FAIL(yyloc, "endpoints of Unicode range should be of length 1");
            $$ = new DotExpr;
        } else if (c0 > c1) {
            FAIL(yyloc, "negative Unicode range");
            $$ = new DotExpr;
        } else {
            $$ = new UnicodeRangeExpr(c0, c1 + 1);
            $$->loc = yyloc;
        }
      }
    | '(' union_expr ')' { $$ = $2; }
    | repeat { $$ = $1; }
    | factor '>' action { $$ = $1; $$->entering.push_back($3); }
    | factor '@' action { $$ = $1; $$->finishing.push_back($3); }
    | factor '%' action { $$ = $1; $$->leaving.push_back($3); }
    | factor '$' action { $$ = $1; $$->transiting.push_back($3); }
    | factor '?' { $$ = new MaybeExpr($1); $$->loc = yyloc; }
    | factor '*' { $$ = new ClosureExpr($1); $$->loc = yyloc; }
    | factor '+' { $$ = new PlusExpr($1); $$->loc = yyloc; }

repeat:
    factor '{' INTEGER ',' INTEGER '}' { gen_repeat($$, $1, $3, $5); $$->loc = yyloc; }
    | factor '{' INTEGER ',' '}' { gen_repeat($$, $1, $3, LONG_MAX); $$->loc = yyloc; }
    | factor '{' INTEGER '}' { gen_repeat($$, $1, $3, $3); $$->loc = yyloc; }
    | factor '{' ',' INTEGER '}' { gen_repeat($$, $1, 0, $4); $$->loc = yyloc; }

action:
    IDENT { std::string t; $$ = new RefAction(t, *$1); delete $1; $$->loc = yyloc; }
    | IDENT SEMISEMI IDENT { $$ = new RefAction(*$1, *$3); delete $1; delete $3; $$->loc = yyloc; }
    | BRACED_CODE { $$ = new InlineAction(*$1); delete $1; $$->loc = yyloc; }

bracket:                                            // 字符集
    '[' bracket_items ']' {                         // bracket_items { $$ = $1;
        $$ = $2;
        //printf("bracket [ ] \n");
    }
    | '[' '^' bracket_items ']' {
        $$ = $3;
        REP(i, $$->size())
            $3->flip(i);
    }

bracket_items:
    bracket_items CHAR '-' CHAR {                   // 递归 // 前提是必需得有上面的基础作为起点/引子 才能递归
        $$ = $1;                                    // 把原来的 bitset 继承下来
        if ($2 > $4) {
            FAIL(yyloc, "Negative range in character class");
        } else {
            FOR(i, $2, $4 + 1)                      // $2 是起始字符，$4 是结束字符
                $$->set(i);
        }
    }
    | bracket_items CHAR {
        $$ = $1;
        $$->set($2) ;
    }
    | %empty { $$ = new bitset<256>; }
%% 

int parse(const LocationFile &locfile, Stmt *&res) {// 对外的解析入口函数
    yyscan_t lexer;
    long errors = 0;
    raw_yylex_init_extra(0, &lexer);                // 初始化 Flex lexer
    YY_BUFFER_STATE buf = raw_yy_scan_bytes(locfile.data.c_str(),   // 描输入字符串
            locfile.data.size(), lexer);
    yyparse(res, errors, locfile, &lexer);          // 调用 Bison 的 yyparse()
    raw_yy_delete_buffer(buf, lexer);               // 清理资源
    raw_yylex_destroy(lexer);
    if (errors > 0) {
        stmt_free(res);
        res = NULL;
    }
    return errors;
}

