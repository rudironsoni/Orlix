/* include/ixland/stat_types.h
 * Linux-shaped stat types for IXLandSystem
 *
 * This header provides Linux UAPI-compatible stat structure that Linux-owner
 * code uses. The bridge layer (internal/ios/fs/path_host.c) translates
 * between this and Darwin's struct stat.
 */

#ifndef IXLAND_STAT_TYPES_H
#define IXLAND_STAT_TYPES_H

/* Freestanding - no system headers to avoid Darwin module contamination */
/* Use compiler builtins for fixed-width types */
typedef __UINT32_TYPE__ __ixland_uint32_t;
typedef __UINT64_TYPE__ __ixland_uint64_t;
typedef __INT32_TYPE__ __ixland_int32_t;
typedef __INT64_TYPE__ __ixland_int64_t;

/* Linux stat structure - compatible with UAPI layout
 * This is what Linux-owner code uses.
 */
struct linux_stat {
    unsigned long   st_dev;         /* Device */
    unsigned long   st_ino;         /* Inode */
    unsigned int    st_mode;        /* Protection */
    unsigned int    st_nlink;       /* Number of hard links */
    unsigned int    st_uid;         /* User ID of owner */
    unsigned int    st_gid;         /* Group ID of owner */
    unsigned long   st_rdev;        /* Device type (if inode device) */
    unsigned long   __pad1;
    long            st_size;        /* Total size, in bytes */
    int             st_blksize;     /* Block size for filesystem I/O */
    int             __pad2;
    long            st_blocks;      /* Number of 512-byte blocks allocated */
    long            st_atime;       /* Time of last access */
    unsigned long   st_atime_nsec;
    long            st_mtime;       /* Time of last modification */
    unsigned long   st_mtime_nsec;
    long            st_ctime;       /* Time of last status change */
    unsigned long   st_ctime_nsec;
    unsigned int    __unused4;
    unsigned int    __unused5;
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
