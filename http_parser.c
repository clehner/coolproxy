// vi: expandtab sts=4 ts=4 sw=4
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include "http_parser.h"

void parser_init(struct http_parser *parser,
        struct http_parser_callbacks *callbacks) {
    parser->mode = parser_mode_new;
    parser->callbacks = callbacks;
}

void parse_http_version(enum http_version *version, char *version_str) {
    assert(version);
    if (!strcmp("HTTP/1.0", version_str)) {
        *version = http_version_1_0;
    } else if (!strcmp("HTTP/1.1", version_str)) {
        *version = http_version_1_1;
    } else {
        *version = http_version_unknown;
    }
};

void parser_handle_status_line(struct http_parser *parser, const char *buf) {
    char line[256];
    struct http_parser_status status;

    strncpy(line, buf, sizeof line);
    printf("got status line %s\n", line);
    sscanf(buf, "HTTP/1.1 %d %s", &status.code, status.reason);
    do_callback(&parser->callbacks->on_status, &status);
}

void parser_handle_request_line(struct http_parser *parser, const char *buf) {
    struct http_parser_request req;

    printf("got status line\n");

    // Get the first word of the line (request method)
    char *first = strchr(buf, ' ');
    if (!first) {
        fprintf(stderr, "Missing space\n");
        return;
    }
    *first = '\0';
    req.method = (char *)buf;
    req.uri = first+1;

    // Get the last word of the line (HTTP version)
    char *last = strrchr(req.uri, ' ');
    if (!last) {
        req.http_version = http_version_unknown;
    } else {
        *last = '\0';
        parse_http_version(&req.http_version, last+1);
    }

    //sscanf(buf, "%s %s HTTP/%s", req.method, req.uri, http_version_str);
    do_callback(&parser->callbacks->on_request, &req);
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

    do_callback(&parser->callbacks->on_header, &header);
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
