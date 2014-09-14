// vi: expandtab sts=4 ts=4 sw=4
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "eventloop.h"
#include "proxy_server.h"
#include "proxy_client.h"
#include "http_parser.h"

const char err_400[] = "HTTP/1.1 400 Bad Request\r\n\r\n";
const char err_500[] = "HTTP/1.1 500 Internal Server Error\r\n\r\n";

/*
 * a client that has connected to the proxy server
 */
struct proxy_client {
    int fd;
    eventloop_t loop;
    struct proxy_server *server;
    struct callback recv_cb;
    struct http_parser parser;
    struct http_parser_callbacks parser_callbacks;
};

int proxy_client_on_http_request(struct proxy_client *client,
        struct http_parser_request *status);

int proxy_client_on_http_header(struct proxy_client *client,
        struct http_parser_header *header);

static int proxy_client_recv_cb(void *client, void *data) {
    return proxy_client_recv((struct proxy_client *)client);
}

struct proxy_client *proxy_client_new(eventloop_t loop,
        struct proxy_server *server, int sockfd) {
    struct proxy_client *client = calloc(1, sizeof(struct proxy_client));
    if (!client) return NULL;

    client->fd = sockfd;
    client->loop = loop;
    client->server = server;

    parser_init(&client->parser, &client->parser_callbacks);

    callback_set(&client->recv_cb, client, proxy_client_recv_cb);

    callback_set(&client->parser_callbacks.on_request, client,
            (callback_fn) proxy_client_on_http_request);

    callback_set(&client->parser_callbacks.on_header, client,
            (callback_fn) proxy_client_on_http_header);

    // Register the server socket in the event loop
    if (eventloop_add(loop, sockfd, &client->recv_cb) < 0) {
        perror("eventloop_add");
        return NULL;
    }

    return client;
}

int proxy_client_recv(struct proxy_client *client) {
    size_t len = 0;
    char buf[512];

    len = recv(client->fd, buf, sizeof buf, 0);
    if (len == -1) {
        perror("recv");
        return 0;
    } else if (len == 0) {
        printf("Client disconnected\n");
        close(client->fd);
        // fd is automatically removed from epoll set when the socket is closed.
        // Notify the server that we closed
        proxy_server_notify_client_closed(client->server, client);
        return 0;
    }

    switch (parser_parse(&client->parser, buf, len)) {
        case 400:
            fprintf(stderr, "Received message without \\r\n");
            send(client->fd, err_400, sizeof err_400, 0);
            return 1;
        default:
            return 0;
    }
}

int proxy_client_on_http_request(struct proxy_client *client,
        struct http_parser_request *request) {
    char buf[256];
    // Respond to the client with the equivalent response line
    int len = snprintf(buf, sizeof buf, "HTTP/1.1 %d You requested %s %s\r\n",
            200, request->method, request->uri);
    if (len < 0) {
        perror("snprintf: client http status");
        send(client->fd, err_500, sizeof err_500, 0);
        return 1;
    }

    send(client->fd, buf, len, 0);
    return 0;
}

int proxy_client_on_http_header(struct proxy_client *client,
        struct http_parser_header *header) {
    char buf[256];
    int len = snprintf(buf, sizeof buf, "You sent %s: %s\r\n",
            header->name, header->value);
    if (len < 0) {
        perror("snprintf: client http status");
        send(client->fd, err_500, sizeof err_500, 0);
        return 1;
    }

    send(client->fd, buf, len, 0);
    return 0;
}
