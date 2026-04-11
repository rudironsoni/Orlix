/* iXland - Random Number Generator
 *
 * Canonical owner for random syscalls:
 * - getrandom()
 * - getentropy()
 *
 * Linux-shaped canonical owner - iOS mediation as implementation detail
 */

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

/* iOS doesn't have sys/random.h, but getentropy is available */
extern int getentropy(void *buf, size_t buflen);

#include "../internal/ixland_internal.h"

/* ============================================================================
 * GETRANDOM - Linux-compatible random bytes
 * ============================================================================ */

ssize_t ixland_getrandom(void *buf, size_t buflen, unsigned int flags) {
    /* Use iOS arc4random_buf for secure random */
    (void)flags;

    if (!buf) {
        errno = EFAULT;
        return -1;
    }

    /* Use getentropy for small requests */
    if (buflen <= 256) {
        return getentropy(buf, buflen) == 0 ? (ssize_t)buflen : -1;
    }

    /* For larger requests, use /dev/urandom */
    int fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        errno = EIO;
        return -1;
    }

    ssize_t total = 0;
    char *p = buf;
    while ((size_t)total < buflen) {
        ssize_t n = read(fd, p + total, buflen - total);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            close(fd);
            return -1;
        }
        total += n;
    }

    close(fd);
    return total;
}

/* ============================================================================
 * GETENTROPY - BSD-compatible
 * ============================================================================ */

int ixland_getentropy(void *buffer, size_t length) {
    return getentropy(buffer, length);
}
