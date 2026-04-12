/* IXLandSystem/kernel/signal.h
 * Internal kernel signal owner header
 * Linux-shaped internal declarations only
 */

#ifndef IXLAND_SYSTEM_KERNEL_SIGNAL_H
#define IXLAND_SYSTEM_KERNEL_SIGNAL_H

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

/* Darwin doesn't define NSIG in signal.h, define for Linux compatibility */
#ifndef NSIG
#define NSIG 64
#endif

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
    struct sigaction action[NSIG];
    sigset_t blocked;
    sigset_t pending;
    sigqueue_t queue;
    atomic_int refs;
};

/* Sighand allocation and management */
struct sighand_struct *alloc_sighand(void);
void free_sighand(struct sighand_struct *sighand);
struct sighand_struct *dup_sighand(struct sighand_struct *parent);

/* Signal actions - Linux internal naming */
int do_sigaction(int sig, const struct sigaction *act, struct sigaction *oldact);

/* Signal sending - use Linux syscall-facing names */
int kill(pid_t pid, int sig);
int killpg(pid_t pgrp, int sig);

/* Signal masking - Linux syscall-facing names */
int sigprocmask(int how, const sigset_t *set, sigset_t *oldset);
int sigpending(sigset_t *set);
int sigsuspend(const sigset_t *mask);

/* Signal to current process - Linux syscall-facing name */
int raise(int sig);

/* Wait for signal - Linux syscall-facing name */
int pause(void);

/* Initialization (internal use) */
void signal_init(void);
void signal_deinit(void);

/* Internal signal delivery (called by task.c) */
void force_sig(int sig, struct task_struct *task);

#ifdef __cplusplus
}
#endif

#endif /* IXLAND_SYSTEM_KERNEL_SIGNAL_H */
