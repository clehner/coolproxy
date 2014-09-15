// vi: expandtab sts=4 ts=4 sw=4
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include "http_parser.h"

#define parser_callback(parser, cb, data) \
    parser->callbacks->cb ? parser->callbacks->cb(parser->obj, data) : 0

int parse_uri();

void parser_init(struct http_parser *parser,
        struct http_parser_callbacks *callbacks, void *obj) {
    parser->mode = parser_mode_new;
    parser->callbacks = callbacks;
    parser->obj = obj;
}

void parse_http_version(struct http_version *version, char *version_str) {
    if (!version_str || strncmp("HTTP/", version_str, 5)) {
        version->major_version = -1;
        version->minor_version = 0;
        return;
    }

    assert(version);
    assert(version_str);
    char *point = strchr(version_str, ' ');
    if (point) {
        *point = '\0';
        version->minor_version = atoi(point+1);
    }
    version->major_version = atoi(version_str);
}

void parser_handle_status_line(struct http_parser *parser, const char *buf) {
    char line[256];
    struct http_parser_status status;

    strncpy(line, buf, sizeof line);
    printf("got status line %s\n", line);
    sscanf(buf, "HTTP/1.1 %hd %s", &status.code, status.reason);
    parser_callback(parser, on_status, &status);
}

void parser_handle_request_line(struct http_parser *parser, const char *buf) {
    struct http_parser_request req;
    char *uri;

    // Get the first word of the line (request method)
    char *first = strchr(buf, ' ');
    if (!first) {
        fprintf(stderr, "Missing space\n");
        return;
    }
    *first = '\0';
    req.method = (char *)buf;
    uri = first+1;

    // Get the last word of the line (HTTP version)
    char *last = strrchr(uri, ' ');
    if (!last) {
        req.http_version.major_version = -1;
        req.http_version.minor_version = 0;
    } else {
        *last = '\0';
        parse_http_version(&req.http_version, last+1);
    }

    // Look for scheme before "://"
    char *colon = strchr(uri, ':');
    if (colon && colon[1] == '/' && colon[2] == '/') {
        *colon = '\0';
        char *scheme = uri;
        uri = colon + 3;
        req.uri.scheme = !strcmp("http", scheme)
            ? http_scheme_http
            : http_scheme_other;
    } else {
        // Be lenient and assume http if no scheme is given
        req.uri.scheme = http_scheme_http;
    }

    // Get the hostname, port and path
    char *slash = strchr(uri, '/');
    if (slash && slash != uri) {
        *slash = '\0';
        char *host = uri;
        uri = slash + 1;

        colon = strchr(host, ':');
        if (colon) {
            *colon = '\0';
            req.uri.port = atoi(colon + 1);
        } else {
            req.uri.port = 80;
        }
        req.uri.host = host;
    } else {
        req.uri.host = NULL;
    }

    // Note: the path has initial slash removed
    req.uri.path = uri;

    parser_callback(parser, on_request, &req);
}

void parser_handle_header_line(struct http_parser *parser, const char *buf) {
    struct http_parser_header header;

    char *split = strchr(buf, ':');
    if (!split) {
        fprintf(stderr, "Missing colon in header line\n");
        return;
    }
    *split = '\0';
    for (split++; *split == ' '; split++);
    header.name = (char *)buf;
    header.value = split;

    parser_callback(parser, on_header, &header);
}

int parser_parse(struct http_parser *parser, const char *buf, size_t len) {
    char *line_end;

    switch (parser->mode) {
        case parser_mode_new:
            // Read status line
            // Look for the \r\n (or just \n)
            line_end = memchr(buf, '\n', len);
            if (!line_end) {
                return 400;
            }
            if (line_end == buf) {
                // got a blank line
            } else if (*(line_end-1) == '\r') {
                // got the carriage return before the newline
                line_end--;
            }
            *line_end = '\0';
            if (!strncmp("HTTP/", buf, 5)) {
                parser_handle_status_line(parser, buf);
            } else {
                parser_handle_request_line(parser, buf);
            }
            parser->mode = parser_mode_headers;
            break;
        case parser_mode_headers:
            // Read headers

            line_end = memchr(buf, '\n', len);
            if (!line_end) {
                return 400;
            }
            if (line_end == buf) {
                // got a blank line
            } else if (*(line_end-1) == '\r') {
                // got the carriage return before the newline
                line_end--;
            }
            *line_end = '\0';

            if (buf == line_end) break;

            parser_handle_header_line(parser, buf);
            break;
        case parser_mode_body:
            // Read response
            fprintf(stderr, "headers\n");
            break;
    }

    return 0;
}
