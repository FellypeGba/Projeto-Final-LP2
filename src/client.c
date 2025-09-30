#include "tslog.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 9000

// Thread para receber mensagens do servidor
void *receive_thread(void *arg) {
    int sock = *(int *)arg;
    char buffer[256];
    ssize_t len;
    while ((len = recv(sock, buffer, sizeof(buffer)-1, 0)) > 0) {
        buffer[len] = '\0';
        printf("%s\n", buffer); // Exibe qualquer mensagem recebida do servidor
        tslog_write(LOG_INFO, "Broadcast recebido: %s", buffer);
    }
    return NULL;
}

int main() {
    char logname[64];
    snprintf(logname, sizeof(logname), "client_log_%d.txt", getpid());
    tslog_init(logname, 0);

    int sock = socket(AF_INET, SOCK_STREAM, 0); // Cria socket
    if (sock < 0) {
        perror("Falha ao criar socket");
        tslog_close();
        return 1;
    }
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0) {
        perror("Falha ao conectar");
        tslog_close();
        return 1;
    }

    pthread_t tid;
    tslog_write(LOG_INFO, "Cliente conectado ao servidor %s:%d (thread principal: %lu)", SERVER_IP, SERVER_PORT, pthread_self());
    pthread_create(&tid, NULL, receive_thread, &sock);

    char msg[256];
    printf("Digite sua mensagem:\n");
    while (fgets(msg, sizeof(msg), stdin)) {
        size_t len = strlen(msg);
        if (len > 0 && msg[len - 1] == '\n') {
            msg[len - 1] = '\0';
        }
        send(sock, msg, strlen(msg), 0);
        tslog_write(LOG_INFO, "Mensagem enviada ao servidor pelo cliente %d", sock);

        sleep(1); // Aguarda 1 segundo para receber broadcast e garantir escrita no log
        break;    // Encerra após enviar uma mensagem
    }

    close(sock);
    tslog_close();
    return 0;
}