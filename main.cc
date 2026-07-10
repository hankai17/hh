#include "common.hh"
#include "loader.hh"
#include "option.hh"

#include <string.h>
#include <sysexits.h>
#include <getopt.h>

using namespace std;

void print_help(FILE *fh) {
    fprintf(fh, "Usage: %s [Options] dir\n", program_invocation_short_name);
    fputs(
        "\n"
        "Options:\n"
        "   -h, --help          display this help and exit\n"
        "\n"
        "Examples:\n",
        fh);
    exit(fh == stdout ? 0 : EX_USAGE);
}

int main(int argc, char **argv) {
    long r;
    int opt;
    FILE *file;
    int errors;
    string data;
    char buf[BUF_SIZE];
    char *opt_output_filename = NULL;

    static struct option long_options[] = {
        {"help",    no_argument,       0,  'h'},
        {"module-info",  required_argument,   0,  'm'},
        {"output",  required_argument,   0,  'O'},
        {0,         0,                  0,  'h'},
    };

    while ((opt = getopt_long(argc, argv, "Dhmo:t", long_options, NULL)) != -1) {
        switch (opt) {
            case 'D':
                break;
            case 'h':
                print_help(stdout);
                break;
            case 'm':
                opt_module_info = true;
                break;
            case 'o':
                opt_output_filename = optarg;
                break;
            case 't':
                opt_dump_tree = true;
                break;
            case '?':
                print_help(stderr);
                break;
        }
    }
    argc -= optind;
    argv += optind;

    load(argc ? argv[0] : NULL);
    unload_all();

    return 0;
}

