#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>

#include "fdtable.h"
#include "vfs.h"

int open_impl(const char *pathname, int flags, mode_t mode) {
    if (!pathname) {
        errno = EFAULT;
        return -1;
    }

    int fd = alloc_fd_impl();
    if (fd < 0) {
        return -1;
    }

    int real_fd;
    int ret = vfs_open(pathname, flags, mode, &real_fd);
    if (ret < 0) {
        free_fd_impl(fd);
        return -1;
    }

    init_fd_entry_impl(fd, real_fd, flags, mode, pathname);
    return fd;
}

int openat_impl(int dirfd, const char *pathname, int flags, mode_t mode) {
    (void)dirfd;
    return open_impl(pathname, flags, mode);
}

int creat_impl(const char *pathname, mode_t mode) {
    return open_impl(pathname, O_WRONLY | O_CREAT | O_TRUNC, mode);
}

int close_impl(int fd) {
    if (fd < 0 || fd >= NR_OPEN_DEFAULT) {
        errno = EBADF;
        return -1;
    }

    if (fd <= 2) {
        return 0;
    }

    void *entry = get_fd_entry_impl(fd);
    if (!entry) {
        errno = EBADF;
        return -1;
    }

    int real_fd = get_real_fd_impl(entry);
    put_fd_entry_impl(entry);
    close(real_fd);
    free_fd_impl(fd);
    return 0;
}

__attribute__((visibility("default"))) int open(const char *pathname, int flags, ...) {
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, int);
        va_end(args);
    }
    return open_impl(pathname, flags, mode);
}

__attribute__((visibility("default"))) int openat(int dirfd, const char *pathname, int flags, ...) {
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, int);
        va_end(args);
    }
    (void)dirfd;
    return open_impl(pathname, flags, mode);
}

__attribute__((visibility("default"))) int creat(const char *pathname, mode_t mode) {
    return creat_impl(pathname, mode);
}

__attribute__((visibility("default"))) int close(int fd) {
    return close_impl(fd);
}
