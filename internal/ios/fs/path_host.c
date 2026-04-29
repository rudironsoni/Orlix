/* internal/ios/fs/path_host.c
 * Host path operations bridge implementation
 *
 * This file contains the host-specific implementations for path operations.
 * All Darwin host calls are isolated here, providing a narrow seam for
 * Linux-owner code in fs/namei.c
 */

/* Include shared stat type definition */
#include "include/ixland/stat_types.h"

#include "path_host.h"

/* Darwin headers - these define S_IFMT, S_ISDIR, etc. which are compatible
 * with Linux ABI values, so we use them directly in bridge code */
#include <sys/stat.h>
#include <sys/stdio.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

/* Linux errno definitions for mapping Darwin errno to Linux errno */
#define LINUX_EPERM        1
#define LINUX_ENOENT       2
#define LINUX_EIO          5
#define LINUX_EACCES      13
#define LINUX_EFAULT      14
#define LINUX_EEXIST      17
#define LINUX_ENODEV      19
#define LINUX_ENOTDIR     20
#define LINUX_EISDIR      21
#define LINUX_EINVAL      22
#define LINUX_ENFILE      23
#define LINUX_EMFILE      24
#define LINUX_ENOSPC      28
#define LINUX_ENAMETOOLONG 36
#define LINUX_ENOSYS      38
#define LINUX_ENOTEMPTY   39
#define LINUX_ELOOP       40
#define LINUX_EOPNOTSUPP  95

/* Map Darwin errno to Linux errno for VFS boundary */
static int map_darwin_errno_to_linux(int darwin_errno) {
    switch (darwin_errno) {
        case EPERM: return -LINUX_EPERM;
        case ENOENT: return -LINUX_ENOENT;
        case EIO: return -LINUX_EIO;
        case EACCES: return -LINUX_EACCES;
        case EFAULT: return -LINUX_EFAULT;
        case EEXIST: return -LINUX_EEXIST;
        case ENODEV: return -LINUX_ENODEV;
        case ENOTDIR: return -LINUX_ENOTDIR;
        case EISDIR: return -LINUX_EISDIR;
        case EINVAL: return -LINUX_EINVAL;
        case ENFILE: return -LINUX_ENFILE;
        case EMFILE: return -LINUX_EMFILE;
        case ENOSPC: return -LINUX_ENOSPC;
        case ENAMETOOLONG: return -LINUX_ENAMETOOLONG;
        case ENOSYS: return -LINUX_ENOSYS;
        case ENOTEMPTY: return -LINUX_ENOTEMPTY;
        case ELOOP: return -LINUX_ELOOP;
        case EOPNOTSUPP: return -LINUX_EOPNOTSUPP;
        default: return -LINUX_EIO;
    }
}

/* Translate Darwin struct stat to Linux struct linux_stat */
static void translate_stat_to_linux(const struct stat *darwin_stat, struct linux_stat *linux_stat)
{
    memset(linux_stat, 0, sizeof(*linux_stat));
    linux_stat->st_dev = darwin_stat->st_dev;
    linux_stat->st_ino = darwin_stat->st_ino;
    linux_stat->st_mode = darwin_stat->st_mode;
    linux_stat->st_nlink = darwin_stat->st_nlink;
    linux_stat->st_uid = darwin_stat->st_uid;
    linux_stat->st_gid = darwin_stat->st_gid;
    linux_stat->st_rdev = darwin_stat->st_rdev;
    linux_stat->st_size = darwin_stat->st_size;
    linux_stat->st_blksize = darwin_stat->st_blksize;
    linux_stat->st_blocks = darwin_stat->st_blocks;
    /* Darwin has timespec fields for timestamps */
    linux_stat->st_atime_sec = darwin_stat->st_atimespec.tv_sec;
    linux_stat->st_atime_nsec = (unsigned long long)darwin_stat->st_atimespec.tv_nsec;
    linux_stat->st_mtime_sec = darwin_stat->st_mtimespec.tv_sec;
    linux_stat->st_mtime_nsec = (unsigned long long)darwin_stat->st_mtimespec.tv_nsec;
    linux_stat->st_ctime_sec = darwin_stat->st_ctimespec.tv_sec;
    linux_stat->st_ctime_nsec = (unsigned long long)darwin_stat->st_ctimespec.tv_nsec;
}

/* Host stat operations - return Linux-shaped negative errno on failure */
int host_stat_impl(const char *path, struct linux_stat *statbuf)
{
    struct stat darwin_stat;
    int ret = (int)syscall(SYS_stat64, path, &darwin_stat);
    if (ret == 0) {
        translate_stat_to_linux(&darwin_stat, statbuf);
        return 0;
    }
    return map_darwin_errno_to_linux(errno);
}

int host_lstat_impl(const char *path, struct linux_stat *statbuf)
{
    struct stat darwin_stat;
    int ret = (int)syscall(SYS_lstat64, path, &darwin_stat);
    if (ret == 0) {
        translate_stat_to_linux(&darwin_stat, statbuf);
        return 0;
    }
    return map_darwin_errno_to_linux(errno);
}

int host_access_impl(const char *path, int mode)
{
    int ret = (int)syscall(SYS_access, path, mode);
    if (ret == 0) {
        return 0;
    }
    return map_darwin_errno_to_linux(errno);
}

/* Host rename operation (Darwin renameatx_np) */
int host_renameatx_np_impl(int fromfd, const char *from, int tofd, const char *to, unsigned int flags)
{
    return renameatx_np(fromfd, from, tofd, to, flags);
}

/* Directory operations */
int host_mkdir_impl(const char *pathname, uint32_t mode)
{
    return (int)syscall(SYS_mkdir, pathname, (mode_t)mode);
}

int host_rmdir_impl(const char *pathname)
{
    return (int)syscall(SYS_rmdir, pathname);
}

/* File operations */
int host_unlink_impl(const char *pathname)
{
    return (int)syscall(SYS_unlink, pathname);
}

int host_link_impl(const char *oldpath, const char *newpath)
{
    return (int)syscall(SYS_link, oldpath, newpath);
}

int host_symlink_impl(const char *target, const char *linkpath)
{
    return (int)syscall(SYS_symlink, target, linkpath);
}

ssize_t host_readlink_impl(const char *pathname, char *buf, size_t bufsiz)
{
    return (ssize_t)syscall(SYS_readlink, pathname, buf, bufsiz);
}

/* Fchdir */
int host_fchdir_impl(int fd)
{
    return (int)syscall(SYS_fchdir, fd);
}

#pragma clang diagnostic pop
