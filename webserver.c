#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

#include "webserver.h"

_Bool webserver_new(struct Webserver *webserver, uint16_t port, char *root_dir, char *root_file, _Bool verbose) {
    if (root_dir[strlen(root_dir) - 1] != '/') {
        fprintf(stderr, "`root_dir` must end in a slash\n");
        return 1;
    }
    webserver->root_dir = root_dir;
    webserver->root_file = root_file;
    webserver->verbose = verbose;

    int socket_fd = socket(
        AF_INET,     /* IPv4 */
        SOCK_STREAM, /* TCP/IP */
        0
    );
    if (socket_fd == -1) {
        perror("obtaining socket file descriptor");
        return 1;
    }

    // now we have a socket, but it's not bound to any address yet!

    struct sockaddr_in socket_address;
    socket_address.sin_family = AF_INET; /* IPv4 */
    /* The port.
     * `htons` converts from host to network byte order.
     * I.e. it ensures endianness matches.
     * The TCP/IP standard network byte order is big-endian so
     * this will presumably convert from little-endian to big-endian.
     */
    if (verbose)
        printf("converted port 0x%4X to 0x%4X\n", port, htons(port));
    socket_address.sin_port = htons(port);
    const in_addr_t localhost = inet_addr("127.0.0.1");
    socket_address.sin_addr.s_addr = localhost; /* the address */

    if (bind(
        socket_fd,
        (struct sockaddr*)&socket_address,
        sizeof(socket_address)
    ) == -1) {
        perror("binding address to socket");
        return 1;
    }

    // now we have a socket with an address assigned but we're still not listening for connections

    if (listen(
        socket_fd,
        5 /* maximum amount of connections that can be enqueued */
    ) == -1) {
        perror("failed to open socket");
        return 1;
    }

    printf("server connection established at <http://localhost:%d>\n", port);

    webserver->socket_fd = socket_fd;
    return 0;
}

static struct Response parse_request(char *buf, unsigned len, struct Request *request, _Bool verbose);
static int handle_request(char *buf, int len, int fd, struct Webserver webserver);

/**
 * Listens to new connections and handles the requests.
 * Attempts to continue after errors.
 */
_Bool webserver_listen(struct Webserver webserver) {
    char buf[1024];
    int bytes_read, client_socket_fd;
    struct sockaddr_in client_socket;
    socklen_t client_socket_len;
    while (1) {
        client_socket_fd = accept(webserver.socket_fd, (struct sockaddr*)&client_socket, &client_socket_len);
        if (client_socket_fd == -1) {
            perror("accepting socket");
            continue;
        }

        printf("new socket connection from a client\n");

        bytes_read = recv(client_socket_fd, &buf, sizeof(buf), 0);
        if (bytes_read == -1) {
            perror("reading from accepted socket");
            continue;
        }

        printf("%s %s\n", webserver.root_dir, webserver.root_file);
        if (handle_request(buf, bytes_read, client_socket_fd, webserver) < 0) {
            fprintf(stderr, "error handling request\n");
        }
        printf("%s %s\n", webserver.root_dir, webserver.root_file);

        printf("closing socket\n");
        close(client_socket_fd);
    }
}

static int send_response(enum Method method, struct Response response, char *body, size_t body_len, char *content_type, int fd, _Bool verbose) {
    // for our purpose a HEAD request is the same as a GET request
    // but without a body if there is one
    switch (method) {
        case METHOD_GET:
            break;
        case METHOD_HEAD:
            body = 0;
            content_type = 0;
    }

    int bytes_written, total_bytes_written = 0;
    if ((bytes_written = dprintf(
        fd, "HTTP/1.1 %d %s\r\n",
        response.status_code, response.reason_phrase
    )) < 0) return -1;
    total_bytes_written += bytes_written;

    if (content_type) {
        if ((bytes_written = dprintf(
            fd, "Content-Type: %s\r\n",
            content_type
        )) < 0) return -1;
        total_bytes_written += bytes_written;
    }

    if (body_len) {
        if ((bytes_written = dprintf(
            fd, "Content-Length: %ld\r\n",
            body_len
        )) < 0) return -1;
        total_bytes_written += bytes_written;
    }

    if (body) {
        if ((bytes_written = dprintf(
            fd, "\r\n"
        )) < 0) return -1;
        total_bytes_written += bytes_written;
        // NOTE: we can *not* use dprintf and `%.*s` to format the binary data here
        //       because even though we specify a length it will still stop at the first
        //       0 it finds! There's no way around it. `%*.s` isn't a thing either.
        //       So we write the binary data using `send`.
        if ((bytes_written = send(fd, body, body_len, 0)) < 0)
            return -1;
        total_bytes_written += bytes_written;
    }

    if (verbose)
        printf(
            "sent %d %s with Content-Type: %s and a %ldB body (%d bytes)\n",
            response.status_code, response.reason_phrase,
            content_type, body_len, total_bytes_written
        );
    return bytes_written;
}

static char *get_content_type(char *path) {
    char *file_extension;
    // notice the extra 'r' in `strrchr` which starts scanning from the right
    // rather than the left like `strchr` does
    if ((file_extension = strrchr(path, '.'))) {
        if (strcmp(file_extension, ".html") == 0) {
            return "text/html";
        } else if (strcmp(file_extension, ".css") == 0) {
            return "text/css";
        } else if (strcmp(file_extension, ".ico") == 0) {
            return "image/x-icon";
        } else if (strcmp(file_extension, ".otf") == 0) {
            return "font/otf";
        } else if (strcmp(file_extension, ".js") == 0) {
            return "text/javascript";
        } else if (strcmp(file_extension, ".png") == 0) {
            return "image/png";
        } else {
            return 0;
        }
    } else {
        return 0;
    }
}

static int handle_request(char *buf, int len, int fd, struct Webserver webserver) {
    struct Request request;
    struct Response response = parse_request(buf, len, &request, webserver.verbose);

    if (webserver.verbose) {
        printf(
            "received request:\n  method: %d\n  path: %s\n",
            request.method, request.path
        );
        printf("  headers:\n");
        for (unsigned i = 0; i < request.headers_len; i++) {
            struct Header header = request.headers[i];
            printf("    #%d: %s: %s\n", i, header.key, header.value);
        }
        printf("  body: %s\n", request.body);
    }

    if (response.status_code == 200) {
        // TODO: parse the path in a slighly more sophisticated way
        if (request.path[0] == '/')
            request.path++;

        // TODO: a cache to keep file content in memory
        int path_len = strlen(webserver.root_dir) + strlen(request.path);
        char *path = malloc(path_len);
        if (!path) {
            fprintf(stderr, "memory allocation failed\n");
            exit(1);
        }
        memcpy(path, webserver.root_dir, strlen(webserver.root_dir));
        memcpy(path + strlen(webserver.root_dir), request.path, strlen(request.path));
        path[path_len] = 0;

        if (strcmp(path, webserver.root_dir) == 0) {
            memcpy(path + path_len, webserver.root_file, strlen(webserver.root_file));
            path_len += strlen(webserver.root_file);
        }

        if (path[strlen(path) - 1] == '/')
            path[strlen(path) - 1] = 0;

        if (webserver.verbose)
            printf("opening requested file \"%s\"\n", path);
        FILE *file;
        if (!(file = fopen(path, "r"))) {
            perror("opening requested file");
            response.status_code = 404;
            response.reason_phrase = "Not Found";
            return send_response(request.method, response, 0, 0, 0, fd, webserver.verbose);
        }

        // seek to the end of the file to figure out the length for preallocation
        if (fseek(file, 0, SEEK_END) == -1) {
            perror("seeking to the end");
            return -1;
        }
        long file_len;
        if ((file_len = ftell(file)) == -1) {
            perror("getting file length");
            return -1;
        }
        rewind(file); // now go back to the beginning

        char *body = malloc(file_len + 1);
        size_t body_len = fread(body, 1, file_len, file);
        if (body_len != (unsigned long)file_len) {
            perror("reading requested file");
            exit(1);
        }
        if (fclose(file)) {
            perror("closing requested file");
            exit(1);
        }
        char *content_type = get_content_type(request.path);
        if (webserver.verbose)
            printf("Content-Type: %s\n", content_type);
        int bytes_written = send_response(request.method, response, body, body_len, content_type, fd, webserver.verbose);
        return bytes_written;
    } else {
        return send_response(request.method, response, 0, 0, 0, fd, webserver.verbose);
    }
}

/**
 * Skips all optional whitespace (OWS) and returns the final index.
 */
static unsigned next_optional_whitespace(char *buf, char *end) {
    char *init_buf = buf;
    while (buf < end && (*buf == ' ' || *buf == '\t'))
        buf++;
    return buf - init_buf;
}

/**
 * Advances until CRLF is reached and returns the final index.
 */
static unsigned next_crlf(char *buf, char *end) {
    char *init_buf = buf;
    for (; buf < end; buf++) {
        if (*buf == '\r') {
            buf++;
            if (buf < end && *buf == '\n')
                break;
            else
                buf--;
        }
    }
    return buf - init_buf;
}

static _Bool is_crlf(char *buf, char *end) {
    if (buf < end && *buf == '\r') {
        buf++;
        if (buf < end && *buf == '\n')
            return 1;
        else
            buf--;
    }
    return 0;
}

// TODO: take in a data-streaming reader that receives more bytes as necessary?
/**
 * Parses a request.
 * https://httpwg.org/specs/rfc7230.html#http.message
 */
static struct Response parse_request(char *buf, unsigned len, struct Request *request, _Bool verbose) {
    (void)verbose;
    unsigned i;
    char *end = buf + len, c;

    // first we parse the request line
    // https://httpwg.org/specs/rfc7230.html#request.line

    /* only these 2 properties are optional */
    request->headers = 0; request->headers_len = 0;
    request->body = 0;

    // parse the case-sensitive request method
    char method_s[4]; // suffices for "GET" and "HEAD"
    for (i = 0; i < sizeof(method_s) && buf < end && (c = *buf) != ' '; buf++)
        method_s[i++] = c;
    method_s[i] = 0;
    if (strcmp(method_s, "GET") == 0) {
        request->method = METHOD_GET;
    } else if (strcmp(method_s, "HEAD") == 0) {
        request->method = METHOD_HEAD;
    } else {
        fprintf(stderr, "invalid method \"%s\"\n", method_s);
        struct Response response = {
            .status_code = 501,
            .reason_phrase = "Not Implemented",
        };
        return response;
    }

    buf++; // skip space

    // read path (also called the request target)
    // https://httpwg.org/specs/rfc7230.html#request-target
    request->path = 0;
    for (i = 0; buf < end && (c = *buf) != ' '; buf++) {
        if (!(request->path = realloc(request->path, ++i + 1))) {
            fprintf(stderr, "memory allocation failed\n");
            exit(1);
        }
        request->path[i - 1] = c;
    }
    if (!request->path) {
        fprintf(stderr, "reading path failed\n");
        struct Response response = {
            .status_code = 400,
            .reason_phrase = "Bad Request",
        };
        return response;
    }
    request->path[i] = 0;

    buf++; // skip space

    // validate HTTP version
    if (!memcmp(buf, "HTTP/1.1", end - buf)) {
        fprintf(stderr, "only HTTP/1.1 supported!\n");
        struct Response response = {
            .status_code = 505,
            .reason_phrase = "HTTP Version Not Supported",
        };
        return response;
    }

    buf += next_crlf(buf, end);
    buf++; // skip the carriage return

    // the generic response
    struct Response ok = {
        .status_code = 200,
        .reason_phrase = "OK",
    };

    // parse header fields
    // https://httpwg.org/specs/rfc7230.html#header.fields
    // (`request->headers` and `request->headers_len` have been zeroed above)
    while (1) {
        struct Header header = { 0 };

        for (i = 0; buf < end && (c = *buf) != ':'; buf++) {
            if (!(header.key = realloc(header.key, ++i + 1))) {
                fprintf(stderr, "memory allocation failed\n");
                exit(1);
            }
            header.key[i - 1] = c;
        }
        if (!header.key)
            return ok;
        header.key[i] = 0;
        buf++; // skip colon

        buf += next_optional_whitespace(buf, end);

        // TODO: currently we are including the possible trailing OWS
        for (i = 0; !is_crlf(buf, end); buf++) {
            if (!(header.value = realloc(header.value, ++i))) {
                fprintf(stderr, "memory allocation failed\n");
                exit(1);
            }
            header.value[i - 1] = *buf;
        }
        if (!header.value)
            return ok;
        header.value[i] = 0;
        buf += 2; // skip the CRLF

        // do we have another CRLF?
        if (is_crlf(buf, end))
            // we have reached the end of the header fields
            break;

        if (!(request->headers = realloc(request->headers, (++request->headers_len) * sizeof(struct Header)))) {
            fprintf(stderr, "memory allocation failed\n");
            exit(1);
        }
        request->headers[request->headers_len - 1] = header;
    }

    return ok;
}
