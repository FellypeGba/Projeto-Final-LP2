#ifndef TSLOG_H
#define TSLOG_H

#include <stdio.h>
#include <pthread.h>

/**
 * Níveis de log
 */
typedef enum {
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_DEBUG
} log_level_t;

/**
 * Inicializa o logger.
 * @param filename: arquivo de saída.
 * @param overwrite: 0 ou 1, define se o arquivo será sobrescrito ou não.
 * @return 0 se sucesso, -1 se erro.
 */
int tslog_init(const char *filename, int overwrite);

/**
 * Fecha o logger e libera recursos.
 */
void tslog_close(void);

/**
 * Escreve uma mensagem de log thread-safe.
 * @param level: nível da mensagem.
 * @param fmt: formato printf-like.
 */
void tslog_write(log_level_t level, const char *fmt, ...);

#endif // TSLOG_H
