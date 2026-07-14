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

    LocationFile() = default;
    LocationFile(const std::string &filename, const std::string &data);
    LocationFile &operator=(const LocationFile &) = default;
    void locate(const Location &loc, const char *fmt, ...) const;
};

