#include "chat_server.h"
#include "tslog.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>

static void *broadcaster_func(void *arg) {
    ChatServer *s = (ChatServer *)arg;
    char *msg = NULL;
    int sender;
    /*
     * Thread broadcaster: consome mensagens da fila de mensagens e as
     * encaminha para todos os clientes conectados, exceto o remetente.
     * A thread mantém o mutex de clients enquanto itera a lista para
     * garantir consistência. A posse da mensagem é transferida por
     * mq_pop (que retorna uma string alocada no heap que o broadcaster
     * deve liberar).
     */
    while (s->running) {
        if (mq_pop(&s->mq, &msg, &sender) != 0) {
            break; // queue closed
        }
        pthread_mutex_lock(&s->clients_mtx);
        for (int i = 0; i < s->num_clients; i++) {
            int fd = s->clients[i];
            if (fd != sender) {
                ssize_t sent = send(fd, msg, strlen(msg), 0);
                if (sent < 0) {
                    tslog_write(LOG_WARN, "Falha ao enviar para cliente %d: %s", fd, strerror(errno));
                }
            }
        }
        pthread_mutex_unlock(&s->clients_mtx);
        tslog_write(LOG_INFO, "Broadcast enviado: %s", msg);
        free(msg);
        msg = NULL;
    }
    return NULL;
}

int chat_server_init(ChatServer *s, int max_clients, int history_size) {
    if (!s) return -1;
    s->clients = calloc(max_clients, sizeof(int));
    if (!s->clients) return -1;
    s->num_clients = 0;
    s->max_clients = max_clients;
    /* inicializa as primitivas de sincronização -- em uma versão mais
     * robusta deveríamos checar os retornos de cada inicialização e
     * realizar cleanup em caso de erro (omitido aqui por brevidade). */
    pthread_mutex_init(&s->clients_mtx, NULL);
    sem_init(&s->slots, 0, max_clients);
    mq_init(&s->mq);

    s->history_size = history_size > 0 ? history_size : CHAT_HISTORY_DEFAULT;
    s->history = calloc(s->history_size, sizeof(char*));
    s->history_start = 0;
    s->history_count = 0;

    s->running = 1;
    if (pthread_create(&s->broadcaster_tid, NULL, broadcaster_func, s) != 0) {
        s->running = 0;
        free(s->clients);
        free(s->history);
        return -1;
    }
    return 0;
}

int chat_server_add_client(ChatServer *s, int client_fd) {
    if (!s) return -1;
    if (sem_trywait(&s->slots) != 0) {
        return -1; // no slots
    }
    pthread_mutex_lock(&s->clients_mtx);
    if (s->num_clients >= s->max_clients) {
        pthread_mutex_unlock(&s->clients_mtx);
        sem_post(&s->slots);
        return -1;
    }
    s->clients[s->num_clients++] = client_fd;
    pthread_mutex_unlock(&s->clients_mtx);
    tslog_write(LOG_INFO, "Cliente adicionado (fd=%d), total=%d", client_fd, s->num_clients);
    return 0;
}

void chat_server_remove_client(ChatServer *s, int client_fd) {
    if (!s) return;
    pthread_mutex_lock(&s->clients_mtx);
    for (int i = 0; i < s->num_clients; i++) {
        if (s->clients[i] == client_fd) {
            s->clients[i] = s->clients[s->num_clients-1];
            s->num_clients--;
            break;
        }
    }
    pthread_mutex_unlock(&s->clients_mtx);
    sem_post(&s->slots);
    tslog_write(LOG_INFO, "Cliente removido (fd=%d), total=%d", client_fd, s->num_clients);
}

int chat_server_enqueue_message(ChatServer *s, const char *msg, int sender_fd) {
    if (!s || !msg) return -1;
    /* save to history */
    pthread_mutex_lock(&s->clients_mtx);
    int idx = (s->history_start + s->history_count) % s->history_size;
    if (s->history_count == s->history_size) {
        /* overwrite oldest */
        free(s->history[s->history_start]);
        s->history[s->history_start] = strdup(msg);
        s->history_start = (s->history_start + 1) % s->history_size;
    } else {
        s->history[idx] = strdup(msg);
        s->history_count++;
    }
    pthread_mutex_unlock(&s->clients_mtx);

    /* enqueue the message for broadcasting; mq_push duplicates the string
     * so ownership remains with the caller until mq_pop transfers it to the
     * broadcaster (which frees it). */
    return mq_push(&s->mq, msg, sender_fd);
}

void chat_server_shutdown(ChatServer *s) {
    if (!s) return;
    s->running = 0;
    mq_close(&s->mq);
    pthread_join(s->broadcaster_tid, NULL);

    pthread_mutex_lock(&s->clients_mtx);
    for (int i = 0; i < s->num_clients; i++) {
        close(s->clients[i]);
    }
    pthread_mutex_unlock(&s->clients_mtx);

    mq_destroy(&s->mq);
    free(s->clients);
    for (int i = 0; i < s->history_count; i++) {
        int idx = (s->history_start + i) % s->history_size;
        free(s->history[idx]);
    }
    free(s->history);
    /* destroy synchronization primitives */
    pthread_mutex_destroy(&s->clients_mtx);
    sem_destroy(&s->slots);
}
