#include "string.h"

typedef struct hashtable* Hashtable;

Hashtable hashtable_create(int size);
int hash_string(Hashtable h,const char *key);
int hashtable_insert(Hashtable h,String key,String val);
struct sync_info_rec *hashtable_search(Hashtable h,const char *key);
void hashtable_delete(Hashtable h,const char *key);
void hashtable_free(Hashtable h);