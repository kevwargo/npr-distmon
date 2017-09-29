#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "distmon.h"
#include "distlock.h"
#include "debug.h"

int main(int argc, char **argv)
{
    pthread_t distmon_thread;
    int interactive = distmon_start(argc, argv, &distmon_thread);

    if (interactive < 0) {
        fprintf(stderr, "Failed to start distmon\n");
        return 1;
    }

    struct {
        int len;
        int put_pos;
        int get_pos;
        int data[10];
    } buffer;
    memset(&buffer, 0, sizeof(buffer));
    distlock_cond_init(global_distenv, &buffer, sizeof(buffer));

    if (interactive) {
        DEBUG_PRINTF("Running interactively...");
        char *line = NULL;
        size_t size = 0;
        size_t length = 0;
        while ((length = getline(&line, &size, stdin)) > 0) {
            if (line[length - 1] == '\n') {
                line[length - 1] = '\0';
            }
            if (strcmp(line, "lock") == 0) {
                distlock_lock(global_distenv);
            } else if (strcmp(line, "unlock") == 0) {
                distlock_unlock(global_distenv);
            } else if (strncmp(line, "put ", 4) == 0) {
                distlock_lock(global_distenv);
                while (buffer.len >= 10) {
                    DEBUG_PRINTF("No space (%d), waiting for elements remove...", buffer.len);
                    distlock_cond_wait(global_distenv);
                }
                int val = atoi(line + 4);
                buffer.data[buffer.put_pos] = val;
                buffer.len++;
                DEBUG_PRINTF("Put %d at %d. Len: %d", val, buffer.put_pos, buffer.len);
                buffer.put_pos = (buffer.put_pos + 1) % 10;
                distlock_cond_broadcast(global_distenv);
                distlock_unlock(global_distenv);
            } else if (strcmp(line, "get") == 0) {
                distlock_lock(global_distenv);
                while (buffer.len <= 0) {
                    DEBUG_PRINTF("To few elements: %d. Waiting...", buffer.len);
                    distlock_cond_wait(global_distenv);
                }
                int val = buffer.data[buffer.get_pos];
                buffer.len--;
                DEBUG_PRINTF("Got %d from %d. Len: %d", val, buffer.put_pos, buffer.len);
                buffer.get_pos = (buffer.get_pos + 1) % 10;
                distlock_cond_broadcast(global_distenv);
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
