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

/* Forward declarations */
struct file;
struct files_struct;

/* Linux-compatible file structure */
struct file {
    int fd;
    int real_fd;
    unsigned int flags;
    off_t pos;
    void *private_data;
    atomic_int refs;
};

/* Linux-compatible files_struct (per-task file table) */
struct files_struct {
    struct file **fd;
    size_t max_fds;
    pthread_mutex_t lock;
};

/* Files table allocation/management */
struct files_struct *alloc_files(size_t max_fds);
void free_files(struct files_struct *files);
struct files_struct *dup_files(struct files_struct *parent);

/* File structure allocation/management */
struct file *alloc_file(void);
void free_file(struct file *file);
struct file *dup_file(struct file *file);

/* FD operations */
int alloc_fd(struct files_struct *files, struct file *file);
int free_fd(struct files_struct *files, int fd);
struct file *fget(struct files_struct *files, int fd);
int dup_fd(struct files_struct *files, int oldfd);
int do_dup2(struct files_struct *files, int oldfd, int newfd);
int set_cloexec(struct files_struct *files, int fd, bool cloexec);
bool get_cloexec(struct files_struct *files, int fd);
int close_on_exec(struct files_struct *files);

/* Internal implementation helpers - NOT for external use */
void file_init_impl(void);
int alloc_fd_impl(void);
void free_fd_impl(int fd);
void *get_fd_entry_impl(int fd);
void put_fd_entry_impl(void *entry);
int get_real_fd_impl(void *entry);
int get_fd_flags_impl(void *entry);
void set_fd_flags_impl(void *entry, int flags);
off_t get_fd_offset_impl(void *entry);
void set_fd_offset_impl(void *entry, off_t offset);
void init_fd_entry_impl(int fd, int real_fd, int flags, mode_t mode, const char *path);
void clone_fd_entry_impl(int newfd, int oldfd);
int close_impl(int fd);

#ifdef __cplusplus
}
#endif

#endif
