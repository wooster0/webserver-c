# webserver

A basic HTTP/1.1 webserver written in C.

# What we want

A basic HTTP webserver that can handle HTTP GET and HEAD requests.
It should be capable of serving files to a browser,
such as HTML, CSS, JS, images, and fonts.

It should allow specifiying a specific port for the webserver to run on
as well as two parameters specifiying a root file and a root directory path,
used when no route is specified in the URL.
A `root_dir` parameter should allow configuring the
webserver in such a way that `hostname/` points to for example a directory
called "public" which contains the website's content (i.e. `hostname/public/`).
A parameter called `root_file` should be a file name for a file inside the directory
specified by `root_dir` to be used when no file is specified in the URL.
In other words `hostname/` -> `hostname/root_dir/` -> `hostname/root_dir/root_file`
so that for example if your webserver runs at `localhost:8080` and a user visits that page,
they will see the `index.html` contained in `public/` without having to actually
specify `localhost:8080/public/index.html`
(which wouldn't work anyway because that would point to `localhost:8080/public/public/index.html`).

Additionally, for the user's convenience the webserver binary should accept an argument `--verbose`
to enable verbose output in the terminal.

# How we do it

The server is written in C for Linux and handles GET and HEAD
HTTP (not HTTPS) requests over TCP/IP connections, on any port.
Because the only methods that webservers are required to support are GET and HEAD
and because this is supposed to be a simple webserver,
these will be the only methods supported for now.
For all other methods we return 501 Not Implemented.

1. `webserver_new`: on startup we call `socket(AF_INET, SOCK_STREAM, 0)` to obtain a
   file descriptor (`int`) to an IPv4 TCP/IP socket.
   This represents the server's socket. Next we assign an address to it using `bind`.
   In our case we assign `localhost` + any port the user chooses.
   Finally, in order to accept connections to our server socket,
   we call `listen(fd, n)` where *n* is the amount of connections that can be enqueued
   at a time. Connections beyond that limit will be rejected.
2. `webserver_listen`: next we start listening for new connections in a loop.
   We pass our server's socket file descriptor to `accept` and then this call
   will block (wait) until a new connection from a client comes in.
   This will return a new socket file descriptor for the client as well as their
   socket address.
   Using `recv` we read their request into a buffer of a specific length
   and then parse the request.

   Here's an example of a basic HTTP request:
   ```
   GET /index.html HTTP/1.1**CRLF**
   Host: localhost:8080**CRLF**
   Content-Length: 31
   **CRLF**
   this is the body of the request
   ```
   (sending a body in a GET request is usually not useful but that's besides the point)
   It consists of a **request line** which consists of a **method**,
   the **request target** (path), and the **HTTP version**.
   Next we have **header fields**: a **header name** followed by the **header's value**.
   Finally, there can be a **body** (arbitrary data) of any length.
   The body's length is often specified using a **`Content-Length`** header.
   Only the request line is mandatory. The headers and the body are optional.

   The way HEAD requests are handled is that their treated the same as a GET request
   except that their responded to without a body.

   For unknown paths we return 404 Not Found.

# What makes it a **web**server?

It serves the web using HTTP (Hypertext Transfer Protocol).
HTTP is the foundation of data communication for the World Wide Web.

# Limitations

* Requests cannot be arbitrarily large (depends on buffer size)

# Why C?

Just for practice and because it turned 50 this year (as of 2022).
I don't actually think it's very suitable for this project (or any project really).
There are much better languages nowadays that offer more safety, efficiency, and less footguns.
In fact this webserver still suffers from memory leaks and refactoring C code
without breaking anything accidentally is incredibly hard.

# References

* https://httpwg.org/specs/rfc7230.html
* https://www.ibm.com/docs/en/cics-ts/5.2?topic=protocol-http-responses
* https://developer.mozilla.org/en-US/docs/Web/HTTP/Status
* https://en.wikipedia.org/wiki/Hypertext_Transfer_Protocol
* https://developer.mozilla.org/en-US/docs/Web/HTTP/Basics_of_HTTP/MIME_types
* https://developer.mozilla.org/en-US/docs/Web/HTTP/Basics_of_HTTP/MIME_types/Common_types
