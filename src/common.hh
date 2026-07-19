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

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

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

template<class T>
void sorted_insert(std::vector<T>& a, const T& x)
{
    a.emplace_back();
    auto it = a.end();
    while (a.begin() != --it && x < it[-1]) {
        *it = it[-1];
    }
    *it = x;
}

