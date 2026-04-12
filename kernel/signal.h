#ifndef SIGNAL_H
#define SIGNAL_H

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <signal.h>
#include <stdbool.h>
#include <time.h>

#include "task.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Linux-shaped signal-space constants for canonical core */
#define KERNEL_NSIG 64
#define KERNEL_NSIG_WORDS (KERNEL_NSIG / (sizeof(unsigned long) * 8))

/* Canonical internal signal set type (Linux-shaped) */
typedef struct {
    unsigned long sig[KERNEL_NSIG_WORDS];
} ksigset_t;

/* Canonical internal signal action (Linux-shaped) */
struct k_sigaction {
    void (*handler)(int);
    ksigset_t mask;
    int flags;
};

/* Signal queue entry */
typedef struct sigqueue_entry {
    int sig;
    siginfo_t info;
    struct sigqueue_entry *next;
} sigqueue_entry_t;

/* Signal queue */
typedef struct sigqueue {
    sigqueue_entry_t *head;
    sigqueue_entry_t *tail;
    int count;
    pthread_mutex_t lock;
} sigqueue_t;

/* Signal handling state (Linux-style sighand_struct) */
struct sighand_struct {
    struct k_sigaction action[KERNEL_NSIG];
    ksigset_t blocked;
    ksigset_t pending;
    sigqueue_t queue;
    atomic_int refs;
};

/* Sighand allocation and management */
struct sighand_struct *alloc_sighand(void);
void free_sighand(struct sighand_struct *sighand);
struct sighand_struct *dup_sighand(struct sighand_struct *parent);

/* Signal actions */
int do_sigaction(int sig, const struct k_sigaction *act, struct k_sigaction *oldact);

/* Signal sending */
int do_kill(pid_t pid, int sig);
int do_killpg(pid_t pgrp, int sig);

/* Signal masking */
int do_sigprocmask(struct sighand_struct *sighand, int how, const ksigset_t *set,
                   ksigset_t *oldset);
int do_sigpending(struct sighand_struct *sighand, ksigset_t *set);
int do_sigsuspend(const ksigset_t *mask);

/* Signal queuing and waiting */
int do_sigqueue(pid_t pid, int sig, const union sigval value);
int do_sigtimedwait(const ksigset_t *set, siginfo_t *info, const struct timespec *timeout);

/* Simplified handler installation */
typedef void (*sighandler_t)(int);
sighandler_t do_signal(int signum, sighandler_t handler);

/* Signal to current process */
int do_raise(int sig);

/* Alarm timer */
unsigned int do_alarm(unsigned int seconds);

/* Wait for signal */
int do_pause(void);

/* Initialization (internal use) */
void signal_init(void);
void signal_deinit(void);

/* Internal signal delivery (called by task.c) */
void force_sig(int sig, struct task_struct *task);

/* Signal dispatch (internal) */
void __apply_signal_to_task(struct task_struct *task, int sig);

#ifdef __cplusplus
}
#endif

#endif /* SIGNAL_H */
