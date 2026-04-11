/* iXland - Mount Operations
 *
 * Canonical owner for mount syscalls:
 * - mount(), umount(), umount2()
 * - statfs(), fstatfs()
 * - sysfs(), statmount() (Linux-specific)
 *
 * Linux-shaped canonical owner - iOS mediation as implementation detail
 * Note: mount/umount are restricted on iOS sandbox
 */

#include <errno.h>
#include <fcntl.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/statvfs.h>
#include <sys/ucred.h>
#include <unistd.h>

#include "../internal/ixland_internal.h"

/* ============================================================================
 * MOUNT - Mount filesystem (restricted on iOS)
 * ============================================================================ */

int ixland_mount(const char *source, const char *target, const char *filesystemtype,
                 unsigned long mountflags, const void *data) {
    /* iOS restriction: mounting not allowed in sandbox */
    (void)source;
    (void)target;
    (void)filesystemtype;
    (void)mountflags;
    (void)data;
    errno = EPERM;
    return -1;
}

int ixland_umount(const char *target) {
    /* iOS restriction: unmounting not allowed */
    (void)target;
    errno = EPERM;
    return -1;
}

int ixland_umount2(const char *target, int flags) {
    /* iOS restriction: unmounting not allowed */
    (void)target;
    (void)flags;
    errno = EPERM;
    return -1;
}

/* ============================================================================
 * STATFS - Filesystem statistics
 * ============================================================================ */

int ixland_statfs(const char *path, struct statfs *buf) {
    return statfs(path, buf);
}

int ixland_fstatfs(int fd, struct statfs *buf) {
    return fstatfs(fd, buf);
}

int ixland_statvfs(const char *path, struct statvfs *buf) {
    return statvfs(path, buf);
}

int ixland_fstatvfs(int fd, struct statvfs *buf) {
    return fstatvfs(fd, buf);
}
