/* iXland - Superblock Operations
 *
 * Canonical owner for filesystem-level operations:
 * - sync(), fsync(), fdatasync()
 * - syncfs()
 * - statfs(), fstatfs()
 * - statvfs(), fstatvfs()
 * - posix_fadvise(), posix_fallocate()
 *
 * Linux-shaped canonical owner - iOS mediation as implementation detail
 */

#define _DARWIN_FEATURE_64_BIT_INODE 1
#include <errno.h>
#include <sys/param.h>
#include <sys/statvfs.h>

#include "internal/ios/fs/backing_io.h"

/* iOS system call stubs - private implementation detail */
extern int _statfs(const char *, struct statfs *);
extern int _fstatfs(int, struct statfs *);
extern int _statvfs(const char *, struct statvfs *);
extern int _fstatvfs(int, struct statvfs *);

/* ============================================================================
 * SYNC - Flush filesystem buffers
 * ============================================================================ */

static void sync_impl(void) {
    sync();
}

static int fsync_impl(int fd) {
    return fsync(fd);
}

static int fdatasync_impl(int fd) {
    /* fdatasync not available on iOS, use fsync or F_FULLFSYNC */
#ifdef F_FULLFSYNC
    return fcntl(fd, F_FULLFSYNC);
#else
    return fsync(fd);
#endif
}

static int syncfs_impl(int fd) {
    /* iOS doesn't have syncfs, use fsync instead */
    return fsync(fd);
}

/* ============================================================================
 * STATFS - Filesystem statistics
 * ============================================================================ */

static int statfs_impl(const char *path, struct statfs *buf) {
    return _statfs(path, buf);
}

static int fstatfs_impl(int fd, struct statfs *buf) {
    return _fstatfs(fd, buf);
}

static int statvfs_impl(const char *path, struct statvfs *buf) {
    return _statvfs(path, buf);
}

static int fstatvfs_impl(int fd, struct statvfs *buf) {
    return _fstatvfs(fd, buf);
}

/* ============================================================================
 * POSIX_FADVISE - File access advice
 * ============================================================================ */

static int posix_fadvise_impl(int fd, off_t offset, off_t len, int advice) {
    /* iOS doesn't support posix_fadvise, ignore */
    (void)fd;
    (void)offset;
    (void)len;
    (void)advice;
    return 0;
}

/* ============================================================================
 * POSIX_FALLOCATE - Preallocate file space
 * ============================================================================ */

static int posix_fallocate_impl(int fd, off_t offset, off_t len) {
    /* iOS doesn't support fallocate, simulate by writing zeros */
    off_t current = lseek(fd, 0, SEEK_CUR);
    if (current < 0) {
        return -1;
    }

    if (lseek(fd, offset + len - 1, SEEK_SET) < 0) {
        return -1;
    }

    char zero = 0;
    if (write(fd, &zero, 1) != 1) {
        lseek(fd, current, SEEK_SET);
        return -1;
    }

    lseek(fd, current, SEEK_SET);
    return 0;
}

/* ============================================================================
 * Public Canonical Syscalls
 * ============================================================================ */

__attribute__((visibility("default"))) void sync(void) {
    sync_impl();
}

__attribute__((visibility("default"))) int fsync(int fd) {
    return fsync_impl(fd);
}

__attribute__((visibility("default"))) int fdatasync(int fd) {
    return fdatasync_impl(fd);
}

__attribute__((visibility("default"))) int syncfs(int fd) {
    return syncfs_impl(fd);
}

__attribute__((visibility("default"))) int statfs(const char *path, struct statfs *buf) {
    return statfs_impl(path, buf);
}

__attribute__((visibility("default"))) int fstatfs(int fd, struct statfs *buf) {
    return fstatfs_impl(fd, buf);
}

__attribute__((visibility("default"))) int statvfs(const char *path, struct statvfs *buf) {
    return statvfs_impl(path, buf);
}

__attribute__((visibility("default"))) int fstatvfs(int fd, struct statvfs *buf) {
    return fstatvfs_impl(fd, buf);
}

__attribute__((visibility("default"))) int posix_fadvise(int fd, off_t offset, off_t len, int advice) {
    return posix_fadvise_impl(fd, offset, len, advice);
}

__attribute__((visibility("default"))) int posix_fallocate(int fd, off_t offset, off_t len) {
    return posix_fallocate_impl(fd, offset, len);
}
