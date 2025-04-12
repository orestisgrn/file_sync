#include "string.h"

typedef struct list_node* List;

List list_create(void);
void list_free(List l);
List list_insert(List l, String key, String val, int *succ);
List list_delete(List l,const char *key);
struct sync_info_rec *list_search(List l,const char *key);