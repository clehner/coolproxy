// vi: expandtab sts=4 ts=4 sw=4
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include "util.h"
#include "eventloop.h"
#include "proxy_server.h"
#include "proxy_client.h"
#include "proxy_worker.h"

struct proxy_server {
    int fd;
    eventloop_t loop;
    struct callback accept_cb;
    struct proxy_worker workers;
};

static int proxy_server_accept_cb(void *ps, void *data) {
    return proxy_server_accept((struct proxy_server *)ps);
}

struct proxy_server *proxy_server_new(eventloop_t loop) {
    struct proxy_server *ps = calloc(1, sizeof(struct proxy_server));
    if (!ps) {
        return NULL;
    }

    ps->fd = -1;
    ps->loop = loop;

    // Set up the accept callback
    ps->accept_cb.obj = ps;
    ps->accept_cb.fn = proxy_server_accept_cb;

    return ps;
}

int proxy_server_listen(struct proxy_server *ps, int port) {
    struct sockaddr_in6 server_addr;
    struct sockaddr *addr = (struct sockaddr *)&server_addr;
    int reuse = true;

    // Create the proxy server socket
    if ((ps->fd = socket(AF_INET6, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        return 1;
    }

    // Make it non-blocking
    if (fcntl(ps->fd, F_SETFL, O_NONBLOCK) < 0) {
        perror("fcntl: server");
    }

    // Build the address to listen on
    server_addr.sin6_family = AF_INET6;
    server_addr.sin6_addr = in6addr_any;
    server_addr.sin6_port = htons(port);

    // Prevent socket reuse errors
    if (setsockopt(ps->fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof reuse)) {
        perror("setsockopt");
        return 1;
    }

    // Bind to the given port/address
    if (bind(ps->fd, addr, sizeof(server_addr))) {
        perror("bind");
        return 1;
    }

    // Listen for clients
    if (listen(ps->fd, 7)) {
        perror("listen");
        return 1;
    }

    // Register the server socket in the event loop
    if (eventloop_add(ps->loop, ps->fd, &ps->accept_cb) < 0) {
        perror("eventloop_add");
        return 1;
    }

    printf("Listening on %s\n", sprint_addrport(addr));

    return 0;
}

int proxy_server_accept(struct proxy_server *ps) {
    struct sockaddr addr;
    socklen_t addrlen = sizeof addr;
    int client_fd;
    struct proxy_client *client;

    if ((client_fd = accept(ps->fd, &addr, &addrlen)) < 0) {
        perror("accept");
        return -1;
    }

    if (fcntl(client_fd, F_SETFL, O_NONBLOCK) < 0) {
        perror("fcntl: client");
    }

    printf("Accepted connection from %s\n", sprint_addrport(&addr));

    if (!(client = proxy_client_new(ps->loop, ps, client_fd))) {
        fprintf(stderr, "Unable to create proxy client\n");
        if (!close(client_fd)) {
            perror("close client_fd");
        }
        return 1;
    }

    return 0;
}

void proxy_server_notify_client_closed(struct proxy_server *ps,
        struct proxy_client *client) {
    free(client);
}

struct proxy_worker *proxy_server_request_worker(struct proxy_server *ps,
        const char *host, unsigned short port) {
    struct proxy_worker *worker;

    // Reuse persistent server connections.
    // Look for an idle worker for this host/port
    // TODO: use a hash table instead of linked list
    for (struct proxy_worker *w = &ps->workers; w; w = w->next) {
        if (w->idle && w->connected &&
                port == w->port && !strcmp(host, w->host)) {
            // Found an idle connected worker for this host/port
            printf("Found idle worker\n");
            w->idle = false;
            return w;
        }
    }

    // Allocate a new worker
    printf("Allocating new worker\n");
    worker = proxy_worker_new(host, port);
    if (!worker) return NULL;

    // Insert the worker into the linked list
    worker->prev = &ps->workers;
    if (ps->workers.next) {
        ps->workers.next->prev = worker;
    }
    worker->next = ps->workers.next;
    ps->workers.next = worker;

    return worker;
}
