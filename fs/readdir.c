#include <dirent.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "../internal/ixland_internal.h"

static unsigned char ixland_map_dtype(unsigned char dtype) {
    switch (dtype) {
    case DT_FIFO:
        return IXLAND_DT_FIFO;
    case DT_CHR:
        return IXLAND_DT_CHR;
    case DT_DIR:
        return IXLAND_DT_DIR;
    case DT_BLK:
        return IXLAND_DT_BLK;
    case DT_REG:
        return IXLAND_DT_REG;
    case DT_LNK:
        return IXLAND_DT_LNK;
    case DT_SOCK:
        return IXLAND_DT_SOCK;
#ifdef DT_WHT
    case DT_WHT:
        return IXLAND_DT_WHT;
#endif
    default:
        return IXLAND_DT_UNKNOWN;
    }
}

ssize_t __ixland_getdents64_impl(int fd, void *dirp, size_t count) {
    if (dirp == NULL) {
        errno = EFAULT;
        return -1;
    }

    if (count == 0) {
        errno = EINVAL;
        return -1;
    }

    void *entry = __ixland_get_fd_entry_impl(fd);
    if (entry == NULL) {
        errno = EBADF;
        return -1;
    }

    int real_fd = __ixland_get_real_fd_impl(entry);
    off_t saved_offset = __ixland_get_fd_offset_impl(entry);

    int dup_fd = dup(real_fd);
    if (dup_fd < 0) {
        __ixland_put_fd_entry_impl(entry);
        return -1;
    }

    DIR *dp = fdopendir(dup_fd);
    if (dp == NULL) {
        int saved_errno = errno;
        close(dup_fd);
        __ixland_put_fd_entry_impl(entry);
        errno = saved_errno;
        return -1;
    }

    if (saved_offset > 0) {
        seekdir(dp, saved_offset);
    }

    size_t written = 0;
    off_t latest_offset = saved_offset;
    errno = 0;

    while (true) {
        struct dirent *native = readdir(dp);
        if (native == NULL) {
            if (errno != 0 && written == 0) {
                int saved_errno = errno;
                closedir(dp);
                __ixland_put_fd_entry_impl(entry);
                errno = saved_errno;
                return -1;
            }
            break;
        }

        size_t name_len = strlen(native->d_name);
        size_t base_len = offsetof(struct ixland_dirent_64, d_name);
        size_t record_len = base_len + name_len + 1;
        size_t aligned_len = (record_len + 7U) & ~7U;

        if (aligned_len > count - written) {
            if (written == 0) {
                closedir(dp);
                __ixland_put_fd_entry_impl(entry);
                errno = EINVAL;
                return -1;
            }
            break;
        }

        struct ixland_dirent_64 *out = (struct ixland_dirent_64 *)((char *)dirp + written);
        out->d_ino = native->d_ino;
        latest_offset = (off_t)telldir(dp);
        out->d_off = latest_offset;
        out->d_reclen = (unsigned short)aligned_len;
        out->d_type = ixland_map_dtype(native->d_type);
        memcpy(out->d_name, native->d_name, name_len + 1);

        if (aligned_len > record_len) {
            memset(((char *)out) + record_len, 0, aligned_len - record_len);
        }

        written += aligned_len;
    }

    __ixland_set_fd_offset_impl(entry, latest_offset);
    closedir(dp);
    __ixland_put_fd_entry_impl(entry);
    return (ssize_t)written;
}

ssize_t __ixland_getdents_impl(int fd, void *dirp, size_t count) {
    return __ixland_getdents64_impl(fd, dirp, count);
}

ssize_t ixland_getdents64(int fd, void *dirp, size_t count) {
    return __ixland_getdents64_impl(fd, dirp, count);
}

ssize_t ixland_getdents(int fd, void *dirp, size_t count) {
    return __ixland_getdents_impl(fd, dirp, count);
}
