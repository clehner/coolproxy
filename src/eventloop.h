// vi: expandtab sts=4 ts=4 sw=4
#ifndef EVENTLOOP_H
#define EVENTLOOP_H

#include "util.h"

typedef int eventloop_t;

enum eventloop_events {
    eventloop_read,
    eventloop_write,
    eventloop_readwrite
};

eventloop_t eventloop_new();
int eventloop_add(eventloop_t loop, int fd, struct callback *cb, int events);
int eventloop_mod(eventloop_t loop, int fd, struct callback *cb, int events);
int eventloop_run(eventloop_t loop);

#endif /* EVENTLOOP_H */
