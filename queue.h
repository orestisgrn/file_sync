#include "string.h"

typedef struct queue* Queue;

struct work_rec {
    struct sync_info_rec *rec;
    String filename;
    int op;
    int from_queue;
};

Queue queue_create(void);
int queue_push(Queue q,struct sync_info_rec *rec,String filename,int op);
struct work_rec *queue_pop(Queue q);