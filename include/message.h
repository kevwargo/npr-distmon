#ifndef _MESSAGE_H_INCLUDED_
#define _MESSAGE_H_INCLUDED_

#include <pthread.h>
#include <stdio.h>
#include "dllist.h"
#include "distenv.h"
#include "node.h"

struct node;
struct distenv;
struct message;

typedef int (*message_handler_t)(struct distenv *distenv, struct message *message);

struct __attribute__((packed)) message_header {
    int type;
    int len;
    unsigned char data[0];
};

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
    struct dllist *handlers;
};

extern int handle_message(struct distenv *distenv, struct node *node);
extern int send_message(struct node *node, int type, int len, void *buf);
extern void register_handler(struct distenv *distenv, message_handler_t handler);
extern void unregister_handler(struct distenv *distenv, message_handler_t handler);


#endif
