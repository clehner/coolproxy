#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include "proxy_server.h"

int main(int argc, char *argv[])
{
	struct proxy_server *ps;
	int port;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 0;
    }

    port = atoi(argv[1]);

	ps = proxy_server_new();
	if (!ps) {
		fprintf(stderr, "Unable to create proxy server.\n");
		return 1;
	}

	proxy_server_listen(ps, port);
	if (!ps) {
		fprintf(stderr, "Unable to start proxy server.\n");
		return 1;
	}

	return proxy_server_run(ps);
}
