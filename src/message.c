#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "message.h"
#include "distenv.h"
#include "dllist.h"
#include "socket.h"
#include "node.h"
#include "debug.h"

int handle_message(struct distenv *distenv, struct node *node)
{
    int flags = 0;

    DEBUG_PRINTF("message: node: %d msg_part %p", node->id, node->msg_part);
    if (! node->msg_part) {
        struct message_header header;
        int ret = recv(node->sfd, &header, sizeof(struct message_header), 0);
        if (ret <= 0) {
            if (ret < 0) {
                DEBUG_PERROR("recv message header from a node");
            } else {
                DEBUG_FPRINTF(stderr, "Node %d disconnected while receiving message", node->id);
            }
            return -1;
        }
        node->msg_part = (struct message *)malloc(sizeof(struct message) + header.len);
        memset(node->msg_part, 0, sizeof(struct message) + header.len);
        node->msg_part->sender = node;
        node->msg_part->type = header.type;
        node->msg_part->len = header.len;
        node->msg_part->received = 0;
        flags = MSG_DONTWAIT;
    }

    unsigned char *buf = &node->msg_part->data[node->msg_part->received];
    int remaining = node->msg_part->len - node->msg_part->received;
    int received = recv(node->sfd, buf, remaining, flags);
    if (received < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            DEBUG_PERROR("recv message body from a node");
            return -1;
        }
    } else if (received == 0 && remaining > 0) {
        DEBUG_FPRINTF(stderr, "Node %d disconnected while receiving message", node->id);
        return -1;
    } else {
        node->msg_part->received += received;
        if (received == remaining) {
            pthread_mutex_lock(distenv->msg_buffer->mutex);

            int handled = 0;
            struct message_handler *handler;
            dllist_foreach(handler, distenv->msg_buffer->handlers) {
                if (handler->callback(distenv, node->msg_part, handler->data)) {
                    handled = 1;
                    break;
                }
            }

            if (! handled) {
                dllist_add(distenv->msg_buffer->queue, node->msg_part);
                pthread_cond_broadcast(distenv->msg_buffer->cond);
            }
            free(node->msg_part);
            node->msg_part = NULL;

            pthread_mutex_unlock(distenv->msg_buffer->mutex);
        }
    }

    return 0;
}

int send_message(struct node *node, int type, int len, void *buf)
{
    if (len < 0) {
        errno = EINVAL;
        return -1;
    }
    size_t size = sizeof(struct message_header) + len;
    struct message_header *msg = (struct message_header *)malloc(size);
    msg->type = type;
    msg->len = len;
    if (len > 0) {
        memcpy(msg->data, buf, len);
    }
    return sendall(node->sfd, msg, size);
}

void register_handler(struct distenv *distenv, message_callback_t callback, void *data)
{
    pthread_mutex_lock(distenv->msg_buffer->mutex);
    DEBUG_PRINTF("Looking for callback %p...", callback);
    struct message_handler *handler;
    dllist_foreach(handler, distenv->msg_buffer->handlers) {
        if (handler->callback == callback) {
            DEBUG_PRINTF("updating data for %p: %p", callback, data);
            handler->data = data;
            pthread_mutex_unlock(distenv->msg_buffer->mutex);
            return;
        }
    }
    DEBUG_PRINTF("adding new callback %p: %p", callback, data);
    struct message_handler new_handler;
    new_handler.callback = callback;
    new_handler.data = data;
    dllist_add(distenv->msg_buffer->handlers, &new_handler);
    pthread_mutex_unlock(distenv->msg_buffer->mutex);
}

void unregister_handler(struct distenv *distenv, message_callback_t callback, int free_data)
{
    pthread_mutex_lock(distenv->msg_buffer->mutex);
    DEBUG_PRINTF("Looking for callback %p (%sfreeing data)...", callback, (free_data ? "" : "not "));
    struct message_handler *handler;
    dllist_foreach(handler, distenv->msg_buffer->handlers) {
        if (handler->callback == callback) {
            if (free_data && handler->data) {
                DEBUG_PRINTF("freeing handler %p data %p...", callback, handler->data);
                free(handler->data);
            }
            dllist_delete(distenv->msg_buffer->handlers, handler);
            pthread_mutex_unlock(distenv->msg_buffer->mutex);
            return;
        }
    }
    DEBUG_PRINTF("callback %p not found", callback);
    pthread_mutex_unlock(distenv->msg_buffer->mutex);
}

struct message *wait_for_message(struct distenv *distenv, message_callback_t matcher, void *data)
{
    struct message *message;
    int found = 0;

    pthread_mutex_lock(distenv->msg_buffer->mutex);
    while (! found) {
        struct message *m;
        dllist_foreach(m, distenv->msg_buffer->queue) {
            if (matcher(distenv, m, data)) {
                found = 1;
            }
        }
        if (! found) {
            pthread_cond_wait(distenv->msg_buffer->cond, distenv->msg_buffer->mutex);
            continue;
        }
        message = (struct message *)malloc(sizeof(struct message));
        memcpy(message, m, sizeof(struct message));
        dllist_delete(distenv->msg_buffer->queue, m);
    }
    
    pthread_mutex_unlock(distenv->msg_buffer->mutex);
    return message;
}
