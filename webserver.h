#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <stdint.h>

struct Webserver {
    int socket_fd;
    /* Enables verbose logging. */
    _Bool verbose;
    /**
     * A path that will be prepended to all requested paths.
     * As an example, setting this to "public" will link "hostname/index.html" to "hostname/public/index.html" under the hood.
     */
    char *root_dir;
    /**
     * The file that `${root_dir}/` points at.
     */
    char *root_file;
};

/* 1 indicates failure, 0 indicates success */
_Bool webserver_new(struct Webserver *webserver, uint16_t port, char *root_dir, char *root_file, _Bool verbose);
_Bool webserver_listen(struct Webserver webserver);

enum Method {
    METHOD_GET,
    METHOD_HEAD
};

struct Header {
    char *key;
    char *value;
};

struct Request {
    /* Mandatory. */
    enum Method method;
    /* Mandatory. */
    char *path;
    /* Optional and may be zero. */
    struct Header *headers;
    unsigned headers_len;
    /* Optional and may be zero. */
    char *body; // TODO: this will need `body_len`! `0` could be in the body
};

struct Response {
    unsigned status_code;
    char *reason_phrase;
};

#endif /* WEBSERVER_H */
