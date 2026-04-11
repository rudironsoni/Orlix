#ifndef IXLAND_VFS_H
#define IXLAND_VFS_H

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "ixland/ixland_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define IXLAND_MAX_MOUNTS 64

typedef enum {
    IXLAND_VTYPE_FILE = 1,
    IXLAND_VTYPE_DIR,
    IXLAND_VTYPE_LINK,
    IXLAND_VTYPE_CHR,
    IXLAND_VTYPE_BLK,
    IXLAND_VTYPE_FIFO,
    IXLAND_VTYPE_SOCK
} ixland_vtype_t;

typedef struct ixland_vnode ixland_vnode_t;
typedef struct ixland_vfs_ops ixland_vfs_ops_t;
typedef struct ixland_mount ixland_mount_t;
typedef struct ixland_fs ixland_fs_t;

struct ixland_vnode {
    uint64_t ino;
    ixland_vtype_t type;
    mode_t mode;
    uid_t uid;
    gid_t gid;
    off_t size;
    struct timespec atime;
    struct timespec mtime;
    struct timespec ctime;
    atomic_int refs;
    void *fs_data;
    ixland_mount_t *mount;
};

struct ixland_vfs_ops {
    int (*open)(ixland_vnode_t *node, int flags);
    int (*close)(ixland_vnode_t *node);
    ssize_t (*read)(ixland_vnode_t *node, void *buf, size_t count, off_t offset);
    ssize_t (*write)(ixland_vnode_t *node, const void *buf, size_t count, off_t offset);
    int (*stat)(ixland_vnode_t *node, struct stat *st);
    int (*lookup)(ixland_mount_t *mnt, ixland_vnode_t *parent, const char *name,
                  ixland_vnode_t **result);
    int (*create)(ixland_mount_t *mnt, ixland_vnode_t *parent, const char *name, mode_t mode);
    int (*mkdir)(ixland_mount_t *mnt, ixland_vnode_t *parent, const char *name, mode_t mode);
    int (*unlink)(ixland_mount_t *mnt, ixland_vnode_t *parent, const char *name);
    int (*rmdir)(ixland_mount_t *mnt, ixland_vnode_t *parent, const char *name);
    int (*readdir)(ixland_mount_t *mnt, ixland_vnode_t *dir, void *buf, size_t count);
};

struct ixland_mount {
    char mountpoint[IXLAND_MAX_PATH];
    char source[IXLAND_MAX_PATH];
    ixland_vfs_ops_t *ops;
    void *fs_data;
    atomic_int refs;
};

struct ixland_fs {
    char cwd[IXLAND_MAX_PATH];
    char root[IXLAND_MAX_PATH];
    mode_t umask;
    ixland_mount_t *root_mount;
    pthread_mutex_t lock;
    atomic_int refs;
};

ixland_fs_t *ixland_fs_alloc(void);
void ixland_fs_free(ixland_fs_t *fs);
ixland_fs_t *ixland_fs_dup(ixland_fs_t *parent);

int ixland_vfs_init(void);
void ixland_vfs_deinit(void);
int ixland_vfs_mount(const char *source, const char *target, ixland_vfs_ops_t *ops);
int ixland_vfs_umount(const char *target);

int ixland_vfs_open(const char *path, int flags, mode_t mode, ixland_vnode_t **vnode);
int ixland_vfs_close(ixland_vnode_t *vnode);
int ixland_vfs_stat(const char *path, struct stat *st);
int ixland_vfs_mkdir(const char *path, mode_t mode);
int ixland_vfs_unlink(const char *path);
int ixland_vfs_rmdir(const char *path);

int ixland_vfs_lookup(ixland_fs_t *fs, const char *path, ixland_vnode_t **vnode);
int ixland_vfs_path_walk(ixland_fs_t *fs, const char *path, ixland_vnode_t **vnode);

#ifdef __cplusplus
}
#endif

#endif
