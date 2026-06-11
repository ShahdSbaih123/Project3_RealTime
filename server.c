//ready
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "config.h"
#include "logger.h"
#ifndef NO_OPENGL
#include "opengl_display.h"
#endif

/* ── Global state ─────────────────────────────────────────── */
static ServerConfig g_cfg;
static int          g_server_fd = -1;

#ifndef NO_OPENGL
/* argc/argv forwarded to GLUT (set in main before GL thread starts) */
static int    g_gl_argc;
static char **g_gl_argv;
#endif

/* ── Per-client thread argument ───────────────────────────── */
typedef struct {
    int  client_fd;
    char client_ip[INET_ADDRSTRLEN];
    int  client_port;
} ClientArg;

/* ── Signal handler ───────────────────────────────────────── */
static void handle_signal(int sig)
{
    const char *msg = "\n[SERVER] Shutdown signal received. Closing...\n";
    write(STDOUT_FILENO, msg, strlen(msg));
    if (g_server_fd != -1) close(g_server_fd);
    logger_close();
    _exit(0);
    (void)sig;
}

/* ── write_all helper ─────────────────────────────────────── */
static int write_all(int fd, const char *buf, size_t len)
{
    size_t total = 0;
    while (total < len) {
        ssize_t w = write(fd, buf + total, len - total);
        if (w < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        total += (size_t)w;
    }
    return 0;
}

/* ── Send update file to client ───────────────────────────── */
static int send_update_file(int fd, const char *ip, int port)
{
    FILE *fp = fopen(g_cfg.update_file, "rb");
    if (!fp) {
        log_event(ip, port, "ERROR: update file not found");
        dprintf(fd, "ERROR: update file unavailable\n");
        return -1;
    }

    struct stat st;
    if (stat(g_cfg.update_file, &st) != 0) {
        log_event(ip, port, "ERROR: stat() on update file failed");
        fclose(fp);
        return -1;
    }
    long file_size = (long)st.st_size;

    dprintf(fd, "SIZE:%ld\n", file_size);

    char *buf = malloc(g_cfg.buffer_size);
    if (!buf) {
        log_event(ip, port, "ERROR: malloc failed in send_update_file");
        fclose(fp);
        return -1;
    }

    long   sent = 0;
    size_t n;
    while ((n = fread(buf, 1, (size_t)g_cfg.buffer_size, fp)) > 0) {
        if (write_all(fd, buf, n) != 0) {
            log_event(ip, port, "ERROR: write failed during file transfer");
            free(buf);
            fclose(fp);
            return -1;
        }
        sent += (long)n;
    }

    free(buf);
    fclose(fp);

    char msg[128];
    snprintf(msg, sizeof(msg),
             "Transfer complete: %ld / %ld bytes sent", sent, file_size);
    log_event(ip, port, msg);
    return 0;
}

/* ── Per-client handler thread ────────────────────────────── */
static void *handle_client(void *arg)
{
    ClientArg *ca   = (ClientArg *)arg;
    int        fd   = ca->client_fd;
    char       ip[INET_ADDRSTRLEN];
    int        port = ca->client_port;
    strncpy(ip, ca->client_ip, INET_ADDRSTRLEN - 1);
    ip[INET_ADDRSTRLEN - 1] = '\0';
    free(ca);

    log_event(ip, port, "Connected");
    logger_increment_total_connections();
    logger_set_last_client(ip, "Connected");

    /* read one VERSION:<n>\n line */
    char    line[256] = {0};
    ssize_t r         = 0;
    int     total     = 0;

    while (total < (int)(sizeof(line) - 1)) {
        r = read(fd, line + total, 1);
        if (r <= 0) break;
        if (line[total] == '\n') { total++; break; }
        total++;
    }

    if (r <= 0 || total == 0) {
        log_event(ip, port, "ERROR: no data received — closing connection");
        close(fd);
        logger_update_client_count(-1);
        return NULL;
    }

    log_event(ip, port, "Version request received");

    int client_ver = -1;
    if (sscanf(line, "VERSION:%d", &client_ver) != 1) {
        log_event(ip, port, "ERROR: malformed request (expected VERSION:<n>)");
        dprintf(fd, "ERROR: invalid request format\n");
        close(fd);
        logger_update_client_count(-1);
        return NULL;
    }

    char detail[128];
    snprintf(detail, sizeof(detail),
             "Client version=%d  server latest=%d",
             client_ver, g_cfg.latest_version);
    log_event(ip, port, detail);

    if (client_ver < g_cfg.latest_version) {
        log_event(ip, port, "Decision: UPDATE_AVAILABLE — sending file");
        dprintf(fd, "UPDATE_AVAILABLE\n");
        logger_set_last_client(ip, "Downloading update");

        if (send_update_file(fd, ip, port) != 0)
            log_event(ip, port, "ERROR: file transfer failed");
    } else {
        log_event(ip, port, "Decision: UP_TO_DATE");
        dprintf(fd, "UP_TO_DATE\n");
        logger_set_last_client(ip, "Already up to date");
    }

    close(fd);
    log_event(ip, port, "Disconnected");
    logger_update_client_count(-1);
    return NULL;
}

/* ── Create + bind + listen ───────────────────────────────── */
static int create_server_socket(int port)
{
    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd < 0) { perror("socket"); return -1; }

    int opt = 1;
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons((uint16_t)port);

    if (bind(sfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind"); close(sfd); return -1;
    }
    if (listen(sfd, g_cfg.max_clients) < 0) {
        perror("listen"); close(sfd); return -1;
    }
    return sfd;
}

/* ── Accept loop ──────────────────────────────────────────── */
static void accept_loop(int server_fd)
{
    char startup[64];
    snprintf(startup, sizeof(startup), "Listening on port %d", g_cfg.port);
    log_event("-", 0, startup);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);

        int cfd = accept(server_fd,
                         (struct sockaddr *)&client_addr, &addr_len);
        if (cfd < 0) {
            if (errno == EINTR) continue;
            if (errno == EBADF || errno == EINVAL) {
                log_event("-", 0, "Server socket closed — exiting accept loop");
                break;
            }
            perror("accept");
            continue;
        }

        /* check capacity using logger's counter */
        if (logger_get_client_count() >= g_cfg.max_clients) {
            log_event("-", 0, "REJECTED: max clients reached");
            dprintf(cfd, "ERROR: server busy, max clients reached\n");
            close(cfd);
            continue;
        }
        logger_update_client_count(+1);

        ClientArg *ca = malloc(sizeof(ClientArg));
        if (!ca) {
            close(cfd);
            log_event("-", 0, "ERROR: malloc failed for ClientArg");
            logger_update_client_count(-1);
            continue;
        }
        ca->client_fd   = cfd;
        ca->client_port = ntohs(client_addr.sin_port);
        inet_ntop(AF_INET, &client_addr.sin_addr,
                  ca->client_ip, INET_ADDRSTRLEN);

        char msg[128];
        snprintf(msg, sizeof(msg), "Connection attempt from %s:%d",
                 ca->client_ip, ca->client_port);
        log_event(ca->client_ip, ca->client_port, msg);

        pthread_t      tid;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

        if (pthread_create(&tid, &attr, handle_client, ca) != 0) {
            perror("pthread_create");
            log_event(ca->client_ip, ca->client_port,
                      "ERROR: failed to spawn client thread");
            free(ca);
            close(cfd);
            logger_update_client_count(-1);
        }
        pthread_attr_destroy(&attr);
    }
}

#ifndef NO_OPENGL
/* ── OpenGL thread entry ──────────────────────────────────── */
static void *gl_thread_func(void *arg)
{
    (void)arg;
    opengl_server_init(&g_gl_argc, g_gl_argv);
    opengl_server_run();   /* blocks in GLUT main loop */
    return NULL;
}
#endif

/* ── main ─────────────────────────────────────────────────── */
/*  Usage: ./server [config_file]                              */
int main(int argc, char *argv[])
{
#ifndef NO_OPENGL
    /* save for GLUT */
    g_gl_argc = argc;
    g_gl_argv = argv;
#endif

    const char *cfg_file = (argc > 1) ? argv[1] : "config.txt";

    if (load_config(cfg_file, &g_cfg) != 0) {
        fprintf(stderr, "Failed to load config from '%s'\n", cfg_file);
        return 1;
    }
    print_config(&g_cfg);

    signal(SIGINT,  handle_signal);
    signal(SIGTERM, handle_signal);

    /* initialise shared logger */
    logger_init(g_cfg.log_file);
    log_event("-", 0, "Server startup");

#ifndef NO_OPENGL
    /* spin up OpenGL monitor in its own thread */
    pthread_t gl_tid;
    if (pthread_create(&gl_tid, NULL, gl_thread_func, NULL) != 0) {
        perror("pthread_create (GL thread)");
        fprintf(stderr, "Warning: OpenGL monitor disabled\n");
    } else {
        pthread_detach(gl_tid);
    }
#endif

    g_server_fd = create_server_socket(g_cfg.port);
    if (g_server_fd < 0) {
        log_event("-", 0, "FATAL: could not create server socket");
        logger_close();
        return 1;
    }

    accept_loop(g_server_fd);

    log_event("-", 0, "Server shutdown");
    close(g_server_fd);
    logger_close();
    return 0;
}