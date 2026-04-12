#ifndef EVENTPOLL_H
#define EVENTPOLL_H

#include <signal.h>
#include <stdint.h>

/* epoll event types */
#define EPOLLIN (1 << 0)
#define EPOLLPRI (1 << 1)
#define EPOLLOUT (1 << 2)
#define EPOLLERR (1 << 3)
#define EPOLLHUP (1 << 4)
#define EPOLLNVAL (1 << 5)
#define EPOLLRDNORM (1 << 6)
#define EPOLLRDBAND (1 << 7)
#define EPOLLWRNORM EPOLLOUT
#define EPOLLWRBAND (1 << 9)
#define EPOLLMSG (1 << 10)
#define EPOLLRDHUP (1 << 13)

/* epoll flag options */
#define EPOLLONESHOT (1 << 30)
#define EPOLLET (1 << 31)

/* epoll_ctl operations */
enum {
    EPOLL_CTL_ADD = 1,
    EPOLL_CTL_DEL = 2,
    EPOLL_CTL_MOD = 3,
};

/* epoll_create flags */
#define EPOLL_CLOEXEC (1 << 31)

/* epoll_data_t union (Linux-compatible) */
union epoll_data {
    void *ptr;
    int fd;
    uint32_t u32;
    uint64_t u64;
};

/* epoll_event structure (Linux-compatible) */
struct epoll_event {
    uint32_t events;
    union epoll_data data;
};

/* epoll API */
int epoll_create(int size);
int epoll_create1(int flags);
int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);
int epoll_pwait(int epfd, struct epoll_event *events, int maxevents, int timeout,
                const sigset_t *sigmask);

#endif /* EVENTPOLL_H */
