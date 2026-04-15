#ifndef READINESS_H
#define READINESS_H

/*
 * IXLandSystem internal readiness layer
 *
 * Linux-shaped internal interface using private types.
 * NO Darwin headers included here - Darwin bridge is separate.
 */

#include <stddef.h>
#include <stdint.h>
#include <signal.h>
#include <sys/poll.h>
#include <sys/time.h>

/* Internal fd_set representation */
#define NR_READINESS_FDS 1024

typedef struct {
    uint64_t bits[NR_READINESS_FDS / 64];
} readiness_fd_set_t;

static inline void readiness_fd_zero(readiness_fd_set_t *set) {
    if (set) {
        for (size_t i = 0; i < NR_READINESS_FDS / 64; i++) {
            set->bits[i] = 0;
        }
    }
}

static inline void readiness_fd_set(int fd, readiness_fd_set_t *set) {
    if (set && fd >= 0 && fd < NR_READINESS_FDS) {
        set->bits[fd / 64] |= (1ULL << (fd % 64));
    }
}

static inline void readiness_fd_clr(int fd, readiness_fd_set_t *set) {
    if (set && fd >= 0 && fd < NR_READINESS_FDS) {
        set->bits[fd / 64] &= ~(1ULL << (fd % 64));
    }
}

static inline int readiness_fd_isset(int fd, const readiness_fd_set_t *set) {
    if (set && fd >= 0 && fd < NR_READINESS_FDS) {
        return (set->bits[fd / 64] & (1ULL << (fd % 64))) != 0;
    }
    return 0;
}

/* Poll using internal fd_set representation */
int do_poll(struct pollfd *fds, unsigned int nfds, int timeout);
int do_ppoll(struct pollfd *fds, unsigned int nfds, const struct timespec *timeout,
             const sigset_t *sigmask);
int do_select(int nfds, readiness_fd_set_t *readfds, readiness_fd_set_t *writefds,
              readiness_fd_set_t *exceptfds, struct timeval *timeout);
int do_pselect(int nfds, readiness_fd_set_t *readfds, readiness_fd_set_t *writefds,
               readiness_fd_set_t *exceptfds, const struct timespec *timeout,
               const sigset_t *sigmask);

#endif /* READINESS_H */
