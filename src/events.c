#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "node.h"
#include "events.h"


int build_pollfds(int serv_sock, struct node *nodes, struct pollfd **pollfds)
{
    int size = count_nodes(nodes) + 1;
    *pollfds = calloc(size, sizeof(struct pollfd));
    struct pollfd *pfds = *pollfds;
    pfds[0].fd = serv_sock;
    pfds[0].events = POLLIN;
    printf("POLL: Registering serv_sock\n");
    int i = 1;
    for (struct node *node = nodes; node; node = node->next) {
        printf("POLL: Registering node %d\n", node->id);
        pfds[i].fd = node->sfd;
        pfds[i].events = POLLIN;
        i++;
    }
    return size;
}

void accept_node(int id, int serv_sock, struct node **node_list)
{
    struct sockaddr_in c_addr;
    socklen_t c_addr_len = sizeof(struct sockaddr_in);
    int cfd = accept(serv_sock, (struct sockaddr *)&c_addr, &c_addr_len);
    if (cfd < 0) {
        perror("accept");
        exit(1);
    }

    if (send(cfd, &id, sizeof(int), 0) < 0) {
        perror("send self node id");
        exit(1);
    }
    int bufsize = 0;
    uint8_t *buffer = pack_nodes(*node_list, &bufsize);
    if (send(cfd, &bufsize, sizeof(int), 0) < 0) {
        perror("send node list len");
        exit(1);
    }
    if (bufsize > 0) {
        int sent = send(cfd, buffer, bufsize, 0);
        /* TODO */
        if (sent < bufsize) {
            fprintf(stderr, "SENT LESS THAN BUFSIZE!!!\n");
            exit(2);
        }

        if (sent < 0) {
            perror("send node list");
            exit(1);
        }

        free(buffer);
    }

    struct packed_node n;
    if (recv(cfd, &n, PACKED_NODE_SIZE, MSG_WAITALL) < 0) {
        perror("recv node info");
        exit(1);
    }
    struct node node;
    node.id = n.id;
    node.sfd = cfd;
    node.saddr.sin_family = AF_INET;
    node.saddr.sin_addr.s_addr = n.ip;
    node.saddr.sin_port = n.port;
    add_node(node_list, &node);
    printf("New node list after accepting a node:\n");
    print_nodes(*node_list);
}

char *str_revents(short revents)
{
    if (revents == 0) {
        return NULL;
    }
    char *result = (char *)calloc(13, 12);
    if (revents & POLLIN) {
        strcat(result, "POLLIN | ");
    }
    if (revents & POLLPRI) {
        strcat(result, "POLLPRI | ");
    }
    if (revents & POLLOUT) {
        strcat(result, "POLLOUT | ");
    }
    if (revents & POLLRDNORM) {
        strcat(result, "POLLRDNORM | ");
    }
    if (revents & POLLRDBAND) {
        strcat(result, "POLLRDBAND | ");
    }
    if (revents & POLLWRNORM) {
        strcat(result, "POLLWRNORM | ");
    }
    if (revents & POLLWRBAND) {
        strcat(result, "POLLWRBAND | ");
    }
    if (revents & POLLERR) {
        strcat(result, "POLLERR | ");
    }
    if (revents & POLLHUP) {
        strcat(result, "POLLHUP | ");
    }
    if (revents & POLLNVAL) {
        strcat(result, "POLLNVAL | ");
    }
    return result;
}
 
void event_loop(int id, int serv_sock, struct node **node_list)
{
    struct pollfd *pfds;
    int nfds = build_pollfds(serv_sock, *node_list, &pfds);
    while (1) {
        int selected = poll(pfds, nfds, -1);
        if (selected < 0) {
            if (errno == EINTR) {
                printf("EINTR caught on poll, restarting...\n");
                continue;
            }
            perror("poll");
            exit(1);
        }
        printf("poll returned %d\n", selected);
        char *str = str_revents(pfds[0].revents);
        if (str) {
            printf("server socket revents: %s\n", str);
            free(str);
        }
        int i = 1;
        for (struct node *node = *node_list; node; node = node->next, i++) {
            str = str_revents(pfds[i].revents);
            if (str) {
                printf("node %d revents: %s\n", node->id, str);
                free(str);
            }
            if (pfds[i].revents & POLLIN) {
                char c;
                int recved = recv(pfds[i].fd, &c, 1, 0);
                if (recved <= 0) {
                    fprintf(stderr, "Node %d disconnected", node->id);
                    if (recved < 0) {
                        perror(" with error");
                    }
                    putc('\n', stderr);
                    delete_node(node_list, node);
                    free(pfds);
                    nfds = build_pollfds(serv_sock, *node_list, &pfds);
                } else {
                    printf("Node %d sent char %x\n", node->id, c);
                }
            }
        }
        if (pfds[0].revents & POLLIN) {
            accept_node(id, serv_sock, node_list);
            free(pfds);
            nfds = build_pollfds(serv_sock, *node_list, &pfds);
        }
    }
}
