#include "tslog.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include "net.h"

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 9000

// Thread para receber mensagens do servidor
void *receive_thread(void *arg) {
    int sock = *(int *)arg;
    free(arg);
    char buffer[256];
    ssize_t len;
    /*
     * Thread de recepção: imprime qualquer mensagem enviada pelo servidor.
     * Usa um buffer de tamanho fixo (256 bytes); mensagens maiores serão
     * truncadas.
     */
    while ((len = recv(sock, buffer, sizeof(buffer)-1, 0)) > 0) {
        buffer[len] = '\0';
        printf("%s\n", buffer); // Exibe qualquer mensagem recebida do servidor
        tslog_write(LOG_INFO, "Broadcast recebido: %s", buffer);
    }
    return NULL;
}

int main(int argc, char **argv) {
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
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) != 1) {
        fprintf(stderr, "Endereco IP invalido: %s\n", SERVER_IP);
        close(sock);
        tslog_close();
        return 1;
    }

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0) {
        perror("Falha ao conectar");
        tslog_close();
        return 1;
    }

    /* Se o usuário passou um nome como argumento, envie como primeira mensagem usando o prefixo NAME: */
    if (argc >= 2) {
        const char *name = argv[1];
        char name_msg[256];
        /* enviar NAME com newline para delimitar claramente o campo */
        snprintf(name_msg, sizeof(name_msg), "NAME:%s\n", name);
        if (send_all(sock, name_msg, strlen(name_msg)) < 0) {
            perror("send (name)");
            tslog_write(LOG_WARN, "Falha ao enviar nome do cliente");
        } else {
            tslog_write(LOG_INFO, "Nome do cliente enviado: %s", name);
        }
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
        fprintf(stderr, "Falha ao criar thread de recepcao\n");
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
     * Envio: usa-se uma única chamada send(). Em casos raros send()
     * pode escrever parcialmente; uma implementação robusta deveria
     * iterar até enviar tudo ou ocorrer um erro (ver sugestão
     * send_all nas notas do checkpoint).
     */
        if (send_all(sock, msg, strlen(msg)) < 0) {
            perror("send");
            tslog_write(LOG_ERROR, "Falha no send do cliente %d: %s", sock, strerror(errno));
            break;
        }
        tslog_write(LOG_INFO, "Mensagem enviada ao servidor pelo cliente %d", sock);

        /* pequena pausa usada em testes para aumentar a chance de ver o
         * broadcast antes do cliente encerrar; é opcional e pode ser
         * removida quando os testes estiverem estáveis. */
        sleep(1);
    }

    close(sock);
    tslog_close();
    return 0;
}