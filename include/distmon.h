#ifndef _DISTMON_H_INCLUDED_
#define _DISTMON_H_INCLUDED_

#include <pthread.h>
#include "distenv.h"


extern struct distenv *global_distenv;

extern int distmon_start(int argc, char **argv, pthread_t *thread);

#endif
