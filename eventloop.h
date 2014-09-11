// vi: expandtab sts=4 ts=4 sw=4
#ifndef EVENTLOOP_H
#define EVENTLOOP_H

#include <arpa/inet.h>

struct callback {
    void *obj;
    int (*fn) (void *obj, void *data);
};

typedef int eventloop_t;

int do_callback(struct callback *cb, void *data);

eventloop_t eventloop_new();
int eventloop_add(eventloop_t loop, int fd, struct callback *cb);
int eventloop_run(eventloop_t loop);

#endif /* EVENTLOOP_H */
