#include "fsa.hh"
#include "common.hh"

#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include <bitset>
#include <set>
#include <stack>
#include <cstdint>
#include <climits>
#include <iostream>
#include <cassert>

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

void Fsa::check() const {
    REP (i, n()) {
        FOR (j, 1, adj[i].size()) {
            assert((adj[i][j - 1].first.second == 0 &&
                    adj[i][j].first.second == 0) ||
                    adj[i][j - 1].first.second <= adj[i][j].first.first);
        }
    }
}

bool Fsa::is_final(long x) const {
    return std::binary_search(ALL(finals), x);
}

bool Fsa::has(long u, long a) const {
    auto it = std::upper_bound(ALL(adj[u]),
            std::make_pair(
                std::make_pair(a, LONG_MAX),
                LONG_MAX
            )
    );
    return it != adj[u].begin() && a < (--it)->first.second;
}

bool Fsa::has_call(long u) const {
    auto it = std::upper_bound(ALL(adj[u]),
            std::make_pair(
                std::make_pair(call_label_base, LONG_MAX),
                LONG_MAX
            )
    );
    return (it != adj[u].end() && it->first.first < call_label) ||
            (it != adj[u].begin() && call_label_base < (--it)->first.second);
}

bool Fsa::has_call_or_collapse(long u) const {
    auto it = std::upper_bound(ALL(adj[u]),
            std::make_pair(
                std::make_pair(call_label_base, LONG_MAX),
                LONG_MAX
            )
    );
    return it != adj[u].end() ||
            (it != adj[u].begin() && call_label_base < (--it)->first.second);
}

void Fsa::epsilon_closure(std::vector<long> &src) const {
    static std::vector<bool> vis;
    if (n() > (long)vis.size()) {
        vis.resize(n());
    }
    for (long i : src) {
        vis[i] = true;
    }
    REP (i, src.size()) {
        long u = src[i];
        for (auto &e : adj[u]) {
            if (-1 < e.first.first) {
                break;
            }
            if (!vis[e.second]) {
                vis[e.second] = true;
                src.push_back(e.second);
            }
        }
    }
    for (long i : src) {
        vis[i] = false;
    }
    std::sort(ALL(src));
}

Fsa Fsa::operator~() const {
    long accept = n();
    Fsa r;
    r.start = start;
    r.adj.resize(accept + 1);
    REP (i, accept) {
        long j = 0;
        for (auto &e : adj[i]) {
            if (j < e.first.first) {
                r.adj[i].emplace_back(std::make_pair(j, e.first.first), accept);
            }
            r.adj[i].emplace_back(e.first, e.second);
            j = e.first.second;
        }
        if (j < AB) {
            r.adj[i].emplace_back(std::make_pair(j, AB), accept);
        }
    }
    r.adj[accept].emplace_back(std::make_pair(0, AB), accept);
    std::vector<long> new_finals;
    auto j = finals.begin();
    REP (i, accept + 1) {
        while (j != finals.end() && *j < i) {
            ++j;
        }
        if (j == finals.end() || *j != i) {
            new_finals.push_back(i);
        }
    }
    r.finals = std::move(new_finals);
    return r;
}

void Fsa::accessible(const std::vector<long> *starts, std::function<void(long)> relate) {
    std::vector<long> q {start};
    std::vector<long> id(n(), 0);
    id[start] = 1;
    if (starts) {
        for (long u : *starts) {
            if (!id[u]) {
                id[u] = 1;
                q.push_back(u);
            }
        }
    }
    REP (i, q.size()) {
        long u = q[i];
        for (auto &e : adj[u]) {
            if (!id[e.second]) {
                id[e.second] = 1;
                q.push_back(e.second);
            }
        }
    }
    long j = 0;
    REP (i, n()) {
        id[i] = id[i] ? j++ : -1;
    }
    auto it = finals.begin();
    auto it2 = it;
    REP (i, n()) {
        if (id[i] >= 0) {
            relate(i);
            if (start == i) {
                start = id[i];
            }
            while (it != finals.end() && *it < i) {
                ++it;
            }
            if (it != finals.end() && *it == i) {
                *it2++ = id[i];
            }
            long k = 0;
            for (auto &e : adj[i]) {
                if (id[e.second] >= 0) {
                    adj[i][k++] = {
                        e.first,
                        id[e.second]
                    };
                }
            }
            adj[i].resize(k);
            if (id[i] != i) {
                adj[id[i]] = std::move(adj[i]);
            }
        }
    }
    finals.erase(it2, finals.end());
    adj.resize(j);
}

void Fsa::co_accessible(const std::vector<bool> *final, std::function<void(long)> relate) {
    std::vector<std::vector<long>> radj(n());
    REP (i, n()) {
        for (auto &e : adj[i]) {
            radj[e.second].push_back(i);
        }
    }
    REP (i, n()) {
        std::sort(ALL(radj[i]));
    }
    std::vector<long> q = finals;
    std::vector<long> id(n(), 0);
    for (long f : finals) {
        id[f] = 1;
    }
    if (final) {
        REP (i, n()) {
            if ((*final)[i] && !id[i]) {
                id[i] = 1;
                q.push_back(i);
            }
        }
    }
    REP (i, q.size()) {
        long u = q[i];
        for (auto &v : radj[u]) {
            if (!id[v]) {
                id[v] = 1;
                q.push_back(v);
            }
        }
    }
    if (!id[start]) {
        start = 0;
        finals.clear();
        adj.assign(1, {});
        return;
    }
    long j = 0; 
    REP (i, n()) {
        id[i] = id[i] ? j++ : -1;
    }
    auto it = finals.begin();
    auto it2 = it;
    REP (i, n()) {
        if (id[i] >= 0) {
            relate(i);
            if (start == i) {
                start = id[i];
            }
            while (it != finals.end() && *it < i) {
                ++it;
            }
            if (it != finals.end() && *it == i) {
                *it2++ = id[i];
            }
            long k = 0;
            for (auto &e : adj[i]) {
                if (id[e.second] >= 0) {
                    adj[i][k++] = {
                        e.first,
                        id[e.second]
                    };
                }
            }
            adj[i].resize(k);
            if (id[i] != i) {
                adj[id[i]] = std::move(adj[i]);
            }
        }
    }
    finals.erase(it2, finals.end());
    adj.resize(j);
}

Fsa Fsa::intersect(const Fsa &rhs, std::function<void (long, long)> relate) const {
    Fsa r;
    long u0;
    long u1;
    std::unordered_map<long, long> m;
    std::vector<std::pair<long, long>> q;

    q.emplace_back(start, rhs.start);
    m[rhs.n() * start + rhs.start] = 0;
    r.start = 0;
    REP (i, q.size()) {
        std::tie(u0, u1) = q[i];
        if (is_final(u0) && rhs.is_final(u1)) {
            r.finals.push_back(i);
        }
        r.adj.emplace_back();
        relate(u0, u1);
        auto it0 = adj[u0].begin();
        auto it1 = rhs.adj[u1].begin();
        
        while (it0 != adj[u0].end() && it1 != rhs.adj[u1].end()) {
            if (it0->first.second <= it1->first.first) {
                it0++;
            } else if (it1->first.second <= it0->first.first) {
                it1++;
            } else {
                long t = rhs.n() * it0->second + it1->second;
                auto mit = m.find(t);
                if (mit == m.end()) {
                    mit = m.emplace(t, m.size()).first;
                    q.emplace_back(it0->second, it1->second);
                }
                r.adj[i].emplace_back(
                        std::make_pair(
                            std::max(it0->first.first, it1->first.first),
                            std::min(it0->first.second, it1->first.second)
                        ), mit->second);
                if (it0->first.second < it1->first.second) {
                    ++it0;
                } else if (it0->first.second > it1->first.second) {
                    ++it1;
                } else {
                    ++it0;
                    ++it1;
                }
            }
        }
    }
    return r;
}

Fsa Fsa::difference(const Fsa &rhs, std::function<void (long)> relate) const {
    Fsa r;
    long u0;
    long u1;
    std::vector<std::pair<long, long>> q;
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
        relate(u0);
        auto it0 = adj[u0].begin();
        auto it1 = rhs.adj[u1].begin();
        auto it1e = it1;
        if (u1 == rhs.n()) {
            it1 = it1e = rhs.adj[0].end();
        } else {
            it1 = rhs.adj[u1].begin();
            it1e = rhs.adj[u1].end();
        }
        long last = LONG_MIN;
        while (it0 != adj[u0].end()) {
            long from = std::max(last, it0->first.first);
            long to = it0->first.second;
            while (it1 != it1e && it1->first.second <= from) {
                ++it1;
            }
            if (it1 != it1e) {
                to = std::min(to, from < it1->first.first ?
                            it1->first.first : it1->first.second);
            }
            last = to;
            long v1 = it1 != it1e &&
                    it1->first.first <= from ?  it1->second : rhs.n();
            long t = (rhs.n() + 1) * it0->second + v1;
            auto mit = m.find(t);
            if (mit == m.end()) {
                mit = m.emplace(t, m.size()).first;
                q.emplace_back(it0->second, v1);
            }
            r.adj[i].emplace_back(std::make_pair(from, to), mit->second);
            if (to == it0->first.second) {
                ++it0;
            }
        }
    }
    return r;
}

Fsa Fsa::determinize(const std::vector<long> *starts,
        std::function<void (long, const std::vector<long>&)> relate) const {
    Fsa r;
    r.start = 0;
    std::unordered_map<std::vector<long>, long> m;
    std::vector<std::vector<Edge>::const_iterator> its(n());
    std::vector<long> vs {start};
    std::vector<std::pair<long, long>> events;
    epsilon_closure(vs);
    m[vs] = 0;
    std::stack<std::vector<long>> st;
    st.push(std::move(vs));
    if (starts) {
        for (long u : *starts) {
            vs.assign(1, u);
            epsilon_closure(vs);
            if (!m.count(vs)) {
                m.emplace(vs, m.size());
                st.push(std::move(vs));
            }
        }
    }
    while (st.size()) {
        std::vector<long> x = std::move(st.top());
        st.pop();
        long id = m[x];
        if (id + 1 > (long)r.adj.size()) {
            r.adj.resize(id + 1);
        }
        relate(id, x);
        bool final = false;
        events.clear();
        for (long u : x) {
            if (is_final(u)) {
                final = true;
            }
            for (auto &e : adj[u]) {
                events.emplace_back(e.first.first, e.second);
                events.emplace_back(e.first.second, ~ e.second);
            }
        }
        if (final) {
            r.finals.push_back(id);
        }
        long last = 0;
        std::multiset<long> live;
        std::sort(ALL(events));
        for (auto &ev : events) {
            if (last < ev.first) {
                if (live.size()) {
                    vs.assign(ALL(live));
                    vs.erase(std::unique(ALL(vs)), vs.end());
                    epsilon_closure(vs);
                    /*
                    std::cout << "vs: ";
                    for (auto &v : vs) {
                        std::cout << v << ", ";
                    }
                    std::cout << std::endl;
                    */
                    auto mit = m.find(vs);
                    if (mit == m.end()) {
                        mit = m.emplace(vs, m.size()).first;
                        st.push(vs);
                    }
                    if (r.adj[id].size() &&
                            r.adj[id].back().first.second == last &&
                            r.adj[id].back().second == mit->second) {
                        r.adj[id].back().first.second = ev.first;
                    } else {
                        r.adj[id].emplace_back(std::make_pair(last, ev.first), mit->second);
                    }
                    //std::cout << "mit->second: " << mit->second << std::endl;
                }
                last = ev.first;
            }
            if (ev.second >= 0) {
                live.insert(ev.second);
            } else {
                live.erase(live.find(~ ev.second));
            }
        }
    }
    std::sort(ALL(r.finals));
    /*
    for (auto &e : m) {
        std::cout << "status: " << e.second << ", contains: ";
        for (auto &v : e.first) {
            std::cout << v << ", ";
        }
        std::cout << std::endl;
    }
    */
    return r;
}

Fsa Fsa::hopcroft_minimize(std::function<void (std::vector<long>&)> relate) {
    std::vector<long> scale;
    REP (i, n()) {
        for (auto &e : adj[i]) {
            scale.push_back(e.first.first);
            scale.push_back(e.first.second);
        }
    }
    std::sort(ALL(scale));
    scale.erase(std::unique(ALL(scale)), scale.end());
    std::vector<std::vector<std::pair<long, long>>> radj(n());
    REP (i, n()) {
        for (auto &e : adj[i]) {
            long from = std::lower_bound(ALL(scale), e.first.first) - scale.begin();
            long to = std::lower_bound(ALL(scale), e.first.second) - scale.begin();
            FOR (j, from, to) {
                radj[e.second].emplace_back(j, i);    // 2态 : <-1, 5态> <'a', 4态>  既到2态 所有的 需要输入字符以及依赖的状态
            }
        }
    }
    REP (i, n()) {
        std::sort(ALL(radj[i]));
    }

    std::vector<long> L(n());
    std::vector<long> R(n());
    std::vector<long> B(n());
    std::vector<long> C(n(), 0);
    std::vector<long> CC(n(), 0);
    std::vector<bool> mark(n(), false);

    long fx = -1;
    long x = -1;
    long fy = -1;
    long y = -1;
    long j = 0;

    REP (i, n()) {                                      // 遍历n个态
        if (j < (long)finals.size() && finals[j] == i) {      // 终止态
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
    auto labels = [&] (long fx) {
        std::vector<long> lb;
        for (long x = fx;;) {
            for (auto &e : radj[x]) {
                lb.push_back(e.first);
            }
            if ((x = R[x]) == fx) {
                break;
            }
        }
        std::sort(ALL(lb));
        lb.erase(std::unique(ALL(lb)), lb.end());
        return lb;
    };
    if (fx >= 0) {
        for (long a : labels(fx)) {
            refines.emplace(a, fx);
        }
    }
    if (fy >= 0) {
        for (long a : labels(fy)) {
            refines.emplace(a, fy);
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
                long fv = -1;
                long v = -1;
                std::vector<long> lb = labels(fy);
                for (long i = fy;;) {               // ------> 2.2 遍历整个非终止态 eg: 从A态开始遍历
                    if (mark[i]) {                  // ------> 2.2.1 A态是这个a字符所依赖的状态
                        mark[i] = false;
                        if (u < 0) {
                            C[fu = i] = 0;          // ------> 2.2.2 fu指向 首(A)态
                        } else {
                            R[u] = i;
                        }
                        C[fu]++;                    // ------> 2.2.3 C: 
                        B[i] = fu;                  // ------> 2.2.4 B: 各态均指向首个 mark的态
                        L[i] = u;                   // ------> 2.2.5 L: 指向之前的 mark的态
                        u = i;
                    } else {                        // ------> 2.2.1 B态不是...
                        if (v < 0) {
                            C[fv = i] = 0;
                        } else {
                            R[v] = i;
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
                for (long a : lb) {
                    if (refines.count({a, fy})) {
                        refines.emplace(a, fu != fy ? fu : fv);
                    } else {
                        refines.emplace(a, C[fu] < C[fv] ? fu : fv);
                    }
                }
            } else {
                for (long i = fy;;) {
                    mark[i] = false;
                    if ((i = R[i]) == fy) {
                        break;
                    }
                }
            }
            CC[fy] = 0;
        }

        for (x = fx;;) {
            auto it = lower_bound(ALL(radj[x]),
                    std::make_pair(a, 0L));
            auto ite = upper_bound(ALL(radj[x]),
                    std::make_pair(a, n()));
            for (; it != ite; ++it) {
                y = it->second;
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
    std::vector<long> vs;
    REP (i, n()) {
        if (B[i] == i) {
            vs.clear();
            for (long j = i;;) {
                B[j] = nn;
                vs.push_back(j);
                if ((j = R[j]) == i) {
                    break;
                }
            }
            relate(vs);
            if (std::binary_search(ALL(finals), i)) {
                r.finals.push_back(nn);
            }
            nn++;
        }
    }
    r.start = B[start];
    r.adj.resize(nn);
    REP (i, n()) {
        for (auto &e : adj[i]) {
            r.adj[B[i]].emplace_back(e.first, B[e.second]);
        }
    }
    REP (i, nn) {
        std::sort(ALL(r.adj[i]), [] (const Edge &x, const Edge &y) {
            return x.second != y.second ? x.second < y.second : x.first < y.first;
        });
        auto it2 = r.adj[i].begin();
        for (auto it = r.adj[i].begin(); it != r.adj[i].end();) {
            long v = it->second;
            long from = it->first.first;
            long to = it->first.second;
            while (++it != r.adj[i].end() && it->second == v) {
                if (it->first.first <= to) {
                    to = std::max(to, it->first.second);
                } else {
                    *it2++ = std::make_pair(std::make_pair(from, to), v);
                    std::tie(from, to) = it->first;
                }
            }
            *it2++ = std::make_pair(std::make_pair(from, to), v);
        }
        r.adj[i].erase(it2, r.adj[i].end());
        std::sort(ALL(r.adj[i]));
    }
    return r; 
}

// https://oi-wiki.org/misc/fsm/

