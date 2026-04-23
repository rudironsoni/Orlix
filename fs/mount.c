/* iXland - Mount Operations
 *
 * Canonical owner for mount syscalls:
 * - mount(), umount(), umount2() - virtual mount operations
 *
 * Linux-shaped canonical owner - iOS mediation as implementation detail
 * Virtual mount behavior against IXLand's own VFS, NOT host mount(2).
 *
 * NOTE: This file MUST NOT include any headers that transitively pull in
 * Darwin's sys/mount.h to avoid signature conflicts with BSD mount().
 */

#include <errno.h>
#include <string.h>

/* Forward declare VFS functions we need - avoid including vfs.h which pulls in
 * Darwin headers that declare BSD mount() */
extern int vfs_mount(const char *source, const char *target,
                      const char *fstype, unsigned long flags,
                      const void *data);
extern int vfs_umount(const char *target);

/* ============================================================================
 * MOUNT - Virtual mount in IXLand namespace
 * ============================================================================
 *
 * This implements Linux mount semantics against IXLand's own VFS,
 * NOT the iOS host mount() entrypoint.
 *
 * source: An app-container path or user-granted directory
 * target: A path in IXLand's virtual namespace
 * filesystemtype: Interpreted by IXLand VFS
 * mountflags: Linux-style mount flags
 * data: Filesystem-specific data
 */

static int mount_impl(const char *source, const char *target,
                      const char *filesystemtype, unsigned long mountflags,
                      const void *data) {
    if (!source || !target || !filesystemtype) {
        errno = EFAULT;
        return -1;
    }

    /* Validate inputs */
    if (source[0] == '\0' || target[0] == '\0') {
        errno = ENOENT;
        return -1;
    }

    /* Validate filesystemtype exists */
    if (strlen(filesystemtype) == 0) {
        errno = EINVAL;
        return -1;
    }

    /* Delegate to VFS layer for virtual mount */
    return vfs_mount(source, target, filesystemtype, mountflags, data);
}

/* ============================================================================
 * UMOUNT - Virtual unmount from IXLand namespace
 * ============================================================================ */

static int umount_impl(const char *target) {
    if (!target) {
        errno = EFAULT;
        return -1;
    }

    if (target[0] == '\0') {
        errno = ENOENT;
        return -1;
    }

    return vfs_umount(target);
}

/* ============================================================================
 * UMOUNT2 - Virtual unmount with flags
 * ============================================================================ */

static int umount2_impl(const char *target, int flags) {
    /* For now, flags are parsed but most are not implemented */
    /* MNT_FORCE not supported by IXLand VFS yet */
    /* MNT_DETACH - lazy unmount */
    /* MNT_EXPIRE - mark for expiry */
    (void)flags;

    return umount_impl(target);
}

/* ============================================================================
 * Public Canonical Syscalls
 * ============================================================================ */

__attribute__((visibility("default"))) int mount(const char *source,
                                                   const char *target,
                                                   const char *filesystemtype,
                                                   unsigned long mountflags,
                                                   const void *data) {
    return mount_impl(source, target, filesystemtype, mountflags, data);
}

__attribute__((visibility("default"))) int umount(const char *target) {
    return umount_impl(target);
}

__attribute__((visibility("default"))) int umount2(const char *target, int flags) {
    return umount2_impl(target, flags);
}
