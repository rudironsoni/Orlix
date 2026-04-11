#include "fdtable.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../src/ixland/internal/ixland_internal.h"

ixland_files_t *ixland_files_alloc(size_t max_fds) {
    ixland_files_t *files = calloc(1, sizeof(ixland_files_t));
    if (!files) {
        errno = ENOMEM;
        return NULL;
    }

    files->fd = calloc(max_fds, sizeof(ixland_file_t *));
    if (!files->fd) {
        free(files);
        errno = ENOMEM;
        return NULL;
    }

    files->max_fds = max_fds;
    pthread_mutex_init(&files->lock, NULL);

    return files;
}

void ixland_files_free(ixland_files_t *files) {
    if (!files)
        return;

    pthread_mutex_lock(&files->lock);
    for (size_t i = 0; i < files->max_fds; i++) {
        if (files->fd[i]) {
            ixland_file_free(files->fd[i]);
        }
    }
    pthread_mutex_unlock(&files->lock);

    free(files->fd);
    pthread_mutex_destroy(&files->lock);
    free(files);
}

ixland_files_t *ixland_files_dup(ixland_files_t *parent) {
    if (!parent) {
        errno = EINVAL;
        return NULL;
    }

    ixland_files_t *child = ixland_files_alloc(parent->max_fds);
    if (!child)
        return NULL;

    pthread_mutex_lock(&parent->lock);
    for (size_t i = 0; i < parent->max_fds; i++) {
        if (parent->fd[i]) {
            child->fd[i] = ixland_file_dup(parent->fd[i]);
            if (!child->fd[i]) {
                pthread_mutex_unlock(&parent->lock);
                ixland_files_free(child);
                errno = ENOMEM;
                return NULL;
            }
        }
    }
    pthread_mutex_unlock(&parent->lock);

    return child;
}

ixland_file_t *ixland_file_alloc(void) {
    ixland_file_t *file = calloc(1, sizeof(ixland_file_t));
    if (file) {
        atomic_init(&file->refs, 1);
    }
    return file;
}

void ixland_file_free(ixland_file_t *file) {
    if (!file)
        return;
    if (atomic_fetch_sub(&file->refs, 1) == 1) {
        free(file);
    }
}

ixland_file_t *ixland_file_dup(ixland_file_t *file) {
    if (!file)
        return NULL;
    atomic_fetch_add(&file->refs, 1);
    return file;
}

int ixland_fd_alloc(ixland_files_t *files, ixland_file_t *file) {
    if (!files || !file) {
        errno = EINVAL;
        return -1;
    }

    pthread_mutex_lock(&files->lock);
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

int ixland_fd_free(ixland_files_t *files, int fd) {
    if (!files || fd < 0 || (size_t)fd >= files->max_fds) {
        errno = EBADF;
        return -1;
    }

    pthread_mutex_lock(&files->lock);
    ixland_file_t *file = files->fd[fd];
    if (!file) {
        pthread_mutex_unlock(&files->lock);
        errno = EBADF;
        return -1;
    }

    files->fd[fd] = NULL;
    ixland_file_free(file);
    pthread_mutex_unlock(&files->lock);

    return 0;
}

ixland_file_t *ixland_fd_lookup(ixland_files_t *files, int fd) {
    if (!files || fd < 0 || (size_t)fd >= files->max_fds) {
        errno = EBADF;
        return NULL;
    }

    pthread_mutex_lock(&files->lock);
    ixland_file_t *file = files->fd[fd];
    if (file) {
        atomic_fetch_add(&file->refs, 1);
    }
    pthread_mutex_unlock(&files->lock);

    return file;
}

typedef struct {
    int fd;
    int flags;
    mode_t mode;
    off_t offset;
    char path[IXLAND_MAX_PATH];
    bool used;
    bool is_dir;
    pthread_mutex_t lock;
} ixland_fd_entry_t;

static ixland_fd_entry_t ixland_fd_table[IXLAND_MAX_FD];
static pthread_mutex_t ixland_fd_table_lock = PTHREAD_MUTEX_INITIALIZER;
static atomic_int ixland_fd_table_initialized = 0;

void __ixland_file_init_impl(void) {
    if (atomic_exchange(&ixland_fd_table_initialized, 1) == 1) {
        return;
    }

    pthread_mutex_lock(&ixland_fd_table_lock);
    memset(ixland_fd_table, 0, sizeof(ixland_fd_table));

    ixland_fd_table[STDIN_FILENO].fd = STDIN_FILENO;
    ixland_fd_table[STDIN_FILENO].flags = O_RDONLY;
    ixland_fd_table[STDIN_FILENO].used = true;
    strncpy(ixland_fd_table[STDIN_FILENO].path, "/dev/stdin", IXLAND_MAX_PATH - 1);
    ixland_fd_table[STDIN_FILENO].path[IXLAND_MAX_PATH - 1] = '\0';
    pthread_mutex_init(&ixland_fd_table[STDIN_FILENO].lock, NULL);

    ixland_fd_table[STDOUT_FILENO].fd = STDOUT_FILENO;
    ixland_fd_table[STDOUT_FILENO].flags = O_WRONLY;
    ixland_fd_table[STDOUT_FILENO].used = true;
    strncpy(ixland_fd_table[STDOUT_FILENO].path, "/dev/stdout", IXLAND_MAX_PATH - 1);
    ixland_fd_table[STDOUT_FILENO].path[IXLAND_MAX_PATH - 1] = '\0';
    pthread_mutex_init(&ixland_fd_table[STDOUT_FILENO].lock, NULL);

    ixland_fd_table[STDERR_FILENO].fd = STDERR_FILENO;
    ixland_fd_table[STDERR_FILENO].flags = O_WRONLY;
    ixland_fd_table[STDERR_FILENO].used = true;
    strncpy(ixland_fd_table[STDERR_FILENO].path, "/dev/stderr", IXLAND_MAX_PATH - 1);
    ixland_fd_table[STDERR_FILENO].path[IXLAND_MAX_PATH - 1] = '\0';
    pthread_mutex_init(&ixland_fd_table[STDERR_FILENO].lock, NULL);

    pthread_mutex_unlock(&ixland_fd_table_lock);
}

int __ixland_alloc_fd_impl(void) {
    pthread_mutex_lock(&ixland_fd_table_lock);

    for (int i = 3; i < IXLAND_MAX_FD; i++) {
        if (!ixland_fd_table[i].used) {
            ixland_fd_table[i].used = true;
            ixland_fd_table[i].fd = -1;
            ixland_fd_table[i].offset = 0;
            pthread_mutex_init(&ixland_fd_table[i].lock, NULL);
            pthread_mutex_unlock(&ixland_fd_table_lock);
            return i;
        }
    }

    pthread_mutex_unlock(&ixland_fd_table_lock);
    errno = EMFILE;
    return -1;
}

void __ixland_free_fd_impl(int fd) {
    if (fd < 0 || fd >= IXLAND_MAX_FD || fd <= STDERR_FILENO) {
        return;
    }

    pthread_mutex_lock(&ixland_fd_table_lock);
    if (ixland_fd_table[fd].used) {
        pthread_mutex_destroy(&ixland_fd_table[fd].lock);
        memset(&ixland_fd_table[fd], 0, sizeof(ixland_fd_entry_t));
    }
    pthread_mutex_unlock(&ixland_fd_table_lock);
}

void *__ixland_get_fd_entry_impl(int fd) {
    if (fd < 0 || fd >= IXLAND_MAX_FD) {
        return NULL;
    }

    pthread_mutex_lock(&ixland_fd_table_lock);
    ixland_fd_entry_t *entry = ixland_fd_table[fd].used ? &ixland_fd_table[fd] : NULL;
    if (entry) {
        pthread_mutex_lock(&entry->lock);
    }
    pthread_mutex_unlock(&ixland_fd_table_lock);
    return entry;
}

void __ixland_put_fd_entry_impl(void *entry) {
    if (entry) {
        pthread_mutex_unlock(&((ixland_fd_entry_t *)entry)->lock);
    }
}

int __ixland_get_real_fd_impl(void *entry) {
    return ((ixland_fd_entry_t *)entry)->fd;
}

int __ixland_get_fd_flags_impl(void *entry) {
    return ((ixland_fd_entry_t *)entry)->flags;
}

void __ixland_set_fd_flags_impl(void *entry, int flags) {
    ((ixland_fd_entry_t *)entry)->flags = flags;
}

off_t __ixland_get_fd_offset_impl(void *entry) {
    return ((ixland_fd_entry_t *)entry)->offset;
}

void __ixland_set_fd_offset_impl(void *entry, off_t offset) {
    ((ixland_fd_entry_t *)entry)->offset = offset;
}

void __ixland_init_fd_entry_impl(int fd, int real_fd, int flags, mode_t mode, const char *path) {
    ixland_fd_entry_t *entry = &ixland_fd_table[fd];
    pthread_mutex_lock(&entry->lock);
    entry->fd = real_fd;
    entry->flags = flags;
    entry->mode = mode;
    entry->offset = 0;
    strncpy(entry->path, path, IXLAND_MAX_PATH - 1);
    entry->path[IXLAND_MAX_PATH - 1] = '\0';

    struct stat file_stat;
    if (fstat(real_fd, &file_stat) == 0) {
        entry->is_dir = S_ISDIR(file_stat.st_mode);
    }
    pthread_mutex_unlock(&entry->lock);
}

void __ixland_clone_fd_entry_impl(int newfd, int oldfd) {
    pthread_mutex_lock(&ixland_fd_table_lock);
    memcpy(&ixland_fd_table[newfd], &ixland_fd_table[oldfd], sizeof(ixland_fd_entry_t));
    pthread_mutex_init(&ixland_fd_table[newfd].lock, NULL);
    pthread_mutex_unlock(&ixland_fd_table_lock);
}
