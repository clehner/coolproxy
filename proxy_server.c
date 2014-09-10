// vi: expandtab sts=4 ts=4 sw=4
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include "proxy_server.h"
#include "util.h"

struct proxy_server {
    int fd;
    int epoll_fd;
    struct callback accept_cb;
};

struct proxy_server *proxy_server_new() {
    struct proxy_server *ps = malloc(sizeof(struct proxy_server));
    if (!ps) {
        return NULL;
    }
    if (proxy_server_init(ps) < 0) {
        free(ps);
        return NULL;
    }
    return ps;
}

int proxy_server_init(struct proxy_server *ps) {
    ps->fd = -1;

    if ((ps->epoll_fd = epoll_create1(0)) < 0) {
        perror("epoll_create1");
        return -1;
    }

    // Set up the accept callback
    ps->accept_cb.obj = ps;
    ps->accept_cb.fn = proxy_server_accept_cb;

    return 0;
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
    fcntl(ps->fd, F_SETFL, O_NONBLOCK);

    server_addr.sin6_family = AF_INET6;
    server_addr.sin6_addr = in6addr_any;
    server_addr.sin6_port = htons(port);

    // Prevent socket reuse errors
    if (setsockopt(ps->fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof reuse))
    {
        perror("setsockopt");
        return 1;
    }

    // Bind to the given port/address
    if (bind(ps->fd, addr, sizeof(server_addr))) {
        perror("bind");
        return 1;
    }

    if (listen(ps->fd, 7)) {
        perror("listen");
        return 1;
    }

    // Register the server socket in the epoll object
    struct epoll_event event = {
        .events = EPOLLIN,
        .data.ptr = &ps->accept_cb
    };
    /*
    struct epoll_event *event = (struct epoll_event *)malloc(sizeof (struct epoll_event));
    event->events = EPOLLIN;
    event->data.ptr = ps;
    */
    if (epoll_ctl(ps->epoll_fd, EPOLL_CTL_ADD, ps->fd, &event) < 0) {
        perror("epoll_ctl");
        return 1;
    }

    printf("Listening on %s\n", sprint_addrport(addr));

    return 0;
}

int proxy_server_run(struct proxy_server *ps) {
    int status = 0;
    struct epoll_event events[12];
    int num_ready;
    int i;

    // Main event loop
    while (!status) {
        // Wait for events on our fds
        num_ready = epoll_wait(ps->epoll_fd, events, SIZE(events), -1);
        if (num_ready < 0) {
            perror("epoll_wait");
            status = -1;
        }
        // Execute callbacks for the events
        for (i = 0; i < num_ready && !status; i++) {
            status = do_callback(events[i].data.ptr, NULL);
        }
    }
    return status;
}

// Wrapper function for the accept method
int proxy_server_accept_cb(void *ps, void *data) {
    return proxy_server_accept((struct proxy_server *)ps);
}

int proxy_server_accept(struct proxy_server *ps) {
    struct sockaddr addr;
    socklen_t addrlen = sizeof addr;
    int client_fd;

    if ((client_fd = accept(ps->fd, &addr, &addrlen)) < 0) {
        perror("accept");
        return -1;
    }

    printf("Accepted connection from %s\n", sprint_addrport(&addr));

    return 0;
}

