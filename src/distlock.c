#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "debug.h"
#include "distenv.h"
#include "node.h"
#include "message.h"
#include "distlock.h"


static int cs_allow_entry(struct distenv *distenv, struct message *message, void *data)
{
    if (message->type == DISTLOCK_REQUEST) {
        struct distlock_request *request = (struct distlock_request *)message->data;
        if (request->timestamp > distenv->lock->timestamp) {
            distenv->lock->timestamp = request->timestamp + 1;
        } else {
            distenv->lock->timestamp++;
        }
        DEBUG_PRINTF("Received REQUEST %x from %d with timestamp %d. New timestamp is %d. Sending REPLY", message->id, message->sender->id, request->timestamp, distenv->lock->timestamp);
        if (send_message(message->sender, DISTLOCK_REPLY, 0, NULL) < 0) {
            DEBUG_FPRINTF(stderr, "Failed to send REPLY to %d", message->sender->id);
            return -1;
        }
        return 1;
    }
    return 0;
}

void distlock_init(struct distenv *distenv)
{
    register_handler(distenv, cs_allow_entry, NULL);
    distenv->lock = (struct distlock *)malloc(sizeof(struct distlock));
    memset(distenv->lock, 0, sizeof(struct distlock));
    distenv->lock->timestamp = 1;
    distenv->lock->deferred = dllist_create();
}

static int match_message_type(struct distenv *distenv, struct message *message, void *data)
{
    int *type_ptr;
    dllist_foreach(type_ptr, (struct dllist *)data) {
        if (message->type == *type_ptr) {
            return 1;
        }
    }
    return 0;
}

static int cs_defer_requests(struct distenv *distenv, struct message *message, void *data)
{
    if (message->type == DISTLOCK_REQUEST) {
        DEBUG_PRINTF("Adding requesting node %d to deferred list", message->sender->id);
        dllist_append(distenv->lock->deferred, message);
        return 1;
    }
    return 0;
}

int distlock_lock(struct distenv *distenv)
{
    DEBUG_PRINTF("Preparing to enter CS...");
    unregister_handler(distenv, cs_allow_entry, NULL, 0);
    distenv->lock->timestamp++;
    struct distlock_request request;
    memset(&request, 0, sizeof(struct distlock_request));
    request.timestamp = distenv->lock->timestamp;
    struct node *node;
    dllist_foreach(node, distenv->node_list) {
        DEBUG_PRINTF("Sending REQUEST to %d", node->id);
        if (send_message(node, DISTLOCK_REQUEST, sizeof(struct distlock_request), &request) < 0) {
            DEBUG_FPRINTF(stderr, "Failed to send REQUEST to node %d", node->id);
            return -1;
        }
    }
    int replies = 0;
    struct dllist *types = dllist_create();
    int type = DISTLOCK_REQUEST;
    dllist_append(types, &type);
    type = DISTLOCK_REPLY;
    dllist_append(types, &type);
    DEBUG_PRINTF("Waiting for critical section (need to get %d replies)...", distenv->node_list->size);
    while (replies < distenv->node_list->size) {
        struct message *message = wait_for_message(distenv, match_message_type, types);
        if (! message) {
            DEBUG_FPRINTF(stderr, "Error while waiting for critical section");
            dllist_destroy(types);
            return -1;
        }
        if (message->type == DISTLOCK_REPLY) {
            replies++;
            DEBUG_PRINTF("Received REPLY %x from %d. Total replies: %d/%d", message->id, message->sender->id, replies, distenv->node_list->size);
        } else {
            struct distlock_request *request = (struct distlock_request *)message->data;
            DEBUG_PRINTF("Received REQUEST %x from %d while entering CS. (myid: %d, mytime: %d, reqtime: %d)", message->id, message->sender->id, distenv->self_id, distenv->lock->timestamp, request->timestamp);
            if (request->timestamp < distenv->lock->timestamp || (request->timestamp == distenv->lock->timestamp && message->sender->id < distenv->self_id)) {
                DEBUG_PRINTF("Sending REPLY to higher priority node %d", message->sender->id);
                if (send_message(message->sender, DISTLOCK_REPLY, 0, NULL) < 0) {
                    DEBUG_FPRINTF(stderr, "Failed to send REPLY to requesting node %d with higher priority", message->sender->id);
                    free(message);
                    dllist_destroy(types);
                    return -1;
                }
            } else {
                DEBUG_PRINTF("Adding node %d to deferred list", message->sender->id);
                dllist_append(distenv->lock->deferred, message);
            }
        }
        free(message);
    }
    dllist_destroy(types);
    DEBUG_PRINTF("Successfully entered CS");
    register_handler(distenv, cs_defer_requests, NULL);
    distenv->lock->is_held = 1;
    return 0;
}

int distlock_unlock(struct distenv *distenv)
{
    DEBUG_PRINTF("Preparing to leave CS...");
    pthread_mutex_lock(distenv->msg_buffer->mutex);
    /* DEBUG_PRINTF("after mutex lock"); */

    unregister_handler(distenv, cs_defer_requests, NULL, 0);
    distenv->lock->timestamp++;

    struct message *message;
    dllist_foreach(message, distenv->lock->deferred) {
        DEBUG_PRINTF("Sending REPLY to deferred node %d", message->sender->id);
        if (send_message(message->sender, DISTLOCK_REPLY, 0, NULL) < 0) {
            DEBUG_FPRINTF(stderr, "Failed to send REPLY to deferred node %d", message->sender->id);
            pthread_mutex_unlock(distenv->msg_buffer->mutex);
            return -1;
        }
    }
    dllist_clear(distenv->lock->deferred);

    register_handler(distenv, cs_allow_entry, NULL);

    distenv->lock->is_held = 0;
    pthread_mutex_unlock(distenv->msg_buffer->mutex);

    DEBUG_PRINTF("Left critical section (internal mutex unlocked)");
    return 0;
}

static int receive_signal(struct distenv *distenv, struct message *message, void *data)
{
    if (message->type == DISTLOCK_COND_SIGNAL) {
        struct distlock_signal *sigmsg = (struct distlock_signal *)message->data;
        char *hex = hexdump(sigmsg->buffer, sigmsg->bufsize);
        DEBUG_PRINTF("SIGNAL %x received from %d (%s), buffer:\n%s", message->id, message->sender->id, (char *)data, hex);
        free(hex);
        distenv->lock->cond->bufsize = sigmsg->bufsize;
        memcpy(distenv->lock->cond->buffer, sigmsg->buffer, sigmsg->bufsize);
        return 1;
    }
    return 0;
}

void distlock_cond_init(struct distenv *distenv, void *buffer, int bufsize)
{
    distenv->lock->cond = (struct distlock_cond *)malloc(sizeof(struct distlock_cond));
    distenv->lock->cond->bufsize = bufsize;
    distenv->lock->cond->buffer = buffer;
    register_handler(distenv, receive_signal, NULL);
}

int distlock_cond_wait(struct distenv *distenv)
{
    if (! distenv->lock->is_held) {
        DEBUG_FPRINTF(stderr, "Cannot wait on a distlock_cond outside critical section");
        return -1;
    }

    if (distlock_unlock(distenv) < 0) {
        DEBUG_FPRINTF(stderr, "Failed to leave critical section to wait for distlock_cond");
        return -1;
    }

    struct dllist *types = dllist_create();
    int type = DISTLOCK_COND_SIGNAL;
    dllist_append(types, &type);

    DEBUG_PRINTF("Waiting for signal...");
    struct message *message = wait_for_message(distenv, match_message_type, types);
    if (! message) {
        DEBUG_FPRINTF(stderr, "Error while waiting for critical section");
        dllist_destroy(types);
        return -1;
    }

    receive_signal(distenv, message, "caught from cond_wait");

    if (distlock_lock(distenv) < 0) {
        DEBUG_FPRINTF(stderr, "Failed to reenter critical section after wait for distlock_cond");
        return -1;
    }

    return 0;
}

int distlock_cond_broadcast(struct distenv *distenv)
{
    if (! distenv->lock->is_held) {
        DEBUG_FPRINTF(stderr, "Cannot broadcast on a distlock_cond outside critical section");
        return -1;
    }

    int signal_size = sizeof(struct distlock_signal) + distenv->lock->cond->bufsize;
    struct distlock_signal *sigmsg = (struct distlock_signal *)malloc(signal_size);
    sigmsg->bufsize = distenv->lock->cond->bufsize;
    memcpy(sigmsg->buffer, distenv->lock->cond->buffer, sigmsg->bufsize);
    /* char *hex = hexdump(sigmsg->buffer, sigmsg->bufsize); */
    /* DEBUG_PRINTF("Buffer to send with SIGNAL:\n%s", hex); */
    /* free(hex); */
    struct node *node;
    dllist_foreach(node, distenv->node_list) {
        DEBUG_PRINTF("Sending signal to %d", node->id);
        if (send_message(node, DISTLOCK_COND_SIGNAL, signal_size, sigmsg) < 0) {
            DEBUG_FPRINTF(stderr, "Failed to send signal to %d", node->id);
            free(sigmsg);
            return -1;
        }
    }

    free(sigmsg);
    return 0;
}
