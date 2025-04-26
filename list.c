#include <stdlib.h>
#include <string.h>
#include "list.h"
#include "string.h"

struct list_node {
    struct sync_info_rec* rec;
    struct list_node* next;
};

List list_create(void) {
    return NULL;
}

void list_free(List l,int is_reflist) {
    List temp;
    while(l!=NULL) {
        temp = l;
        l = l->next;
        if (!is_reflist) {
            string_free(temp->rec->source_dir);
            string_free(temp->rec->target_dir);
            free(temp->rec);
        }
        free(temp);
    }
}

List list_insert_newrec(List l, String key, String val,int *succ) {
    List new_node = malloc(sizeof(struct list_node));
    if (new_node==NULL) {
        *succ = 0;
        return l;
    }
    new_node->rec = malloc(sizeof(struct sync_info_rec));
    if (new_node->rec==NULL) {
        free(new_node);
        *succ = 0;
        return l;
    }
    new_node->rec->source_dir = key;
    new_node->rec->target_dir = val;
    new_node->rec->status = ACTIVE;
    new_node->rec->last_sync_time = -1;
    new_node->rec->error_count = 0;
    new_node->rec->watch_desc = -1;     // See how to handle pipe field
    new_node->next=l;
    *succ = 1;
    return new_node;
}

List list_insert_ref(List l,struct sync_info_rec *ref,int *succ) {
    List new_node = malloc(sizeof(struct list_node));
    if (new_node==NULL) {
        *succ = 0;
        return l;
    }
    new_node->rec = ref;
    new_node->next=l;
    *succ = 1;
    return new_node;
}

List list_delete_path(List l,const char *key,int is_reflist) {
    List temp,head=l,prev=NULL;
    while(l!=NULL) {
        temp = l->next;
        if (!strcmp(string_ptr(l->rec->source_dir),key)) {
            if (!is_reflist) {
                string_free(l->rec->source_dir);
                string_free(l->rec->target_dir);
                free(l->rec);
            }
            free(l);
            if (prev!=NULL) {
                prev->next=temp;
                return head;
            }
            else {
                return temp;
            }
        }
        prev=l;
        l=l->next;        
    }
    return head;
}

List list_delete_watchdesc(List l,int key,int is_reflist) {
    List temp,head=l,prev=NULL;
    while(l!=NULL) {
        temp = l->next;
        if (l->rec->watch_desc==key) {
            if (!is_reflist) {
                string_free(l->rec->source_dir);
                string_free(l->rec->target_dir);
                free(l->rec);
            }
            free(l);
            if (prev!=NULL) {
                prev->next=temp;
                return head;
            }
            else {
                return temp;
            }
        }
        prev=l;
        l=l->next;        
    }
    return head;
}

struct sync_info_rec *list_search_path(List l,const char *key) {
    while(l!=NULL) {
        if(!strcmp(string_ptr(l->rec->source_dir),key))
            return l->rec;
        l=l->next;
    }
    return NULL;
}

struct sync_info_rec *list_search_watchdesc(List l,int key) {
    while(l!=NULL) {
        if(l->rec->watch_desc==key)
            return l->rec;
        l=l->next;
    }
    return NULL;
}