#include "location.hh"

#include <algorithm>
#include <cstdarg>

using namespace std;


LocationFile::LocationFile(const std::string &filename,
        const std::string &data) :
        filename(filename), data(data) {
    long nlines = 1 + count(data.begin(), data.end(), '\n');
    linemap.assign(nlines + 1, 0);
    long line = 1;
    for (long i = 0; i < data.size(); i++) {
        if (data[i] == '\n') {
            linemap[line ++] = i + 1;            // 记录每行开始的偏移下标
        }
    }
    linemap[line] = data.size();
}

void LocationFile::locate(const Location &loc,  // TODO
        const char *fmt, ...) const {
    va_list va;
    va_start(va, fmt);
    long line = 0;
    while (linemap[line + 1] <= loc.start) {
        line ++;
    }
    fprintf(stderr, "line %ld %ld-%ld: ",
            line + 1,
            loc.start - linemap[line] + 1,
            loc.end - linemap[line]);
    vfprintf(stderr, fmt, va);
    fprintf(stderr, "\n");
    va_end(va);
}

