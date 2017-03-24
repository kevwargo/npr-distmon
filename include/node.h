#ifndef _NODE_H_INCLUDED_
#define _NODE_H_INCLUDED_

#include <netinet/in.h>

struct node {
    int id;
    int sfd;
    struct sockaddr_in saddr;
};

struct node_list {
    int len;
    int allocated;
    struct node array[0];
};


extern void add_node(struct node_list **list_ptr, int sfd, struct sockaddr_in *saddr);
extern void print_nodes(struct node_list *list);

#endif
