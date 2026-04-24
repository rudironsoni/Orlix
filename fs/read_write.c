/* IXLandSystem/fs/read_write.c
 * Virtual read/write/lseek implementation
 */

/* Include Linux UAPI constants FIRST */
#include "third_party/linux-uapi/6.12/arm64/include/ixland/linux_uapi_constants.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "fdtable.h"
#include "internal/ios/fs/backing_io_decls.h"
#include "internal/ios/fs/sync.h"
#include "pty.h"
#include "vfs.h"

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

    if (get_fd_is_synthetic_dev_impl(entry)) {
        synthetic_dev_node_t dev_node = get_fd_synthetic_dev_node_impl(entry);
        put_fd_entry_impl(entry);

        if (dev_node == SYNTHETIC_DEV_NULL) {
            return 0;
        } else if (dev_node == SYNTHETIC_DEV_ZERO) {
            memset(buf, 0, count);
            return (ssize_t)count;
        } else if (dev_node == SYNTHETIC_DEV_URANDOM) {
            arc4random_buf(buf, count);
            return (ssize_t)count;
        }
        errno = EINVAL;
        return -1;
    }

    if (get_fd_is_synthetic_pty_impl(entry)) {
        unsigned int pty_index = get_fd_synthetic_pty_index_impl(entry);
        bool is_master = get_fd_is_synthetic_pty_master_impl(entry);
        put_fd_entry_impl(entry);

        bool nonblock = (get_fd_flags_impl(entry) & O_NONBLOCK) != 0;
        return pty_read_slave_impl(pty_index, buf, count, nonblock);
    }

    if (get_fd_is_synthetic_dir_impl(entry)) {
        put_fd_entry_impl(entry);
        errno = EISDIR;
        return -1;
    }

    if (get_fd_is_synthetic_proc_file_impl(entry)) {
        synthetic_proc_file_t proc_file = get_fd_synthetic_proc_file_impl(entry);
        int fd_num = get_fd_proc_file_fd_num_impl(entry);
        put_fd_entry_impl(entry);

        return vfs_proc_file_read(proc_file, fd_num, buf, count);
    }

    ssize_t bytes = host_read_impl(get_real_fd_impl(entry), buf, count);
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

    if (get_fd_is_synthetic_dev_impl(entry)) {
        synthetic_dev_node_t dev_node = get_fd_synthetic_dev_node_impl(entry);
        put_fd_entry_impl(entry);

        if (dev_node == SYNTHETIC_DEV_NULL) {
            return (ssize_t)count;
        } else if (dev_node == SYNTHETIC_DEV_ZERO || dev_node == SYNTHETIC_DEV_URANDOM) {
            errno = EINVAL;
            return -1;
        }
    }

    if (get_fd_is_synthetic_pty_impl(entry)) {
        unsigned int pty_index = get_fd_synthetic_pty_index_impl(entry);
        bool is_master = get_fd_is_synthetic_pty_master_impl(entry);
        put_fd_entry_impl(entry);

        if (is_master) {
            bool nonblock = (get_fd_flags_impl(entry) & O_NONBLOCK) != 0;
            return pty_write_master_impl(pty_index, buf, count, nonblock);
        }

        bool nonblock = (get_fd_flags_impl(entry) & O_NONBLOCK) != 0;
        return pty_write_slave_impl(pty_index, buf, count, nonblock);
    }

    if (get_fd_is_synthetic_dir_impl(entry)) {
        put_fd_entry_impl(entry);
        errno = EISDIR;
        return -1;
    }

    if (get_fd_is_synthetic_proc_file_impl(entry)) {
        put_fd_entry_impl(entry);
        errno = EINVAL;
        return -1;
    }

    off_t current_size = host_lseek_impl(get_real_fd_impl(entry), 0, SEEK_END);
    if (current_size < 0) {
        put_fd_entry_impl(entry);
        return -1;
    }

    if (get_fd_is_append_impl(entry)) {
        if (host_lseek_impl(get_real_fd_impl(entry), 0, SEEK_END) < 0) {
            put_fd_entry_impl(entry);
            return -1;
        }
    }

    ssize_t bytes = host_write_impl(get_real_fd_impl(entry), buf, count);
    put_fd_entry_impl(entry);
    return bytes;
}

off_t lseek_impl(int fd, off_t offset, int whence) {
    void *entry;

    if (fd < 0 || fd >= NR_OPEN_DEFAULT) {
        errno = EBADF;
        return (off_t)-1;
    }

    if (fd <= 2) {
        return host_lseek_impl(fd, offset, whence);
    }

    entry = get_fd_entry_impl(fd);
    if (!entry) {
        errno = EBADF;
        return (off_t)-1;
    }

    if (get_fd_is_synthetic_dir_impl(entry) || get_fd_is_synthetic_proc_file_impl(entry)) {
        put_fd_entry_impl(entry);
        errno = ESPIPE;
        return (off_t)-1;
    }

    if (get_fd_is_synthetic_dev_impl(entry) || get_fd_is_synthetic_pty_impl(entry)) {
        put_fd_entry_impl(entry);
        errno = ESPIPE;
        return (off_t)-1;
    }

    off_t result = host_lseek_impl(get_real_fd_impl(entry), offset, whence);
    put_fd_entry_impl(entry);
    return result;
}

ssize_t pread_impl(int fd, void *buf, size_t count, off_t offset) {
    if (!buf) {
        errno = EFAULT;
        return -1;
    }

    if (fd < 0 || fd >= NR_OPEN_DEFAULT) {
        errno = EBADF;
        return -1;
    }

    if (fd <= 2) {
        return host_pread_impl(fd, buf, count, offset);
    }

    void *entry = get_fd_entry_impl(fd);
    if (!entry) {
        errno = EBADF;
        return -1;
    }

    if (get_fd_is_synthetic_dir_impl(entry)) {
        put_fd_entry_impl(entry);
        errno = EISDIR;
        return -1;
    }

    ssize_t bytes = host_pread_impl(get_real_fd_impl(entry), buf, count, offset);
    put_fd_entry_impl(entry);
    return bytes;
}

ssize_t pwrite_impl(int fd, const void *buf, size_t count, off_t offset) {
    if (!buf) {
        errno = EFAULT;
        return -1;
    }

    if (fd < 0 || fd >= NR_OPEN_DEFAULT) {
        errno = EBADF;
        return -1;
    }

    if (fd <= 2) {
        return host_pwrite_impl(fd, buf, count, offset);
    }

    void *entry = get_fd_entry_impl(fd);
    if (!entry) {
        errno = EBADF;
        return -1;
    }

    if (get_fd_is_synthetic_dir_impl(entry)) {
        put_fd_entry_impl(entry);
        errno = EISDIR;
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
