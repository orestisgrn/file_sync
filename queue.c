#include <stdlib.h>
#include "queue.h"

struct node {
    struct work_rec* rec;
    struct node* next;
};

struct queue {
    struct node* fst;
    struct node* last;
};

Queue queue_create(void) {
    Queue q = malloc(sizeof(struct queue));
    if (q==NULL) {
        return NULL;
    }
    else {
        q->fst  = NULL;
        q->last = NULL;
    }
    return q;
}

int queue_push(Queue q,String source,String target,String filename,int op) {
    struct node *new_node = malloc(sizeof(struct node));
    if (new_node==NULL)
        return 0;
    struct work_rec *rec = malloc(sizeof(struct work_rec));
    if (rec==NULL)
        return 0;
    if (q->last==NULL) {
        q->fst  = new_node;
        q->last = new_node;
        new_node->next = NULL;
    }
    else {
        q->last->next = new_node;
        q->last = new_node;
    }
    new_node->rec->source = source;
    new_node->rec->target = target;
    new_node->rec->filename = filename;
    new_node->rec->op = op;
    return 1;
}

struct work_rec *queue_pop(Queue q) {
    if (q->fst==NULL)
        return NULL;
    struct node *pop_node = q->fst;
    q->fst = q->fst->next;
    struct work_rec *rec = pop_node->rec;
    free(pop_node);
    return rec;
}

