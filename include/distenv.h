#ifndef _DISTENV_H_INCLUDED_
#define _DISTENV_H_INCLUDED_

#include <poll.h>
#include "dllist.h"

struct distenv {
    struct dllist *node_list;
    int self_id;
    int self_sock;
    struct pollfd *pfds;
};

#endif
