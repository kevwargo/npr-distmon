#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "debug.h"

int __argc_;
char **__argv_;


void log_nodes(int signum)
{
    char logname[127];
    sprintf(logname, "nodes-%03d.log", global_distenv->self_id);
    FILE *logfile = fopen(logname, "w+");
    if (! logfile) {
        DEBUG_PERROR("log_nodes: fopen logfile");
        return;
    }
    struct sockaddr_in bind_addr;
    socklen_t addrlen;
    if (getsockname(global_distenv->self_sock, (struct sockaddr *)&bind_addr, &addrlen) < 0) {
        DEBUG_PERROR("getsockname");
        return;
    }
    DEBUG_FPRINTF(logfile, "%d %s:%d", global_distenv->self_id, inet_ntoa(bind_addr.sin_addr), ntohs(bind_addr.sin_port));
    struct node *node;
    dllist_foreach(node, global_distenv->node_list) {
        DEBUG_FPRINTF(logfile, "%d %s:%d", node->id, inet_ntoa(node->saddr.sin_addr), ntohs(node->saddr.sin_port));
    }
    fclose(logfile);
}

void init_log(int argc, char **argv)
{
    __argc_ = argc;
    __argv_ = argv;
}
