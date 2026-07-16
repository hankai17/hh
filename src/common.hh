#pragma once

#undef va_list
#undef __gnuc_va_list
#undef __va_list
#undef _VA_LIST
#undef _VA_LIST_DEFINED
#undef __VA_LIST__

#include <cstddef>
#include <cstdint>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>

using ::va_list;

const size_t BUF_SIZE = 512;

#define LEN_OF(x) (sizeof(x) / sizeof(*x))
#define ALL(x) (x).begin(), (x).end()
#define REP(i, n) FOR(i, 0, n)
#define FOR(i, s, e) \
    for (decltype(e) i = (s); i < (e); i++)
#define ROF(i, s, e) \
    for (decltype(e) i = (e); --i >= (s);)

void output_error(bool use_err, const char *format, va_list ap);
void err_msg(const char *format, ...);
void err_exit(int exitno, const char *format, ...);
long get_long(const char *arg);
