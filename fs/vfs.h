#ifndef VFS_H
#define VFS_H

#define _DARWIN_NO_64_BIT_INODE

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "fdtable.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_MOUNTS 64

/* VFS backing storage classes */
enum vfs_backing_class {
    VFS_BACKING_PERSISTENT = 0,
    VFS_BACKING_CACHE,
    VFS_BACKING_TEMP,
    VFS_BACKING_SYNTHETIC,
    VFS_BACKING_EXTERNAL,

    VFS_BACKING_CLASS_COUNT
};

/* Linux VFS inode types (file types)
 * These are defined in standard system headers, no need to redefine */

/* Forward declarations */
struct inode;
struct dentry;
struct file;
struct super_block;
struct file_system_type;
struct mount;
struct poll_table_struct;
struct iattr;
struct page;
struct address_space;
struct writeback_control;

/* Linux-compatible VFS operations structure */
struct file_operations {
    ssize_t (*read)(struct file *file, char *buf, size_t count, off_t *pos);
    ssize_t (*write)(struct file *file, const char *buf, size_t count, off_t *pos);
    int (*open)(struct inode *inode, struct file *file);
    int (*release)(struct inode *inode, struct file *file);
    int (*ioctl)(struct file *file, unsigned int cmd, unsigned long arg);
    int (*mmap)(struct file *file, void *addr, size_t len, int prot, int flags, off_t offset);
    unsigned int (*poll)(struct file *file, struct poll_table_struct *table);
};

/* Linux-compatible inode operations */
struct inode_operations {
    struct dentry *(*lookup)(struct inode *dir, struct dentry *dentry);
    int (*create)(struct inode *dir, struct dentry *dentry, mode_t mode);
    int (*link)(struct dentry *old_dentry, struct inode *dir, struct dentry *new_dentry);
    int (*unlink)(struct inode *dir, struct dentry *dentry);
    int (*symlink)(struct inode *dir, struct dentry *dentry, const char *oldname);
    int (*mkdir)(struct inode *dir, struct dentry *dentry, mode_t mode);
    int (*rmdir)(struct inode *dir, struct dentry *dentry);
    int (*rename)(struct inode *old_dir, struct dentry *old_dentry, struct inode *new_dir,
                  struct dentry *new_dentry);
    int (*readlink)(struct dentry *dentry, char *buf, int buflen);
    int (*setattr)(struct dentry *dentry, struct iattr *attr);
    int (*getattr)(const char *path, struct dentry *dentry, struct stat *stat);
};

/* Linux-compatible address space operations */
struct address_space_operations {
    int (*readpage)(struct file *file, struct page *page);
    int (*writepage)(struct page *page, struct writeback_control *wbc);
    int (*write_begin)(struct file *file, struct address_space *mapping, off_t pos, unsigned len,
                       unsigned flags, struct page **pagep, void **fsdata);
    int (*write_end)(struct file *file, struct address_space *mapping, off_t pos, unsigned len,
                     unsigned copied, struct page *page, void *fsdata);
};

/* Linux-compatible super block operations */
struct super_operations {
    struct inode *(*alloc_inode)(struct super_block *sb);
    void (*destroy_inode)(struct inode *inode);
    void (*dirty_inode)(struct inode *inode);
    int (*write_inode)(struct inode *inode, struct writeback_control *wbc);
    void (*evict_inode)(struct inode *inode);
    int (*statfs)(struct dentry *dentry, struct statfs *buf);
    int (*remount_fs)(struct super_block *sb, int *flags, char *data);
    void (*clear_inode)(struct inode *inode);
    void (*umount_begin)(struct super_block *sb);
};

/* Linux-compatible inode structure */
struct inode {
    uint64_t i_ino;
    unsigned int i_mode;
    uid_t i_uid;
    gid_t i_gid;
    off_t i_size;
    struct timespec i_atime;
    struct timespec i_mtime;
    struct timespec i_ctime;
    atomic_int i_count;
    void *i_private;
    struct super_block *i_sb;
    const struct inode_operations *i_op;
    const struct file_operations *i_fop;
    void *i_fspriv;
};

/* Linux-compatible dentry (directory entry) structure */
struct dentry {
    struct inode *d_inode;
    struct super_block *d_sb;
    const unsigned char *d_name;
    atomic_int d_count;
    struct dentry *d_parent;
    void *d_fsdata;
};

/* Linux-compatible file system type */
struct file_system_type {
    const char *name;
    int (*mount)(struct file_system_type *fs_type, int flags, const char *dev_name, void *data,
                  struct dentry *mnt_root);
    void (*kill_sb)(struct super_block *sb);
    struct module *owner;
};

/* Linux-compatible mount point */
struct mount {
    struct dentry *mnt_root;
    struct super_block *mnt_sb;
    int mnt_flags;
    char mnt_devname[MAX_PATH];
    atomic_int mnt_count;
    atomic_int mnt_ondie;
    struct mount *mnt_parent;
};

/* Linux-compatible fs context (per-task filesystem context)
 * Stores virtual root and pwd as char arrays for task-aware path resolution */
struct fs_struct {
    struct dentry *root;
    struct dentry *pwd;
    mode_t umask;
    atomic_int users;
    pthread_mutex_t lock;
    /* Task-aware path resolution state */
    char root_path[MAX_PATH];      /* Virtual root path (absolute, normalized) */
    char pwd_path[MAX_PATH];       /* Virtual pwd path (absolute, normalized) */
};

/* VFS context API */
struct fs_struct *alloc_fs_struct(void);
void free_fs_struct(struct fs_struct *fs);
struct fs_struct *dup_fs_struct(struct fs_struct *old);
int fs_init_root(struct fs_struct *fs, const char *root_path);
int fs_init_pwd(struct fs_struct *fs, const char *pwd_path);
int fs_set_pwd(struct fs_struct *fs, const char *new_pwd);
int fs_set_root(struct fs_struct *fs, const char *new_root);

/* VFS initialization */
int vfs_init(void);
void vfs_deinit(void);

/* Mount operations */
int vfs_kern_mount(struct file_system_type *type, int flags, const char *dev_name, void *data);
int vfs_mount(const char *source, const char *target, const char *fstype, unsigned long flags,
              const void *data);
int vfs_umount(const char *target);
int vfs_mount_basic(void);

/* Path lookup and walk */
int vfs_lookup(const char *path, struct dentry **dentry);
int vfs_path_walk(const char *path, struct dentry **dentry);
int vfs_mkdir(const char *path, mode_t mode);
int vfs_unlink(const char *path);
int vfs_rmdir(const char *path);

/* File operations through VFS */
int vfs_open(const char *path, int flags, mode_t mode, int *target_fd);
int vfs_close(struct file *file);

/* Task-aware path translation between virtual and host paths */
int vfs_translate_path(const char *vpath, char *host_path, size_t host_path_len);
int vfs_translate_path_task(const char *vpath, char *host_path, size_t host_path_len,
                            struct fs_struct *fs);
int vfs_translate_path_at(int dirfd, const char *vpath, char *host_path, size_t host_path_len);
int vfs_resolve_virtual_path_task(const char *vpath, char *resolved_vpath, size_t resolved_vpath_len,
                                  struct fs_struct *fs);
int vfs_resolve_virtual_path_at(int dirfd, const char *vpath, char *resolved_vpath,
                                size_t resolved_vpath_len);
int vfs_getcwd_path_task(struct fs_struct *fs, char *vpath, size_t vpath_len);
int vfs_normalize_linux_path(const char *input, char *output, size_t output_len);
int vfs_reverse_translate(const char *host_path, char *vpath, size_t vpath_len);
const char *vfs_host_backing_root(void);
const char *vfs_virtual_root(void);

/* Backing class determination for storage policy routing */
enum vfs_backing_class vfs_backing_class_for_path(const char *vpath);
const char *vfs_backing_root_for_class(enum vfs_backing_class cls);

/* Backing root accessors for different storage classes */
const char *vfs_persistent_backing_root(void);
const char *vfs_cache_backing_root(void);
const char *vfs_temp_backing_root(void);

/* Stat operations */
int vfs_stat_path(const char *pathname, struct stat *statbuf);
int vfs_lstat(const char *pathname, struct stat *statbuf);
int vfs_access(const char *pathname, int mode);
int vfs_fstatat(int dirfd, const char *pathname, struct stat *statbuf, int flags);
int vfs_faccessat(int dirfd, const char *pathname, int mode, int flags);

#ifdef __cplusplus
}
#endif

#endif
