#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "config.h"
#include "logger.h"
#include "opengl_display.h"

/* ── Global config ────────────────────────────────────────── */
static ClientConfig g_cfg;

/* argc/argv saved for GLUT */
static int    g_gl_argc;
static char **g_gl_argv;

/* ── getCurrentVersion ────────────────────────────────────── */
int getCurrentVersion(void)
{
    return g_cfg.client_version;
}

/* ── read_line helper ─────────────────────────────────────── */
static int read_line(int fd, char *buf, int maxlen)
{
    int total = 0;
    while (total < maxlen - 1) {
        char    c;
        ssize_t r = read(fd, &c, 1);
        if (r <= 0) break;
        buf[total++] = c;
        if (c == '\n') break;
    }
    buf[total] = '\0';
    return total;
}

/* ── Connect to server ────────────────────────────────────── */
static int connect_to_server(void)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { log_client("ERROR: socket() failed"); return -1; }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((uint16_t)g_cfg.port);

    if (inet_pton(AF_INET, g_cfg.server_ip, &addr.sin_addr) <= 0) {
        log_client("ERROR: invalid server IP");
        close(fd);
        return -1;
    }

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        log_client("ERROR: connect() failed — is the server running?");
        close(fd);
        return -1;
    }

    log_client("Connected to server");
    opengl_client_set_status("Connected to server");
    return fd;
}

/* ── Send version ─────────────────────────────────────────── */
static void send_version(int fd, int version)
{
    dprintf(fd, "VERSION:%d\n", version);
    char msg[64];
    snprintf(msg, sizeof(msg), "Sent VERSION:%d", version);
    log_client(msg);
    opengl_client_set_status(msg);
}

/* ── Read server response ─────────────────────────────────── */
static int read_response(int fd, char *out, int maxlen)
{
    if (read_line(fd, out, maxlen) <= 0) {
        log_client("ERROR: no response from server");
        return -1;
    }
    out[strcspn(out, "\r\n")] = '\0';
    char msg[128];
    snprintf(msg, sizeof(msg), "Server response: %s", out);
    log_client(msg);
    opengl_client_set_status(msg);
    return 0;
}

/* ── Read SIZE header ─────────────────────────────────────── */
static long read_size_header(int fd)
{
    char line[256];
    if (read_line(fd, line, sizeof(line)) <= 0) {
        log_client("ERROR: did not receive SIZE header");
        return -1;
    }
    long file_size = -1;
    if (sscanf(line, "SIZE:%ld", &file_size) != 1 || file_size < 0) {
        log_client("ERROR: malformed SIZE header");
        return -1;
    }
    return file_size;
}

/* ── Receive file chunks ──────────────────────────────────── */
static int receive_chunks(int fd, FILE *fp, long file_size)
{
    char *buf = malloc(g_cfg.buffer_size);
    if (!buf) { log_client("ERROR: malloc failed"); return -1; }

    long received = 0;
    opengl_client_set_progress(0);

    while (received < file_size) {
        long    remaining = file_size - received;
        int     to_read   = (remaining < g_cfg.buffer_size)
                            ? (int)remaining : g_cfg.buffer_size;
        ssize_t r = read(fd, buf, to_read);
        if (r <= 0) {
            if (errno == EINTR) continue;
            log_client("ERROR: connection dropped during download");
            free(buf);
            return -1;
        }
        fwrite(buf, 1, (size_t)r, fp);
        received += r;

        int pct = (int)((received * 100) / file_size);
        printf("\r  Downloading... %d%%", pct);
        fflush(stdout);

        /* update OpenGL progress bar */
        opengl_client_set_progress(pct);

        char status[64];
        snprintf(status, sizeof(status), "Downloading... %d%%", pct);
        opengl_client_set_status(status);
    }
    printf("\n");
    free(buf);
    return 0;
}

/* ── Save update file ─────────────────────────────────────── */
static int save_update_file(int fd, long file_size)
{
    char out_filename[64];
    snprintf(out_filename, sizeof(out_filename),
             "update_v%d.bin", g_cfg.client_version + 1);

    FILE *fp = fopen(out_filename, "wb");
    if (!fp) { log_client("ERROR: cannot open output file"); return -1; }

    if (receive_chunks(fd, fp, file_size) != 0) {
        fclose(fp);
        return -1;
    }
    fclose(fp);

    char msg[128];
    snprintf(msg, sizeof(msg), "Saved as %s", out_filename);
    log_client(msg);
    opengl_client_set_status(msg);
    return 0;
}

/* ── Simulated install ────────────────────────────────────── */
static void simulate_install(void)
{
    opengl_client_set_status("Installing update...");
    printf("  Installing update");
    for (int i = 0; i < 3; i++) { sleep(1); printf("."); fflush(stdout); }
    printf("\n");
    log_client("Installation complete");
    opengl_client_set_status("Installation complete!");
    opengl_client_set_progress(100);
}

/* ── Handle UPDATE_AVAILABLE ──────────────────────────────── */
static void handle_update(int fd)
{
    long file_size = read_size_header(fd);
    if (file_size < 0) return;

    char msg[64];
    snprintf(msg, sizeof(msg), "Expecting %ld bytes", file_size);
    log_client(msg);

    if (save_update_file(fd, file_size) != 0) {
        log_client("ERROR: download failed");
        opengl_client_set_status("ERROR: download failed");
        return;
    }
    simulate_install();
}

/* ── Handle UP_TO_DATE ────────────────────────────────────── */
static void handle_up_to_date(void)
{
    log_client("Software is up to date — no update needed");
    printf("  Your software is already up to date.\n");
    opengl_client_set_status("Software is up to date.");
    opengl_client_set_progress(100);
}

/* ── CheckForUpdate ───────────────────────────────────────── */
void CheckForUpdate(void)
{
    char msg[64];
    snprintf(msg, sizeof(msg), "Current version: %d", getCurrentVersion());
    log_client(msg);

    int fd = connect_to_server();
    if (fd < 0) return;

    send_version(fd, getCurrentVersion());

    char response[64];
    if (read_response(fd, response, sizeof(response)) != 0) {
        close(fd);
        return;
    }

    if      (strcmp(response, "UPDATE_AVAILABLE") == 0) handle_update(fd);
    else if (strcmp(response, "UP_TO_DATE")       == 0) handle_up_to_date();
    else    log_client("ERROR: unexpected server response");

    close(fd);
    log_client("Connection closed");
}

/* ── OpenGL thread entry ──────────────────────────────────── */
static void *gl_thread_func(void *arg)
{
    (void)arg;
    opengl_client_init(&g_gl_argc, g_gl_argv);
    opengl_client_run();
    return NULL;
}

/* ── main ─────────────────────────────────────────────────── */
int main(int argc, char *argv[])
{
    g_gl_argc = argc;
    g_gl_argv = argv;

    const char *cfg_file = (argc > 1) ? argv[1] : "config.txt";

    if (load_client_config(cfg_file, &g_cfg) != 0) {
        fprintf(stderr, "Failed to load config from '%s'\n", cfg_file);
        return 1;
    }
    print_client_config(&g_cfg);

    logger_init(g_cfg.log_file);
    log_client("Client startup");

    /* spin up OpenGL progress window in its own thread */
    pthread_t gl_tid;
    if (pthread_create(&gl_tid, NULL, gl_thread_func, NULL) != 0) {
        perror("pthread_create (GL thread)");
        fprintf(stderr, "Warning: OpenGL progress display disabled\n");
    } else {
        pthread_detach(gl_tid);
        /* small delay so GLUT window can open before work begins */
        usleep(300000);
    }

    CheckForUpdate();

    log_client("Client shutdown");
    logger_close();
    return 0;
}