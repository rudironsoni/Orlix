#ifndef IXLAND_INTERNAL_IOS_FS_SYNC_H
#define IXLAND_INTERNAL_IOS_FS_SYNC_H

#include <pthread.h>
#include <signal.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef pthread_mutex_t fs_mutex_t;
typedef pthread_cond_t fs_cond_t;
typedef sigset_t fs_sigset_t;

#define FS_MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER
#define FS_COND_INITIALIZER PTHREAD_COND_INITIALIZER

static inline int fs_mutex_init(fs_mutex_t *mutex) {
    return pthread_mutex_init(mutex, NULL);
}

static inline int fs_mutex_destroy(fs_mutex_t *mutex) {
    return pthread_mutex_destroy(mutex);
}

static inline int fs_mutex_lock(fs_mutex_t *mutex) {
    return pthread_mutex_lock(mutex);
}

static inline int fs_mutex_unlock(fs_mutex_t *mutex) {
    return pthread_mutex_unlock(mutex);
}

static inline int fs_cond_init(fs_cond_t *cond) {
    return pthread_cond_init(cond, NULL);
}

static inline int fs_cond_destroy(fs_cond_t *cond) {
    return pthread_cond_destroy(cond);
}

static inline int fs_cond_wait(fs_cond_t *cond, fs_mutex_t *mutex) {
    return pthread_cond_wait(cond, mutex);
}

static inline int fs_cond_broadcast(fs_cond_t *cond) {
    return pthread_cond_broadcast(cond);
}

static inline int fs_thread_sigmask(int how, const fs_sigset_t *set, fs_sigset_t *oldset) {
    return pthread_sigmask(how, set, oldset);
}

#ifdef __cplusplus
}
#endif

#endif
