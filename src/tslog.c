#include "tslog.h"
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>

static FILE *log_file = NULL;
static pthread_mutex_t log_mutex;

static const char *level_to_str(log_level_t level) { //Mensagens de Erro
    switch (level) {
        case LOG_INFO:  return "INFO";
        case LOG_WARN:  return "WARN";
        case LOG_ERROR: return "ERROR";
        case LOG_DEBUG: return "DEBUG";
        default:        return "UNKNOWN";
    }
}

int tslog_init(const char *filename, int overwrite) { //Inicialização do arquivo de logs
    if (filename == NULL) {
        log_file = stdout;
    } else {
        const char *mode = overwrite ? "w" : "a";  // escolhe modo
        log_file = fopen(filename, mode);
        if (!log_file) {
            perror("Erro abrindo arquivo de log");
            return -1;
        }
    }

    if (pthread_mutex_init(&log_mutex, NULL) != 0) { //Criação do mutex para exclusão mútua
        perror("Erro inicializando mutex");
        if (filename) fclose(log_file);
        return -1;
    }

    return 0;
}


void tslog_close(void) {
    if (log_file && log_file != stdout && log_file != stderr) {
        fclose(log_file);
    }
    pthread_mutex_destroy(&log_mutex); //Encerramento das ferramentas de Sincronização
}

void tslog_write(log_level_t level, const char *fmt, ...) { //Escrita no log
    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    va_list args;
    va_start(args, fmt);

    pthread_mutex_lock(&log_mutex); //Obtém a trava para escrever no log

    fprintf(log_file, "[%02d:%02d:%02d] [%s] ",
            t->tm_hour, t->tm_min, t->tm_sec, level_to_str(level));
    vfprintf(log_file, fmt, args);
    fprintf(log_file, "\n");
    fflush(log_file);

    pthread_mutex_unlock(&log_mutex); //Libera a trava do log

    va_end(args);
}
