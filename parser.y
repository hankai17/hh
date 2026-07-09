%code requires {
#include "location.hh"
#include "syntax.hh"

using std::bitset;

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
    char *string;                                   // yylval可存储 IDENT、STRING_LITERAL、BRACED_CODE
    bitset<256> *charset;                           // yylval可存储 字符集 [...]
    Action *action;
    Expr *expr;                                     // yylval可存储 抽象语法树节点
    Stmt *stmt;
    char *errmsg;
}

%destructor { free($$); }  <string>
%destructor { delete $$; } <action>
%destructor { delete $$; } <expr>
%destructor { delete $$; } <stmt>
%destructor { delete $$; } <charset>

                                                    // Token(lexer解析而得) 声明 // 告诉 Bison 哪些 token(终结符) 有值，以及值的类型
%token ACTION AS EXPORT IMPORT INVALID_CHARACTER SEMISEMI
%token <integer> CHAR INTEGER
%token <string> IDENT
%token <string> STRING_LITERAL
%token <string> BRACED_CODE

%nonassoc IDENT                                     // 声明这些 Token 没有结合性（Non-associative) eg: 不允许 ab cd 这种隐式连接
%nonassoc '.'                                       // 限制点运算符不能连续（如 a..b 是非法的）

                                                    // 非终结符类型 // 指定不同产生式返回值的类型
%type <action> action
%type <stmt> stmt stmt_list
%type <expr> concat_expr difference_expr factor union_expr
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
        locfile.locate(*loc, "IDENT: %s\n", yylval->string);
    }
    if (token == STRING_LITERAL) {
        locfile.locate(*loc, "STRING_LITERAL: %s\n", yylval->string);
    }
    if (token == BRACED_CODE) {
        locfile.locate(*loc, "BRACED_CODE: %s\n", yylval->string);
    }
    return token;
}
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

stmt:                                               // 支持两种语句: 普通赋值：x = expr; 初始化赋值：x := expr;
    IDENT '=' union_expr { $$ = new AssignStmt(false, $1, $3); }
    | EXPORT IDENT '=' union_expr { $$ = new AssignStmt(true, $2, $4); }
    | IMPORT STRING_LITERAL AS IDENT { $$ = new ImportStmt($2, $4); }
    | IMPORT STRING_LITERAL { $$ = new ImportStmt($2, NULL); }
    | ACTION IDENT BRACED_CODE { $$ = new ActionStmt($2, $3); }

union_expr:                                         // 并集  ab即a后边跟着b即ab就是并集
    difference_expr { $$ = $1; }
    | union_expr '|' difference_expr { $$ = new UnionExpr($1, $3); }

difference_expr:                                    // 差集
    concat_expr { $$ = $1; }
    | difference_expr '-' factor { $$ = new DifferenceExpr($1, $3); }

concat_expr:                                        // 链接
    factor { $$ = $1; }
    | concat_expr factor { $$ = new ConcatExpr($1, $2); }

factor:                                             // 基础因子
    IDENT { $$ = new EmbedExpr(NULL, $1); }         // IDENT类型 创建的AST实例类型是EmbedExpr       eg: abc
    | IDENT SEMISEMI IDENT { $$ = new EmbedExpr($1, $3); }
    | '&' IDENT { $$ = new CollapseExpr(NULL, $2); }// &IDENT ...                                   eg: &ref
    | '&' IDENT SEMISEMI IDENT { $$ = new CollapseExpr($2, $4); }   // ?
    | STRING_LITERAL { $$ = new LiteralExpr($1); }
    | '.' { $$ = new DotExpr(); }
    | bracket { $$ = new BracketExpr($1); }         // bracket类型 创建的AST实例类型是BracketExpr   eg: [a-z]
    | '(' union_expr ')' { $$ = $2; }
    | factor '>' action { $$ = $1; $1->entering.push_back($3); }
    | factor '@' action { $$ = $1; $1->finishing.push_back($3); }
    | factor '%' action { $$ = $1; $1->leaving.push_back($3); }
    | factor '$' action { $$ = $1; $1->transiting.push_back($3); }
    | factor '?' { $$ = new MaybeExpr($1); }
    | factor '*' { $$ = new ClosureExpr($1); }
    | factor '+' { $$ = new PlusExpr($1); }         // 

action:
    IDENT { $$ = new RefAction($1); }
    | BRACED_CODE { $$ = new InlineAction($1); }

bracket:                                            // 字符集
    '[' bracket_items ']' {                         // bracket_items { $$ = $1;
        $$ = $2;
        printf("bracket [ ] \n");
    }
    | '[' '^' bracket_items ']' {
        $$ = $3;
        REP(i, $$->size())
            $3->flip(i);
    }

bracket_items:
    | bracket_items CHAR '-' CHAR {                 // 递归 // 前提是必需得有上面的基础作为起点/引子 才能递归
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

