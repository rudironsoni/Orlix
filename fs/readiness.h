#ifndef READINESS_H
#define READINESS_H

/*
 * IXLandSystem internal readiness layer
 *
 * Linux-shaped internal interface using private types.
 * NO Darwin headers included here.
 */

#include <stddef.h>
#include <stdint.h>

/* Internal pollfd structure - Linux-shaped */
struct ixland_pollfd {
    int fd;
    short events;
    short revents;
};

/* Poll event flags - Linux definitions */
#define POLLIN      0x001
#define POLLPRI     0x002
#define POLLOUT     0x004
#define POLLERR     0x008
#define POLLHUP     0x010
#define POLLNVAL    0x020

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

/* Internal timespec - Linux-shaped */
struct ixland_timespec {
    long tv_sec;
    long tv_nsec;
};

/* Internal timeval - Linux-shaped */
struct ixland_timeval {
    long tv_sec;
    long tv_usec;
};

/* Internal sigset - Linux-shaped, avoid Darwin sigset_t */
#define NR_IXLAND_SIGNALS 64
typedef struct {
    uint64_t bits[NR_IXLAND_SIGNALS / 64];
} ixland_sigset_t;

/* Poll using internal fd_set representation */
int do_poll(struct ixland_pollfd *fds, unsigned int nfds, int timeout);
int do_ppoll(struct ixland_pollfd *fds, unsigned int nfds, const struct ixland_timespec *timeout,
             const ixland_sigset_t *sigmask);
int do_select(int nfds, readiness_fd_set_t *readfds, readiness_fd_set_t *writefds,
              readiness_fd_set_t *exceptfds, struct ixland_timeval *timeout);
int do_pselect(int nfds, readiness_fd_set_t *readfds, readiness_fd_set_t *writefds,
               readiness_fd_set_t *exceptfds, const struct ixland_timespec *timeout,
               const ixland_sigset_t *sigmask);

#endif /* READINESS_H */
