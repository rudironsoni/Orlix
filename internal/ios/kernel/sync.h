#ifndef IXLAND_INTERNAL_IOS_KERNEL_SYNC_H
#define IXLAND_INTERNAL_IOS_KERNEL_SYNC_H

#include <pthread.h>
#include <signal.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef pthread_mutex_t kernel_mutex_t;
typedef pthread_cond_t kernel_cond_t;
typedef pthread_t kernel_thread_t;
typedef pthread_once_t kernel_once_t;
typedef pthread_attr_t kernel_thread_attr_t;
typedef sigset_t kernel_sigset_t;

#define KERNEL_MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER
#define KERNEL_COND_INITIALIZER PTHREAD_COND_INITIALIZER
#define KERNEL_ONCE_INIT PTHREAD_ONCE_INIT

static inline int kernel_mutex_init(kernel_mutex_t *mutex) {
    return pthread_mutex_init(mutex, NULL);
}

static inline int kernel_mutex_destroy(kernel_mutex_t *mutex) {
    return pthread_mutex_destroy(mutex);
}

static inline int kernel_mutex_lock(kernel_mutex_t *mutex) {
    return pthread_mutex_lock(mutex);
}

static inline int kernel_mutex_unlock(kernel_mutex_t *mutex) {
    return pthread_mutex_unlock(mutex);
}

static inline int kernel_cond_init(kernel_cond_t *cond) {
    return pthread_cond_init(cond, NULL);
}

static inline int kernel_cond_destroy(kernel_cond_t *cond) {
    return pthread_cond_destroy(cond);
}

static inline int kernel_cond_wait(kernel_cond_t *cond, kernel_mutex_t *mutex) {
    return pthread_cond_wait(cond, mutex);
}

static inline int kernel_cond_broadcast(kernel_cond_t *cond) {
    return pthread_cond_broadcast(cond);
}

static inline int kernel_thread_attr_init(kernel_thread_attr_t *attr) {
    return pthread_attr_init(attr);
}

static inline int kernel_thread_attr_destroy(kernel_thread_attr_t *attr) {
    return pthread_attr_destroy(attr);
}

static inline int kernel_thread_attr_setstacksize(kernel_thread_attr_t *attr, size_t stacksize) {
    return pthread_attr_setstacksize(attr, stacksize);
}

static inline int kernel_thread_create(kernel_thread_t *thread, const kernel_thread_attr_t *attr,
                                       void *(*start_routine)(void *), void *arg) {
    return pthread_create(thread, attr, start_routine, arg);
}

static inline int kernel_thread_detach(kernel_thread_t thread) {
    return pthread_detach(thread);
}

static inline kernel_thread_t kernel_thread_self(void) {
    return pthread_self();
}

static inline void kernel_thread_exit(void *value_ptr) {
    pthread_exit(value_ptr);
}

static inline int kernel_once(kernel_once_t *once_control, void (*init_routine)(void)) {
    return pthread_once(once_control, init_routine);
}

static inline int kernel_thread_sigmask(int how, const kernel_sigset_t *set, kernel_sigset_t *oldset) {
    return pthread_sigmask(how, set, oldset);
}

static inline int kernel_clock_gettime(clockid_t clock_id, struct timespec *tp) {
    return clock_gettime(clock_id, tp);
}

#ifdef __cplusplus
}
#endif

#endif
