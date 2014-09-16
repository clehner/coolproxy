// vi: expandtab sts=4 ts=4 sw=4
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include "eventloop.h"
#include "proxy_server.h"
#include "proxy_client.h"
#include "http_parser.h"
#include "proxy_worker.h"

#define status(code, reason) \
    const char err_##code[] = "HTTP/1.1 "#code" "#reason"\r\n\r\n";

status(200, OK)
status(400, Bad Request)
status(500, Internal Server Error)
status(501, Not Implemented)
status(502, Bad Gateway)
status(503, Service Unavailable)
status(504, Gateway Timeout)
status(505, HTTP Version Not Supported)

#define proxy_client_send_status(client, code) \
    send(client->fd, err_##code, sizeof err_##code, 0)

/*
 * a client that has connected to the proxy server
 */
struct proxy_client {
    int fd;
    eventloop_t loop;
    struct proxy_server *server;
    struct callback recv_cb;
    struct http_parser parser;
    bool persistent_connection;
};

int proxy_client_on_http_request(struct proxy_client *client,
        struct http_parser_request *status);

int proxy_client_on_http_header(struct proxy_client *client,
        struct http_parser_header *header);

static int proxy_client_recv_cb(void *client, void *data) {
    return proxy_client_recv((struct proxy_client *)client);
}

static struct http_parser_callbacks parser_callbacks = {
    .on_header = (callback_fn) proxy_client_on_http_header,
    .on_request = (callback_fn) proxy_client_on_http_request,
};

struct proxy_client *proxy_client_new(eventloop_t loop,
        struct proxy_server *server, int sockfd) {
    struct proxy_client *client = calloc(1, sizeof(struct proxy_client));
    if (!client) return NULL;

    client->fd = sockfd;
    client->loop = loop;
    client->server = server;
    client->persistent_connection = false;

    parser_init(&client->parser, &parser_callbacks, client);

    callback_set(&client->recv_cb, client, proxy_client_recv_cb);

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
            proxy_client_send_status(client, 400);
            return 1;
        default:
            return 0;
    }
}

int proxy_client_on_http_request(struct proxy_client *client,
        struct http_parser_request *request) {
    char buf[256];
    struct proxy_worker *worker;

    // Check HTTP version
    if (request->http_version.major_version > 1) {
        return proxy_client_send_status(client, 505);
    }

    // Assume persistent connection for HTTP/1.1 client, for now
    if (request->http_version.minor_version >= 1) {
        client->persistent_connection = true;
    }

    // Check scheme
    if (request->uri.scheme != http_scheme_http) {
        return proxy_client_send_status(client, 501);
    }

    // Give an error if the uri is just a path
    if (!request->uri.host) {
        return proxy_client_send_status(client, 400);
    }

    // Request a worker from the server for this hostname
    if (!(worker = proxy_server_request_worker(client->server,
            request->uri.host, request->uri.port))) {
        return proxy_client_send_status(client, 503);
    }

    if (!worker->connected) {
        // Establish the connection
        if (proxy_worker_connect(worker) < 0) {
            // Connection failed
            proxy_worker_free(worker);
            return proxy_client_send_status(client, 502);
        }
    }

    proxy_worker_request(worker, request->method, request->uri.path);

    int len = snprintf(buf, sizeof buf, "host: %s, port: %hu, path: %s.\r\n",
            worker->host, worker->port, request->uri.path);
    if (len < 0) {
        perror("snprintf: client http status");
        send(client->fd, err_500, sizeof err_500, 0);
        return 1;
    }
    ssize_t bytes = send(client->fd, buf, len, 0);
    if (bytes < len) {
        perror("send");
    }

    // Set callbacks on the worker
    // Issue the request to the worker

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
