#include <string.h>
#include <unistd.h>
#include "cmdline.h"

int parse_cmdline(int argc, char **argv, struct cmdline_options *options)
{
    memset(options, 0, sizeof(struct cmdline_options));
    opterr = 0;
    int c;
    while ((c = getopt(argc, argv, "io:e:")) >= 0) {
        switch (c) {
            case 'i':
                options->interactive = 1;
                break;
            case 'o':
                options->stdout_file = optarg;
                break;
            case 'e':
                options->stderr_file = optarg;
                break;
            default:
                return -1;
        }
    }
    if (optind < argc) {
        options->bind_param = argv[optind];
    }
    if (optind + 1 < argc) {
        options->conn_param = argv[optind + 1];
    }
    return 0;
}
