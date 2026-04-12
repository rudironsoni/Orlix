/* IXLandSystem/kernel/signal.h
 * Internal kernel signal owner header
 * Canonical signal types, NO host signal.h leakage
 */

#ifndef IXLAND_SYSTEM_KERNEL_SIGNAL_H
#define IXLAND_SYSTEM_KERNEL_SIGNAL_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "task.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Signal space definition - canonical, not from host */
#define IX_NSIG 64

/* Signal queue entry - canonical internal type */
typedef struct ix_sigqueue_entry {
    int sig;
    int32_t si_signo;
    int32_t si_errno;
    int32_t si_code;
    struct ix_sigqueue_entry *next;
} ix_sigqueue_entry_t;

/* Signal queue */
typedef struct ix_sigqueue {
    ix_sigqueue_entry_t *head;
    ix_sigqueue_entry_t *tail;
    int count;
    pthread_mutex_t lock;
} ix_sigqueue_t;

/* Canonical signal set (internal representation) */
typedef struct ix_sigset {
    uint64_t sig[IX_NSIG / 64 + 1];
} ix_sigset_t;

/* Canonical sigaction (internal representation) */
struct ix_sigaction {
    void (*sa_handler)(int);
    ix_sigset_t sa_mask;
    int sa_flags;
};

/* Signal handling state (Linux-style sighand_struct) */
struct sighand_struct {
    struct ix_sigaction action[IX_NSIG];
    ix_sigset_t blocked;
    ix_sigset_t pending;
    ix_sigqueue_t queue;
    _Atomic int refs;
};

/* Sighand allocation and management */
struct sighand_struct *alloc_sighand(void);
void free_sighand(struct sighand_struct *sighand);
struct sighand_struct *dup_sighand(struct sighand_struct *parent);

/* Signal actions - internal kernel implementation */
int do_sigaction(int sig, const struct ix_sigaction *act, struct ix_sigaction *oldact);

/* Signal sending - internal kernel implementation */
int do_kill(pid_t pid, int sig);
int do_killpg(pid_t pgrp, int sig);

/* Signal masking - internal kernel implementation */
int do_sigprocmask(int how, const ix_sigset_t *set, ix_sigset_t *oldset);
int do_sigpending(ix_sigset_t *set);
int do_sigsuspend(const ix_sigset_t *mask);

/* Signal to current process - internal implementation */
int do_raise(int sig);

/* Wait for signal - internal implementation */
int do_pause(void);

/* Signal handler installation - internal implementation */
typedef void (*ix_sighandler_t)(int);
ix_sighandler_t do_signal(int signum, ix_sighandler_t handler);

/* Initialization (internal use) */
void signal_init(void);
void signal_deinit(void);

/* Internal signal delivery (called by task.c) */
void force_sig(int sig, struct task_struct *task);

#ifdef __cplusplus
}
#endif

#endif /* IXLAND_SYSTEM_KERNEL_SIGNAL_H */
