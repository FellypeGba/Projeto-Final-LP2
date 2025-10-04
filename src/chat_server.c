#include "chat_server.h"
#include "tslog.h"
#include "net.h"
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
            break; // queue fechada e vazia
        }
        pthread_mutex_lock(&s->clients_mtx); //obtem lock para iterar clients
        int targets = 0;
        for (int i = 0; i < s->num_clients; i++) {
            int fd = s->clients[i];
            if (fd != sender) {
                if (send_all(fd, msg, strlen(msg)) < 0) {
                    tslog_write(LOG_WARN, "Falha ao enviar para cliente %d: %s", fd, strerror(errno));
                }
                targets++;
            }
        }
        pthread_mutex_unlock(&s->clients_mtx); //libera lock apos iterar clients
        /* registra que o broadcast foi enviado e quantos alvos */
        tslog_write(LOG_INFO, "Broadcast enviado (remetente=%d, alvos=%d)", sender, targets);
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
    /* inicializa as primitivas de sincronização com checagem de erros */
    if (pthread_mutex_init(&s->clients_mtx, NULL) != 0) {
        free(s->clients);
        return -1;
    }
    if (sem_init(&s->slots, 0, max_clients) != 0) {
        pthread_mutex_destroy(&s->clients_mtx);
        free(s->clients);
        return -1;
    }
    if (mq_init(&s->mq) != 0) {
        sem_destroy(&s->slots);
        pthread_mutex_destroy(&s->clients_mtx);
        free(s->clients);
        return -1;
    }

    s->history_size = history_size > 0 ? history_size : CHAT_HISTORY_DEFAULT;
    s->history = calloc(s->history_size, sizeof(char*));
    s->history_start = 0;
    s->history_count = 0;

    s->client_names = calloc(max_clients, sizeof(char*));
    if (!s->client_names) {
        free(s->history);
        mq_destroy(&s->mq);
        sem_destroy(&s->slots);
        pthread_mutex_destroy(&s->clients_mtx);
        free(s->clients);
        return -1;
    }

    s->running = 1;
    if (pthread_create(&s->broadcaster_tid, NULL, broadcaster_func, s) != 0) {
        /* cleanup on failure */
        mq_destroy(&s->mq);
        sem_destroy(&s->slots);
        pthread_mutex_destroy(&s->clients_mtx);
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
    pthread_mutex_lock(&s->clients_mtx); //obtem lock para modificar clients
    if (s->num_clients >= s->max_clients) {
        pthread_mutex_unlock(&s->clients_mtx);
        sem_post(&s->slots);
        return -1;
    }
    s->clients[s->num_clients++] = client_fd;
    s->client_names[s->num_clients-1] = NULL;
    pthread_mutex_unlock(&s->clients_mtx); //libera lock apos modificar clients
    tslog_write(LOG_INFO, "Cliente adicionado (fd=%d), total=%d", client_fd, s->num_clients);
    return 0;
}

void chat_server_remove_client(ChatServer *s, int client_fd) {
    if (!s) return;
    pthread_mutex_lock(&s->clients_mtx); //obtem lock para remover um cliente
    for (int i = 0; i < s->num_clients; i++) {
        if (s->clients[i] == client_fd) {
            s->clients[i] = s->clients[s->num_clients-1];
            /* move name too */
            free(s->client_names[i]);
            s->client_names[i] = s->client_names[s->num_clients-1];
            s->client_names[s->num_clients-1] = NULL;
            s->num_clients--;
            break;
        }
    }
    pthread_mutex_unlock(&s->clients_mtx); //liber lock apos remover um cliente
    sem_post(&s->slots);
    tslog_write(LOG_INFO, "Cliente removido (fd=%d), total=%d", client_fd, s->num_clients);
}

/* define o nome de um cliente já registrado (faz strdup) */
int chat_server_set_name(ChatServer *s, int client_fd, const char *name) {
    if (!s || !name) return -1;
    pthread_mutex_lock(&s->clients_mtx);
    for (int i = 0; i < s->num_clients; ++i) {
        if (s->clients[i] == client_fd) {
            char *dup = strdup(name);
            if (!dup) { pthread_mutex_unlock(&s->clients_mtx); return -1; }
            free(s->client_names[i]);
            s->client_names[i] = dup;
            pthread_mutex_unlock(&s->clients_mtx);
            return 0;
        }
    }
    pthread_mutex_unlock(&s->clients_mtx);
    return -1;
}

/* get name (caller must not free) */
const char *chat_server_get_name(ChatServer *s, int client_fd) {
    if (!s) return NULL;
    const char *res = NULL;
    pthread_mutex_lock(&s->clients_mtx);
    for (int i = 0; i < s->num_clients; ++i) {
        if (s->clients[i] == client_fd) { res = s->client_names[i]; break; }
    }
    pthread_mutex_unlock(&s->clients_mtx);
    return res;
}

int chat_server_enqueue_message(ChatServer *s, const char *msg, int sender_fd) {
    if (!s || !msg) return -1;
    /* save to history */
    pthread_mutex_lock(&s->clients_mtx); //obtem lock para modificar history
    int idx = (s->history_start + s->history_count) % s->history_size;
    if (s->history_count == s->history_size) {
        /* sobrescreve o mais antigo */
        free(s->history[s->history_start]);
        s->history[s->history_start] = strdup(msg);
        s->history_start = (s->history_start + 1) % s->history_size;
    } else {
        s->history[idx] = strdup(msg);
        s->history_count++;
    }
    pthread_mutex_unlock(&s->clients_mtx); //libera lock apos modificar history

    // coloca a mensagem na fila para broadcast; mq_push duplica a string
    return mq_push(&s->mq, msg, sender_fd);
}

// desliga o servidor de chat
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
    for (int i = 0; i < s->max_clients; ++i) {
        free(s->client_names[i]);
    }
    free(s->client_names);
    for (int i = 0; i < s->history_count; i++) {
        int idx = (s->history_start + i) % s->history_size;
        free(s->history[idx]);
    }
    free(s->history);
    /* destroi todas as primitivas */
    pthread_mutex_destroy(&s->clients_mtx);
    sem_destroy(&s->slots);
}

int chat_server_send_history(ChatServer *s, int client_fd, int n) {
    if (!s || n <= 0) return 0;
    pthread_mutex_lock(&s->clients_mtx);
    int to_send = n < s->history_count ? n : s->history_count;
    int start_idx = (s->history_start + (s->history_count - to_send)) % s->history_size;

    char **copies = malloc(sizeof(char*) * to_send);
    if (!copies) { pthread_mutex_unlock(&s->clients_mtx); return -1; }
    for (int i = 0; i < to_send; ++i) {
        int idx = (start_idx + i) % s->history_size;
        copies[i] = s->history[idx] ? strdup(s->history[idx]) : NULL;
        if (s->history[idx] && !copies[i]) {
            for (int j = 0; j < i; ++j) free(copies[j]);
            free(copies);
            pthread_mutex_unlock(&s->clients_mtx);
            return -1;
        }
    }
    pthread_mutex_unlock(&s->clients_mtx);

    for (int i = 0; i < to_send; ++i) {
        if (copies[i]) {
            send_all(client_fd, copies[i], strlen(copies[i]));
            send_all(client_fd, "\n", 1);
            free(copies[i]);
        }
    }
    free(copies);
    return 0;
}
