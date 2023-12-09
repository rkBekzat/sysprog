#pragma once

#define NEED_AUTHOR 1
#define NEED_SERVER_FEED 1

enum chat_errcode {
    CHAT_ERR_INVALID_ARGUMENT = 1,
    CHAT_ERR_TIMEOUT,
    CHAT_ERR_PORT_BUSY,
    CHAT_ERR_NO_ADDR,
    CHAT_ERR_ALREADY_STARTED,
    CHAT_ERR_NOT_IMPLEMENTED,
    CHAT_ERR_NOT_STARTED,
    CHAT_ERR_SYS,
};

enum chat_events {
    CHAT_EVENT_INPUT = 1,
    CHAT_EVENT_OUTPUT = 2,
};

struct chat_message {
#if NEED_AUTHOR
    /** Author's name. */
    const char *author;
#endif
    /** 0-terminate text. */
    char *data;

    /* PUT HERE OTHER MEMBERS */
};

struct msg_queue {
    struct msg_queue_elm {
        struct chat_message *msg;
        struct msg_queue_elm *next;
    } *first, *last;
    long int num_msgs;
};

/** Free message's memory. */
void
chat_message_delete(struct chat_message *msg);

/** Convert chat_events mask to events suitable for poll(). */
int
chat_events_to_poll_events(int mask);

/** Remove all starting and ending spacing chars **/
int
chat_message_remove_spacing(struct chat_message *msg);

/** Function for sending the message **/
long int
chat_message_send (struct chat_message *msg, int socket);

/** Function for Receiving the message **/
long int
chat_message_recv (struct chat_message *msg, int socket);

void
queue_add(struct msg_queue *queue, struct chat_message *msg);

struct chat_message *
queue_pop(struct msg_queue *queue);
