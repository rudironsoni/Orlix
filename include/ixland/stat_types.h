/* include/ixland/stat_types.h
 * Linux-shaped stat types for IXLandSystem
 */

#ifndef IXLAND_STAT_TYPES_H
#define IXLAND_STAT_TYPES_H

/* Linux stat structure - compatible with UAPI layout */
struct linux_stat {
    unsigned long long st_dev;
    unsigned long long st_ino;
    unsigned int st_mode;
    unsigned int st_nlink;
    unsigned int st_uid;
    unsigned int st_gid;
    unsigned long long st_rdev;
    unsigned long long __pad1;
    long long st_size;
    int st_blksize;
    int __pad2;
    long long st_blocks;
    /* Use separate typedefs for time fields to avoid any macro conflicts */
    long long st_atime_sec;
    unsigned long long st_atime_nsec;
    long long st_mtime_sec;
    unsigned long long st_mtime_nsec;
    long long st_ctime_sec;
    unsigned long long st_ctime_nsec;
    unsigned int __unused4;
    unsigned int __unused5;
};

/* File type macros */
#define LINUX_S_IFMT    00170000
#define LINUX_S_IFSOCK  0140000
#define LINUX_S_IFLNK   0120000
#define LINUX_S_IFREG   0100000
#define LINUX_S_IFBLK   0060000
#define LINUX_S_IFDIR   0040000
#define LINUX_S_IFCHR   0020000
#define LINUX_S_IFIFO   0010000

#define LINUX_S_ISLNK(m)  (((m) & LINUX_S_IFMT) == LINUX_S_IFLNK)
#define LINUX_S_ISREG(m)  (((m) & LINUX_S_IFMT) == LINUX_S_IFREG)
#define LINUX_S_ISDIR(m)  (((m) & LINUX_S_IFMT) == LINUX_S_IFDIR)
#define LINUX_S_ISCHR(m)  (((m) & LINUX_S_IFMT) == LINUX_S_IFCHR)
#define LINUX_S_ISBLK(m)  (((m) & LINUX_S_IFMT) == LINUX_S_IFBLK)
#define LINUX_S_ISFIFO(m) (((m) & LINUX_S_IFMT) == LINUX_S_IFIFO)
#define LINUX_S_ISSOCK(m) (((m) & LINUX_S_IFMT) == LINUX_S_IFSOCK)

#endif /* IXLAND_STAT_TYPES_H */
