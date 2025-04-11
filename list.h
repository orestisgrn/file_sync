#include "string.h"

typedef struct list_node* List;

List list_create(void);
void list_free(List l);
List list_insert(List l, String key, String val);
List list_delete(List l,char *key);
struct sync_info_rec *list_search(List l,char *key);