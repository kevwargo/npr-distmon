#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "node.h"
#include "socket.h"


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

int main(int argc, char **argv)
{
    int bindsock;
    struct sockaddr_in bind_addr;
    struct sockaddr_in conn_addr;

    int is_connect = parse_cmdline(argc, argv, &bind_addr, &conn_addr);

    bindsock = socket_initbind(&bind_addr);

    struct node_list *node_list = malloc(sizeof(struct node_list));
    node_list->len = 0;
    node_list->allocated = 0;

    if (is_connect) {
        int connsock = socket_initconn(&conn_addr);
        add_node(&node_list, connsock, &conn_addr);
        if (send(connsock, &bind_addr, sizeof(struct sockaddr_in), 0) < 0) {
            perror("send");
            return 1;
        }
    }

    while (1) {
        struct sockaddr_in c_addr;
        socklen_t c_addr_len = sizeof(struct sockaddr_in);
        int cfd = accept(bindsock, (struct sockaddr *)&c_addr, &c_addr_len);
        if (cfd < 0) {
            perror("accept");
            return 1;
        }

        struct sockaddr_in c_bind_addr;
        if (recv(cfd, &c_bind_addr, sizeof(struct sockaddr_in), MSG_WAITALL) < 0) {
            perror("recv");
            return 1;
        }
        add_node(&node_list, cfd, &c_bind_addr);
        print_nodes(node_list);
    }
}
