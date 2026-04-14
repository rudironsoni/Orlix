#ifndef SELECT_H
#define SELECT_H

/* Prevent Darwin select/pselect from conflicting */
#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <signal.h>
#include <stdint.h>
#include <string.h>

/* Poll event constants */
#define POLLIN 0x001
#define POLLPRI 0x002
#define POLLOUT 0x004
#define POLLERR 0x008
#define POLLHUP 0x010
#define POLLNVAL 0x020
#define POLLRDNORM 0x040
#define POLLRDBAND 0x080
#define POLLWRNORM 0x100
#define POLLWRBAND 0x200

/* Linux-compatible pollfd structure */
struct pollfd {
    int fd;
    short events;
    short revents;
};

/* Linux-compatible timespec structure for pselect/ppoll */
struct linux_timespec {
    int64_t tv_sec;
    long tv_nsec;
};

/* Linux-compatible timeval structure for select */
struct linux_timeval {
    int64_t tv_sec;
    int64_t tv_usec;
};

/* Maximum FD set size for select */
#define FD_SETSIZE 1024
#define NFDBITS (8 * sizeof(unsigned long))

/* Linux-compatible nfds_t for poll */
typedef unsigned long nfds_t;

/* Linux-compatible fd_set type */
typedef struct {
    unsigned long fds_bits[(FD_SETSIZE + NFDBITS - 1) / NFDBITS];
} fd_set_t;

/* FD_SET macros */
#define FD_ZERO(set) memset((set), 0, sizeof(*(set)))
#define FD_SET(fd_arg, set_arg) \
    ((set_arg)->fds_bits[(fd_arg) / NFDBITS] |= (1UL << ((fd_arg) % NFDBITS)))
#define FD_CLR(fd_arg, set_arg) \
    ((set_arg)->fds_bits[(fd_arg) / NFDBITS] &= ~(1UL << ((fd_arg) % NFDBITS)))
#define FD_ISSET(fd_arg, set_arg) \
    (((set_arg)->fds_bits[(fd_arg) / NFDBITS] & (1UL << ((fd_arg) % NFDBITS))) != 0)

/* select/poll API - Linux-style internal names
 * 
 * NOTE: Public wrappers select(), pselect(), poll(), ppoll() are
 * BLOCKED BY HEADER DRIFT - see select.c for details.
 * The do_* helpers are fully implemented but public ABI surface
 * requires careful Darwin header isolation that's not yet complete.
 */
int do_select(int nfds, fd_set_t *readfds, fd_set_t *writefds, fd_set_t *exceptfds,
              struct linux_timeval *timeout);
int do_pselect(int nfds, fd_set_t *readfds, fd_set_t *writefds, fd_set_t *exceptfds,
               const struct linux_timespec *timeout, const sigset_t *sigmask);
int do_poll(struct pollfd *fds, unsigned int nfds, int timeout);
int do_ppoll(struct pollfd *fds, unsigned int nfds, const struct linux_timespec *timeout,
              const sigset_t *sigmask);

#endif /* SELECT_H */
