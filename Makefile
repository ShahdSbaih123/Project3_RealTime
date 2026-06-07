CC      = gcc
CFLAGS  = -Wall -Wextra -g -pthread
LDFLAGS = -pthread

.PHONY: all clean

all: server

server: server.o config.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

server.o: server.c config.h
	$(CC) $(CFLAGS) -c -o $@ server.c

config.o: config.c config.h
	$(CC) $(CFLAGS) -c -o $@ config.c

clean:
	rm -f server.o config.o server
