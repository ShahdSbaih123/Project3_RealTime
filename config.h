#ifndef CONFIG_H
#define CONFIG_H

#include <netinet/in.h>

#define MAX_PATH_LEN  256
#define MAX_LINE_LEN  512

// ── Server config ─────────────────────────────────────────────
typedef struct {
    int  port;
    int  max_clients;
    int  latest_version;
    char update_file[MAX_PATH_LEN];
    char log_file[MAX_PATH_LEN];
    int  buffer_size;
} ServerConfig;

int load_config(const char *filename, ServerConfig *cfg);
void print_config(const ServerConfig *cfg);

// ── Client config ─────────────────────────────────────────────
typedef struct {
    char server_ip[INET_ADDRSTRLEN];
    int  port;
    int  client_version;
    char log_file[MAX_PATH_LEN];
    int  buffer_size;
} ClientConfig;

int load_client_config(const char *filename, ClientConfig *cfg);
void print_client_config(const ClientConfig *cfg);

#endif