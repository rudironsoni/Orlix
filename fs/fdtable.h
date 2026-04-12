#ifndef IXLAND_FDTABLE_H
#define IXLAND_FDTABLE_H

#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <sys/types.h>

#define IXLAND_MAX_FD 256
#define IXLAND_MAX_PATH 4096

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ixland_file ixland_file_t;
typedef struct ixland_files ixland_files_t;

struct ixland_file {
    int fd;
    int real_fd;
    unsigned int flags;
    off_t pos;
    void *vfs_file;
    atomic_int refs;
};

struct ixland_files {
    ixland_file_t **fd;
    size_t max_fds;
    pthread_mutex_t lock;
};

ixland_files_t *ixland_files_alloc(size_t max_fds);
void ixland_files_free(ixland_files_t *files);
ixland_files_t *ixland_files_dup(ixland_files_t *parent);

ixland_file_t *ixland_file_alloc(void);
void ixland_file_free(ixland_file_t *file);
ixland_file_t *ixland_file_dup(ixland_file_t *file);

int ixland_fd_alloc(ixland_files_t *files, ixland_file_t *file);
int ixland_fd_free(ixland_files_t *files, int fd);
ixland_file_t *ixland_fd_lookup(ixland_files_t *files, int fd);
int ixland_fd_dup(ixland_files_t *files, int oldfd);
int ixland_fd_dup2(ixland_files_t *files, int oldfd, int newfd);
int ixland_fd_set_cloexec(ixland_files_t *files, int fd, bool cloexec);
bool ixland_fd_get_cloexec(ixland_files_t *files, int fd);
int ixland_fd_close_cloexec(ixland_files_t *files);

/* FD table implementation helpers - used by fs/ owners */
void __ixland_file_init_impl(void);
int __ixland_alloc_fd_impl(void);
void __ixland_free_fd_impl(int fd);
void *__ixland_get_fd_entry_impl(int fd);
void __ixland_put_fd_entry_impl(void *entry);
int __ixland_get_real_fd_impl(void *entry);
int __ixland_get_fd_flags_impl(void *entry);
void __ixland_set_fd_flags_impl(void *entry, int flags);
off_t __ixland_get_fd_offset_impl(void *entry);
void __ixland_set_fd_offset_impl(void *entry, off_t offset);
void __ixland_init_fd_entry_impl(int fd, int real_fd, int flags, mode_t mode, const char *path);
void __ixland_clone_fd_entry_impl(int newfd, int oldfd);
int __ixland_close_impl(int fd);

#ifdef __cplusplus
}
#endif

#endif
