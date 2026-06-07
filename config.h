#ifndef CONFIG_H
#define CONFIG_H

#define MAX_PATH_LEN  256
#define MAX_LINE_LEN  512

typedef struct {
    int  port;
    int  max_clients;
    int  latest_version;
    char update_file[MAX_PATH_LEN];
    char log_file[MAX_PATH_LEN];
    int  buffer_size;
} ServerConfig;

//Load key=value pairs from filename into cfg. 
//Returns 0 on success, -1 on error. Falls back to built-in defaults
int load_config(const char *filename, ServerConfig *cfg); 

void print_config(const ServerConfig *cfg);

#endif
