#include <errno.h>
#include <string.h>
#include <stdio.h>
#include "distenv.h"

#define DEBUG_FPRINTF(stream, format, ...)                              \
    for (int __i = 1; __i < __argc_ - 1; __i++) fprintf(stream, "%s ", __argv_[__i]); \
    if (__argc_ > 1) fprintf(stream, "%s: ", __argv_[__argc_ - 1]); \
    fprintf(stream, format " (%s:%d)\n", ##__VA_ARGS__, __FILE__, __LINE__)

#define DEBUG_PRINTF(format, ...) DEBUG_FPRINTF(stdout, format, ##__VA_ARGS__)

#define DEBUG_PERROR(msg) \
    DEBUG_FPRINTF(stderr, "%s: %s (%s:%d)", msg, strerror(errno), __FILE__, __LINE__)


extern int __argc_;
extern char **__argv_;
extern struct distenv *global_distenv;

extern void init_log(int argc, char **argv);
extern void log_nodes(int signum);
