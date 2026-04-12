#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include "fdtable.h"

int __ixland_dup_impl(int oldfd) {
    if (oldfd < 0 || oldfd >= IXLAND_MAX_FD) {
        errno = EBADF;
        return -1;
    }

    void *entry = __ixland_get_fd_entry_impl(oldfd);
    if (!entry) {
        errno = EBADF;
        return -1;
    }
    __ixland_put_fd_entry_impl(entry);
    return oldfd;
}

int __ixland_dup2_impl(int oldfd, int newfd) {
    if (oldfd < 0 || oldfd >= IXLAND_MAX_FD || newfd < 0 || newfd >= IXLAND_MAX_FD) {
        errno = EBADF;
        return -1;
    }

    if (oldfd == newfd) {
        return newfd;
    }

    void *old_entry = __ixland_get_fd_entry_impl(oldfd);
    if (!old_entry) {
        errno = EBADF;
        return -1;
    }

    void *new_entry = __ixland_get_fd_entry_impl(newfd);
    if (new_entry) {
        __ixland_put_fd_entry_impl(new_entry);
        __ixland_close_impl(newfd);
    }

    __ixland_clone_fd_entry_impl(newfd, oldfd);
    __ixland_put_fd_entry_impl(old_entry);
    return newfd;
}

int __ixland_dup3_impl(int oldfd, int newfd, int flags) {
    (void)flags;
    return __ixland_dup2_impl(oldfd, newfd);
}

int __ixland_fcntl_impl(int fd, int cmd, ...) {
    if (fd < 0 || fd >= IXLAND_MAX_FD) {
        errno = EBADF;
        return -1;
    }

    va_list args;
    va_start(args, cmd);
    int result = -1;

    switch (cmd) {
    case F_DUPFD:
    case F_DUPFD_CLOEXEC: {
        int minfd = va_arg(args, int);
        result = __ixland_dup2_impl(fd, minfd);
        break;
    }
    case F_GETFD:
        result = 0;
        break;
    case F_SETFD:
        (void)va_arg(args, int);
        result = 0;
        break;
    case F_GETFL: {
        void *entry = __ixland_get_fd_entry_impl(fd);
        if (entry) {
            result = __ixland_get_fd_flags_impl(entry);
            __ixland_put_fd_entry_impl(entry);
        } else {
            errno = EBADF;
        }
        break;
    }
    case F_SETFL: {
        int flags = va_arg(args, int);
        void *entry = __ixland_get_fd_entry_impl(fd);
        if (entry) {
            __ixland_set_fd_flags_impl(entry, flags);
            __ixland_put_fd_entry_impl(entry);
            result = 0;
        } else {
            errno = EBADF;
        }
        break;
    }
    default: {
        int arg = va_arg(args, int);
        if (fd > 2) {
            void *entry = __ixland_get_fd_entry_impl(fd);
            if (entry) {
                result = fcntl(__ixland_get_real_fd_impl(entry), cmd, arg);
                __ixland_put_fd_entry_impl(entry);
            } else {
                errno = EBADF;
            }
        } else {
            result = fcntl(fd, cmd, arg);
        }
        break;
    }
    }

    va_end(args);
    return result;
}

int ixland_dup(int oldfd) {
    return __ixland_dup_impl(oldfd);
}

int ixland_dup2(int oldfd, int newfd) {
    return __ixland_dup2_impl(oldfd, newfd);
}

int ixland_dup3(int oldfd, int newfd, int flags) {
    return __ixland_dup3_impl(oldfd, newfd, flags);
}

int ixland_fcntl(int fd, int cmd, ...) {
    va_list args;
    va_start(args, cmd);
    int arg = va_arg(args, int);
    va_end(args);
    return __ixland_fcntl_impl(fd, cmd, arg);
}
