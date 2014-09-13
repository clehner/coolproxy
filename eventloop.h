// vi: expandtab sts=4 ts=4 sw=4
#ifndef EVENTLOOP_H
#define EVENTLOOP_H

#include <arpa/inet.h>
#include "util.h"

typedef int eventloop_t;

eventloop_t eventloop_new();
int eventloop_add(eventloop_t loop, int fd, struct callback *cb);
int eventloop_run(eventloop_t loop);

#endif /* EVENTLOOP_H */
