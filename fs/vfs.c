#include "vfs.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

static const char *const vfs_virtual_root_path = "/";
static const char *const vfs_host_root_path = "/var/mobile/Containers/IXLand";

static int vfs_copy_string(const char *src, char *dst, size_t dst_len) {
    size_t len;

    if (!src || !dst || dst_len == 0) {
        return -EINVAL;
    }

    len = strlen(src);
    if (len >= dst_len) {
        return -ENAMETOOLONG;
    }

    memcpy(dst, src, len + 1);
    return 0;
}

static bool vfs_has_host_root_prefix(const char *path, size_t root_len) {
    if (strncmp(path, vfs_host_root_path, root_len) != 0) {
        return false;
    }

    return path[root_len] == '\0' || path[root_len] == '/';
}

int vfs_normalize_linux_path(const char *input, char *output, size_t output_len) {
    char scratch[MAX_PATH];
    size_t input_len;
    size_t normalized_len;

    if (!input || !output || output_len == 0) {
        return -EINVAL;
    }

    input_len = strlen(input);
    if (input_len == 0) {
        return -ENOENT;
    }

    if (input_len >= sizeof(scratch)) {
        return -ENAMETOOLONG;
    }

    if (strcmp(input, ".") == 0 || strcmp(input, "..") == 0 || strstr(input, "/../") != NULL ||
        strncmp(input, "../", 3) == 0 ||
        (input_len >= 3 && strcmp(input + input_len - 3, "/..") == 0)) {
        return -EINVAL;
    }

    if (input[0] == '/') {
        memcpy(scratch, input, input_len + 1);
    } else {
        if (input_len + 2 > sizeof(scratch)) {
            return -ENAMETOOLONG;
        }
        scratch[0] = '/';
        memcpy(scratch + 1, input, input_len + 1);
    }

    while (strstr(scratch, "//") != NULL) {
        char *double_slash = strstr(scratch, "//");
        memmove(double_slash, double_slash + 1, strlen(double_slash));
    }

    if (strstr(scratch, "/./") != NULL) {
        return -EINVAL;
    }

    normalized_len = strlen(scratch);
    if (normalized_len > 1 && scratch[normalized_len - 1] == '/') {
        scratch[normalized_len - 1] = '\0';
    }

    return vfs_copy_string(scratch, output, output_len);
}

static int vfs_join_host_root(const char *normalized_virtual_path, char *host_path,
                              size_t host_path_len) {
    size_t root_len = strlen(vfs_host_root_path);
    size_t virtual_len;
    size_t total_len;

    if (!normalized_virtual_path || !host_path || host_path_len == 0) {
        return -EINVAL;
    }

    virtual_len = strlen(normalized_virtual_path);
    if (strcmp(normalized_virtual_path, "/") == 0) {
        return vfs_copy_string(vfs_host_root_path, host_path, host_path_len);
    }

    total_len = root_len + virtual_len;
    if (total_len >= host_path_len) {
        return -ENAMETOOLONG;
    }

    memcpy(host_path, vfs_host_root_path, root_len);
    memcpy(host_path + root_len, normalized_virtual_path, virtual_len + 1);
    return 0;
}

const char *vfs_host_backing_root(void) {
    return vfs_host_root_path;
}

const char *vfs_virtual_root(void) {
    return vfs_virtual_root_path;
}

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
    memcpy(new->root_path, old->root_path, MAX_PATH);
    memcpy(new->pwd_path, old->pwd_path, MAX_PATH);
    pthread_mutex_unlock(&old->lock);

    return new;
}

/* Initialize fs_struct with virtual root path */
int fs_init_root(struct fs_struct *fs, const char *root_path) {
    if (!fs || !root_path)
        return -EINVAL;

    char normalized[MAX_PATH];
    if (vfs_normalize_linux_path(root_path, normalized, sizeof(normalized)) < 0)
        return -EINVAL;

    pthread_mutex_lock(&fs->lock);
    memcpy(fs->root_path, normalized, MAX_PATH);
    /* Also set pwd to root if not already set */
    if (fs->pwd_path[0] == '\0')
        memcpy(fs->pwd_path, normalized, MAX_PATH);
    pthread_mutex_unlock(&fs->lock);

    return 0;
}

/* Initialize fs_struct with virtual pwd path */
int fs_init_pwd(struct fs_struct *fs, const char *pwd_path) {
    if (!fs || !pwd_path)
        return -EINVAL;

    char normalized[MAX_PATH];
    if (vfs_normalize_linux_path(pwd_path, normalized, sizeof(normalized)) < 0)
        return -EINVAL;

    pthread_mutex_lock(&fs->lock);
    memcpy(fs->pwd_path, normalized, MAX_PATH);
    /* Also set root to pwd if not already set */
    if (fs->root_path[0] == '\0')
        memcpy(fs->root_path, normalized, MAX_PATH);
    pthread_mutex_unlock(&fs->lock);

    return 0;
}

/* Set new pwd - task-aware path change */
int fs_set_pwd(struct fs_struct *fs, const char *new_pwd) {
    return fs_init_pwd(fs, new_pwd);
}

/* Set new root - task-aware root change */
int fs_set_root(struct fs_struct *fs, const char *new_root) {
    return fs_init_root(fs, new_root);
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

/* Get current task's fs_struct - forward declaration */
struct task_struct *get_current(void);

/* Internal: resolve path from task's root (absolute) or pwd (relative) */
int vfs_translate_path_task(const char *vpath, char *host_path, size_t host_path_len,
                            struct fs_struct *fs) {
    char resolved_virtual[MAX_PATH];
    char work_buffer[MAX_PATH];
    int ret;

    if (!vpath || !host_path || host_path_len == 0)
        return -EINVAL;

    /* Validate and reject parent escapes early */
    if (strstr(vpath, "/../") != NULL || strncmp(vpath, "../", 3) == 0 ||
        (strlen(vpath) >= 3 && strcmp(vpath + strlen(vpath) - 3, "/..") == 0)) {
        return -EINVAL;
    }

    /* Determine base for resolution and resolve into work_buffer */
    if (vpath[0] == '/') {
        /* Absolute path: resolve from task's virtual root */
        const char *root_path = (fs && fs->root_path[0] != '\0') ? fs->root_path : vfs_virtual_root_path;
        /* For absolute paths, prepend root and path */
        size_t root_len = strlen(root_path);
        size_t vpath_len = strlen(vpath);
        if (root_len + vpath_len >= sizeof(work_buffer))
            return -ENAMETOOLONG;
        memcpy(work_buffer, root_path, root_len);
        memcpy(work_buffer + root_len, vpath, vpath_len + 1);
    } else {
        /* Relative path: resolve from task's virtual pwd */
        const char *pwd_path = (fs && fs->pwd_path[0] != '\0') ? fs->pwd_path : vfs_virtual_root_path;
        size_t pwd_len = strlen(pwd_path);
        size_t rel_len = strlen(vpath);
        if (pwd_len + 1 + rel_len >= sizeof(work_buffer))
            return -ENAMETOOLONG;
        memcpy(work_buffer, pwd_path, pwd_len);
        if (pwd_path[pwd_len - 1] != '/') {
            work_buffer[pwd_len] = '/';
            pwd_len++;
        }
        memcpy(work_buffer + pwd_len, vpath, rel_len + 1);
    }

    /* Normalize the resolved virtual path */
    ret = vfs_normalize_linux_path(work_buffer, resolved_virtual, sizeof(resolved_virtual));
    if (ret < 0)
        return ret;

    return vfs_join_host_root(resolved_virtual, host_path, host_path_len);
}

/* Legacy API: translate path using hardcoded root (for backward compatibility) */
int vfs_translate_path(const char *vpath, char *host_path, size_t host_path_len) {
    return vfs_translate_path_task(vpath, host_path, host_path_len, NULL);
}

int vfs_reverse_translate(const char *host_path, char *vpath, size_t vpath_len) {
    size_t root_len;
    const char *suffix;

    if (!host_path || !vpath || vpath_len == 0) {
        return -EINVAL;
    }

    root_len = strlen(vfs_host_root_path);
    if (!vfs_has_host_root_prefix(host_path, root_len)) {
        return -EXDEV;
    }

    suffix = host_path + root_len;
    if (*suffix == '\0') {
        return vfs_copy_string(vfs_virtual_root_path, vpath, vpath_len);
    }

    return vfs_normalize_linux_path(suffix, vpath, vpath_len);
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
