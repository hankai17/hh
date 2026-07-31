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
#include <map>

using ::va_list;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

const size_t BUF_SIZE = 512;
const long MAX_CODEPOINT = 0x10ffff;
extern long action_label;
extern long action_label_base;
extern long call_label;
extern long call_label_base;
extern long collapse_label;
extern long collapse_label_base;

enum class Mode {
    cxx,
    graphviz,
    interactive
};
extern Mode opt_mode;
extern long AB;

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
void ident(FILE *f, int d);

template<class T, class ...Args>
void emplace_front(std::vector<T> &a, Args &&...args) {
    a.emplace(a.begin(), args...);
}

template<class T, class ...Args>
void sorted_emplace(std::vector<T> &a, Args &&...args) {
    T x {args...};
    a.emplace_back();
    auto it = a.end();
    while (a.begin() != --it && x < it[-1]) {
        *it = it[-1];
    }
    *it = x;
}

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

struct DisjointIntervals {
    typedef std::pair<long, long> value_type;
    std::map<long, long> to;

    template<class ...Args>
    void emplace(Args &&...args) {
        value_type x {args...};
        auto it = to.lower_bound(x.first);
        if (it != to.begin() && x.first <= prev(it)->second) {
            x.first = (--it)->first;
        }
        auto it2 = to.upper_bound(x.second);
        if (it2 != to.begin() &&
                prev(it2)->first <= x.second &&
                x.second < prev(it2)->second) {
            x.second = prev(it2)->second;
        }
        while (it != it2) {
            it = to.erase(it);
        }
        to.emplace(x);
    }
    void flip() {
        long i = 0;
        std::map<long, long> to2;
        for (auto &x : to) {
            if (i < x.first) {
                to2.emplace(i, x.first);
            }
            i = x.second;
        }
        if (i < AB) {
            to2.emplace(i, AB);
        }
        to = std::move(to2);
    }
    void print() {
        for (auto &x : to) {
            printf("(%ld,%ld) ", x.first, x.second);
        }
        puts("");
    }
};

