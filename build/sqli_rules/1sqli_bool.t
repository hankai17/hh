
import "sql_lexer.t"
import "sql_parser.t"

sqli_ws = [ \t\r\n]+
sqli_opt_ws = [ \t\r\n]*

////////////////////////////////////////////////////////////////////////////////
// 1. Boolean-based SQL Injection
//
// Examples:
//
//   1 OR 1=1
//   1 AND 1=1
//   'a'='a' OR 'b'='b'
//   1=1 AND 2=2
//  ./a.out "1 OR (2 = 2 AND (3 = 3 OR 4 = 4))"
// 注意：这里只检测“SQL expression + boolean operator + SQL expression”
// 不直接判断真假，避免把完整 SQL expression evaluator 做进 grammar。
////////////////////////////////////////////////////////////////////////////////

//action log_attack {
//    printf("action hit: Boolean-based SQL Injection\n");
//    return 10000 + 1;
//}

//export sqli_boolean = (expr_comparison sqli_ws OR_ sqli_ws expr_comparison | expr_comparison sqli_ws AND_ sqli_ws expr_comparison) @log_attack

//export sqli_boolean_attack = sqli_boolean @log_attack

//export sqli_boolean = (expr_comparison sqli_ws OR_ sqli_ws expr_comparison) @log_attack


//export sqli_boolean = 'a' sqli_ws OR_ sqli_ws 'C'

//export sqli_boolean = expr_comparison sqli_ws OR_ sqli_ws 'C'

//export sqli_boolean = expr_comparison

//export sqli_boolean = (expr_comparison sqli_ws OR_ sqli_ws expr_comparison)

//export sqli_boolean = expr_comparison sqli_ws OR_ sqli_ws expr_comparison >{printf("action hit: entering!\n");}  @{printf("action hit: finished!\n");}
        //%{printf("action hit: leaveing!\n");} \
        //${printf("action hit: transing!\n");} 

//export sqli_boolean =
//      (expr_comparison sqli_ws OR_ sqli_ws expr_comparison
//    | expr_comparison sqli_ws AND_ sqli_ws expr_comparison) %log_attack

export sqli_boolean =
      expr_comparison sqli_ws OR_ sqli_ws expr_comparison
    | expr_comparison sqli_ws AND_ sqli_ws expr_comparison
