#ifndef KERNEL_SYNC_H
#define KERNEL_SYNC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque synchronization types - implementation uses host pthread primitives.
 * Defined in internal/ios/kernel/sync.c */
struct kernel_mutex_impl;
struct kernel_cond_impl;
struct kernel_thread_impl;
struct kernel_thread_attr_impl;
struct kernel_once_impl;

typedef struct kernel_mutex_impl kernel_mutex_t;
typedef struct kernel_cond_impl kernel_cond_t;
typedef struct kernel_thread_impl kernel_thread_t;
typedef struct kernel_thread_attr_impl kernel_thread_attr_t;
typedef struct kernel_once_impl kernel_once_t;

/* Static initializer values */
#define KERNEL_MUTEX_INITIALIZER {(void*)0}
#define KERNEL_COND_INITIALIZER {(void*)0}
#define KERNEL_ONCE_INIT {(void*)0}

/* Mutex operations */
int kernel_mutex_init(kernel_mutex_t *mutex);
int kernel_mutex_destroy(kernel_mutex_t *mutex);
int kernel_mutex_lock(kernel_mutex_t *mutex);
int kernel_mutex_unlock(kernel_mutex_t *mutex);

/* Condition variable operations */
int kernel_cond_init(kernel_cond_t *cond);
int kernel_cond_destroy(kernel_cond_t *cond);
int kernel_cond_wait(kernel_cond_t *cond, kernel_mutex_t *mutex);
int kernel_cond_broadcast(kernel_cond_t *cond);

/* Thread operations */
int kernel_thread_attr_init(kernel_thread_attr_t *attr);
int kernel_thread_attr_destroy(kernel_thread_attr_t *attr);
int kernel_thread_attr_setstacksize(kernel_thread_attr_t *attr, size_t stacksize);
int kernel_thread_create(kernel_thread_t *thread, const kernel_thread_attr_t *attr,
                         void *(*start_routine)(void *), void *arg);
int kernel_thread_detach(kernel_thread_t thread);
kernel_thread_t kernel_thread_self(void);
void kernel_thread_exit(void *value_ptr);

/* Once operations */
int kernel_once(kernel_once_t *once_control, void (*init_routine)(void));

/* Signal mask operations - opaque sigset type */
struct kernel_sigset_impl;
typedef struct kernel_sigset_impl kernel_sigset_t;
int kernel_thread_sigmask(int how, const kernel_sigset_t *set, kernel_sigset_t *oldset);
int kernel_sigemptyset(kernel_sigset_t *set);
int kernel_sigaddset(kernel_sigset_t *set, int signo);
int kernel_sigismember(const kernel_sigset_t *set, int signo);

/* Clock operations - uses host clock_id_t, struct timespec from sys/_types */
struct timespec;
int kernel_clock_gettime(int clock_id, struct timespec *tp);

#ifdef __cplusplus
}
#endif

#endif
