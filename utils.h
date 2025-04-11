#include "string.h"

enum return_codes {
    ARGS_ERR=1,
};

enum status_codes {
    ACTIVE
};

struct sync_info_rec {
    String source_dir;
    String target_dir;
    int status;
    int last_sync_time;
    int error_count;
};