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

#include "path_host.h"

/* Host stat operations */
int host_stat_impl(const char *path, struct stat *statbuf)
{
    return stat(path, statbuf);
}

int host_lstat_impl(const char *path, struct stat *statbuf)
{
    return lstat(path, statbuf);
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
