#include "string.h"

enum return_codes {
    ARGS_ERR=1,
    INOTIFY_ERR,
    FOPEN_ERR,
    ALLOC_ERR,
    PATH_RES_ERR,
    NONEXISTENT_PATH,
};

enum status_codes {
    ACTIVE,
};

struct sync_info_rec {
    String source_dir;
    String target_dir;
    int status;
    int last_sync_time;
    int error_count;
};

#define CLEAN_AND_EXIT(PRINT_CMD,RETURN_CODE) { \
    hashtable_free(sync_info_mem_store); \
    if (config_file != NULL) fclose(config_file); \
    PRINT_CMD; \
    return RETURN_CODE; \
}
