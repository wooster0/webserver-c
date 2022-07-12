CFLAGS_WARNINGS=-Wall -Wextra -Wpedantic -Werror -Wshadow -Wno-unused-function
#CLFAGS=$(CFLAGS_WARNINGS) -std=c99

build:
	cc main.c webserver.c -o webserver $(CLFAGS)

run: build
	./webserver

run-verbose: build
	./webserver --verbose
