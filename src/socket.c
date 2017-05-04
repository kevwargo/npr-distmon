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
        debug_perror("server socket");
        exit(1);
    }
    if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, (char *) &optval, sizeof(optval)) != 0) {
        debug_perror("setsockopt");
        exit(1);
    }
    if (bind(sfd, (struct sockaddr *)saddr, sizeof(struct sockaddr_in)) != 0) {
        debug_perror("bind");
        exit(1);
    }
    if (listen(sfd, SOCK_LISTEN_QUEUE) != 0) {
        debug_perror("listen");
        exit(1);
    }
    return sfd;
}

int socket_initconn(struct sockaddr_in *saddr)
{
    int sfd = socket(PF_INET, SOCK_STREAM, 0);
    if (sfd < 0) {
        debug_perror("client socket");
        exit(1);
    }
    if (connect(sfd, (struct sockaddr *)saddr, sizeof(struct sockaddr_in)) < 0) {
        debug_perror("connect");
        exit(1);
    }
    return sfd;
}

int socket_parseaddr(const char *s, struct sockaddr_in *saddr)
{
    char *ip = strdup(s);
    if (! ip) {
        debug_perror("strdup");
        return -1;
    }
    char *port = strchr(ip, ':');
    if (! port) {
        debug_fprintf(stderr, "Socket address must be of format HOST:PORT\n");
        return -1;
    }
    *port = '\0';
    port++;

    struct addrinfo hints;
    struct addrinfo *ai = NULL;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    int ret = getaddrinfo(ip, port, &hints, &ai);
    if (ret != 0) {
        debug_fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(ret));
        return -1;
    }
    memcpy(saddr, ai->ai_addr, sizeof(struct sockaddr_in));
    freeaddrinfo(ai);
    return 0;
}
