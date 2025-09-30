#include "tslog.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 9000
#define MAX_CLIENTS 10

int client_sockets[MAX_CLIENTS];
int num_clients = 0;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

//Função para enviar mensagem a todos os clientes exceto o remetente
void broadcast_message(const char *msg, int sender_sock) {
    pthread_mutex_lock(&clients_mutex); // Início da seção crítica: protege acesso à lista de clientes
    for (int i = 0; i < num_clients; i++) {
        if (client_sockets[i] != sender_sock) {
            send(client_sockets[i], msg, strlen(msg), 0);
        }
    }
    pthread_mutex_unlock(&clients_mutex); // Fim da seção crítica
}

void broadcast_info(int sender_sock, const char *msg) {
    char info[300];
    snprintf(info, sizeof(info), "Cliente %d enviou: %s", sender_sock, msg);
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < num_clients; i++) {
        if (client_sockets[i] != sender_sock) {
            send(client_sockets[i], info, strlen(info), 0);
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

//Função para lidar com comunicação de um cliente (em cada thread)
void *client_thread(void *arg) {
    int sock = *(int *)arg;
    char buffer[256];
    ssize_t len;

    while ((len = recv(sock, buffer, sizeof(buffer)-1, 0)) > 0) {
        buffer[len] = '\0';
        tslog_write(LOG_INFO, "Mensagem recebida do cliente %d: %s", sock, buffer); // Só loga no servidor
        tslog_write(LOG_INFO, "Mensagem enviada ao servidor pelo cliente %d", sock);
        broadcast_info(sock, buffer); // Informa todos os clientes
    }

    // Remove cliente
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < num_clients; i++) {
        if (client_sockets[i] == sock) {
            client_sockets[i] = client_sockets[num_clients-1];
            num_clients--;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    close(sock);
    tslog_write(LOG_INFO, "Cliente %d desconectado.", sock);
    return NULL;
}


int main() {
    tslog_init("server_log.txt", 1);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(server_fd, MAX_CLIENTS);

    tslog_write(LOG_INFO, "Servidor iniciado na porta %d", PORT);

    while (1) {
        int client_fd = accept(server_fd, NULL, NULL);

        pthread_mutex_lock(&clients_mutex);
        if (num_clients < MAX_CLIENTS) {
            client_sockets[num_clients++] = client_fd;
            pthread_t tid;
            pthread_create(&tid, NULL, client_thread, &client_fd);
            pthread_detach(tid);
            tslog_write(LOG_INFO, "Novo cliente conectado: %d", client_fd);
        } else {
            tslog_write(LOG_WARN, "Limite de clientes atingido.");
            close(client_fd);
        }
        pthread_mutex_unlock(&clients_mutex);
    }

    tslog_write(LOG_INFO, "Servidor encerrado.");
    tslog_close();
    close(server_fd);
    return 0;
}