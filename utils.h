#include "string.h"
#include <time.h>

#define FSS_IN  "fss_in"
#define FSS_OUT "fss_out"

// Χρήσιμα enums, macros, και ορισμοί που χρησιμοποιούνται σε πολλαπλά αρχεία

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
    FIFO_ERR,
};

enum status_codes {
    ACTIVE,
    INACTIVE,
};

enum cmd_codes {
    NO_COMMAND,
    SHUTDOWN,
    ADD,
    CANCEL,
    STATUS,
    SYNC,
    INVALID_SOURCE,
    INVALID_TARGET,
    ARCHIVED,
    NOT_ARCHIVED,
    NOT_MONITORED,
};

struct sync_info_rec {
    String source_dir;
    String target_dir;
    time_t last_sync_time;
    int error_count;
    int watch_desc;
    int worker_num;
};

#define CLEAN_AND_EXIT(PRINT_CMD,RETURN_CODE) { \
    sync_info_lookup_free(sync_info_mem_store); \
    free(worker_queue); \
    for(int i=0;i<cur_workers;i++) { \
        close(worker_table[i]->pipes[0]); \
        close(worker_table[i]->pipes[1]); \
        free(worker_table[i]); \
    } \
    free(worker_table); \
    if (config_file != NULL) fclose(config_file); \
    if (log_file != NULL) fclose(log_file); \
    if (fss_in_fd!=-1) close(fss_in_fd); \
    if (fss_out_fd!=-1) close(fss_out_fd); \
    if (signal_fd!=-1) close(signal_fd); \
    unlink(fss_in); \
    unlink(fss_out); \
    PRINT_CMD; \
    return RETURN_CODE; \
}
