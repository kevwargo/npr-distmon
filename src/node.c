#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "node.h"

#define ALLOC_STEP 2


void add_node(struct node_list **list_ptr, int sfd, struct sockaddr_in *saddr)
{
    if (! list_ptr) {
        return;
    }
    struct node_list *list = *list_ptr;
    if (! list) {
        *list_ptr = malloc(sizeof(struct node_list) + sizeof(struct node) * ALLOC_STEP);
        if (! *list_ptr) {
            perror("malloc");
            exit(1);
        }
        list = *list_ptr;
        list->len = 0;
        list->allocated = ALLOC_STEP;
        printf("allocated %d\n", list->allocated);
    }
    if (list->allocated <= list->len) {
        list->allocated += ALLOC_STEP;
        *list_ptr = realloc(list, sizeof(struct node_list) + sizeof(struct node) * list->allocated);
        if (! *list_ptr) {
            perror("realloc");
            exit(1);
        }
        list = *list_ptr;
        printf("reallocated %d\n", list->allocated);
    }
    list->array[list->len].id = 0;
    list->array[list->len].sfd = sfd;
    memcpy(&list->array[list->len].saddr, saddr, sizeof(struct sockaddr_in));
    list->len++;
    printf("new len %d\n", list->len);
}

void print_nodes(struct node_list *list)
{
    for (int i = 0; i < list->len; i++) {
        printf("%d %s:%d\n", list->array[i].sfd, inet_ntoa(list->array[i].saddr.sin_addr), ntohs(list->array[i].saddr.sin_port));
    }
    printf("\n");
}
