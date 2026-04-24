/* iXland - Inode Operations
 * Canonical owner for inode-level operations:
 * - chmod(), fchmod(), fchmodat()
 * - chown(), fchown(), lchown(), fchownat()
 * - umask()
 * - truncate(), ftruncate()
 *
 * Linux-shaped canonical owner - iOS mediation as implementation detail
 */

#include <errno.h>
#include <sys/stat.h>

#include "internal/ios/fs/sync.h"

/* ============================================================================
 * CHMOD - Change file mode
 * ============================================================================ */

static int chmod_impl(const char *pathname, mode_t mode) {
    return chmod(pathname, mode);
}

static int fchmod_impl(int fd, mode_t mode) {
    return fchmod(fd, mode);
}

static int fchmodat_impl(int dirfd, const char *pathname, mode_t mode, int flags) {
    return fchmodat(dirfd, pathname, mode, flags);
}

/* ============================================================================
 * CHOWN - Change file owner
 * ============================================================================ */

static int chown_impl(const char *pathname, uid_t owner, gid_t group) {
    /* iOS restriction: changing ownership not allowed */
    (void)pathname;
    (void)owner;
    (void)group;
    errno = EPERM;
    return -1;
}

static int fchown_impl(int fd, uid_t owner, gid_t group) {
    (void)fd;
    (void)owner;
    (void)group;
    errno = EPERM;
    return -1;
}

static int lchown_impl(const char *pathname, uid_t owner, gid_t group) {
    return chown_impl(pathname, owner, group);
}

static int fchownat_impl(int dirfd, const char *pathname, uid_t owner, gid_t group, int flags) {
    (void)dirfd;
    (void)pathname;
    (void)owner;
    (void)group;
    (void)flags;
    errno = EPERM;
    return -1;
}

/* ============================================================================
 * UMASK - Set file creation mask
 * ============================================================================ */

static mode_t umask_impl(mode_t mask) {
    return umask(mask);
}

/* ============================================================================
 * TRUNCATE - Change file size
 * ============================================================================ */

static int truncate_impl(const char *path, off_t length) {
    return host_truncate_impl(path, length);
}

static int ftruncate_impl(int fd, off_t length) {
    return host_ftruncate_impl(fd, length);
}

/* ============================================================================
 * Public Canonical Syscalls
 * ============================================================================ */

__attribute__((visibility("default"))) int chmod(const char *pathname, mode_t mode) {
    return chmod_impl(pathname, mode);
}

__attribute__((visibility("default"))) int fchmod(int fd, mode_t mode) {
    return fchmod_impl(fd, mode);
}

__attribute__((visibility("default"))) int fchmodat(int dirfd, const char *pathname, mode_t mode, int flags) {
    return fchmodat_impl(dirfd, pathname, mode, flags);
}

__attribute__((visibility("default"))) int chown(const char *pathname, uid_t owner, gid_t group) {
    return chown_impl(pathname, owner, group);
}

__attribute__((visibility("default"))) int fchown(int fd, uid_t owner, gid_t group) {
    return fchown_impl(fd, owner, group);
}

__attribute__((visibility("default"))) int lchown(const char *pathname, uid_t owner, gid_t group) {
    return lchown_impl(pathname, owner, group);
}

__attribute__((visibility("default"))) int fchownat(int dirfd, const char *pathname, uid_t owner, gid_t group, int flags) {
    return fchownat_impl(dirfd, pathname, owner, group, flags);
}

__attribute__((visibility("default"))) mode_t umask(mode_t mask) {
    return umask_impl(mask);
}

__attribute__((visibility("default"))) int truncate(const char *path, off_t length) {
    return truncate_impl(path, length);
}

__attribute__((visibility("default"))) int ftruncate(int fd, off_t length) {
    return ftruncate_impl(fd, length);
}
