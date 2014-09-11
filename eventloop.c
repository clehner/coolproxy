// vi: expandtab sts=4 ts=4 sw=4
#include <stdlib.h>
#include <stdio.h>
#include <sys/epoll.h>
#include "eventloop.h"
#include "util.h"

int do_callback(struct callback *cb, void *data) {
    return cb->fn ? cb->fn(cb->obj, data) : -1;
}

eventloop_t eventloop_new() {
    return epoll_create1(0);
}

int eventloop_add(eventloop_t loop, int fd, struct callback *cb) {
    struct epoll_event event = {
        .events = EPOLLIN,
        .data.ptr = cb
    };
    return epoll_ctl(loop, EPOLL_CTL_ADD, fd, &event);
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
