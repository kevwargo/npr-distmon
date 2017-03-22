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

#define ALLOC_STEP 2


struct node {
    int sfd;
    struct sockaddr_in saddr;
};

struct node_list {
    int len;
    int allocated;
    struct node array[0];
};


void add_node(struct node_list **list_ptr, int sfd, struct sockaddr_in *saddr)
{
    if (! list_ptr) {
        return;
    }
    struct node_list *list = *list_ptr;
    if (! list) {
        *list_ptr = malloc(sizeof(struct node_list) + sizeof(struct node) * ALLOC_STEP);
        if (! *list_ptr) {
            perror("malloc");
            exit(1);
        }
        list = *list_ptr;
        list->len = 0;
        list->allocated = ALLOC_STEP;
        printf("allocated %d\n", list->allocated);
    }
    if (list->allocated <= list->len) {
        list->allocated += ALLOC_STEP;
        *list_ptr = realloc(list, sizeof(struct node_list) + sizeof(struct node) * list->allocated);
        if (! *list_ptr) {
            perror("realloc");
            exit(1);
        }
        list = *list_ptr;
        printf("reallocated %d\n", list->allocated);
    }
    list->array[list->len].sfd = sfd;
    memcpy(&list->array[list->len].saddr, saddr, sizeof(struct sockaddr_in));
    list->len++;
    printf("new len %d\n", list->len);
}

int socket_init(struct sockaddr *saddr)
{
    int optval = 1;
    int s_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (s_sock < 0) {
        perror("socket failed");
        exit(errno);
    }
    if (setsockopt(s_sock, SOL_SOCKET, SO_REUSEADDR, (char*) &optval, sizeof(optval)) != 0) {
        perror("setsockopt failed");
        exit(errno);
    }
    if (bind(s_sock, saddr, sizeof(*saddr)) != 0) {
        perror("bind failed");
        exit(errno);
    }
    if (listen(s_sock, 5) != 0) {
        perror("listen failed");
        exit(errno);
    }
    return s_sock;
}



void print_nodes(struct node_list *list)
{
    for (int i = 0; i < list->len; i++) {
        printf("%d %s:%d\n", list->array[i].sfd, inet_ntoa(list->array[i].saddr.sin_addr), ntohs(list->array[i].saddr.sin_port));
    }
    printf("\n");
}

int parse_port(const char *port_string)
{
    char *endptr = NULL;
    long long p = strtoll(port_string, &endptr, 10);
    if (*endptr == '\0') {
        if (p >= 0 && p <= 65535) {
            return (short) p;
        }
        return -1;
    }
    return -1;
}


void parse_cmdline(int argc, char **argv, int *sport, struct addrinfo **forward_addr)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s bind_port [forward_address forward_port]\n\n", argv[0]);
        exit(1);
    }

    if (argc > 3) {
        struct addrinfo hints;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        int ret = getaddrinfo(argv[2], argv[3], &hints, forward_addr);
        if (ret != 0) {
            fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(ret));
            exit(ret);
        }
        struct sockaddr_in *fsin = (struct sockaddr_in *)((*forward_addr)->ai_addr);
        printf("Using forward server %s:%hu\n", inet_ntoa(fsin->sin_addr), ntohs(fsin->sin_port));
    }
    if (argc > 1) {
        *sport = parse_port(argv[1]);
        if (*sport < 0) {
            fprintf(stderr, "Invalid port number: %s\n", argv[1]);
            exit(1);
        }
        printf("Using custom port number %hu\n", *sport);
    } else {
        printf("Using default port number %hu\n", *sport);
    }
}


int main(int argc, char **argv)
{
    int sfd;
    struct sockaddr_in saddr;
    struct addrinfo *fwd_addr = NULL;
    int sport;

    parse_cmdline(argc, argv, &sport, &fwd_addr);

    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = 0x0100007f;
    saddr.sin_port = htons(sport);

    sfd = socket_init((struct sockaddr *) &saddr);

    struct node_list *node_list = malloc(sizeof(struct node_list));
    node_list->len = 0;
    node_list->allocated = 0;

    if (fwd_addr) {
        printf("Connecting to %s:%d\n", argv[2], sport);
        int ffd = socket(PF_INET, SOCK_STREAM, 0);
        if (ffd < 0) {
            perror("forward: socket");
            return 1;
        }
        if (connect(ffd, fwd_addr->ai_addr, fwd_addr->ai_addrlen) < 0) {
            perror("connect");
            return 1;
        }
        add_node(&node_list, ffd, (struct sockaddr_in *)fwd_addr->ai_addr);
        if (send(ffd, &saddr, sizeof(struct sockaddr_in), 0) < 0) {
            perror("send");
            return 1;
        }
    }

    while (1) {
        struct sockaddr_in caddr;
        socklen_t caddr_len = sizeof(struct sockaddr_in);
        int cfd = accept(sfd, (struct sockaddr *)&caddr, &caddr_len);
        if (cfd < 0) {
            perror("accept");
            return 1;
        }

        struct sockaddr_in c_saddr;
        if (recv(cfd, &c_saddr, sizeof(struct sockaddr_in), MSG_WAITALL) < 0) {
            perror("recv");
            return 1;
        }
        add_node(&node_list, cfd, &c_saddr);
        print_nodes(node_list);
    }
}
