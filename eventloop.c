// vi: expandtab sts=4 ts=4 sw=4
#include <stdlib.h>
#include <stdio.h>
#include <sys/epoll.h>
#include "eventloop.h"
#include "util.h"

struct eventloop {
    int epoll_fd;
};

int do_callback(struct callback *cb, void *data) {
    return cb->fn ? cb->fn(cb->obj, data) : -1;
}

struct eventloop *eventloop_new() {
    struct eventloop *loop = malloc(sizeof(struct eventloop));
    if (!loop) return NULL;
    if (eventloop_init(loop) < 0) {
        free(loop);
        return NULL;
    }
    return loop;
}

int eventloop_init(struct eventloop *loop) {
    return loop->epoll_fd = epoll_create1(0);
}

int eventloop_add(struct eventloop *loop, int fd, struct callback *cb) {
    struct epoll_event event = {
        .events = EPOLLIN,
        .data.ptr = cb
    };
    return epoll_ctl(loop->epoll_fd, EPOLL_CTL_ADD, fd, &event);
}

int eventloop_run(struct eventloop *loop) {
    int status = 0;
    struct epoll_event events[12];
    int num_ready;
    int i;

    // Main event loop
    while (!status) {
        // Wait for events on our fds
        num_ready = epoll_wait(loop->epoll_fd, events, SIZE(events), -1);
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
