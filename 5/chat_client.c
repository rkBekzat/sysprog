#include "chat.h"
#include "chat_client.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include "sys/socket.h"


struct chat_client {
	/** Socket connected to the server. */
    int socket;
    int poll;
    /** Name related values **/
    char *name;
    int name_sended;
    /** Array of received messages and messages to send */
    struct msg_queue recved, send;
    /* ... */
    /** Output buffer. */
    /* ... */

    struct msg_buff {
        char *buff;
        size_t pos;
        size_t size;
    } msg_buff;

#ifdef NEED_AUTHOR
    struct cli_authors {
        char **names;
        size_t count;
    } authors;
#endif

    /* PUT HERE OTHER MEMBERS */
};


#ifdef NEED_AUTHOR
const char *
find_or_add_authors(struct cli_authors *authors, struct chat_message *msg)
{
    char *ptr_str = NULL;

    for (size_t i = 0; i < authors->count && ptr_str == NULL; i++)
        if (strcmp(authors->names[i], msg->data) == 0)
            ptr_str = authors->names[i];

    if (ptr_str == NULL) {
        if (authors->names == NULL) {
            authors->names = malloc(sizeof(char *));
            authors->count = 1;
        } else {
            authors->names = realloc(authors->names, sizeof(char *) * ++authors->count);
        }
        ptr_str = authors->names[authors->count - 1] = malloc(strlen(msg->data) + 1);
        memcpy(ptr_str, msg->data, strlen(msg->data));
        ptr_str[strlen(msg->data)] = 0;
    }
    return ptr_str;
}
#endif

struct chat_client *
chat_client_new(const char *name)
{

    struct chat_client *client = calloc(1, sizeof(*client));
    client->socket = -1;
    client->poll = -1;

    // Init name
    client->name = malloc(strlen(name) + 1);
    memcpy(client->name, name, strlen(name));
    client->name[strlen(name)] = 0;
    client->name_sended = 0;

    // Init array of received messages
    client->recved = (struct msg_queue) {NULL, NULL, 0};
    client->send = (struct msg_queue) {NULL, NULL, 0};
    client->msg_buff = (struct msg_buff) {NULL, 0, 0};

#ifdef NEED_AUTHOR
    client->authors = (struct cli_authors) {NULL, 0};
#endif

    return client;
}

void
chat_client_delete(struct chat_client *client)
{
    if (client->socket >= 0)
        close(client->socket);
    if (client->poll >= 0)
        close(client->poll);
    if (client->name != NULL)
        free(client->name);

    struct chat_message *msg;
    while ((msg = queue_pop(&client->recved)) != NULL)
        chat_message_delete(msg);
    while ((msg = queue_pop(&client->send)) != NULL)
        chat_message_delete(msg);

    if (client->msg_buff.buff != NULL)
        free(client->msg_buff.buff);

#ifdef NEED_AUTHOR
    for (size_t i = 0; i < client->authors.count; i++)
        free(client->authors.names[i]);
    if (client->authors.names != NULL) free(client->authors.names);
#endif

    free(client);
}

int
chat_client_connect(struct chat_client *client, const char *addr)
{
    if (client->socket != -1)
        return CHAT_ERR_ALREADY_STARTED;

    if ((client->socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
        return CHAT_ERR_SYS;

    struct sockaddr_in servaddr;
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    inet_aton(addr, &servaddr.sin_addr);
    inet_pton(AF_INET, addr, &servaddr.sin_addr);

    int pos = 0;
    while(addr[pos]!= ':'){
        pos++;
    }
    servaddr.sin_port = htons(atoi(addr+pos+1));


    if (connect(client->socket, (struct sockaddr *) &servaddr, sizeof(servaddr)) != 0)
        return CHAT_ERR_NO_ADDR;


    client->poll = epoll_create(1);

    // Create event to
    struct epoll_event new_event;
    new_event.data.fd = client->socket;
    new_event.events = EPOLLIN;

    if (epoll_ctl(client->poll, EPOLL_CTL_ADD, client->socket, &new_event) == -1) {
        close(client->socket);
        client->socket = -1;
        return CHAT_ERR_SYS;
    }

    return 0;
}

struct chat_message *
chat_client_pop_next(struct chat_client *client)
{
	/* IMPLEMENT THIS FUNCTION */
    return queue_pop(&client->recved);;
}

int
chat_client_update(struct chat_client *client, double timeout)
{
    if (client->socket == -1)
        return CHAT_ERR_NOT_STARTED;

    struct epoll_event new_event;
    struct chat_message *msg;

#ifdef NEED_AUTHOR
    char const *auth_name;
#endif

    int res;

    int nfds = epoll_wait(client->poll, &new_event, 1, (int) (timeout * 1000));
    if (client->send.num_msgs == 0) {
        if (nfds == 0) return CHAT_ERR_TIMEOUT;
        else if (nfds < 0) return CHAT_ERR_SYS;
    } else {
        msg = queue_pop(&client->send);

        res = chat_message_send(msg, client->socket);
        chat_message_delete(msg);
        if (res <= 0) return CHAT_ERR_SYS;
    }

    if (nfds > 0) {
        msg = malloc(sizeof(struct chat_message));
#ifdef NEED_AUTHOR
        *msg = (struct chat_message) {NULL, NULL};

        // recved message with author name
        if (chat_message_recv(msg, client->socket) <= 0) {
            chat_message_delete(msg);
            return CHAT_ERR_SYS;
        }
        auth_name = find_or_add_authors(&client->authors, msg);
        chat_message_delete(msg);

        msg = malloc(sizeof(struct chat_message));
        *msg = (struct chat_message) {NULL, NULL};
#else
        *msg = (struct chat_message) {NULL};
#endif

        // recved message
        if (chat_message_recv(msg, client->socket) <= 0) {
            chat_message_delete(msg);
            return CHAT_ERR_SYS;
        }

#ifdef NEED_AUTHOR // Set to the message the author name
        msg->author = auth_name;
#endif
        queue_add(&client->recved, msg);
    }
    return 0;
}

int
chat_client_get_descriptor(const struct chat_client *client)
{
	return client->socket;
}

int
chat_client_get_events(const struct chat_client *client)
{
    int result = 0;
    if (client->send.num_msgs != 0) result |= CHAT_EVENT_OUTPUT;
    if (client->socket != -1) result |= CHAT_EVENT_INPUT;
    return result;
}

int
chat_client_feed(struct chat_client *client, const char *msg, uint32_t msg_size)
{// Check is the client connected to the chat
    if (client->socket == -1) return CHAT_ERR_NOT_STARTED;

    if (client->msg_buff.size - client->msg_buff.pos < msg_size) {
        char *new_buff = malloc(client->msg_buff.pos + msg_size);
        memcpy(new_buff, client->msg_buff.buff, client->msg_buff.pos);
        free(client->msg_buff.buff);
        client->msg_buff.buff = new_buff;
    }

    struct chat_message *msg_;
    char ch;

    for (uint32_t i = 0; i < msg_size; i++) {
        if ((ch = msg[i]) == '\n') {
            client->msg_buff.buff[client->msg_buff.pos++] = 0;

            // Make message struct
            msg_ = malloc(sizeof(struct chat_message));
#ifdef NEED_AUTHOR
            *msg_ = (struct chat_message) {NULL, NULL};
#else
            *msg_ = (struct chat_message) {NULL};
#endif

            msg_->data = malloc(client->msg_buff.pos);
            memcpy(msg_->data, client->msg_buff.buff, client->msg_buff.pos);
            // Clear buffer
            client->msg_buff.pos = 0;

            // Remove spacings and decide what to do
            if (chat_message_remove_spacing(msg_) == 0) {
                chat_message_delete(msg_);
                continue;
            }

            // Add author if needs
#ifdef NEED_AUTHOR
            if (!client->name_sended) {
                client->name_sended = 1;
                msg_->author = client->name;
            }
#endif
            // Push message to send buffer
            queue_add(&client->send, msg_);
        } else {
            client->msg_buff.buff[client->msg_buff.pos++] = ch;
        }
    }
    return 0;
}
