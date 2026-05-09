#ifndef ORLIX_INTERNAL_PRIVATE_KERNEL_SYNC_H
#define ORLIX_INTERNAL_PRIVATE_KERNEL_SYNC_H

#include <stddef.h>
#include <stdint.h>

#include <linux/time_types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct signal_mask_bits;

#define KERNEL_COND_WAIT_TIMED_OUT 1

typedef struct kernel_mutex {
    void *impl;
} kernel_mutex_t;

typedef struct kernel_cond {
    void *impl;
} kernel_cond_t;

typedef struct kernel_thread {
    void *impl;
} kernel_thread_t;

typedef struct kernel_thread_attr {
    void *impl;
} kernel_thread_attr_t;

typedef struct kernel_once {
    int done;
} kernel_once_t;

#define KERNEL_MUTEX_INITIALIZER {NULL}
#define KERNEL_COND_INITIALIZER {NULL}
#define KERNEL_ONCE_INIT {0}
#define KERNEL_SIGSET_INITIALIZER {{0}}

int kernel_mutex_init(kernel_mutex_t *mutex);
int kernel_mutex_destroy(kernel_mutex_t *mutex);
int kernel_mutex_lock(kernel_mutex_t *mutex);
int kernel_mutex_unlock(kernel_mutex_t *mutex);

int kernel_cond_init(kernel_cond_t *cond);
int kernel_cond_destroy(kernel_cond_t *cond);
int kernel_cond_wait(kernel_cond_t *cond, kernel_mutex_t *mutex);
int kernel_cond_timedwait_ms(kernel_cond_t *cond, kernel_mutex_t *mutex, int timeout_ms);
int kernel_cond_signal(kernel_cond_t *cond);
int kernel_cond_broadcast(kernel_cond_t *cond);

int kernel_thread_attr_init(kernel_thread_attr_t *attr);
int kernel_thread_attr_destroy(kernel_thread_attr_t *attr);
int kernel_thread_attr_setstacksize(kernel_thread_attr_t *attr, size_t stacksize);
int kernel_thread_create(kernel_thread_t *thread, const kernel_thread_attr_t *attr,
                         void *(*start_routine)(void *), void *arg);
int kernel_thread_detach(kernel_thread_t thread);
kernel_thread_t kernel_thread_self(void);
void kernel_thread_exit(void *value_ptr);

int kernel_once(kernel_once_t *once_control, void (*init_routine)(void));

int kernel_thread_sigmask(int how, const struct signal_mask_bits *set,
                          struct signal_mask_bits *oldset);
int kernel_sigemptyset(struct signal_mask_bits *set);
int kernel_sigaddset(struct signal_mask_bits *set, int signo);
int kernel_sigismember(const struct signal_mask_bits *set, int signo);
int kernel_sleep_ms(int timeout_ms);

int kernel_clock_gettime(int clock_id, struct __kernel_timespec *tp);

#ifdef __cplusplus
}
#endif

#endif
