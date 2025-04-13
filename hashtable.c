#include <stdlib.h>
#include "hashtable.h"
#include "list.h"
#include "string.h"
#include "utils.h"

struct hashtable {
    List *arr;
    int size;
};

Hashtable hashtable_create(int size) {
    Hashtable h=malloc(sizeof(struct hashtable));
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

int hash_string(Hashtable h,const char *key) {      // Maybe change hash func to a better one
    int sum=0;                                      // http://www.cse.yorku.ca/~oz/hash.html
    while (*key != '\0')                            // Also maybe add rehashing
        sum += *(key++);
    return sum % h->size;
}

/* -1: duplicate entry | 0: unsuccessful insert | 1: success */
int hashtable_insert(Hashtable h,String key,String val) {
    List *bucket = &h->arr[hash_string(h,string_ptr(key))];
    if (list_search(*bucket,string_ptr(key))==NULL) {
        int succ;
        *bucket = list_insert(*bucket,key,val,&succ);
        return succ;
    }
    else {
        return -1;
    }
}

struct sync_info_rec *hashtable_search(Hashtable h,const char *key) {
    List bucket = h->arr[hash_string(h,key)];
    return list_search(bucket,key);
}

void hashtable_delete(Hashtable h,const char *key) {
    List *bucket = &h->arr[hash_string(h,key)];
    *bucket = list_delete(*bucket,key);
}

void hashtable_free(Hashtable h) {
    if (h!=NULL) {
        for (int i=0;i<h->size;i++)
            list_free(h->arr[i]);
        free(h->arr);
        free(h);
    }
}
