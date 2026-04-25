/* internal/ios/fs/path_host.c
 * Host path operations bridge implementation
 *
 * This file contains the host-specific implementations for path operations.
 * All Darwin host calls are isolated here, providing a narrow seam for
 * Linux-owner code in fs/namei.c
 */

#include <sys/stat.h>
#include <sys/stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "path_host.h"

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

/* Host stat operations */
int host_stat_impl(const char *path, struct linux_stat *statbuf)
{
    struct stat darwin_stat;
    int ret = stat(path, &darwin_stat);
    if (ret == 0) {
        translate_stat_to_linux(&darwin_stat, statbuf);
    }
    return ret;
}

int host_lstat_impl(const char *path, struct linux_stat *statbuf)
{
    struct stat darwin_stat;
    int ret = lstat(path, &darwin_stat);
    if (ret == 0) {
        translate_stat_to_linux(&darwin_stat, statbuf);
    }
    return ret;
}

int host_access_impl(const char *path, int mode)
{
    return access(path, mode);
}

/* Host rename operation (Darwin renameatx_np) */
int host_renameatx_np_impl(int fromfd, const char *from, int tofd, const char *to, unsigned int flags)
{
    return renameatx_np(fromfd, from, tofd, to, flags);
}

/* Directory operations */
int host_mkdir_impl(const char *pathname, uint32_t mode)
{
    return mkdir(pathname, (mode_t)mode);
}

int host_rmdir_impl(const char *pathname)
{
    return rmdir(pathname);
}

/* File operations */
int host_unlink_impl(const char *pathname)
{
    return unlink(pathname);
}

int host_link_impl(const char *oldpath, const char *newpath)
{
    return link(oldpath, newpath);
}

int host_symlink_impl(const char *target, const char *linkpath)
{
    return symlink(target, linkpath);
}

ssize_t host_readlink_impl(const char *pathname, char *buf, size_t bufsiz)
{
    return readlink(pathname, buf, bufsiz);
}

/* Fchdir */
int host_fchdir_impl(int fd)
{
    return fchdir(fd);
}
