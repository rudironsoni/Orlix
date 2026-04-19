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
    unsigned int fd_flags;
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

/* ============================================================================
 * STATIC FD TABLE API (for host-mediated FDs)
 * These functions work with the internal static fd table, not files_struct
 * ============================================================================ */

/* Initialize the static fd table */
void file_init_impl(void);

/* Allocate/free slots in static fd table */
int alloc_fd_impl(void);
void free_fd_impl(int fd);

/* FD entry access - returns locked entry, must call put_fd_entry_impl to unlock */
void *get_fd_entry_impl(int fd);
void put_fd_entry_impl(void *entry);

/* Getters/setters for fd entry properties */
int get_real_fd_impl(void *entry);
int get_fd_flags_impl(void *entry);
int get_fd_descriptor_flags_impl(void *entry);
bool get_fd_is_synthetic_dir_impl(void *entry);
bool get_fd_is_dir_impl(void *entry);
int get_fd_path_impl(void *entry, char *path, size_t path_len);
void set_fd_flags_impl(void *entry, int flags);
void set_fd_descriptor_flags_impl(void *entry, int flags);
off_t get_fd_offset_impl(void *entry);
void set_fd_offset_impl(void *entry, off_t offset);
bool get_fd_is_append_impl(void *entry);
int clone_fd_entry_impl(int oldfd, int minfd, bool cloexec);
int replace_fd_entry_impl(int newfd, int oldfd, bool cloexec);

/* Initialize/clone fd entries */
void init_fd_entry_impl(int fd, int real_fd, int flags, mode_t mode, const char *path);

void init_synthetic_fd_entry_impl(int fd, int flags, mode_t mode, const char *path);

/* Close implementation using static fd table */
int close_impl(int fd);

#ifdef __cplusplus
}
#endif

#endif
