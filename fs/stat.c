/* IXLandSystem/fs/stat.c
 * Virtual stat/fstat implementation
 */
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "fdtable.h"
#include "vfs.h"

#ifndef MAX_PATH
#define MAX_PATH 4096
#endif

int stat_impl(const char *pathname, struct stat *statbuf) {
    if (!pathname || !statbuf) {
        errno = EFAULT;
        return -1;
    }

    if (stat(pathname, statbuf) == 0) {
        return 0;
    }

    if (errno != ENOENT) {
        return -1;
    }

    return vfs_stat_path(pathname, statbuf);
}

int fstat_impl(int fd, struct stat *statbuf) {
    if (!statbuf) {
        errno = EFAULT;
        return -1;
    }

    if (fstat(fd, statbuf) == 0) {
        return 0;
    }

    if (fd < 0 || fd >= NR_OPEN_DEFAULT) {
        errno = EBADF;
        return -1;
    }

    if (fd <= 2) {
        return fstat(fd, statbuf);
    }

    void *entry = get_fd_entry_impl(fd);
    if (!entry) {
        errno = EBADF;
        return -1;
    }

    int real_fd = get_real_fd_impl(entry);
    int result = fstat(real_fd, statbuf);
    put_fd_entry_impl(entry);
    return result;
}

int lstat_impl(const char *pathname, struct stat *statbuf) {
    if (!pathname || !statbuf) {
        errno = EFAULT;
        return -1;
    }
    return vfs_lstat(pathname, statbuf);
}

int access_impl(const char *pathname, int mode) {
    if (!pathname) {
        errno = EFAULT;
        return -1;
    }
    return vfs_access(pathname, mode);
}

int fstatat_impl(int dirfd, const char *pathname, struct stat *statbuf, int flags) {
    int ret;

    if (!pathname || !statbuf) {
        errno = EFAULT;
        return -1;
    }

    ret = vfs_fstatat(dirfd, pathname, statbuf, flags);
    if (ret != 0) {
        errno = -ret;
        return -1;
    }
    return 0;
}

int faccessat_impl(int dirfd, const char *pathname, int mode, int flags) {
    int ret;

    if (!pathname) {
        errno = EFAULT;
        return -1;
    }

    ret = vfs_faccessat(dirfd, pathname, mode, flags);
    if (ret != 0) {
        errno = -ret;
        return -1;
    }
    return 0;
}

__attribute__((visibility("default"))) int stat(const char *pathname, struct stat *statbuf) {
    return stat_impl(pathname, statbuf);
}

__attribute__((visibility("default"))) int fstat(int fd, struct stat *statbuf) {
    return fstat_impl(fd, statbuf);
}

__attribute__((visibility("default"))) int lstat(const char *pathname, struct stat *statbuf) {
    return lstat_impl(pathname, statbuf);
}

__attribute__((visibility("default"))) int access(const char *pathname, int mode) {
    return access_impl(pathname, mode);
}

__attribute__((visibility("default"))) int faccessat(int dirfd, const char *pathname, int mode, int flags) {
    return faccessat_impl(dirfd, pathname, mode, flags);
}

__attribute__((visibility("default"))) int fstatat(int dirfd, const char *pathname, struct stat *statbuf, int flags) {
    return fstatat_impl(dirfd, pathname, statbuf, flags);
}

__attribute__((visibility("default"))) int newfstatat(int dirfd, const char *pathname, struct stat *statbuf, int flags) {
    return fstatat_impl(dirfd, pathname, statbuf, flags);
}
