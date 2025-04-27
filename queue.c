#include <stdlib.h>
#include "queue.h"

struct node {
    struct work_rec* work_rec;
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

int queue_push(Queue q,struct sync_info_rec *rec,String filename,int op) {
    struct node *new_node = malloc(sizeof(struct node));
    if (new_node==NULL)
        return 0;
    new_node->work_rec = malloc(sizeof(struct work_rec));
    if (new_node->work_rec==NULL) {
        free(new_node);
        return 0;
    }
    if (q->last==NULL) {
        q->fst  = new_node;
        q->last = new_node;
    }
    else {
        q->last->next = new_node;
        q->last = new_node;
    }
    new_node->next = NULL;
    new_node->work_rec->rec = rec;
    new_node->work_rec->filename = filename;
    new_node->work_rec->op = op;
    return 1;
}

struct work_rec *queue_pop(Queue q) {
    if (q->fst==NULL)
        return NULL;
    struct node *pop_node = q->fst;
    q->fst = q->fst->next;
    if (q->fst==NULL)
        q->last = NULL;
    struct work_rec *work_rec = pop_node->work_rec;
    free(pop_node);
    return work_rec;
}

