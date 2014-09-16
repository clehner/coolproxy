// vi: expandtab sts=4 ts=4 sw=4
#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H

#include "util.h"

enum parser_mode {
    parser_mode_new,
    parser_mode_headers,
    parser_mode_body
};

enum http_scheme {
    http_scheme_http,
    http_scheme_other
};

struct body_msg {
    size_t len;
    const char *msg;
};


struct http_version {
    char major_version;
    char minor_version;
};

struct http_uri {
    enum http_scheme scheme;
    char *host;
    unsigned short int port;
    char *path;
};

struct http_parser_callbacks {
    callback_fn on_status;
    callback_fn on_request;
    callback_fn on_header;
    callback_fn on_body;
};

struct http_parser_status {
    struct http_version http_version;
    unsigned short code;
    char *reason;
};

struct http_parser_request {
    char *method;
    struct http_uri uri;
    struct http_version http_version;
};

struct http_parser_header {
    char *name;
    char *value;
};

struct http_parser {
    char line_buffer[256];
    enum parser_mode mode;
    struct http_parser_callbacks *callbacks;
    void *obj;
};

void parser_init(struct http_parser *parser, struct http_parser_callbacks *cbs,
        void *obj);
int parser_parse(struct http_parser *parser, const char *buf, size_t len);

#endif /* HTTP_PARSER_H */
