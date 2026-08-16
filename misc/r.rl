#include <stdio.h>
#include <string.h>

%%{
    machine demo_machine;

    action foo1 { puts("foo1; start"); }
    action foo2 { puts("foo2: transit action"); }
    action foo3 { puts("foo3: transit"); }
    action foo4 { puts("foo4: leaving"); }

    main := [0-9]+ >foo1 @foo2 $foo3 %foo4;
}%%

%% write data;

int main(void)
{
    int cs;
    const char *p, *pe, *eof;          // 添加 eof 声明
    char input[] = "123";

    p = input;
    pe = input + strlen(input);        // pe 指向末尾（不含 '\0'）
    eof = pe;                          // eof 与 pe 相同，表示扫描结束位置

    %% write init;
    %% write exec;

    printf("done, cs=%d\n", cs);
    return 0;
}

// ragel -C r.rl -o demo.c
