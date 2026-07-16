#include "common.hh"

#define _GNU_SOURCE
#include <stdio.h>

using namespace std;

char *aprintf(const char *fmt, ...)
{
    va_list va;
    char *strp = NULL;
    va_start(va, fmt);
    vasprintf(&strp, fmt, va);
    va_end(va);
    return strp;
}
