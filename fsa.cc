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
    std::vector<bool> mark(n());

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
            C[B[i] = fy]++;                             // B: 存储各态 首个终止、非终止态 // 各个态表态站队 站队头 // 状态 i 所属块的代表元
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

    if (x >= 0 && y >= 0) {
        L[fx] = x;                                      // 环形双向链表1
        R[x] = fx;
        L[fy] = y;                                      // 环形双向链表2
        R[y] = fy;
        
        std::set<std::pair<long, long>> refines;
        REP (a, 256) {
            refines.emplace(a, C[fx] < C[fy] ? fx : fy);

        }
        while (refines.size()) {                        // 拆分每个字符绑定的fx/y块
            long a;
            std::tie(a, fx) = *refines.begin();
            refines.erase(refines.begin());

            for (x = fx;;) {                            // eg: radj[5] = [ { 'a', 2 }, { 'a', 7 }, { 'a', 12 }, { 'b', 3 }, { 'c', 8 } ]
                auto it = lower_bound(ALL(radj[x]),     // 在 radj[x] 中找到第一个满足 转移字符 >= a 且 来源状态 >= 0 的元素
                        std::make_pair(a, 0L));
                auto ite = upper_bound(ALL(radj[x]),    // 在 radj[x] 中找到第一个满足 转移字符 > a 或 来源状态 > n() 的元素
                        std::make_pair(a, n()));
                for (; it != ite; ++it) {               // 该区间 正好包含所有转移字符等于 a 的边
                    y = it->second;
                    CC[B[y]] = 0;
                    mark[y] = false;
                }
                if ((x = R[x]) == fx) {
                    break;
                }
            }

            for (x = fx;;) {                            // 统计每个状态被字符 a 转移到的目标分区的数量
                auto it = lower_bound(ALL(radj[x]),
                        std::make_pair(a, 0L));
                auto ite = upper_bound(ALL(radj[x]),
                        std::make_pair(a, n()));
                for (; it != ite; ++it) {
                    y = it->second;                     // y是前驱态
                    CC[B[y]] = ++;                      // 统计每个子分区(前驱态所站的队)被 a 字符转移到的数量
                    mark[y] = true;
                }
                if ((x = R[x]) == fx) {
                    break;
                }
            }

            for (x = fx;;) {                            // refine: 分区分裂
                auto it = lower_bound(ALL(radj[x]),
                        std::make_pair(a, 0L));
                auto ite = upper_bound(ALL(radj[x]),
                        std::make_pair(a, n()));        // 查找通过字符 a 转移到当前分区的前驱
                for (; it != ite; ++it) {
                    fy = B[it->second];                 // fy是前驱状态所属的分区(所站的队)
                    if (CC[fy] != C[fy] && !--CC[fy]) { // 如果发现某个分区fy中的状态被字符a转移到的目标分区不一致，就准备分裂它
                        long fu = -1;
                        long u = -1;
                        long cu = 0;
                        long fv = -1;
                        long v = -1;
                        long cv = 0;

                        for (long i = fy;;) {
                            if (mark[i]) {              // 被字符a转移到的状态
                                if (u < 0) {
                                    fu = i;
                                } else {
                                    R[fu] = i;
                                }
                                cu++;
                                B[i] = fu;              // 加入fu分区
                                L[i] = u;
                                u = i;
                            } else {
                                if (v < 0) {
                                    fv = i;
                                } else {
                                    R[fv] = i;
                                }
                                cv++;
                                B[i] = fv;
                                L[i] = v;
                                v = i;
                            }
                            if ((i = R[i]) == fy) {
                                break;
                            }
                        }
                        L[fu] = u;
                        R[u] = fu;
                        L[fv] = v;
                        R[v] = fv;
                        REP (a, 256) {
                            std::pair<long, long> t {a, fu != y ? fu : fv};
                            if (refines.count({a, fy})) {
                                refines.emplace(a, fu != fy ? fu : fv);
                            } else {
                                refines.emplace(a, cu < cv ? fu : fv);
                            }
                        }
                    }
                }
                if ((x = R[x]) == fx) {
                    break;
                }
            }
        }
    }
}

Fsa Fsa::determinize() const {
    Fsa r;
    return r;
}

