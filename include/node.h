#ifndef _NODE_H_INCLUDED_
#define _NODE_H_INCLUDED_

#include <stdio.h>
#include <netinet/in.h>
#include <poll.h>

#define PACKED_NODE_ID_SIZE (sizeof(int))
#define PACKED_NODE_IP_SIZE (sizeof(uint32_t))
#define PACKED_NODE_PORT_SIZE (sizeof(in_port_t))
#define PACKED_NODE_SIZE (PACKED_NODE_ID_SIZE + PACKED_NODE_IP_SIZE + PACKED_NODE_PORT_SIZE)

struct node {
    int id;
    int sfd;
    struct sockaddr_in saddr;
    struct pollfd *pfd;
    struct node *prev;
    struct node *next;
};

struct packed_node {
    int id;
    uint32_t ip;
    in_port_t port;
};

struct node_list {
    struct node *head;
    struct node *tail;
    int size;
    struct pollfd *pfds;
};


extern struct node_list create_node_list();
extern struct node *add_node(struct node_list *list, struct node *node);
extern int contains(struct node_list *list, int id);
extern void print_nodes(struct node_list *list);
extern void fprint_nodes(FILE *f, struct node_list *list);
extern void delete_node(struct node_list *list, struct node *node);
extern void delete_node_id(struct node_list *list, int id);
extern void free_node_list(struct node_list *list);
extern uint8_t *pack_nodes(struct node_list *list);

#endif
