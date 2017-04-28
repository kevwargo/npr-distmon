#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include "node.h"
#include "socket.h"
#include "events.h"

struct node_list *global_node_list = NULL;
int global_id = 1;
struct sockaddr_in bind_addr;

int parse_cmdline(int argc, char **argv, struct sockaddr_in *bind_addr, struct sockaddr_in *conn_addr)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s bind_host:bind_port [connect_host:connect_port]\n\n", argv[0]);
        exit(1);
    }
    printf("Bind address: %s\n", argv[1]);
    socket_parseaddr(argv[1], bind_addr);
    if (argc > 2) {
        printf("Connect address: %s\n", argv[2]);
        socket_parseaddr(argv[2], conn_addr);
        return 1;
    }
    return 0;
}

int propagate(struct sockaddr_in *conn_addr, struct node_list *node_list)
{
    int connsock = socket_initconn(conn_addr);
    int node_id;
    if (recv(connsock, &node_id, sizeof(int), MSG_WAITALL) < 0) {
        perror("recv node id");
        exit(1);
    }
    if (! contains(node_list, node_id)) {
        struct node node;
        node.id = node_id;
        node.sfd = connsock;
        memcpy(&node.saddr, conn_addr, sizeof(struct sockaddr_in));
        add_node(node_list, &node);
    }

    int id = node_id + 1;
    int bufsize;
    uint8_t *buffer;
    if (recv(connsock, &bufsize, sizeof(int), MSG_WAITALL) < 0) {
        perror("recv node list len");
        exit(1);
    }
    if (bufsize > 0) {
        buffer = (uint8_t *)malloc(bufsize);
        if (recv(connsock, buffer, bufsize, MSG_WAITALL) < 0) {
            perror("recv node list");
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

int init_distenv(struct sockaddr_in *bind_addr, struct sockaddr_in *conn_addr, struct node_list *node_list)
{
    struct packed_node self;
    self.id = propagate(conn_addr, node_list);
    self.ip = bind_addr->sin_addr.s_addr;
    self.port = bind_addr->sin_port;
    for (struct node *node = node_list->head; node; node = node->next) {
        if (send(node->sfd, &self, PACKED_NODE_SIZE, 0) < 0) {
            perror("send self node info");
            exit(1);
        }
    }
    return self.id;
}

void log_nodes(int signum)
{
    char logname[127];
    snprintf(logname, 127, "nodes-%03d.log", global_id);
    FILE *logfile = fopen(logname, "w+");
    if (! logfile) {
        perror("log_nodes: fopen logfile");
        return;
    }
    if (fprintf(logfile, "%d %s:%d\n", global_id, inet_ntoa(bind_addr.sin_addr), ntohs(bind_addr.sin_port)) < 0) {
        perror("log_nodes: fprintf logfile header");
        fclose(logfile);
        return;
    }
    for (struct node *node = global_node_list->head; node; node = node->next) {
        if (fprintf(logfile, "%d %s:%d\n", node->id, inet_ntoa(node->saddr.sin_addr), ntohs(node->saddr.sin_port)) < 0) {
            perror("log_nodes: fprintf logfile");
            fclose(logfile);
            return;
        }
    }
    fclose(logfile);
}

int main(int argc, char **argv)
{
    int bindsock;
    struct sockaddr_in conn_addr;
    /* struct node *node_list = NULL; */
    /* int id = 1; */

    int is_connect = parse_cmdline(argc, argv, &bind_addr, &conn_addr);
    bindsock = socket_initbind(&bind_addr);
    global_node_list = create_node_list();
    global_node_list->self_sock = bindsock;

    if (is_connect) {
        global_id = init_distenv(&bind_addr, &conn_addr, global_node_list);
        printf("self.id = %d\n", global_id);
        print_nodes(global_node_list);
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_handler = log_nodes;
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGUSR1, &sa, NULL) < 0) {
        perror("sigaction");
    }

    event_loop(global_id, global_node_list);

    return 0;
}
