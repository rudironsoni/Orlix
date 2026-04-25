/* IXLandSystem/fs/stat.c
 * Virtual stat/fstat implementation
 */
#include <linux/fcntl.h>

#include <errno.h>
#include <string.h>
#include <sys/stat.h>

#include "fdtable.h"
#include "internal/ios/fs/sync.h"
#include "internal/ios/fs/file_io_host.h"
#include "vfs.h"

#ifndef MAX_PATH
#define MAX_PATH 4096
#endif

int stat_impl(const char *pathname, struct stat *statbuf) {
    int ret;

    if (!pathname || !statbuf) {
        errno = EFAULT;
        return -1;
    }

    if (vfs_path_is_linux_route(pathname)) {
        ret = vfs_fstatat(AT_FDCWD, pathname, statbuf, 0);
        if (ret != 0) {
            errno = -ret;
            return -1;
        }
        return 0;
    }

    return host_stat_impl(pathname, statbuf);
}

int fstat_impl(int fd, struct stat *statbuf) {
    if (!statbuf) {
        errno = EFAULT;
        return -1;
    }

    if (host_fstat_impl(fd, statbuf) == 0) {
        return 0;
    }

    if (fd < 0 || fd >= NR_OPEN_DEFAULT) {
        errno = EBADF;
        return -1;
    }

    if (fd <= 2) {
        return host_fstat_impl(fd, statbuf);
    }

    void *entry = get_fd_entry_impl(fd);
    if (!entry) {
        errno = EBADF;
        return -1;
    }

    int real_fd = get_real_fd_impl(entry);
    int result = host_fstat_impl(real_fd, statbuf);
    put_fd_entry_impl(entry);
    return result;
}

int lstat_impl(const char *pathname, struct stat *statbuf) {
    int ret;

    if (!pathname || !statbuf) {
        errno = EFAULT;
        return -1;
    }

    if (vfs_path_is_linux_route(pathname)) {
        ret = vfs_fstatat(AT_FDCWD, pathname, statbuf, AT_SYMLINK_NOFOLLOW);
        if (ret != 0) {
            errno = -ret;
            return -1;
        }
        return 0;
    }

    ret = vfs_lstat(pathname, statbuf);
    if (ret != 0) {
        errno = -ret;
        return -1;
    }
    return 0;
}

int access_impl(const char *pathname, int mode) {
    int ret;

    if (!pathname) {
        errno = EFAULT;
        return -1;
    }

    ret = vfs_access(pathname, mode);
    if (ret != 0) {
        errno = -ret;
        return -1;
    }
    return 0;
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
