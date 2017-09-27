#ifndef _DEBUG_H_INCLUDED_
#define _DEBUG_H_INCLUDED_

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include "distenv.h"
#include "cmdline.h"

#define DEBUG_FPRINTF(stream, format, ...) \
    fprintf(stream, "%s (in %s): " format " (%s:%d)\n", __logprefix, __PRETTY_FUNCTION__, ##__VA_ARGS__, __FILE__, __LINE__); \
    fflush(stream)

#define DEBUG_PRINTF(format, ...) DEBUG_FPRINTF(stdout, format, ##__VA_ARGS__)

#define DEBUG_PERROR(msg) \
    __errno_bak = errno; \
    DEBUG_FPRINTF(stderr, "%s: %s (%s:%d)", msg, strerror(__errno_bak), __FILE__, __LINE__)


extern char __logprefix[];
extern int __errno_bak;

extern void init_log(struct cmdline_options *options);
extern void log_nodes(int signum);
extern void hexdump(void *voidbuf, int length);

#endif
