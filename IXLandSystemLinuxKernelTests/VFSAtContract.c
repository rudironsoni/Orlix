/* IXLandSystemTests/VFSAtContract.c
 * C translation unit for VFS AT_* flag Linux UAPI contract tests.
 *
 * Compiled in a Linux-UAPI-clean context.
 * Uses canonical Linux names directly.
 */

#include <asm-generic/errno.h>
#include <linux/fcntl.h>

#include "fs/vfs.h"

/* Access mode constants - defined locally to avoid Darwin <unistd.h> */
#ifndef X_OK
#define X_OK 1
#endif

/* Contract: vfs_fstatat supports AT_FDCWD */
int vfs_contract_fstatat_at_fdcwd(void) {
    struct linux_stat st;
    return vfs_fstatat(AT_FDCWD, "/etc/passwd", &st, 0);
}

/* Contract: vfs_fstatat supports AT_SYMLINK_NOFOLLOW */
int vfs_contract_fstatat_symlink_nofollow(void) {
    struct linux_stat st;
    return vfs_fstatat(AT_FDCWD, "/etc/passwd", &st, AT_SYMLINK_NOFOLLOW);
}

/* Contract: vfs_fstatat rejects unsupported synthetic paths with AT_SYMLINK_NOFOLLOW */
int vfs_contract_fstatat_synthetic_child_nofollow(void) {
    struct linux_stat st;
    return vfs_fstatat(AT_FDCWD, "/sys/kernel", &st, AT_SYMLINK_NOFOLLOW);
}

/* Contract: vfs_faccessat reports ENOTSUP for AT_EACCESS */
int vfs_contract_faccessat_eaccess_returns_enotsup(void) {
    return vfs_faccessat(AT_FDCWD, "/etc", X_OK, AT_EACCESS);
}

/* Contract: vfs_faccessat reports ENOTSUP for AT_SYMLINK_NOFOLLOW */
int vfs_contract_faccessat_symlink_nofollow_returns_enotsup(void) {
    return vfs_faccessat(AT_FDCWD, "/etc", X_OK, AT_SYMLINK_NOFOLLOW);
}

/* Diagnostic: prove stat works via translate + stat_path */
int vfs_contract_probe_stat_via_translate(void) {
    char host_path[4096];
    int ret;

    /* Step 1: Translate virtual to host path */
    ret = vfs_translate_path_at(AT_FDCWD, "/etc/passwd", host_path, sizeof(host_path));
    if (ret != 0) {
        return -1000 + ret;
    }

    /* Step 2: Stat the host path */
    struct linux_stat st;
    ret = vfs_stat_path(host_path, &st);
    return ret;
}
