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
static int discover_id(struct sockaddr_in *conn_addr, struct dllist *node_list);


struct distenv *distenv_init(char *bind_param, char *conn_param)
{
    if (! bind_param) {
        return NULL;
    }

    struct sockaddr_in bind_addr;
    if (socket_parseaddr(bind_param, &bind_addr) < 0) {
        DEBUG_FPRINTF(stderr, "Error while parsing bind address");
        return NULL;
    }
    DEBUG_PRINTF("Bound to %s", bind_param);

    struct distenv *distenv = (struct distenv *)malloc(sizeof(struct distenv));
    memset(distenv, 0, sizeof(struct distenv));
    distenv->self_id = 1;
    distenv->self_sock = socket_initbind(&bind_addr);
    distenv->node_list = dllist_create();

    if (conn_param) {
        struct sockaddr_in conn_addr;
        if (socket_parseaddr(conn_param, &conn_addr) < 0) {
            DEBUG_FPRINTF(stderr, "Error while parsing connect address");
            free(distenv);
            return NULL;
        }
        if (distenv_connect(distenv, &conn_addr) < 0) {
            DEBUG_FPRINTF(stderr, "Error while connecting to other nodes");
            free(distenv);
            return NULL;
        }
        DEBUG_PRINTF("Connected to %s", conn_param);
    }

    distenv->msg_buffer = (struct msg_buffer *)malloc(sizeof(struct msg_buffer));
    distenv->msg_buffer->queue = dllist_create();
    distenv->msg_buffer->handlers = dllist_create();
    distenv->msg_buffer->mutex = malloc(sizeof(pthread_mutex_t));
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(distenv->msg_buffer->mutex, &attr);
    pthread_mutexattr_destroy(&attr);
    distenv->msg_buffer->cond = malloc(sizeof(pthread_cond_t));
    pthread_cond_init(distenv->msg_buffer->cond, NULL);

    distlock_init(distenv);

    return distenv;
}

static int distenv_connect(struct distenv *distenv, struct sockaddr_in *conn_addr)
{
    struct sockaddr_in bind_addr;
    socklen_t addrlen = sizeof(struct sockaddr_in);
    if (getsockname(distenv->self_sock, (struct sockaddr *)&bind_addr, &addrlen) < 0) {
        DEBUG_PERROR("getsockname");
        return -1;
    }

    struct packed_node self;
    self.id = discover_id(conn_addr, distenv->node_list);
    self.ip = bind_addr.sin_addr.s_addr;
    self.port = bind_addr.sin_port;

    struct node *node;
    dllist_foreach(node, distenv->node_list) {
        if (send(node->sfd, &self, sizeof(struct packed_node), 0) < 0) {
            DEBUG_PERROR("send self node info");
            return -1;
        }
    }

    distenv->self_id = self.id;

    return 0;
}

static int discover_id(struct sockaddr_in *conn_addr, struct dllist *node_list)
{
    int connsock = socket_initconn(conn_addr);
    int node_id;
    if (recv(connsock, &node_id, sizeof(int), MSG_WAITALL) < 0) {
        DEBUG_PERROR("recv node id");
        exit(1);
    }
    if (! find_node(node_list, node_id)) {
        struct node node;
        memset(&node, 0, sizeof(struct node));
        node.id = node_id;
        node.sfd = connsock;
        memcpy(&node.saddr, conn_addr, sizeof(struct sockaddr_in));
        add_node(node_list, &node);
    }

    int id = node_id + 1;
    int nodes_count;
    if (recv(connsock, &nodes_count, sizeof(int), MSG_WAITALL) < 0) {
        DEBUG_PERROR("recv node list len");
        exit(1);
    }
    if (nodes_count > 0) {
        int bufsize = nodes_count * sizeof(struct packed_node);
        struct packed_node *nodes_buf = (struct packed_node *)malloc(bufsize);
        if (recv(connsock, nodes_buf, bufsize, MSG_WAITALL) < 0) {
            DEBUG_PERROR("recv node list");
            exit(1);
        }
        for (int i = 0; i < nodes_count; i++) {
            if (! find_node(node_list, nodes_buf[i].id)) {
                struct sockaddr_in saddr;
                memset(&saddr, 0, sizeof(struct sockaddr_in));
                saddr.sin_family = AF_INET;
                saddr.sin_addr.s_addr = nodes_buf[i].ip;
                saddr.sin_port = nodes_buf[i].port;
                int temp_id = discover_id(&saddr, node_list);
                if (temp_id > id) {
                    id = temp_id;
                }
            }
        }
        free(nodes_buf);
    }

    return id;
}

