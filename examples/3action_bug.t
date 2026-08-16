
//import "sql_lexer.t"
//import "sql_parser.t"

//NUMERIC_LITERAL:
//    (DIGIT+ ('_' DIGIT+)* ('.' (DIGIT+ ('_' DIGIT+)*)?)? | '.' DIGIT+ ('_' DIGIT+)*) (
//        'E' [-+]? DIGIT+ ('_' DIGIT+)*
//    )?
//    | '0x' HEX_DIGIT+ ('_' HEX_DIGIT+)*

NUMERIC_LITERAL: [0-9]+

//sqli_ws = [ \t\r\n]+
//ws = [ \t\r\n]+

action foo {
    puts("foo");
}

//export bar = 'a' @{puts("finishing");}
//export bar = 'a' sqli_ws OR_ sqli_ws 'C' @{puts("finishing");}
//export bar = 'a' sqli_ws OR_ sqli_ws 'C' @foo
//export bar = expr_comparison sqli_ws OR_ sqli_ws 'C' @foo
//export bar = 'a' sqli_ws OR_ sqli_ws expr_comparison  @foo        // 没问题

// bug开始出现
//export bar = expr_comparison sqli_ws OR_ sqli_ws expr_comparison  %foo
//export bar = (expr_comparison sqli_ws OR_ sqli_ws expr_comparison)  @foo
//export bar = expr_comparison @foo


//expr_unary = (MINUS | PLUS | TILDE)* opt_ws NUMERIC_LITERAL
//export bar = expr_unary @foo

//export expr_unary = (MINUS | PLUS | TILDE)* opt_ws NUMERIC_LITERAL @foo
//export expr_unary = opt_ws NUMERIC_LITERAL @foo

//export expr_unary = NUMERIC_LITERAL @foo
//export expr_unary = NUMERIC_LITERAL %foo




//export expr_unary = NUMERIC_LITERAL >{ puts("entering\n"); }  // bad
//export expr_unary = 'a' NUMERIC_LITERAL >{ puts("entering\n"); }  // ok

//export expr_unary = NUMERIC_LITERAL @{ puts("finish\n"); }  // ok
//export expr_unary = NUMERIC_LITERAL %{ puts("leaving\n"); }     // bad

//digit = [0-9]
//export number = digit+ 'a' >{ start = p; } ${ printf("char=%c\n", *p); } %{ end = p; };

action foo1 {
    puts("foo1\n");
}
action foo2 {
    puts("foo2\n");
}
action foo3 {
    puts("foo3\n");
}
action foo4 {
    puts("foo4\n");
}

//export bar = 'a' >foo1 @foo2 $foo3 %foo4 >{puts("unnamed");}
//export bar = 'abcdefg' @foo1  // ok
//export bar = 'abcdefg' $foo1  // ok
//export bar = 'abcdefg' >foo1  // bad 需要在withins中对比差集


//export bar = "123456" @foo1
//export bar = 123456 @foo1
export bar = [0-9]+ @foo1
//export bar = [0-9]+ %foo1
//export bar = [123456] @foo1
