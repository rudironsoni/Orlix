#include "vfs.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

ixland_fs_t *ixland_fs_alloc(void) {
    ixland_fs_t *fs = calloc(1, sizeof(ixland_fs_t));
    if (!fs)
        return NULL;

    atomic_init(&fs->refs, 1);
    pthread_mutex_init(&fs->lock, NULL);
    fs->umask = 022;
    strncpy(fs->cwd, "/", sizeof(fs->cwd));
    strncpy(fs->root, "/", sizeof(fs->root));

    return fs;
}

void ixland_fs_free(ixland_fs_t *fs) {
    if (!fs)
        return;
    if (atomic_fetch_sub(&fs->refs, 1) > 1)
        return;

    pthread_mutex_destroy(&fs->lock);
    free(fs);
}

ixland_fs_t *ixland_fs_dup(ixland_fs_t *parent) {
    if (!parent)
        return NULL;

    ixland_fs_t *child = ixland_fs_alloc();
    if (!child)
        return NULL;

    pthread_mutex_lock(&parent->lock);
    strncpy(child->cwd, parent->cwd, sizeof(child->cwd));
    strncpy(child->root, parent->root, sizeof(child->root));
    child->umask = parent->umask;
    child->root_mount = parent->root_mount;
    pthread_mutex_unlock(&parent->lock);

    return child;
}

/* VFS init/deinit/mount/umount are implemented in vfs/ixland_vfs.c */
/* We declare them as external here to avoid duplicate definitions */
extern int ixland_vfs_init(void);
extern void ixland_vfs_deinit(void);
extern int ixland_vfs_mount(const char *source, const char *target, ixland_vfs_ops_t *ops);
extern int ixland_vfs_umount(const char *target);

int ixland_vfs_path_walk(ixland_fs_t *fs, const char *path, ixland_vnode_t **vnode) {
    if (!fs || !path) {
        errno = EINVAL;
        return -1;
    }

    /* TODO: Implement full path resolution */
    /* This is a simplified version */

    if (!fs->root_mount) {
        errno = ENOENT;
        return -1;
    }

    *vnode = NULL;
    errno = ENOENT;
    return -1;
}
