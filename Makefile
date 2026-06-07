CC      = gcc
CFLAGS  = -Wall -Wextra -g -pthread
LDFLAGS = -pthread

.PHONY: all clean

all: server client

# ── Server ────────────────────────────────────────────────────
server: server.o config.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

server.o: server.c config.h
	$(CC) $(CFLAGS) -c -o $@ server.c

# ── Client ────────────────────────────────────────────────────
client: client.o config.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

client.o: client.c config.h
	$(CC) $(CFLAGS) -c -o $@ client.c

# ── Shared ────────────────────────────────────────────────────
config.o: config.c config.h
	$(CC) $(CFLAGS) -c -o $@ config.c

# ── Clean ─────────────────────────────────────────────────────
clean:
	rm -f server.o client.o config.o server client