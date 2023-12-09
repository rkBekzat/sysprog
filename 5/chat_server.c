#include "chat.h"
#include "chat_server.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/epoll.h>

struct chat_peer {
	/** Client's socket. To read/write messages. */
	int socket;

#ifdef NEED_AUTHOR
    char *author;
#endif
	/** Output buffer. */
	/* ... */
	/* PUT HERE OTHER MEMBERS */
};

struct chat_server {
	/** Listening socket. To accept new clients. */
	int socket;
    int poll;
	/** Array of peers. */

    struct chat_peer_stats {
        struct chat_peer *peers;
        size_t count;
    } peers_data;

    struct msg_queue recved, send;

    struct msg_buff {
        char *buff;
        size_t pos;
        size_t size;
    } msg_buff;

	/* PUT HERE OTHER MEMBERS */
};


void
peers_data_add(struct chat_peer_stats *peers_data, int fd) {
    if (peers_data->peers == NULL) {
        peers_data->peers = malloc(sizeof(struct chat_peer));
        peers_data->count = 1;
    } else {
        peers_data->peers = realloc(peers_data->peers, sizeof(struct chat_peer) * ++peers_data->count);
    }
#ifdef NEED_AUTHOR
    peers_data->peers[peers_data->count - 1] = (struct chat_peer) {fd, NULL};
#else
    peers_data->peers[peers_data->count - 1] = (struct chat_peer) {fd};
#endif
}

struct chat_peer *
peers_data_find(struct chat_peer_stats *peers_data, int fd) {
    for (size_t i = 0; i < peers_data->count; i++)
        if (fd == peers_data->peers[i].socket)
            return peers_data->peers + i;
    return NULL;
}


void
chat_server_remove_client(struct chat_peer_stats *peers_data, int fd)
{
    int pr_id = -1;

    for (size_t i = 0; i < peers_data->count && pr_id == -1; i++)
        if (fd == peers_data->peers[i].socket)
            pr_id = i;


#ifdef NEED_AUTHOR
    if (peers_data->peers[pr_id].author != NULL)
        free(peers_data->peers[pr_id].author);
#endif

    struct chat_peer *new_peers = malloc(sizeof (struct chat_peer) * (peers_data->count - 1));
    memcpy(new_peers, peers_data->peers, pr_id * sizeof(struct chat_peer));
    memcpy(new_peers + pr_id, peers_data->peers + pr_id + 1, (peers_data->count-- - pr_id) * sizeof(struct chat_peer));

    free(peers_data->peers);
    peers_data->peers = new_peers;
}

struct chat_server *
chat_server_new(void)
{
    struct chat_server *server = calloc(1, sizeof(*server));
    server->socket = -1;
    server->poll = -1;

    server->recved = (struct msg_queue) {NULL, NULL, 0};
    server->send = (struct msg_queue) {NULL, NULL, 0};
    server->peers_data = (struct chat_peer_stats) {NULL, 0};
    server->msg_buff = (struct msg_buff) {NULL, 0, 0};

    return server;
}

void
chat_server_delete(struct chat_server *server)
{
    if (server->socket >= 0)
        close(server->socket);
    if (server->poll >= 0)
        close(server->poll);

    struct chat_message *msg;
    while ((msg = queue_pop(&server->recved)) != NULL)
        chat_message_delete(msg);
    while ((msg = queue_pop(&server->send)) != NULL)
        chat_message_delete(msg);

    if (server->msg_buff.buff != NULL)
        free(server->msg_buff.buff);

#ifdef NEED_AUTHOR
    for (size_t i = 0; i < server->peers_data.count; i++) {
        if (server->peers_data.peers[i].author != NULL)
            free(server->peers_data.peers[i].author);
    }
#endif
    if (server->peers_data.peers != NULL)
        free(server->peers_data.peers);

    free(server);
}

int
chat_server_listen(struct chat_server *server, uint16_t port)
{
    if (server->socket != -1)
        return CHAT_ERR_ALREADY_STARTED;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_port = htons(port);
    /* Listen on all IPs of this machine. */
    addr.sin_addr.s_addr = htonl(INADDR_ANY);


    server->socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server->socket == -1)
        return CHAT_ERR_SYS;

    if (
            (bind(server->socket, (struct sockaddr *) &addr, sizeof(addr))) != 0 ||
            (listen(server->socket, 128)) < 0) {
        close(server->socket);
        server->socket = -1;
        return CHAT_ERR_PORT_BUSY;
    }

    server->poll = epoll_create(1);

    // Create event to
    struct epoll_event new_event;
    new_event.data.fd = server->socket;
    new_event.events = EPOLLIN;

    if (epoll_ctl(server->poll, EPOLL_CTL_ADD, server->socket, &new_event) == -1) {
        close(server->socket);
        server->socket = -1;
        return CHAT_ERR_SYS;
    }
    return 0;
}

struct chat_message *
chat_server_pop_next(struct chat_server *server)
{
    return queue_pop(&server->recved);
}

int
chat_server_update(struct chat_server *server, double timeout)
{
    if (server->socket == -1)
        return CHAT_ERR_NOT_STARTED;

    struct epoll_event new_event;
    struct chat_message *msg;
    struct chat_peer *peer;

    int res;
    int fd;

    int nfds = epoll_wait(server->poll, &new_event, 1, (int) (timeout * 1000));
    if (server->send.num_msgs == 0) {
        if (nfds == 0) return CHAT_ERR_TIMEOUT;
        else if (nfds < 0) return CHAT_ERR_SYS;
    } else {
        msg = queue_pop(&server->send);
        for (size_t i = 0; i < server->peers_data.count; i++)
            chat_message_send(msg, server->peers_data.peers[i].socket);
        chat_message_delete(msg);
    }

    if (nfds <= 0) return 0;
    if (new_event.data.fd == server->socket) {
        fd = accept(server->socket, NULL, NULL);
        if (fd == -1) return CHAT_ERR_SYS;

        new_event.data.fd = fd;
        new_event.events = EPOLLIN;
        if (epoll_ctl(server->poll, EPOLL_CTL_ADD, fd, &new_event) == -1)
            return CHAT_ERR_SYS;

        peers_data_add(&server->peers_data, fd);
    } else {
        fd = new_event.data.fd;
        peer = peers_data_find(&server->peers_data, fd);

        msg = malloc(sizeof(struct chat_message));
#ifdef NEED_AUTHOR
        *msg = (struct chat_message) {NULL, NULL};

        if (peer->author == NULL) {
            // recved message with author name
            if ((res = chat_message_recv(msg, peer->socket)) <= 0) {
                chat_message_delete(msg);
                if (res == 0) {
                    chat_server_remove_client(&server->peers_data, fd);
                    epoll_ctl(server->poll, EPOLL_CTL_DEL, fd, NULL);
                    close(fd);
                    return 0;
                }
                return CHAT_ERR_SYS;
            }
            peer->author = malloc(strlen(msg->data) + 1);
            memcpy(peer->author, msg->data, strlen(msg->data));
            peer->author[strlen(msg->data)] = 0;
            chat_message_delete(msg);

            msg = malloc(sizeof(struct chat_message));
            *msg = (struct chat_message) {NULL, NULL};
        }
#else
        *msg = (struct chat_message) {NULL};
#endif

        // recved message
        if ((res = chat_message_recv(msg, peer->socket)) <= 0) {
            chat_message_delete(msg);
            if (res == 0) {
                chat_server_remove_client(&server->peers_data, fd);
                epoll_ctl(server->poll, EPOLL_CTL_DEL, fd, NULL);
                close(fd);
                return 0;
            }
            return CHAT_ERR_SYS;
        }

#ifdef NEED_AUTHOR
        msg->author = peer->author;
#endif
        queue_add(&server->recved, msg);

        for (size_t i = 0; i < server->peers_data.count; i++)
            if (peer->socket != server->peers_data.peers[i].socket)
                chat_message_send(msg, server->peers_data.peers[i].socket);
    }

    return 0;
}

int
chat_server_get_descriptor(const struct chat_server *server)
{
    return server->poll;
}

int
chat_server_get_socket(const struct chat_server *server)
{
	return server->socket;
}

int
chat_server_get_events(const struct chat_server *server)
{
    int result = 0;
    if (server->send.num_msgs != 0) result |= CHAT_EVENT_OUTPUT;
    if (server->socket != -1) result |= CHAT_EVENT_INPUT;
    return result;
}

int
chat_server_feed(struct chat_server *server, const char *msg, uint32_t msg_size)
{
#if NEED_SERVER_FEED
    // Check is the client connected to the chat
    if (server->socket == -1) return CHAT_ERR_NOT_STARTED;

    if (server->msg_buff.size - server->msg_buff.pos < msg_size) {
        char *new_buff = malloc(server->msg_buff.pos + msg_size);
        memcpy(new_buff, server->msg_buff.buff, server->msg_buff.pos);
        free(server->msg_buff.buff);
        server->msg_buff.buff = new_buff;
    }

    struct chat_message *msg_;
    char ch;

    for (uint32_t i = 0; i < msg_size; i++) {
        if ((ch = msg[i]) == '\n') {
            server->msg_buff.buff[server->msg_buff.pos++] = 0;

            // Make message struct
            msg_ = malloc(sizeof(struct chat_message));
#ifdef NEED_AUTHOR
            *msg_ = (struct chat_message) {NULL, NULL};
#else
            *msg_ = (struct chat_message) {NULL};
#endif

            msg_->data = malloc(server->msg_buff.pos);
            memcpy(msg_->data, server->msg_buff.buff, server->msg_buff.pos);
            // Clear buffer
            server->msg_buff.pos = 0;

            // Remove spacings and decide what to do
            if (chat_message_remove_spacing(msg_) == 0) {
                chat_message_delete(msg_);
                continue;
            }

            // Add author if needs
#ifdef NEED_AUTHOR
            msg_->author = "server\0";
#endif
            // Push message to send buffer
            queue_add(&server->send, msg_);
        } else {
            server->msg_buff.buff[server->msg_buff.pos++] = ch;
        }
    }
    return 0;
#endif
}
