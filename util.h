// vi: expandtab sts=4 ts=4 sw=4
#ifndef UTIL_H
#define UTIL_H

#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define SIZE(array) (sizeof(array) / sizeof((array)[0]))

const char *sprint_addrport(struct sockaddr *addr);

#endif /* UTIL_H */
