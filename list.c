#include <stdlib.h>
#include <string.h>
#include "list.h"
#include "string.h"
#include "utils.h"

struct list_node {
    struct sync_info_rec rec;
    struct list_node* next;
};

List list_create(void) {
    return NULL;
}

void list_free(List l) {
    List temp;
    while(l!=NULL) {
        temp = l;
        l = l->next;
        string_free(temp->rec.source_dir);
        string_free(temp->rec.target_dir);
        free(temp);
    }
}

List list_insert(List l, String key, String val) {
    List new_node = malloc(sizeof(struct list_node));
    new_node->rec.source_dir = key;
    new_node->rec.target_dir = val;
    new_node->rec.status = ACTIVE;
    new_node->rec.last_sync_time = 0;
    new_node->rec.error_count = 0;
    new_node->next=l;
    return new_node;
}

List list_delete(List l,const char *key) {
    List temp,head=l,prev=NULL;
    while(l!=NULL) {
        temp = l->next;
        if (!strcmp(string_ptr(l->rec.source_dir),key)) {
            string_free(l->rec.source_dir);
            string_free(l->rec.target_dir);
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

struct sync_info_rec *list_search(List l,const char *key) {
    while(l!=NULL) {
        if(!strcmp(string_ptr(l->rec.source_dir),key))
            return (struct sync_info_rec*) &l->rec;
        l=l->next;
    }
    return NULL;
}