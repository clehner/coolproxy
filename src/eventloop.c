// vi: expandtab sts=4 ts=4 sw=4
#include <stdlib.h>
#include <stdio.h>
#include <sys/epoll.h>
#include "eventloop.h"
#include "util.h"

// these correspond to the values of enum eventloop_events
static int eventloop_epoll_events[] = {
    EPOLLIN,
    EPOLLOUT,
    EPOLLIN | EPOLLOUT
};

eventloop_t eventloop_new() {
    return epoll_create1(0);
}

static int eventloop_ctl(eventloop_t loop, int fd, struct callback *cb,
        int events, int op) {
    if (events < 0 || events >= SIZE(eventloop_epoll_events)) {
        return -1;
    }
    struct epoll_event event = {
        .events = eventloop_epoll_events[events],
        .data.ptr = cb
    };
    return epoll_ctl(loop, op, fd, &event);
}

int eventloop_add(eventloop_t loop, int fd, struct callback *cb, int events) {
    return eventloop_ctl(loop, fd, cb, events, EPOLL_CTL_ADD);
}

int eventloop_mod(eventloop_t loop, int fd, struct callback *cb, int events) {
    return eventloop_ctl(loop, fd, cb, events, EPOLL_CTL_MOD);
}

int eventloop_run(eventloop_t loop) {
    int status = 0;
    struct epoll_event events[12];
    int num_ready;
    int i;

    // Main event loop
    while (!status) {
        // Wait for events on our fds
        num_ready = epoll_wait(loop, events, SIZE(events), -1);
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
