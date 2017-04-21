#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "node.h"

void fprint_nodes(FILE *f, struct node *list)
{
    for (struct node *node = list; node; node = node->next) {
        fprintf(f, "%d %s:%d (p: %lx, n: %lx)\n", node->id, inet_ntoa(node->saddr.sin_addr), ntohs(node->saddr.sin_port), (unsigned long int)node->prev, (unsigned long int)node->next);
    }
    fprintf(f, "\n");
}

void print_nodes(struct node *list)
{
    fprint_nodes(stdout, list);
}


int contains(struct node *list, int id)
{
    for (struct node *node = list; node; node = node->prev) {
        if (node->id == id) {
            return 1;
        }
    }
    for (struct node *node = list; node; node = node->next) {
        if (node->id == id) {
            return 1;
        }
    }
    return 0;
}

struct node *add_node(struct node **list, struct node *node)
{
    if (contains(*list, node->id)) {
        return NULL;
    }
    struct node **newptr;
    struct node *new, *prev = NULL;
    for (newptr = list; *newptr; newptr = &(*newptr)->next) {
        prev = *newptr;
    }
    *newptr = (struct node *)malloc(sizeof(struct node));
    new = *newptr;
    memcpy(new, node, sizeof(struct node));
    new->prev = prev;
    new->next = NULL;
    return new;
}

void delete_node(struct node **list, struct node *node)
{
    if (*list == node) {
        *list = node->next;
    }
    if (node->prev) {
        node->prev->next = node->next;
    }
    if (node->next) {
        node->next->prev = node->prev;
    }
    free(node);
}

void delete_node_by_id(struct node **list, int id)
{
    for (struct node *node = *list; node; node = node->next) {
        if (node->id == id) {
            delete_node(list, node);
            return;
        }
    }
}

void free_node_list(struct node **list)
{
    for (; *list; delete_node(list, *list)) ;
}

int count_nodes(struct node *list)
{
    int count = 0;
    for (; list; list = list->next) {
        count++;
    }
    return count;
}

uint8_t *pack_nodes(struct node *list, int *sizeptr)
{
    if (! list) {
        return NULL;
    }
    int size = count_nodes(list);
    if (sizeptr) {
        *sizeptr = PACKED_NODE_SIZE * size;
    }
    uint8_t *buffer = (uint8_t *)malloc(PACKED_NODE_SIZE * size);
    int offset = 0;
    for (; list; list = list->next) {
        struct packed_node *node = (struct packed_node *)(buffer + offset);
        node->id = list->id;
        node->ip = list->saddr.sin_addr.s_addr;
        node->port = list->saddr.sin_port;
        offset += PACKED_NODE_SIZE;
    }
    return buffer;
}
