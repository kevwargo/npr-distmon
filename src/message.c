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

struct wait_data {
    message_callback_t matcher;
    void *matcher_data;
    struct message **msgptr;
};


int handle_message(struct distenv *distenv, struct node *node)
{
    int flags = 0;

    /* DEBUG_PRINTF("message: node: %d msg_part %p", node->id, node->msg_part); */
    if (! node->msg_part) {
        struct message_header header;
        int ret = recv(node->sfd, &header, sizeof(struct message_header), 0);
        if (ret <= 0) {
            if (ret < 0) {
                DEBUG_PERROR("recv message header from a node");
            } else {
                DEBUG_PRINTF("Node %d disconnected while receiving message", node->id);
            }
            return -1;
        }
        node->msg_part = (struct message *)malloc(sizeof(struct message) + header.len);
        memset(node->msg_part, 0, sizeof(struct message) + header.len);
        node->msg_part->sender = node;
        node->msg_part->id = header.id;
        node->msg_part->type = header.type;
        node->msg_part->len = header.len;
        flags = MSG_DONTWAIT;
        /* DEBUG_PRINTF("id %x type %d len %d", header.id, header.type, header.len); */
    }

    unsigned char *buf = &node->msg_part->data[node->msg_part->received];
    int remaining = node->msg_part->len - node->msg_part->received;
    int received = 0;
    if (remaining > 0) {
        received = recv(node->sfd, buf, remaining, flags);
    }
    /* DEBUG_PRINTF("rem %d recvd %d", remaining, received); */
    if (received < 0) {
        /* DEBUG_PRINTF("< 0"); */
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            DEBUG_PERROR("recv message body from a node");
            return -1;
        }
    } else if (received == 0 && remaining > 0) {
        /* DEBUG_PRINTF("== 0"); */
        DEBUG_PRINTF("Node %d disconnected while receiving message", node->id);
        return -1;
    } else {
        /* DEBUG_PRINTF("else"); */
        node->msg_part->received += received;
        /* DEBUG_PRINTF("after"); */
        if (received == remaining) {
            /* DEBUG_PRINTF("before lock"); */
            pthread_mutex_lock(distenv->msg_buffer->mutex);
            /* DEBUG_PRINTF("after lock"); */

            int handled = 0;
            struct message_handler *handler;
            dllist_foreach(handler, distenv->msg_buffer->handlers) {
                /* DEBUG_PRINTF("testing callback %p", handler->callback); */
                int ret = handler->callback(distenv, node->msg_part, handler->data);
                if (ret < 0) {
                    DEBUG_FPRINTF(stderr, "Error in message handler %p", handler->callback);
                    pthread_mutex_unlock(distenv->msg_buffer->mutex);
                    return -1;
                } else if (ret > 0) {
                    /* DEBUG_PRINTF("handled"); */
                    handled = 1;
                    if (node->msg_part->stop_propagation) {
                        /* DEBUG_PRINTF("stop propagation"); */
                        break;
                    }
                }
            }

            if (! handled) {
                /* DEBUG_PRINTF("not handled"); */
                dllist_new(distenv->msg_buffer->queue, node->msg_part, sizeof(struct message) + node->msg_part->len, 0);
                pthread_cond_broadcast(distenv->msg_buffer->cond);
            }
            free(node->msg_part);
            node->msg_part = NULL;

            pthread_mutex_unlock(distenv->msg_buffer->mutex);
        }
        /* DEBUG_PRINTF("out of =="); */
    }

    return 0;
}

size_t send_message(struct node *node, int type, int len, void *buf)
{
    if (len < 0) {
        DEBUG_FPRINTF(stderr, "len is negative: %d", len);
        errno = EINVAL;
        return -1;
    }
    size_t size = sizeof(struct message_header) + len;
    struct message_header *msg = (struct message_header *)malloc(size);
    msg->id = rand();
    msg->type = type;
    msg->len = len;
    if (len > 0) {
        memcpy(msg->data, buf, len);
    }
    size = sendall(node->sfd, msg, size);
    DEBUG_PRINTF("Sent message with id: %x", msg->id);
    free(msg);
    return size;
}

void register_handler(struct distenv *distenv, message_callback_t callback, void *data)
{
    pthread_mutex_lock(distenv->msg_buffer->mutex);
    /* DEBUG_PRINTF("Looking for callback %p...", callback); */
    struct message_handler *handler;
    dllist_foreach(handler, distenv->msg_buffer->handlers) {
        if (handler->callback == callback && handler->data == data) {
            pthread_mutex_unlock(distenv->msg_buffer->mutex);
            return;
        }
    }
    /* DEBUG_PRINTF("adding new callback %p: %p", callback, data); */
    struct message_handler new_handler;
    new_handler.callback = callback;
    new_handler.data = data;
    dllist_append(distenv->msg_buffer->handlers, &new_handler);
    pthread_mutex_unlock(distenv->msg_buffer->mutex);
}

void unregister_handler(struct distenv *distenv, message_callback_t callback, void *data, int free_data)
{
    pthread_mutex_lock(distenv->msg_buffer->mutex);
    /* DEBUG_PRINTF("Looking for callback %p (%sfreeing data)...", callback, (free_data ? "" : "not ")); */
    struct message_handler *handler;
    dllist_foreach(handler, distenv->msg_buffer->handlers) {
        if (handler->callback == callback && handler->data == data) {
            if (free_data && handler->data) {
                /* DEBUG_PRINTF("freeing handler %p data %p...", callback, handler->data); */
                free(handler->data);
            }
            /* DEBUG_PRINTF("Removing callback %p", callback); */
            dllist_remove(distenv->msg_buffer->handlers, handler);
            pthread_mutex_unlock(distenv->msg_buffer->mutex);
            return;
        }
    }
    /* DEBUG_PRINTF("callback %p not found", callback); */
    pthread_mutex_unlock(distenv->msg_buffer->mutex);
}

static int wait_handler(struct distenv *distenv, struct message *message, void *data)
{
    /* DEBUG_PRINTF("start"); */
    struct wait_data *wait_data = (struct wait_data *)data;
    int ret = wait_data->matcher(distenv, message, wait_data->matcher_data);
    if (ret < 0) {
        DEBUG_PRINTF("Matcher %p failed", wait_data->matcher);
        return -1;
    } else if (ret > 0) {
        /* DEBUG_PRINTF("match"); */
        int msg_size = sizeof(struct message) + message->len;
        while (*(wait_data->msgptr)) {
            /* DEBUG_PRINTF("before wait"); */
            pthread_cond_wait(distenv->msg_buffer->cond, distenv->msg_buffer->mutex);
            /* DEBUG_PRINTF("after wait"); */
        }
        *(wait_data->msgptr) = (struct message *)malloc(msg_size);
        memcpy(*(wait_data->msgptr), message, msg_size);
        message->stop_propagation = 1;
        pthread_cond_broadcast(distenv->msg_buffer->cond);
        return 1;
    }
    return 0;
}

struct message *wait_for_message(struct distenv *distenv, message_callback_t matcher, void *data)
{
    struct message *message = NULL;
    int found = 0;

    pthread_mutex_lock(distenv->msg_buffer->mutex);
    pthread_cond_broadcast(distenv->msg_buffer->cond);
    /* DEBUG_PRINTF("after mutex lock"); */

    struct wait_data wait_data;
    wait_data.matcher = matcher;
    wait_data.matcher_data = data;
    wait_data.msgptr = &message;

    struct message_handler handler;
    handler.callback = wait_handler;
    handler.data = &wait_data;
    struct message_handler *hp = dllist_prepend(distenv->msg_buffer->handlers, &handler);
    /* DEBUG_PRINTF("prepended %p", wait_handler); */

    while (! found && ! message) {
        /* DEBUG_PRINTF("! found && ! message"); */
        struct message *m;
        dllist_foreach(m, distenv->msg_buffer->queue) {
            int ret = matcher(distenv, m, data);
            if (ret < 0) {
                DEBUG_FPRINTF(stderr, "Message matcher %p failed", matcher);
                dllist_remove(distenv->msg_buffer->handlers, &handler);
                pthread_mutex_unlock(distenv->msg_buffer->mutex);
                return NULL;
            } else if (ret > 0) {
                /* DEBUG_PRINTF("found"); */
                found = 1;
                break;
            }
        }
        if (! found) {
            /* DEBUG_PRINTF("before wait"); */
            pthread_cond_wait(distenv->msg_buffer->cond, distenv->msg_buffer->mutex);
            /* DEBUG_PRINTF("after wait"); */
            continue;
        }
        int size = sizeof(struct message) + m->len;
        message = (struct message *)malloc(size);
        memcpy(message, m, size);
        dllist_remove(distenv->msg_buffer->queue, m);
    }

    /* DEBUG_PRINTF("found = %d, message = %p, dllist = %d", found, message, distenv->msg_buffer->handlers->size); */

    dllist_remove(distenv->msg_buffer->handlers, hp);
    /* DEBUG_PRINTF("dllist = %d", distenv->msg_buffer->handlers->size); */
    pthread_mutex_unlock(distenv->msg_buffer->mutex);
    /* DEBUG_PRINTF("after mutex unlock"); */
    return message;
}
