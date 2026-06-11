CC      = gcc
CFLAGS  = -Wall -Wextra -g -pthread
LDFLAGS = -pthread -lGL -lGLU -lglut

.PHONY: all test clean

all: server client

# ── Server ────────────────────────────────────────────────────
server: server.o config.o logger.o opengl_display.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

server.o: server.c config.h logger.h opengl_display.h
	$(CC) $(CFLAGS) -c -o $@ server.c

# ── Client ────────────────────────────────────────────────────
client: client.o config.o logger.o opengl_display.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

client.o: client.c config.h logger.h opengl_display.h
	$(CC) $(CFLAGS) -c -o $@ client.c

# ── Shared modules ────────────────────────────────────────────
config.o: config.c config.h
	$(CC) $(CFLAGS) -c -o $@ config.c

logger.o: logger.c logger.h
	$(CC) $(CFLAGS) -c -o $@ logger.c

opengl_display.o: opengl_display.c opengl_display.h logger.h
	$(CC) $(CFLAGS) -c -o $@ opengl_display.c

# ── Headless test binaries (no OpenGL, for CI / test_all.sh) ──
test: server_test client_test
	bash test_all.sh

server_test: server_t.o config_t.o logger_t.o
	$(CC) $(CFLAGS) -DNO_OPENGL -o server_test $^ -pthread

server_t.o: server.c config.h logger.h opengl_display.h
	$(CC) $(CFLAGS) -DNO_OPENGL -c -o $@ server.c

client_test: client_t.o config_t.o logger_t.o
	$(CC) $(CFLAGS) -DNO_OPENGL -o client_test $^ -pthread

client_t.o: client.c config.h logger.h opengl_display.h
	$(CC) $(CFLAGS) -DNO_OPENGL -c -o $@ client.c

config_t.o: config.c config.h
	$(CC) $(CFLAGS) -DNO_OPENGL -c -o $@ config.c

logger_t.o: logger.c logger.h
	$(CC) $(CFLAGS) -DNO_OPENGL -c -o $@ logger.c

# ── Clean ─────────────────────────────────────────────────────
clean:
	rm -f server.o client.o config.o logger.o opengl_display.o \
	      server_t.o client_t.o config_t.o logger_t.o \
	      server client server_test client_test \
	      update_v*.bin server.log client.log