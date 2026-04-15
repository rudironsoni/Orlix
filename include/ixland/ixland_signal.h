/* IXLand Signal Public Interface
 * Canonical Linux/POSIX signal operations
 */

#ifndef IXLAND_SIGNAL_H
#define IXLAND_SIGNAL_H

#include <signal.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * SIGNAL HANDLER MANAGEMENT
 * ============================================================================ */

/* Signal handler type - canonical POSIX */
typedef void (*sighandler_t)(int);

/* Install signal handler - POSIX signal() */
sighandler_t signal(int signum, sighandler_t handler);

/* Examine and change signal action - POSIX sigaction() */
int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);

/* ============================================================================
 * SIGNAL SIGNALING
 * ============================================================================ */

/* Send signal to process - POSIX kill() */
int kill(pid_t pid, int sig);

/* Send signal to process group - POSIX killpg() */
int killpg(pid_t pgrp, int sig);

/* Send signal to self - POSIX raise() */
int raise(int sig);

/* ============================================================================
 * SIGNAL MASKING
 * ============================================================================ */

/* Examine and change blocked signals - POSIX sigprocmask() */
int sigprocmask(int how, const sigset_t *set, sigset_t *oldset);

/* Get set of pending signals - POSIX sigpending() */
int sigpending(sigset_t *set);

/* Wait for signal with mask replacement - POSIX sigsuspend() */
int sigsuspend(const sigset_t *mask);

/* ============================================================================
 * SIGNAL WAITING
 * ============================================================================ */

/* Wait for signal - POSIX pause() */
int pause(void);

#ifdef __cplusplus
}
#endif

#endif /* IXLAND_SIGNAL_H */
