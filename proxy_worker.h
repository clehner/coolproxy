// vi: expandtab sts=4 ts=4 sw=4
#ifndef PROXY_WORKER_H
#define PROXY_WORKER_H

struct proxy_worker {
	struct proxy_worker *next;
	struct proxy_worker *prev;
	char *host;
	unsigned short port;
	bool idle;
};

struct proxy_worker *proxy_worker_new(const char *host, unsigned short port);
void proxy_worker_free(struct proxy_worker *worker);

#endif /* PROXY_WORKER_H */

