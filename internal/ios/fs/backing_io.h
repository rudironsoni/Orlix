#ifndef IXLAND_INTERNAL_IOS_FS_BACKING_IO_H
#define IXLAND_INTERNAL_IOS_FS_BACKING_IO_H

#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <time.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef pthread_mutex_t ix_mutex_t;
typedef pthread_cond_t ix_cond_t;
typedef pthread_t ix_thread_t;
typedef pthread_once_t ix_once_t;
typedef pthread_attr_t ix_thread_attr_t;
typedef sigset_t ix_sigset_t;

#define IX_MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER
#define IX_COND_INITIALIZER PTHREAD_COND_INITIALIZER
#define IX_ONCE_INIT PTHREAD_ONCE_INIT
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

static inline int ix_cond_init_impl(ix_cond_t *cond) {
    return pthread_cond_init(cond, NULL);
}

static inline int ix_cond_destroy_impl(ix_cond_t *cond) {
    return pthread_cond_destroy(cond);
}

static inline int ix_cond_wait_impl(ix_cond_t *cond, ix_mutex_t *mutex) {
    return pthread_cond_wait(cond, mutex);
}

static inline int ix_cond_broadcast_impl(ix_cond_t *cond) {
    return pthread_cond_broadcast(cond);
}

static inline int ix_thread_attr_init_impl(ix_thread_attr_t *attr) {
    return pthread_attr_init(attr);
}

static inline int ix_thread_attr_destroy_impl(ix_thread_attr_t *attr) {
    return pthread_attr_destroy(attr);
}

static inline int ix_thread_attr_setstacksize_impl(ix_thread_attr_t *attr, size_t stacksize) {
    return pthread_attr_setstacksize(attr, stacksize);
}

static inline int ix_thread_create_impl(ix_thread_t *thread, const ix_thread_attr_t *attr,
                                        void *(*start_routine)(void *), void *arg) {
    return pthread_create(thread, attr, start_routine, arg);
}

static inline int ix_thread_detach_impl(ix_thread_t thread) {
    return pthread_detach(thread);
}

static inline ix_thread_t ix_thread_self_impl(void) {
    return pthread_self();
}

static inline void ix_thread_exit_impl(void *value_ptr) {
    pthread_exit(value_ptr);
}

static inline int ix_once_impl(ix_once_t *once_control, void (*init_routine)(void)) {
    return pthread_once(once_control, init_routine);
}

static inline int ix_sigemptyset_impl(ix_sigset_t *set) {
    return sigemptyset(set);
}

static inline int ix_sigaddset_impl(ix_sigset_t *set, int signo) {
    return sigaddset(set, signo);
}

static inline int ix_sigismember_impl(const ix_sigset_t *set, int signo) {
    return sigismember(set, signo);
}

static inline int ix_thread_sigmask_impl(int how, const ix_sigset_t *set, ix_sigset_t *oldset) {
    return pthread_sigmask(how, set, oldset);
}

static inline int ix_clock_gettime_impl(clockid_t clock_id, struct timespec *tp) {
    return clock_gettime(clock_id, tp);
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
int host_truncate_impl(const char *path, off_t length);
int host_ftruncate_impl(int fd, off_t length);
int host_ensure_directory_impl(const char *path, mode_t mode);

#ifdef __cplusplus
}
#endif

#endif
