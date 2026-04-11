#include <errno.h>
#include <stdarg.h>
#include <sys/ioctl.h>

#include "../src/ixland/internal/ixland_internal.h"

int __ixland_ioctl_impl(int fd, unsigned long request, ...) {
    if (fd < 0 || fd >= IXLAND_MAX_FD) {
        errno = EBADF;
        return -1;
    }

    va_list args;
    va_start(args, request);
    void *arg = va_arg(args, void *);
    va_end(args);

    if (fd <= 2) {
        return ioctl(fd, request, arg);
    }

    void *entry = __ixland_get_fd_entry_impl(fd);
    if (!entry) {
        errno = EBADF;
        return -1;
    }

    int result = ioctl(__ixland_get_real_fd_impl(entry), request, arg);
    __ixland_put_fd_entry_impl(entry);
    return result;
}

int ixland_ioctl(int fd, unsigned long request, ...) {
    va_list args;
    va_start(args, request);
    void *arg = va_arg(args, void *);
    va_end(args);
    return __ixland_ioctl_impl(fd, request, arg);
}
