/* IXLandSystem/kernel/signal.c
 * Internal kernel signal owner implementation
 * Canonical signal types only, NO host signal.h
 */

#include "signal.h"

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct sighand_struct *alloc_sighand(void) {
    struct sighand_struct *sighand = calloc(1, sizeof(struct sighand_struct));
    if (!sighand)
        return NULL;

    atomic_init(&sighand->refs, 1);
    pthread_mutex_init(&sighand->queue.lock, NULL);

    /* Initialize default handlers */
    for (int i = 0; i < IX_NSIG; i++) {
        sighand->action[i].sa_handler = NULL; /* SIG_DFL equivalent */
        memset(&sighand->action[i].sa_mask, 0, sizeof(ix_sigset_t));
        sighand->action[i].sa_flags = 0;
    }

    memset(&sighand->blocked, 0, sizeof(ix_sigset_t));
    memset(&sighand->pending, 0, sizeof(ix_sigset_t));

    return sighand;
}

void free_sighand(struct sighand_struct *sighand) {
    if (!sighand)
        return;
    if (atomic_fetch_sub(&sighand->refs, 1) > 1)
        return;

    /* Free queued signals */
    pthread_mutex_lock(&sighand->queue.lock);
    ix_sigqueue_entry_t *entry = sighand->queue.head;
    while (entry) {
        ix_sigqueue_entry_t *next = entry->next;
        free(entry);
        entry = next;
    }
    pthread_mutex_unlock(&sighand->queue.lock);

    pthread_mutex_destroy(&sighand->queue.lock);
    free(sighand);
}

struct sighand_struct *dup_sighand(struct sighand_struct *parent) {
    if (!parent)
        return NULL;

    struct sighand_struct *child = alloc_sighand();
    if (!child)
        return NULL;

    /* Copy signal handlers */
    memcpy(child->action, parent->action, sizeof(child->action));

    /* Child inherits parent's signal mask */
    child->blocked = parent->blocked;

    /* But pending signals are cleared */
    memset(&child->pending, 0, sizeof(ix_sigset_t));

    return child;
}

int do_sigaction(int sig, const struct ix_sigaction *act, struct ix_sigaction *oldact) {
    if (sig < 1 || sig >= IX_NSIG) {
        errno = EINVAL;
        return -1;
    }

    if (sig == 9 || sig == 19) { /* SIGKILL=9, SIGSTOP=19 */
        errno = EINVAL;
        return -1;
    }

    struct task_struct *task = get_current();
    if (!task || !task->sighand) {
        errno = ESRCH;
        return -1;
    }

    if (oldact) {
        *oldact = task->sighand->action[sig];
    }

    if (act) {
        task->sighand->action[sig] = *act;
    }

    return 0;
}

static void apply_signal_to_task(struct task_struct *task, int sig) {
    /* Mark signal as pending */
    int idx = sig / 64;
    int bit = sig % 64;
    task->sighand->pending.sig[idx] |= (1ULL << bit);

    /* Handle SIGSTOP: transition to STOPPED state */
    if (sig == 19) { /* SIGSTOP */
        atomic_store(&task->state, TASK_STOPPED);
    }

    /* Handle SIGCONT: transition back to RUNNING */
    if (sig == 18) { /* SIGCONT */
        atomic_store(&task->state, TASK_RUNNING);
    }

    /* Handle terminating signals */
    if (sig == 15 || sig == 9 || sig == 2) { /* SIGTERM, SIGKILL, SIGINT */
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

int do_kill(pid_t pid, int sig) {
    if (sig < 0 || sig >= IX_NSIG) {
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
            return do_killpg(task->pgid, sig);
        } else if (pid == -1) {
            /* All processes (privileged) */
            errno = EPERM;
            return -1;
        } else {
            /* Process group |pid| */
            return do_killpg(-pid, sig);
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

    /* Apply signal */
    pthread_mutex_lock(&task->lock);
    apply_signal_to_task(task, sig);
    pthread_mutex_unlock(&task->lock);

    free_task(task);
    return 0;
}

int do_killpg(pid_t pgrp, int sig) {
    if (sig < 0 || sig >= IX_NSIG) {
        errno = EINVAL;
        return -1;
    }

    if (pgrp <= 0) {
        errno = EINVAL;
        return -1;
    }

    int check_only = (sig == 0);
    int match_count = 0;
    int found = 0;

    pthread_mutex_lock(&task_table_lock);

    for (int i = 0; i < TASK_MAX_TASKS; i++) {
        struct task_struct *task = task_table[i];
        while (task) {
            if (task->pgid == pgrp) {
                found = 1;
                if (!check_only) {
                    atomic_fetch_add(&task->refs, 1);
                    pthread_mutex_lock(&task->lock);
                    apply_signal_to_task(task, sig);
                    pthread_mutex_unlock(&task->lock);
                    free_task(task);
                    match_count++;
                }
            }
            task = task->hash_next;
        }
    }

    pthread_mutex_unlock(&task_table_lock);

    if (!found) {
        errno = ESRCH;
        return -1;
    }

    return 0;
}

int do_sigprocmask(int how, const ix_sigset_t *set, ix_sigset_t *oldset) {
    struct task_struct *task = get_current();
    if (!task || !task->sighand) {
        errno = ESRCH;
        return -1;
    }

    struct sighand_struct *sighand = task->sighand;

    if (oldset) {
        *oldset = sighand->blocked;
    }

    if (set) {
        switch (how) {
        case 0: /* SIG_BLOCK */
            for (int i = 0; i < IX_NSIG / 64 + 1; i++) {
                sighand->blocked.sig[i] |= set->sig[i];
            }
            break;
        case 1: /* SIG_UNBLOCK */
            for (int i = 0; i < IX_NSIG / 64 + 1; i++) {
                sighand->blocked.sig[i] &= ~set->sig[i];
            }
            break;
        case 2: /* SIG_SETMASK */
            sighand->blocked = *set;
            break;
        default:
            errno = EINVAL;
            return -1;
        }
    }

    return 0;
}

int do_sigpending(ix_sigset_t *set) {
    if (!set) {
        errno = EFAULT;
        return -1;
    }

    struct task_struct *task = get_current();
    if (!task || !task->sighand) {
        errno = ESRCH;
        return -1;
    }

    *set = task->sighand->pending;
    return 0;
}

ix_sighandler_t do_signal(int signum, ix_sighandler_t handler) {
    if (signum < 1 || signum >= IX_NSIG) {
        errno = EINVAL;
        return NULL;
    }

    if (signum == 9 || signum == 19) { /* SIGKILL, SIGSTOP */
        errno = EINVAL;
        return NULL;
    }

    struct task_struct *task = get_current();
    if (!task || !task->sighand) {
        errno = ESRCH;
        return NULL;
    }

    ix_sighandler_t old_handler = task->sighand->action[signum].sa_handler;
    task->sighand->action[signum].sa_handler = handler;
    task->sighand->action[signum].sa_flags = 0;
    memset(&task->sighand->action[signum].sa_mask, 0, sizeof(ix_sigset_t));

    return old_handler;
}

int do_raise(int sig) {
    struct task_struct *task = get_current();
    if (!task) {
        errno = ESRCH;
        return -1;
    }
    return do_kill(task->pid, sig);
}

static int is_sigset_empty(const ix_sigset_t *set) {
    for (int i = 0; i < IX_NSIG / 64 + 1; i++) {
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

    while (is_sigset_empty(&task->sighand->pending)) {
        task->waiters++;
        pthread_cond_wait(&task->wait_cond, &task->wait_lock);
        task->waiters--;
    }

    pthread_mutex_unlock(&task->wait_lock);

    errno = EINTR;
    return -1;
}

int do_sigsuspend(const ix_sigset_t *mask) {
    struct task_struct *task = get_current();
    if (!task || !task->sighand) {
        errno = ESRCH;
        return -1;
    }

    if (!mask) {
        errno = EFAULT;
        return -1;
    }

    /* Save old mask */
    ix_sigset_t old_mask = task->sighand->blocked;

    /* Install new mask */
    task->sighand->blocked = *mask;

    /* Wait for signal */
    pthread_mutex_lock(&task->wait_lock);
    task->waiters++;
    pthread_cond_wait(&task->wait_cond, &task->wait_lock);
    task->waiters--;
    pthread_mutex_unlock(&task->wait_lock);

    /* Restore old mask */
    task->sighand->blocked = old_mask;

    errno = EINTR;
    return -1;
}

void force_sig(int sig, struct task_struct *task) {
    if (!task || !task->sighand)
        return;

    pthread_mutex_lock(&task->lock);
    apply_signal_to_task(task, sig);
    pthread_mutex_unlock(&task->lock);
}

void signal_init(void) {
    /* Signal subsystem initialization */
}

void signal_deinit(void) {
    /* Signal subsystem cleanup */
}
