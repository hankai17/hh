%code requires {
#include "location.hh"
#include "syntax.hh"
#include <limits.h>
#include <unicode/utf8.h>

using std::bitset;

#define YYINITDEPTH 10000
#define YYMAXDEPTH  100000

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

%locations
%define parse.error verbose
%define api.pure

%parse-param {Stmt *&res}
%parse-param {long &errors}
%parse-param {const LocationFile &locfile}
%parse-param {void **lexer}

%lex-param {Stmt *&res}
%lex-param {long &errors}
%lex-param {const LocationFile &locfile}
%lex-param {void **lexer}

%union {
    long integer;
    std::string *str;
    DisjointIntervals *intervals;
    Action *action;
    Expr *expr;
    Stmt *stmt;
    char *errmsg;
}

%destructor { delete $$; } <str>
%destructor { delete $$; } <action>
%destructor { delete $$; } <expr>
%destructor { delete $$; } <stmt>
%destructor { delete $$; } <intervals>

%token ACTION AMPERAMPER AS COLONCOLON CPP DOTDOT EPSILON EXPORT IMPORT INTACT INVALID_CHARACTER PREPROCESS_DEFINE
%token <integer> CHAR INTEGER
%token <str> IDENT
%token <str> BRACED_CODE
%token <str> STRING_LITERAL

%nonassoc IDENT                                     // 声明这些 Token 没有结合性（Non-associative) eg: 不允许 ab cd 这种隐式连接
%nonassoc '.'                                       // 限制点运算符不能连续（如 a..b 是非法的）

                                                    // 非终结符类型 // 指定不同产生式返回值的类型
%type <action> action
%type <expr> concat_expr difference_expr factor repeat intersect_expr union_expr union_expr2 unop_expr
%type <intervals> bracket bracket_items
%type <stmt> define_stmt preprocess stmt stmt_list
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
                                                    
%%
toplevel:
    stmt_list { res = $1; }

stmt_list:
    %empty { $$ = new EmptyStmt; }
    | '\n' stmt_list { $$ = $2; }
    | stmt stmt_list {
        $1->next = $2;
        $2->prev = $1;
        $$ = $1;
    }
    | error stmt_list { $$ = $2; }

stmt:
    define_stmt { $$ = $1; }
    | preprocess '\n' { $$ = $1; }
    | IMPORT STRING_LITERAL AS IDENT '\n' { $$ = new ImportStmt(*$2, *$4); delete $2; delete $4; $$->loc = yyloc; }
    | IMPORT STRING_LITERAL '\n' { std::string t; $$ = new ImportStmt(*$2, t); delete $2; $$->loc = yyloc; }
    | ACTION IDENT BRACED_CODE '\n' { $$ = new ActionStmt(*$2, *$3); delete $2; delete $3; $$->loc = yyloc; }
    | CPP BRACED_CODE '\n' { $$ = new CppStmt(*$2); delete $2; $$->loc = yyloc; }

preprocess:
    PREPROCESS_DEFINE IDENT INTEGER { $$ = new PreprocessDefineStmt(*$2, $3); delete $2; $$->loc = yyloc; }

eq:
    '='
    | ':'

define_stmt:
    IDENT eq union_expr '\n' { $$ = new DefineStmt(*$1, $3); delete $1; $$->loc = yyloc; }
    | IDENT eq '|' union_expr '\n' { $$ = new DefineStmt(*$1, $4); delete $1; $$->loc = yyloc; }
    | IDENT eq '\n' union_expr2 '\n' { $$ = new DefineStmt(*$1, $4); delete $1; $$->loc = yyloc; }
    | IDENT eq '\n' '|' union_expr2 '\n' { $$ = new DefineStmt(*$1, $5); delete $1; $$->loc = yyloc; }
    | EXPORT define_stmt { $$ = $2; ((DefineStmt*)$$)->export_ = true; $$->loc = yyloc; }
    | EXPORT BRACED_CODE define_stmt { $$ = $3; ((DefineStmt*)$$)->export_ = true; ((DefineStmt*)$$)->export_params = *$2; delete $2; $$->loc = yyloc; }
    | INTACT define_stmt { $$ = $2; ((DefineStmt*)$$)->intact = true; $$->loc = yyloc; }

union_expr:                                         // 并集  ab即a后边跟着b即ab就是并集
    intersect_expr { $$ = $1; }
    | union_expr '|' intersect_expr { $$ = new UnionExpr($1, $3); $$->loc = yyloc; }

union_expr2:
    intersect_expr { $$ = $1; }
    | union_expr2 '|' intersect_expr { $$ = new UnionExpr($1, $3); $$->loc = yyloc; }
    | union_expr2 '\n' '|' intersect_expr { $$ = new UnionExpr($1, $4); $$->loc = yyloc; }

intersect_expr:
    difference_expr { $$ = $1; }
    | intersect_expr AMPERAMPER difference_expr { $$ = new IntersectExpr($1, $3); $$->loc = yyloc; }

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
    | IDENT COLONCOLON IDENT { $$ = new EmbedExpr(*$1, *$3); delete $1; delete $3; $$->loc = yyloc; }
    | '!' IDENT { std::string t; $$ = new CollapseExpr(t, *$2); delete $2; $$->loc = yyloc; }// &IDENT ...                                   eg: &ref
    | '!' IDENT COLONCOLON IDENT { $$ = new CollapseExpr(*$2, *$4); delete $2; delete $4; $$->loc = yyloc; }   // ?
    | '&' IDENT { std::string t; $$ = new CallExpr(t, *$2); delete $2; $$->loc = yyloc; }
    | '&' IDENT COLONCOLON IDENT { $$ = new CallExpr(*$2, *$4); delete $2; delete $4; $$->loc = yyloc; }
    | STRING_LITERAL { $$ = new LiteralExpr(*$1); delete $1; $$->loc = yyloc; }
    | '.' { $$ = new DotExpr(); $$->loc = yyloc; }
    | INTEGER {
        auto t = new DisjointIntervals;
        t->emplace($1, $1 + 1);
        $$ = new BracketExpr(t);
        $$->loc = yyloc;
    }
    | bracket { $$ = new BracketExpr($1); $$->loc = yyloc; }
    | STRING_LITERAL DOTDOT STRING_LITERAL {
        i32 c0 = 0;
        i32 c1 = 0;
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
            auto t = new DisjointIntervals;
            t->emplace(c0, c1 + 1);
            $$ = new BracketExpr(t);
            $$->loc = yyloc;
        }
      }
    | '(' union_expr ')' { $$ = $2; }
    | '(' error ')' { $$ = new DotExpr; }
    | repeat { $$ = $1; }
    | factor '>' action { $$ = $1; $$->entering.emplace_back($3, 0L); }
    | factor '>' INTEGER action { $$ = $1; $$->entering.emplace_back($4, $3); }
    | factor '@' action { $$ = $1; $$->finishing.emplace_back($3, 0L); }
    | factor '@' INTEGER action { $$ = $1; $$->finishing.emplace_back($4, $3); }
    | factor '%' action { $$ = $1; $$->leaving.emplace_back($3, 0L); }
    | factor '%' INTEGER action { $$ = $1; $$->leaving.emplace_back($4, $3); }
    | factor '$' action { $$ = $1; $$->transiting.emplace_back($3, 0L); }
    | factor '$' INTEGER action { $$ = $1; $$->transiting.emplace_back($4, $3); }
    | factor '+' { $$ = new PlusExpr($1); $$->loc = yyloc; }
    | factor '?' { $$ = new MaybeExpr($1); $$->loc = yyloc; }
    | factor '*' { $$ = new ClosureExpr($1); $$->loc = yyloc; }

repeat:
    factor '{' INTEGER ',' INTEGER '}' { gen_repeat($$, $1, $3, $5); $$->loc = yyloc; }
    | factor '{' INTEGER ',' '}' { gen_repeat($$, $1, $3, LONG_MAX); $$->loc = yyloc; }
    | factor '{' INTEGER '}' { gen_repeat($$, $1, $3, $3); $$->loc = yyloc; }
    | factor '{' ',' INTEGER '}' { gen_repeat($$, $1, 0, $4); $$->loc = yyloc; }

action:
    IDENT { std::string t; $$ = new RefAction(t, *$1); delete $1; $$->loc = yyloc; }
    | IDENT COLONCOLON IDENT { $$ = new RefAction(*$1, *$3); delete $1; delete $3; $$->loc = yyloc; }
    | BRACED_CODE { $$ = new InlineAction(*$1); delete $1; $$->loc = yyloc; }

bracket:
    '[' bracket_items ']' { $$ = $2; }
    | '[' '^' bracket_items ']' {
        $$ = $3;
        $$->flip();
    }

bracket_items:
    bracket_items CHAR '-' CHAR {                   // 递归 // 前提是必需得有上面的基础作为起点/引子 才能递归
        $$ = $1;                                    // 把原来的 bitset 继承下来
        if ($2 > $4) {
            FAIL(yyloc, "Negative range in character class");
        } else {
            $$->emplace($2, $4 + 1);
        }
    }
    | bracket_items CHAR {
        $$ = $1;
        $$->emplace($2, $2 + 1);
    }
    | %empty { $$ = new DisjointIntervals; }
%% 

int parse(const LocationFile &locfile, Stmt *&res) {
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

