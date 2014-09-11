// vi: expandtab sts=4 ts=4 sw=4
#ifndef EVENTLOOP_H
#define EVENTLOOP_H

#include <arpa/inet.h>

struct callback {
    void *obj;
    int (*fn) (void *obj, void *data);
};

struct eventloop;

int do_callback(struct callback *cb, void *data);

struct eventloop *eventloop_new();
int eventloop_init(struct eventloop *loop);
int eventloop_add(struct eventloop *loop, int fd, struct callback *cb);
int eventloop_run(struct eventloop *loop);

#endif /* EVENTLOOP_H */
