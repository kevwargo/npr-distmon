#ifndef _DISTENV_H_INCLUDED_
#define _DISTENV_H_INCLUDED_

#include <poll.h>
#include "dllist.h"
#include "message.h"

struct distenv {
    int self_id;
    int self_sock;
    int timestamp;
    struct dllist *node_list;
    struct pollfd *pfds;
    struct msg_buffer *msg_buffer;
};

extern struct distenv *distenv_init();

#endif
