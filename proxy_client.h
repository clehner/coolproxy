// vi: expandtab sts=4 ts=4 sw=4
#ifndef PROXY_CLIENT_H
#define PROXY_CLIENT_H

#include "eventloop.h"

struct proxy_client;
struct proxy_server;

struct proxy_client *proxy_client_new(struct eventloop *loop,
		struct proxy_server *server, int sockfd);
int proxy_client_read(struct proxy_client *ps);

#endif /* PROXY_CLIENT_H */



