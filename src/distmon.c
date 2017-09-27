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
#include "cmdline.h"


struct distenv *global_distenv = NULL;

int distmon_start(int argc, char **argv, pthread_t *thread)
{
    struct cmdline_options options;

    if (parse_cmdline(argc, argv, &options) < 0) {
        fprintf(stderr, "Usage: %s [-i] [-o stdout_file] [-e stderr_file] [bind_host:]bind_port [[connect_host:]connect_port]", argv[0]);
        return -1;
    }

    init_log(&options);
    
    global_distenv = distenv_init(options.bind_param, options.conn_param);
    if (! global_distenv) {
        DEBUG_FPRINTF(stderr, "Usage: %s [-i] [-o stdout_file] [-e stderr_file] [bind_host:]bind_port [[connect_host:]connect_port]", argv[0]);
        return -1;
    }

    DEBUG_PRINTF("Discovered id: %d", global_distenv->self_id);
    print_nodes(global_distenv->node_list);

    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_flags = SA_RESTART;
    sa.sa_handler = log_nodes;
    if (sigaction(SIGUSR1, &sa, NULL) < 0) {
        DEBUG_PERROR("sigaction USR1");
    }

    if (pthread_create(thread, NULL, (void *(*)(void *))event_loop, global_distenv) < 0) {
        DEBUG_PERROR("pthread_create");
        return -1;
    }

    return options.interactive;
}
