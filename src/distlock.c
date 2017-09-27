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
        struct distlock_msg *request = (struct distlock_msg *)message->data;
        if (request->timestamp > distenv->lock->timestamp) {
            distenv->lock->timestamp = request->timestamp + 1;
        } else {
            distenv->lock->timestamp++;
        }
        DEBUG_PRINTF("Received REQUEST from %d with timestamp %d. New timestamp is %d", message->sender->id, request->timestamp, distenv->lock->timestamp);
        request->timestamp = distenv->lock->timestamp;
        if (send_message(message->sender, DISTLOCK_REPLY, sizeof(struct distlock_msg), request) < 0) {
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

static int match_request_reply(struct distenv *distenv, struct message *message, void *data)
{
    switch (message->type) {
        case DISTLOCK_REQUEST:
        case DISTLOCK_REPLY:
            return 1;
        default:
            return 0;
    }
}

static int cs_defer_requests(struct distenv *distenv, struct message *message, void *data)
{
    if (message->type == DISTLOCK_REQUEST) {
        DEBUG_PRINTF("Adding requesting node %d to deferred list", message->sender->id);
        dllist_add(distenv->lock->deferred, message);
        return 1;
    }
    return 0;
}

int distlock_lock(struct distenv *distenv)
{
    DEBUG_PRINTF("Preparing to enter CS...");
    unregister_handler(distenv, cs_allow_entry, 0);
    distenv->lock->timestamp++;
    struct distlock_msg dlmsg;
    memset(&dlmsg, 0, sizeof(struct distlock_msg));
    dlmsg.timestamp = distenv->lock->timestamp;
    struct node *node;
    dllist_foreach(node, distenv->node_list) {
        DEBUG_PRINTF("Sending REQUEST to %d", node->id);
        if (send_message(node, DISTLOCK_REQUEST, sizeof(struct distlock_msg), &dlmsg) < 0) {
            DEBUG_FPRINTF(stderr, "Error while sending critical section request to node %d", node->id);
            return -1;
        }
    }
    int replies = 0;
    DEBUG_PRINTF("Waiting for critical section (need to get %d replies)...", distenv->node_list->size);
    while (replies < distenv->node_list->size) {
        struct message *message = wait_for_message(distenv, match_request_reply, NULL);
        if (! message) {
            DEBUG_FPRINTF(stderr, "Error while waiting for critical section");
            return -1;
        }
        struct distlock_msg *dlmsg_p = (struct distlock_msg *)message->data;
        if (message->type == DISTLOCK_REPLY) {
            // TODO
            // maybe add some check for old replies...
            replies++;
            DEBUG_PRINTF("Received REPLY from %d. Total replies: %d/%d", message->sender->id, replies, distenv->node_list->size);
        } else {
            DEBUG_PRINTF("Received REQUEST while entering CS. (myid: %d, mytime: %d, reqtime: %d)", distenv->self_id, distenv->lock->timestamp, dlmsg_p->timestamp);
            if (dlmsg_p->timestamp < distenv->lock->timestamp || (dlmsg_p->timestamp == distenv->lock->timestamp && message->sender->id < distenv->self_id)) {
                DEBUG_PRINTF("Sending REPLY to higher priority node %d", message->sender->id);
                dlmsg_p->timestamp = distenv->lock->timestamp;
                if (send_message(message->sender, DISTLOCK_REPLY, sizeof(struct distlock_msg), dlmsg_p) < 0) {
                    DEBUG_FPRINTF(stderr, "Error while sending reply to higher priority requesting node %d", message->sender->id);
                    return -1;
                }
                DEBUG_PRINTF("Sent REPLY to higher priority node %d", message->sender->id);
            } else {
                DEBUG_PRINTF("Adding node %d to deferred list", message->sender->id);
                dllist_add(distenv->lock->deferred, message);
            }
        }
    }
    DEBUG_PRINTF("Successfully entered CS");
    register_handler(distenv, cs_defer_requests, NULL);
    return 0;
}

int distlock_unlock(struct distenv *distenv)
{
    DEBUG_PRINTF("Preparing to unlock CS...");
    pthread_mutex_lock(distenv->msg_buffer->mutex);
    DEBUG_PRINTF("after mutex lock");

    unregister_handler(distenv, cs_defer_requests, 0);
    distenv->lock->timestamp++;

    struct message *message;
    struct distlock_msg dlmsg;
    memset(&dlmsg, 0, sizeof(struct distlock_msg));
    dlmsg.timestamp = distenv->lock->timestamp;
    dllist_foreach(message, distenv->lock->deferred) {
        DEBUG_PRINTF("Sending REPLY to deferred node %d with timestamp %d", message->sender->id, dlmsg.timestamp);
        if (send_message(message->sender, DISTLOCK_REPLY, sizeof(struct distlock_msg), &dlmsg) < 0) {
            DEBUG_FPRINTF(stderr, "Failed to send REPLY to deferred node %d", message->sender->id);
            pthread_mutex_unlock(distenv->msg_buffer->mutex);
            return -1;
        }
    }
    dllist_clear(distenv->lock->deferred);
    
    register_handler(distenv, cs_allow_entry, NULL);

    pthread_mutex_unlock(distenv->msg_buffer->mutex);
    DEBUG_PRINTF("CS and mutex unlocked");

    return 0;
}
