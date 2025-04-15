#include "string.h"

enum insert_codes {
    FAILED,
    SUCCESS,
    DUPL,
};

typedef struct sync_info_lookup* Sync_Info_Lookup;

Sync_Info_Lookup sync_info_lookup_create(int size);
struct sync_info_rec *sync_info_insert(Sync_Info_Lookup ltable,String key,String val,int watch_desc,int *insert_code);
struct sync_info_rec *sync_info_path_search(Sync_Info_Lookup ltable,const char *key);
struct sync_info_rec *sync_info_watchdesc_search(Sync_Info_Lookup ltable,int key);
void sync_info_delete(Sync_Info_Lookup ltable,const char *key);
void sync_info_lookup_free(Sync_Info_Lookup ltable);