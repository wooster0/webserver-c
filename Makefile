# std: gnu99 includes GNU extensions while c99 does not
FLAGS=-Wall -Wextra -Wpedantic -Werror -Wshadow -Wno-unused-function -std=gnu99

build:
	cc main.c webserver.c -o webserver $(FLAGS)

run: build
	./webserver

run-verbose: build
	./webserver --verbose
