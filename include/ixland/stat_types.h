/* include/ixland/stat_types.h
 * Linux-compatible stat structure definition
 *
 * This header provides ONLY the struct linux_stat definition.
 * This is a Linux-facing public ABI type used by IXLand's stat-family functions.
 * No system headers. No macros. Safe for both Linux-owner and bridge code.
 */

#ifndef IXLAND_STAT_TYPES_H
#define IXLAND_STAT_TYPES_H

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

#endif /* IXLAND_STAT_TYPES_H */
