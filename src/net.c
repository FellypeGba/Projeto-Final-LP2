#include "net.h"
#include <unistd.h>
#include <errno.h>

ssize_t send_all(int fd, const void *buf, size_t len) {
    const char *p = buf;
    size_t left = len;
    while (left > 0) {
        ssize_t n = send(fd, p, left, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) return -1;
        p += n;
        left -= n;
    }
    return (ssize_t)len;
}
