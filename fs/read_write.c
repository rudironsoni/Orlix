#include <errno.h>
#include <unistd.h>

#include "fdtable.h"

ssize_t read_impl(int fd, void *buf, size_t count) {
    if (!buf) {
        errno = EFAULT;
        return -1;
    }

    if (fd < 0 || fd >= NR_OPEN_DEFAULT) {
        errno = EBADF;
        return -1;
    }

    if (fd <= 2) {
        return read(fd, buf, count);
    }

    void *entry = get_fd_entry_impl(fd);
    if (!entry) {
        errno = EBADF;
        return -1;
    }

    ssize_t bytes = read(get_real_fd_impl(entry), buf, count);
    if (bytes > 0) {
        set_fd_offset_impl(entry, get_fd_offset_impl(entry) + bytes);
    }

    put_fd_entry_impl(entry);
    return bytes;
}

ssize_t write_impl(int fd, const void *buf, size_t count) {
    if (!buf) {
        errno = EFAULT;
        return -1;
    }

    if (fd < 0 || fd >= NR_OPEN_DEFAULT) {
        errno = EBADF;
        return -1;
    }

    if (fd <= 2) {
        return write(fd, buf, count);
    }

    void *entry = get_fd_entry_impl(fd);
    if (!entry) {
        errno = EBADF;
        return -1;
    }

    ssize_t bytes = write(get_real_fd_impl(entry), buf, count);
    if (bytes > 0) {
        set_fd_offset_impl(entry, get_fd_offset_impl(entry) + bytes);
    }

    put_fd_entry_impl(entry);
    return bytes;
}

off_t lseek_impl(int fd, off_t offset, int whence) {
    if (fd < 0 || fd >= NR_OPEN_DEFAULT) {
        errno = EBADF;
        return -1;
    }

    if (fd <= 2) {
        errno = ESPIPE;
        return -1;
    }

    void *entry = get_fd_entry_impl(fd);
    if (!entry) {
        errno = EBADF;
        return -1;
    }

    off_t result = lseek(get_real_fd_impl(entry), offset, whence);
    if (result >= 0) {
        set_fd_offset_impl(entry, result);
    }

    put_fd_entry_impl(entry);
    return result;
}

ssize_t pread_impl(int fd, void *buf, size_t count, off_t offset) {
    if (fd <= 2) {
        errno = ESPIPE;
        return -1;
    }

    void *entry = get_fd_entry_impl(fd);
    if (!entry) {
        errno = EBADF;
        return -1;
    }

    ssize_t bytes = pread(get_real_fd_impl(entry), buf, count, offset);
    put_fd_entry_impl(entry);
    return bytes;
}

ssize_t pwrite_impl(int fd, const void *buf, size_t count, off_t offset) {
    if (fd <= 2) {
        errno = ESPIPE;
        return -1;
    }

    void *entry = get_fd_entry_impl(fd);
    if (!entry) {
        errno = EBADF;
        return -1;
    }

    ssize_t bytes = pwrite(get_real_fd_impl(entry), buf, count, offset);
    put_fd_entry_impl(entry);
    return bytes;
}

ssize_t ixland_read(int fd, void *buf, size_t count) {
    return read_impl(fd, buf, count);
}

ssize_t ixland_write(int fd, const void *buf, size_t count) {
    return write_impl(fd, buf, count);
}

off_t ixland_lseek(int fd, off_t offset, int whence) {
    return lseek_impl(fd, offset, whence);
}

ssize_t ixland_pread(int fd, void *buf, size_t count, off_t offset) {
    return pread_impl(fd, buf, count, offset);
}

ssize_t ixland_pwrite(int fd, const void *buf, size_t count, off_t offset) {
    return pwrite_impl(fd, buf, count, offset);
}
