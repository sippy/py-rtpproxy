#pragma once

#define MAX_MODULES 16
#define MAX_EXTRA_ARGS 64

struct RTPPInitializeParams {
    int ttl;
    int setup_ttl;
    int port_min;
    int port_max;
    const char *socket;
    const char *debug_level;
    const char *notify_socket;
    const char *rec_spool_dir;
    const char *rec_final_dir;
    const char **modules;
};
