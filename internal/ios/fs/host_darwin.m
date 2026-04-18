#include "host_darwin.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <unistd.h>

/* Private host mediation via direct syscalls
 *
 * This file provides non-interposing access to host Darwin syscalls.
 * We use the syscall() interface with SYS_* constants to call host
 * operations without recursion through IXLand's exported wrappers.
 *
 * Note: syscall() is deprecated on Darwin but remains functional.
 * This is acceptable for a virtualization layer that needs to bypass
 * its own wrappers to reach the host kernel.
 */

/* Suppress deprecation warnings for intentional syscall() usage */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

int host_open_impl(const char *path, int flags, mode_t mode) {
    int ret = syscall(SYS_open_nocancel, path, flags, mode);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return ret;
}

int host_close_impl(int fd) {
    int ret = syscall(SYS_close_nocancel, fd);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return ret;
}

int host_dup_impl(int fd) {
    int ret = syscall(SYS_dup, fd);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return ret;
}

int host_stat_impl(const char *path, struct stat *statbuf) {
    int ret = syscall(SYS_stat, path, statbuf);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return ret;
}

int host_lstat_impl(const char *path, struct stat *statbuf) {
    int ret = syscall(SYS_lstat, path, statbuf);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return ret;
}

int host_access_impl(const char *path, int mode) {
    int ret = syscall(SYS_access, path, mode);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return ret;
}

int host_fstat_impl(int fd, struct stat *statbuf) {
    int ret = syscall(SYS_fstat, fd, statbuf);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return ret;
}

ssize_t host_read_impl(int fd, void *buf, size_t count) {
    ssize_t ret = syscall(SYS_read_nocancel, fd, buf, count);
    if (ret < 0) {
        errno = (int)-ret;
        return -1;
    }
    return ret;
}

ssize_t host_write_impl(int fd, const void *buf, size_t count) {
    ssize_t ret = syscall(SYS_write_nocancel, fd, buf, count);
    if (ret < 0) {
        errno = (int)-ret;
        return -1;
    }
    return ret;
}

off_t host_lseek_impl(int fd, off_t offset, int whence) {
    off_t ret = syscall(SYS_lseek, fd, offset, whence);
    if (ret < 0) {
        errno = (int)-ret;
        return -1;
    }
    return ret;
}

ssize_t host_pread_impl(int fd, void *buf, size_t count, off_t offset) {
    ssize_t ret = syscall(SYS_pread_nocancel, fd, buf, count, offset);
    if (ret < 0) {
        errno = (int)-ret;
        return -1;
    }
    return ret;
}

ssize_t host_pwrite_impl(int fd, const void *buf, size_t count, off_t offset) {
    ssize_t ret = syscall(SYS_pwrite_nocancel, fd, buf, count, offset);
    if (ret < 0) {
        errno = (int)-ret;
        return -1;
    }
    return ret;
}

ssize_t host_readv_impl(int fd, const struct iovec *iov, int iovcnt) {
    ssize_t ret = syscall(SYS_readv_nocancel, fd, iov, iovcnt);
    if (ret < 0) {
        errno = (int)-ret;
        return -1;
    }
    return ret;
}

ssize_t host_writev_impl(int fd, const struct iovec *iov, int iovcnt) {
    ssize_t ret = syscall(SYS_writev_nocancel, fd, iov, iovcnt);
    if (ret < 0) {
        errno = (int)-ret;
        return -1;
    }
    return ret;
}

/* Storage class discovery - Darwin-specific host mediation
 * These use Foundation APIs to query standard iOS container paths.
 * This is the appropriate level for host container awareness.
 * Returns 0 on success, -1 on error.
 */
#ifndef MAX_PATH
#define MAX_PATH 4096
#endif
#include <Foundation/Foundation.h>

int host_get_application_support_path_impl(char *path, size_t path_len) {
    @autoreleasepool {
        NSURL *url = [[NSFileManager defaultManager] 
                      URLForDirectory:NSApplicationSupportDirectory
                      inDomain:NSUserDomainMask
                      appropriateForURL:nil
                      create:YES
                      error:nil];
        if (!url) {
            errno = ENOENT;
            return -1;
        }
        const char *cpath = [url fileSystemRepresentation];
        if (!cpath || strlen(cpath) >= path_len) {
            errno = ENAMETOOLONG;
            return -1;
        }
        strncpy(path, cpath, path_len - 1);
        path[path_len - 1] = '\0';
        return 0;
    }
}

int host_get_caches_path_impl(char *path, size_t path_len) {
    @autoreleasepool {
        NSURL *url = [[NSFileManager defaultManager] 
                      URLForDirectory:NSCachesDirectory
                      inDomain:NSUserDomainMask
                      appropriateForURL:nil
                      create:YES
                      error:nil];
        if (!url) {
            errno = ENOENT;
            return -1;
        }
        const char *cpath = [url fileSystemRepresentation];
        if (!cpath || strlen(cpath) >= path_len) {
            errno = ENAMETOOLONG;
            return -1;
        }
        strncpy(path, cpath, path_len - 1);
        path[path_len - 1] = '\0';
        return 0;
    }
}

int host_get_tmp_path_impl(char *path, size_t path_len) {
    @autoreleasepool {
        NSString *tmp = NSTemporaryDirectory();
        if (!tmp) {
            errno = ENOENT;
            return -1;
        }
        const char *cpath = [tmp UTF8String];
        if (!cpath || strlen(cpath) >= path_len) {
            errno = ENAMETOOLONG;
            return -1;
        }
        strncpy(path, cpath, path_len - 1);
        path[path_len - 1] = '\0';
        return 0;
    }
}

int host_ensure_directory_impl(const char *path, mode_t mode) {
    struct stat st;
    if (stat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            return 0;
        }
        errno = ENOTDIR;
        return -1;
    }
    if (errno != ENOENT) {
        return -1;
    }
    /* Create parent directories recursively */
    char parent[MAX_PATH];
    size_t len = strlen(path);
    if (len >= sizeof(parent)) {
        errno = ENAMETOOLONG;
        return -1;
    }
    memcpy(parent, path, len + 1);
    
    /* Find the last slash */
    char *last_slash = strrchr(parent, '/');
    if (last_slash && last_slash != parent) {
        *last_slash = '\0';
        if (host_ensure_directory_impl(parent, mode) < 0) {
            return -1;
        }
    }
    
    /* Create this directory */
    int ret = syscall(SYS_mkdir, path, mode);
    if (ret < 0 && errno != EEXIST) {
        return -1;
    }
    return 0;
}

#pragma clang diagnostic pop
