/* IXLand Signal Subsystem Header
 * Canonical signal mask representation and operations
 */

#ifndef IXLAND_SIGNAL_H
#define IXLAND_SIGNAL_H

#include <signal.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* IXLand-owned signal mask representation
 * Matches Linux kernel sigset_t semantics
 * Independent of host platform sigset_t layout
 */
#define IXLAND_NSIG 64
#define IXLAND_NSIG_WORDS (IXLAND_NSIG / (sizeof(unsigned long) * 8))

typedef struct {
    unsigned long sig[IXLAND_NSIG_WORDS];
} ixland_sigset_t;

/* Signal mask operations */
void ixland_sigemptyset(ixland_sigset_t *set);
void ixland_sigfillset(ixland_sigset_t *set);
int ixland_sigaddset(ixland_sigset_t *set, int sig);
int ixland_sigdelset(ixland_sigset_t *set, int sig);
int ixland_sigismember(const ixland_sigset_t *set, int sig);

/* Signal mask operations */
void ixland_sigandset(ixland_sigset_t *dest, const ixland_sigset_t *left,
                      const ixland_sigset_t *right);
void ixland_sigorset(ixland_sigset_t *dest, const ixland_sigset_t *left,
                     const ixland_sigset_t *right);
void ixland_signotset(ixland_sigset_t *dest, const ixland_sigset_t *src);

/* Darwin boundary conversion - only at host mediation edge */
void ixland_sigset_to_host(const ixland_sigset_t *ixland_set, sigset_t *host_set);
void ixland_sigset_from_host(const sigset_t *host_set, ixland_sigset_t *ixland_set);

/* Thread-local signal mask storage */
ixland_sigset_t *ixland_thread_sigmask(void);

#ifdef __cplusplus
}
#endif

#endif /* IXLAND_SIGNAL_H */
