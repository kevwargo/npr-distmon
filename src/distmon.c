#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
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
#include "debug.h"

struct distenv *global_distenv = NULL;


int parse_cmdline(int argc, char **argv, struct sockaddr_in *bind_addr, struct sockaddr_in *conn_addr)
{
    if (argc < 2) {
        
        exit(1);
    }
    DEBUG_PRINTF("Bind address: %s", argv[1]);
    socket_parseaddr(argv[1], bind_addr);
    if (argc > 2) {
        DEBUG_PRINTF("Connect address: %s", argv[2]);
        socket_parseaddr(argv[2], conn_addr);
        return 1;
    }
    return 0;
}


int distmond(int argc, char **argv)
{
    init_log(argc, argv);
    global_distenv = distenv_init(argc > 1 ? argv[1] : NULL, argc > 2 ? argv[2] : NULL);
    if (! global_distenv) {
        DEBUG_FPRINTF(stderr, "Usage: %s bind_host:bind_port [connect_host:connect_port]\n", argv[0]);
        return 1;
    }

    DEBUG_PRINTF("self.id = %d", global_distenv->self_id);
    print_nodes(global_distenv->node_list);

    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_flags = SA_RESTART;
    sa.sa_handler = log_nodes;
    if (sigaction(SIGUSR1, &sa, NULL) < 0) {
        DEBUG_PERROR("sigaction USR1");
    }

    kill(getppid(), SIGTERM);

    event_loop(global_distenv);

    return 0;
}

void watcher_term(int signum)
{
    printf("distmon daemon is running in background\n");
    exit(0);
}

int main(int argc, char **argv)
{
    if (fork()) {
        signal(SIGTERM, watcher_term);
        wait(NULL);
    } else {
        strcpy(argv[0], "distmond");
        return distmond(argc, argv);
    }
}
