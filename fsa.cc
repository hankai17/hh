#include "fsa.hh"
#include "common.hh"

#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include <set>
#include <cstdint>
#include <climits>

template <typename T>
struct std::hash<std::vector<T>> {
    size_t operator() (const std::vector<T> &v) const {
        hash<T> h;
        size_t r = 0;
        for (auto x : v) {
            r = r * 17 + h(x);
        }
        return r;
    };
};

bool Fsa::is_final(long x) const {
    return std::binary_search(ALL(finals), x);
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
    std::sort(ALL(src));
}

void Fsa::product(const Fsa &rhs,
            std::vector<std::pair<long, long>> &nodes,
            std::vector<std::vector<std::pair<long, long>>> &edges) const {
}

Fsa Fsa::operator~() const {
    Fsa r;
    r.adj.resize(n() + 1);
    r.start = start;
    REP (i, n()) {
        long last = 0;
        for (auto &e : adj[i]) {
            for (; last < e.first; last++) {
                r.adj[i].emplace_back(last, n());
            }
            r.adj[i].emplace_back(e.first, e.second);
        }
    }
    REP (i, 256) {
        r.adj[n()].emplace_back(i, n());
    }
    long j = 0;
    REP (i, n() + 1) {
        if (j < finals.size() && i == finals[j]) {
            j++;
        } else {
            r.finals.push_back(i);
        }
    }
    return r;
}

Fsa Fsa::operator&(const Fsa &rhs) const {
    Fsa r;
    std::vector<std::pair<long, long>> q;
    long u0;
    long u1;
    std::unordered_map<long, long> m;
    q.emplace_back(start, rhs.start);
    m[(rhs.n() + 1) * start + rhs.start] = 0;
    r.start = 0;
    REP (i, q.size()) {
        std::tie(u0, u1) = q[i];
        if (is_final(u0) && rhs.is_final(u1)) {
            r.finals.push_back(i);
        }
        r.adj.emplace_back();
        auto it0 = adj[u0].begin();
        auto it1 = rhs.adj[u1].begin();
        
        while (it0 != adj[u0].end() && it1 != rhs.adj[u1].end()) {
            if (it0->first < it1->first) {
                it0++;
            } else if (it0->first > it1->first) {
                it1++;
            } else {
                long t = rhs.n() * it0->second + it1->second;
                auto mit = m.find(t);
                if (mit == m.end()) {
                    mit = m.emplace(t, m.size()).first;
                    q.emplace_back(it0->second, it1->second);
                }
                r.adj[i].emplace_back(it0->first, mit->second);
                it0++;
                it1++;
            }
        }
    }
    return r;
}

Fsa Fsa::operator|(const Fsa &rhs) const {
    Fsa r;
    r.start = n() + rhs.n(); 
    r.finals = finals;
    for (long i : rhs.finals) {
        r.finals.push_back(n() + i);
    }
    r.adj = adj;
    r.adj.resize(r.start + 1);
    r.adj[r.start].emplace_back(-1, 0);
    r.adj[r.start].emplace_back(-1, n());
    REP (i, rhs.n()) {
        for (auto &e : rhs.adj[i]) {
            r.adj[n() + i].emplace_back(e.first, n() + e.second);
        }
    }
    return r.determinize().hopcroft_minimize();
}

Fsa Fsa::operator-(const Fsa &rhs) const {
    Fsa r;
    std::vector<std::pair<long, long>> q;
    long u0;
    long u1;
    std::unordered_map<long, long> m;
    q.emplace_back(start, rhs.start);
    m[(rhs.n() + 1) * start + rhs.start] = 0;
    r.start = 0;
    REP (i, q.size()) {
        std::tie(u0, u1) = q[i];
        if (is_final(u0) && !rhs.is_final(u1)) {
            r.finals.push_back(i);
        }
        r.adj.emplace_back();
        auto it0 = adj[u0].begin();
        auto it1 = rhs.adj[u1].begin();
        auto it1e = it1;
        if (u1 == rhs.n()) {
            it1 = it1e = rhs.adj[0].end();
        } else {
            it1 = rhs.adj[u1].begin();
            it1e = rhs.adj[u1].end();
        }
        for (; it0 != adj[u0].end(); ++it0) {
            while (it1 != it1e && it1->first < it0->first) {
                ++it1;
            }
            long v1 = it1 != it1e || it1->first == it1e->first ?
                    it1->second : rhs.n();
            long t = (rhs.n() + 1) * it0->second + v1;
            auto mit = m.find(t);
            if (mit == m.end()) {
                mit = m.emplace(t, m.size()).first;
                q.emplace_back(it0->second, it1->second);
            }
            r.adj[i].emplace_back(it0->first, mit->second);
        }
    }
    return r;
}

Fsa Fsa::determinize() const {
    Fsa r;
    std::unordered_map<std::vector<long>, long> m;
    std::vector<std::vector<long>> q{{start}};
    std::vector<std::vector<std::pair<long, long>>::const_iterator> its(n());
    std::vector<long> vs;
    epsilon_closure(q[0]);
    m[q[0]] = 0;
    r.start = 0;
    REP (i, q.size()) {
        bool final = false;
        for (long u : q[i]) {
            if (std::binary_search(ALL(finals), u)) {
                final = true;
            }
            its[u] = adj[u].begin();
            while (its[u] != adj[u].end() && its[u]->first < 0) {
                ++its[u];
            }
        }
        if (final) {
            r.finals.push_back(i);
        }
        r.adj.emplace_back();
        for (;;) {
            long c = LONG_MAX;
            for (long u : q[i]) {
                if (its[u] != adj[u].end()) {
                    c = std::min(c, its[u]->first);
                }
            }
            if (c == LONG_MAX) {
                break;
            }
            vs.clear();
            for (long u : q[i]) {
                for (; its[u] != adj[u].end() && its[u]->first == c; ++its[u]) {
                    vs.push_back(its[u]->second);
                }
            }
            std::sort(ALL(vs));
            vs.erase(std::unique(ALL(vs)), vs.end());
            epsilon_closure(vs);
            auto mit = m.find(vs);
            if (mit == m.end()) {
                mit = m.emplace(vs, m.size()).first;
                q.push_back(vs);
            }
            r.adj[i].emplace_back(c, mit->second);
        }
    }
    return r;
}

Fsa Fsa::hopcroft_minimize() {
    std::vector<std::vector<std::pair<long, long>>> radj(n());
    REP (i, n()) {
        for (auto &e : adj[i]) {
            radj[e.second].emplace_back(e.first, i);    // 2态 : <-1, 5态> <'a', 4态>  既到2态 所有的 需要输入字符以及依赖的状态
        }
    }
    REP (i, n()) {
        std::sort(ALL(radj[i]));
    }

    std::vector<long> L(n());
    std::vector<long> R(n());
    std::vector<long> B(n());
    std::vector<long> C(n(), 0);
    std::vector<long> CC(n());
    std::vector<bool> mark(n(), false);

    long fx = -1;
    long x = -1;
    long fy = -1;
    long y = -1;
    long j = 0;

    REP (i, n()) {                                      // 遍历n个态
        if (j < finals.size() && finals[j] == i) {      // 终止态
            j++;
            if (y < 0) {                                // fy指向首个 终止态
                fy = i;
            } else {
                R[y] = i;                               // 当前态挂到R链表 // R: 下标跟值均为 终止态 // 下标是上一个终止态 值为下一个终止态
            }
            C[B[i] = fy]++;                             // B: 各态均指向 首个终止|非终止态 // 各个态表态站队 站队头 // 状态 i 所属块的代表元
                                                        // C: 记录站队 长度
            L[i] = y;                                   // L: 下标为当前终止态 值为上一个终止态
            y = i;
        } else {
            if (x < 0) {                                // fx指向首个 非终止态
                fx = i;
            } else {
                R[x] = i;
            }
            C[B[i] = fx]++;
            L[i] = x;
            x = i;
        }
    }

    if (x >= 0) {
        L[fx] = x;                                      // 非终态环形双向链表1
        R[x] = fx;
    }

    if (y >= 0) {
        L[fy] = y;                                      //   终态环形双向链表2
        R[y] = fy;
    }

    std::set<std::pair<long, long>> refines;

    if (x >= 0 || y >= 0) {
        REP (a, 256) {
            refines.emplace(a, fy < 0 || fx >= 0 && C[fx] < C[fy] ? fx : fy);
        }
    }
                                                    // ------> 0 eg: 所有字符[a-zA-Z]均被分到fx中了 
    while (refines.size()) {                        // 拆分每个字符绑定的fx/y块
        long a;
        std::tie(a, fx) = *refines.begin();
        refines.erase(refines.begin());
                                                    // ------> 1 遍历所有态 统计依赖该字符(eg: 'a')的边的个数
        std::vector<long> bs;
        for (x = fx;;) {                            // eg: radj[5] = [ { 'a', 2 }, { 'a', 7 }, { 'a', 12 }, { 'b', 3 }, { 'c', 8 } ]
            auto it = lower_bound(ALL(radj[x]),     // 在 radj[x] 中找到第一个满足 转移字符 >= a 且 来源状态 >= 0 的元素
                    std::make_pair(a, 0L));
            auto ite = upper_bound(ALL(radj[x]),    // 在 radj[x] 中找到第一个满足 转移字符 > a 或 来源状态 > n() 的元素
                    std::make_pair(a, n()));
            for (; it != ite; ++it) {               // 该区间 正好包含所有转移字符等于 a 的边
                y = it->second;
                if (!CC[B[y]]++) {                  // ------> 1.1 eg: 字符a状态2的B 存储的是 首个非|终止态
                    bs.push_back(B[y]);             //              CC记录命中 首个非|终止态 的总次数
                }
                mark[y] = true;
            }
            if ((x = R[x]) == fx) {
                break;
            }
        }

        for (long fy : bs) {                        // ------> 2 eg: 'a'所依赖的状态2 在非终止态 fy既是非终止态的头
            if (CC[fy] < C[fy]) {                   // ------> 2.1 分区中一部分状态被 a 转移走了 一部分没有 所以需要分裂
                long fu = -1;
                long u = -1;
                long cu = 0;
                long fv = -1;
                long v = -1;
                long cv = 0;

                for (long i = fy;;) {               // ------> 2.2 遍历整个非终止态 eg: 从A态开始遍历
                    if (mark[i]) {                  // ------> 2.2.1 A态是这个a字符所依赖的状态
                        if (u < 0) {
                            C[fu = i] = 0;          // ------> 2.2.2 fu指向 首(A)态
                        } else {
                            R[fu] = i;
                        }
                        C[fu]++;                    // ------> 2.2.3 C: 
                        B[i] = fu;                  // ------> 2.2.4 B: 各态均指向首个 mark的态
                        L[i] = u;                   // ------> 2.2.5 L: 指向之前的 mark的态
                        u = i;
                    } else {                        // ------> 2.2.1 B态不是...
                        if (v < 0) {
                            C[fv = i] = 0;
                        } else {
                            R[fv] = i;
                        }
                        C[fv]++;
                        B[i] = fv;
                        L[i] = v;
                        v = i;
                    }
                    if ((i = R[i]) == fy) {
                        break;
                    }
                }
                L[fu] = u;                          // ------> 3 大概意思是 把依赖'a'的状态串起来 不依赖'a'的状态也串起来 形成两个链表
                R[u] = fu;
                L[fv] = v;
                R[v] = fv;
                REP (a, 256) {
                    if (refines.count({a, fy})) {   // ------> 4 分区: 当你分裂一个分区后，这个变化会影响其他字符的判断 所以每个字符都要重新分配分区
                        refines.emplace(a, fu != fy ? fu : fv);
                    } else {
                        refines.emplace(a, C[fu] < C[fv] ? fu : fv);
                    }
                }
            }
        }

        for (x = fx;;) {
            auto it = lower_bound(ALL(radj[x]),
                    std::make_pair(a, 0L));
            auto ite = upper_bound(ALL(radj[x]),
                    std::make_pair(a, n()));
            for (; it != ite; ++it) {
                fy = B[it->second];
                CC[B[y]] = 0;
                mark[y] = false;
            }
            if ((x = R[x]) == fx) {
                break;
            }
        }
    }
    Fsa r;
    long nn = 0;
    REP (i, n()) {
        if (B[i] == i) {
            for (long j = i;;) {
                B[j] = nn;
                if ((j = R[j]) == i) {
                    break;
                }
            }
            if (std::binary_search(ALL(finals), i)) {
                r.finals.push_back(nn);
            }
            nn++;
        }
    }
    r.start = B[start];
    r.adj.resize(nn);
    REP (i, n()) {
        std::sort(ALL(r.adj[i]));
        r.adj[i].erase(std::unique(ALL(r.adj[i])), r.adj[i].end());
    }
    return r; 
}

