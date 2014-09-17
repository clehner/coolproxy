// vi: expandtab sts=4 ts=4 sw=4
#ifndef PROXY_WORKER_H
#define PROXY_WORKER_H

#include "http_parser.h"

struct msg {
    char *data;
    size_t len;
    struct msg *next;
};

struct proxy_worker {
    struct proxy_worker *next;
    struct proxy_worker *prev;
    struct callback connect_cb;
    struct callback recv_cb;
    struct callback close_cb;
    char *host;
    unsigned short port;
    bool idle;
    bool connected;
    eventloop_t loop;
    int fd;
    struct addrinfo *addrs, *rp; // resolved addresses for connects in progress
    struct msg *send_queue; // messages to send once connected
    struct http_parser parser; // parser for incoming data
};

struct proxy_worker *proxy_worker_new(const char *host, unsigned short port,
        eventloop_t loop);
void proxy_worker_free(struct proxy_worker *worker);
int proxy_worker_resolve(struct proxy_worker *worker);
int proxy_worker_connect(struct proxy_worker *worker);
int proxy_worker_try_connect(struct proxy_worker *worker);
int proxy_worker_connected(struct proxy_worker *worker);
int proxy_worker_recv(struct proxy_worker *worker);
int proxy_worker_request(struct proxy_worker *worker, const char *method,
        const char *uri);
void proxy_worker_flush_queue(struct proxy_worker *worker);
int proxy_worker_send(struct proxy_worker *worker, const char *data,
        size_t len);
int proxy_worker_close(struct proxy_worker *worker);
int proxy_worker_dissociate(struct proxy_worker *worker);
int proxy_worker_on_http_request(struct proxy_worker *worker,
        struct http_parser_header *header);
int proxy_worker_on_http_header(struct proxy_worker *worker,
        struct http_parser_header *header);

#endif /* PROXY_WORKER_H */

