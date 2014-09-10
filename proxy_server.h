#ifndef PROXY_SERVER_H
#define PROXY_SERVER_H

struct proxy_server *proxy_server_new();
int proxy_server_listen(struct proxy_server *ps, int port);

#endif /* PROXY_SERVER_H */

