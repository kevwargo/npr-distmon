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


int message_handler(struct distenv *distenv, struct message *message);

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


int distmond(int argc, char **argv, pthread_t *thread)
{
    init_log(argc, argv);
    global_distenv = distenv_init(argc > 1 ? argv[1] : NULL, argc > 2 ? argv[2] : NULL);
    if (! global_distenv) {
        DEBUG_FPRINTF(stderr, "Usage: %s [bind_host:]bind_port [[connect_host:]connect_port]", argv[0]);
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

    register_handler(global_distenv, message_handler);

    if (thread) {
        return pthread_create(thread, NULL, (void *(*)(void *))event_loop, global_distenv);
    }

    kill(getppid(), SIGTERM);
    event_loop(global_distenv);

    return 0;
}

void watcher_term(int signum)
{
    DEBUG_PRINTF("distmon daemon is running in background");
    exit(0);
}

int main(int argc, char **argv)
{
    int interactive = 0;
    if (argc > 1) {
        if (strcmp(argv[1], "-i") == 0) {
            argc--;
            argv++;
            interactive = 1;
        }
    }
    if (interactive) {
        pthread_t distmon_thread;
        if (distmond(argc, argv, &distmon_thread) < 0) {
            DEBUG_PERROR("pthread_create");
            return 1;
        }
        while (1) {
            char *line = NULL;
            size_t len = 0;
            if (getline(&line, &len, stdin) < 0) {
                DEBUG_PERROR("getline");
                return 1;
            }
            char *text = strchr(line, ' ');
            int node_id = 0;
            if (text) {
                *text = '\0';
                text++;
                node_id = atoi(line);
            } else {
                text = line;
            }
            struct node *n;
            dllist_foreach(n, global_distenv->node_list) {
                if (n->id == node_id || node_id == 0) {
                    if (send_message(n, 1, strlen(text), text) < 0) {
                        DEBUG_PERROR("send_message");
                        return 1;
                    }
                }
            }
        }
        return 0;
    } else {
        if (fork()) {
            signal(SIGTERM, watcher_term);
            wait(NULL);
        } else {
            strcpy(argv[0], "distmond");
            return distmond(argc, argv, NULL);
        }
    }
}

int message_handler(struct distenv *distenv, struct message *message)
{
    DEBUG_PRINTF("message_handler: type %d, len %d: %s", message->type, message->len, message->data);
    return 1;
}
