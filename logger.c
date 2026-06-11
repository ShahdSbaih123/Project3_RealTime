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

static int  s_total_connections = 0;
static char s_last_client_ip[INET_ADDRSTRLEN] = "-";
static char s_last_client_action[128]         = "-";

void logger_increment_total_connections(void)
{
    pthread_mutex_lock(&s_count_mutex);
    s_total_connections++;
    pthread_mutex_unlock(&s_count_mutex);
}

int logger_get_total_connections(void)
{
    pthread_mutex_lock(&s_count_mutex);
    int t = s_total_connections;
    pthread_mutex_unlock(&s_count_mutex);
    return t;
}

void logger_set_last_client(const char *ip, const char *action)
{
    pthread_mutex_lock(&s_count_mutex);
    if (ip)     { strncpy(s_last_client_ip, ip, INET_ADDRSTRLEN - 1);
                  s_last_client_ip[INET_ADDRSTRLEN - 1] = '\0'; }
    if (action) { strncpy(s_last_client_action, action, 127);
                  s_last_client_action[127] = '\0'; }
    pthread_mutex_unlock(&s_count_mutex);
}

void logger_get_last_client(char *ip_buf,     int ip_len,
                             char *action_buf, int action_len)
{
    pthread_mutex_lock(&s_count_mutex);
    strncpy(ip_buf,     s_last_client_ip,     ip_len     - 1);
    strncpy(action_buf, s_last_client_action, action_len - 1);
    ip_buf[ip_len - 1]         = '\0';
    action_buf[action_len - 1] = '\0';
    pthread_mutex_unlock(&s_count_mutex);
}