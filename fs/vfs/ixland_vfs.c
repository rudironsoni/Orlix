/* iOS Subsystem for Linux - Virtual File System
 *
 * Complete VFS implementation for path translation, sandboxing,
 * and virtual mount points.
 */

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../internal/ixland_internal.h"

/* ============================================================================
 * VFS GLOBAL STATE
 * ============================================================================ */

pthread_mutex_t vfs_lock = PTHREAD_MUTEX_INITIALIZER;

/* iOS Sandbox root - determined at runtime */
char ios_sandbox_root[IXLAND_MAX_PATH] = "";
char ios_home_dir[IXLAND_MAX_PATH] = "";
char ios_tmp_dir[IXLAND_MAX_PATH] = "";

/* Virtual mount points */
#define VFS_MAX_MOUNTS 64
ixland_vfs_mount_t vfs_mount_table[VFS_MAX_MOUNTS];
int vfs_mount_count = 0;

static int ixland_vfs_mount_unlocked(const char *source, const char *target, unsigned long flags) {
    if (!source || !target) {
        errno = EINVAL;
        return -1;
    }

    if (vfs_mount_count >= VFS_MAX_MOUNTS) {
        errno = ENOMEM;
        return -1;
    }

    for (int i = 0; i < vfs_mount_count; i++) {
        if (vfs_mount_table[i].active && strcmp(vfs_mount_table[i].mountpoint, target) == 0) {
            errno = EBUSY;
            return -1;
        }
    }

    ixland_vfs_mount_t *mnt = &vfs_mount_table[vfs_mount_count++];
    mnt->active = true;
    mnt->flags = flags;
    strncpy(mnt->mountpoint, target, IXLAND_MAX_PATH - 1);
    strncpy(mnt->target, source, IXLAND_MAX_PATH - 1);
    mnt->mountpoint[IXLAND_MAX_PATH - 1] = '\0';
    mnt->target[IXLAND_MAX_PATH - 1] = '\0';

    return 0;
}

/* ============================================================================
 * PATH TRANSLATION
 * ============================================================================ */

int ixland_vfs_init(void) {
    pthread_mutex_lock(&vfs_lock);

    /* Clear mount table */
    memset(vfs_mount_table, 0, sizeof(vfs_mount_table));
    vfs_mount_count = 0;

    /* Determine iOS sandbox root */
    char *home = getenv("HOME");
    if (home) {
        strncpy(ios_sandbox_root, home, IXLAND_MAX_PATH - 1);
        ios_sandbox_root[IXLAND_MAX_PATH - 1] = '\0';

        /* Standard iOS directories */
        snprintf(ios_home_dir, IXLAND_MAX_PATH, "%s/Documents", home);
        snprintf(ios_tmp_dir, IXLAND_MAX_PATH, "%s/tmp", home);

        /* Set up default mounts - don't fail if mkdir fails */
        ixland_vfs_mount_unlocked("/home/user", ios_home_dir, IXLAND_VFS_BIND);
        ixland_vfs_mount_unlocked("/tmp", ios_tmp_dir, IXLAND_VFS_BIND);

        /* /etc -> ~/Library/etc (config files) - optional */
        char etc_path[IXLAND_MAX_PATH];
        snprintf(etc_path, IXLAND_MAX_PATH, "%s/Library/etc", home);
        /* Don't call mkdir here - might fail in restricted contexts */
        ixland_vfs_mount_unlocked("/etc", etc_path, IXLAND_VFS_BIND);

        /* /var/tmp -> ~/tmp */
        ixland_vfs_mount_unlocked("/var/tmp", ios_tmp_dir, IXLAND_VFS_BIND);
    } else {
        /* No HOME set - use default paths */
        strncpy(ios_sandbox_root, "/", IXLAND_MAX_PATH - 1);
        ios_sandbox_root[IXLAND_MAX_PATH - 1] = '\0';
        strncpy(ios_home_dir, "/", IXLAND_MAX_PATH - 1);
        ios_home_dir[IXLAND_MAX_PATH - 1] = '\0';
        strncpy(ios_tmp_dir, "/tmp", IXLAND_MAX_PATH - 1);
        ios_tmp_dir[IXLAND_MAX_PATH - 1] = '\0';

        /* Set up minimal mounts */
        ixland_vfs_mount_unlocked("/home/user", ios_home_dir, IXLAND_VFS_BIND);
        ixland_vfs_mount_unlocked("/tmp", ios_tmp_dir, IXLAND_VFS_BIND);
    }

    pthread_mutex_unlock(&vfs_lock);
    return 0;
}

void ixland_vfs_deinit(void) {
    pthread_mutex_lock(&vfs_lock);

    /* Unmount everything */
    for (int i = 0; i < vfs_mount_count; i++) {
        if (vfs_mount_table[i].active) {
            vfs_mount_table[i].active = false;
        }
    }
    vfs_mount_count = 0;

    pthread_mutex_unlock(&vfs_lock);
}

/* Translate virtual path to iOS path */
int ixland_vfs_translate(const char *vpath, char *ios_path, size_t ios_path_len) {
    if (!vpath || !ios_path || ios_path_len == 0) {
        errno = EINVAL;
        return -1;
    }

    pthread_mutex_lock(&vfs_lock);

    /* Normalize virtual path first */
    char norm_vpath[IXLAND_MAX_PATH];
    strncpy(norm_vpath, vpath, IXLAND_MAX_PATH - 1);
    norm_vpath[IXLAND_MAX_PATH - 1] = '\0';
    __ixland_path_normalize(norm_vpath);

    /* Check for mount points (longest match first) */
    ixland_vfs_mount_t *best_match = NULL;
    size_t best_len = 0;

    for (int i = 0; i < vfs_mount_count; i++) {
        if (!vfs_mount_table[i].active)
            continue;

        size_t mount_len = strlen(vfs_mount_table[i].mountpoint);
        if (strncmp(norm_vpath, vfs_mount_table[i].mountpoint, mount_len) == 0) {
            /* Check if it's a complete path component match */
            if (norm_vpath[mount_len] == '/' || norm_vpath[mount_len] == '\0') {
                if (mount_len > best_len) {
                    best_match = &vfs_mount_table[i];
                    best_len = mount_len;
                }
            }
        }
    }

    if (best_match) {
        /* Translate path */
        const char *remainder = norm_vpath + best_len;
        if (*remainder == '/')
            remainder++;

        snprintf(ios_path, ios_path_len, "%s/%s", best_match->target, remainder);
    } else {
        /* No mount point - map to sandbox relative */
        if (norm_vpath[0] == '/') {
            /* Absolute path without mount - use home as fallback */
            snprintf(ios_path, ios_path_len, "%s%s", ios_home_dir, norm_vpath);
        } else {
            /* Relative path - resolve against CWD */
            char cwd[IXLAND_MAX_PATH];
            if (getcwd(cwd, sizeof(cwd))) {
                snprintf(ios_path, ios_path_len, "%s/%s", cwd, norm_vpath);
            } else {
                strncpy(ios_path, norm_vpath, ios_path_len - 1);
                ios_path[ios_path_len - 1] = '\0';
            }
        }
    }

    /* Final normalization */
    __ixland_path_normalize(ios_path);

    /* Validate result is within sandbox */
    if (!ixland_vfs_in_sandbox(ios_path)) {
        pthread_mutex_unlock(&vfs_lock);
        errno = EACCES;
        return -1;
    }

    pthread_mutex_unlock(&vfs_lock);
    return 0;
}

/* Reverse translate iOS path to virtual path */
int ixland_vfs_reverse_translate(const char *ios_path, char *vpath, size_t vpath_len) {
    if (!ios_path || !vpath || vpath_len == 0) {
        errno = EINVAL;
        return -1;
    }

    pthread_mutex_lock(&vfs_lock);

    /* Find the mount point this iOS path belongs to */
    for (int i = 0; i < vfs_mount_count; i++) {
        if (!vfs_mount_table[i].active)
            continue;

        size_t target_len = strlen(vfs_mount_table[i].target);
        if (strncmp(ios_path, vfs_mount_table[i].target, target_len) == 0) {
            if (ios_path[target_len] == '/' || ios_path[target_len] == '\0') {
                const char *remainder = ios_path + target_len;
                if (*remainder == '/')
                    remainder++;

                if (strlen(remainder) == 0) {
                    strncpy(vpath, vfs_mount_table[i].mountpoint, vpath_len - 1);
                    vpath[vpath_len - 1] = '\0';
                } else {
                    snprintf(vpath, vpath_len, "%s/%s", vfs_mount_table[i].mountpoint, remainder);
                }

                pthread_mutex_unlock(&vfs_lock);
                return 0;
            }
        }
    }

    /* No matching mount - return as-is */
    strncpy(vpath, ios_path, vpath_len - 1);
    vpath[vpath_len - 1] = '\0';

    pthread_mutex_unlock(&vfs_lock);
    return 0;
}

/* ============================================================================
 * MOUNT OPERATIONS
 * ============================================================================ */

int ixland_vfs_mount(const char *source, const char *target, unsigned long flags) {
    pthread_mutex_lock(&vfs_lock);
    int result = ixland_vfs_mount_unlocked(source, target, flags);
    pthread_mutex_unlock(&vfs_lock);
    return result;
}

int ixland_vfs_umount(const char *target) {
    if (!target) {
        errno = EINVAL;
        return -1;
    }

    pthread_mutex_lock(&vfs_lock);

    for (int i = 0; i < vfs_mount_count; i++) {
        if (vfs_mount_table[i].active && strcmp(vfs_mount_table[i].mountpoint, target) == 0) {
            vfs_mount_table[i].active = false;
            pthread_mutex_unlock(&vfs_lock);
            return 0;
        }
    }

    pthread_mutex_unlock(&vfs_lock);
    errno = EINVAL;
    return -1;
}

int ixland_vfs_umount2(const char *target, int flags) {
    (void)flags;
    return ixland_vfs_umount(target);
}

/* ============================================================================
 * SANDBOX VALIDATION
 * ============================================================================ */

bool ixland_vfs_in_sandbox(const char *path) {
    if (!path)
        return false;

    /* Must be absolute path */
    if (path[0] != '/')
        return false;

    /* Check against sandbox root */
    size_t sandbox_len = strlen(ios_sandbox_root);
    if (strncmp(path, ios_sandbox_root, sandbox_len) == 0) {
        /* Path is within sandbox */
        return true;
    }

    /* Allow /dev/null, /dev/zero, /dev/random, etc. */
    if (strncmp(path, "/dev/", 5) == 0) {
        /* Allow specific device files */
        const char *dev = path + 5;
        if (strcmp(dev, "null") == 0 || strcmp(dev, "zero") == 0 || strcmp(dev, "random") == 0 ||
            strcmp(dev, "urandom") == 0 || strcmp(dev, "tty") == 0) {
            return true;
        }
    }

    return false;
}

int ixland_vfs_chroot(const char *path) {
    /* chroot is not allowed in iOS sandbox */
    (void)path;
    errno = EPERM;
    return -1;
}

/* ============================================================================
 * FILE OPERATIONS WITH VFS
 * ============================================================================ */

int ixland_vfs_open(const char *pathname, int flags, mode_t mode) {
    char ios_path[IXLAND_MAX_PATH];

    if (ixland_vfs_translate(pathname, ios_path, sizeof(ios_path)) != 0) {
        return -1;
    }

    return open(ios_path, flags, mode);
}

int ixland_vfs_stat(const char *pathname, struct stat *statbuf) {
    char ios_path[IXLAND_MAX_PATH];

    if (ixland_vfs_translate(pathname, ios_path, sizeof(ios_path)) != 0) {
        return -1;
    }

    return stat(ios_path, statbuf);
}

int ixland_vfs_lstat(const char *pathname, struct stat *statbuf) {
    char ios_path[IXLAND_MAX_PATH];

    if (ixland_vfs_translate(pathname, ios_path, sizeof(ios_path)) != 0) {
        return -1;
    }

    return lstat(ios_path, statbuf);
}

int ixland_vfs_access(const char *pathname, int mode) {
    char ios_path[IXLAND_MAX_PATH];

    if (ixland_vfs_translate(pathname, ios_path, sizeof(ios_path)) != 0) {
        return -1;
    }

    return access(ios_path, mode);
}

int ixland_vfs_mkdir(const char *pathname, mode_t mode) {
    char ios_path[IXLAND_MAX_PATH];

    if (ixland_vfs_translate(pathname, ios_path, sizeof(ios_path)) != 0) {
        return -1;
    }

    return mkdir(ios_path, mode);
}

int ixland_vfs_rmdir(const char *pathname) {
    char ios_path[IXLAND_MAX_PATH];

    if (ixland_vfs_translate(pathname, ios_path, sizeof(ios_path)) != 0) {
        return -1;
    }

    return rmdir(ios_path);
}

int ixland_vfs_unlink(const char *pathname) {
    char ios_path[IXLAND_MAX_PATH];

    if (ixland_vfs_translate(pathname, ios_path, sizeof(ios_path)) != 0) {
        return -1;
    }

    return unlink(ios_path);
}

int ixland_vfs_rename(const char *oldpath, const char *newpath) {
    char ios_old[IXLAND_MAX_PATH];
    char ios_new[IXLAND_MAX_PATH];

    if (ixland_vfs_translate(oldpath, ios_old, sizeof(ios_old)) != 0) {
        return -1;
    }

    if (ixland_vfs_translate(newpath, ios_new, sizeof(ios_new)) != 0) {
        return -1;
    }

    return rename(ios_old, ios_new);
}

int ixland_vfs_symlink(const char *target, const char *linkpath) {
    char ios_link[IXLAND_MAX_PATH];

    if (ixland_vfs_translate(linkpath, ios_link, sizeof(ios_link)) != 0) {
        return -1;
    }

    /* Target is stored as-is in symlink */
    return symlink(target, ios_link);
}

ssize_t ixland_vfs_readlink(const char *pathname, char *buf, size_t bufsiz) {
    char ios_path[IXLAND_MAX_PATH];

    if (ixland_vfs_translate(pathname, ios_path, sizeof(ios_path)) != 0) {
        return -1;
    }

    return readlink(ios_path, buf, bufsiz);
}

int ixland_vfs_chmod(const char *pathname, mode_t mode) {
    char ios_path[IXLAND_MAX_PATH];

    if (ixland_vfs_translate(pathname, ios_path, sizeof(ios_path)) != 0) {
        return -1;
    }

    return chmod(ios_path, mode);
}

int ixland_vfs_chown(const char *pathname, uid_t owner, gid_t group) {
    /* chown is restricted in iOS sandbox */
    (void)pathname;
    (void)owner;
    (void)group;
    errno = EPERM;
    return -1;
}

int ixland_vfs_link(const char *oldpath, const char *newpath) {
    char ios_old[IXLAND_MAX_PATH];
    char ios_new[IXLAND_MAX_PATH];

    if (ixland_vfs_translate(oldpath, ios_old, sizeof(ios_old)) != 0) {
        return -1;
    }

    if (ixland_vfs_translate(newpath, ios_new, sizeof(ios_new)) != 0) {
        return -1;
    }

    return link(ios_old, ios_new);
}

/* ============================================================================
 * VFS PUBLIC API
 * ============================================================================ */

const char *ixland_vfs_get_sandbox_root(void) {
    return ios_sandbox_root;
}

const char *ixland_vfs_get_home(void) {
    return ios_home_dir;
}

const char *ixland_vfs_get_tmp(void) {
    return ios_tmp_dir;
}
