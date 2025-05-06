#pragma once

struct RTPPInitializeParams {
    const char *ttl;
    const char *setup_ttl;
    const char *socket;
    const char *debug_level;
    const char *notify_socket;
    const char *rec_spool_dir;
    const char *rec_final_dir;
    const char *modules[];
};

#define howmany(x, y) (sizeof(x) / sizeof(y))
