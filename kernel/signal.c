#include "signal.h"

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

ixland_sighand_t *ixland_sighand_alloc(void) {
    ixland_sighand_t *sighand = calloc(1, sizeof(ixland_sighand_t));
    if (!sighand)
        return NULL;

    atomic_init(&sighand->refs, 1);
    pthread_mutex_init(&sighand->queue.lock, NULL);

    /* Initialize default handlers */
    for (int i = 0; i < IXLAND_NSIG; i++) {
        sighand->action[i].sa_handler = SIG_DFL;
        sigemptyset(&sighand->action[i].sa_mask);
        sighand->action[i].sa_flags = 0;
    }

    sigemptyset(&sighand->blocked);
    sigemptyset(&sighand->pending);

    return sighand;
}

void ixland_sighand_free(ixland_sighand_t *sighand) {
    if (!sighand)
        return;
    if (atomic_fetch_sub(&sighand->refs, 1) > 1)
        return;

    /* Free queued signals */
    pthread_mutex_lock(&sighand->queue.lock);
    ixland_sigqueue_entry_t *entry = sighand->queue.head;
    while (entry) {
        ixland_sigqueue_entry_t *next = entry->next;
        free(entry);
        entry = next;
    }
    pthread_mutex_unlock(&sighand->queue.lock);

    pthread_mutex_destroy(&sighand->queue.lock);
    free(sighand);
}

ixland_sighand_t *ixland_sighand_dup(ixland_sighand_t *parent) {
    if (!parent)
        return NULL;

    ixland_sighand_t *child = ixland_sighand_alloc();
    if (!child)
        return NULL;

    /* Copy signal handlers */
    memcpy(child->action, parent->action, sizeof(child->action));

    /* Child inherits parent's signal mask */
    child->blocked = parent->blocked;

    /* But pending signals are cleared */
    sigemptyset(&child->pending);

    return child;
}

int ixland_sigaction(int sig, const struct sigaction *act, struct sigaction *oldact) {
    if (sig < 1 || sig >= IXLAND_NSIG) {
        errno = EINVAL;
        return -1;
    }

    if (sig == SIGKILL || sig == SIGSTOP) {
        errno = EINVAL;
        return -1;
    }

    ixland_task_t *task = ixland_current_task();
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

/* Apply signal to a single task with state transitions and parent notification.
 * Must be called with task lock held. Does NOT release task reference.
 */
static void __ixland_apply_signal_to_task(ixland_task_t *task, int sig) {
    int terminating = (sig == SIGTERM || sig == SIGKILL || sig == SIGINT);
    /* Add signal to pending */
    sigaddset(&task->sighand->pending, sig);

    /* Handle SIGSTOP: transition to STOPPED state (not reaped) */
    if (sig == SIGSTOP) {
        atomic_store(&task->state, IXLAND_TASK_STOPPED);
        atomic_store(&task->stopped, true);
        atomic_store(&task->stopsig, SIGSTOP);
    }

    /* Handle SIGCONT: transition back to RUNNING (not reaped) */
    if (sig == SIGCONT) {
        atomic_store(&task->state, IXLAND_TASK_RUNNING);
        atomic_store(&task->stopped, false);
        atomic_store(&task->continued, true);
    }

    /* Handle terminating signals: mark for reap on wait (SIGKILL, SIGTERM, SIGINT) */
    if (terminating) {
        atomic_store(&task->signaled, true);
        atomic_store(&task->termsig, sig);
        atomic_store(&task->exited, true);
        atomic_store(&task->state, IXLAND_TASK_ZOMBIE);
    }

    /* Notify parent if this child state is wait-visible */
    int state_changed = (sig == SIGSTOP) || (sig == SIGCONT) || atomic_load(&task->signaled);
    if (state_changed && task->parent) {
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

    /* Deliver signal to target thread via pthread_kill for actual signal handling.
     * This ensures the target thread's signal handler is invoked if not blocked.
     * We use SIGUSR1 as the notification signal - the task's signal handling
     * infrastructure will process the actual pending signal from sighand->pending.
     */
    pthread_kill(task->thread, SIGUSR1);
}

int ixland_kill(pid_t pid, int sig) {
    if (sig < 0 || sig >= IXLAND_NSIG) {
        errno = EINVAL;
        return -1;
    }

    if (pid <= 0) {
        /* Process group handling */
        if (pid == 0) {
            /* Current process group */
            ixland_task_t *task = ixland_current_task();
            if (!task) {
                errno = ESRCH;
                return -1;
            }
            return ixland_killpg(task->pgid, sig);
        } else if (pid == -1) {
            /* All processes (privileged) */
            errno = EPERM;
            return -1;
        } else {
            /* Process group |pid| */
            return ixland_killpg(-pid, sig);
        }
    }

    ixland_task_t *task = ixland_task_lookup(pid);
    if (!task) {
        errno = ESRCH;
        return -1;
    }

    if (sig == 0) {
        /* Just check if process exists */
        ixland_task_free(task);
        return 0;
    }

    /* Apply signal with state transitions and notifications */
    pthread_mutex_lock(&task->lock);
    __ixland_apply_signal_to_task(task, sig);
    pthread_mutex_unlock(&task->lock);

    ixland_task_free(task);
    return 0;
}

int ixland_killpg(pid_t pgrp, int sig) {
    /* Contract:
     * - pgrp <= 0: return -1, errno = EINVAL
     * - no matching tasks: return -1, errno = ESRCH
     * - valid PGID with matches: deliver to all, return 0
     */

    /* Validate signal number */
    if (sig < 0 || sig >= IXLAND_NSIG) {
        errno = EINVAL;
        return -1;
    }

    /* Validate process group ID (must be positive) */
    if (pgrp <= 0) {
        errno = EINVAL;
        return -1;
    }

    /* Signal 0 is used to check if process group exists */
    int check_only = (sig == 0);

/* Collect matching tasks under lock */
#define IXLAND_KILLPG_MAX_MATCHES 256
    ixland_task_t *matches[IXLAND_KILLPG_MAX_MATCHES];
    int match_count = 0;

    extern pthread_mutex_t task_table_lock;
    extern ixland_task_t *task_table[];
    extern int task_hash(pid_t pid);

    pthread_mutex_lock(&task_table_lock);

    /* Iterate all buckets in task table */
    for (int i = 0; i < IXLAND_MAX_TASKS; i++) {
        ixland_task_t *task = task_table[i];
        while (task) {
            if (task->pgid == pgrp) {
                if (match_count < IXLAND_KILLPG_MAX_MATCHES) {
                    atomic_fetch_add(&task->refs, 1);
                    matches[match_count++] = task;
                }
                /* If we hit the limit, continue counting but don't store */
            }
            task = task->hash_next;
        }
    }

    pthread_mutex_unlock(&task_table_lock);

    /* No matching tasks found */
    if (match_count == 0) {
        errno = ESRCH;
        return -1;
    }

    /* Signal 0: just checking existence, don't actually deliver */
    if (check_only) {
        for (int i = 0; i < match_count; i++) {
            ixland_task_free(matches[i]);
        }
        return 0;
    }

    /* Deliver signal to each matched task using shared helper */
    for (int i = 0; i < match_count; i++) {
        ixland_task_t *task = matches[i];

        pthread_mutex_lock(&task->lock);
        __ixland_apply_signal_to_task(task, sig);
        pthread_mutex_unlock(&task->lock);

        ixland_task_free(matches[i]);
    }

    return 0;
}

int ixland_sigprocmask(int how, const sigset_t *set, sigset_t *oldset) {
    ixland_task_t *task = ixland_current_task();
    if (!task || !task->sighand) {
        errno = ESRCH;
        return -1;
    }

    if (oldset) {
        *oldset = task->sighand->blocked;
    }

    if (set) {
        switch (how) {
        case SIG_BLOCK:
            for (int i = 1; i < IXLAND_NSIG; i++) {
                if (sigismember(set, i)) {
                    sigaddset(&task->sighand->blocked, i);
                }
            }
            break;
        case SIG_UNBLOCK:
            for (int i = 1; i < IXLAND_NSIG; i++) {
                if (sigismember(set, i)) {
                    sigdelset(&task->sighand->blocked, i);
                }
            }
            break;
        case SIG_SETMASK:
            task->sighand->blocked = *set;
            break;
        default:
            errno = EINVAL;
            return -1;
        }
    }

    return 0;
}

int ixland_sigpending(sigset_t *set) {
    if (!set) {
        errno = EFAULT;
        return -1;
    }

    ixland_task_t *task = ixland_current_task();
    if (!task || !task->sighand) {
        errno = ESRCH;
        return -1;
    }

    *set = task->sighand->pending;
    return 0;
}

/* ============================================================================
 * SIGNAL - Simple signal handler installation
 * ============================================================================ */

ixland_sighandler_t ixland_signal(int signum, ixland_sighandler_t handler) {
    if (signum < 1 || signum >= IXLAND_NSIG) {
        errno = EINVAL;
        return SIG_ERR;
    }

    if (signum == SIGKILL || signum == SIGSTOP) {
        errno = EINVAL;
        return SIG_ERR;
    }

    ixland_task_t *task = ixland_current_task();
    if (!task || !task->sighand) {
        errno = ESRCH;
        return SIG_ERR;
    }

    /* Get old handler */
    ixland_sighandler_t old_handler = task->sighand->action[signum].sa_handler;

    /* Install new handler */
    task->sighand->action[signum].sa_handler = handler;
    task->sighand->action[signum].sa_flags = 0;
    sigemptyset(&task->sighand->action[signum].sa_mask);

    return old_handler;
}

/* ============================================================================
 * RAISE - Send signal to current process
 * ============================================================================ */

int ixland_raise(int sig) {
    ixland_task_t *task = ixland_current_task();
    if (!task) {
        errno = ESRCH;
        return -1;
    }
    return ixland_kill(task->pid, sig);
}

/* ============================================================================
 * ALARM - Set alarm timer (simulated)
 *
 * Note: iOS doesn't support POSIX timers (timer_create/timer_delete).
 * We implement a simplified version that tracks alarm state without
 * actual timer delivery.
 * ============================================================================ */

static pthread_mutex_t alarm_lock = PTHREAD_MUTEX_INITIALIZER;
static unsigned int alarm_remaining = 0;
static time_t alarm_set_time = 0;
static int alarm_active = 0;

unsigned int ixland_alarm(unsigned int seconds) {
    pthread_mutex_lock(&alarm_lock);

    unsigned int old_remaining = 0;

    /* Calculate remaining time from previous alarm */
    if (alarm_active && alarm_set_time > 0) {
        time_t elapsed = time(NULL) - alarm_set_time;
        if (elapsed < (time_t)alarm_remaining) {
            old_remaining = alarm_remaining - (unsigned int)elapsed;
        }
    }

    /* Cancel existing alarm */
    alarm_active = 0;
    alarm_remaining = 0;
    alarm_set_time = 0;

    if (seconds == 0) {
        /* Just cancel, don't set new alarm */
        pthread_mutex_unlock(&alarm_lock);
        return old_remaining;
    }

    /* Set new alarm state */
    alarm_remaining = seconds;
    alarm_set_time = time(NULL);
    alarm_active = 1;

    /* Note: On iOS, we cannot create real timers that deliver signals.
     * The alarm is tracked but not actively delivered.
     * In a complete implementation, a background thread would
     * monitor alarms and deliver SIGALRM when they expire.
     */

    pthread_mutex_unlock(&alarm_lock);
    return old_remaining;
}

/* Helper to check if sigset is empty (macOS/iOS doesn't have sigisemptyset) */
static int sigset_is_empty(const sigset_t *set) {
    for (int i = 1; i < IXLAND_NSIG; i++) {
        if (sigismember(set, i)) {
            return 0;
        }
    }
    return 1;
}

/* ============================================================================
 * PAUSE - Wait for signal
 * ============================================================================ */

int ixland_pause(void) {
    ixland_task_t *task = ixland_current_task();
    if (!task) {
        errno = ESRCH;
        return -1;
    }

    /* Block until a signal is delivered */
    /* In our simulation, we use a condition variable that's signaled on signal delivery */
    pthread_mutex_lock(&task->wait_lock);

    /* Wait for a signal to be delivered */
    /* The signal delivery mechanism will wake us up */
    while (sigset_is_empty(&task->sighand->pending)) {
        task->waiters++;
        pthread_cond_wait(&task->wait_cond, &task->wait_lock);
        task->waiters--;
    }

    pthread_mutex_unlock(&task->wait_lock);

    /* Always returns -1 with EINTR when signal is caught */
    errno = EINTR;
    return -1;
}

/* ============================================================================
 * SIGSUSPEND - Atomically replace mask and wait for signal
 * ============================================================================ */

int ixland_sigsuspend(const sigset_t *mask) {
    ixland_task_t *task = ixland_current_task();
    if (!task || !task->sighand) {
        errno = ESRCH;
        return -1;
    }

    if (!mask) {
        errno = EFAULT;
        return -1;
    }

    /* Save old mask */
    sigset_t old_mask = task->sighand->blocked;

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

    /* Always returns -1 with EINTR */
    errno = EINTR;
    return -1;
}
/* IXLand Signal Mask Implementation
 * Canonical signal mask operations
 * Linux-shaped semantics, Darwin boundary conversion at edge
 */

#include <errno.h>
#include <pthread.h>
#include <string.h>

#include "../include/ixland/ixland_signal.h"

static __thread ixland_sigset_t thread_sigmask;
static __thread bool thread_sigmask_initialized = false;

/* Initialize thread signal mask on first use */
static void ensure_sigmask_initialized(void) {
    if (!thread_sigmask_initialized) {
        ixland_sigemptyset(&thread_sigmask);
        thread_sigmask_initialized = true;
    }
}

/* Signal mask operations */
void ixland_sigemptyset(ixland_sigset_t *set) {
    memset(set->sig, 0, sizeof(set->sig));
}

void ixland_sigfillset(ixland_sigset_t *set) {
    memset(set->sig, ~0UL, sizeof(set->sig));
}

int ixland_sigaddset(ixland_sigset_t *set, int sig) {
    if (sig < 1 || sig >= IXLAND_NSIG) {
        errno = EINVAL;
        return -1;
    }
    unsigned long word = (sig - 1) / (sizeof(unsigned long) * 8);
    unsigned long bit = (sig - 1) % (sizeof(unsigned long) * 8);
    set->sig[word] |= (1UL << bit);
    return 0;
}

int ixland_sigdelset(ixland_sigset_t *set, int sig) {
    if (sig < 1 || sig >= IXLAND_NSIG) {
        errno = EINVAL;
        return -1;
    }
    unsigned long word = (sig - 1) / (sizeof(unsigned long) * 8);
    unsigned long bit = (sig - 1) % (sizeof(unsigned long) * 8);
    set->sig[word] &= ~(1UL << bit);
    return 0;
}

int ixland_sigismember(const ixland_sigset_t *set, int sig) {
    if (sig < 1 || sig >= IXLAND_NSIG) {
        errno = EINVAL;
        return -1;
    }
    unsigned long word = (sig - 1) / (sizeof(unsigned long) * 8);
    unsigned long bit = (sig - 1) % (sizeof(unsigned long) * 8);
    return (set->sig[word] & (1UL << bit)) != 0;
}

/* Signal mask logic operations */
void ixland_sigandset(ixland_sigset_t *dest, const ixland_sigset_t *left,
                      const ixland_sigset_t *right) {
    for (size_t i = 0; i < IXLAND_NSIG_WORDS; i++) {
        dest->sig[i] = left->sig[i] & right->sig[i];
    }
}

void ixland_sigorset(ixland_sigset_t *dest, const ixland_sigset_t *left,
                     const ixland_sigset_t *right) {
    for (size_t i = 0; i < IXLAND_NSIG_WORDS; i++) {
        dest->sig[i] = left->sig[i] | right->sig[i];
    }
}

void ixland_signotset(ixland_sigset_t *dest, const ixland_sigset_t *src) {
    for (size_t i = 0; i < IXLAND_NSIG_WORDS; i++) {
        dest->sig[i] = ~src->sig[i];
    }
}

/* Darwin boundary conversion - host mediation edge only */
void ixland_sigset_to_host(const ixland_sigset_t *ixland_set, sigset_t *host_set) {
    sigemptyset(host_set);
    for (int sig = 1; sig < IXLAND_NSIG && sig < NSIG; sig++) {
        if (ixland_sigismember(ixland_set, sig)) {
            sigaddset(host_set, sig);
        }
    }
}

void ixland_sigset_from_host(const sigset_t *host_set, ixland_sigset_t *ixland_set) {
    ixland_sigemptyset(ixland_set);
    for (int sig = 1; sig < IXLAND_NSIG && sig < NSIG; sig++) {
        if (sigismember(host_set, sig)) {
            ixland_sigaddset(ixland_set, sig);
        }
    }
}

/* Thread-local signal mask accessor */
ixland_sigset_t *ixland_thread_sigmask(void) {
    ensure_sigmask_initialized();
    return &thread_sigmask;
}
