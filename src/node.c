#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "node.h"
#include "dllist.h"
#include "debug.h"

void fprint_nodes(FILE *f, struct dllist *list)
{
    if (list->size == 0) {
        DEBUG_FPRINTF(f, "No nodes.");
        return;
    }
    size_t size = list->size * 32;
    char *str = (char *)malloc(size);
    char *ptr = str;
    str[0] = '\0';
    struct node *node;
    dllist_foreach(node, list) {
        int printed = snprintf(ptr, size, "(%d %s:%d) ", node->id, inet_ntoa(node->saddr.sin_addr), ntohs(node->saddr.sin_port));
        if (printed < 0) {
            DEBUG_FPRINTF(f, "Error in snprintf");
            return;
        }
        size -= printed;
        ptr += printed;
    }
    DEBUG_FPRINTF(f, "Nodes: %s. (%lu B)", str, size);
    free(str);
}

void print_nodes(struct dllist *list)
{
    fprint_nodes(stdout, list);
}

struct node *find_node(struct dllist *list, int id)
{
    struct node *node;
    dllist_foreach(node, list) {
        if (node->id == id) {
            return node;
        }
    }
    return NULL;
}

struct node *add_node(struct dllist *list, struct node *node)
{
    if (! find_node(list, node->id)) {
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
    struct packed_node *buffer = (struct packed_node *)malloc(list->size * sizeof(struct packed_node));
    int i = 0;
    struct node *node;
    dllist_foreach(node, list) {
        buffer[i].id = node->id;
        buffer[i].ip = node->saddr.sin_addr.s_addr;
        buffer[i].port = node->saddr.sin_port;
        i++;
    }
    return (uint8_t *)buffer;
}
