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
#include "message.h"
#include "debug.h"


int refresh_pollfds(struct distenv *distenv)
{
    int size = distenv->node_list->size + 1;
    if (distenv->pfds) {
        free(distenv->pfds);
    }
    distenv->pfds = calloc(size, sizeof(struct pollfd));
    distenv->pfds[0].fd = distenv->self_sock;
    distenv->pfds[0].events = POLLIN;
    DEBUG_PRINTF("POLL: Registering serv_sock\n");
    int i = 1;
    struct node *node;
    dllist_foreach(node, distenv->node_list) {
        /* DEBUG_PRINTF("POLL: Registering node %d\n", node->id); */
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
        DEBUG_PERROR("accept");
        exit(1);
    }

    if (send(cfd, &distenv->self_id, sizeof(int), 0) < 0) {
        DEBUG_PERROR("send self node id");
        exit(1);
    }
    int bufsize = distenv->node_list->size * PACKED_NODE_SIZE;
    if (send(cfd, &bufsize, sizeof(int), 0) < 0) {
        DEBUG_PERROR("send node list len");
        exit(1);
    }
    DEBUG_PRINTF("sent node_list bufsize: %d\n", bufsize);
    if (bufsize > 0) {
        uint8_t *buffer = pack_nodes(distenv->node_list);
        int sent = send(cfd, buffer, bufsize, 0);
        /* TODO */
        if (sent < bufsize) {
            DEBUG_FPRINTF(stderr, "SENT LESS THAN BUFSIZE!!!\n");
            exit(2);
        }
        if (sent < 0) {
            DEBUG_PERROR("send node list");
            exit(1);
        }
        free(buffer);
    }

    struct packed_node n;
    if (recv(cfd, &n, PACKED_NODE_SIZE, MSG_WAITALL) < 0) {
        DEBUG_PERROR("recv node info");
        exit(1);
    }
    struct node node;
    memset(&node, 0, sizeof(struct node));
    node.id = n.id;
    node.sfd = cfd;
    node.saddr.sin_family = AF_INET;
    node.saddr.sin_addr.s_addr = n.ip;
    node.saddr.sin_port = n.port;
    add_node(distenv->node_list, &node);
    DEBUG_PRINTF("New node list after accepting a node:\n");
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

void node_disconnect(struct dllist *list, struct node *node)
{
    DEBUG_FPRINTF(stderr, "Node %d disconnected\n", node->id);
    dllist_delete(list, node);
}
 
void event_loop(struct distenv *distenv)
{
    int nfds = refresh_pollfds(distenv);
    while (1) {
        int selected = poll(distenv->pfds, nfds, -1);
        if (selected < 0) {
            if (errno == EINTR) {
                DEBUG_PRINTF("EINTR caught on poll, restarting...\n");
                continue;
            }
            DEBUG_PERROR("poll");
            exit(1);
        }
        DEBUG_PRINTF("poll returned %d: ", selected);
        char *str = str_revents(distenv->pfds[0].revents);
        if (str) {
            printf("s: %s; ", str);
            free(str);
        }
        struct node *node;
        dllist_foreach(node, distenv->node_list) {
            char *str = str_revents(node->pfd->revents);
            if (str) {
                printf("%d: %s; ", node->id, str);
                free(str);
            }
        }
        printf("\n");
        dllist_foreach(node, distenv->node_list) {
            DEBUG_PRINTF("node: %p\n", node);
            DEBUG_PRINTF("node->pfd: %p\n", node->pfd);
            str = str_revents(node->pfd->revents);
            if (str) {
                DEBUG_PRINTF("node %d revents: %s\n", node->id, str);
                free(str);
            }
            if (node->pfd->revents & POLLIN) {
                if (handle_message(distenv, node) < 0) {
                    node_disconnect(distenv->node_list, node);
                    nfds = refresh_pollfds(distenv);
                }
                DEBUG_PRINTF("After handle_message\n");
            }
        }
        if (distenv->pfds[0].revents & POLLIN) {
            accept_node(distenv);
            nfds = refresh_pollfds(distenv);
        }
    }
}
