#include "fdtable.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "internal/ios/fs/backing_io.h"

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
    pthread_mutex_init(&files->lock, NULL);

    return files;
}

void free_files(struct files_struct *files) {
    if (!files)
        return;

    pthread_mutex_lock(&files->lock);
    for (size_t i = 0; i < files->max_fds; i++) {
        if (files->fd[i]) {
            free_file(files->fd[i]);
        }
    }
    pthread_mutex_unlock(&files->lock);

    free(files->fd);
    pthread_mutex_destroy(&files->lock);
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

    pthread_mutex_lock(&parent->lock);
    for (size_t i = 0; i < parent->max_fds; i++) {
        if (parent->fd[i]) {
            child->fd[i] = dup_file(parent->fd[i]);
            if (!child->fd[i]) {
                pthread_mutex_unlock(&parent->lock);
                free_files(child);
                errno = ENOMEM;
                return NULL;
            }
        }
    }
    pthread_mutex_unlock(&parent->lock);

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

    pthread_mutex_lock(&files->lock);
    for (size_t i = 0; i < files->max_fds; i++) {
        if (!files->fd[i]) {
            files->fd[i] = file;
            pthread_mutex_unlock(&files->lock);
            return (int)i;
        }
    }
    pthread_mutex_unlock(&files->lock);

    errno = EMFILE;
    return -1;
}

int free_fd(struct files_struct *files, int fd) {
    if (!files || fd < 0 || (size_t)fd >= files->max_fds) {
        errno = EBADF;
        return -1;
    }

    pthread_mutex_lock(&files->lock);
    struct file *file = files->fd[fd];
    if (!file) {
        pthread_mutex_unlock(&files->lock);
        errno = EBADF;
        return -1;
    }

    files->fd[fd] = NULL;
    free_file(file);
    pthread_mutex_unlock(&files->lock);

    return 0;
}

struct file *fget(struct files_struct *files, int fd) {
    if (!files || fd < 0 || (size_t)fd >= files->max_fds) {
        errno = EBADF;
        return NULL;
    }

    pthread_mutex_lock(&files->lock);
    struct file *file = files->fd[fd];
    pthread_mutex_unlock(&files->lock);

    return file;
}

int dup_fd(struct files_struct *files, int oldfd) {
    if (!files || oldfd < 0 || (size_t)oldfd >= files->max_fds) {
        errno = EBADF;
        return -1;
    }

    pthread_mutex_lock(&files->lock);
    struct file *file = files->fd[oldfd];
    if (!file) {
        pthread_mutex_unlock(&files->lock);
        errno = EBADF;
        return -1;
    }

    for (size_t i = 0; i < files->max_fds; i++) {
        if (!files->fd[i]) {
            files->fd[i] = file;
            atomic_fetch_add(&file->refs, 1);
            pthread_mutex_unlock(&files->lock);
            return (int)i;
        }
    }
    pthread_mutex_unlock(&files->lock);

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

    pthread_mutex_lock(&files->lock);
    struct file *file = files->fd[oldfd];
    if (!file) {
        pthread_mutex_unlock(&files->lock);
        errno = EBADF;
        return -1;
    }

    if (files->fd[newfd]) {
        free_file(files->fd[newfd]);
    }

    files->fd[newfd] = file;
    atomic_fetch_add(&file->refs, 1);
    pthread_mutex_unlock(&files->lock);

    return 0;
}

int set_cloexec(struct files_struct *files, int fd, bool cloexec) {
    if (!files || fd < 0 || (size_t)fd >= files->max_fds) {
        errno = EBADF;
        return -1;
    }

    pthread_mutex_lock(&files->lock);
    struct file *file = files->fd[fd];
    if (!file) {
        pthread_mutex_unlock(&files->lock);
        errno = EBADF;
        return -1;
    }

    if (cloexec) {
        file->fd_flags |= FD_CLOEXEC;
    } else {
        file->fd_flags &= ~FD_CLOEXEC;
    }
    pthread_mutex_unlock(&files->lock);

    return 0;
}

bool get_cloexec(struct files_struct *files, int fd) {
    if (!files || fd < 0 || (size_t)fd >= files->max_fds) {
        return false;
    }

    pthread_mutex_lock(&files->lock);
    struct file *file = files->fd[fd];
    bool cloexec = false;
    if (file) {
        cloexec = (file->fd_flags & FD_CLOEXEC) != 0;
    }
    pthread_mutex_unlock(&files->lock);

    return cloexec;
}

int close_on_exec(struct files_struct *files) {
    if (!files) {
        errno = EINVAL;
        return -1;
    }

    int closed = 0;
    pthread_mutex_lock(&files->lock);
    for (size_t i = 0; i < files->max_fds; i++) {
        if (files->fd[i] && (files->fd[i]->fd_flags & FD_CLOEXEC)) {
            free_file(files->fd[i]);
            files->fd[i] = NULL;
            closed++;
        }
    }
    pthread_mutex_unlock(&files->lock);

    return closed;
}

/* ============================================================================
 * SINGLE STATIC FD TABLE (for host-mediated FDs)
 * This is the internal implementation - external code should use the API above
 * ============================================================================ */

typedef struct fd_description {
    int fd;
    int flags;
    mode_t mode;
    off_t offset;
    char path[MAX_PATH];
    bool is_dir;
    atomic_int refs;
    pthread_mutex_t lock;
} fd_description_t;

typedef struct {
    fd_description_t *desc;
    int fd_flags;
    bool used;
    pthread_mutex_t lock;
} fd_entry_t;

static fd_entry_t fd_table[NR_OPEN_DEFAULT];
static pthread_mutex_t fd_table_lock = PTHREAD_MUTEX_INITIALIZER;
static atomic_int fd_table_initialized = 0;

static fd_description_t *alloc_fd_description(int real_fd, int flags, mode_t mode, const char *path) {
    fd_description_t *desc = calloc(1, sizeof(fd_description_t));
    if (!desc) {
        errno = ENOMEM;
        return NULL;
    }

    desc->fd = real_fd;
    desc->flags = flags;
    desc->mode = mode;
    desc->offset = 0;
    desc->is_dir = (flags & O_DIRECTORY) != 0;
    atomic_init(&desc->refs, 1);
    pthread_mutex_init(&desc->lock, NULL);
    if (path) {
        strncpy(desc->path, path, MAX_PATH - 1);
        desc->path[MAX_PATH - 1] = '\0';
    }

    struct stat file_stat;
    if ((flags & O_DIRECTORY) == 0 && host_fstat_impl(real_fd, &file_stat) == 0) {
        desc->is_dir = S_ISDIR(file_stat.st_mode);
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
        host_close_impl(desc->fd);
        pthread_mutex_destroy(&desc->lock);
        free(desc);
    }
}

void file_init_impl(void) {
    if (atomic_exchange(&fd_table_initialized, 1) == 1) {
        return;
    }

    pthread_mutex_lock(&fd_table_lock);
    memset(fd_table, 0, sizeof(fd_table));
    for (int i = 0; i < NR_OPEN_DEFAULT; i++) {
        pthread_mutex_init(&fd_table[i].lock, NULL);
    }

    fd_table[STDIN_FILENO].used = true;
    fd_table[STDIN_FILENO].desc = alloc_fd_description(STDIN_FILENO, O_RDONLY, 0, "/dev/stdin");

    fd_table[STDOUT_FILENO].used = true;
    fd_table[STDOUT_FILENO].desc = alloc_fd_description(STDOUT_FILENO, O_WRONLY, 0, "/dev/stdout");

    fd_table[STDERR_FILENO].used = true;
    fd_table[STDERR_FILENO].desc = alloc_fd_description(STDERR_FILENO, O_WRONLY, 0, "/dev/stderr");

    pthread_mutex_unlock(&fd_table_lock);
}

int alloc_fd_impl(void) {
    file_init_impl();
    pthread_mutex_lock(&fd_table_lock);

    for (int i = 3; i < NR_OPEN_DEFAULT; i++) {
        if (!fd_table[i].used) {
            fd_table[i].used = true;
            fd_table[i].desc = NULL;
            fd_table[i].fd_flags = 0;
            pthread_mutex_unlock(&fd_table_lock);
            return i;
        }
    }

    pthread_mutex_unlock(&fd_table_lock);
    errno = EMFILE;
    return -1;
}

void free_fd_impl(int fd) {
    file_init_impl();
    if (fd < 0 || fd >= NR_OPEN_DEFAULT || fd <= STDERR_FILENO) {
        return;
    }

    pthread_mutex_lock(&fd_table_lock);
    if (fd_table[fd].used) {
        fd_description_t *desc = fd_table[fd].desc;
        fd_table[fd].desc = NULL;
        fd_table[fd].fd_flags = 0;
        fd_table[fd].used = false;
        pthread_mutex_unlock(&fd_table_lock);
        release_fd_description(desc);
        return;
    }
    pthread_mutex_unlock(&fd_table_lock);
}

void *get_fd_entry_impl(int fd) {
    file_init_impl();
    if (fd < 0 || fd >= NR_OPEN_DEFAULT) {
        return NULL;
    }

    pthread_mutex_lock(&fd_table_lock);
    fd_entry_t *entry = fd_table[fd].used ? &fd_table[fd] : NULL;
    if (entry) {
        pthread_mutex_lock(&entry->lock);
    }
    pthread_mutex_unlock(&fd_table_lock);
    return entry;
}

void put_fd_entry_impl(void *entry) {
    if (entry) {
        pthread_mutex_unlock(&((fd_entry_t *)entry)->lock);
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

bool get_fd_is_dir_impl(void *entry) {
    fd_entry_t *fd_entry = (fd_entry_t *)entry;
    return fd_entry->desc ? fd_entry->desc->is_dir : false;
}

int get_fd_path_impl(void *entry, char *path, size_t path_len) {
    fd_entry_t *fd_entry = (fd_entry_t *)entry;
    size_t len;

    if (!fd_entry || !fd_entry->desc || !path || path_len == 0) {
        errno = EINVAL;
        return -1;
    }

    len = strlen(fd_entry->desc->path);
    if (len >= path_len) {
        errno = ENAMETOOLONG;
        return -1;
    }

    memcpy(path, fd_entry->desc->path, len + 1);
    return 0;
}

void set_fd_flags_impl(void *entry, int flags) {
    fd_entry_t *fd_entry = (fd_entry_t *)entry;
    if (fd_entry->desc) {
        fd_entry->desc->flags = flags;
    }
}

void set_fd_descriptor_flags_impl(void *entry, int flags) {
    ((fd_entry_t *)entry)->fd_flags = flags;
}

off_t get_fd_offset_impl(void *entry) {
    fd_entry_t *fd_entry = (fd_entry_t *)entry;
    return fd_entry->desc ? fd_entry->desc->offset : -1;
}

void set_fd_offset_impl(void *entry, off_t offset) {
    fd_entry_t *fd_entry = (fd_entry_t *)entry;
    if (fd_entry->desc) {
        fd_entry->desc->offset = offset;
    }
}

bool get_fd_is_append_impl(void *entry) {
    fd_entry_t *fd_entry = (fd_entry_t *)entry;
    return fd_entry->desc && (fd_entry->desc->flags & O_APPEND);
}

void init_fd_entry_impl(int fd, int real_fd, int flags, mode_t mode, const char *path) {
    file_init_impl();
    fd_entry_t *entry = &fd_table[fd];
    pthread_mutex_lock(&entry->lock);
    entry->desc = alloc_fd_description(real_fd, flags, mode, path);
    entry->fd_flags = (flags & O_CLOEXEC) ? FD_CLOEXEC : 0;
    pthread_mutex_unlock(&entry->lock);
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

    pthread_mutex_lock(&fd_table_lock);
    if (!fd_table[oldfd].used || !fd_table[oldfd].desc) {
        pthread_mutex_unlock(&fd_table_lock);
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
    pthread_mutex_unlock(&fd_table_lock);

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

    pthread_mutex_lock(&fd_table_lock);
    if (!fd_table[oldfd].used || !fd_table[oldfd].desc) {
        pthread_mutex_unlock(&fd_table_lock);
        errno = EBADF;
        return -1;
    }

    old_desc = fd_table[oldfd].desc;
    retain_fd_description(old_desc);

    new_desc = fd_table[newfd].used ? fd_table[newfd].desc : NULL;
    fd_table[newfd].used = true;
    fd_table[newfd].desc = old_desc;
    fd_table[newfd].fd_flags = cloexec ? FD_CLOEXEC : 0;
    pthread_mutex_unlock(&fd_table_lock);

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
