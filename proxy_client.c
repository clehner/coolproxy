// vi: expandtab sts=4 ts=4 sw=4
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "eventloop.h"
#include "proxy_server.h"
#include "proxy_client.h"

struct proxy_client {
	int fd;
    struct eventloop *loop;
	struct proxy_server *server;
    struct callback recv_cb;
};

static int proxy_client_recv_cb(void *client, void *data);

struct proxy_client *proxy_client_new(struct eventloop *loop,
		struct proxy_server *server, int sockfd) {
    struct proxy_client *client = malloc(sizeof(struct proxy_client));
    if (!client) return NULL;

	client->fd = sockfd;
	client->loop = loop;
	client->server = server;

	client->recv_cb.obj = client;
	client->recv_cb.fn = proxy_client_recv_cb;

    // Register the server socket in the event loop
    if (eventloop_add(loop, sockfd, &client->recv_cb) < 0) {
        perror("eventloop_add");
        return NULL;
    }

	return client;
}

int proxy_client_recv(struct proxy_client *client) {
	size_t len = 0;
	char buf[512];

	len = recv(client->fd, buf, sizeof buf-1, 0);
	if (len == -1) {
		perror("recv");
		return 0;
	} else if (len == 0) {
		printf("Client disconnected\n");
		close(client->fd);
		// fd is automatically removed from epoll set when the socket is closed.
		// Notify the server that we closed
		proxy_server_notify_client_closed(client->server, client);
		return 0;
	} else {
		buf[len] = '\0';
		printf("got %zu bytes: \"%s\"\n", len, buf);
		return 0;
	}
}

static int proxy_client_recv_cb(void *client, void *data) {
    return proxy_client_recv((struct proxy_client *)client);
}

