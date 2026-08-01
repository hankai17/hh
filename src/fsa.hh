#pragma once

#include <vector>
#include <functional>

typedef std::pair<long, long> Label;
typedef std::pair<Label, long> Edge;
const Label epsilon {-1L, 0L};

struct Fsa {
    long start;
    std::vector<long> finals;
    std::vector<std::vector<Edge>> adj;

    void check() const;
    long n() const { return adj.size(); }
    bool is_final(long x) const;
    bool has(long u, long a) const;
    bool has_call(long u) const;
    bool has_call_or_collapse(long u) const;
    void epsilon_closure(std::vector<long> &src) const;

    Fsa operator~() const;
    void accessible(const std::vector<long> *start, std::function<void(long)> relate);
    void co_accessible(const std::vector<bool> *final, std::function<void(long)> relate);
    Fsa difference(const Fsa &rhs, std::function<void (long)> relate) const;
    Fsa intersect(const Fsa &rhs, std::function<void (long, long)> relate) const;
    Fsa determinize(const std::vector<long> *starts, std::function<void (long, const std::vector<long>&)> relate) const;
    Fsa hopcroft_minimize(std::function<void (std::vector<long>&)> relate);
};

