#include <pthread.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "message.h"
#include "distenv.h"
#include "dllist.h"
#include "socket.h"
#include "message.h"
#include "debug.h"

static int distenv_connect(struct distenv *distenv, struct sockaddr_in *conn_addr);
static int propagate(struct sockaddr_in *conn_addr, struct dllist *node_list);


struct distenv *distenv_init(char *bind_param, char *conn_param)
{
    if (! bind_param) {
        return NULL;
    }

    struct sockaddr_in bind_addr;
    if (socket_parseaddr(bind_param, &bind_addr) < 0) {
        DEBUG_FPRINTF(stderr, "Error while parsing bind address\n");
        return NULL;
    }
    DEBUG_PRINTF("Bound to %s\n", bind_param);

    struct distenv *distenv = (struct distenv *)malloc(sizeof(struct distenv));
    memset(distenv, 0, sizeof(struct distenv));
    distenv->self_id = 1;
    distenv->self_sock = socket_initbind(&bind_addr);
    distenv->node_list = dllist_create();
    distenv->msg_buffer = (struct msg_buffer *)malloc(sizeof(struct msg_buffer));

    if (conn_param) {
        struct sockaddr_in conn_addr;
        if (socket_parseaddr(conn_param, &conn_addr) < 0) {
            DEBUG_FPRINTF(stderr, "Error while parsing connect address\n");
            free(distenv);
            return NULL;
        }
        if (distenv_connect(distenv, &conn_addr) < 0) {
            DEBUG_FPRINTF(stderr, "Error while connecting to other nodes\n");
            free(distenv);
            return NULL;
        }
        DEBUG_PRINTF("Connected to %s\n", conn_param);
    }

    distenv->msg_buffer->list = dllist_create();
    distenv->msg_buffer->mutex = malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(distenv->msg_buffer->mutex, NULL);
    distenv->msg_buffer->cond = malloc(sizeof(pthread_cond_t));
    pthread_cond_init(distenv->msg_buffer->cond, NULL);

    return distenv;
}

static int distenv_connect(struct distenv *distenv, struct sockaddr_in *conn_addr)
{
    struct sockaddr_in bind_addr;
    socklen_t addrlen;
    if (getsockname(distenv->self_sock, (struct sockaddr *)&bind_addr, &addrlen) < 0) {
        DEBUG_PERROR("getsockname");
        return -1;
    }
    DEBUG_PRINTF("bind_addr: ");
    hexdump(&bind_addr, sizeof(struct sockaddr_in));

    struct packed_node self;
    self.id = propagate(conn_addr, distenv->node_list);
    self.ip = bind_addr.sin_addr.s_addr;
    self.port = bind_addr.sin_port;
    DEBUG_PRINTF("self: ");
    hexdump(&self, sizeof(struct sockaddr_in));


    struct node *node;
    dllist_foreach(node, distenv->node_list) {
        if (send(node->sfd, &self, PACKED_NODE_SIZE, 0) < 0) {
            DEBUG_PERROR("send self node info");
            return -1;
        }
    }

    distenv->self_id = self.id;

    return 0;
}

static int propagate(struct sockaddr_in *conn_addr, struct dllist *node_list)
{
    int connsock = socket_initconn(conn_addr);
    int node_id;
    if (recv(connsock, &node_id, sizeof(int), MSG_WAITALL) < 0) {
        DEBUG_PERROR("recv node id");
        exit(1);
    }
    if (! contains(node_list, node_id)) {
        struct node node;
        memset(&node, 0, sizeof(struct node));
        node.id = node_id;
        node.sfd = connsock;
        memcpy(&node.saddr, conn_addr, sizeof(struct sockaddr_in));
        add_node(node_list, &node);
    }

    int id = node_id + 1;
    int bufsize;
    uint8_t *buffer;
    if (recv(connsock, &bufsize, sizeof(int), MSG_WAITALL) < 0) {
        DEBUG_PERROR("recv node list len");
        exit(1);
    }
    if (bufsize > 0) {
        buffer = (uint8_t *)malloc(bufsize);
        if (recv(connsock, buffer, bufsize, MSG_WAITALL) < 0) {
            DEBUG_PERROR("recv node list");
            exit(1);
        }
        for (int offset = 0; offset <= bufsize - PACKED_NODE_SIZE; offset += PACKED_NODE_SIZE) {
            struct packed_node *node = (struct packed_node *)(buffer + offset);
            if (! contains(node_list, node->id)) {
                struct sockaddr_in saddr;
                saddr.sin_family = AF_INET;
                saddr.sin_addr.s_addr = node->ip;
                saddr.sin_port = node->port;
                int temp_id = propagate(&saddr, node_list);
                if (temp_id > id) {
                    id = temp_id;
                }
            }
        }
        free(buffer);
    }

    return id;
}

