#include "tslog.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>
#include "chat_server.h"
#include "net.h"

#define PORT 9000
#define MAX_CLIENTS 10

static volatile sig_atomic_t server_running = 1;
static int server_fd_global = -1;
static ChatServer chat;

static void sigint_handler(int signo) {
    (void)signo;
    server_running = 0;
    if (server_fd_global != -1) {
        close(server_fd_global);
        server_fd_global = -1;
    }
    /*
     * Handler de SIGINT: marcar o servidor para parar e fechar o socket
     * de escuta. Atenção: chamar funções complexas (por exemplo
     * chat_server_shutdown) diretamente dentro de um signal handler pode
     * ser inseguro, pois muitas funções não são async-signal-safe. Aqui
     * apenas setamos a flag e fechamos o descritor de escuta para que
     * accept() seja desbloqueado; o encerramento completo é feito na
     * thread principal.
     */
    /* sinal recebido; main() irá lidar com o encerramento e registrar o log */
}

// Função para lidar com comunicação de um cliente (em cada thread)
void *client_thread(void *arg) {
    int sock = *(int *)arg;
    free(arg);
    char buffer[256];
    ssize_t len;

    /*
     * Thread por cliente: lê do socket do cliente e enfileira as
     * mensagens recebidas na fila do ChatServer. Esta thread não envia
     * broadcasts diretamente para evitar races em send(); a thread
     * broadcaster faz o reencaminhamento para os demais clientes.
     */
    while ((len = recv(sock, buffer, sizeof(buffer)-1, 0)) > 0) {
        buffer[len] = '\0';
        tslog_write(LOG_INFO, "Mensagem recebida do cliente %d: %s", sock, buffer);
        /* mensagem a ser colocada na fila pra broadcast */
        chat_server_enqueue_message(&chat, buffer, sock);
    }

    /* remove client e faz a limpeza */
    chat_server_remove_client(&chat, sock);
    close(sock);
    tslog_write(LOG_INFO, "Cliente %d desconectado.", sock);
    return NULL;
}

/* old broadcast_info and duplicate client_thread removed; using ChatServer APIs and broadcaster thread */


int main() {
    tslog_init("server_log.txt", 1);
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        tslog_write(LOG_ERROR, "Falha ao criar socket do servidor: %s", strerror(errno));
        tslog_close();
        return 1;
    }

    server_fd_global = server_fd;
    signal(SIGINT, sigint_handler);

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        tslog_write(LOG_ERROR, "Falha no bind: %s", strerror(errno));
        close(server_fd);
        tslog_close();
        return 1;
    }

    if (listen(server_fd, MAX_CLIENTS) < 0) {
        tslog_write(LOG_ERROR, "Falha no listen: %s", strerror(errno));
        close(server_fd);
        tslog_close();
        return 1;
    }

    /*
     * Fluxo principal do servidor:
     * 1. inicializar o logger e o socket de escuta
     * 2. inicializar o ChatServer (lista de clientes, fila, broadcaster)
     * 3. loop de accept: aceitar novas conexões, registrar o cliente com
     *    chat_server_add_client e criar uma thread por cliente que lê do
     *    socket e enfileira mensagens
     * 4. quando sinalizado, sair do loop e executar chat_server_shutdown
     */
    tslog_write(LOG_INFO, "Servidor iniciado na porta %d", PORT);

    if (chat_server_init(&chat, MAX_CLIENTS, 100) != 0) {
        tslog_write(LOG_ERROR, "Falha ao iniciar ChatServer");
        tslog_close();
        close(server_fd);
        return 1;
    }

    while (server_running) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
            if (errno == EINTR) {
                break; // interrompido por signal
            }
            tslog_write(LOG_ERROR, "Accept falhou: %s", strerror(errno));
            continue;
        }

        int *pclient = malloc(sizeof(int));
        if (!pclient) {
            tslog_write(LOG_ERROR, "Memória insuficiente ao aceitar cliente");
            close(client_fd);
            continue;
        }
        *pclient = client_fd;

        if (chat_server_add_client(&chat, client_fd) != 0) {
            tslog_write(LOG_WARN, "Limite de clientes atingido. Rejeitando fd=%d", client_fd);
            free(pclient);
            close(client_fd);
            continue;
        }

        pthread_t tid;
        if (pthread_create(&tid, NULL, client_thread, pclient) != 0) {
            tslog_write(LOG_ERROR, "Falha ao criar thread para cliente %d", client_fd);
            chat_server_remove_client(&chat, client_fd);
            free(pclient);
            close(client_fd);
            continue;
        }
        pthread_detach(tid);
        tslog_write(LOG_INFO, "Novo cliente conectado: %d", client_fd);
    }
    /* main thread detected server_running == 0 or accept was interrupted */
    tslog_write(LOG_INFO, "Servidor encerrando: sinal recebido ou loop de accept finalizado.");
    chat_server_shutdown(&chat);
    tslog_close();
    if (server_fd >= 0) close(server_fd);
    return 0;
}