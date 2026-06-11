#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include "logger.h"

/* ── Internal state ────────────────────────────────────────── */

static FILE           *s_log_fp       = NULL;
static pthread_mutex_t s_log_mutex    = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t s_count_mutex  = PTHREAD_MUTEX_INITIALIZER;

/* shared buffer read by the OpenGL thread */
static char s_last_line[512]  = "(no events yet)";
static int  s_client_count    = 0;

/* ── Helpers ───────────────────────────────────────────────── */

static void make_timestamp(char *buf, int buflen)
{
    time_t     now = time(NULL);
    struct tm *tm  = localtime(&now);
    strftime(buf, (size_t)buflen, "%Y-%m-%d %H:%M:%S", tm);
}

static void write_line(const char *line)
{
    /* caller already holds s_log_mutex */
    printf("%s\n", line);
    if (s_log_fp) {
        fprintf(s_log_fp, "%s\n", line);
        fflush(s_log_fp);
    }
    /* snapshot for OpenGL display — safe because we hold the mutex */
    strncpy(s_last_line, line, sizeof(s_last_line) - 1);
    s_last_line[sizeof(s_last_line) - 1] = '\0';
}

/* ── Public API ────────────────────────────────────────────── */

void logger_init(const char *log_file)
{
    pthread_mutex_lock(&s_log_mutex);
    if (log_file) {
        s_log_fp = fopen(log_file, "a");
        if (!s_log_fp)
            fprintf(stderr, "logger_init: cannot open '%s' — stdout only\n",
                    log_file);
    }
    pthread_mutex_unlock(&s_log_mutex);
}

void logger_close(void)
{
    pthread_mutex_lock(&s_log_mutex);
    if (s_log_fp) {
        fclose(s_log_fp);
        s_log_fp = NULL;
    }
    pthread_mutex_unlock(&s_log_mutex);
}

/* Server-side: [TIMESTAMP] TID=<tid>  <ip>:<port>  <event> */
void log_event(const char *ip, int port, const char *event)
{
    char ts[32];
    make_timestamp(ts, sizeof(ts));

    pthread_t tid = pthread_self();

    char line[768];
    snprintf(line, sizeof(line),
             "[%s] TID=%-10lu  %-15s:%5d  %s",
             ts, (unsigned long)tid,
             ip   ? ip   : "-",
             port,
             event ? event : "");

    pthread_mutex_lock(&s_log_mutex);
    write_line(line);
    pthread_mutex_unlock(&s_log_mutex);
}

/* Client-side: [TIMESTAMP] CLIENT  <event> */
void log_client(const char *event)
{
    char ts[32];
    make_timestamp(ts, sizeof(ts));

    char line[640];
    snprintf(line, sizeof(line),
             "[%s] CLIENT  %s",
             ts, event ? event : "");

    pthread_mutex_lock(&s_log_mutex);
    write_line(line);
    pthread_mutex_unlock(&s_log_mutex);
}

/* OpenGL helpers */
void logger_get_last_line(char *buf, int buflen)
{
    pthread_mutex_lock(&s_log_mutex);
    strncpy(buf, s_last_line, (size_t)(buflen - 1));
    buf[buflen - 1] = '\0';
    pthread_mutex_unlock(&s_log_mutex);
}

int logger_get_client_count(void)
{
    pthread_mutex_lock(&s_count_mutex);
    int c = s_client_count;
    pthread_mutex_unlock(&s_count_mutex);
    return c;
}

void logger_update_client_count(int delta)
{
    pthread_mutex_lock(&s_count_mutex);
    s_client_count += delta;
    pthread_mutex_unlock(&s_count_mutex);
}