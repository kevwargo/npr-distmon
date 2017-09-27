#ifndef _DEBUG_H_INCLUDED_
#define _DEBUG_H_INCLUDED_

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include "distenv.h"
#include "cmdline.h"

#define DEBUG_FPRINTF(stream, format, ...) \
    ({ \
        struct timespec ts; \
        clock_gettime(CLOCK_REALTIME, &ts); \
        double tm = (double)ts.tv_sec + ((double)ts.tv_nsec / (double)(1000 * 1000 * 1000)) - __start_time; \
        fprintf(stream, "%.2lf: %s (in %s): " format " (%s:%d)\n", tm, __logprefix, __PRETTY_FUNCTION__, ##__VA_ARGS__, __FILE__, __LINE__); \
        fflush(stream); \
    })

#define DEBUG_PRINTF(format, ...) DEBUG_FPRINTF(stdout, format, ##__VA_ARGS__)

#define DEBUG_PERROR(format, ...) \
    __errno_bak = errno; \
    DEBUG_FPRINTF(stderr, format ": %s", ##__VA_ARGS__, strerror(__errno_bak))


extern char __logprefix[];
extern int __errno_bak;
extern double __start_time;

extern void init_log(struct cmdline_options *options);
extern void log_nodes(int signum);
extern void hexdump(void *voidbuf, int length);

#endif
