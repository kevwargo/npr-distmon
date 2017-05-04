#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <execinfo.h>
#include "node.h"
#include "socket.h"
#include "events.h"
#include "distenv.h"

struct distenv *global_distenv = NULL;
char *log_prefix = NULL;

void debug_printf(char *fmt, ...)
{
    char *buf = malloc(strlen(fmt) + strlen(log_prefix) + 20);
    strcpy(buf, log_prefix);
    strcat(buf, fmt);
    va_list args;
    va_start(args, fmt);
    vprintf(buf, args);
    va_end(args);
    free(buf);
}
void debug_fprintf(FILE *fp, char *fmt, ...)
{
    char *buf = malloc(strlen(fmt) + strlen(log_prefix) + 20);
    strcpy(buf, log_prefix);
    strcat(buf, fmt);
    va_list args;
    va_start(args, fmt);
    vfprintf(fp, buf, args);
    va_end(args);
    free(buf);
}
void debug_perror(char *msg)
{
    char *buf = malloc(strlen(msg) + strlen(log_prefix) + 20);
    strcpy(buf, log_prefix);
    strcat(buf, msg);
    perror(buf);
    free(buf);
}


int parse_cmdline(int argc, char **argv, struct sockaddr_in *bind_addr, struct sockaddr_in *conn_addr)
{
    if (argc < 2) {
        
        exit(1);
    }
    debug_printf("Bind address: %s\n", argv[1]);
    socket_parseaddr(argv[1], bind_addr);
    if (argc > 2) {
        debug_printf("Connect address: %s\n", argv[2]);
        socket_parseaddr(argv[2], conn_addr);
        return 1;
    }
    return 0;
}

void log_nodes(int signum)
{
    char logname[127];
    snprintf(logname, 127, "nodes-%03d.log", global_distenv->self_id);
    FILE *logfile = fopen(logname, "w+");
    if (! logfile) {
        debug_perror("log_nodes: fopen logfile");
        return;
    }
    struct sockaddr_in bind_addr;
    socklen_t addrlen;
    if (getsockname(global_distenv->self_sock, (struct sockaddr *)&bind_addr, &addrlen) < 0) {
        debug_perror("getsockname");
        return;
    }
    debug_fprintf(logfile, "%d %s:%d\n", global_distenv->self_id, inet_ntoa(bind_addr.sin_addr), ntohs(bind_addr.sin_port));
    struct node *node;
    dllist_foreach(node, global_distenv->node_list) {
        debug_fprintf(logfile, "%d %s:%d\n", node->id, inet_ntoa(node->saddr.sin_addr), ntohs(node->saddr.sin_port));
    }
    fclose(logfile);
}

void on_term(int signum)
{
    char filename[32];
    struct sockaddr_in addr;
    socklen_t addr_len;
    if (getsockname(global_distenv->self_sock, (struct sockaddr *)&addr, &addr_len) < 0) {
        perror("signal: getsockname");
    }
    snprintf(filename, 32, "core-%d.log", addr.sin_port);
    int f = open(filename, O_WRONLY | O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO);
    if (f < 0) {
        perror("signal: open");
        exit(1);
    }
    void *stack[32];
    size_t stack_size = backtrace(stack, 32);
    if (dprintf(f, "Caught %s\n", strsignal(signum)) < 0) {
        perror("signal: dprintf");
        exit(1);
    }
    backtrace_symbols_fd(stack, stack_size, f);
    if (close(f) < 0) {
        perror("signal: close");
        exit(1);
    }
    exit(0);
}

int main(int argc, char **argv)
{
    int log_prefix_len = 0;
    for (int i = 0; i < argc; i++) {
        log_prefix_len += strlen(argv[i]);
        log_prefix_len++; // space or null-byte
    }
    log_prefix_len += 20; // colon and space
    log_prefix = malloc(log_prefix_len);
    log_prefix[0] = '\0';
    for (int i = 0; i < argc; i++) {
        strcat(log_prefix, argv[i]);
        strcat(log_prefix, " ");
    }
    strcat(log_prefix, ": ");
    

    global_distenv = distenv_init(argc > 1 ? argv[1] : NULL, argc > 2 ? argv[2] : NULL);
    if (! global_distenv) {
        debug_fprintf(stderr, "Usage: %s bind_host:bind_port [connect_host:connect_port]\n\n", argv[0]);
        return 1;
    }

    debug_printf("self.id = %d\n", global_distenv->self_id);
    print_nodes(global_distenv->node_list);

    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_flags = SA_RESTART;
    sa.sa_handler = log_nodes;
    if (sigaction(SIGUSR1, &sa, NULL) < 0) {
        debug_perror("sigaction USR1");
    }

    /* sa.sa_handler = on_term; */
    /* if (sigaction(SIGTERM, &sa, NULL) < 0) { */
    /*     debug_perror("sigaction TERM"); */
    /* } */
    /* if (sigaction(SIGSEGV, &sa, NULL) < 0) { */
    /*     debug_perror("sigaction SEGV"); */
    /* } */
    signal(SIGPIPE, SIG_IGN);

    event_loop(global_distenv);

    return 0;
}
