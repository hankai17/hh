#include "fsa.hh"
#include "unittest_helper.hh"

#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <unistd.h>

/*
const char test[] =
    "4 7 2\n"
    "2 3  \n"
    "0 0 1\n"
    "0 -1 2\n"
    "1 1 1\n"
    "1 1 3\n"
    "2 -1 1\n"
    "2 0 3\n"
    "3 0 2\n";

int main(int argc, char **argv) {
    if (argc == 1) {
        char filename[] = "/tmp/XXXXXX";
        int fd = mkstemp(filename);
        write(fd, test, sizeof(test) - 1);
        close(fd);
        freopen(filename, "r", stdin);
        unlink(filename);
    }

    //Fsa fsa = read_nfa();
    Fsa fsa = read_nfa().determinize();
    print_fsa(fsa);
    return 0;
}
*/

/*
const char test[] =
    "4 4 1\n"
    "3  \n"
    "0 0 1\n"
    "0 1 2\n"
    "1 0 3\n"
    "2 0 3\n";

int main(int argc, char **argv) {
    if (argc == 1) {
        char filename[] = "/tmp/XXXXXX";    // 必须这样写
        int fd = mkstemp(filename);
        write(fd, test, sizeof(test) - 1);
        close(fd);
        freopen(filename, "r", stdin);
        unlink(filename);
    }

    //Fsa fsa = read_dfa();
    Fsa fsa = read_dfa().hopcroft_minimize();
    print_fsa(fsa);

    return 0;
}
*/

const char test[] =
    "4 4 1\n"
    "3  \n"
    "0 0 1\n"
    "0 1 2\n"
    "1 0 3\n"
    "2 1 3\n"

    "4 4 1\n"
    "3  \n"
    "0 0 1\n"
    "0 1 2\n"
    "1 1 3\n"
    "2 1 3\n";

int main(int argc, char **argv) {
    if (argc == 1) {
        char filename[] = "/tmp/XXXXXX";    // 必须这样写
        int fd = mkstemp(filename);
        write(fd, test, sizeof(test) - 1);
        close(fd);
        freopen(filename, "r", stdin);
        unlink(filename);
    }

    Fsa a = read_nfa();
    Fsa b = read_nfa();
    //Fsa f = a | b;
    //Fsa f = a & b;
    Fsa f = a - b;
    print_fsa(f);
    std::cout << "f.n(): " << f.n() << std::endl;

    return 0;
}
