#ifndef IXLAND_FDTABLE_H
#define IXLAND_FDTABLE_H

#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <sys/types.h>

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

#ifdef __cplusplus
}
#endif

#endif
