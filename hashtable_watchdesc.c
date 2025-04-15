#include <stdlib.h>
#include "hashtable_watchdesc.h"
#include "string.h"

struct hashtable {
    List *arr;
    int size;
};

Hashtable_Watchdesc hashtable_watchdesc_create(int size) {
    Hashtable_Watchdesc h=malloc(sizeof(struct hashtable));
    if (h==NULL)
        return NULL;
    h->arr = malloc(sizeof(List)*size);
    if (h->arr==NULL) {
        free(h);
        return NULL;
    }
    for(int i=0;i<size;i++) {
        h->arr[i]=list_create();
    }
    h->size=size;
    return h;
}

struct sync_info_rec *hashtable_watchdesc_insert(Hashtable_Watchdesc h,struct sync_info_rec *ref,int *succ) {
    List *bucket = &h->arr[ref->watch_desc % h->size];          // Beware of hashing overflow...
    if (list_search_watchdesc(*bucket,ref->watch_desc)==NULL) {
        *bucket = list_insert_ref(*bucket,ref,succ);
        if (*succ)
            return list_search_watchdesc(*bucket,ref->watch_desc);
        else
            return NULL;
    }
    else {
        *succ = 1;
        return NULL;
    }
}

struct sync_info_rec *hashtable_watchdesc_search(Hashtable_Watchdesc h,int key) {
    List bucket = h->arr[key % h->size];
    return list_search_watchdesc(bucket,key);
}

void hashtable_watchdesc_delete(Hashtable_Watchdesc h,int key) {
    List *bucket = &h->arr[key % h->size];
    *bucket = list_delete_watchdesc(*bucket,key,1);
}

void hashtable_watchdesc_free(Hashtable_Watchdesc h) {
    if (h!=NULL) {
        for (int i=0;i<h->size;i++)
            list_free(h->arr[i],1);
        free(h->arr);
        free(h);
    }
}
