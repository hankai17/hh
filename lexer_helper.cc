#include "common.hh"

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
