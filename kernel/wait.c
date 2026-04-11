#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/wait.h>

#include "task.h"

static int task_to_status(ixland_task_t *task) {
    int status = 0;

    if (atomic_load(&task->signaled)) {
        /* Child was terminated by signal */
        status = task->termsig;
    } else if (atomic_load(&task->stopped)) {
        /* Child is stopped */
        status = (task->stopsig << 8) | 0x7f;
    } else if (atomic_load(&task->continued)) {
        /* Child continued (reported via wait with WCONTINUED) */
        status = (SIGCONT << 8) | 0x7f;
    } else {
        /* Normal exit */
        status = (task->exit_status & 0xFF) << 8;
    }

    return status;
}

/* Status check helpers */
#define W_STOPPED(status) (((status) & 0xff) == 0x7f)
#define W_CONTINUED(status) ((status) == 0xffff)

pid_t ixland_waitpid(pid_t pid, int *wstatus, int options) {
    ixland_task_t *parent = ixland_current_task();
    if (!parent) {
        errno = ESRCH;
        return -1;
    }

    ixland_task_t *child = NULL;

    pthread_mutex_lock(&parent->lock);
    parent->waiters++;

    while (1) {
        /* Find matching child based on pid selector */
        if (pid > 0) {
            /* Wait for specific child */
            child = parent->children;
            while (child && child->pid != pid) {
                child = child->next_sibling;
            }
        } else if (pid == -1) {
            /* Wait for any child - iterate to find one matching state criteria */
            child = parent->children;
            while (child) {
                if (atomic_load(&child->exited) ||
                    ((options & WUNTRACED) && atomic_load(&child->stopped)) ||
                    ((options & WCONTINUED) && atomic_load(&child->continued))) {
                    break;
                }
                child = child->next_sibling;
            }
        } else if (pid == 0) {
            /* Wait for any child in same process group */
            child = parent->children;
            while (child) {
                if (child->pgid == parent->pgid &&
                    (atomic_load(&child->exited) ||
                     ((options & WUNTRACED) && atomic_load(&child->stopped)) ||
                     ((options & WCONTINUED) && atomic_load(&child->continued)))) {
                    break;
                }
                child = child->next_sibling;
            }
        } else {
            /* pid < -1: Wait for any child in process group |pid| */
            pid_t pgid = -pid;
            child = parent->children;
            while (child) {
                if (child->pgid == pgid &&
                    (atomic_load(&child->exited) ||
                     ((options & WUNTRACED) && atomic_load(&child->stopped)) ||
                     ((options & WCONTINUED) && atomic_load(&child->continued)))) {
                    break;
                }
                child = child->next_sibling;
            }
        }

        /* Check if we found a matching child */
        if (child) {
            /* Check for exited child */
            if (atomic_load(&child->exited)) {
                break;
            }

            /* Check for stopped child (WUNTRACED) */
            if ((options & WUNTRACED) && atomic_load(&child->stopped) &&
                !atomic_load(&child->exited)) {
                break;
            }

            /* Check for continued child (WCONTINUED) */
            if ((options & WCONTINUED) && atomic_load(&child->continued) &&
                !atomic_load(&child->exited) && !atomic_load(&child->stopped)) {
                break;
            }
        }

        /* No matching exited child */
        if (options & WNOHANG) {
            parent->waiters--;
            pthread_mutex_unlock(&parent->lock);
            return 0;
        }

        if (!parent->children) {
            /* No children at all */
            parent->waiters--;
            pthread_mutex_unlock(&parent->lock);
            errno = ECHILD;
            return -1;
        }

        /* Wait for child to exit */
        if (options & WNOHANG) {
            /* Non-blocking: just check once */
            parent->waiters--;
            pthread_mutex_unlock(&parent->lock);
            return 0;
        }

        /* Block waiting for child */
        pthread_cond_wait(&parent->wait_cond, &parent->lock);
    }

    /* Determine if child should be reaped (exited/signaled) or just reported (stopped/continued) */
    int should_reap = atomic_load(&child->exited) || atomic_load(&child->signaled);

    /* Only unlink and free terminated children */
    if (should_reap) {
        ixland_task_t **pp = &parent->children;
        while (*pp && *pp != child) {
            pp = &(*pp)->next_sibling;
        }
        if (*pp) {
            *pp = child->next_sibling;
        }
    }

    parent->waiters--;
    pthread_mutex_unlock(&parent->lock);

    /* Return status */
    if (wstatus) {
        *wstatus = task_to_status(child);
    }

    pid_t child_pid = child->pid;

    /* Only free terminated children */
    if (should_reap) {
        ixland_task_free(child);
    }

    return child_pid;
}

pid_t ixland_wait4(pid_t pid, int *wstatus, int options, struct rusage *rusage) {
    /* rusage not implemented yet - just call waitpid */
    (void)rusage;
    return ixland_waitpid(pid, wstatus, options);
}

pid_t ixland_wait(int *wstatus) {
    return ixland_waitpid(-1, wstatus, 0);
}

pid_t ixland_wait3(int *wstatus, int options, struct rusage *rusage) {
    return ixland_wait4(-1, wstatus, options, rusage);
}
