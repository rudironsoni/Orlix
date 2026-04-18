#include "vfs.h"
#include "internal/ios/fs/backing_io.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

/* Linux UAPI AT flag values - these are the public ABI contract */
/* Darwin headers define these with different values; undef and redefine for Linux ABI */
#undef AT_SYMLINK_NOFOLLOW
#define AT_SYMLINK_NOFOLLOW	0x100

#undef AT_EACCESS
#define AT_EACCESS		0x200

/* Linux open flags - cannot include linux/fcntl.h due to conflicts, use canonical values */
#define IX_O_RDONLY		0x0000
#define IX_O_WRONLY		0x0001
#define IX_O_RDWR		0x0002
#define IX_O_ACCMODE		0x0003
#define IX_O_CREAT		0x0040
#define IX_O_EXCL		0x0080
#define IX_O_NOCTTY		0x0100
#define IX_O_TRUNC		0x0200
#define IX_O_APPEND		0x0400
#define IX_O_NONBLOCK		0x0800
#define IX_O_DSYNC		0x1000
#define IX_O_SYNC		0x101000
#define IX_O_RSYNC		0x101000
#define IX_O_CLOEXEC		0x80000

#include "fdtable.h"
#include "../kernel/task.h"

static const char *const vfs_virtual_root_path = "/";
static const char *const vfs_host_root_path = "/tmp";

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

/* Bootstrap Linux identity/config baseline in private rootfs */
static int vfs_bootstrap_etc_files_impl(void) {
    const char *passwd_content =
        "root:x:0:0:root:/root:/bin/sh\n"
        "ixland:x:1000:1000:IXLand User:/home/ixland:/bin/sh\n";
    const char *group_content =
        "root:x:0:\n"
        "ixland:x:1000:\n";
    const char *hosts_content =
        "127.0.0.1\tlocalhost\n"
        "::1\t\tlocalhost ip6-localhost ip6-loopback\n";
    const char *resolv_content =
        "nameserver 8.8.8.8\n"
        "nameserver 8.8.4.4\n";

    char etc_path[MAX_PATH];
    char file_path[MAX_PATH];
    int fd;
    ssize_t written;
    size_t len;

/* Suppress deprecation warnings for intentional syscall usage in bootstrap */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

    /* Create /etc directory under backing root using host syscall */
    snprintf(etc_path, sizeof(etc_path), "%s/etc", vfs_host_root_path);
    syscall(SYS_mkdir, etc_path, 0755);

#pragma clang diagnostic pop

    /* Create /etc/passwd */
    snprintf(file_path, sizeof(file_path), "%s/passwd", etc_path);
    fd = host_open_impl(file_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        len = strlen(passwd_content);
        written = host_write_impl(fd, passwd_content, len);
        host_close_impl(fd);
        if (written != (ssize_t)len) {
            /* Non-fatal: file creation may fail in constrained environments */
        }
    }

    /* Create /etc/group */
    snprintf(file_path, sizeof(file_path), "%s/group", etc_path);
    fd = host_open_impl(file_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        len = strlen(group_content);
        written = host_write_impl(fd, group_content, len);
        host_close_impl(fd);
        if (written != (ssize_t)len) {
            /* Non-fatal: file creation may fail in constrained environments */
        }
    }

    /* Create /etc/hosts */
    snprintf(file_path, sizeof(file_path), "%s/hosts", etc_path);
    fd = host_open_impl(file_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        len = strlen(hosts_content);
        written = host_write_impl(fd, hosts_content, len);
        host_close_impl(fd);
        if (written != (ssize_t)len) {
            /* Non-fatal: file creation may fail in constrained environments */
        }
    }

    /* Create /etc/resolv.conf */
    snprintf(file_path, sizeof(file_path), "%s/resolv.conf", etc_path);
    fd = host_open_impl(file_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        len = strlen(resolv_content);
        written = host_write_impl(fd, resolv_content, len);
        host_close_impl(fd);
        if (written != (ssize_t)len) {
            /* Non-fatal: file creation may fail in constrained environments */
        }
    }

    return 0;
}

/* VFS operations - to be implemented in full */
int vfs_init(void) {
    /* Bootstrap Linux identity/config baseline */
    vfs_bootstrap_etc_files_impl();
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
    int real_fd;

    if (!path || !target_fd) {
        return -EFAULT;
    }

    /* Current in-repo callers pass the host platform's open flags.
     * Preserve the actual call surface used by IXLandSystem while the Linux-facing
     * contract is modeled in higher layers. Validate only combinations we can
     * represent coherently now.
     */
    if ((flags & O_EXCL) && !(flags & O_CREAT)) {
        return -EINVAL;
    }

    real_fd = host_open_impl(path, flags, mode);
    if (real_fd < 0) {
        return -errno;
    }

    *target_fd = real_fd;
    return 0;
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

static int vfs_join_virtual_path(const char *base_path, const char *suffix, char *joined_path,
                                 size_t joined_path_len) {
    size_t base_len;
    size_t suffix_len;
    size_t suffix_offset;

    if (!base_path || !suffix || !joined_path || joined_path_len == 0) {
        return -EINVAL;
    }

    base_len = strlen(base_path);
    suffix_len = strlen(suffix);
    suffix_offset = (suffix[0] == '/') ? 1 : 0;

    if (base_len == 0) {
        return -EINVAL;
    }

    if (strcmp(base_path, "/") == 0) {
        if (suffix_len - suffix_offset + 1 >= joined_path_len) {
            return -ENAMETOOLONG;
        }
        joined_path[0] = '/';
        memcpy(joined_path + 1, suffix + suffix_offset, suffix_len - suffix_offset + 1);
        return 0;
    }

    if (base_len + 1 + suffix_len - suffix_offset >= joined_path_len) {
        return -ENAMETOOLONG;
    }

    memcpy(joined_path, base_path, base_len);
    joined_path[base_len] = '/';
    memcpy(joined_path + base_len + 1, suffix + suffix_offset, suffix_len - suffix_offset + 1);
    return 0;
}

int vfs_resolve_virtual_path_task(const char *vpath, char *resolved_vpath, size_t resolved_vpath_len,
                                  struct fs_struct *fs) {
    char work_buffer[MAX_PATH];
    const char *root_path;
    const char *pwd_path;
    int ret;

    if (!vpath || !resolved_vpath || resolved_vpath_len == 0) {
        return -EINVAL;
    }

    root_path = (fs && fs->root_path[0] != '\0') ? fs->root_path : vfs_virtual_root_path;
    pwd_path = (fs && fs->pwd_path[0] != '\0') ? fs->pwd_path : root_path;

    if (vpath[0] == '/') {
        ret = vfs_join_virtual_path(root_path, vpath, work_buffer, sizeof(work_buffer));
    } else {
        ret = vfs_join_virtual_path(pwd_path, vpath, work_buffer, sizeof(work_buffer));
    }
    if (ret < 0) {
        return ret;
    }

    return vfs_normalize_linux_path(work_buffer, resolved_vpath, resolved_vpath_len);
}

int vfs_getcwd_path_task(struct fs_struct *fs, char *vpath, size_t vpath_len) {
    const char *pwd_path;

    if (!vpath || vpath_len == 0) {
        return -EINVAL;
    }

    pwd_path = (fs && fs->pwd_path[0] != '\0') ? fs->pwd_path : vfs_virtual_root_path;
    return vfs_copy_string(pwd_path, vpath, vpath_len);
}

int vfs_resolve_virtual_path_at(int dirfd, const char *vpath, char *resolved_vpath,
                                size_t resolved_vpath_len) {
    struct task_struct *task;
    struct fs_struct *fs;
    char dir_host_path[MAX_PATH];
    char dir_virtual_path[MAX_PATH];
    char joined_virtual_path[MAX_PATH];
    void *entry;
    int ret;

    if (!vpath || !resolved_vpath || resolved_vpath_len == 0) {
        return -EINVAL;
    }

    task = get_current();
    if (!task) {
        return -ESRCH;
    }

    fs = task->fs;
    if (vpath[0] == '/' || dirfd == AT_FDCWD) {
        return vfs_resolve_virtual_path_task(vpath, resolved_vpath, resolved_vpath_len, fs);
    }

    entry = get_fd_entry_impl(dirfd);
    if (!entry) {
        return -EBADF;
    }

    if (!get_fd_is_dir_impl(entry)) {
        put_fd_entry_impl(entry);
        return -ENOTDIR;
    }

    ret = get_fd_path_impl(entry, dir_host_path, sizeof(dir_host_path));
    put_fd_entry_impl(entry);
    if (ret != 0) {
        return -errno;
    }

    ret = vfs_reverse_translate(dir_host_path, dir_virtual_path, sizeof(dir_virtual_path));
    if (ret != 0) {
        return ret;
    }

    ret = vfs_join_virtual_path(dir_virtual_path, vpath, joined_virtual_path, sizeof(joined_virtual_path));
    if (ret != 0) {
        return ret;
    }

    return vfs_normalize_linux_path(joined_virtual_path, resolved_vpath, resolved_vpath_len);
}

int vfs_translate_path_task(const char *vpath, char *host_path, size_t host_path_len,
                            struct fs_struct *fs) {
    char resolved_virtual[MAX_PATH];
    int ret;

    if (!vpath || !host_path || host_path_len == 0) {
        return -EINVAL;
    }

    ret = vfs_resolve_virtual_path_task(vpath, resolved_virtual, sizeof(resolved_virtual), fs);
    if (ret < 0) {
        return ret;
    }

    return vfs_join_host_root(resolved_virtual, host_path, host_path_len);
}

int vfs_translate_path_at(int dirfd, const char *vpath, char *host_path, size_t host_path_len) {
    char resolved_virtual[MAX_PATH];
    int ret;

    if (!vpath || !host_path || host_path_len == 0) {
        return -EINVAL;
    }

    ret = vfs_resolve_virtual_path_at(dirfd, vpath, resolved_virtual, sizeof(resolved_virtual));
    if (ret < 0) {
        return ret;
    }

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
    if (!pathname || !statbuf) {
        return -EFAULT;
    }
    if (host_stat_impl(pathname, statbuf) != 0) {
        return -errno;
    }
    return 0;
}

int vfs_lstat(const char *pathname, struct stat *statbuf) {
    if (!pathname || !statbuf) {
        return -EFAULT;
    }
    if (host_lstat_impl(pathname, statbuf) != 0) {
        return -errno;
    }
    return 0;
}

int vfs_access(const char *pathname, int mode) {
    if (!pathname) {
        return -EFAULT;
    }
    if (host_access_impl(pathname, mode) != 0) {
        return -errno;
    }
    return 0;
}

int vfs_fstatat(int dirfd, const char *pathname, struct stat *statbuf, int flags) {
    char translated_path[MAX_PATH];
    int ret;
    bool follow_symlink;
    int supported_flags = AT_SYMLINK_NOFOLLOW;

    if (!pathname || !statbuf) {
        return -EFAULT;
    }

    if (flags & ~supported_flags) {
        return -EINVAL;
    }

    follow_symlink = !(flags & AT_SYMLINK_NOFOLLOW);

    ret = vfs_translate_path_at(dirfd, pathname, translated_path, sizeof(translated_path));
    if (ret != 0) {
        return ret;
    }

    if (follow_symlink) {
        return vfs_stat_path(translated_path, statbuf);
    } else {
        return vfs_lstat(translated_path, statbuf);
    }
}

int vfs_faccessat(int dirfd, const char *pathname, int mode, int flags) {
    char translated_path[MAX_PATH];
    int ret;

    if (!pathname) {
        return -EFAULT;
    }

    if (flags & ~(AT_EACCESS | AT_SYMLINK_NOFOLLOW)) {
        return -EINVAL;
    }

    if (flags & AT_EACCESS) {
        return -ENOTSUP;
    }

    ret = vfs_translate_path_at(dirfd, pathname, translated_path, sizeof(translated_path));
    if (ret != 0) {
        return ret;
    }

    if (flags & AT_SYMLINK_NOFOLLOW) {
        return -ENOTSUP;
    }

    return vfs_access(translated_path, mode);
}
