/*
 * IXLandSystem Syscall Adapter
 *
 * Bridges public package seam to internal owners.
 * These functions are the ONLY entry points IXLandLibC may call.
 */

#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "../include/ixland/ixland_types.h"
#include "../kernel/signal.h"

/* sighandler_t may not be defined on all systems */
#ifndef sighandler_t
typedef void (*sighandler_t)(int);
#endif

/* SIG_ERR may not be defined on all systems */
#ifndef SIG_ERR
#define SIG_ERR ((sighandler_t)-1)
#endif

/* ============================================================================
 * Internal owner declarations (from kernel/)
 * ============================================================================ */
extern pid_t fork_impl(void);
extern int vfork_impl(void);
extern void exit_impl(int status);
/* exit_group not implemented yet - _exit calls _Exit directly */
extern pid_t getpid_impl(void);
extern pid_t getppid_impl(void);
extern pid_t getpgrp_impl(void);
extern pid_t getpgid_impl(pid_t pid);
extern int setpgid_impl(pid_t pid, pid_t pgid);
extern pid_t setsid_impl(void);
extern pid_t getsid_impl(pid_t pid);
extern pid_t wait_impl(int *wstatus);
extern pid_t waitpid_impl(pid_t pid, int *wstatus, int options);
extern pid_t wait4_impl(pid_t pid, int *wstatus, int options, struct rusage *rusage);

/* Signal helper declarations and implementations in static section below */

/* From fs/exec.c - execve, execv, execvp, fexecve are canonical and exported directly */

/* From kernel/init.c */
extern int ixland_init_internal(const ixland_config_t *config);
extern void ixland_cleanup_internal(void);
extern const char *ixland_version_internal(void);
extern int ixland_is_initialized_internal(void);

/* ============================================================================
 * PROCESS CREATION
 * ============================================================================ */

__attribute__((visibility("default"))) pid_t fork(void) {
    return fork_impl();
}

__attribute__((visibility("default"))) int vfork(void) {
    return vfork_impl();
}

/* ============================================================================
 * PROCESS TERMINATION
 * ============================================================================ */

__attribute__((visibility("default"), __noreturn__)) void exit(int status) {
    exit_impl(status);
}

__attribute__((visibility("default"))) void _exit(int status) {
    /* Immediate exit without cleanup - exit_group not implemented */
    _Exit(status);
}

/* ============================================================================
 * PROCESS IDENTIFICATION
 * ============================================================================ */

__attribute__((visibility("default"))) pid_t getpid(void) {
    return getpid_impl();
}

__attribute__((visibility("default"))) pid_t getppid(void) {
    return getppid_impl();
}

/* ============================================================================
 * PROCESS GROUPS
 * ============================================================================ */

__attribute__((visibility("default"))) pid_t getpgrp(void) {
    return getpgrp_impl();
}

__attribute__((visibility("default"))) pid_t getpgid(pid_t pid) {
    return getpgid_impl(pid);
}

__attribute__((visibility("default"))) int setpgid(pid_t pid, pid_t pgid) {
    return setpgid_impl(pid, pgid);
}

/* ============================================================================
 * SESSIONS
 * ============================================================================ */

__attribute__((visibility("default"))) pid_t setsid(void) {
    return setsid_impl();
}

__attribute__((visibility("default"))) pid_t getsid(pid_t pid) {
    return getsid_impl(pid);
}

/* ============================================================================
 * WAIT
 * ============================================================================ */

__attribute__((visibility("default"))) pid_t wait(int *wstatus) {
    return wait_impl(wstatus);
}

__attribute__((visibility("default"))) pid_t waitpid(pid_t pid, int *wstatus, int options) {
    return waitpid_impl(pid, wstatus, options);
}

__attribute__((visibility("default"))) pid_t wait4(pid_t pid, int *wstatus, int options, struct rusage *rusage) {
    return wait4_impl(pid, wstatus, options, rusage);
}

__attribute__((visibility("default"))) pid_t wait3(int *wstatus, int options, struct rusage *rusage) {
    return wait4_impl(-1, wstatus, options, rusage);
}

/* ============================================================================
 * SIGNAL HELPER IMPLEMENTATIONS (type conversion layer)
 * ============================================================================
 * These static helper functions bridge between Linux/POSIX public signal types
 * and internal IXLand signal representations.
 */

/* Convert Linux sigset_t to internal ix_sigset_t */
static void sigset_to_ix(const sigset_t *linux_set, ix_sigset_t *ix_set) {
    memset(ix_set, 0, sizeof(*ix_set));
    if (!linux_set) return;

    /* Copy signal bits - both use bit masks, copy first 64 signals */
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

    /* Copy signal bits from internal to Linux sigset_t */
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

    /* Copy handler */
    k_act->handler = linux_act->sa_handler;

    /* Copy signal mask */
    sigset_to_ix(&linux_act->sa_mask, &k_act->mask);

    /* Copy flags */
    k_act->flags = linux_act->sa_flags;
}

/* Convert internal k_sigaction to Linux struct sigaction */
static void k_to_sigaction(const struct k_sigaction *k_act, struct sigaction *linux_act) {
    memset(linux_act, 0, sizeof(*linux_act));
    if (!k_act) return;

    /* Copy handler */
    linux_act->sa_handler = k_act->handler;

    /* Copy signal mask */
    ix_to_sigset(&k_act->mask, &linux_act->sa_mask);

    /* Copy flags */
    linux_act->sa_flags = k_act->flags;
}

static int sigaction_impl(int signum, const struct sigaction *act, struct sigaction *oldact) {
    struct k_sigaction k_act, k_oldact;
    struct k_sigaction *k_act_ptr = NULL;
    struct k_sigaction *k_oldact_ptr = NULL;

    /* Validate signal number */
    if (signum < 1 || signum >= _NSIG) {
        errno = EINVAL;
        return -1;
    }

    /* Blocked signals cannot have their action changed */
    if (signum == 9 || signum == 19) { /* SIGKILL, SIGSTOP */
        errno = EINVAL;
        return -1;
    }

    /* Convert input act to internal k_sigaction */
    if (act) {
        sigaction_to_k(act, &k_act);
        k_act_ptr = &k_act;
    }

    /* Handle oldact */
    if (oldact) {
        k_oldact_ptr = &k_oldact;
    }

    /* Call internal signal owner */
    int result = do_sigaction(signum, k_act_ptr, k_oldact_ptr);

    /* Convert oldact back to Linux format if requested */
    if (oldact && result == 0) {
        k_to_sigaction(&k_oldact, oldact);
    }

    return result;
}

static sighandler_t signal_impl(int signum, sighandler_t handler) {
    /* Validate signal number */
    if (signum < 1 || signum >= _NSIG) {
        errno = EINVAL;
        return SIG_ERR;
    }

    /* Blocked signals cannot have their handler changed */
    if (signum == 9 || signum == 19) { /* SIGKILL, SIGSTOP */
        errno = EINVAL;
        return SIG_ERR;
    }

    /* Call internal do_signal which manages handlers directly */
    ix_sighandler_t old_ix_handler = do_signal(signum, handler);
    return (sighandler_t)old_ix_handler;
}

static int sigprocmask_impl(int how, const sigset_t *set, sigset_t *oldset) {
    ix_sigset_t ix_set, ix_oldset;
    ix_sigset_t *ix_set_ptr = NULL;
    ix_sigset_t *ix_oldset_ptr = NULL;

    /* Convert input set to internal representation */
    if (set) {
        sigset_to_ix(set, &ix_set);
        ix_set_ptr = &ix_set;
    }

    /* Handle oldset */
    if (oldset) {
        ix_oldset_ptr = &ix_oldset;
    }

    /* Call internal signal owner */
    int result = do_sigprocmask(how, ix_set_ptr, ix_oldset_ptr);

    /* Convert oldset back to Linux format if requested */
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

/* ============================================================================
 * SIGNAL
 * ============================================================================ */

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

/* ============================================================================
 * INITIALIZATION
 * ============================================================================ */

int ixland_init(const ixland_config_t *config) {
    return ixland_init_internal(config);
}

void ixland_cleanup(void) {
    ixland_cleanup_internal();
}

const char *ixland_version(void) {
    return ixland_version_internal();
}

int ixland_is_initialized(void) {
    return ixland_is_initialized_internal();
}
