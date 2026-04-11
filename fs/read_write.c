#include <errno.h>
#include <unistd.h>

#include "../src/ixland/internal/ixland_internal.h"

ssize_t __ixland_read_impl(int fd, void *buf, size_t count) {
    if (!buf) {
        errno = EFAULT;
        return -1;
    }

    if (fd < 0 || fd >= IXLAND_MAX_FD) {
        errno = EBADF;
        return -1;
    }

    if (fd <= 2) {
        return read(fd, buf, count);
    }

    void *entry = __ixland_get_fd_entry_impl(fd);
    if (!entry) {
        errno = EBADF;
        return -1;
    }

    ssize_t bytes = read(__ixland_get_real_fd_impl(entry), buf, count);
    if (bytes > 0) {
        __ixland_set_fd_offset_impl(entry, __ixland_get_fd_offset_impl(entry) + bytes);
    }

    __ixland_put_fd_entry_impl(entry);
    return bytes;
}

ssize_t __ixland_write_impl(int fd, const void *buf, size_t count) {
    if (!buf) {
        errno = EFAULT;
        return -1;
    }

    if (fd < 0 || fd >= IXLAND_MAX_FD) {
        errno = EBADF;
        return -1;
    }

    if (fd <= 2) {
        return write(fd, buf, count);
    }

    void *entry = __ixland_get_fd_entry_impl(fd);
    if (!entry) {
        errno = EBADF;
        return -1;
    }

    ssize_t bytes = write(__ixland_get_real_fd_impl(entry), buf, count);
    if (bytes > 0) {
        __ixland_set_fd_offset_impl(entry, __ixland_get_fd_offset_impl(entry) + bytes);
    }

    __ixland_put_fd_entry_impl(entry);
    return bytes;
}

off_t __ixland_lseek_impl(int fd, off_t offset, int whence) {
    if (fd < 0 || fd >= IXLAND_MAX_FD) {
        errno = EBADF;
        return -1;
    }

    if (fd <= 2) {
        errno = ESPIPE;
        return -1;
    }

    void *entry = __ixland_get_fd_entry_impl(fd);
    if (!entry) {
        errno = EBADF;
        return -1;
    }

    off_t result = lseek(__ixland_get_real_fd_impl(entry), offset, whence);
    if (result >= 0) {
        __ixland_set_fd_offset_impl(entry, result);
    }

    __ixland_put_fd_entry_impl(entry);
    return result;
}

ssize_t __ixland_pread_impl(int fd, void *buf, size_t count, off_t offset) {
    if (fd <= 2) {
        errno = ESPIPE;
        return -1;
    }

    void *entry = __ixland_get_fd_entry_impl(fd);
    if (!entry) {
        errno = EBADF;
        return -1;
    }

    ssize_t bytes = pread(__ixland_get_real_fd_impl(entry), buf, count, offset);
    __ixland_put_fd_entry_impl(entry);
    return bytes;
}

ssize_t __ixland_pwrite_impl(int fd, const void *buf, size_t count, off_t offset) {
    if (fd <= 2) {
        errno = ESPIPE;
        return -1;
    }

    void *entry = __ixland_get_fd_entry_impl(fd);
    if (!entry) {
        errno = EBADF;
        return -1;
    }

    ssize_t bytes = pwrite(__ixland_get_real_fd_impl(entry), buf, count, offset);
    __ixland_put_fd_entry_impl(entry);
    return bytes;
}

ssize_t ixland_read(int fd, void *buf, size_t count) {
    return __ixland_read_impl(fd, buf, count);
}

ssize_t ixland_write(int fd, const void *buf, size_t count) {
    return __ixland_write_impl(fd, buf, count);
}

off_t ixland_lseek(int fd, off_t offset, int whence) {
    return __ixland_lseek_impl(fd, offset, whence);
}

ssize_t ixland_pread(int fd, void *buf, size_t count, off_t offset) {
    return __ixland_pread_impl(fd, buf, count, offset);
}

ssize_t ixland_pwrite(int fd, const void *buf, size_t count, off_t offset) {
    return __ixland_pwrite_impl(fd, buf, count, offset);
}
