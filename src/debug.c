#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "debug.h"
#include "distmon.h"

char __logprefix[256];
int __errno_bak;
double __start_time;


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

void init_log(struct cmdline_options *options)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    __start_time = (double)ts.tv_sec + ((double)ts.tv_nsec / (double)(1000 * 1000 * 1000));
    snprintf(__logprefix, 256, "%s %s", options->bind_param, options->conn_param);
    if (options->stdout_file) {
        if (!freopen(options->stdout_file, "a+", stdout)) {
            DEBUG_FPRINTF(stderr, "Failed to set stdout to file %s", options->stdout_file);
        }
    }
    if (options->stderr_file) {
        if (!freopen(options->stderr_file, "a+", stderr)) {
            DEBUG_FPRINTF(stderr, "Failed to set stderr to file %s", options->stderr_file);
        }
    }
}

void hexdump(void *voidbuf, int length)
{
    unsigned char *buf = (unsigned char *)voidbuf;
    for (int i = 0; i < length; i++)
    {
        printf("%.2x", buf[i]);
        if (! (~i & 0x1f))
        {
            putchar('\n');
            continue;
        }
        else if (! (~i & 0x07))
            putchar(' ');
        if (i < length - 1)
            putchar(' ');
    }
    putchar('\n');
}
