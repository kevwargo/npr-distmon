#ifndef _MESSAGE_H_INCLUDED_
#define _MESSAGE_H_INCLUDED_

#include <pthread.h>
#include <stdio.h>
#include "dllist.h"
#include "distenv.h"
#include "node.h"

struct node;
struct distenv;

struct message {
    int sender_id;
    int type;
    int len;
    int received;
    unsigned char data[0];
};

struct msg_buffer {
    struct dllist *list;
    pthread_mutex_t *mutex;
    pthread_cond_t *cond;
};

extern int handle_message(struct distenv *distenv, struct node *node);


#endif
