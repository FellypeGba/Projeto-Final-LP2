#ifndef THREADSAFE_QUEUE_H
#define THREADSAFE_QUEUE_H

#include <pthread.h>

typedef struct mq_item {
    char *msg;
    int sender;
    struct mq_item *next;
} mq_item_t;

typedef struct {
    mq_item_t *head;
    mq_item_t *tail;
    pthread_mutex_t mtx;
    pthread_cond_t cond;
    int closed;
} message_queue_t;

int mq_init(message_queue_t *q);
int mq_push(message_queue_t *q, const char *msg, int sender);
int mq_pop(message_queue_t *q, char **out_msg, int *out_sender); /* returns 0 on success, -1 if closed */
void mq_close(message_queue_t *q);
void mq_destroy(message_queue_t *q);

#endif // THREADSAFE_QUEUE_H
