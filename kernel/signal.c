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

#include "task.h"

struct sighand_struct *alloc_sighand(void) {
    struct sighand_struct *sighand = calloc(1, sizeof(struct sighand_struct));
    if (!sighand)
        return NULL;

    atomic_init(&sighand->refs, 1);
    pthread_mutex_init(&sighand->queue.lock, NULL);

    /* Initialize default handlers */
    for (int i = 0; i < _NSIG; i++) {
        sighand->action[i].handler = NULL; /* SIG_DFL equivalent */
        memset(&sighand->action[i].mask, 0, sizeof(ix_sigset_t));
        sighand->action[i].flags = 0;
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

int do_sigaction(int sig, const struct k_sigaction *act, struct k_sigaction *oldact) {
    if (sig < 1 || sig >= _NSIG) {
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
    if (sig < 0 || sig >= _NSIG) {
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
    if (sig < 0 || sig >= _NSIG) {
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
            for (int i = 0; i < _NSIG / 64 + 1; i++) {
                sighand->blocked.sig[i] |= set->sig[i];
            }
            break;
        case 1: /* SIG_UNBLOCK */
            for (int i = 0; i < _NSIG / 64 + 1; i++) {
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
    if (signum < 1 || signum >= _NSIG) {
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

    ix_sighandler_t old_handler = task->sighand->action[signum].handler;
    task->sighand->action[signum].handler = handler;
    task->sighand->action[signum].flags = 0;
    memset(&task->sighand->action[signum].mask, 0, sizeof(ix_sigset_t));

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
    for (int i = 0; i < _NSIG / 64 + 1; i++) {
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

/* ============================================================================
 * PUBLIC CANONICAL SIGNAL WRAPPERS
 * ============================================================================
 * These wrappers convert between POSIX/Linux public signal types and
 * IXLandSystem's internal representation. The internal owner functions
 * use ix_sigset_t and k_sigaction; the public ABI uses sigset_t and
 * struct sigaction from the host platform.
 */

#include <signal.h>
#include <string.h>

/* sighandler_t may not be defined on all platforms */
#ifndef sighandler_t
typedef void (*sighandler_t)(int);
#endif

/* SIG_ERR may not be defined on all platforms */
#ifndef SIG_ERR
#define SIG_ERR ((sighandler_t)-1)
#endif

/* Convert Linux sigset_t to internal ix_sigset_t */
static void sigset_to_ix(const sigset_t *linux_set, ix_sigset_t *ix_set) {
    memset(ix_set, 0, sizeof(*ix_set));
    if (!linux_set) return;

    for (int sig = 1; sig < 64 && sig < _NSIG; sig++) {
        if (sigismember(linux_set, sig)) {
            int idx = sig / 64;
            int bit = sig % 64;
            if (idx < (_NSIG / 64 + 1)) {
                ix_set->sig[idx] |= (1ULL << bit);
            }
        }
    }
}

/* Convert internal ix_sigset_t to Linux sigset_t */
static void ix_to_sigset(const ix_sigset_t *ix_set, sigset_t *linux_set) {
    if (!linux_set) return;
    sigemptyset(linux_set);
    if (!ix_set) return;

    for (int sig = 1; sig < 64 && sig < _NSIG; sig++) {
        int idx = sig / 64;
        int bit = sig % 64;
        if (idx < (_NSIG / 64 + 1)) {
            if (ix_set->sig[idx] & (1ULL << bit)) {
                sigaddset(linux_set, sig);
            }
        }
    }
}

/* Convert Linux struct sigaction fields to internal k_sigaction */
static void sigaction_to_k(const struct sigaction *linux_act, struct k_sigaction *k_act) {
    memset(k_act, 0, sizeof(*k_act));
    if (!linux_act) return;

    k_act->handler = linux_act->sa_handler;
    sigset_to_ix(&linux_act->sa_mask, &k_act->mask);
    k_act->flags = linux_act->sa_flags;
}

/* Convert internal k_sigaction to Linux struct sigaction */
static void k_to_sigaction(const struct k_sigaction *k_act, struct sigaction *linux_act) {
    memset(linux_act, 0, sizeof(*linux_act));
    if (!k_act) return;

    linux_act->sa_handler = k_act->handler;
    ix_to_sigset(&k_act->mask, &linux_act->sa_mask);
    linux_act->sa_flags = k_act->flags;
}

static int sigaction_impl(int signum, const struct sigaction *act, struct sigaction *oldact) {
    struct k_sigaction k_act, k_oldact;
    struct k_sigaction *k_act_ptr = NULL;
    struct k_sigaction *k_oldact_ptr = NULL;

    if (signum < 1 || signum >= _NSIG) {
        errno = EINVAL;
        return -1;
    }

    if (signum == 9 || signum == 19) {
        errno = EINVAL;
        return -1;
    }

    if (act) {
        sigaction_to_k(act, &k_act);
        k_act_ptr = &k_act;
    }

    if (oldact) {
        k_oldact_ptr = &k_oldact;
    }

    int result = do_sigaction(signum, k_act_ptr, k_oldact_ptr);

    if (oldact && result == 0) {
        k_to_sigaction(&k_oldact, oldact);
    }

    return result;
}

static sighandler_t signal_impl(int signum, sighandler_t handler) {
    if (signum < 1 || signum >= _NSIG) {
        errno = EINVAL;
        return SIG_ERR;
    }

    if (signum == 9 || signum == 19) {
        errno = EINVAL;
        return SIG_ERR;
    }

    ix_sighandler_t old_ix_handler = do_signal(signum, handler);
    return (sighandler_t)old_ix_handler;
}

static int sigprocmask_impl(int how, const sigset_t *set, sigset_t *oldset) {
    ix_sigset_t ix_set, ix_oldset;
    ix_sigset_t *ix_set_ptr = NULL;
    ix_sigset_t *ix_oldset_ptr = NULL;

    if (set) {
        sigset_to_ix(set, &ix_set);
        ix_set_ptr = &ix_set;
    }

    if (oldset) {
        ix_oldset_ptr = &ix_oldset;
    }

    int result = do_sigprocmask(how, ix_set_ptr, ix_oldset_ptr);

    if (oldset && result == 0) {
        ix_to_sigset(&ix_oldset, oldset);
    }

    return result;
}

static int sigpending_impl(sigset_t *set) {
    if (!set) {
        errno = EFAULT;
        return -1;
    }

    ix_sigset_t ix_set;
    int result = do_sigpending(&ix_set);

    if (result == 0) {
        ix_to_sigset(&ix_set, set);
    }

    return result;
}

static int sigsuspend_impl(const sigset_t *mask) {
    if (!mask) {
        errno = EFAULT;
        return -1;
    }

    ix_sigset_t ix_mask;
    sigset_to_ix(mask, &ix_mask);

    return do_sigsuspend(&ix_mask);
}

__attribute__((visibility("default"))) int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact) {
    return sigaction_impl(signum, act, oldact);
}

__attribute__((visibility("default"))) sighandler_t signal(int signum, sighandler_t handler) {
    return signal_impl(signum, handler);
}

__attribute__((visibility("default"))) int kill(pid_t pid, int sig) {
    return do_kill(pid, sig);
}

__attribute__((visibility("default"))) int killpg(pid_t pgrp, int sig) {
    return do_killpg(pgrp, sig);
}

__attribute__((visibility("default"))) int sigprocmask(int how, const sigset_t *set, sigset_t *oldset) {
    return sigprocmask_impl(how, set, oldset);
}

__attribute__((visibility("default"))) int sigpending(sigset_t *set) {
    return sigpending_impl(set);
}

__attribute__((visibility("default"))) int sigsuspend(const sigset_t *mask) {
    return sigsuspend_impl(mask);
}

__attribute__((visibility("default"))) int raise(int sig) {
    return do_raise(sig);
}

__attribute__((visibility("default"))) int pause(void) {
    return do_pause();
}
