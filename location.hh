#pragma once
#include "common.hh"

#include <vector>
#include <string>

struct Location {
    long start;
    long end;
};

struct LocationFile {
    std::string filename;
    std::string data;
    std::vector<long> linemap;

    LocationFile(const std::string &filename, const std::string &data);
    void locate(const Location &loc, const char *fmt, ...) const;
};

