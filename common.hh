#pragma once

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif

#include <stdio.h>
#include <cstdarg>
#include <cstddef>

using namespace std;

#define LEN_OF(x) (sizeof(x) / sizeof(*x))
#define REP(i, n) FOR(i, 0, n)
#define FOR(i, s, e) \
    for (decltype(e) i = (s); i < (e); i++)
#define ROF(i, s, e) \
    for (decltype(e) i = (e); --i >= (s);)
