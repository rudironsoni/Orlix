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
