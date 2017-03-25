#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "node.h"

#define ALLOC_STEP 2

struct node_list *create_node_list()
{
    struct node_list *list = malloc(sizeof(struct node_list));
    if (! list) {
        perror("create_node_list malloc");
        exit(1);
    }
    memset(list, 0, sizeof(struct node_list));
    return list;
}

void add_node(struct node_list *list, int id, int sfd, struct sockaddr_in *saddr)
{
    if (list->allocated <= list->len) {
        while (list->allocated <= list->len) {
            list->allocated += ALLOC_STEP;
        }
        list->array = realloc(list->array, sizeof(struct node) * list->allocated);
        if (! list->array) {
            perror("realloc");
            exit(1);
        }
    }
    list->array[list->len].id = id;
    list->array[list->len].sfd = sfd;
    memcpy(&list->array[list->len].saddr, saddr, sizeof(struct sockaddr_in));
    list->len++;
}

void print_nodes(struct node_list *list)
{
    fprint_nodes(stdout, list);
}

void fprint_nodes(FILE *f, struct node_list *list)
{
    for (int i = 0; i < list->len; i++) {
        fprintf(f, "%d %s:%d\n", list->array[i].id, inet_ntoa(list->array[i].saddr.sin_addr), ntohs(list->array[i].saddr.sin_port));
    }
    fprintf(f, "\n");
}

void expand_node_list(struct node_list *list, int newlen)
{
    if (list->allocated < newlen) {
        list->allocated = newlen;
    }
    list->array = realloc(list->array, sizeof(struct node) * list->allocated);
}

void free_node_list(struct node_list *list)
{
    if (list->array) {
        free(list->array);
    }
    free(list);
}
