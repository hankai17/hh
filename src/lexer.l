%{
#include "parser.hh"
#include "syntax.hh"
#include "lexer_helper.hh"

#include <climits>
#include <cstdlib>
#include <cstring>
#include <string>

using namespace std;
                                                    // 1.C代码段
                                                    // lexer: 词法分析器 也叫 扫描器
                                                    // 自定义领域语言: DSL(领域特定语言)

#define YY_USER_ACTION                              \
    do {                                            \
        yylloc->start   = yyget_extra(yyscanner);   \
        yylloc->end     = yylloc->start + yyleng;   \
        yyset_extra(yylloc->end, yyscanner);         \
    } while (0);

static int my_top_state(yyscan_t yyscanner);
static string tmp_bracket;
static string tmp_str;

static void unexpected_eof(YYSTYPE *yylval, const char* token_end) {
    yylval->errmsg = aprintf("Missing %s at end of file", token_end);
}

static void unexpected_newline(YYSTYPE *yylval, const char* token_end) {
    yylval->errmsg = aprintf("Missing %s at end of file", token_end);
}
%}
                                                    // 2.Flex(lexer)生成器的行为控制
%option noyywrap nounput noinput
                                                    // 支持可重入
%option reentrant
%option extra-type="long"
%option bison-bridge bison-locations
                                                    // 生成的函数名前缀
%option prefix="raw_yy"
                                                    // 支持状态栈 push/pop
%option stack

                                                    // 3.状态声明: 多条件扫描状态
                                                    // 单行注释 #
%x IN_LINE_COMMENT
                                                    // {
%x IN_BRACE
                                                    // [
%x IN_BRACKET
%x IN_BRACKET_FIRST
%x IN_Q_STRING
%x IN_QQ_STRING
                                                    // 4.正则宏定义
D   [0-9]
L   [a-zA-Z_]
                                                    // 16进制
H   [a-fA-F0-9]
                                                    // 浮点数指数部分
E   [Ee][+-]?{D}+
                                                    // 浮点后缀
FS  (f|F|l|L)
IS  (u|U|l|L)*
                                                    // 5.词法匹配规则: 核心规则/核心词法扫描逻辑: 规则 { 动作 }
                                                    // 优先级依次降低
                                                    // 单字符运算符 这些符号直接返回自身作为 token（例如 + 就返回 +）
                                                    // Flex 从文件/缓冲区读取原始输入字符，匹配正则成功后，把本次匹配到的一串字符存在内部缓冲区，用 raw_yyget_text(yyscanner) 拿到指针，宏简化成 yytext。
                                                    //  yytext 是当前刚匹配到的一段输入文本
                                                    //  yytext 指向 flex 内部一块复用缓冲区，每次匹配新 token 都会覆盖原有内容，不能长期保存
                                                    //  所以如果要长期存字符 / 字符串，必须拷贝到 yylval 里（atol /strdup）

                                                    //  raw_yyparse                             ---> bison语法分析器    // https://www.doubao.com/chat/38434024556114946
                                                    //      raw_yylex(*yylval/*in-out*/)        ---> 执行下面的每条lex规则 并传出yylval(即token)
                                                    //

                                                    // Flex 和 Bison 需要“交接数据 要把匹配上的数据存放到 yylval 里
                                                    // flex 需要识别出 token 并返给bison 具体的数据类型
                                                    // Flex 从左到右扫描输入，每次尝试(匹配所有规则)找到当前能匹配的最长 token，并且优先选择最先定义的规则。匹配成功后，才把这个 token 返回给 Bison。

%%
"::" return SEMISEMI;
"."|"!"|"?"|"="|";"|","|":"|"|"|"+"|"-"|"*"|"/"|"%"|"\$"|"<"|">"|"&" { return yytext[0]; }
"("|")" { return yytext[0]; }

"action" return ACTION;
"as" return AS;
"export" return EXPORT;
"import" return IMPORT;

{L}({L}|{D})* {                                     // 匹配abc123 存到string里并返回IDENT
    //printf("DEBUG: IDENT recognized: '%s'\n", yytext);
    yylval->str = new std::string(yytext);
    return IDENT;
}


[0-9]+ { yylval->integer = atol(yytext); return INTEGER; }

"#" yy_push_state(IN_LINE_COMMENT, yyscanner);
<IN_LINE_COMMENT>{
    "\n" yy_pop_state(yyscanner);                   // 换行结束注释
    <<EOF>> yy_pop_state(yyscanner);
    . {}                                            // 其他字符忽略 不生成token
}


"[" yy_push_state(IN_BRACKET_FIRST, yyscanner); return '[';
<IN_BRACKET_FIRST>{                                 // 处理 [ 后面的第一个字符
    "^" {
        yy_pop_state(yyscanner);
        yy_push_state(IN_BRACKET, yyscanner);
        return '^';                                 // 返回 ^ 作为取反标记
    }
    [^-\\\]\n] {
        yy_pop_state(yyscanner);
        yy_push_state(IN_BRACKET, yyscanner);
        yylval->integer = yytext[0];
        return CHAR;                                // 普通字符
    }
}
<IN_BRACKET>{
    "]" {
        yy_pop_state(yyscanner);
        if (my_top_state(yyscanner) == INITIAL) {
            return ']';
        }
    }
    [^-\\\]\n] {                                    // 匹配除 ]、换行以外所有字符
        yylval->integer = yytext[0];
        return CHAR;
    }
}
<IN_BRACKET_FIRST,IN_BRACKET>{                     // 多状态逗号分隔写法：规则同时对 IN_BRACKET_FIRST、IN_BRACKET 生效
    \\. yylval->integer = yytext[1]; return CHAR;
    - return '-';
    <<EOF>> unexpected_eof(yylval, "]"); return INVALID_CHARACTER;
    "\n" unexpected_newline(yylval, "]"); return INVALID_CHARACTER;
}

"{" tmp_bracket.clear(); yy_push_state(IN_BRACE, yyscanner);
<IN_BRACE>{
    "'" { tmp_bracket += '\''; yy_push_state(IN_Q_STRING, yyscanner); }
    "\"" { tmp_bracket += '"'; yy_push_state(IN_QQ_STRING, yyscanner); }
    "[" { tmp_bracket += '['; yy_push_state(IN_BRACKET, yyscanner); }
    "{" { tmp_bracket += '{'; yy_push_state(IN_BRACE, yyscanner); }
    "}" {
        yy_pop_state(yyscanner);
        if (my_top_state(yyscanner) == INITIAL) {
            yylval->str = new std::string(tmp_bracket);
            return BRACED_CODE;
        }
    }
    "\n" unexpected_newline(yylval, "}"); return INVALID_CHARACTER;
    . tmp_bracket += yytext[0];
    <<EOF>> unexpected_eof(yylval, "}"); return INVALID_CHARACTER;
}

"'" tmp_str.clear(); yy_push_state(IN_Q_STRING, yyscanner);
<IN_Q_STRING>{
    "'" {
        yy_pop_state(yyscanner);
        if (my_top_state(yyscanner) == INITIAL) {
            yylval->str = new std::string(tmp_str);
            return STRING_LITERAL;
        }
    }
    \\' tmp_str += '\'';
    \\n unexpected_newline(yylval, "'");
    . tmp_str += yytext[0];
    <<EOF>> unexpected_eof(yylval, "'");
}


"\"" tmp_str.clear(); yy_push_state(IN_QQ_STRING, yyscanner);
<IN_QQ_STRING>{
    "\"" {
        yy_pop_state(yyscanner);
        if (my_top_state(yyscanner) == INITIAL) {
            yylval->str = new std::string(tmp_str);
            return STRING_LITERAL;
        }
    }
    <<EOF>> unexpected_eof(yylval, "\"");
    "\n" unexpected_newline(yylval, "\"");
}

<IN_Q_STRING,IN_QQ_STRING>{
    \\[0-7]{1,3} {                                  // 8进制
        long c = strtol(yytext + 1, NULL, 8);
        if (UCHAR_MAX < c) {
            yylval->errmsg = aprintf("Invalid number after \\-escape: %s", yytext + 1);
            return INVALID_CHARACTER;
        }
        tmp_str.push_back(c);
    }
    \\x[0-9a-fA-F]+ {                               // 16进制
        long c = strtol(yytext + 2, NULL, 16);
        if (UCHAR_MAX < c) {
            yylval->errmsg = aprintf("Invalid number after \\-escape: %s", yytext + 1);
            return INVALID_CHARACTER;
        }
        tmp_str.push_back(c);
    }
    \\a tmp_str += '\a';
    \\b tmp_str += '\b';
    \\f tmp_str += '\f';
    \\n tmp_str += '\n';
    \\r tmp_str += '\r';
    \\t tmp_str += '\t';
    \\v tmp_str += '\v';
    \\[\"\'?\\] tmp_str += yytext[1];
    \\. {
        yylval->errmsg = aprintf("Invalied \\-escape: %s", yytext);
        return INVALID_CHARACTER;
    }
    . tmp_str += yytext[0];
    <<EOF>> unexpected_eof(yylval, "\""); return INVALID_CHARACTER;
    "\n" unexpected_newline(yylval, "\""); return INVALID_CHARACTER;
}

"\n" return '\n';
[ \t]+ {}                                         // 空字符 空动作 直接丢弃 不生成任何 token
. {
    yylval->errmsg = strdup("Invalid character");
    return INVALID_CHARACTER;
}

%%

static int my_top_state(yyscan_t yyscanner) {
    struct yyguts_t *yyg = (struct yyguts_t *)yyscanner;
    return yyg->yy_start_stack_ptr ?
        yyg->yy_start_stack[yyg->yy_start_stack_ptr - 1] : INITIAL;
}
