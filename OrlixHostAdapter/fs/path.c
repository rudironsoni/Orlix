/* OrlixHostAdapter/fs/path.c
 * Backing path operations implementation
 *
 * This file contains the host-specific implementations for path operations.
 * All Darwin host calls are isolated here, providing a narrow seam for
 * Linux-owner code in fs/namei.c
 */

#include "backing_path.h"
#include "backing_stat_translate.h"

/* Darwin headers - these define S_IFMT, S_ISDIR, etc. which are compatible
 * with Linux ABI values, so we use them directly in bridge code */
#include <sys/stat.h>
#include <sys/stdio.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>

#include "errno_translation.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"


static void capture_backing_stat(const struct stat *source, struct backing_stat_data *target) {
    target->dev = source->st_dev;
    target->ino = source->st_ino;
    target->mode = source->st_mode;
    target->nlink = source->st_nlink;
    target->uid = source->st_uid;
    target->gid = source->st_gid;
    target->rdev = source->st_rdev;
    target->size = source->st_size;
    target->blksize = source->st_blksize;
    target->blocks = source->st_blocks;
    target->atime_sec = source->st_atimespec.tv_sec;
    target->atime_nsec = (uint64_t)source->st_atimespec.tv_nsec;
    target->mtime_sec = source->st_mtimespec.tv_sec;
    target->mtime_nsec = (uint64_t)source->st_mtimespec.tv_nsec;
    target->ctime_sec = source->st_ctimespec.tv_sec;
    target->ctime_nsec = (uint64_t)source->st_ctimespec.tv_nsec;
}

/* Backing stat operations return Linux-shaped negative errno on failure. */
int backing_stat(const char *path, struct stat *statbuf)
{
    struct stat darwin_stat;
    struct backing_stat_data data;
    int ret = (int)syscall(SYS_stat64, path, &darwin_stat);
    if (ret == 0) {
        capture_backing_stat(&darwin_stat, &data);
        backing_stat_translate(&data, statbuf);
        return 0;
    }
    return -linux_errno_from_darwin_errno(errno);
}

int backing_lstat(const char *path, struct stat *statbuf)
{
    struct stat darwin_stat;
    struct backing_stat_data data;
    int ret = (int)syscall(SYS_lstat64, path, &darwin_stat);
    if (ret == 0) {
        capture_backing_stat(&darwin_stat, &data);
        backing_stat_translate(&data, statbuf);
        return 0;
    }
    return -linux_errno_from_darwin_errno(errno);
}

int backing_access(const char *path, int mode)
{
    int ret = (int)syscall(SYS_access, path, mode);
    if (ret == 0) {
        return 0;
    }
    return -linux_errno_from_darwin_errno(errno);
}

int backing_directory_is_empty(const char *path)
{
    DIR *dir = opendir(path);
    struct dirent *entry;

    if (!dir) {
        return -linux_errno_from_darwin_errno(errno);
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            closedir(dir);
            return 0;
        }
    }

    closedir(dir);
    return 1;
}

/* Host rename operation (Darwin renameatx_np) */
int backing_rename_with_flags(int fromfd, const char *from, int tofd, const char *to, unsigned int flags)
{
    return renameatx_np(fromfd, from, tofd, to, flags);
}

int backing_rename_exchange(const char *from, const char *to)
{
    return renameatx_np(AT_FDCWD, from, AT_FDCWD, to, RENAME_SWAP);
}

/* Directory operations */
int backing_mkdir(const char *pathname, uint32_t mode)
{
    return (int)syscall(SYS_mkdir, pathname, (mode_t)mode);
}

int backing_rmdir(const char *pathname)
{
    return (int)syscall(SYS_rmdir, pathname);
}

/* File operations */
int backing_unlink(const char *pathname)
{
    return (int)syscall(SYS_unlink, pathname);
}

int backing_link(const char *oldpath, const char *newpath)
{
    return (int)syscall(SYS_link, oldpath, newpath);
}

int backing_linkat(const char *oldpath, const char *newpath, int follow_symlink)
{
    return (int)syscall(SYS_linkat, AT_FDCWD, oldpath, AT_FDCWD, newpath,
                        follow_symlink ? AT_SYMLINK_FOLLOW : 0);
}

int backing_symlink(const char *target, const char *linkpath)
{
    return (int)syscall(SYS_symlink, target, linkpath);
}

long backing_readlink(const char *pathname, char *buf, size_t bufsiz)
{
    return (long)syscall(SYS_readlink, pathname, buf, bufsiz);
}

/* Fchdir */
int backing_fchdir(int fd)
{
    return (int)syscall(SYS_fchdir, fd);
}

#pragma clang diagnostic pop
