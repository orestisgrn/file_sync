#include <stdlib.h>
#include "hashtable_path.h"
#include "list.h"
#include "string.h"

struct hashtable {
    List *arr;
    int size;
};

Hashtable_Path hashtable_path_create(int size) {
    Hashtable_Path h=malloc(sizeof(struct hashtable));
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

unsigned hash_string(Hashtable_Path h,const char *key) {    // Maybe change hash func to a better one
    unsigned sum=0;                                         // http://www.cse.yorku.ca/~oz/hash.html
    while (*key != '\0')                                    // Also maybe add rehashing
        sum += *(key++);
    return sum % h->size;
}

struct sync_info_rec *hashtable_path_insert(Hashtable_Path h,String key,String val,int watch_desc,int *succ) {
    List *bucket = &h->arr[hash_string(h,string_ptr(key))];
    if (list_search_path(*bucket,string_ptr(key))==NULL) {
        *bucket = list_insert_newrec(*bucket,key,val,watch_desc,succ);
        if (*succ)
            return list_search_path(*bucket,string_ptr(key));
        else
            return NULL;
    }
    else {
        *succ = 1;
        return NULL;
    }
}

struct sync_info_rec *hashtable_path_search(Hashtable_Path h,const char *key) {
    List bucket = h->arr[hash_string(h,key)];
    return list_search_path(bucket,key);
}

void hashtable_path_delete(Hashtable_Path h,const char *key) {
    List *bucket = &h->arr[hash_string(h,key)];
    *bucket = list_delete_path(*bucket,key,0);
}

void hashtable_path_free(Hashtable_Path h) {
    if (h!=NULL) {
        for (int i=0;i<h->size;i++)
            list_free(h->arr[i],0);
        free(h->arr);
        free(h);
    }
}