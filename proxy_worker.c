// vi: expandtab sts=4 ts=4 sw=4
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "proxy_worker.h"

struct proxy_worker *proxy_worker_new(const char *host, unsigned short port) {
    // Store the host string contiguous with the struct in memory
    size_t host_len = strlen(host) + 1;
    struct proxy_worker *worker = calloc(1, sizeof(struct proxy_worker));
    if (!worker) return NULL;

    worker->port = port;
    worker->host = (char *)malloc(host_len * sizeof(char));
    strncpy(worker->host, host, host_len);

    worker->idle = false;
    worker->connected = false;

    return worker;
}

void proxy_worker_free(struct proxy_worker *worker) {
    // Remove the worker from the linked list
    if (worker->next) {
        worker->next->prev = worker->prev;
    }
    worker->prev->next = worker->next;

    free(worker->host);
    free(worker);
}

int proxy_worker_connect(struct proxy_worker *worker) {
    struct addrinfo hints = {0};
    struct addrinfo *addrs, *rp;
    int sfd, status;
    char port[6];

    // Convert the port to string for getaddrinfo
    if (snprintf(port, sizeof port, "%hd", worker->port) < 1) {
        return -1;
    }

    hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
    hints.ai_flags = AI_PASSIVE;    /* For wildcard IP address */
    hints.ai_protocol = 0;          /* Any protocol */
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;

    printf("host1: %s\n", worker->host);

    // Resolve hostname to address.
    if ((status = getaddrinfo(worker->host, port, &hints, &addrs))) {
        fprintf(stderr, "getaddrinfo error: %s (%s, %s)\n",
                gai_strerror(status), worker->host, port);
        printf("host2: %s\n", worker->host);
        return -1;
    }


    /* getaddrinfo() returns a list of address structures.
        Try each address until we successfully bind(2).
        If socket(2) (or bind(2)) fails, we (close the socket
        and) try the next address. */

    for (rp = addrs; rp != NULL; rp = rp->ai_next) {
        sfd = socket(rp->ai_family, rp->ai_socktype,
                rp->ai_protocol);
        if (sfd < 0) {
            perror("socket");
            continue;
        }

        if (connect(sfd, rp->ai_addr, rp->ai_addrlen) == 0) {
            // Success
            worker->fd = sfd;
            break;
        }

        close(sfd);
    }

    if (rp == NULL) {
        // No address succeeded
        fprintf(stderr, "Could not connect to %s:%s\n", worker->host, port);
        freeaddrinfo(addrs);
        return -2;
    }

    worker->connected = true;
    freeaddrinfo(addrs);
    return 0;
}
