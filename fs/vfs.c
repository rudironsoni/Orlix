#include "vfs.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

struct fs_struct *alloc_fs_struct(void) {
    struct fs_struct *fs = calloc(1, sizeof(struct fs_struct));
    if (!fs)
        return NULL;

    atomic_init(&fs->users, 1);
    pthread_mutex_init(&fs->lock, NULL);
    fs->umask = 022;

    return fs;
}

void free_fs_struct(struct fs_struct *fs) {
    if (!fs)
        return;
    if (atomic_fetch_sub(&fs->users, 1) > 1)
        return;

    pthread_mutex_destroy(&fs->lock);
    free(fs);
}

struct fs_struct *dup_fs_struct(struct fs_struct *old) {
    if (!old)
        return NULL;

    struct fs_struct *new = alloc_fs_struct();
    if (!new)
        return NULL;

    pthread_mutex_lock(&old->lock);
    if (old->root)
        new->root = old->root;
    if (old->pwd)
        new->pwd = old->pwd;
    new->umask = old->umask;
    pthread_mutex_unlock(&old->lock);

    return new;
}

/* VFS operations - to be implemented in full */
int vfs_init(void) {
    return 0;
}

void vfs_deinit(void) {
}

int vfs_mount(const char *source, const char *target, const char *fstype, unsigned long flags,
              const void *data) {
    (void)source;
    (void)target;
    (void)fstype;
    (void)flags;
    (void)data;
    return -ENOSYS;
}

int vfs_umount(const char *target) {
    (void)target;
    return -ENOSYS;
}

int vfs_open(const char *path, int flags, mode_t mode, int *target_fd) {
    (void)path;
    (void)flags;
    (void)mode;
    (void)target_fd;
    return -ENOSYS;
}

int vfs_close(struct file *file) {
    (void)file;
    return -ENOSYS;
}

int vfs_lookup(const char *path, struct dentry **dentry) {
    (void)path;
    (void)dentry;
    return -ENOSYS;
}

int vfs_path_walk(const char *path, struct dentry **dentry) {
    (void)path;
    (void)dentry;
    return -ENOSYS;
}

int vfs_mkdir(const char *path, mode_t mode) {
    (void)path;
    (void)mode;
    return -ENOSYS;
}

int vfs_unlink(const char *path) {
    (void)path;
    return -ENOSYS;
}

int vfs_rmdir(const char *path) {
    (void)path;
    return -ENOSYS;
}

int vfs_translate_path(const char *vpath, char *host_path, size_t host_path_len) {
    if (!vpath || !host_path || host_path_len == 0) {
        return -EINVAL;
    }

    /* Simple passthrough for now */
    if (strlen(vpath) >= host_path_len) {
        return -ENAMETOOLONG;
    }

    strncpy(host_path, vpath, host_path_len - 1);
    host_path[host_path_len - 1] = '\0';
    return 0;
}

int vfs_reverse_translate(const char *host_path, char *vpath, size_t vpath_len) {
    if (!host_path || !vpath || vpath_len == 0) {
        return -EINVAL;
    }

    /* Simple passthrough for now */
    if (strlen(host_path) >= vpath_len) {
        return -ENAMETOOLONG;
    }

    strncpy(vpath, host_path, vpath_len - 1);
    vpath[vpath_len - 1] = '\0';
    return 0;
}

int vfs_stat_path(const char *pathname, struct stat *statbuf) {
    (void)pathname;
    (void)statbuf;
    return -ENOSYS;
}

int vfs_lstat(const char *pathname, struct stat *statbuf) {
    (void)pathname;
    (void)statbuf;
    return -ENOSYS;
}

int vfs_access(const char *pathname, int mode) {
    (void)pathname;
    (void)mode;
    return -ENOSYS;
}
