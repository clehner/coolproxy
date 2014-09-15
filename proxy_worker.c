// vi: expandtab sts=4 ts=4 sw=4
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include "proxy_worker.h"

struct proxy_worker *proxy_worker_new(const char *host, unsigned short port) {
    // Store the host string contiguous with the struct in memory
    size_t host_len = strlen(host) + 1;
    size_t struct_size = sizeof(struct proxy_worker);
    struct proxy_worker *worker = calloc(1, struct_size + host_len);
    if (!worker) return NULL;

    worker->port = port;
    worker->host = (char *)(worker + struct_size);
    strncpy(worker->host, host, host_len);

    worker->idle = true;

    return worker;
}

void proxy_worker_free(struct proxy_worker *worker) {
    free(worker);
}

