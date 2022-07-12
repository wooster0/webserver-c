#include <stdio.h>
#include <string.h>

#include "webserver.h"

int main(int argc, char const *argv[]) {
    _Bool verbose = 0;
    if (argc >= 2) {
        if (strcmp(argv[1], "--verbose") == 0) {
            verbose = 1;
            printf("verbose output enabled!\n");
        } else {
            fprintf(stderr, "use `--verbose` to enable verbose output\n");
            return 1;
        }
    }

    struct Webserver webserver;
    if (webserver_new(&webserver, 8083, "public/", "index.html", verbose) == 1) {
        return 1;
    }

    if (webserver_listen(webserver) == 1) {
        return 1;
    }
}

// TODO: shouldn't this kind of stuff be possible too?
// server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
//   request->send(200, "text/html", "<p>Hello World</p>");
// });

// server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request){
//   request->send(SPIFFS, "/favicon.png", "image/png");
// });
