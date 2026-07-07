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
    Expr *expr;                                     // yylval可存储 抽象语法树节点
    Stmt *stmt;
    char *errmsg;
}

%destructor {                                       // 析构函数内存管理 当规约失败或出错时，Bison 会自动调用这些析构函数释放内存，防止内存泄漏
    printf("free: %p\n", $$);
    free($$);
} <string>

%destructor { delete $$; } <expr>
%destructor { delete $$; } <stmt>
%destructor { delete $$; } <charset>

                                                    // Token 声明 // 告诉 Bison 哪些 token 有值，以及值的类型
%token INVALID_CHARACTER
%token <integer> CHAR INTEGER
%token <string> IDENT
%token <string> STRING_LITERAL
%token <string> BRACED_CODE
                                                    // 优先级与结合性 从低到高
%left '|'
%left '-'
%left '+' '*' '?'
                                                    // 非终结符类型 // 指定不同产生式返回值的类型
%type <stmt> stmt stmt_list
%type <expr> expr
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
%%
toplevel:
    stmt_list { res = $1; }                         // 整个输入是语句列表
                                                    // stmt_list 的结果赋值给 res // res 是parse函数的 in-out参数
                                                    // 把解析完的所有语句交给外部调用者

stmt_list:                                          // 支持空列表 + 链表式多条语句（双向链表）
    %empty { $$ = new EmptyStmt; }
    | stmt stmt_list {                              // 左递归写法，用于处理零个或多个 stmt // $1是stmt $2是stmt_list $$把第一个stmt作为整个列表的头节点返回 
        $1->next = $2;
        $2->prev = $1;
        $$ = $1;
    }

stmt:                                               // 支持两种语句: 普通赋值：x = expr; 初始化赋值：x := expr;
    IDENT '=' expr ';' { $$ = new AssignStmt($1, $3); } // $$是stmt即赋值语句; $1是IDENT $2是= $3是expr $4是;
    | IDENT ':' '=' expr ';' { $$ = new InstantiationStmt($1, $4); }

expr:
    IDENT { $$ = new EmbedExpr($1); }               // IDENT类型 创建的AST实例类型是EmbedExpr       eg: abc
    | bracket { $$ = new BracketExpr($1); }         // bracket类型 创建的AST实例类型是BracketExpr   eg: [a-z]
    | '&' IDENT { $$ = new CollapseExpr($2); }      // &IDENT ...                                   eg: &ref
    | expr expr { $$ = new ConcatExpr($1, $2); }    // 隐式链接                                     eg: abc [a-z]
    | expr '*' { $$ = new ClosureExpr($1); }        // 零次或多次 eg: [0-9]* abc*
    | expr '+' { $$ = new PlusExpr($1); }           // 
    | expr '-' expr { $$ = new DifferenceExpr($1, $3); }    // 差集
    | expr '|' expr { $$ = new UnionExpr($1, $3); } // 并集

bracket:                                            // 字符集
    '[' bracket_items ']' { $$ = $2; }
    | '[' '^' bracket_items ']' { $$ = $3; }

bracket_items:
    CHAR {
        $$ = new bitset<256>;
        $$->set($1);
    }
    | CHAR '-' CHAR {
        $$ = new bitset<256>;
        FOR(i, $1, $3 + 1) $$->set(i);
    } 
    | bracket_items CHAR '-' CHAR {                 // 递归 // 前提是必需得有上面的基础作为起点/引子 才能递归
        $$ = $1;                                    // 把原来的 bitset 继承下来
        $1 = NULL;                                  // 防止析构函数重复释放
        FOR(i, $2, $4 + 1)                          // $2 是起始字符，$4 是结束字符
            $$->set(i);
    }
    | bracket_items CHAR {
        $$ = $1;
        $1 = NULL;
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
        printf("errors! TODO\n");
    }
    return errors;
}

