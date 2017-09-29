#ifndef _DISTLOCK_H_INCLUDED_
#define _DISTLOCK_H_INCLUDED_

#include "dllist.h"
#include "distenv.h"

#define DISTLOCK_REQUEST 1
#define DISTLOCK_REPLY 2
#define DISTLOCK_COND_SIGNAL 3

struct distenv;

struct distlock_cond {
    int bufsize;
    unsigned char *buffer;
};

struct distlock {
    int timestamp;
    struct dllist *deferred;
    struct distlock_cond *cond;
    int is_held;
};

struct distlock_request {
    int timestamp;
};

struct distlock_signal {
    int bufsize;
    unsigned char buffer[0];
};

extern void distlock_init(struct distenv *distenv);
extern int distlock_lock(struct distenv *distenv);
extern int distlock_unlock(struct distenv *distenv);

extern void distlock_cond_init(struct distenv *distenv, void *buffer, int bufsize);
extern int distlock_cond_wait(struct distenv *distenv);
extern int distlock_cond_broadcast(struct distenv *distenv);

#endif
