#include "string.h"
#include "list.h"

typedef struct hashtable* Hashtable_Watchdesc;

Hashtable_Watchdesc hashtable_watchdesc_create(int size);
struct sync_info_rec *hashtable_watchdesc_insert(Hashtable_Watchdesc h,struct sync_info_rec *ref,int *succ);
struct sync_info_rec *hashtable_watchdesc_search(Hashtable_Watchdesc h,int key);
void hashtable_watchdesc_delete(Hashtable_Watchdesc h,int key);
void hashtable_watchdesc_free(Hashtable_Watchdesc h);
