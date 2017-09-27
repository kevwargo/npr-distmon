#ifndef _CMDLINE_H_INCLUDED_
#define _CMDLINE_H_INCLUDED_

struct cmdline_options {
    char *stdout_file;
    char *stderr_file;
    char *bind_param;
    char *conn_param;
    int interactive;
};

extern int parse_cmdline(int argc, char **argv, struct cmdline_options *options);

#endif
