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
    parser->state = parser_state_new;
    parser->callbacks = callbacks;
    parser->obj = obj;
}

void parse_http_version(struct http_version *version, char *version_str) {
    if (!version_str || strncmp("HTTP/", version_str, 5)) {
        version->major_version = -1;
        version->minor_version = 0;
        return;
    }
    // Skip past the HTTP/
    version_str += 5;

    char *point = strchr(version_str, '.');
    if (point) {
        *point = '\0';
        version->minor_version = atoi(point+1);
    }
    version->major_version = atoi(version_str);
}

int parser_handle_status_line(struct http_parser *parser, const char *buf) {
    struct http_parser_status status;

    printf("got status line \"%s\"\n", buf);

    // Get the first word of the line (HTTP version)
    char *space = strchr(buf, ' ');
    if (!space) {
        fprintf(stderr, "Missing space\n");
        return -1;
    }
    *space = '\0';
    parse_http_version(&status.http_version, (char *)buf);
    buf = space+1;

    // Get the second word of the line (status code)
    space = strchr(buf, ' ');
    if (!space) {
        fprintf(stderr, "Missing second space\n");
        return -1;
    }
    *space = '\0';
    status.code = atoi(buf);
    status.reason = space+1;

    printf("HTTP/%hhi.%hhi %hd %s\n", status.http_version.major_version,
            status.http_version.minor_version, status.code, status.reason);
    parser_callback(parser, on_status, &status);
    return 0;
}

int parser_handle_request_line(struct http_parser *parser, const char *buf) {
    struct http_parser_request req;
    char *uri;

    printf("got request line \"%s\"\n", buf);

    // Get the first word of the line (request method)
    char *first = strchr(buf, ' ');
    if (!first) {
        fprintf(stderr, "Missing space\n");
        return -1;
    }
    *first = '\0';
    req.method = (char *)buf;
    uri = first+1;

    // Get the last word of the line (HTTP version)
    char *last = strrchr(uri, ' ');
    if (last) {
        *last = '\0';
    }
    parse_http_version(&req.http_version, last ? last+1 : NULL);

    // Look for scheme before "://"
    char *colon = strchr(uri, ':');
    if (colon && colon[1] == '/' && colon[2] == '/') {
        *colon = '\0';
        char *scheme = uri;
        uri = colon + 3;
        req.uri.scheme = !strcasecmp("http", scheme)
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
    return 0;
}

void parser_handle_header_line(struct http_parser *parser, const char *buf) {
    struct http_parser_header header;

    char *split = strchr(buf, ':');
    if (!split) {
        fprintf(stderr, "Missing colon in header line %s\n", buf);
        return;
    }
    *split = '\0';
    for (split++; *split == ' '; split++);
    header.name = (char *)buf;
    header.value = split;
    //printf("Received header %s: %s\n", buf, split);

    parser_callback(parser, on_header, &header);
}

// Get the next line of input, taking into account the line buffer.
// Returns 1 if read a full line, 0 if not
// If read a line, writes line to line and number of bytes to line_len
int parser_parse_line(struct http_parser *parser, const char *buf,
        size_t len, char *line, size_t *line_len, size_t *bytes_read) {

    // Look for the \r\n (or just \n)
    char *line_end = memchr(buf, '\n', len);

    if (!line_end) {
        // No line ending found. Append data to line buffer
        int buf_avail = sizeof(parser->line_buffer) - parser->line_buffer_len;
        if (buf_avail < len) {
            fprintf(stderr, "Line too long for buffer. Truncating.\n");
        } else {
            // Copy just len bytes
            buf_avail = len;
        }
        memcpy(parser->line_buffer + parser->line_buffer_len, buf, buf_avail);
        return len;
    }

    // next time this function is called, buf will be advanced by line_len
    *bytes_read = line_end - buf + 1;

    if (line_end > buf && *(line_end-1) == '\r') {
        // Got the carriage return before the newline
        line_end--;
    }
    *line_len = line_end - buf;
    if (line_end == buf) {
        // Got a blank line
    } else {
        memcpy(line, buf, line_end - buf);
    }
    line[line_end - buf] = '\0';
    //printf("line [%zu/%zu]: \"%s\" '%s'\n", *line_len, *bytes_read, line,buf);
    return 1;
}

// Parse stuff from bufer
// Returns number of bytes parsed in buf
size_t parser_parse_once(struct http_parser *parser, const char *buf,
        size_t len) {
    char line[512];
    size_t line_len;
    size_t len_read;

    switch (parser->state) {
        case parser_state_new:
            // Read status line
            if (!parser_parse_line(parser, buf, len, line, &line_len,
                        &len_read)) {
                return len_read;
            }

            int status;
            if (!strncmp("HTTP/", line, 5)) {
                status = parser_handle_status_line(parser, line);
            } else {
                status = parser_handle_request_line(parser, line);
            }
            if (status == 0) {
                parser->state = parser_state_headers;
            }
            return len_read;

        case parser_state_headers:
            // Read a header line
            if (!parser_parse_line(parser, buf, len, line, &line_len,
                        &len_read)) {
                return len_read;
            }

            if (line_len == 0) {
                //printf("Finished receiving headers\n");
                parser_callback(parser, on_header, NULL);
                // Receive body now
                parser->state = parser_state_body;
                break;
            }

            parser_handle_header_line(parser, line);
            return len_read;

        case parser_state_body:
            // Read response
            parser_callback(parser, on_body, &((struct body_msg){len, buf}));
            return len;
    }

    return 0;
}

int parser_parse(struct http_parser *parser, const char *buf, size_t len) {
    while (len > 0) {
        size_t len_parsed = parser_parse_once(parser, buf, len);
        if (len_parsed == 0) {
            return 1;
        }
        len -= len_parsed;
        buf += len_parsed;
    }
    return 0;
}
