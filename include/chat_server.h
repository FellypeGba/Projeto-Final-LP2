#ifndef CHAT_SERVER_H
#define CHAT_SERVER_H

#include "threadsafe_queue.h"
#include <pthread.h>
#include <semaphore.h>

#define CHAT_HISTORY_DEFAULT 100

typedef struct {
    pthread_mutex_t clients_mtx;
    int *clients; /* dynamic array */
    int num_clients;
    int max_clients;
    sem_t slots;
    message_queue_t mq;

    /* history circular buffer */
    char **history;
    int history_size;
    int history_start;
    int history_count;

    /* client names parallel to clients[] (nullable) */
    char **client_names;

    pthread_t broadcaster_tid;
    int running;
} ChatServer;

int chat_server_init(ChatServer *s, int max_clients, int history_size);
int chat_server_add_client(ChatServer *s, int client_fd);
void chat_server_remove_client(ChatServer *s, int client_fd);
int chat_server_enqueue_message(ChatServer *s, const char *msg, int sender_fd);
void chat_server_shutdown(ChatServer *s);
int chat_server_send_history(ChatServer *s, int client_fd, int n);
int chat_server_set_name(ChatServer *s, int client_fd, const char *name);
const char *chat_server_get_name(ChatServer *s, int client_fd);

#endif // CHAT_SERVER_H
