#include "string.h"
#include <time.h>

enum return_codes {
    ARGS_ERR=1,
    INOTIFY_ERR,
    FOPEN_ERR,
    ALLOC_ERR,
    PATH_RES_ERR,
    NONEXISTENT_PATH,
    FORK_ERR,
    PIPE_ERR,
    EXEC_ERR,
    SIGNALFD_ERR,
};

enum status_codes {
    ACTIVE,
};

struct sync_info_rec {
    String source_dir;
    String target_dir;
    int status;
    time_t last_sync_time;
    int error_count;
    int watch_desc;
    int pipes[2];
};

#define CLEAN_AND_EXIT(PRINT_CMD,RETURN_CODE) { \
    sync_info_lookup_free(sync_info_mem_store); \
    free(worker_queue); \
    if (config_file != NULL) fclose(config_file); \
    if (log_file != NULL) fclose(log_file); \
    close(signal_fd); \
    PRINT_CMD; \
    return RETURN_CODE; \
}
