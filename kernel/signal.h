/* IXLandSystem/kernel/signal.h
 * Private internal owner header for virtual signal subsystem
 * 
 * This is PRIVATE internal state for the virtual kernel's signal handling.
 * NOT a public Linux ABI header.
 * 
 * Virtual signal behavior emulated:
 * - standard and realtime signals
 * - per-thread signal masks
 * - pending signals
 * - process-directed vs thread-directed delivery
 * - fork inheriting signal mask
 * - handler installation via sigaction
 * - sigprocmask, sigpending, sigsuspend, kill, killpg, raise
 */

#ifndef IXLAND_KERNEL_SIGNAL_H
#define IXLAND_KERNEL_SIGNAL_H

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>
#include <sys/types.h>
#include <signal.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations - avoid circular include with task.h */
struct task_struct;

/* Virtual signal namespace - Linux signal count */
#define _NSIG 64

/* Virtual signal queue entry - internal representation */
typedef struct ix_sigqueue_entry {
    int sig;
    int32_t si_signo;
    int32_t si_errno;
    int32_t si_code;
    struct ix_sigqueue_entry *next;
} ix_sigqueue_entry_t;

/* Virtual signal queue - internal representation */
typedef struct ix_sigqueue {
    ix_sigqueue_entry_t *head;
    ix_sigqueue_entry_t *tail;
    int count;
    pthread_mutex_t lock;
} ix_sigqueue_t;

/* Virtual signal set - internal representation (matches Linux sigset_t size)
 * Use Linux UAPI sigset_t in public contracts, this is for internal tracking */
typedef struct ix_sigset {
    uint64_t sig[(_NSIG / 64) + 1];
} ix_sigset_t;

/* Virtual kernel sigaction - internal representation
 * NOT the Linux UAPI struct sigaction - that's in asm/sigaction.h */
typedef struct ix_k_sigaction {
    void (*handler)(int);
    uint64_t mask;
    int flags;
} ix_k_sigaction_t;

/* Signal handler table - per-task signal configuration */
struct sighand_struct {
    atomic_int refs;
    ix_k_sigaction_t action[_NSIG];
    ix_sigset_t blocked;
    ix_sigset_t pending;
    ix_sigqueue_t queue;
    pthread_mutex_t lock;
};

/* Virtual signal stack state */
struct ix_sigaltstack {
    void *ss_sp;
    size_t ss_size;
    int ss_flags;
};

/* Initialize signal state for a new task */
int signal_init_task(struct task_struct *task);

/* Inherit signal state on fork/clone */
struct sighand_struct *alloc_sighand(void);
void free_sighand(struct sighand_struct *sighand);
struct sighand_struct *dup_sighand(struct task_struct *parent, struct task_struct *child);

/* Reset signal state on exec */
void signal_reset_on_exec(struct task_struct *task);

/* Virtual signal enqueue helpers */
int signal_enqueue_task(struct task_struct *task, int sig);
int signal_enqueue_group(struct task_struct *task, int sig, bool group_wide);
int signal_dequeue(struct task_struct *task, sigset_t *mask, int *sig);

/* Recompute pending state after mask changes */
void signal_recompute_pending(struct task_struct *task);

/* Signal wakeup - wake the right task after signal generation */
void signal_wake_task(struct task_struct *task, bool group_wide);

/* Virtual signal syscalls (internal helpers) */
int kill_impl(pid_t pid, int sig);
int killpg_impl(pid_t pgrp, int sig);
int raise_impl(int sig);
int sigaction_impl(int sig, const struct sigaction *act, struct sigaction *oldact);
int sigprocmask_impl(int how, const sigset_t *set, sigset_t *oldset);
int sigpending_impl(sigset_t *set);
int sigsuspend_impl(const sigset_t *mask);
int sigaltstack_impl(const struct sigaction *ss, struct sigaction *oss);

/* Translate between internal ix_sigset_t and POSIX sigset_t */
void ix_sigset_to_sigset(const ix_sigset_t *ix, sigset_t *posix);
void sigset_to_ix_sigset(const sigset_t *posix, ix_sigset_t *ix);

#ifdef __cplusplus
}
#endif

#endif /* IXLAND_KERNEL_SIGNAL_H */
