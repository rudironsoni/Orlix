#ifndef FDTABLE_H
#define FDTABLE_H

#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <sys/types.h>

#define NR_OPEN_DEFAULT 256
#define MAX_PATH 4096

#ifdef __cplusplus
extern "C" {
#endif

struct file;
struct files_struct;

struct file {
    int fd;
    int real_fd;
    unsigned int flags;
    off_t pos;
    void *private_data;
    atomic_int refs;
};

struct files_struct {
    struct file **fd;
    size_t max_fds;
    pthread_mutex_t lock;
};

struct files_struct *alloc_files(size_t max_fds);
void free_files(struct files_struct *files);
struct files_struct *dup_files(struct files_struct *parent);

struct file *alloc_file(void);
void free_file(struct file *file);
struct file *dup_file(struct file *file);

int alloc_fd(struct files_struct *files, struct file *file);
int free_fd(struct files_struct *files, int fd);

/* Internal helpers for fd entry access */
void *get_fd_entry_impl(struct files_struct *files, int fd);
int get_real_fd_impl(void *entry);
void put_fd_entry_impl(void *entry);

#ifdef __cplusplus
}
#endif

#endif