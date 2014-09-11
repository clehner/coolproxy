// vi: expandtab sts=4 ts=4 sw=4
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include "util.h"

/* sprint_addrport - Print a human-readable address and port to a string.
 * Returns a pointer to a static buffer. Do not free it. Expect it to be
 * overwritten on next call */
const char *
sprint_addrport(struct sockaddr *in)
{
    static char buffer[INET6_ADDRSTRLEN + 9]; // port{max 5}, :, [], \0
    char *buffer6 = buffer + 1; // start at pos 1 to put a [ before ipv6 addrs
    buffer[0] = '[';
    uint16_t port = 0;

    struct sockaddr_in6 *addr = (struct sockaddr_in6 *)in;
    if (addr->sin6_family == AF_INET) {
        struct sockaddr_in *addr4 = (struct sockaddr_in *)in;
        if (!inet_ntop(AF_INET, &(addr4->sin_addr), buffer, INET_ADDRSTRLEN)) {
            perror("inet_ntop");
            buffer[0] = '\0';
        }
        port = addr4->sin_port;
    } else {
        if (!inet_ntop(AF_INET6, &(addr->sin6_addr), buffer6, INET6_ADDRSTRLEN)) {
            perror("inet_ntop");
            buffer6[0] = '\0';
        }
        port = addr->sin6_port;
    }

    int len = strlen(buffer);
    if (addr->sin6_family == AF_INET6) {
        buffer[len] = ']';
        len++;
    }
    buffer[len] = ':';
    len++;

    snprintf(buffer + len, 6, "%d", ntohs(port));

    return buffer;
}