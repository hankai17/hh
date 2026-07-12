#include "fsa.hh"
#include "common.hh"

#include <unordered_set>

bool Fsa::is_final(long x) const {
    return lower_bound(ALL(finals), x) != finals.end();
}

void Fsa::epsilon_closure(std::vector<long> &src) const {   // 计算ε-闭包 // 从一组状态出发，通过任意多次 ε 转移（不消耗输入字符的转移）所能到达的所有状态集合
    std::unordered_set<long> visit { ALL(src) };
    for (long i = 0; i < src.size(); i++) {
        long u = src[i];                                    // 取出要处理的状态
        for (auto &e : adj[u]) {                            // 遍历 要处理状态的所有ε出边 eg: vector1<ε, 状态1>, vector2<ε, 状态2>, vector3<'A', 状态3>, vector4<'B', 状态4> // 只会处理v1, v2 把状态1/2 放入src
            if (-1 < e.first) {
                break;
            }
            if (!visit.count(e.second)) {
                visit.insert(e.second);
                src.push_back(e.second);
            }
        }
    }
}

void Fsa::product(const Fsa &rhs,
            std::vector<std::pair<long, long>> &nodes,
            std::vector<std::vector<std::pair<long, long>>> &edges) const {
    long u0, u1;
    std::unordered_map<long, long> m;

    nodes.emplace_back(start, rhs.start);                   // 1 创建起始状态对 ---> 新状态0
    m[rhs.n() * start + rhs.start] = 0;

    REP (i, nodes.size()) {                                 // 2 遍历已知状态对
        std::tie(u0, u1) = nodes[i];

        auto it0 = adj[u0].begin();
        auto it1 = rhs.adj[u1].begin();

        while (it0 != adj[u0].end() && it1 != rhs.adj[u1].end()) {
            if (it0->first < it1->first) {                  // 左边字符小
                it0++;
            } else if (it0->first > it1->first) {
                it1++;
            } else {
                long t = rhs.n() * it0->second + it1->second;
                if (!m.count(t)) {
                    long id = m.size();
                    m[t] = id;
                    nodes.emplace_back(it0->second, it1->second);
                }
                edges[i].emplace_back(it0->first, m[t]);
                ++it0;
                ++it1;
            }
        }
    }
}

Fsa Fsa::operator~() const {

}

Fsa Fsa::operator&(const Fsa &rhs) const {
    Fsa r;
    r.start = 0;
    std::vector<std::pair<long, long>> nodes;
    product(rhs, nodes, r.adj);
    REP (i, nodes.size()) {
        if (is_final(nodes[i].first) &&
                rhs.is_final(nodes[i].second)) {
            r.finals.push_back(i);
        }
    }
    return r;
}

Fsa Fsa::operator|(const Fsa &rhs) const {
    Fsa r;
    r.start = 0;
    std::vector<std::pair<long, long>> nodes;
    product(rhs, nodes, r.adj);
    REP (i, nodes.size()) {
        if (is_final(nodes[i].first) &&
                rhs.is_final(nodes[i].second)) {
            r.finals.push_back(i);
        }
    }
    return r;

}

Fsa Fsa::operator-(const Fsa &rhs) const {

}

void Fsa::hopcroft_minimize() {

}

Fsa Fsa::determinize() const {

}

