#pragma once

#include <vector>
#include <functional>

struct Fsa {
    long start;
    std::vector<long> finals;
    std::vector<std::vector<std::pair<long, long>>> adj;    // 每个元素是一个状态出边集合 eg: 状态2的出边 包含有<'b', 2> <'b', 4>
                                                            //                              状态3的出边 <-1, 2>, <a, 4>

    long n() const { return adj.size(); }
    bool is_final(long x) const;
    void epsilon_closure(std::vector<long> &src) const;

    Fsa operator~() const;
    Fsa difference(const Fsa &rhs, std::function<void (long, long)> relate) const;
    Fsa intersect(const Fsa &rhs, std::function<void (long, long)> relate) const;
    Fsa determinize(std::function<void (std::vector<long>&)> relate) const;
    Fsa hopcroft_minimize(std::function<void (std::vector<long>&)> relate);
};

