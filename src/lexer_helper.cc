#include "common.hh"

//#define _GNU_SOURCE
#include <stdio.h>

using namespace std;

char *aprintf(const char *fmt, ...)
{
    va_list va;
    char *strp = NULL;
    va_start(va, fmt);
    int len = vasprintf(&strp, fmt, va);
    if (len == -1) {
        strp = NULL;
        fprintf(stderr, "vasprintf: memory allocation failed\n");
        return NULL;
    }
    va_end(va);
    return strp;
}
