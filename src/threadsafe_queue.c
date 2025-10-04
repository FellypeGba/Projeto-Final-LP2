#include "threadsafe_queue.h"
#include <stdlib.h>
#include <string.h>

int mq_init(message_queue_t *q) {
    if (!q) return -1;
    q->head = q->tail = NULL;
    q->closed = 0;
    if (pthread_mutex_init(&q->mtx, NULL) != 0) return -1;
    if (pthread_cond_init(&q->cond, NULL) != 0) {
        pthread_mutex_destroy(&q->mtx);
        return -1;
    }
    return 0;
}

int mq_push(message_queue_t *q, const char *msg, int sender) {
    if (!q || !msg) return -1;
    mq_item_t *it = malloc(sizeof(mq_item_t));
    if (!it) return -1;
    /* duplicar a mensagem: a fila passa a possuir esta cópia e o chamador
     * pode liberar seu próprio buffer independentemente. mq_pop transferirá
     * a posse para o consumidor (que deve liberar). */
    it->msg = strdup(msg);
    if (!it->msg) {
        free(it);
        return -1;
    }
    it->sender = sender;
    it->next = NULL;

    pthread_mutex_lock(&q->mtx); //obtem lock para modificar a fila
    if (q->closed) {
        pthread_mutex_unlock(&q->mtx);
        free(it->msg);
        free(it);
        return -1;
    }
    if (q->tail) q->tail->next = it;
    q->tail = it;
    if (!q->head) q->head = it;
    pthread_cond_signal(&q->cond); //dá o sinal que há uma nova mensagem
    pthread_mutex_unlock(&q->mtx); //libera o lock apos modificar a fila
    return 0;
}

int mq_pop(message_queue_t *q, char **out_msg, int *out_sender) {
    if (!q || !out_msg || !out_sender) return -1;
    pthread_mutex_lock(&q->mtx); //obtem lock para modificar a fila
    while (!q->head && !q->closed) {
        pthread_cond_wait(&q->cond, &q->mtx); //espera por nova mensagem ou fechamento
    }
    if (!q->head && q->closed) {
        pthread_mutex_unlock(&q->mtx); //libera o lock pois a fila está fechada
        return -1; // closed and empty
    }
    mq_item_t *it = q->head;
    q->head = it->next;
    if (!q->head) q->tail = NULL;
    pthread_mutex_unlock(&q->mtx); //libera o lock apos modificar a fila
    /* transferir a posse da string para o chamador */
    *out_msg = it->msg;
    *out_sender = it->sender;
    free(it); //envia a copia e libera o item da fila
    return 0;
}

void mq_close(message_queue_t *q) { //fecha a fila (nenhum push futuro)
    if (!q) return;
    pthread_mutex_lock(&q->mtx);
    q->closed = 1;
    pthread_cond_broadcast(&q->cond);
    pthread_mutex_unlock(&q->mtx);
}

void mq_destroy(message_queue_t *q) { //destroi a fila e libera recursos
    if (!q) return;
    pthread_mutex_lock(&q->mtx);
    mq_item_t *it = q->head;
    while (it) {
        mq_item_t *next = it->next;
        free(it->msg);
        free(it);
        it = next;
    }
    q->head = q->tail = NULL;
    pthread_mutex_unlock(&q->mtx);
    pthread_mutex_destroy(&q->mtx);
    pthread_cond_destroy(&q->cond);
}
