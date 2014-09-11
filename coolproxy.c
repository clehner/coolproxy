// vi: expandtab sts=4 ts=4 sw=4
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include "eventloop.h"
#include "proxy_server.h"

int main(int argc, char *argv[])
{
    struct proxy_server *ps;
    struct eventloop *loop;
    int port;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 0;
    }

    port = atoi(argv[1]);

    if (!(loop = eventloop_new())) {
        perror("eventloop_new");
        return 1;
    }

    ps = proxy_server_new(loop);
    if (!ps) {
        fprintf(stderr, "Unable to create proxy server.\n");
        return 1;
    }

    proxy_server_listen(ps, port);
    if (!ps) {
        fprintf(stderr, "Unable to start proxy server.\n");
        return 1;
    }

    return eventloop_run(loop);
}
