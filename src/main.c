#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "distmon.h"
#include "debug.h"

int main(int argc, char **argv)
{
    pthread_t distmon_thread;
    int interactive = distmon_start(argc, argv, &distmon_thread);

    if (interactive < 0) {
        fprintf(stderr, "Failed to start distmon\n");
        return 1;
    }

    if (interactive) {
        DEBUG_PRINTF("Running interactively...");
        char *line = NULL;
        size_t size = 0;
        size_t length = 0;
        while ((length = getline(&line, &size, stdin)) > 0) {
            if (line[length - 1] == '\n') {
                line[length - 1] = '\0';
            }
            DEBUG_PRINTF("line: %s\n", line);
            if (strcmp(line, "lock") == 0) {
                distlock_lock(global_distenv);
            } else if (strcmp(line, "unlock") == 0) {
                distlock_unlock(global_distenv);
            }
        }
    } else {
        DEBUG_PRINTF("Running automatically...");
        while (1) {
            sleep(2);
            
        }
    }
}
