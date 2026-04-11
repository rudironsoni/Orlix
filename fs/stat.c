#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../src/ixland/internal/ixland_internal.h"

int __ixland_stat_impl(const char *pathname, struct stat *statbuf) {
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

    return ixland_vfs_stat(pathname, statbuf);
}

int __ixland_fstat_impl(int fd, struct stat *statbuf) {
    if (!statbuf) {
        errno = EFAULT;
        return -1;
    }

    if (fstat(fd, statbuf) == 0) {
        return 0;
    }

    if (fd < 0 || fd >= IXLAND_MAX_FD) {
        errno = EBADF;
        return -1;
    }

    if (fd <= 2) {
        return fstat(fd, statbuf);
    }

    void *entry = __ixland_get_fd_entry_impl(fd);
    if (!entry) {
        errno = EBADF;
        return -1;
    }

    int result = fstat(__ixland_get_real_fd_impl(entry), statbuf);
    __ixland_put_fd_entry_impl(entry);
    return result;
}

int __ixland_lstat_impl(const char *pathname, struct stat *statbuf) {
    if (!pathname || !statbuf) {
        errno = EFAULT;
        return -1;
    }
    return ixland_vfs_lstat(pathname, statbuf);
}

int __ixland_access_impl(const char *pathname, int mode) {
    if (!pathname) {
        errno = EFAULT;
        return -1;
    }
    return ixland_vfs_access(pathname, mode);
}

int __ixland_faccessat_impl(int dirfd, const char *pathname, int mode, int flags) {
    (void)dirfd;
    (void)flags;
    if (!pathname) {
        errno = EFAULT;
        return -1;
    }
    return ixland_vfs_access(pathname, mode);
}

int ixland_stat(const char *pathname, struct stat *statbuf) {
    return __ixland_stat_impl(pathname, statbuf);
}

int ixland_fstat(int fd, struct stat *statbuf) {
    return __ixland_fstat_impl(fd, statbuf);
}

int ixland_lstat(const char *pathname, struct stat *statbuf) {
    return __ixland_lstat_impl(pathname, statbuf);
}

int ixland_access(const char *pathname, int mode) {
    return __ixland_access_impl(pathname, mode);
}

int ixland_faccessat(int dirfd, const char *pathname, int mode, int flags) {
    return __ixland_faccessat_impl(dirfd, pathname, mode, flags);
}
