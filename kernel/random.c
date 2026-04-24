/* iXland - Random Number Generator
 *
 * Canonical owner for random syscalls:
 * - getrandom()
 * - getentropy()
 *
 * Linux-shaped canonical owner - iOS mediation as implementation detail
 */

/* Include Linux UAPI constants FIRST */
#include "../third_party/linux-uapi/6.12/arm64/include/ixland/linux_uapi_constants.h"

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

/* iOS system getentropy - private implementation detail */
extern int _getentropy(void *buf, size_t buflen);

/* ============================================================================
 * GETRANDOM - Linux-compatible random bytes
 * ============================================================================ */

static ssize_t getrandom_impl(void *buf, size_t buflen, unsigned int flags) {
    /* Use iOS arc4random_buf for secure random */
    (void)flags;

    if (!buf) {
        errno = EFAULT;
        return -1;
    }

    /* Use getentropy for small requests */
    if (buflen <= 256) {
        return _getentropy(buf, buflen) == 0 ? (ssize_t)buflen : -1;
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

static int getentropy_impl(void *buffer, size_t length) {
    return _getentropy(buffer, length);
}

/* ============================================================================
 * Public Canonical Syscalls
 * ============================================================================ */

__attribute__((visibility("default"))) ssize_t getrandom(void *buf, size_t buflen, unsigned int flags) {
    return getrandom_impl(buf, buflen, flags);
}

__attribute__((visibility("default"))) int getentropy(void *buffer, size_t length) {
    return getentropy_impl(buffer, length);
}
