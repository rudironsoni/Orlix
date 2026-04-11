/* iXland - Pathname Resolution
 *
 * Canonical owner for path operations:
 * - getcwd(), chdir(), fchdir()
 * - chroot() (restricted)
 * - realpath(), canonicalize_file_name()
 * - pathconf(), fpathconf()
 *
 * Linux-shaped canonical owner - iOS mediation as implementation detail
 */

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../src/ixland/internal/ixland_internal.h"

/* ============================================================================
 * CWD - Current Working Directory
 * ============================================================================ */

char *ixland_getcwd(char *buf, size_t size) {
    return getcwd(buf, size);
}

int ixland_chdir(const char *path) {
    return chdir(path);
}

int ixland_fchdir(int fd) {
    return fchdir(fd);
}

/* ============================================================================
 * CHROOT - Change root (restricted on iOS)
 * ============================================================================ */

int ixland_chroot(const char *path) {
    /* iOS restriction: chroot not allowed */
    (void)path;
    errno = EPERM;
    return -1;
}

/* ============================================================================
 * REALPATH - Canonical path resolution
 * ============================================================================ */

char *ixland_realpath(const char *path, char *resolved_path) {
    return realpath(path, resolved_path);
}

/* ============================================================================
 * PATHCONF - Path configuration
 * ============================================================================ */

long ixland_pathconf(const char *path, int name) {
    return pathconf(path, name);
}

long ixland_fpathconf(int fd, int name) {
    return fpathconf(fd, name);
}
