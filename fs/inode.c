/* iXland - Inode Operations
 *
 * Canonical owner for inode-level operations:
 * - chmod(), fchmod(), fchmodat()
 * - chown(), fchown(), lchown(), fchownat()
 * - access(), faccessat()
 * - umask()
 * - truncate(), ftruncate()
 *
 * Linux-shaped canonical owner - iOS mediation as implementation detail
 */

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../src/ixland/internal/ixland_internal.h"

/* ============================================================================
 * CHMOD - Change file mode
 * ============================================================================ */

int ixland_chmod(const char *pathname, mode_t mode) {
    return chmod(pathname, mode);
}

int ixland_fchmod(int fd, mode_t mode) {
    return fchmod(fd, mode);
}

int ixland_fchmodat(int dirfd, const char *pathname, mode_t mode, int flags) {
    return fchmodat(dirfd, pathname, mode, flags);
}

/* ============================================================================
 * CHOWN - Change file owner
 * ============================================================================ */

int ixland_chown(const char *pathname, uid_t owner, gid_t group) {
    /* iOS restriction: changing ownership not allowed */
    (void)pathname;
    (void)owner;
    (void)group;
    errno = EPERM;
    return -1;
}

int ixland_fchown(int fd, uid_t owner, gid_t group) {
    (void)fd;
    (void)owner;
    (void)group;
    errno = EPERM;
    return -1;
}

int ixland_lchown(const char *pathname, uid_t owner, gid_t group) {
    return ixland_chown(pathname, owner, group);
}

int ixland_fchownat(int dirfd, const char *pathname, uid_t owner, gid_t group, int flags) {
    (void)dirfd;
    (void)pathname;
    (void)owner;
    (void)group;
    (void)flags;
    errno = EPERM;
    return -1;
}

/* ============================================================================
 * ACCESS - Check file accessibility
 * ============================================================================ */

int ixland_access(const char *pathname, int mode) {
    return access(pathname, mode);
}

int ixland_faccessat(int dirfd, const char *pathname, int mode, int flags) {
    return faccessat(dirfd, pathname, mode, flags);
}

/* ============================================================================
 * UMASK - Set file creation mask
 * ============================================================================ */

mode_t ixland_umask(mode_t mask) {
    return umask(mask);
}

/* ============================================================================
 * TRUNCATE - Change file size
 * ============================================================================ */

int ixland_truncate(const char *path, off_t length) {
    return truncate(path, length);
}

int ixland_ftruncate(int fd, off_t length) {
    return ftruncate(fd, length);
}
