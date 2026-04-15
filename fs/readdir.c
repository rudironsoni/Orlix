/* IXLandSystem/fs/readdir.c
 * Virtual getdents/getdents64 implementation
 */
#include <dirent.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "fdtable.h"
#include "path.h"

/* Linux dirent64 structure - matches Linux UAPI */
struct linux_dirent64 {
    uint64_t d_ino;
    int64_t  d_off;
    unsigned short d_reclen;
    unsigned char  d_type;
    char           d_name[];
};

static unsigned char map_dtype(unsigned char dtype) {
    switch (dtype) {
    case DT_FIFO:
        return DT_FIFO;
    case DT_CHR:
        return DT_CHR;
    case DT_DIR:
        return DT_DIR;
    case DT_BLK:
        return DT_BLK;
    case DT_REG:
        return DT_REG;
    case DT_LNK:
        return DT_LNK;
    case DT_SOCK:
        return DT_SOCK;
#ifdef DT_WHT
    case DT_WHT:
        return DT_WHT;
#endif
    default:
        return DT_UNKNOWN;
    }
}

ssize_t getdents64_impl(int fd, void *dirp, size_t count) {
    if (dirp == NULL) {
        errno = EFAULT;
        return -1;
    }

    if (count == 0) {
        errno = EINVAL;
        return -1;
    }

    void *entry = get_fd_entry_impl(fd);
    if (entry == NULL) {
        errno = EBADF;
        return -1;
    }

    int real_fd = get_real_fd_impl(entry);
    off_t saved_offset = get_fd_offset_impl(entry);

    int dup_fd = dup(real_fd);
    if (dup_fd < 0) {
        put_fd_entry_impl(entry);
        return -1;
    }

    DIR *dp = fdopendir(dup_fd);
    if (dp == NULL) {
        int saved_errno = errno;
        close(dup_fd);
        put_fd_entry_impl(entry);
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
                put_fd_entry_impl(entry);
                errno = saved_errno;
                return -1;
            }
            break;
        }

        size_t name_len = strlen(native->d_name);
        size_t base_len = sizeof(struct linux_dirent64);
        size_t record_len = base_len + name_len + 1;
        size_t aligned_len = (record_len + 7U) & ~7U;

        if (aligned_len > count - written) {
            if (written == 0) {
                closedir(dp);
                put_fd_entry_impl(entry);
                errno = EINVAL;
                return -1;
            }
            break;
        }

        struct linux_dirent64 *out = (struct linux_dirent64 *)((char *)dirp + written);
        out->d_ino = native->d_ino;
        latest_offset = (off_t)telldir(dp);
        out->d_off = latest_offset;
        out->d_reclen = (unsigned short)aligned_len;
        out->d_type = map_dtype(native->d_type);
        memcpy(out->d_name, native->d_name, name_len + 1);

        if (aligned_len > record_len) {
            memset(((char *)out) + record_len, 0, aligned_len - record_len);
        }

        written += aligned_len;
    }

    set_fd_offset_impl(entry, latest_offset);
    closedir(dp);
    put_fd_entry_impl(entry);
    return (ssize_t)written;
}

ssize_t getdents_impl(int fd, void *dirp, size_t count) {
    return getdents64_impl(fd, dirp, count);
}

__attribute__((visibility("default"))) ssize_t getdents(int fd, void *dirp, size_t count) {
    return getdents_impl(fd, dirp, count);
}

__attribute__((visibility("default"))) ssize_t getdents64(int fd, void *dirp, size_t count) {
    return getdents64_impl(fd, dirp, count);
}
