/* IXLandSystem/arch/darwin/signal_bridge.c
 * Darwin host bridge for signal syscalls
 *
 * This file is ONLY for host-bridge purposes. It includes Darwin
 * headers and converts between Darwin types and IXLand internal types.
 */

#include <signal.h>
#include <string.h>
#include <errno.h>

#include "../../kernel/signal.h"
#include "../../kernel/task.h"

/* Darwin signal numbers (for reference) */
#define DARWIN_SIGHUP    1
#define DARWIN_SIGINT    2
#define DARWIN_SIGQUIT   3
#define DARWIN_SIGILL    4
#define DARWIN_SIGTRAP   5
#define DARWIN_SIGABRT   6
#define DARWIN_SIGIOT    DARWIN_SIGABRT
#define DARWIN_SIGEMT    7
#define DARWIN_SIGFPE    8
#define DARWIN_SIGKILL   9
#define DARWIN_SIGBUS    10
#define DARWIN_SIGSEGV   11
#define DARWIN_SIGSYS    12
#define DARWIN_SIGPIPE   13
#define DARWIN_SIGALRM   14
#define DARWIN_SIGTERM   15
#define DARWIN_SIGURG    16
#define DARWIN_SIGSTOP   17
#define DARWIN_SIGTSTP   18
#define DARWIN_SIGCONT   19
#define DARWIN_SIGCHLD   20
#define DARWIN_SIGTTIN   21
#define DARWIN_SIGTTOU   22
#define DARWIN_SIGIO     23
#define DARWIN_SIGXCPU   24
#define DARWIN_SIGXFSZ   25
#define DARWIN_SIGVTALRM 26
#define DARWIN_SIGPROF   27
#define DARWIN_SIGWINCH  28
#define DARWIN_SIGINFO   29
#define DARWIN_SIGUSR1   30
#define DARWIN_SIGUSR2   31

#ifndef SIG_ERR
#define SIG_ERR ((sighandler_t)-1)
#endif

/* Convert Darwin sigset_t to internal signal_mask_bits */
static void sigset_to_internal(const sigset_t *darwin_set, struct signal_mask_bits *internal_set) {
    memset(internal_set, 0, sizeof(*internal_set));
    if (!darwin_set) return;

    /* Darwin sigset_t is typically an array of 32-bit words */
    /* IXLand internal uses 64-bit words */
    for (int sig = 1; sig < SIGNAL_NSIG && sig <= 32; sig++) {
        if (sigismember(darwin_set, sig)) {
            int idx = sig / 64;
            int bit = sig % 64;
            if (idx < SIGNAL_NSIG_WORDS) {
                internal_set->sig[idx] |= (1ULL << bit);
            }
        }
    }
}

/* Convert internal signal_mask_bits to Darwin sigset_t */
static void internal_to_sigset(const struct signal_mask_bits *internal_set, sigset_t *darwin_set) {
    if (!darwin_set) return;
    sigemptyset(darwin_set);
    if (!internal_set) return;

    for (int sig = 1; sig < SIGNAL_NSIG && sig <= 32; sig++) {
        int idx = sig / 64;
        int bit = sig % 64;
        if (idx < SIGNAL_NSIG_WORDS) {
            if (internal_set->sig[idx] & (1ULL << bit)) {
                sigaddset(darwin_set, sig);
            }
        }
    }
}

/* Convert Darwin struct sigaction to internal signal_action_slot */
static void sigaction_to_internal(const struct sigaction *darwin_act, struct signal_action_slot *internal_act) {
    memset(internal_act, 0, sizeof(*internal_act));
    if (!darwin_act) return;

    internal_act->handler = darwin_act->sa_handler;
    sigset_to_internal(&darwin_act->sa_mask, &internal_act->mask);
    internal_act->flags = darwin_act->sa_flags;
}

/* Convert internal signal_action_slot to Darwin struct sigaction */
static void internal_to_sigaction(const struct signal_action_slot *internal_act, struct sigaction *darwin_act) {
    memset(darwin_act, 0, sizeof(*darwin_act));
    if (!internal_act) return;

    darwin_act->sa_handler = internal_act->handler;
    internal_to_sigset(&internal_act->mask, &darwin_act->sa_mask);
    darwin_act->sa_flags = internal_act->flags;
}

__attribute__((visibility("default"))) int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact) {
    struct signal_action_slot internal_act, internal_oldact;
    struct signal_action_slot *internal_act_ptr = NULL;
    struct signal_action_slot *internal_oldact_ptr = NULL;

    if (act) {
        sigaction_to_internal(act, &internal_act);
        internal_act_ptr = &internal_act;
    }

    if (oldact) {
        internal_oldact_ptr = &internal_oldact;
    }

    int result = do_sigaction(signum, internal_act_ptr, internal_oldact_ptr);

    if (oldact && result == 0) {
        internal_to_sigaction(&internal_oldact, oldact);
    }

    return result;
}

__attribute__((visibility("default"))) sighandler_t signal(int signum, sighandler_t handler) {
    if (signum < 1 || signum >= SIGNAL_NSIG) {
        errno = EINVAL;
        return SIG_ERR;
    }

    if (signum == 9 || signum == 19) {
        errno = EINVAL;
        return SIG_ERR;
    }

    sighandler_t old_handler = do_signal(signum, handler);
    return old_handler ? old_handler : SIG_ERR;
}

__attribute__((visibility("default"))) int kill(int32_t pid, int sig) {
    return do_kill(pid, sig);
}

__attribute__((visibility("default"))) int killpg(int32_t pgrp, int sig) {
    return do_killpg(pgrp, sig);
}

__attribute__((visibility("default"))) int sigprocmask(int how, const sigset_t *set, sigset_t *oldset) {
    struct signal_mask_bits internal_set, internal_oldset;
    struct signal_mask_bits *internal_set_ptr = NULL;
    struct signal_mask_bits *internal_oldset_ptr = NULL;

    if (set) {
        sigset_to_internal(set, &internal_set);
        internal_set_ptr = &internal_set;
    }

    if (oldset) {
        internal_oldset_ptr = &internal_oldset;
    }

    int result = do_sigprocmask(how, internal_set_ptr, internal_oldset_ptr);

    if (oldset && result == 0) {
        internal_to_sigset(&internal_oldset, oldset);
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
        internal_to_sigset(&internal_set, set);
    }

    return result;
}

__attribute__((visibility("default"))) int sigsuspend(const sigset_t *mask) {
    if (!mask) {
        errno = EFAULT;
        return -1;
    }

    struct signal_mask_bits internal_mask;
    sigset_to_internal(mask, &internal_mask);

    return do_sigsuspend(&internal_mask);
}

__attribute__((visibility("default"))) int raise(int sig) {
    return do_raise(sig);
}

__attribute__((visibility("default"))) int pause(void) {
    return do_pause();
}
