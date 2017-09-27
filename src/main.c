#include <unistd.h>
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
        while (1) {
            sleep(1);
        }
    } else {
        DEBUG_PRINTF("Running automatically...");
        while (1) {
            sleep(1);
        }
    }
}
