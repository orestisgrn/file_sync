#include <stdlib.h>
#include "sync_info_lookup.h"
#include "hashtable_path.h"
#include "hashtable_watchdesc.h"
#include "string.h"

struct sync_info_lookup {
    Hashtable_Path path_lookup;
    Hashtable_Watchdesc wd_lookup;
};

Sync_Info_Lookup sync_info_lookup_create(int size) {
    Sync_Info_Lookup ltable = malloc(sizeof(struct sync_info_lookup));
    if (ltable==NULL)
        return NULL;
    if ((ltable->path_lookup = hashtable_path_create(size))==NULL) {
        free(ltable);
        return NULL;
    }
    if ((ltable->wd_lookup = hashtable_watchdesc_create(size))==NULL) {
        hashtable_path_free(ltable->path_lookup);
        free(ltable);
        return NULL;
    }
    return ltable;
}

struct sync_info_rec*
sync_info_insert(Sync_Info_Lookup ltable,String key,String val,int *insert_code) {
    int succ;
    struct sync_info_rec *rec=hashtable_path_insert(ltable->path_lookup,key,val,&succ);
    if (rec==NULL) {
        if (succ)
            *insert_code = DUPL;
        else
            *insert_code = FAILED;
    }
    else {
        *insert_code = SUCCESS;
    }
    return rec;
}

struct sync_info_rec*
sync_info_index_watchdesc(Sync_Info_Lookup ltable, struct sync_info_rec *rec, int *insert_code) {
    int succ;
    rec = hashtable_watchdesc_insert(ltable->wd_lookup,rec,&succ);
    if (rec==NULL) {
        if (succ)
            *insert_code = DUPL;
        else
            *insert_code = FAILED;
    }
    else {
        *insert_code = SUCCESS;
    }
    return rec;
}

struct sync_info_rec *sync_info_path_search(Sync_Info_Lookup ltable,const char *key) {
    return hashtable_path_search(ltable->path_lookup,key);
}

struct sync_info_rec *sync_info_watchdesc_search(Sync_Info_Lookup ltable,int key) {
    return hashtable_watchdesc_search(ltable->wd_lookup,key);
}

void sync_info_watchdesc_delete(Sync_Info_Lookup ltable,int key) {
    hashtable_watchdesc_delete(ltable->wd_lookup,key);
}

void sync_info_lookup_free(Sync_Info_Lookup ltable) {
    if (ltable!=NULL) {
        hashtable_watchdesc_free(ltable->wd_lookup);
        hashtable_path_free(ltable->path_lookup);
        free(ltable);
    }
}