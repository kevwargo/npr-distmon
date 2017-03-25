#ifndef _NODE_H_INCLUDED_
#define _NODE_H_INCLUDED_

#include <stdio.h>
#include <netinet/in.h>

struct node {
    int id;
    int sfd;
    struct sockaddr_in saddr;
};

struct node_list {
    int len;
    int allocated;
    struct node *array;
};


extern struct node_list *create_node_list();
extern void add_node(struct node_list *list, int id, int sfd, struct sockaddr_in *saddr);
extern void print_nodes(struct node_list *list);
extern void fprint_nodes(FILE *f, struct node_list *list);
extern void expand_node_list(struct node_list *list, int newlen);

#endif
