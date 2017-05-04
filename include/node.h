#ifndef _NODE_H_INCLUDED_
#define _NODE_H_INCLUDED_

#include <stdio.h>
#include <netinet/in.h>
#include <poll.h>
#include "dllist.h"
#include "message.h"

#define PACKED_NODE_ID_SIZE (sizeof(int))
#define PACKED_NODE_IP_SIZE (sizeof(uint32_t))
#define PACKED_NODE_PORT_SIZE (sizeof(in_port_t))
#define PACKED_NODE_SIZE (PACKED_NODE_ID_SIZE + PACKED_NODE_IP_SIZE + PACKED_NODE_PORT_SIZE)

struct node {
    int id;
    int sfd;
    struct sockaddr_in saddr;
    struct pollfd *pfd;
    struct message *msg_part;
};

struct packed_node {
    int id;
    uint32_t ip;
    in_port_t port;
};


extern struct node *add_node(struct dllist *list, struct node *node);
extern int contains(struct dllist *list, int id);
extern void print_nodes(struct dllist *list);
extern void fprint_nodes(FILE *f, struct dllist *list);
extern void delete_node_id(struct dllist *list, int id);
extern uint8_t *pack_nodes(struct dllist *list);

#endif
