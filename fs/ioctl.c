/* iXland - IOCTL Operations
 *
 * Canonical owner for ioctl syscall:
 * - ioctl()
 *
 * Linux-shaped canonical owner - iOS mediation as implementation detail
 */

#include <errno.h>
#include <stdarg.h>
#include <sys/ioctl.h>

#include "fdtable.h"

static int ioctl_impl(int fd, unsigned long request, void *arg) {
    if (fd < 0 || fd >= NR_OPEN_DEFAULT) {
        errno = EBADF;
        return -1;
    }

    if (fd <= 2) {
        return ioctl(fd, request, arg);
    }

    void *entry = get_fd_entry_impl(fd);
    if (!entry) {
        errno = EBADF;
        return -1;
    }

    int result = ioctl(get_real_fd_impl(entry), request, arg);
    put_fd_entry_impl(entry);
    return result;
}

__attribute__((visibility("default"))) int ioctl(int fd, unsigned long request, ...) {
    va_list args;
    va_start(args, request);
    void *arg = va_arg(args, void *);
    va_end(args);
    return ioctl_impl(fd, request, arg);
}
