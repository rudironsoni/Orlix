/* IXLand Linux epoll Compatibility Header
 *
 * Linux epoll structures and constants for Darwin (iOS/macOS)
 * Used internally by IXLandSystem eventpoll implementation
 */

#ifndef IXLAND_EPOLL_INTERNAL_H
#define IXLAND_EPOLL_INTERNAL_H

#include <stdint.h>

/* Linux epoll event types */
#define EPOLLIN (1 << 0)
#define EPOLLPRI (1 << 1)
#define EPOLLOUT (1 << 2)
#define EPOLLERR (1 << 3)
#define EPOLLHUP (1 << 4)
#define EPOLLNVAL (1 << 5)

/* Additional Linux event types */
#define EPOLLRDNORM (1 << 6)
#define EPOLLRDBAND (1 << 7)
#define EPOLLWRNORM EPOLLOUT
#define EPOLLWRBAND (1 << 9)
#define EPOLLMSG (1 << 10)
#define EPOLLRDHUP (1 << 13)

/* Linux epoll flag options */
#define EPOLLONESHOT (1 << 30)
#define EPOLLET (1 << 31)

/* Linux epoll_op values */
enum {
    EPOLL_CTL_ADD = 1,
    EPOLL_CTL_DEL = 2,
    EPOLL_CTL_MOD = 3,
};

/* Additional epoll flags */
#define EPOLL_CLOEXEC (1 << 31)

/* Linux epoll_data_t union (Linux-compatible) */
union epoll_data {
    void *ptr;
    int fd;
    uint32_t u32;
    uint64_t u64;
};

/* Linux epoll_event structure (Linux-compatible) */
struct epoll_event {
    uint32_t events;
    union epoll_data data;
};

#endif /* IXLAND_EPOLL_INTERNAL_H */
