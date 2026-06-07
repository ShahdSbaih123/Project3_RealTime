#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "config.h"

static char *trim(char *s)
{
    while (isspace((unsigned char)*s)) s++;
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) *end-- = '\0';
    return s;
}
//Opens config.txt and reads it line by line
int load_config(const char *filename, ServerConfig *cfg)
{
    if (!filename || !cfg) return -1;

    //Built-in defaults applied before parsing(for any missing key)
    cfg->port           = 8080;
    cfg->max_clients    = 50;
    cfg->latest_version = 1;
    cfg->buffer_size    = 4096;
    strncpy(cfg->update_file, "update.bin", MAX_PATH_LEN - 1);
    strncpy(cfg->log_file,    "server.log", MAX_PATH_LEN - 1);
    cfg->update_file[MAX_PATH_LEN - 1] = '\0';
    cfg->log_file[MAX_PATH_LEN - 1]    = '\0';

    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("load_config: fopen");
        return -1;
    }

    char line[MAX_LINE_LEN];
    while (fgets(line, sizeof(line), fp)) {
        char *p = trim(line);
        if (*p == '#' || *p == '\0') continue;   //skip comments and blank lines

        char *eq = strchr(p, '=');
        if (!eq) continue;                  

        *eq = '\0';
        char *key = trim(p);
        char *val = trim(eq + 1);

        if      (strcmp(key, "PORT")           == 0) cfg->port           = atoi(val);
        else if (strcmp(key, "MAX_CLIENTS")    == 0) cfg->max_clients    = atoi(val);
        else if (strcmp(key, "LATEST_VERSION") == 0) cfg->latest_version = atoi(val);
        else if (strcmp(key, "BUFFER_SIZE")    == 0) cfg->buffer_size    = atoi(val);
        else if (strcmp(key, "UPDATE_FILE")    == 0) {
            strncpy(cfg->update_file, val, MAX_PATH_LEN - 1);
            cfg->update_file[MAX_PATH_LEN - 1] = '\0';
        }
        else if (strcmp(key, "LOG_FILE")       == 0) {
            strncpy(cfg->log_file, val, MAX_PATH_LEN - 1);
            cfg->log_file[MAX_PATH_LEN - 1] = '\0';
        }
    }
    fclose(fp);
    return 0;
}

void print_config(const ServerConfig *cfg)
{
    printf("=== Server Configuration ===\n");
    printf("  PORT           : %d\n", cfg->port);
    printf("  MAX_CLIENTS    : %d\n", cfg->max_clients);
    printf("  LATEST_VERSION : %d\n", cfg->latest_version);
    printf("  UPDATE_FILE    : %s\n", cfg->update_file);
    printf("  LOG_FILE       : %s\n", cfg->log_file);
    printf("  BUFFER_SIZE    : %d\n", cfg->buffer_size);
    printf("============================\n");
}
