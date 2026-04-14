#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include "fdtable.h"

int dup_impl(int oldfd) {
    if (oldfd < 0 || oldfd >= NR_OPEN_DEFAULT) {
        errno = EBADF;
        return -1;
    }

    void *entry = get_fd_entry_impl(oldfd);
    if (!entry) {
        errno = EBADF;
        return -1;
    }
    put_fd_entry_impl(entry);
    return oldfd;
}

int dup2_impl(int oldfd, int newfd) {
    if (oldfd < 0 || oldfd >= NR_OPEN_DEFAULT || newfd < 0 || newfd >= NR_OPEN_DEFAULT) {
        errno = EBADF;
        return -1;
    }

    if (oldfd == newfd) {
        return newfd;
    }

    void *old_entry = get_fd_entry_impl(oldfd);
    if (!old_entry) {
        errno = EBADF;
        return -1;
    }

    void *new_entry = get_fd_entry_impl(newfd);
    if (new_entry) {
        put_fd_entry_impl(new_entry);
        close_impl(newfd);
    }

    clone_fd_entry_impl(newfd, oldfd);
    put_fd_entry_impl(old_entry);
    return newfd;
}

int dup3_impl(int oldfd, int newfd, int flags) {
    (void)flags;
    return dup2_impl(oldfd, newfd);
}

int fcntl_impl(int fd, int cmd, ...) {
    if (fd < 0 || fd >= NR_OPEN_DEFAULT) {
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
        result = dup2_impl(fd, minfd);
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
        void *entry = get_fd_entry_impl(fd);
        if (entry) {
            result = get_fd_flags_impl(entry);
            put_fd_entry_impl(entry);
        } else {
            errno = EBADF;
        }
        break;
    }
    case F_SETFL: {
        int flags = va_arg(args, int);
        void *entry = get_fd_entry_impl(fd);
        if (entry) {
            set_fd_flags_impl(entry, flags);
            put_fd_entry_impl(entry);
            result = 0;
        } else {
            errno = EBADF;
        }
        break;
    }
    default: {
        int arg = va_arg(args, int);
        if (fd > 2) {
            void *entry = get_fd_entry_impl(fd);
            if (entry) {
                result = fcntl(get_real_fd_impl(entry), cmd, arg);
                put_fd_entry_impl(entry);
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

__attribute__((visibility("default"))) int dup(int oldfd) {
    return dup_impl(oldfd);
}

__attribute__((visibility("default"))) int dup2(int oldfd, int newfd) {
    return dup2_impl(oldfd, newfd);
}

__attribute__((visibility("default"))) int dup3(int oldfd, int newfd, int flags) {
    return dup3_impl(oldfd, newfd, flags);
}

__attribute__((visibility("default"))) int fcntl(int fd, int cmd, ...) {
    va_list args;
    va_start(args, cmd);
    int arg = va_arg(args, int);
    va_end(args);
    return fcntl_impl(fd, cmd, arg);
}