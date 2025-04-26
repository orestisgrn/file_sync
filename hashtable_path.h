#include "string.h"

typedef struct hashtable* Hashtable_Path;

Hashtable_Path hashtable_path_create(int size);
struct sync_info_rec *hashtable_path_insert(Hashtable_Path h,String key,String val,int *succ);
struct sync_info_rec *hashtable_path_search(Hashtable_Path h,const char *key);
void hashtable_path_delete(Hashtable_Path h,const char *key);
void hashtable_path_free(Hashtable_Path h);
