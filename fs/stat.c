#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

#include "fdtable.h"
#include "vfs.h"

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

    return vfs_stat(pathname, statbuf);
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

    void *entry = __ixland_get_fd_entry_impl(fd);
    if (!entry) {
        errno = EBADF;
        return -1;
    }

    int result = fstat(__ixland_get_real_fd_impl(entry), statbuf);
    __ixland_put_fd_entry_impl(entry);
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

int faccessat_impl(int dirfd, const char *pathname, int mode, int flags) {
    (void)dirfd;
    (void)flags;
    if (!pathname) {
        errno = EFAULT;
        return -1;
    }
    return vfs_access(pathname, mode);
}

int ixland_stat(const char *pathname, struct stat *statbuf) {
    return stat_impl(pathname, statbuf);
}

int ixland_fstat(int fd, struct stat *statbuf) {
    return fstat_impl(fd, statbuf);
}

int ixland_lstat(const char *pathname, struct stat *statbuf) {
    return lstat_impl(pathname, statbuf);
}

int ixland_access(const char *pathname, int mode) {
    return access_impl(pathname, mode);
}

int ixland_faccessat(int dirfd, const char *pathname, int mode, int flags) {
    return faccessat_impl(dirfd, pathname, mode, flags);
}
