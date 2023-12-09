#include "chat.h"

#include <poll.h>
#include <stdlib.h>

#include <stdio.h>
#include <sys/socket.h>
#include <string.h>

void
chat_message_delete(struct chat_message *msg)
{
    if (msg->data != NULL) free(msg->data);
    free(msg);
}

int
chat_events_to_poll_events(int mask)
{
    int res = 0;
    if ((mask & CHAT_EVENT_INPUT) != 0)
        res |= POLLIN;
    if ((mask & CHAT_EVENT_OUTPUT) != 0)
        res |= POLLOUT;
    return res;
}

int
chat_message_remove_spacing(struct chat_message *msg)
{
    if (msg->data == NULL) return 0;

    size_t s_pos = 0; // Start position of msg
    size_t e_pos = strlen(msg->data); // End position of msg
    char ch;

    // Remove all starting space chars
    for (; s_pos < e_pos; s_pos++) {
        ch = msg->data[s_pos];
        if (ch != ' ' && ch != '\n' && ch != '\t' && ch != '\v' && ch != '\f' && ch != '\r') break;
    }

    // Remove all ending space chars
    for (; e_pos > s_pos; e_pos--) {
        ch = msg->data[e_pos - 1];
        if (ch != ' ' && ch != '\n' && ch != '\t' && ch != '\v' && ch != '\f' && ch != '\r') break;
    }

    // Check is the result length is zero
    e_pos -= s_pos;
    if (e_pos == 0) return 0;

    // Create and copy the new message
    char *new_msg = malloc(e_pos + 1);
    memcpy(new_msg, msg->data + s_pos, e_pos);
    new_msg[e_pos] = 0;

    // Replace old message with new one
    free(msg->data);
    msg->data = new_msg;

    // Return resulting size of the new messsage
    return e_pos;
}


// to manage the send/write data, first I send the size of data.
// the size will be written on char with 4 values, cause multiply of 4 bytes give us int
int
send_sizes(size_t sz, int socket){
    char * size_buf = malloc(sizeof (char) * 4);
    memset(size_buf, 0, 4);
    size_t pos = 0;
    while (sz > 0){
        size_buf[pos++] = (char)(sz % 256);
        sz /= 256;
    }
    pos = 0;
    int result;
    while(pos < 4){
        if ((result = (int) send(socket, size_buf + pos, 4 - pos, 0)) <= 0) {
            free(size_buf);
            return result;
        }
        pos += result;
    }
    free(size_buf);
    return pos;
}

int
send_data(const char * msg, int socket){
    size_t size = 0;
    int result;
    while (size != strlen(msg)) {
        if ((result = (int) send(socket, msg + size, strlen(msg) - size, 0)) <= 0)
            return result;
        size += result;
    }
    return size;
}

ssize_t
chat_message_send (struct chat_message *msg, int socket)
{
    int result;
    int msg_size = strlen(msg->data);

#ifdef NEED_AUTHOR
    if (msg->author != NULL) {
        result = send_sizes(strlen(msg->author), socket);
        if(result <= 0){
            return result;
        }

        result = send_data(msg->author, socket);
        if(result <= 0){
            return result;
        }
	}
#endif
    result = send_sizes(msg_size, socket);
    if(result <= 0){
        return result;
    }

    result = send_data(msg->data, socket);
    if(result <= 0){
        return result;
    }
    return result;
}

ssize_t
chat_message_recv (struct chat_message *msg, int socket)
{
    char * size_buf = malloc(sizeof (char ) * 4);
    memset(size_buf, 0, 4);
    int msg_size = 0;
    int size = 0;
    int result;

    size = 0;
    while (size < 4) {
        if ((result = (int) recv(socket, size_buf + size, 4 - size, 0)) <= 0) {
            free(size_buf);
            return result;
        }
        size += result;
    }

    int pos = 3;
    while(pos >= 0){
        msg_size *= 256;
        msg_size += (unsigned int) size_buf[pos--];
    }

    if (msg->data != NULL) free(msg->data);
    msg->data = malloc(msg_size + 1);
    msg->data[msg_size] = 0;

    size = 0;
    while (size != msg_size) {
        if ((result = (int) recv(socket, msg->data + size, msg_size - size, 0)) <= 0) {
            free(size_buf);
            return result;
        }
        size += result;
    }
    free(size_buf);
    return size;
}

void
queue_add(struct msg_queue *queue, struct chat_message *msg)
{
    struct msg_queue_elm *new_elm = malloc(sizeof(struct msg_queue_elm));
    *new_elm = (struct msg_queue_elm){msg, NULL};
    if (queue->num_msgs++ == 0) queue->first = queue->last = new_elm;
    else queue->last = queue->last->next = new_elm;
}

struct chat_message *
queue_pop(struct msg_queue *queue)
{
    if (queue->num_msgs == 0) return NULL;

    struct msg_queue_elm *elm = queue->first;
    struct chat_message *res = elm->msg;

    if (--queue->num_msgs == 0) queue->first = queue->last = NULL;
    else queue->first = elm->next;

    free(elm);
    return res;
}