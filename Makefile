IP_SERVER=127.0.0.1
PORT=49011

build: server subscriber
CFLAGS = -Wall -Wextra -g

all: server subscriber

server: server.c util.h
	gcc $(CFLAGS) server.c util.c -o server

subscriber: subscriber.c util.h
	gcc $(CFLAGS) subscriber.c util.c -o subscriber -lm

.PHONY: clean run_server

run_server: server
	./server ${PORT}

clean:
	rm -f server subscriber