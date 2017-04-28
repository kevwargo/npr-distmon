#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "node.h"

void fprint_nodes(FILE *f, struct node_list *list)
{
    for (struct node *node = list->head; node; node = node->next) {
        fprintf(f, "%d %s:%d (p: %lx, n: %lx)\n", node->id, inet_ntoa(node->saddr.sin_addr), ntohs(node->saddr.sin_port), (unsigned long int)node->prev, (unsigned long int)node->next);
    }
    fprintf(f, "\n");
}

void print_nodes(struct node_list *list)
{
    fprint_nodes(stdout, list);
}


int contains(struct node_list *list, int id)
{
    for (struct node *node = list->head; node; node = node->next) {
        if (node->id == id) {
            return 1;
        }
    }
    return 0;
}

struct node_list *create_node_list()
{
    struct node_list *list = (struct node_list *)malloc(sizeof(struct node_list));
    if (! list) {
        perror("create node list");
        exit(1);
    }
    memset(list, 0, sizeof(struct node_list));
    return list;
}

struct node *add_node(struct node_list *list, struct node *node)
{
    if (contains(list, node->id)) {
        return NULL;
    }
    struct node *new = (struct node *)malloc(sizeof(struct node));
    memcpy(new, node, sizeof(struct node));
    new->next = NULL;
    new->prev = list->tail;
    if (list->size == 0) {
        list->head = list->tail = new;
    } else {
        list->tail->next = new;
        list->tail = new;
    }
    list->size++;
    return new;
}

void delete_node(struct node_list *list, struct node *node)
{
    if (list->head == node) {
        list->head = node->next;
    }
    if (list->tail == node) {
        list->tail = node->prev;
    }
    if (node->next) {
        node->next->prev = node->prev;
    }
    if (node->prev) {
        node->prev->next = node->next;
    }
    list->size--;
    free(node);
}

void delete_node_by_id(struct node_list *list, int id)
{
    for (struct node *node = list->head; node; node = node->next) {
        if (node->id == id) {
            delete_node(list, node);
            return;
        }
    }
}

void free_node_list(struct node_list *list)
{
    for (struct node *node = list->head; node; ) {
        delete_node(list, node);
    }
    free(list);
}

uint8_t *pack_nodes(struct node_list *list)
{
    if (list->size <= 0) {
        return NULL;
    }
    uint8_t *buffer = (uint8_t *)malloc(PACKED_NODE_SIZE * list->size);
    int offset = 0;
    for (struct node *node = list->head; node; node = node->next) {
        struct packed_node *packed = (struct packed_node *)(buffer + offset);
        packed->id = node->id;
        packed->ip = node->saddr.sin_addr.s_addr;
        packed->port = node->saddr.sin_port;
        offset += PACKED_NODE_SIZE;
    }
    return buffer;
}
