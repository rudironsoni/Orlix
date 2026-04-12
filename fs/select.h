#ifndef SELECT_H
#define SELECT_H

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

/* Linux-compatible fd_set type */
typedef struct {
    unsigned long fds_bits[(FD_SETSIZE + NFDBITS - 1) / NFDBITS];
} fd_set_t;

/* FD_SET macros */
#define FD_ZERO(set) memset((set), 0, sizeof(*(set)))
#define FD_SET(fd, set) ((set)->fds_bits[(fd) / NFDBITS] |= (1UL << ((fd) % NFDBITS)))
#define FD_CLR(fd, set) ((set)->fds_bits[(fd) / NFDBITS] &= ~(1UL << ((fd) % NFDBITS)))
#define FD_ISSET(fd, set) (((set)->fds_bits[(fd) / NFDBITS] & (1UL << ((fd) % NFDBITS))) != 0)

/* select/poll API */
int select(int nfds, fd_set_t *readfds, fd_set_t *writefds, fd_set_t *exceptfds,
           struct linux_timeval *timeout);
int pselect(int nfds, fd_set_t *readfds, fd_set_t *writefds, fd_set_t *exceptfds,
            const struct linux_timespec *timeout, const sigset_t *sigmask);
int poll(struct pollfd *fds, unsigned int nfds, int timeout);
int ppoll(struct pollfd *fds, unsigned int nfds, const struct linux_timespec *timeout,
          const sigset_t *sigmask);

#endif /* SELECT_H */
