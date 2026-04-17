#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include "fdtable.h"
#include "vfs.h"

#define MAX_PATH 4096

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
    char translated_path[MAX_PATH];
    int ret;

    if (!pathname) {
        errno = EFAULT;
        return -1;
    }

    ret = vfs_translate_path_at(dirfd, pathname, translated_path, sizeof(translated_path));
    if (ret != 0) {
        errno = -ret;
        return -1;
    }

    int fd = alloc_fd_impl();
    if (fd < 0) {
        return -1;
    }

    int real_fd;
    ret = vfs_open(translated_path, flags, mode, &real_fd);
    if (ret < 0) {
        free_fd_impl(fd);
        errno = -ret;
        return -1;
    }

    init_fd_entry_impl(fd, real_fd, flags, mode, translated_path);
    return fd;
}

int creat_impl(const char *pathname, mode_t mode) {
    return open_impl(pathname, O_WRONLY | O_CREAT | O_TRUNC, mode);
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
    return openat_impl(dirfd, pathname, flags, mode);
}

__attribute__((visibility("default"))) int creat(const char *pathname, mode_t mode) {
    return creat_impl(pathname, mode);
}

__attribute__((visibility("default"))) int close(int fd) {
    return close_impl(fd);
}
