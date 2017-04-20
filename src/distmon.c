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

struct node_list global_node_list = {0, 0, NULL};
int global_id = 1;

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
    int found = 0;
    for (int i = 0; i < node_list->len; i++) {
        if (node_list->array[i].id == node_id) {
            found = 1;
            break;
        }
    }
    if (! found) {
        add_node(node_list, node_id, connsock, conn_addr);
    }

    int id = node_id + 1;
    struct node_list *list = create_node_list();
    if (recv(connsock, &list->len, sizeof(int), MSG_WAITALL) < 0) {
        perror("recv node list len");
        exit(1);
    }
    if (list->len > 0) {
        expand_node_list(list, list->len);
        if (recv(connsock, list->array, sizeof(struct node) * list->len, MSG_WAITALL) < 0) {
            perror("recv node list");
            exit(1);
        }
    }

    for (int i = 0; i < list->len; i++) {
        found = 0;
        for (int j = 0; j < node_list->len; j++) {
            if (list->array[i].id == node_list->array[j].id) {
                found = 1;
                break;
            }
        }
        if (! found) {
            int temp_id = propagate(&list->array[i].saddr, node_list);
            if (temp_id > id) {
                id = temp_id;
            }
        }
    }

    return id;
}

int init_distenv(struct sockaddr_in *bind_addr, struct sockaddr_in *conn_addr, struct node_list *node_list)
{
    struct node self;
    self.id = propagate(conn_addr, node_list);
    memcpy(&self.saddr, bind_addr, sizeof(struct sockaddr_in));
    for (int i = 0; i < node_list->len; i++) {
        if (send(node_list->array[i].sfd, &self, sizeof(struct node), 0) < 0) {
            perror("send self node info");
            exit(1);
        }
    }
    return self.id;
}

void accept_loop(int id, int bindsock, struct node_list *node_list)
{
    while (1) {
        struct sockaddr_in c_addr;
        socklen_t c_addr_len = sizeof(struct sockaddr_in);
        int cfd = accept(bindsock, (struct sockaddr *)&c_addr, &c_addr_len);
        if (cfd < 0) {
            perror("accept");
            exit(1);
        }

        if (send(cfd, &id, sizeof(int), 0) < 0) {
            perror("send self node id");
            exit(1);
        }
        if (send(cfd, &node_list->len, sizeof(int), 0) < 0) {
            perror("send node list len");
            exit(1);
        }
        if (node_list->len > 0) {
            if (send(cfd, node_list->array, sizeof(struct node) * node_list->len, 0) < 0) {
                perror("send node list");
                exit(1);
            }
        }

        struct node node;
        if (recv(cfd, &node, sizeof(struct node), MSG_WAITALL) < 0) {
            perror("recv node info");
            exit(1);
        }
        add_node(node_list, node.id, cfd, &node.saddr);
        printf("New node list after accepting a node:\n");
        print_nodes(node_list);
    }
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
    if (fprintf(logfile, "Nodes connected to %d:\n", global_id) < 0) {
        perror("log_nodes: fprintf logfile header");
        fclose(logfile);
        return;
    }
    for (int i = 0; i < global_node_list.len; i++) {
        struct node *node = &global_node_list.array[i];
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
    struct sockaddr_in bind_addr;
    struct sockaddr_in conn_addr;
    /* struct node_list *node_list = create_node_list(); */
    /* int id = 1; */

    int is_connect = parse_cmdline(argc, argv, &bind_addr, &conn_addr);
    bindsock = socket_initbind(&bind_addr);

    if (is_connect) {
        global_id = init_distenv(&bind_addr, &conn_addr, &global_node_list);
        printf("self.id = %d\n", global_id);
        print_nodes(&global_node_list);
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_handler = log_nodes;
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGUSR1, &sa, NULL) < 0) {
        perror("sigaction");
    }

    accept_loop(global_id, bindsock, &global_node_list);

    return 0;
}
