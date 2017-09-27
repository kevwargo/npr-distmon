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

typedef int (*message_callback_t)(struct distenv *distenv, struct message *message, void *data);

struct __attribute__((packed)) message_header {
    int type;
    int len;
    unsigned char data[0];
};

struct message {
    struct node *sender;
    int type;
    int len;
    int received;
    unsigned char data[0];
};

struct msg_buffer {
    struct dllist *queue;
    pthread_mutex_t *mutex;
    pthread_cond_t *cond;
    struct dllist *handlers;
};

struct message_handler {
    message_callback_t callback;
    void *data;
};

extern int handle_message(struct distenv *distenv, struct node *node);
extern size_t send_message(struct node *node, int type, int len, void *buf);
extern void register_handler(struct distenv *distenv, message_callback_t handler, void *data);
extern void unregister_handler(struct distenv *distenv, message_callback_t handler, int free_data);
extern struct message *wait_for_message(struct distenv *distenv, message_callback_t matcher, void *data);


#endif
