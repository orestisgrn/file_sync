#include "string.h"
#include "utils.h"

typedef struct list_node* List;

List list_create(void);
void list_free(List l,int is_reflist);
List list_insert_newrec(List l, String key, String val, int watch_desc, int *succ);
List list_insert_ref(List l,struct sync_info_rec *ref,int *succ);
List list_delete_path(List l,const char *key,int is_reflist);
List list_delete_watchdesc(List l,int key,int is_reflist);
struct sync_info_rec *list_search_path(List l,const char *key);
struct sync_info_rec *list_search_watchdesc(List l,int key);