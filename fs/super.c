/* iXland - Superblock Operations
 *
 * Canonical owner for filesystem-level operations:
 * - sync(), fsync(), fdatasync()
 * - syncfs()
 * - statfs(), fstatfs()
 * - sysfs()
 *
 * Linux-shaped canonical owner - iOS mediation as implementation detail
 */

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "../internal/ixland_internal.h"

/* ============================================================================
 * SYNC - Flush filesystem buffers
 * ============================================================================ */

void ixland_sync(void) {
    sync();
}

int ixland_fsync(int fd) {
    return fsync(fd);
}

int ixland_fdatasync(int fd) {
    /* fdatasync not available on iOS, use fsync or F_FULLFSYNC */
#ifdef F_FULLFSYNC
    return fcntl(fd, F_FULLFSYNC);
#else
    return fsync(fd);
#endif
}

int ixland_syncfs(int fd) {
    /* iOS doesn't have syncfs, use fsync instead */
    return fsync(fd);
}

/* ============================================================================
 * POSIX_FADVISE - File access advice
 * ============================================================================ */

int ixland_posix_fadvise(int fd, off_t offset, off_t len, int advice) {
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

int ixland_posix_fallocate(int fd, off_t offset, off_t len) {
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
