/* IXLandSystem/kernel/signal.c
 * Internal kernel signal owner implementation
 *
 * Public wrappers use proper POSIX types.
 * Internal logic uses private types only.
 */

#include "signal.h"

#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>

#include "task.h"

/* Include host signal.h ONLY for the public wrapper signatures.
 * This is acceptable because signal.c owns the public signal contract. */
#include <signal.h>

struct signal_struct *alloc_signal_struct(void) {
    struct signal_struct *sig = calloc(1, sizeof(struct signal_struct));
    if (!sig)
        return NULL;

    atomic_init(&sig->refs, 1);
    pthread_mutex_init(&sig->queue.lock, NULL);

    /* Initialize default handlers (SIG_DFL = NULL) */
    for (int i = 0; i < SIGNAL_NSIG; i++) {
        sig->actions[i].handler = NULL;
        memset(&sig->actions[i].mask, 0, sizeof(struct signal_mask_bits));
        sig->actions[i].flags = 0;
    }

    memset(&sig->blocked, 0, sizeof(struct signal_mask_bits));
    memset(&sig->pending, 0, sizeof(struct signal_mask_bits));

    return sig;
}

void free_signal_struct(struct signal_struct *sig) {
    if (!sig)
        return;
    if (atomic_fetch_sub(&sig->refs, 1) > 1)
        return;

    /* Free queued signals */
    pthread_mutex_lock(&sig->queue.lock);
    struct signal_queue_entry *entry = sig->queue.head;
    while (entry) {
        struct signal_queue_entry *next = entry->next;
        free(entry);
        entry = next;
    }
    pthread_mutex_unlock(&sig->queue.lock);

    pthread_mutex_destroy(&sig->queue.lock);
    free(sig);
}

struct signal_struct *dup_signal_struct(struct signal_struct *parent) {
    if (!parent)
        return NULL;

    struct signal_struct *child = alloc_signal_struct();
    if (!child)
        return NULL;

    /* Copy signal handlers */
    memcpy(child->actions, parent->actions, sizeof(child->actions));

    /* Child inherits parent's signal mask */
    child->blocked = parent->blocked;

    /* But pending signals are cleared */
    memset(&child->pending, 0, sizeof(struct signal_mask_bits));

    return child;
}

static void apply_signal_to_task(struct task_struct *task, int32_t sig) {
    if (!task || !task->signal)
        return;

    /* Mark signal as pending */
    int idx = sig / 64;
    int bit = sig % 64;
    if (idx < SIGNAL_NSIG_WORDS) {
        task->signal->pending.sig[idx] |= (1ULL << bit);
    }

    /* Handle SIGSTOP: transition to STOPPED state */
    if (sig == 19) {
        atomic_store(&task->state, TASK_STOPPED);
    }

    /* Handle SIGCONT: transition back to RUNNING */
    if (sig == 18) {
        atomic_store(&task->state, TASK_RUNNING);
    }

    /* Handle terminating signals */
    if (sig == 15 || sig == 9 || sig == 2) {
        atomic_store(&task->signaled, true);
        atomic_store(&task->termsig, sig);
        atomic_store(&task->exited, true);
        atomic_store(&task->state, TASK_ZOMBIE);
    }

    /* Notify parent */
    if (task->parent) {
        pthread_mutex_lock(&task->parent->lock);
        if (task->parent->waiters > 0) {
            pthread_cond_broadcast(&task->parent->wait_cond);
        }
        pthread_mutex_unlock(&task->parent->lock);
    }

    /* Wake up this task if waiting */
    if (task->waiters > 0) {
        pthread_cond_broadcast(&task->wait_cond);
    }
}

int signal_generate_task(struct task_struct *target, int32_t sig) {
    if (!target || sig < 0 || sig >= SIGNAL_NSIG)
        return -EINVAL;

    if (sig == 0) {
        /* Just check if process exists */
        return 0;
    }

    pthread_mutex_lock(&target->lock);
    apply_signal_to_task(target, sig);
    pthread_mutex_unlock(&target->lock);

    return 0;
}

int signal_generate_pgrp(int32_t pgid, int32_t sig) {
    if (sig < 0 || sig >= SIGNAL_NSIG)
        return -EINVAL;

    if (pgid <= 0)
        return -EINVAL;

    int found = 0;

    pthread_mutex_lock(&task_table_lock);

    for (int i = 0; i < TASK_MAX_TASKS; i++) {
        struct task_struct *task = task_table[i];
        while (task) {
            if (task->pgid == pgid) {
                found = 1;
                atomic_fetch_add(&task->refs, 1);
                pthread_mutex_lock(&task->lock);
                apply_signal_to_task(task, sig);
                pthread_mutex_unlock(&task->lock);
                free_task(task);
            }
            task = task->hash_next;
        }
    }

    pthread_mutex_unlock(&task_table_lock);

    if (!found)
        return -ESRCH;

    return 0;
}

int signal_enqueue_task(struct task_struct *task, int32_t sig) {
    return signal_generate_task(task, sig);
}

int signal_enqueue_group(int32_t pgid, int32_t sig) {
    return signal_generate_pgrp(pgid, sig);
}

int signal_dequeue(struct task_struct *task, struct signal_mask_bits *mask, int32_t *sig) {
    if (!task || !task->signal || !sig)
        return -EINVAL;

    /* Find first pending signal that's not blocked */
    for (int i = 1; i < SIGNAL_NSIG; i++) {
        int idx = i / 64;
        int bit = i % 64;

        if (idx >= SIGNAL_NSIG_WORDS)
            continue;

        /* Check if pending and not blocked */
        if ((task->signal->pending.sig[idx] & (1ULL << bit)) &&
            !(task->signal->blocked.sig[idx] & (1ULL << bit))) {

            /* Also check against provided mask if given */
            if (mask && (mask->sig[idx] & (1ULL << bit)))
                continue;

            *sig = i;
            /* Clear pending bit */
            task->signal->pending.sig[idx] &= ~(1ULL << bit);
            return 0;
        }
    }

    return -EAGAIN;
}

void signal_recompute_pending(struct task_struct *task) {
    /* Recompute whether task has any deliverable pending signals */
    if (!task || !task->signal)
        return;

    /* This would update any cached "has_pending" flags */
    /* For now, pending signals are checked on demand */
}

void signal_wake_task(struct task_struct *task, bool group_wide) {
    (void)group_wide;

    if (!task)
        return;

    /* Wake the task if it's waiting */
    pthread_mutex_lock(&task->wait_lock);
    if (task->waiters > 0) {
        pthread_cond_broadcast(&task->wait_cond);
    }
    pthread_mutex_unlock(&task->wait_lock);
}

bool signal_is_blocked(const struct task_struct *task, int32_t sig) {
    if (!task || !task->signal)
        return false;

    int idx = sig / 64;
    int bit = sig % 64;

    if (idx >= SIGNAL_NSIG_WORDS)
        return false;

    return (task->signal->blocked.sig[idx] & (1ULL << bit)) != 0;
}

void signal_reset_on_exec(struct task_struct *task) {
    if (!task || !task->signal)
        return;

    /* Reset signal handlers that have SA_RESETHAND flag set */
    /* For now, simplified: reset pending signals */
    memset(&task->signal->pending, 0, sizeof(struct signal_mask_bits));
}

int signal_init_task(struct task_struct *task) {
    if (!task)
        return -EINVAL;

    task->signal = alloc_signal_struct();
    if (!task->signal)
        return -ENOMEM;

    return 0;
}

/* ============================================================================
 * INTERNAL SIGNAL SYSCALL IMPLEMENTATIONS
 * ============================================================================
 * These use only private internal types from kernel/signal.h
 */

int do_sigaction(int32_t sig, const struct signal_action_slot *act,
                 struct signal_action_slot *oldact) {
    if (sig < 1 || sig >= SIGNAL_NSIG) {
        errno = EINVAL;
        return -1;
    }

    if (sig == 9 || sig == 19) {
        errno = EINVAL;
        return -1;
    }

    struct task_struct *task = get_current();
    if (!task || !task->signal) {
        errno = ESRCH;
        return -1;
    }

    if (oldact) {
        *oldact = task->signal->actions[sig];
    }

    if (act) {
        task->signal->actions[sig] = *act;
    }

    return 0;
}

int do_sigprocmask(int how, const struct signal_mask_bits *set,
		   struct signal_mask_bits *oldset) {
	struct task_struct *task = get_current();
	if (!task || !task->signal) {
		errno = ESRCH;
		return -1;
	}

	struct signal_struct *sig = task->signal;

	if (oldset) {
		*oldset = sig->blocked;
	}

	if (set) {
		switch (how) {
		case SIG_BLOCK: /* Block signals in set */
			for (int i = 0; i < SIGNAL_NSIG_WORDS; i++) {
				sig->blocked.sig[i] |= set->sig[i];
			}
			break;
		case SIG_UNBLOCK: /* Unblock signals in set */
			for (int i = 0; i < SIGNAL_NSIG_WORDS; i++) {
				sig->blocked.sig[i] &= ~set->sig[i];
			}
			break;
		case SIG_SETMASK: /* Replace blocked set with set */
			sig->blocked = *set;
			break;
		default:
			errno = EINVAL;
			return -1;
		}
	}

	return 0;
}

int do_sigpending(struct signal_mask_bits *set) {
    if (!set) {
        errno = EFAULT;
        return -1;
    }

    struct task_struct *task = get_current();
    if (!task || !task->signal) {
        errno = ESRCH;
        return -1;
    }

    *set = task->signal->pending;
    return 0;
}

sighandler_t do_signal(int32_t signum, sighandler_t handler) {
    if (signum < 1 || signum >= SIGNAL_NSIG) {
        errno = EINVAL;
        return NULL;
    }

    if (signum == 9 || signum == 19) {
        errno = EINVAL;
        return NULL;
    }

    struct task_struct *task = get_current();
    if (!task || !task->signal) {
        errno = ESRCH;
        return NULL;
    }

    sighandler_t old_handler = task->signal->actions[signum].handler;
    task->signal->actions[signum].handler = handler;
    task->signal->actions[signum].flags = 0;
    memset(&task->signal->actions[signum].mask, 0, sizeof(struct signal_mask_bits));

    return old_handler;
}

int do_raise(int32_t sig) {
    struct task_struct *task = get_current();
    if (!task) {
        errno = ESRCH;
        return -1;
    }
    return signal_generate_task(task, sig);
}

static int is_sigset_empty(const struct signal_mask_bits *set) {
    for (int i = 0; i < SIGNAL_NSIG_WORDS; i++) {
        if (set->sig[i] != 0)
            return 0;
    }
    return 1;
}

int do_pause(void) {
    struct task_struct *task = get_current();
    if (!task) {
        errno = ESRCH;
        return -1;
    }

    pthread_mutex_lock(&task->wait_lock);

    while (is_sigset_empty(&task->signal->pending)) {
        task->waiters++;
        pthread_cond_wait(&task->wait_cond, &task->wait_lock);
        task->waiters--;
    }

    pthread_mutex_unlock(&task->wait_lock);

    errno = EINTR;
    return -1;
}

int do_sigsuspend(const struct signal_mask_bits *mask) {
    struct task_struct *task = get_current();
    if (!task || !task->signal) {
        errno = ESRCH;
        return -1;
    }

    if (!mask) {
        errno = EFAULT;
        return -1;
    }

    /* Save old mask */
    struct signal_mask_bits old_mask = task->signal->blocked;

    /* Install new mask */
    task->signal->blocked = *mask;

    /* Wait for signal */
    pthread_mutex_lock(&task->wait_lock);
    task->waiters++;
    pthread_cond_wait(&task->wait_cond, &task->wait_lock);
    task->waiters--;
    pthread_mutex_unlock(&task->wait_lock);

    /* Restore old mask */
    task->signal->blocked = old_mask;

    errno = EINTR;
    return -1;
}

int do_kill(int32_t pid, int32_t sig) {
    if (sig < 0 || sig >= SIGNAL_NSIG) {
        errno = EINVAL;
        return -1;
    }

    if (pid <= 0) {
        /* Process group handling */
        if (pid == 0) {
            /* Current process group */
            struct task_struct *task = get_current();
            if (!task) {
                errno = ESRCH;
                return -1;
            }
            return signal_generate_pgrp(task->pgid, sig);
        } else if (pid == -1) {
            /* All processes (privileged) */
            errno = EPERM;
            return -1;
        } else {
            /* Process group |pid| */
            return signal_generate_pgrp(-pid, sig);
        }
    }

    struct task_struct *task = task_lookup(pid);
    if (!task) {
        errno = ESRCH;
        return -1;
    }

    if (sig == 0) {
        /* Just check if process exists */
        free_task(task);
        return 0;
    }

    int result = signal_generate_task(task, sig);
    free_task(task);
    return result;
}

int do_killpg(int32_t pgrp, int32_t sig) {
    return signal_generate_pgrp(pgrp, sig);
}

/* ============================================================================
 * PUBLIC CANONICAL SIGNAL WRAPPERS
 * ============================================================================
 * These export the Linux-facing signal ABI from the kernel signal owner.
 * They use opaque void* to avoid depending on Darwin host types.
 */

/* Bridge helpers declared in arch/darwin/signal_bridge.c */
extern void bridge_signal_from_host(const struct sigaction *host_act, struct signal_action_slot *out);
extern void bridge_signal_to_host(const struct signal_action_slot *internal, struct sigaction *host_act);
extern void bridge_sigset_from_host(const sigset_t *host_set, struct signal_mask_bits *out);
extern void bridge_sigset_to_host(const struct signal_mask_bits *internal, sigset_t *host_set);

__attribute__((visibility("default"))) int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact) {
    struct signal_action_slot internal_act, internal_oldact;
    struct signal_action_slot *internal_act_ptr = NULL;
    struct signal_action_slot *internal_oldact_ptr = NULL;

    if (act) {
        bridge_signal_from_host(act, &internal_act);
        internal_act_ptr = &internal_act;
    }

    if (oldact) {
        internal_oldact_ptr = &internal_oldact;
    }

    int result = do_sigaction(signum, internal_act_ptr, internal_oldact_ptr);

    if (oldact && result == 0) {
        bridge_signal_to_host(&internal_oldact, oldact);
    }

    return result;
}

__attribute__((visibility("default"))) sighandler_t signal(int signum, sighandler_t handler) {
    if (signum < 1 || signum >= SIGNAL_NSIG) {
        errno = EINVAL;
        return (sighandler_t)-1;
    }

    if (signum == 9 || signum == 19) {
        errno = EINVAL;
        return (sighandler_t)-1;
    }

    sighandler_t old_handler = do_signal(signum, handler);
    return old_handler ? old_handler : (sighandler_t)-1;
}

__attribute__((visibility("default"))) int kill(int32_t pid, int sig) {
    return do_kill(pid, sig);
}

__attribute__((visibility("default"))) int killpg(int32_t pgrp, int sig) {
    return do_killpg(pgrp, sig);
}

__attribute__((visibility("default"))) int sigprocmask(int how, const sigset_t *set, sigset_t *oldset) {
	if (how < SIG_BLOCK || how > SIG_SETMASK) {
		errno = EINVAL;
		return -1;
	}

    struct signal_mask_bits internal_set, internal_oldset;
    struct signal_mask_bits *internal_set_ptr = NULL;
    struct signal_mask_bits *internal_oldset_ptr = NULL;

    if (set) {
        bridge_sigset_from_host(set, &internal_set);
        internal_set_ptr = &internal_set;
    }

    if (oldset) {
        internal_oldset_ptr = &internal_oldset;
    }

    int result = do_sigprocmask(how, internal_set_ptr, internal_oldset_ptr);

    if (oldset && result == 0) {
        bridge_sigset_to_host(&internal_oldset, oldset);
    }

    return result;
}

__attribute__((visibility("default"))) int sigpending(sigset_t *set) {
    if (!set) {
        errno = EFAULT;
        return -1;
    }

    struct signal_mask_bits internal_set;
    int result = do_sigpending(&internal_set);

    if (result == 0) {
        bridge_sigset_to_host(&internal_set, set);
    }

    return result;
}

__attribute__((visibility("default"))) int sigsuspend(const sigset_t *mask) {
    if (!mask) {
        errno = EFAULT;
        return -1;
    }

    struct signal_mask_bits internal_mask;
    bridge_sigset_from_host(mask, &internal_mask);

    return do_sigsuspend(&internal_mask);
}

__attribute__((visibility("default"))) int raise(int sig) {
    return do_raise(sig);
}

__attribute__((visibility("default"))) int pause(void) {
    return do_pause();
}
