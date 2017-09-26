#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <stdio.h>
#include <pthread.h>
#include "message.h"
#include "distenv.h"
#include "dllist.h"
#include "node.h"
#include "debug.h"

int handle_message(struct distenv *distenv, struct node *node)
{
    struct message *message;
    int flags = 0;
    
    pthread_mutex_lock(distenv->msg_buffer->mutex);

    DEBUG_PRINTF("message: node: %d msg_part %p", node->id, node->msg_part);
    if (node->msg_part) {
        message = node->msg_part;
    } else {
        struct {
            int type;
            int len;
        } header;
        int ret = recv(node->sfd, &header, sizeof(header), 0);
        if (ret <= 0) {
            if (ret < 0) {
                DEBUG_PERROR("recv message header from a node");
            } else {
                DEBUG_FPRINTF(stderr, "Node %d disconnected while receiving message", node->id);
            }
            goto _handle_message_error;
        }
        message = __dllist_add(distenv->msg_buffer->list, NULL, sizeof(struct message) + header.len);
        message->sender_id = node->id;
        message->type = header.type;
        message->len = header.len;
        message->received = 0;
        flags = MSG_DONTWAIT;
    }

    unsigned char *buf = &message->data[message->received];
    int remaining = message->len - message->received;
    int received = recv(node->sfd, buf, remaining, flags);
    if (received < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            DEBUG_PERROR("recv message body from a node");
            dllist_delete(distenv->msg_buffer->list, message);
            goto _handle_message_error;
        }
    } else if (received == 0) {
        DEBUG_FPRINTF(stderr, "Node %d disconnected while receiving message", node->id);
        goto _handle_message_error;
    } else {
        message->received += received;
        if (received == remaining) {
            node->msg_part = NULL;
            pthread_cond_broadcast(distenv->msg_buffer->cond);
        } else {
            node->msg_part = message;
        }
    }

    pthread_mutex_unlock(distenv->msg_buffer->mutex);
    return 0;

_handle_message_error:
    pthread_mutex_unlock(distenv->msg_buffer->mutex);
    return -1;
}
