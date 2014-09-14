// vi: expandtab sts=4 ts=4 sw=4
#ifndef UTIL_H
#define UTIL_H

#include <arpa/inet.h>

#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define SIZE(array) (sizeof(array) / sizeof((array)[0]))

typedef int (*callback_fn) (void *obj, void *data);

struct callback {
    void *obj;
    callback_fn fn;
};

int do_callback(struct callback *cb, void *data);
void callback_set(struct callback *cb, void *obj, callback_fn fn);

const char *sprint_addrport(struct sockaddr *addr);

#endif /* UTIL_H */
