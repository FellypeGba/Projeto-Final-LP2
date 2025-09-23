#include "tslog.h"
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>

static FILE *log_file = NULL;
static pthread_mutex_t log_mutex;

static const char *level_to_str(log_level_t level) {
    switch (level) {
        case LOG_INFO:  return "INFO";
        case LOG_WARN:  return "WARN";
        case LOG_ERROR: return "ERROR";
        case LOG_DEBUG: return "DEBUG";
        default:        return "UNKNOWN";
    }
}

int tslog_init(const char *filename) {
    if (filename == NULL) {
        log_file = stdout;
    } else {
        log_file = fopen(filename, "a");
        if (!log_file) {
            perror("Erro abrindo arquivo de log");
            return -1;
        }
    }

    if (pthread_mutex_init(&log_mutex, NULL) != 0) {
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
    pthread_mutex_destroy(&log_mutex);
}

void tslog_write(log_level_t level, const char *fmt, ...) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    va_list args;
    va_start(args, fmt);

    pthread_mutex_lock(&log_mutex);

    fprintf(log_file, "[%02d:%02d:%02d] [%s] ",
            t->tm_hour, t->tm_min, t->tm_sec, level_to_str(level));
    vfprintf(log_file, fmt, args);
    fprintf(log_file, "\n");
    fflush(log_file);

    pthread_mutex_unlock(&log_mutex);

    va_end(args);
}
