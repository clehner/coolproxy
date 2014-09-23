// vi: expandtab sts=4 ts=4 sw=4
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include "util.h"
#include "eventloop.h"
#include "proxy_server.h"
#include "proxy_client.h"
#include "http_parser.h"
#include "proxy_worker.h"

#define status(code, reason) \
    const char err_##code[] = "HTTP/1.1 "#code" "#reason"\r\n\r\n"#reason"\n";

status(200, OK)
status(400, Bad Request)
status(500, Internal Server Error)
status(501, Not Implemented)
status(502, Bad Gateway)
status(503, Service Unavailable)
status(504, Gateway Timeout)
status(505, HTTP Version Not Supported)

#define proxy_client_send_error(client, code) \
    send(client->fd, err_##code, sizeof err_##code, 0) >= 0 ? \
    close(client->fd) : -1

/*
 * a client that has connected to the proxy server
 */
struct proxy_client {
    int fd;
    eventloop_t loop;
    struct proxy_server *server;
    struct callback recv_cb;
    struct http_parser parser;
    struct proxy_worker *worker;
    bool persistent_connection;
};

// proxy client callbacks

int proxy_client_on_http_request(struct proxy_client *client,
        struct http_parser_request *request);

int proxy_client_on_http_header(struct proxy_client *client,
        struct http_parser_header *header);

int proxy_client_on_http_header(struct proxy_client *client,
        struct http_parser_header *header);

int proxy_client_on_http_body(struct proxy_client *client,
        struct body_msg *body);

// proxy worker callbacks

int on_worker_status(struct proxy_client *client,
        struct http_parser_status *status);

int on_worker_header(struct proxy_client *client,
        struct http_parser_header *header);

int on_worker_body(struct proxy_client *client, struct body_msg *body);

int on_worker_close(struct proxy_client *client, void *data);

static int proxy_client_recv_cb(void *client, void *data) {
    return proxy_client_recv((struct proxy_client *)client);
}

static struct http_parser_callbacks parser_callbacks = {
    .on_header = (callback_fn) proxy_client_on_http_header,
    .on_request = (callback_fn) proxy_client_on_http_request,
    .on_body = (callback_fn) proxy_client_on_http_body,
};

static struct http_parser_callbacks worker_callbacks = {
    .on_status = (callback_fn) on_worker_status,
    .on_header = (callback_fn) on_worker_header,
    .on_body = (callback_fn) on_worker_body,
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
    if (eventloop_add(loop, sockfd, &client->recv_cb, eventloop_read) < 0) {
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
        // fd is automatically removed from epoll set when the socket is closed.
        proxy_client_close(client);

        // Dissociate from worker
        if (client->worker) {
            proxy_worker_dissociate(client->worker);
        } else {
            printf("No worker to dissociate\n");
        }

        // Notify the server that we closed
        proxy_server_notify_client_closed(client->server, client);
        return 0;
    }

    if (parser_parse(&client->parser, buf, len)) {
        fprintf(stderr, "Error parsing HTTP\n");
        proxy_client_send_error(client, 400);
        return 1;
    }
    return 0;
}

int proxy_client_send(struct proxy_client *client,
        const char *data, size_t len) {
    return sendall(client->fd, data, len);
}

int proxy_client_close(struct proxy_client *client) {
    if (close(client->fd)) {
        perror("close client");
        return 1;
    }
    return 0;
}

int proxy_client_on_http_request(struct proxy_client *client,
        struct http_parser_request *request) {
    //char buf[256];
    struct proxy_worker *worker;

    // Check HTTP version
    if (request->http_version.major_version > 1) {
        return proxy_client_send_error(client, 505);
    }

    // Assume persistent connection for HTTP/1.1 client, for now
    if (request->http_version.minor_version >= 1) {
        client->persistent_connection = true;
    }

    // Check scheme
    if (request->uri.scheme != http_scheme_http) {
        return proxy_client_send_error(client, 501);
    }

    // Give an error if the uri is just a path
    if (!request->uri.host) {
        return proxy_client_send_error(client, 400);
    }

    // Request a worker from the server for this hostname
    if (!(worker = proxy_server_request_worker(client->server,
            request->uri.host, request->uri.port))) {
        return proxy_client_send_error(client, 503);
    }
    client->worker = worker;

    if (!worker->connected) {
        // Establish the connection
        if (proxy_worker_connect(worker) < 0) {
            // Connection failed
            proxy_worker_free(worker);
            client->worker = NULL;
            return proxy_client_send_error(client, 502);
        }
    }

    // Set callbacks on the worker
    parser_init(&worker->parser, &worker_callbacks, client);
    callback_set(&worker->close_cb, client, (callback_fn)on_worker_close);

    // Issue the request to the worker
    if (proxy_worker_request(worker, request->method, request->uri.path)) {
        printf("Unable to send request to worker\n");
    }

    return 0;
}

int proxy_client_on_http_header(struct proxy_client *client,
        struct http_parser_header *header) {
    char buf[256];
    int len;

    if (!client->worker) {
        // Upstream connection is gone. Client connection should be already
        // closed now.
        return 0;
    }

    if (header == NULL) {
        // Finished receiving headers.
        // Send empty line to worker
        proxy_worker_send(client->worker, "\r\n", 2);
        return 0;
    }

    len = parser_write_header(header, buf, sizeof buf);
    if (!len) {
        fprintf(stderr, "Unable to write header\n");
        return 0;
    }
    if (proxy_worker_send(client->worker, buf, len)) {
        fprintf(stderr, "Unable to send header\n");
    }

    return 0;
}

int proxy_client_on_http_body(struct proxy_client *client,
        struct body_msg *body) {
    return proxy_worker_send(client->worker, body->msg, body->len);
}

int on_worker_status(struct proxy_client *client,
        struct http_parser_status *status) {
    char buf[256];
    ssize_t bytes;
    // TODO: investigate passing the status line string directly

    // Send the status line received from the worker/server, to the client
    int len = snprintf(buf, sizeof buf, "HTTP/%hhi.%hhi %hu %s\r\n",
            status->http_version.major_version,
            status->http_version.minor_version,
            status->code, status->reason);
    if (len < 0) {
        perror("snprintf: worker http status");
        send(client->fd, err_500, sizeof err_500, 0);
        return 1;
    }
    if (len >= sizeof buf) {
        fprintf(stderr, "Server status line too long\n");
        send(client->fd, err_500, sizeof err_500, 0);
        return 1;
    }

    bytes = send(client->fd, buf, len, 0);
    if (bytes < len) {
        fprintf(stderr, "Server status line truncated\n");
    }

    return 0;
}

int on_worker_header(struct proxy_client *client,
        struct http_parser_header *header) {
    char buf[256];

    if (header == NULL) {
        // Server finished sending headers
        // Send empty line to client
        if (proxy_client_send(client, "\r\n", 2) < 0) {
            perror("send: client empty line");
        }
        return 0;
    }

    // TODO; consider passing received header directly
    int len = snprintf(buf, sizeof buf, "%s: %s\r\n",
            header->name, header->value);
    if (len < 0) {
        perror("snprintf: worker http request");
        return 1;
    }
    if (len == sizeof buf) {
        fprintf(stderr, "Header may have been truncated\n");
    }

    //printf("worker: %s: %s\n", header->name, header->value);
    if (proxy_client_send(client, buf, len) < 0) {
        perror("send: client header");
    }
    return 0;
}

int on_worker_body(struct proxy_client *client, struct body_msg *body) {
    if (proxy_client_send(client, body->msg, body->len) < 0) {
        perror("send: worker body");
        return 1;
    }
    return 0;
}

int on_worker_close(struct proxy_client *client, void *data) {
    // Upstream closed.
    printf("worker closed\n");
    if (!client->worker) {
        fprintf(stderr, "No worker, or worker already freed\n");
        return 0;
    }

    proxy_worker_free(client->worker);
    client->worker = NULL;

    // Close downstream
    proxy_client_close(client);
    return 0;
}
