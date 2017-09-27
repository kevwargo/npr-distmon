#ifndef _DISTLOCK_H_INCLUDED_
#define _DISTLOCK_H_INCLUDED_

#include "dllist.h"
#include "distenv.h"

#define DISTLOCK_REQUEST 1
#define DISTLOCK_REPLY 2

struct distenv;

struct distlock {
    int timestamp;
    struct dllist *deferred;
};

struct distlock_msg {
    int timestamp;
};

extern void distlock_init(struct distenv *distenv);
extern int distlock_lock(struct distenv *distenv);
extern int distlock_unlock(struct distenv *distenv);

#endif
