// vi: expandtab sts=4 ts=4 sw=4
#ifndef PROXY_SERVER_H
#define PROXY_SERVER_H

#include "eventloop.h"

struct proxy_server;

struct proxy_server *proxy_server_new(struct eventloop *loop);
int proxy_server_listen(struct proxy_server *ps, int port);
int proxy_server_run(struct proxy_server *ps);
int proxy_server_accept(struct proxy_server *ps);

int proxy_server_accept_cb(void *ps, void *data);

#endif /* PROXY_SERVER_H */

