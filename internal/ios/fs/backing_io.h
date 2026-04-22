#ifndef IXLAND_INTERNAL_IOS_FS_BACKING_IO_H
#define IXLAND_INTERNAL_IOS_FS_BACKING_IO_H

#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef pthread_mutex_t ix_mutex_t;

#define IX_MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER
#define IX_STDIN_FILENO STDIN_FILENO
#define IX_STDOUT_FILENO STDOUT_FILENO
#define IX_STDERR_FILENO STDERR_FILENO
#define IX_AT_FDCWD AT_FDCWD

static inline int ix_mutex_init_impl(ix_mutex_t *mutex) {
    return pthread_mutex_init(mutex, NULL);
}

static inline int ix_mutex_destroy_impl(ix_mutex_t *mutex) {
    return pthread_mutex_destroy(mutex);
}

static inline int ix_mutex_lock_impl(ix_mutex_t *mutex) {
    return pthread_mutex_lock(mutex);
}

static inline int ix_mutex_unlock_impl(ix_mutex_t *mutex) {
    return pthread_mutex_unlock(mutex);
}

/* Host container path discovery - quarantined in iOS bridge */
int vfs_discover_persistent_root(char *path, size_t path_len);
int vfs_discover_cache_root(char *path, size_t path_len);
int vfs_discover_temp_root(char *path, size_t path_len);

/* Host filesystem operations via direct syscalls */
int host_open_impl(const char *path, int flags, mode_t mode);
int host_close_impl(int fd);
int host_dup_impl(int fd);
int host_stat_impl(const char *path, struct stat *statbuf);
int host_lstat_impl(const char *path, struct stat *statbuf);
int host_access_impl(const char *path, int mode);
int host_fstat_impl(int fd, struct stat *statbuf);
ssize_t host_read_impl(int fd, void *buf, size_t count);
ssize_t host_write_impl(int fd, const void *buf, size_t count);
off_t host_lseek_impl(int fd, off_t offset, int whence);
ssize_t host_pread_impl(int fd, void *buf, size_t count, off_t offset);
ssize_t host_pwrite_impl(int fd, const void *buf, size_t count, off_t offset);
ssize_t host_readv_impl(int fd, const struct iovec *iov, int iovcnt);
ssize_t host_writev_impl(int fd, const struct iovec *iov, int iovcnt);
int host_poll_impl(struct pollfd *fds, nfds_t nfds, int timeout);
int host_ioctl_impl(int fd, unsigned long request, void *arg);
int host_ensure_directory_impl(const char *path, mode_t mode);

#ifdef __cplusplus
}
#endif

#endif
