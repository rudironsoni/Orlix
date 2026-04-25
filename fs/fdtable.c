#include "fdtable.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "include/ixland/fcntl_constants.h"

#include "internal/ios/fs/sync.h"
#include "internal/ios/fs/backing_io_decls.h"
#include "pty.h"

struct files_struct *alloc_files(size_t max_fds) {
    if (max_fds == 0) {
        errno = EINVAL;
        return NULL;
    }

    struct files_struct *files = calloc(1, sizeof(struct files_struct));
    if (!files) {
        errno = ENOMEM;
        return NULL;
    }

    files->fd = calloc(max_fds, sizeof(struct file *));
    if (!files->fd) {
        free(files);
        errno = ENOMEM;
        return NULL;
    }

    files->max_fds = max_fds;
    fs_mutex_init(&files->lock);

    return files;
}

void free_files(struct files_struct *files) {
    if (!files)
        return;

    fs_mutex_lock(&files->lock);
    for (size_t i = 0; i < files->max_fds; i++) {
        if (files->fd[i]) {
            free_file(files->fd[i]);
        }
    }
    fs_mutex_unlock(&files->lock);

    free(files->fd);
    fs_mutex_destroy(&files->lock);
    free(files);
}

struct files_struct *dup_files(struct files_struct *parent) {
    if (!parent) {
        errno = EINVAL;
        return NULL;
    }

    struct files_struct *child = alloc_files(parent->max_fds);
    if (!child)
        return NULL;

    fs_mutex_lock(&parent->lock);
    for (size_t i = 0; i < parent->max_fds; i++) {
        if (parent->fd[i]) {
            child->fd[i] = dup_file(parent->fd[i]);
            if (!child->fd[i]) {
                fs_mutex_unlock(&parent->lock);
                free_files(child);
                errno = ENOMEM;
                return NULL;
            }
        }
    }
    fs_mutex_unlock(&parent->lock);

    return child;
}

struct file *alloc_file(void) {
    struct file *file = calloc(1, sizeof(struct file));
    if (file) {
        atomic_init(&file->refs, 1);
    }
    return file;
}

void free_file(struct file *file) {
    if (!file)
        return;
    if (atomic_fetch_sub(&file->refs, 1) == 1) {
        free(file);
    }
}

struct file *dup_file(struct file *file) {
    if (!file)
        return NULL;
    atomic_fetch_add(&file->refs, 1);
    return file;
}

int alloc_fd(struct files_struct *files, struct file *file) {
    if (!files || !file) {
        errno = EINVAL;
        return -1;
    }

    fs_mutex_lock(&files->lock);
    for (size_t i = 0; i < files->max_fds; i++) {
        if (!files->fd[i]) {
            files->fd[i] = file;
            fs_mutex_unlock(&files->lock);
            return (int)i;
        }
    }
    fs_mutex_unlock(&files->lock);

    errno = EMFILE;
    return -1;
}

int free_fd(struct files_struct *files, int fd) {
    if (!files || fd < 0 || (size_t)fd >= files->max_fds) {
        errno = EBADF;
        return -1;
    }

    fs_mutex_lock(&files->lock);
    struct file *file = files->fd[fd];
    if (!file) {
        fs_mutex_unlock(&files->lock);
        errno = EBADF;
        return -1;
    }

    files->fd[fd] = NULL;
    free_file(file);
    fs_mutex_unlock(&files->lock);

    return 0;
}

struct file *fget(struct files_struct *files, int fd) {
    if (!files || fd < 0 || (size_t)fd >= files->max_fds) {
        errno = EBADF;
        return NULL;
    }

    fs_mutex_lock(&files->lock);
    struct file *file = files->fd[fd];
    fs_mutex_unlock(&files->lock);

    return file;
}

int dup_fd(struct files_struct *files, int oldfd) {
    if (!files || oldfd < 0 || (size_t)oldfd >= files->max_fds) {
        errno = EBADF;
        return -1;
    }

    fs_mutex_lock(&files->lock);
    struct file *file = files->fd[oldfd];
    if (!file) {
        fs_mutex_unlock(&files->lock);
        errno = EBADF;
        return -1;
    }

    for (size_t i = 0; i < files->max_fds; i++) {
        if (!files->fd[i]) {
            files->fd[i] = file;
            atomic_fetch_add(&file->refs, 1);
            fs_mutex_unlock(&files->lock);
            return (int)i;
        }
    }
    fs_mutex_unlock(&files->lock);

    errno = EMFILE;
    return -1;
}

int do_dup2(struct files_struct *files, int oldfd, int newfd) {
    if (!files || oldfd < 0 || newfd < 0 || (size_t)oldfd >= files->max_fds ||
        (size_t)newfd >= files->max_fds) {
        errno = EBADF;
        return -1;
    }

    // dup2 to same FD is a no-op
    if (oldfd == newfd) {
        return 0;
    }

    fs_mutex_lock(&files->lock);
    struct file *file = files->fd[oldfd];
    if (!file) {
        fs_mutex_unlock(&files->lock);
        errno = EBADF;
        return -1;
    }

    if (files->fd[newfd]) {
        free_file(files->fd[newfd]);
    }

    files->fd[newfd] = file;
    atomic_fetch_add(&file->refs, 1);
    fs_mutex_unlock(&files->lock);

    return 0;
}

int set_cloexec(struct files_struct *files, int fd, bool cloexec) {
    if (!files || fd < 0 || (size_t)fd >= files->max_fds) {
        errno = EBADF;
        return -1;
    }

    fs_mutex_lock(&files->lock);
    struct file *file = files->fd[fd];
    if (!file) {
        fs_mutex_unlock(&files->lock);
        errno = EBADF;
        return -1;
    }

    if (cloexec) {
        file->fd_flags |= FD_CLOEXEC;
    } else {
        file->fd_flags &= ~FD_CLOEXEC;
    }
    fs_mutex_unlock(&files->lock);

    return 0;
}

bool get_cloexec(struct files_struct *files, int fd) {
    if (!files || fd < 0 || (size_t)fd >= files->max_fds) {
        return false;
    }

    fs_mutex_lock(&files->lock);
    struct file *file = files->fd[fd];
    bool cloexec = false;
    if (file) {
        cloexec = (file->fd_flags & FD_CLOEXEC) != 0;
    }
    fs_mutex_unlock(&files->lock);

    return cloexec;
}

int close_on_exec(struct files_struct *files) {
    if (!files) {
        errno = EINVAL;
        return -1;
    }

    int closed = 0;
    fs_mutex_lock(&files->lock);
    for (size_t i = 0; i < files->max_fds; i++) {
        if (files->fd[i] && (files->fd[i]->fd_flags & FD_CLOEXEC)) {
            free_file(files->fd[i]);
            files->fd[i] = NULL;
            closed++;
        }
    }
    fs_mutex_unlock(&files->lock);

    return closed;
}

/* ============================================================================
 * SINGLE STATIC FD TABLE (for host-mediated FDs)
 * This is the internal implementation - external code should use the API above
 * ============================================================================ */

enum fd_type {
    FD_TYPE_HOST, /* Normal host-backed fd */
    FD_TYPE_SYNTHETIC_DIR, /* Synthetic directory (no host backing) */
    FD_TYPE_SYNTHETIC_DEV, /* Synthetic char device (no host backing) */
    FD_TYPE_SYNTHETIC_PROC_FILE, /* Synthetic proc file (no host backing) */
    FD_TYPE_SYNTHETIC_PTY /* Synthetic PTY endpoint */
};

typedef struct synthetic_dir_state {
    off_t cursor;
    bool entries_emitted;
    synthetic_dir_class_t dir_class;
} synthetic_dir_state_t;

typedef struct fd_description {
    enum fd_type type;
    int fd;
    int flags;
    linux_mode_t mode;
    off_t offset;
    char path[MAX_PATH];
    bool is_dir;
    void *synthetic_state;
    synthetic_dev_node_t dev_node;
    synthetic_proc_file_t proc_file;
    int proc_file_fd_num;
    unsigned int pty_index;
    bool pty_is_master;
    atomic_int refs;
    fs_mutex_t lock;
} fd_description_t;



static fd_entry_t fd_table[NR_OPEN_DEFAULT];
static fs_mutex_t fd_table_lock = FS_MUTEX_INITIALIZER;
static atomic_int fd_table_initialized = 0;

static fd_description_t *alloc_fd_description(int real_fd, int flags, linux_mode_t mode, const char *path) {
    fd_description_t *desc = calloc(1, sizeof(fd_description_t));
    if (!desc) {
        errno = ENOMEM;
        return NULL;
    }

    desc->type = FD_TYPE_HOST;
    desc->fd = real_fd;
    desc->flags = flags;
    desc->mode = mode;
    desc->offset = 0;
    desc->is_dir = (flags & O_DIRECTORY) != 0;
    desc->synthetic_state = NULL;
    desc->dev_node = SYNTHETIC_DEV_NONE;
    desc->proc_file = SYNTHETIC_PROC_FILE_NONE;
    desc->proc_file_fd_num = -1;
    desc->pty_index = 0;
    desc->pty_is_master = false;
    desc->synthetic_state = calloc(1, sizeof(synthetic_dir_state_t));

    if (!desc->synthetic_state) {
        free(desc);
        errno = ENOMEM;
        return NULL;
    }
    ((synthetic_dir_state_t *)desc->synthetic_state)->dir_class = SYNTHETIC_DIR_GENERIC;
    atomic_init(&desc->refs, 1);
    fs_mutex_init(&desc->lock);
    if (path) {
        strncpy(desc->path, path, MAX_PATH - 1);
        desc->path[MAX_PATH - 1] = '\0';
    }
    return desc;
}

static fd_description_t *alloc_synthetic_subdir_fd_description(int flags, linux_mode_t mode, const char *path, synthetic_dir_class_t dir_class) {
    fd_description_t *desc = calloc(1, sizeof(fd_description_t));
    if (!desc) {
        errno = ENOMEM;
        return NULL;
    }
    desc->type = FD_TYPE_SYNTHETIC_DIR;
    desc->fd = -1;
    desc->flags = flags;
    desc->mode = mode;
    desc->offset = 0;
    desc->is_dir = true;
    desc->dev_node = SYNTHETIC_DEV_NONE;
    desc->proc_file = SYNTHETIC_PROC_FILE_NONE;
    desc->proc_file_fd_num = -1;
    desc->pty_index = 0;
    desc->pty_is_master = false;
    desc->synthetic_state = calloc(1, sizeof(synthetic_dir_state_t));
    if (!desc->synthetic_state) {
        free(desc);
        errno = ENOMEM;
        return NULL;
    }
    ((synthetic_dir_state_t *)desc->synthetic_state)->dir_class = dir_class;
    atomic_init(&desc->refs, 1);
    fs_mutex_init(&desc->lock);
    if (path) {
        strncpy(desc->path, path, MAX_PATH - 1);
        desc->path[MAX_PATH - 1] = '\0';
    }
    return desc;
}

static fd_description_t *alloc_synthetic_dev_fd_description(int flags, linux_mode_t mode, const char *path, synthetic_dev_node_t dev_node) {
    fd_description_t *desc = calloc(1, sizeof(fd_description_t));
    if (!desc) {
        errno = ENOMEM;
        return NULL;
    }
    desc->type = FD_TYPE_SYNTHETIC_DEV;
    desc->fd = -1;
    desc->flags = flags;
    desc->mode = mode;
    desc->offset = 0;
    desc->is_dir = false;
    desc->dev_node = dev_node;
    desc->proc_file = SYNTHETIC_PROC_FILE_NONE;
    desc->proc_file_fd_num = -1;
    desc->pty_index = 0;
    desc->pty_is_master = false;
    desc->synthetic_state = NULL;
    atomic_init(&desc->refs, 1);
    fs_mutex_init(&desc->lock);
    if (path) {
        strncpy(desc->path, path, MAX_PATH - 1);
        desc->path[MAX_PATH - 1] = '\0';
    }
    return desc;
}

static fd_description_t *alloc_synthetic_proc_file_fd_description(int flags, linux_mode_t mode, const char *path, synthetic_proc_file_t proc_file) {
    fd_description_t *desc = calloc(1, sizeof(fd_description_t));
    if (!desc) {
        errno = ENOMEM;
        return NULL;
    }
    desc->type = FD_TYPE_SYNTHETIC_PROC_FILE;
    desc->fd = -1;
    desc->flags = flags;
    desc->mode = mode;
    desc->offset = 0;
    desc->is_dir = false;
    desc->dev_node = SYNTHETIC_DEV_NONE;
    desc->proc_file = proc_file;
    desc->proc_file_fd_num = -1;
    desc->pty_index = 0;
    desc->pty_is_master = false;
    desc->synthetic_state = NULL;
    atomic_init(&desc->refs, 1);
    fs_mutex_init(&desc->lock);
    if (path) {
        strncpy(desc->path, path, MAX_PATH - 1);
        desc->path[MAX_PATH - 1] = '\0';
    }
    return desc;
}

static fd_description_t *alloc_synthetic_pty_fd_description(int flags, linux_mode_t mode, const char *path,
                                                             unsigned int pty_index, bool is_master) {
    fd_description_t *desc = calloc(1, sizeof(fd_description_t));
    if (!desc) {
        errno = ENOMEM;
        return NULL;
    }
    desc->type = FD_TYPE_SYNTHETIC_PTY;
    desc->fd = -1;
    desc->flags = flags;
    desc->mode = mode;
    desc->offset = 0;
    desc->is_dir = false;
    desc->dev_node = SYNTHETIC_DEV_NONE;
    desc->proc_file = SYNTHETIC_PROC_FILE_NONE;
    desc->proc_file_fd_num = -1;
    desc->pty_index = pty_index;
    desc->pty_is_master = is_master;
    desc->synthetic_state = NULL;
    atomic_init(&desc->refs, 1);
    fs_mutex_init(&desc->lock);
    if (path) {
        strncpy(desc->path, path, MAX_PATH - 1);
        desc->path[MAX_PATH - 1] = '\0';
    }
    return desc;
}

static void retain_fd_description(fd_description_t *desc) {
    if (desc) {
        atomic_fetch_add(&desc->refs, 1);
    }
}

static void release_fd_description(fd_description_t *desc) {
    if (!desc) {
        return;
    }
    if (atomic_fetch_sub(&desc->refs, 1) == 1) {
        if (desc->type == FD_TYPE_HOST) {
            host_close_impl(desc->fd);
        } else if (desc->type == FD_TYPE_SYNTHETIC_PTY) {
            pty_close_end_impl(desc->pty_index, desc->pty_is_master);
        }
        if (desc->synthetic_state) {
            free(desc->synthetic_state);
        }
        fs_mutex_destroy(&desc->lock);
        free(desc);
    }
}

void file_init_impl(void) {
    if (atomic_exchange(&fd_table_initialized, 1) == 1) {
        return;
    }

    fs_mutex_lock(&fd_table_lock);
    memset(fd_table, 0, sizeof(fd_table));
    for (int i = 0; i < NR_OPEN_DEFAULT; i++) {
        fs_mutex_init(&fd_table[i].lock);
    }

    fd_table[STDIN_FILENO].used = true;
    fd_table[STDIN_FILENO].desc = alloc_fd_description(STDIN_FILENO, O_RDONLY, 0, "/dev/stdin");

    fd_table[STDOUT_FILENO].used = true;
    fd_table[STDOUT_FILENO].desc = alloc_fd_description(STDOUT_FILENO, O_WRONLY, 0, "/dev/stdout");

    fd_table[STDERR_FILENO].used = true;
    fd_table[STDERR_FILENO].desc = alloc_fd_description(STDERR_FILENO, O_WRONLY, 0, "/dev/stderr");

    fs_mutex_unlock(&fd_table_lock);
}

int alloc_fd_impl(void) {
    file_init_impl();
    fs_mutex_lock(&fd_table_lock);

    for (int i = 3; i < NR_OPEN_DEFAULT; i++) {
        if (!fd_table[i].used) {
            fd_table[i].used = true;
            fd_table[i].desc = NULL;
            fd_table[i].fd_flags = 0;
            fs_mutex_unlock(&fd_table_lock);
            return i;
        }
    }

    fs_mutex_unlock(&fd_table_lock);
    errno = EMFILE;
    return -1;
}

void free_fd_impl(int fd) {
    file_init_impl();
    if (fd < 0 || fd >= NR_OPEN_DEFAULT || fd <= STDERR_FILENO) {
        return;
    }

    fs_mutex_lock(&fd_table_lock);
    if (fd_table[fd].used) {
        fd_description_t *desc = fd_table[fd].desc;
        fd_table[fd].desc = NULL;
        fd_table[fd].fd_flags = 0;
        fd_table[fd].used = false;
        fs_mutex_unlock(&fd_table_lock);
        release_fd_description(desc);
        return;
    }
    fs_mutex_unlock(&fd_table_lock);
}

fd_entry_t *get_fd_entry_impl(int fd) {
    if (fd < 0 || fd >= NR_OPEN_DEFAULT) {
        errno = EBADF;
        return NULL;
    }

    file_init_impl();

    int ret = fs_mutex_lock(&fd_table_lock);
    if (ret != 0) {
        errno = ret;
        return NULL;
    }

    if (!fd_table[fd].used) {
        fs_mutex_unlock(&fd_table_lock);
        errno = EBADF;
        return NULL;
    }

    fd_entry_t *entry = &fd_table[fd];
    ret = fs_mutex_lock(&entry->lock);
    if (ret != 0) {
        fs_mutex_unlock(&fd_table_lock);
        errno = ret;
        return NULL;
    }

    if (!entry->used) {
        fs_mutex_unlock(&entry->lock);
        fs_mutex_unlock(&fd_table_lock);
        errno = EBADF;
        return NULL;
    }

    retain_fd_description(entry->desc);
    fs_mutex_unlock(&fd_table_lock);
    return entry;
}

void put_fd_entry_impl(void *entry) {
    if (entry) {
        fd_entry_t *fd_entry = (fd_entry_t *)entry;
        fd_description_t *desc = fd_entry->desc;
        fs_mutex_unlock(&fd_entry->lock);
        release_fd_description(desc);
    }
}

int get_real_fd_impl(void *entry) {
    fd_entry_t *fd_entry = (fd_entry_t *)entry;
    return fd_entry->desc ? fd_entry->desc->fd : -1;
}

int get_fd_flags_impl(void *entry) {
    fd_entry_t *fd_entry = (fd_entry_t *)entry;
    return fd_entry->desc ? fd_entry->desc->flags : 0;
}

int get_fd_descriptor_flags_impl(void *entry) {
    return ((fd_entry_t *)entry)->fd_flags;
}

bool get_fd_is_synthetic_dir_impl(void *entry) {
    fd_entry_t *fd_entry = (fd_entry_t *)entry;
    return fd_entry->desc && fd_entry->desc->type == FD_TYPE_SYNTHETIC_DIR;
}

bool get_fd_is_dir_impl(void *entry) {
    fd_entry_t *fd_entry = (fd_entry_t *)entry;
    return fd_entry->desc ? fd_entry->desc->is_dir : false;
}

int get_fd_path_impl(fd_entry_t *entry, char *path, size_t path_len) {
    size_t len;

    if (!entry || !entry->desc || !path || path_len == 0) {
        errno = EINVAL;
        return -1;
    }

    len = strlen(entry->desc->path);
    if (len >= path_len) {
        errno = ENAMETOOLONG;
        return -1;
    }

    memcpy(path, entry->desc->path, len + 1);
    return 0;
}

void set_fd_flags_impl(fd_entry_t *entry, int flags) {
    if (entry && entry->desc) {
        entry->desc->flags = flags;
    }
}

void set_fd_descriptor_flags_impl(fd_entry_t *entry, int flags) {
    if (entry) {
        entry->fd_flags = flags;
    }
}

off_t get_fd_offset_impl(fd_entry_t *entry) {
    return (entry && entry->desc) ? entry->desc->offset : -1;
}

void set_fd_offset_impl(fd_entry_t *entry, off_t offset) {
    if (entry && entry->desc) {
        entry->desc->offset = offset;
    }
}

bool get_fd_is_append_impl(fd_entry_t *entry) {
    return entry && entry->desc && (entry->desc->flags & O_APPEND);
}

void init_fd_entry_impl(int fd, int real_fd, int flags, linux_mode_t mode, const char *path) {
    file_init_impl();
    fd_entry_t *entry = &fd_table[fd];
    fs_mutex_lock(&entry->lock);
    entry->desc = alloc_fd_description(real_fd, flags, mode, path);
    entry->fd_flags = (flags & O_CLOEXEC) ? FD_CLOEXEC : 0;
    fs_mutex_unlock(&entry->lock);
}

int clone_fd_entry_impl(int oldfd, int minfd, bool cloexec) {
    int newfd;
    file_init_impl();
    fd_entry_t *old_entry;
    fd_description_t *desc;

    if (minfd < 0 || minfd >= NR_OPEN_DEFAULT) {
        errno = EINVAL;
        return -1;
    }

    fs_mutex_lock(&fd_table_lock);
    if (!fd_table[oldfd].used || !fd_table[oldfd].desc) {
        fs_mutex_unlock(&fd_table_lock);
        errno = EBADF;
        return -1;
    }

    old_entry = &fd_table[oldfd];
    desc = old_entry->desc;
    retain_fd_description(desc);

    newfd = -1;
    for (int i = minfd; i < NR_OPEN_DEFAULT; i++) {
        if (!fd_table[i].used) {
            fd_table[i].used = true;
            fd_table[i].desc = desc;
            fd_table[i].fd_flags = cloexec ? FD_CLOEXEC : 0;
            newfd = i;
            break;
        }
    }
    fs_mutex_unlock(&fd_table_lock);

    if (newfd < 0) {
        release_fd_description(desc);
        errno = EMFILE;
        return -1;
    }

    return newfd;
}

int replace_fd_entry_impl(int newfd, int oldfd, bool cloexec) {
    fd_description_t *old_desc;
    file_init_impl();
    fd_description_t *new_desc;

    if (newfd < 0 || newfd >= NR_OPEN_DEFAULT || oldfd < 0 || oldfd >= NR_OPEN_DEFAULT) {
        errno = EBADF;
        return -1;
    }

    fs_mutex_lock(&fd_table_lock);
    if (!fd_table[oldfd].used || !fd_table[oldfd].desc) {
        fs_mutex_unlock(&fd_table_lock);
        errno = EBADF;
        return -1;
    }

    old_desc = fd_table[oldfd].desc;
    retain_fd_description(old_desc);

    new_desc = fd_table[newfd].used ? fd_table[newfd].desc : NULL;
    fd_table[newfd].used = true;
    fd_table[newfd].desc = old_desc;
    fd_table[newfd].fd_flags = cloexec ? FD_CLOEXEC : 0;
    fs_mutex_unlock(&fd_table_lock);

    release_fd_description(new_desc);
    return newfd;
}

int close_impl(int fd) {
    file_init_impl();
    if (fd < 0 || fd >= NR_OPEN_DEFAULT) {
        errno = EBADF;
        return -1;
    }
    if (!fd_table[fd].used) {
        errno = EBADF;
        return -1;
    }
    free_fd_impl(fd);
    return 0;
}

void init_synthetic_fd_entry_impl(int fd, int flags, linux_mode_t mode, const char *path) {
    file_init_impl();
    fd_entry_t *entry = &fd_table[fd];
    fs_mutex_lock(&entry->lock);
    entry->desc = alloc_synthetic_subdir_fd_description(flags, mode, path, SYNTHETIC_DIR_GENERIC);
    entry->fd_flags = (flags & O_CLOEXEC) ? FD_CLOEXEC : 0;
    fs_mutex_unlock(&entry->lock);
}

void init_synthetic_dev_fd_entry_impl(int fd, int flags, linux_mode_t mode, const char *path, synthetic_dev_node_t dev_node) {
    file_init_impl();
    fd_entry_t *entry = &fd_table[fd];
    fs_mutex_lock(&entry->lock);
    entry->desc = alloc_synthetic_dev_fd_description(flags, mode, path, dev_node);
    entry->fd_flags = (flags & O_CLOEXEC) ? FD_CLOEXEC : 0;
    fs_mutex_unlock(&entry->lock);
}

void init_synthetic_pty_fd_entry_impl(int fd, int flags, linux_mode_t mode, const char *path,
                                      unsigned int pty_index, bool is_master) {
    file_init_impl();
    fd_entry_t *entry = &fd_table[fd];
    fs_mutex_lock(&entry->lock);
    entry->desc = alloc_synthetic_pty_fd_description(flags, mode, path, pty_index, is_master);
    entry->fd_flags = (flags & O_CLOEXEC) ? FD_CLOEXEC : 0;
    fs_mutex_unlock(&entry->lock);
}

bool get_fd_is_synthetic_dev_impl(void *entry) {
    fd_entry_t *fd_entry = (fd_entry_t *)entry;
    return fd_entry->desc && fd_entry->desc->type == FD_TYPE_SYNTHETIC_DEV;
}

synthetic_dev_node_t get_fd_synthetic_dev_node_impl(void *entry) {
    fd_entry_t *fd_entry = (fd_entry_t *)entry;
    return fd_entry->desc ? fd_entry->desc->dev_node : SYNTHETIC_DEV_NONE;
}

bool get_fd_is_synthetic_pty_impl(void *entry) {
    fd_entry_t *fd_entry = (fd_entry_t *)entry;
    return fd_entry->desc && fd_entry->desc->type == FD_TYPE_SYNTHETIC_PTY;
}

bool get_fd_is_synthetic_pty_master_impl(void *entry) {
    fd_entry_t *fd_entry = (fd_entry_t *)entry;
    return fd_entry->desc && fd_entry->desc->type == FD_TYPE_SYNTHETIC_PTY && fd_entry->desc->pty_is_master;
}

unsigned int get_fd_synthetic_pty_index_impl(void *entry) {
    fd_entry_t *fd_entry = (fd_entry_t *)entry;
    return fd_entry->desc ? fd_entry->desc->pty_index : 0;
}

void init_synthetic_proc_file_fd_entry_impl(int fd, int flags, linux_mode_t mode, const char *path, synthetic_proc_file_t proc_file) {
    file_init_impl();
    fd_entry_t *entry = &fd_table[fd];
    fs_mutex_lock(&entry->lock);
    entry->desc = alloc_synthetic_proc_file_fd_description(flags, mode, path, proc_file);
    entry->fd_flags = (flags & O_CLOEXEC) ? FD_CLOEXEC : 0;
    fs_mutex_unlock(&entry->lock);
}

void init_synthetic_proc_file_fd_entry_with_fdnum_impl(int fd, int flags, linux_mode_t mode, const char *path, synthetic_proc_file_t proc_file, int fd_num) {
    file_init_impl();
    fd_entry_t *entry = &fd_table[fd];
    fs_mutex_lock(&entry->lock);
    entry->desc = alloc_synthetic_proc_file_fd_description(flags, mode, path, proc_file);
    if (entry->desc) {
        entry->desc->proc_file_fd_num = fd_num;
    }
    entry->fd_flags = (flags & O_CLOEXEC) ? FD_CLOEXEC : 0;
    fs_mutex_unlock(&entry->lock);
}

bool get_fd_is_synthetic_proc_file_impl(void *entry) {
    fd_entry_t *fd_entry = (fd_entry_t *)entry;
    return fd_entry->desc && fd_entry->desc->type == FD_TYPE_SYNTHETIC_PROC_FILE;
}

synthetic_proc_file_t get_fd_synthetic_proc_file_impl(void *entry) {
    fd_entry_t *fd_entry = (fd_entry_t *)entry;
    return fd_entry->desc ? fd_entry->desc->proc_file : SYNTHETIC_PROC_FILE_NONE;
}

int get_fd_proc_file_fd_num_impl(void *entry) {
    fd_entry_t *fd_entry = (fd_entry_t *)entry;
    return fd_entry->desc ? fd_entry->desc->proc_file_fd_num : -1;
}

void init_synthetic_subdir_fd_entry_impl(int fd, int flags, linux_mode_t mode, const char *path, synthetic_dir_class_t dir_class) {
    file_init_impl();
    fd_entry_t *entry = &fd_table[fd];
    fs_mutex_lock(&entry->lock);
    entry->desc = alloc_synthetic_subdir_fd_description(flags, mode, path, dir_class);
    entry->fd_flags = (flags & O_CLOEXEC) ? FD_CLOEXEC : 0;
    fs_mutex_unlock(&entry->lock);
}

synthetic_dir_class_t get_fd_synthetic_dir_class_impl(void *entry) {
    fd_entry_t *fd_entry = (fd_entry_t *)entry;
    if (!fd_entry->desc || fd_entry->desc->type != FD_TYPE_SYNTHETIC_DIR || !fd_entry->desc->synthetic_state) {
        return SYNTHETIC_DIR_GENERIC;
    }
    return ((synthetic_dir_state_t *)fd_entry->desc->synthetic_state)->dir_class;
}

bool fdtable_is_used_impl(int fd) {
    if (fd < 0 || fd >= NR_OPEN_DEFAULT) {
        return false;
    }
    file_init_impl();
    fs_mutex_lock(&fd_table_lock);
    bool used = fd_table[fd].used;
    fs_mutex_unlock(&fd_table_lock);
    return used;
}
