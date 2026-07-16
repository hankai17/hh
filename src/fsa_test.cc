#include "fsa.hh"
#include <algorithm>
#include <iostream>

#define ALL(x) (x).begin(), (x).end()
#define REP(i, n) FOR(i, 0, n)
#define FOR(i, s, e) \
    for (decltype(e) i = (s); i < (e); i++)
#define ROF(i, s, e) \
    for (decltype(e) i = (e); --i >= (s);)

int main() {
    long n, m, k, u, v;
    std::cin >> n >> m >> k;                    // n个状态    k个终态
    
    Fsa r;
    r.start = 0;
    r.adj.resize(n);
    while (k--) {
        std::cin >> u;
        r.finals.push_back(u);
    }

    std::sort(ALL(r.finals));
    while (m--) {
        char a;
        std::cin >> u >> a >> v;
        if (u < 0 || u >= n || v < 0 || v >= n) {
            return 1;
        }
        r.adj[u].emplace_back(a, v);
    }

    REP (i, n) {
        std::sort(ALL(r.adj[i]));
        if (r.adj[i].size()) {
            REP(j, r.adj[i].size() - 1) {
                if (r.adj[i][j].first == r.adj[i][j + 1].first) {
                    return 1;
                }
            }
        }
    }

    if (std::cin.bad()) {
        return 2;
    }
    r.hopcroft_minimize();
    return 0;
}

