#include "tslog.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#define NUM_THREADS 5

void *thread_func(void *arg) {
    int id = *(int *)arg;
    for (int i = 0; i < 5; i++) {
        tslog_write(LOG_INFO, "Thread %d escrevendo log %d", id, i);
        usleep(100000); // 0.1s para simular trabalho
    }
    return NULL;
}

int main(void) {
    char input[32];
    int overwrite = 0; // padrão: append

    printf("Digite 1 para sobrescrever o arquivo de log ou qualquer outra tecla para continuar: ");
    
    if (fgets(input, sizeof(input), stdin) != NULL) {
        char *endptr;
        errno = 0;
        long val = strtol(input, &endptr, 10);

        if (errno == 0 && endptr != input && *endptr == '\n') {
            // Conversão bem-sucedida e só número digitado
            overwrite = (val == 1) ? 1 : 0;
        } else {
            printf("Entrada inválida detectada. Mantendo modo padrão (append).\n");
        }
    } else {
        printf("Erro ao ler entrada. Mantendo modo padrão (append).\n");
    }

    if (tslog_init("logs.txt", overwrite) != 0) {
        fprintf(stderr, "Falha ao inicializar logger.\n");
        return EXIT_FAILURE;
    }

    pthread_t threads[NUM_THREADS];
    int ids[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS; i++) {
        ids[i] = i;
        pthread_create(&threads[i], NULL, thread_func, &ids[i]);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    tslog_close();
    return EXIT_SUCCESS;
}