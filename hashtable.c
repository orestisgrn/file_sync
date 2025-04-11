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

int hash_string(Hashtable h,const char *key) {
    int sum=0;
    while (*key != '\0')
        sum += *(key++);
    return sum % h->size;
}

int hashtable_insert(Hashtable h,String key,String val) {
    List *bucket = &h->arr[hash_string(h,string_ptr(key))];
    if (list_search(*bucket,string_ptr(key))==NULL) {
        *bucket = list_insert(*bucket,key,val);
        return 1;
    }
    else {
        return 0;
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
    for (int i=0;i<h->size;i++)
        list_free(h->arr[i]);
    free(h->arr);
    free(h);
}
