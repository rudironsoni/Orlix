#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>

#include "../internal/ixland_internal.h"
#include "vfs.h"

int __ixland_open_impl(const char *pathname, int flags, mode_t mode) {
    if (!pathname) {
        errno = EFAULT;
        return -1;
    }

    int fd = __ixland_alloc_fd_impl();
    if (fd < 0) {
        return -1;
    }

    int real_fd = ixland_vfs_open(pathname, flags, mode);
    if (real_fd < 0) {
        __ixland_free_fd_impl(fd);
        return -1;
    }

    __ixland_init_fd_entry_impl(fd, real_fd, flags, mode, pathname);
    return fd;
}

int __ixland_openat_impl(int dirfd, const char *pathname, int flags, mode_t mode) {
    (void)dirfd;
    return __ixland_open_impl(pathname, flags, mode);
}

int __ixland_creat_impl(const char *pathname, mode_t mode) {
    return __ixland_open_impl(pathname, O_WRONLY | O_CREAT | O_TRUNC, mode);
}

int __ixland_close_impl(int fd) {
    if (fd < 0 || fd >= IXLAND_MAX_FD) {
        errno = EBADF;
        return -1;
    }

    if (fd <= 2) {
        return 0;
    }

    void *entry = __ixland_get_fd_entry_impl(fd);
    if (!entry) {
        errno = EBADF;
        return -1;
    }

    int real_fd = __ixland_get_real_fd_impl(entry);
    __ixland_put_fd_entry_impl(entry);
    close(real_fd);
    __ixland_free_fd_impl(fd);
    return 0;
}

int ixland_open(const char *pathname, int flags, ...) {
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, int);
        va_end(args);
    }
    return __ixland_open_impl(pathname, flags, mode);
}

int ixland_creat(const char *pathname, mode_t mode) {
    return __ixland_creat_impl(pathname, mode);
}

int ixland_close(int fd) {
    return __ixland_close_impl(fd);
}
