#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <netinet/in.h>   


/* ── Initialise / teardown ─────────────────────────────────── */

/**
 * Open (append) the log file and initialise the internal mutex.
 * Pass NULL to log to stdout only.
 * Call once before any log_event / log_client calls.
 */
void logger_init(const char *log_file);

/** Flush and close the log file; destroy the mutex. */
void logger_close(void);

/* ── Server-side logging ───────────────────────────────────── */

/**
 * Write one log line for the server.
 * Format:  [TIMESTAMP] TID=<tid>  <ip>:<port>  <event>
 *
 * @param ip    Client IP string (pass "-" when no client is involved).
 * @param port  Client port (pass 0 when no client is involved).
 * @param event Human-readable event description.
 */
void log_event(const char *ip, int port, const char *event);

/* ── Client-side logging ───────────────────────────────────── */

/**
 * Write one log line for the client (no IP / port / TID needed).
 * Format:  [TIMESTAMP] CLIENT  <event>
 *
 * @param event Human-readable event description.
 */
void log_client(const char *event);

/* ── OpenGL display hook ───────────────────────────────────── */

/**
 * Copy the most-recent log line into a shared buffer so the
 * OpenGL display thread can read it without holding the log mutex.
 * Called internally by log_event(); exposed here for testing.
 */
void logger_get_last_line(char *buf, int buflen);

/** Return the current live-client counter (set by server). */
int  logger_get_client_count(void);

/** Update the live-client counter (call with +1 on connect, -1 on disconnect). */
void logger_update_client_count(int delta);

void logger_increment_total_connections(void);
int  logger_get_total_connections(void);
void logger_set_last_client(const char *ip, const char *action);
void logger_get_last_client(char *ip_buf, int ip_len,
                             char *action_buf, int action_len);

#endif /* LOGGER_H */