/* IXLandSystem/fs/read_write.c
 * Virtual read/write/lseek implementation
 */
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "fdtable.h"
#include "host_darwin.h"

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
        return host_read_impl(fd, buf, count);
    }

    void *entry = get_fd_entry_impl(fd);
    if (!entry) {
        errno = EBADF;
        return -1;
    }

    ssize_t bytes = host_read_impl(get_real_fd_impl(entry), buf, count);
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
        return host_write_impl(fd, buf, count);
    }

    void *entry = get_fd_entry_impl(fd);
    if (!entry) {
        errno = EBADF;
        return -1;
    }

    int real_fd = get_real_fd_impl(entry);

    /* Check if O_APPEND is set - writes must go to end of file */
    bool is_append = get_fd_is_append_impl(entry);
    if (is_append) {
        /* Seek to end before writing */
        off_t current_size = host_lseek_impl(real_fd, 0, SEEK_END);
        if (current_size < 0) {
            put_fd_entry_impl(entry);
            return -1;
        }
    }

    ssize_t bytes = host_write_impl(real_fd, buf, count);
    if (bytes > 0) {
        /* Only update local offset on append/regular write */
        off_t new_offset = get_fd_offset_impl(entry) + bytes;
        set_fd_offset_impl(entry, new_offset);
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

    off_t result = host_lseek_impl(get_real_fd_impl(entry), offset, whence);
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

    ssize_t bytes = host_pread_impl(get_real_fd_impl(entry), buf, count, offset);
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

    ssize_t bytes = host_pwrite_impl(get_real_fd_impl(entry), buf, count, offset);
    put_fd_entry_impl(entry);
    return bytes;
}

__attribute__((visibility("default"))) ssize_t read(int fd, void *buf, size_t count) {
    return read_impl(fd, buf, count);
}

__attribute__((visibility("default"))) ssize_t write(int fd, const void *buf, size_t count) {
    return write_impl(fd, buf, count);
}

__attribute__((visibility("default"))) off_t lseek(int fd, off_t offset, int whence) {
    return lseek_impl(fd, offset, whence);
}

__attribute__((visibility("default"))) ssize_t pread(int fd, void *buf, size_t count, off_t offset) {
    return pread_impl(fd, buf, count, offset);
}

__attribute__((visibility("default"))) ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset) {
    return pwrite_impl(fd, buf, count, offset);
}

/* ============================================================================
 * Scatter/Gather I/O (readv/writev)
 * ============================================================================ */

#include <sys/uio.h>

ssize_t readv_impl(int fd, const struct iovec *iov, int iovcnt) {
    if (!iov || iovcnt < 0) {
        errno = EINVAL;
        return -1;
    }
    
    if (iovcnt == 0) {
        return 0;
    }
    
    if (fd < 0 || fd >= NR_OPEN_DEFAULT) {
        errno = EBADF;
        return -1;
    }
    
    if (fd <= 2) {
        return host_readv_impl(fd, iov, iovcnt);
    }
    
    void *entry = get_fd_entry_impl(fd);
    if (!entry) {
        errno = EBADF;
        return -1;
    }
    
    ssize_t total = 0;
    for (int i = 0; i < iovcnt; i++) {
        if (iov[i].iov_len == 0) {
            continue;
        }
        if (!iov[i].iov_base) {
            put_fd_entry_impl(entry);
            errno = EFAULT;
            return -1;
        }
        
        ssize_t bytes = host_read_impl(get_real_fd_impl(entry), iov[i].iov_base, iov[i].iov_len);
        if (bytes < 0) {
            put_fd_entry_impl(entry);
            return -1;
        }
        if (bytes == 0) {
            break; /* EOF */
        }
        total += bytes;
        if ((size_t)bytes < iov[i].iov_len) {
            break; /* Short read */
        }
    }
    
    if (total > 0) {
        set_fd_offset_impl(entry, get_fd_offset_impl(entry) + total);
    }
    
    put_fd_entry_impl(entry);
    return total;
}

ssize_t writev_impl(int fd, const struct iovec *iov, int iovcnt) {
    if (!iov || iovcnt < 0) {
        errno = EINVAL;
        return -1;
    }
    
    if (iovcnt == 0) {
        return 0;
    }
    
    if (fd < 0 || fd >= NR_OPEN_DEFAULT) {
        errno = EBADF;
        return -1;
    }
    
    if (fd <= 2) {
        return host_writev_impl(fd, iov, iovcnt);
    }
    
    void *entry = get_fd_entry_impl(fd);
    if (!entry) {
        errno = EBADF;
        return -1;
    }
    
    int real_fd = get_real_fd_impl(entry);
    
    /* Handle O_APPEND */
    if (get_fd_is_append_impl(entry)) {
        if (host_lseek_impl(real_fd, 0, SEEK_END) < 0) {
            put_fd_entry_impl(entry);
            return -1;
        }
    }
    
    ssize_t total = 0;
    for (int i = 0; i < iovcnt; i++) {
        if (iov[i].iov_len == 0) {
            continue;
        }
        if (!iov[i].iov_base) {
            put_fd_entry_impl(entry);
            errno = EFAULT;
            return -1;
        }
        
        ssize_t bytes = host_write_impl(real_fd, iov[i].iov_base, iov[i].iov_len);
        if (bytes < 0) {
            put_fd_entry_impl(entry);
            return -1;
        }
        total += bytes;
        if ((size_t)bytes < iov[i].iov_len) {
            break; /* Short write */
        }
    }
    
    if (total > 0) {
        set_fd_offset_impl(entry, get_fd_offset_impl(entry) + total);
    }
    
    put_fd_entry_impl(entry);
    return total;
}

__attribute__((visibility("default"))) ssize_t readv(int fd, const struct iovec *iov, int iovcnt) {
    return readv_impl(fd, iov, iovcnt);
}

__attribute__((visibility("default"))) ssize_t writev(int fd, const struct iovec *iov, int iovcnt) {
    return writev_impl(fd, iov, iovcnt);
}
