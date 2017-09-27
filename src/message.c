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
        node->msg_part->sender_id = node->id;
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
            message_handler_t *h;
            dllist_foreach(h, distenv->msg_buffer->handlers) {
                if ((*h)(distenv, node->msg_part)) {
                    handled = 1;
                    break;
                }
            }

            if (! handled) {
                dllist_add(distenv->msg_buffer->list, node->msg_part);
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

void register_handler(struct distenv *distenv, message_handler_t handler)
{
    pthread_mutex_lock(distenv->msg_buffer->mutex);
    message_handler_t *h;
    dllist_foreach(h, distenv->msg_buffer->handlers) {
        if (*h == handler) {
            pthread_mutex_unlock(distenv->msg_buffer->mutex);
            return;
        }
    }
    dllist_add(distenv->msg_buffer->handlers, &handler);
    pthread_mutex_unlock(distenv->msg_buffer->mutex);
}

void unregister_handler(struct distenv *distenv, message_handler_t handler)
{
    pthread_mutex_lock(distenv->msg_buffer->mutex);
    message_handler_t *h;
    dllist_foreach(h, distenv->msg_buffer->handlers) {
        if (*h == handler) {
            dllist_delete(distenv->msg_buffer->handlers, h);
            pthread_mutex_unlock(distenv->msg_buffer->mutex);
            return;
        }
    }
    pthread_mutex_unlock(distenv->msg_buffer->mutex);
}
