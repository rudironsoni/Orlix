#include "fdtable.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

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
        file->flags |= O_CLOEXEC;
    } else {
        file->flags &= ~O_CLOEXEC;
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
        cloexec = (file->flags & O_CLOEXEC) != 0;
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
        if (files->fd[i] && (files->fd[i]->flags & O_CLOEXEC)) {
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

typedef struct {
    int fd;
    int flags;
    mode_t mode;
    off_t offset;
    char path[MAX_PATH];
    bool used;
    bool is_dir;
    pthread_mutex_t lock;
} fd_entry_t;

static fd_entry_t fd_table[NR_OPEN_DEFAULT];
static pthread_mutex_t fd_table_lock = PTHREAD_MUTEX_INITIALIZER;
static atomic_int fd_table_initialized = 0;

void file_init_impl(void) {
    if (atomic_exchange(&fd_table_initialized, 1) == 1) {
        return;
    }

    pthread_mutex_lock(&fd_table_lock);
    memset(fd_table, 0, sizeof(fd_table));

    fd_table[STDIN_FILENO].fd = STDIN_FILENO;
    fd_table[STDIN_FILENO].flags = O_RDONLY;
    fd_table[STDIN_FILENO].used = true;
    strncpy(fd_table[STDIN_FILENO].path, "/dev/stdin", MAX_PATH - 1);
    fd_table[STDIN_FILENO].path[MAX_PATH - 1] = '\0';
    pthread_mutex_init(&fd_table[STDIN_FILENO].lock, NULL);

    fd_table[STDOUT_FILENO].fd = STDOUT_FILENO;
    fd_table[STDOUT_FILENO].flags = O_WRONLY;
    fd_table[STDOUT_FILENO].used = true;
    strncpy(fd_table[STDOUT_FILENO].path, "/dev/stdout", MAX_PATH - 1);
    fd_table[STDOUT_FILENO].path[MAX_PATH - 1] = '\0';
    pthread_mutex_init(&fd_table[STDOUT_FILENO].lock, NULL);

    fd_table[STDERR_FILENO].fd = STDERR_FILENO;
    fd_table[STDERR_FILENO].flags = O_WRONLY;
    fd_table[STDERR_FILENO].used = true;
    strncpy(fd_table[STDERR_FILENO].path, "/dev/stderr", MAX_PATH - 1);
    fd_table[STDERR_FILENO].path[MAX_PATH - 1] = '\0';
    pthread_mutex_init(&fd_table[STDERR_FILENO].lock, NULL);

    pthread_mutex_unlock(&fd_table_lock);
}

int alloc_fd_impl(void) {
    pthread_mutex_lock(&fd_table_lock);

    for (int i = 3; i < NR_OPEN_DEFAULT; i++) {
        if (!fd_table[i].used) {
            fd_table[i].used = true;
            fd_table[i].fd = -1;
            fd_table[i].offset = 0;
            pthread_mutex_init(&fd_table[i].lock, NULL);
            pthread_mutex_unlock(&fd_table_lock);
            return i;
        }
    }

    pthread_mutex_unlock(&fd_table_lock);
    errno = EMFILE;
    return -1;
}

void free_fd_impl(int fd) {
    if (fd < 0 || fd >= NR_OPEN_DEFAULT || fd <= STDERR_FILENO) {
        return;
    }

    pthread_mutex_lock(&fd_table_lock);
    if (fd_table[fd].used) {
        pthread_mutex_destroy(&fd_table[fd].lock);
        memset(&fd_table[fd], 0, sizeof(fd_entry_t));
    }
    pthread_mutex_unlock(&fd_table_lock);
}

void *get_fd_entry_impl(int fd) {
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
    return ((fd_entry_t *)entry)->fd;
}

int get_fd_flags_impl(void *entry) {
    return ((fd_entry_t *)entry)->flags;
}

bool get_fd_is_dir_impl(void *entry) {
    return ((fd_entry_t *)entry)->is_dir;
}

int get_fd_path_impl(void *entry, char *path, size_t path_len) {
    fd_entry_t *fd_entry = (fd_entry_t *)entry;
    size_t len;

    if (!fd_entry || !path || path_len == 0) {
        errno = EINVAL;
        return -1;
    }

    len = strlen(fd_entry->path);
    if (len >= path_len) {
        errno = ENAMETOOLONG;
        return -1;
    }

    memcpy(path, fd_entry->path, len + 1);
    return 0;
}

void set_fd_flags_impl(void *entry, int flags) {
    ((fd_entry_t *)entry)->flags = flags;
}

off_t get_fd_offset_impl(void *entry) {
    return ((fd_entry_t *)entry)->offset;
}

void set_fd_offset_impl(void *entry, off_t offset) {
    ((fd_entry_t *)entry)->offset = offset;
}

void init_fd_entry_impl(int fd, int real_fd, int flags, mode_t mode, const char *path) {
    fd_entry_t *entry = &fd_table[fd];
    pthread_mutex_lock(&entry->lock);
    entry->fd = real_fd;
    entry->flags = flags;
    entry->mode = mode;
    entry->offset = 0;
    strncpy(entry->path, path, MAX_PATH - 1);
    entry->path[MAX_PATH - 1] = '\0';

    struct stat file_stat;
    if (fstat(real_fd, &file_stat) == 0) {
        entry->is_dir = S_ISDIR(file_stat.st_mode);
    }
    pthread_mutex_unlock(&entry->lock);
}

void clone_fd_entry_impl(int newfd, int oldfd) {
    pthread_mutex_lock(&fd_table_lock);
    memcpy(&fd_table[newfd], &fd_table[oldfd], sizeof(fd_entry_t));
    pthread_mutex_init(&fd_table[newfd].lock, NULL);
    pthread_mutex_unlock(&fd_table_lock);
}

int close_impl(int fd) {
    void *entry = get_fd_entry_impl(fd);
    if (!entry) {
        errno = EBADF;
        return -1;
    }

    int real_fd = get_real_fd_impl(entry);
    put_fd_entry_impl(entry);
    close(real_fd);
    free_fd_impl(fd);
    return 0;
}
