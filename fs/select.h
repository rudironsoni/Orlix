#ifndef SELECT_H
#define SELECT_H

#include <signal.h>
#include <stdint.h>
#include <stdbool.h>
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

/* Internal fd set size and ops - private names to avoid Darwin collisions */
#define IX_FD_SETSIZE 1024
#define IX_NFDBITS (8 * sizeof(unsigned long))

/* Linux-compatible nfds_t for poll */
typedef unsigned long nfds_t;

/* Linux-compatible fd_set type - private name to avoid Darwin collision */
typedef struct {
    unsigned long fds_bits[(IX_FD_SETSIZE + IX_NFDBITS - 1) / IX_NFDBITS];
} ix_fd_set_t;

/* FD_SET macros - private names to avoid Darwin collisions */
#define IX_FD_ZERO(set) memset((set), 0, sizeof(*(set)))
#define IX_FD_SET(fd_arg, set_arg) \
    ((set_arg)->fds_bits[(fd_arg) / IX_NFDBITS] |= (1UL << ((fd_arg) % IX_NFDBITS)))
#define IX_FD_CLR(fd_arg, set_arg) \
    ((set_arg)->fds_bits[(fd_arg) / IX_NFDBITS] &= ~(1UL << ((fd_arg) % IX_NFDBITS)))
#define IX_FD_ISSET(fd_arg, set_arg) \
    (((set_arg)->fds_bits[(fd_arg) / IX_NFDBITS] & (1UL << ((fd_arg) % IX_NFDBITS))) != 0)

/* select/poll API - Linux-style internal names */
int select_impl(int nfds, ix_fd_set_t *readfds, ix_fd_set_t *writefds,
                ix_fd_set_t *exceptfds, struct linux_timeval *timeout);
int pselect_impl(int nfds, ix_fd_set_t *readfds, ix_fd_set_t *writefds,
                 ix_fd_set_t *exceptfds, const struct linux_timespec *timeout,
                 const sigset_t *sigmask);
int poll_impl(struct pollfd *fds, unsigned int nfds, int timeout);
int ppoll_impl(struct pollfd *fds, unsigned int nfds, const struct linux_timespec *timeout,
               const sigset_t *sigmask);

#endif /* SELECT_H */