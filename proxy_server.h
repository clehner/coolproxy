// vi: expandtab sts=4 ts=4 sw=4
#ifndef PROXY_SERVER_H
#define PROXY_SERVER_H

#include "eventloop.h"

struct proxy_server;
struct proxy_client;

struct proxy_server *proxy_server_new(eventloop_t loop);
int proxy_server_listen(struct proxy_server *ps, int port);
int proxy_server_run(struct proxy_server *ps);
int proxy_server_accept(struct proxy_server *ps);
void proxy_server_notify_client_closed(struct proxy_server *ps,
        struct proxy_client *client);
struct proxy_worker *proxy_server_request_worker(struct proxy_server *ps,
        const char *host, unsigned short port);

#endif /* PROXY_SERVER_H */

