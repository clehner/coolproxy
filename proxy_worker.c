// vi: expandtab sts=4 ts=4 sw=4
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "util.h"
#include "eventloop.h"
#include "http_parser.h"
#include "proxy_worker.h"

// Callback functions

static int proxy_worker_connect_cb(void *worker, void *data) {
    return proxy_worker_connected((struct proxy_worker *)worker);
}

static int proxy_worker_recv_cb(void *worker, void *data) {
    return proxy_worker_recv((struct proxy_worker *)worker);
}

// Main functions

struct proxy_worker *proxy_worker_new(const char *host, unsigned short port,
        eventloop_t loop) {
    // Store the host string contiguous with the struct in memory
    size_t host_len = strlen(host) + 1;
    struct proxy_worker *worker = calloc(1, sizeof(struct proxy_worker));
    if (!worker) return NULL;

    worker->port = port;
    worker->host = (char *)malloc(host_len * sizeof(char));
    strncpy(worker->host, host, host_len);

    worker->idle = false;
    worker->connected = false;
    worker->loop = loop;

    callback_set(&worker->recv_cb, worker, proxy_worker_recv_cb);
    callback_set(&worker->connect_cb, worker, proxy_worker_connect_cb);

    return worker;
}

void proxy_worker_free(struct proxy_worker *worker) {
    // Remove the worker from the linked list
    if (worker->next) {
        worker->next->prev = worker->prev;
    }
    worker->prev->next = worker->next;

    free(worker->host);
    free(worker);
}

int proxy_worker_connect(struct proxy_worker *worker) {
    int status = proxy_worker_resolve(worker);
    if (status) return status;
    return proxy_worker_try_connect(worker);
}

int proxy_worker_resolve(struct proxy_worker *worker) {
    struct addrinfo hints = {0};
    int status;
    char port[6];

    // Convert the port to string for getaddrinfo
    if (snprintf(port, sizeof port, "%hd", worker->port) < 1) {
        return -1;
    }

    hints.ai_family = AF_UNSPEC;    // Allow IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;

    // Resolve hostname to addresses
    if ((status = getaddrinfo(worker->host, port, &hints, &worker->addrs))) {
        fprintf(stderr, "getaddrinfo error: %s (%s, %s)\n",
                gai_strerror(status), worker->host, port);
        return -1;
    }

    worker->rp = worker->addrs;
    return 0;
}

// Connect to the next possible host
int proxy_worker_try_connect(struct proxy_worker *worker) {
    struct addrinfo *rp;
    int status;

    /* getaddrinfo() returns a list of address structures.
        Try each address until we successfully bind(2).
        If socket(2) (or bind(2)) fails, we (close the socket
        and) try the next address. */

    for (rp = worker->rp; rp != NULL; rp = worker->rp = rp->ai_next) {
        worker->fd = socket(rp->ai_family, rp->ai_socktype | SOCK_NONBLOCK,
                rp->ai_protocol);

        if (worker->fd < 0) {
            perror("socket");
            continue;
        }

        // Register the server socket in the event loop
        if (eventloop_add(worker->loop, worker->fd, &worker->connect_cb,
                eventloop_write)) {
            perror("eventloop_add");
            freeaddrinfo(worker->addrs);
            close(worker->fd);
            return -1;
        }

        printf("Connecting to %s\n", sprint_addrport(rp->ai_addr));
        status = connect(worker->fd, rp->ai_addr, rp->ai_addrlen);
        if (status && errno != EINPROGRESS) {
            perror("connect");
            close(worker->fd);
            continue;
        }

        if (status == 0) {
            // Connected
            proxy_worker_connected(worker);
        }

        // Wait for response from nonblocking connect
        return 0;
    }

    // No address succeeded
    fprintf(stderr, "Could not connect to %s:%hu\n",
            worker->host, worker->port);
    freeaddrinfo(worker->addrs);
    return -2;
}

int proxy_worker_connected(struct proxy_worker *worker) {
    int err;
    socklen_t optlen = sizeof err;

    if (getsockopt(worker->fd, SOL_SOCKET, SO_ERROR, &err, &optlen)) {
        perror("getsockopt");
        return 0;
    }
    if (err) {
        printf("Connect failed.\n");
        close(worker->fd);
        // Try next address
        worker->rp = worker->rp->ai_next;
        proxy_worker_try_connect(worker);
        return 0;
    }

    // Connect succeeded, so we're done with the addrs list
    freeaddrinfo(worker->addrs);

    // Change the callback to be for recv instead of connect
    if (eventloop_mod(worker->loop, worker->fd, &worker->recv_cb,
                eventloop_read) < -1) {
        perror("eventloop_mod");
        return 0;
    }

    worker->connected = true;

    printf("Connected to remote host.\n");

    // Send our request
    proxy_worker_flush_queue(worker);

    return 0;
}

void proxy_worker_flush_queue(struct proxy_worker *worker) {
    if (!worker->connected) {
        printf("Attempted to flush queue without connection\n");
        return;
    }

    // TODO: investigate using a pipe
    while (worker->send_queue) {
        struct msg *msg = worker->send_queue;
        ssize_t bytes = send(worker->fd, msg->data, msg->len, 0);
        if (bytes < 0) {
            perror("send queue");
            return;
        } else if (bytes < msg->len) {
            printf("Send incomplete\n");
        }
        // TODO: use offset from bytes sent when retransmitting
        worker->send_queue = msg->next;
        free(msg);
    }
}

int proxy_worker_request(struct proxy_worker *worker, const char *method,
        const char *uri) {
    // Send request line
    char buf[256];
    int len = snprintf(buf, sizeof buf, "%s /%s HTTP/1.1\r\n", method, uri);
    if (len < 0) {
        perror("snprintf: worker http request");
        return 1;
    }
    proxy_worker_send(worker, (const char *)buf, len);
    return 0;
}

int proxy_worker_send(struct proxy_worker *worker, const char *data,
        size_t len) {
    if (worker->connected) {
        printf("send!\n");
        if (sendall(worker->fd, data, len) < 0) {
            perror("send: worker");
            return 1;
        }

    } else {
        // Queue the message for sending when the socket is connected
        struct msg *msg = malloc(sizeof(struct msg));
        if (!msg) {
            return -1;
        }
        char *buf = malloc(len * sizeof(char));
        if (!buf) {
            free(msg);
            return -1;
        }
        strncpy(buf, data, len);
        msg->data = buf;
        msg->len = len;
        msg->next = worker->send_queue;
        worker->send_queue = msg;
    }
    return 0;
}

int proxy_worker_recv(struct proxy_worker *worker) {
    size_t len = 0;
    char buf[512];

    len = recv(worker->fd, buf, sizeof buf - 1, 0);
    if (len == -1) {
        perror("recv");
        // Emit connection error to client (502)
        return 0;

    } else if (len == 0) {
        printf("Server closed connection\n");
        close(worker->fd);
        // Notify the server that we closed
        //proxy_server_notify_worker_closed(worker->server, worker);
        return 0;
    }

    if (parser_parse(&worker->parser, buf, len)) {
            fprintf(stderr, "Unable to parse server message\n");
    }

    return 0;
}
