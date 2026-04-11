#ifndef IXLAND_SELECT_INTERNAL_H
#define IXLAND_SELECT_INTERNAL_H

#include <signal.h>
#include <stdint.h>
#include <string.h>

#define IXLAND_POLLIN 0x001
#define IXLAND_POLLPRI 0x002
#define IXLAND_POLLOUT 0x004
#define IXLAND_POLLERR 0x008
#define IXLAND_POLLHUP 0x010
#define IXLAND_POLLNVAL 0x020
#define IXLAND_POLLRDNORM 0x040
#define IXLAND_POLLRDBAND 0x080
#define IXLAND_POLLWRNORM 0x100
#define IXLAND_POLLWRBAND 0x200

struct linux_pollfd {
    int fd;
    short events;
    short revents;
};

struct linux_timespec {
    int64_t tv_sec;
    long tv_nsec;
};

struct linux_timeval {
    int64_t tv_sec;
    int64_t tv_usec;
};

#define IXLAND_FD_SETSIZE 1024
#define IXLAND_NFDBITS (8 * sizeof(unsigned long))

typedef struct {
    unsigned long fds_bits[(IXLAND_FD_SETSIZE + IXLAND_NFDBITS - 1) / IXLAND_NFDBITS];
} linux_fd_set_t;

#define IXLAND_FD_ZERO(set) memset((set), 0, sizeof(*(set)))
#define IXLAND_FD_SET(fd, set) \
    ((set)->fds_bits[(fd) / IXLAND_NFDBITS] |= (1UL << ((fd) % IXLAND_NFDBITS)))
#define IXLAND_FD_ISSET(fd, set) \
    (((set)->fds_bits[(fd) / IXLAND_NFDBITS] & (1UL << ((fd) % IXLAND_NFDBITS))) != 0)

typedef sigset_t linux_sigset_t;

#endif