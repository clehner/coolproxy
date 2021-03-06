// vi: expandtab sts=4 ts=4 sw=4
#ifndef PROXY_CLIENT_H
#define PROXY_CLIENT_H

#include "eventloop.h"

struct proxy_client;
struct proxy_server;

struct proxy_client *proxy_client_new(eventloop_t loop,
        struct proxy_server *server, int sockfd);
int proxy_client_recv(struct proxy_client *client);
int proxy_client_send(struct proxy_client *client,
        const char *data, size_t len);
int proxy_client_close(struct proxy_client *client);

#endif /* PROXY_CLIENT_H */
