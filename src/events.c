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
#include "dllist.h"
#include "distenv.h"


int refresh_pollfds(struct distenv *distenv)
{
    int size = distenv->node_list->size + 1;
    if (distenv->pfds) {
        free(distenv->pfds);
    }
    distenv->pfds = calloc(size, sizeof(struct pollfd));
    distenv->pfds[0].fd = distenv->self_sock;
    distenv->pfds[0].events = POLLIN;
    printf("POLL: Registering serv_sock\n");
    int i = 1;
    struct node *node;
    dllist_foreach(node, distenv->node_list) {
        printf("POLL: Registering node %d\n", node->id);
        distenv->pfds[i].fd = node->sfd;
        distenv->pfds[i].events = POLLIN;
        node->pfd = &distenv->pfds[i];
        i++;
    }
    return size;
}

void accept_node(struct distenv *distenv)
{
    struct sockaddr_in c_addr;
    socklen_t c_addr_len = sizeof(struct sockaddr_in);
    int cfd = accept(distenv->self_sock, (struct sockaddr *)&c_addr, &c_addr_len);
    if (cfd < 0) {
        perror("accept");
        exit(1);
    }

    if (send(cfd, &distenv->self_id, sizeof(int), 0) < 0) {
        perror("send self node id");
        exit(1);
    }
    int bufsize = distenv->node_list->size * PACKED_NODE_SIZE;
    if (send(cfd, &bufsize, sizeof(int), 0) < 0) {
        perror("send node list len");
        exit(1);
    }
    printf("sent node_list bufsize: %d\n", bufsize);
    if (bufsize > 0) {
        uint8_t *buffer = pack_nodes(distenv->node_list);
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
    add_node(distenv->node_list, &node);
    printf("New node list after accepting a node:\n");
    print_nodes(distenv->node_list);
}

char *str_revents(short revents)
{
    if (revents == 0) {
        return NULL;
    }
    char *result = (char *)calloc(40, 4);
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
 
void event_loop(struct distenv *distenv)
{
    int nfds = refresh_pollfds(distenv);
    while (1) {
        int selected = poll(distenv->pfds, nfds, -1);
        if (selected < 0) {
            if (errno == EINTR) {
                printf("EINTR caught on poll, restarting...\n");
                continue;
            }
            perror("poll");
            exit(1);
        }
        printf("poll returned %d\n", selected);
        char *str = str_revents(distenv->pfds[0].revents);
        if (str) {
            printf("server socket revents: %s\n", str);
            free(str);
        }
        struct node *node;
        dllist_foreach(node, distenv->node_list) {
            str = str_revents(node->pfd->revents);
            if (str) {
                printf("node %d revents: %s\n", node->id, str);
                free(str);
            }
            if (node->pfd->revents & POLLIN) {
                char c;
                int recved = recv(node->sfd, &c, 1, 0);
                if (recved <= 0) {
                    fprintf(stderr, "Node %d disconnected", node->id);
                    if (recved < 0) {
                        perror(" with error");
                    }
                    putc('\n', stderr);
                    dllist_delete(distenv->node_list, node);
                    nfds = refresh_pollfds(distenv);
                } else {
                    printf("Node %d sent char %x\n", node->id, c);
                }
            }
        }
        if (distenv->pfds[0].revents & POLLIN) {
            accept_node(distenv);
            nfds = refresh_pollfds(distenv);
        }
    }
}
