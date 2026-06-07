#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "./config.h"


static ClientConfig g_cfg;
static FILE        *g_log_fp = NULL;

static void log_event(const char *event)
{
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", tm);
    printf("[%s] CLIENT  %s\n", ts, event);
    if (g_log_fp) {
        fprintf(g_log_fp, "[%s] CLIENT  %s\n", ts, event);
        fflush(g_log_fp);
    }
}

int getCurrentVersion(void)
{
    return g_cfg.client_version;
}



static int read_line(int fd, char *buf, int maxlen)
{
    int total = 0;
    while (total < maxlen - 1) {
        char c;
        ssize_t r = read(fd, &c, 1);
        if (r <= 0) break;
        buf[total++] = c;
        if (c == '\n') break;
    }
    buf[total] = '\0';
    return total;
}


static int connect_to_server(void)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { log_event("ERROR: socket() failed"); return -1; }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((uint16_t)g_cfg.port);

    if (inet_pton(AF_INET, g_cfg.server_ip, &addr.sin_addr) <= 0) {
        log_event("ERROR: invalid server IP");
        close(fd);
        return -1;
    }

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        log_event("ERROR: connect() failed — is the server running?");
        close(fd);
        return -1;
    }

    log_event("Connected to server");
    return fd;
}

static void send_version(int fd, int version)
{
    dprintf(fd, "VERSION:%d\n", version);
    char msg[64];
    snprintf(msg, sizeof(msg), "Sent VERSION:%d", version);
    log_event(msg);
}


static int read_response(int fd, char *out, int maxlen)
{
    if (read_line(fd, out, maxlen) <= 0) {
        log_event("ERROR: no response from server");
        return -1;
    }
    out[strcspn(out, "\r\n")] = '\0';
    char msg[128];
    snprintf(msg, sizeof(msg), "Server response: %s", out);
    log_event(msg);
    return 0;
}


static long read_size_header(int fd)
{
    char line[256];
    if (read_line(fd, line, sizeof(line)) <= 0) {
        log_event("ERROR: did not receive SIZE header");
        return -1;
    }
    long file_size = -1;
    if (sscanf(line, "SIZE:%ld", &file_size) != 1 || file_size < 0) {
        log_event("ERROR: malformed SIZE header");
        return -1;
    }
    return file_size;
}



static int receive_chunks(int fd, FILE *fp, long file_size)
{
    char *buf = malloc(g_cfg.buffer_size);
    if (!buf) { log_event("ERROR: malloc failed"); return -1; }

    long received = 0;
    while (received < file_size) {
        long remaining = file_size - received;
        int  to_read   = (remaining < g_cfg.buffer_size)
                         ? (int)remaining : g_cfg.buffer_size;
        ssize_t r = read(fd, buf, to_read);
        if (r <= 0) {
            if (errno == EINTR) continue;
            log_event("ERROR: connection dropped during download");
            free(buf);
            return -1;
        }
        fwrite(buf, 1, r, fp);
        received += r;
        printf("\r  Downloading... %d%%", (int)((received * 100) / file_size));
        fflush(stdout);
    }
    printf("\n");
    free(buf);
    return 0;
}



static int save_update_file(int fd, long file_size)
{
    char out_filename[64];
    snprintf(out_filename, sizeof(out_filename),
             "update_v%d.bin", g_cfg.client_version + 1);

    FILE *fp = fopen(out_filename, "wb");
    if (!fp) { log_event("ERROR: cannot open output file"); return -1; }

    if (receive_chunks(fd, fp, file_size) != 0) {
        fclose(fp);
        return -1;
    }
    fclose(fp);

    char msg[128];
    snprintf(msg, sizeof(msg), "Saved as %s", out_filename);
    log_event(msg);
    return 0;
}


static void simulate_install(void)
{
    printf("  Installing update");
    for (int i = 0; i < 3; i++) { sleep(1); printf("."); fflush(stdout); }
    printf("\n");
    log_event("Installation complete");
}



static void handle_update(int fd)
{
    long file_size = read_size_header(fd);
    if (file_size < 0) return;

    char msg[64];
    snprintf(msg, sizeof(msg), "Expecting %ld bytes", file_size);
    log_event(msg);

    if (save_update_file(fd, file_size) != 0) {
        log_event("ERROR: download failed");
        return;
    }
    simulate_install();
}


static void handle_up_to_date(void)
{
    log_event("Software is up to date — no update needed");
    printf("  Your software is already up to date.\n");
}


void CheckForUpdate(void)
{
    char msg[64];
    snprintf(msg, sizeof(msg), "Current version: %d", getCurrentVersion());
    log_event(msg);

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
    else    log_event("ERROR: unexpected server response");

    close(fd);
    log_event("Connection closed");
}


int main(int argc, char *argv[])
{
    const char *cfg_file = (argc > 1) ? argv[1] : "config.txt";

    if (load_client_config(cfg_file, &g_cfg) != 0) {
        fprintf(stderr, "Failed to load config from '%s'\n", cfg_file);
        return 1;
    }
    print_client_config(&g_cfg);

    g_log_fp = fopen(g_cfg.log_file, "a");
    if (!g_log_fp)
        fprintf(stderr, "Warning: cannot open log '%s' — stdout only\n",
                g_cfg.log_file);

    log_event("Client startup");
    CheckForUpdate();
    log_event("Client shutdown");

    if (g_log_fp) fclose(g_log_fp);
    return 0;
}