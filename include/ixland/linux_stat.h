/* include/ixland/linux_stat.h
 * Linux-shaped stat interface for IXLandSystem
 *
 * This header provides Linux ABI-compatible stat structure and function declarations.
 * ABI constants are sourced from vendored Linux UAPI headers.
 */

#ifndef IXLAND_LINUX_STAT_H
#define IXLAND_LINUX_STAT_H

/* Include vendored Linux UAPI stat constants */
#include <linux/stat.h>

/* Linux stat structure - compatible with Linux UAPI layout.
 * Field order and sizes match Linux 6.12 arm64 stat struct.
 * This is IXLand's Linux-facing stat contract. */
struct linux_stat {
    unsigned long st_dev;
    unsigned long st_ino;
    unsigned int st_mode;
    unsigned int st_nlink;
    unsigned int st_uid;
    unsigned int st_gid;
    unsigned long st_rdev;
    unsigned long __pad1;
    long st_size;
    int st_blksize;
    int __pad2;
    long st_blocks;
    long st_atime_sec;
    unsigned long st_atime_nsec;
    long st_mtime_sec;
    unsigned long st_mtime_nsec;
    long st_ctime_sec;
    unsigned long st_ctime_nsec;
    unsigned int __unused4;
    unsigned int __unused5;
};

#ifdef __cplusplus
extern "C" {
#endif

/* Public IXLand stat functions - Linux ABI shaped */
int stat(const char *pathname, struct linux_stat *statbuf);
int lstat(const char *pathname, struct linux_stat *statbuf);
int fstat(int fd, struct linux_stat *statbuf);
int fstatat(int dirfd, const char *pathname, struct linux_stat *statbuf, int flags);

/* Internal stat implementations */
int stat_impl(const char *pathname, struct linux_stat *statbuf);
int lstat_impl(const char *pathname, struct linux_stat *statbuf);
int fstat_impl(int fd, struct linux_stat *statbuf);
int fstatat_impl(int dirfd, const char *pathname, struct linux_stat *statbuf, int flags);
int newfstatat(int dirfd, const char *pathname, struct linux_stat *statbuf, int flags);

#ifdef __cplusplus
}
#endif

#endif /* IXLAND_LINUX_STAT_H */
