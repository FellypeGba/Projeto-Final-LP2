#ifndef NET_H
#define NET_H

#include <sys/types.h>
#include <sys/socket.h>
#include <stddef.h>

/* Envia todo o buffer, lidando com writes parciais e EINTR. Retorna
 * número de bytes enviados (== len) ou -1 em erro. */
ssize_t send_all(int fd, const void *buf, size_t len);

#endif // NET_H
