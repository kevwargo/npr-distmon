#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "node.h"
#include "dllist.h"

void fprint_nodes(FILE *f, struct dllist *list)
{
    struct node *node;
    dllist_foreach(node, list) {
        fprintf(f, "%d %s:%d\n", node->id, inet_ntoa(node->saddr.sin_addr), ntohs(node->saddr.sin_port));
    }
    fprintf(f, "\n");
}

void print_nodes(struct dllist *list)
{
    fprint_nodes(stdout, list);
}

int contains(struct dllist *list, int id)
{
    struct node *node;
    dllist_foreach(node, list) {
        if (node->id == id) {
            return 1;
        }
    }
    return 0;
}

struct node *add_node(struct dllist *list, struct node *node)
{
    if (! contains(list, node->id)) {
        return dllist_add(list, node);
    }
    return NULL;
}

void delete_node_by_id(struct dllist *list, int id)
{
    struct node *node;
    dllist_foreach(node, list) {
        if (node->id == id) {
            dllist_delete(list, node);
            return;
        }
    }
}

uint8_t *pack_nodes(struct dllist *list)
{
    if (list->size <= 0) {
        return NULL;
    }
    uint8_t *buffer = (uint8_t *)malloc(PACKED_NODE_SIZE * list->size);
    int offset = 0;
    struct node *node;
    dllist_foreach(node, list) {
        struct packed_node *packed = (struct packed_node *)(buffer + offset);
        packed->id = node->id;
        packed->ip = node->saddr.sin_addr.s_addr;
        packed->port = node->saddr.sin_port;
        offset += PACKED_NODE_SIZE;
    }
    return buffer;
}
