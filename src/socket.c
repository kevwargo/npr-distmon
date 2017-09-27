#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "debug.h"

#define SOCK_LISTEN_QUEUE 5


int socket_initbind(struct sockaddr_in *saddr)
{
    int optval = 1;
    int sfd = socket(PF_INET, SOCK_STREAM, 0);
    if (sfd < 0) {
        DEBUG_PERROR("server socket");
        exit(1);
    }
    if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, (char *) &optval, sizeof(optval)) != 0) {
        DEBUG_PERROR("setsockopt");
        exit(1);
    }
    if (bind(sfd, (struct sockaddr *)saddr, sizeof(struct sockaddr_in)) != 0) {
        DEBUG_PERROR("bind");
        exit(1);
    }
    if (listen(sfd, SOCK_LISTEN_QUEUE) != 0) {
        DEBUG_PERROR("listen");
        exit(1);
    }
    return sfd;
}

int socket_initconn(struct sockaddr_in *saddr)
{
    int sfd = socket(PF_INET, SOCK_STREAM, 0);
    if (sfd < 0) {
        DEBUG_PERROR("client socket");
        exit(1);
    }
    if (connect(sfd, (struct sockaddr *)saddr, sizeof(struct sockaddr_in)) < 0) {
        DEBUG_PERROR("connect");
        exit(1);
    }
    return sfd;
}

int socket_parseaddr(const char *s, struct sockaddr_in *saddr)
{
    char *addr = strdup(s);
    if (! addr) {
        DEBUG_PERROR("strdup");
        return -1;
    }
    char *ip;
    char *port = strchr(addr, ':');
    if (! port) {
        ip = "localhost";
        port = addr;
    } else {
        ip = addr;
        *port = '\0';
        port++;
    }

    struct addrinfo hints;
    struct addrinfo *ai = NULL;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    int ret = getaddrinfo(ip, port, &hints, &ai);
    free(addr);
    if (ret != 0) {
        DEBUG_FPRINTF(stderr, "getaddrinfo error: %s", gai_strerror(ret));
        return -1;
    }
    memcpy(saddr, ai->ai_addr, sizeof(struct sockaddr_in));
    freeaddrinfo(ai);
    return 0;
}

size_t sendall(int socket, void *buffer, size_t length)
{
    size_t total = 0;
    while (length > 0) {
        size_t chunksize = send(socket, buffer + total, length, 0);
        if (chunksize < 0) {
            DEBUG_PERROR("send");
            return chunksize;
        }
        total += chunksize;
        length -= chunksize;
    }
    return total;
}
