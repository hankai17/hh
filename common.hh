#pragma once

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif

#include <stdio.h>
#include <cstdarg>
#include <cstddef>

using namespace std;

#define LEN_OF(x) (sizeof(x) / sizeof(*x))
#define ALL(x) (x).begin(), (x).end()
#define REP(i, n) FOR(i, 0, n)
#define FOR(i, s, e) \
    for (decltype(e) i = (s); i < (e); i++)
#define ROF(i, s, e) \
    for (decltype(e) i = (e); --i >= (s);)

const size_t BUF_SIZE = 512;

void output_error(bool use_err, const char *format, va_list ap);
void err_msg(const char *format, ...);
void err_exit(int exitno, const char *format, ...);
long get_long(const char *arg);
