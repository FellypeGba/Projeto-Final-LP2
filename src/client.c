#include "tslog.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 9000

// Thread para receber mensagens do servidor
void *receive_thread(void *arg) {
    int sock = *(int *)arg;
    free(arg);
    char buffer[256];
    ssize_t len;
    /*
     * Receiver thread: prints any message sent by the server. It uses a
     * fixed-size buffer (256 bytes); messages larger than this will be
     * truncated. For a production implementation consider a length-prefix
     * protocol or dynamic buffer growth.
     */
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
    int *psock = malloc(sizeof(int));
    if (!psock) {
        perror("malloc");
        close(sock);
        tslog_close();
        return 1;
    }
    *psock = sock;
    if (pthread_create(&tid, NULL, receive_thread, psock) != 0) {
        tslog_write(LOG_ERROR, "Falha ao criar thread de recepção");
        free(psock);
        close(sock);
        tslog_close();
        return 1;
    }

    char msg[256];
    printf("Digite sua mensagem:\n");
    while (fgets(msg, sizeof(msg), stdin)) {
        size_t len = strlen(msg);
        if (len > 0 && msg[len - 1] == '\n') {
            msg[len - 1] = '\0';
        }
        if (strcmp(msg, "/quit") == 0) {
            tslog_write(LOG_INFO, "Cliente solicitado /quit");
            break;
        }
        /*
         * Sending: we use a single send() call. In rare cases send() may
         * perform a partial write; a robust implementation should loop
         * until the whole buffer is sent or an error occurs (see
         * send_all suggestion in the checkpoint notes).
         */
        ssize_t s = send(sock, msg, strlen(msg), 0);
        if (s < 0) {
            perror("send");
            tslog_write(LOG_ERROR, "Falha no send do cliente %d: %s", sock, strerror(errno));
            break;
        }
        tslog_write(LOG_INFO, "Mensagem enviada ao servidor pelo cliente %d", sock);

        /* small pause used in tests to increase chance of seeing broadcast
         * output before the client exits; this is optional and can be
         * removed once tests are stable. */
        sleep(1);
    }

    close(sock);
    tslog_close();
    return 0;
}