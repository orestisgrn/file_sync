#include "string.h"

typedef struct queue* Queue;

struct work_rec {
    String source;
    String target;
    String filename;
    int op;
};

Queue queue_create(void);
int queue_push(Queue q,String source,String target,String filename,int op);
struct work_rec *queue_pop(Queue q);