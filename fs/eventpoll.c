#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/event.h>
#include <unistd.h>

#include "epoll_internal.h"

/* Forward declarations for epoll functions */
int ixland_epoll_create1(int flags);

typedef sigset_t ixland_native_sigset_t;

#include "../internal/ixland_internal.h"

/* Forward declarations for epoll wait functions */
int ixland_epoll_pwait(int epfd, struct linux_epoll_event *events, int maxevents, int timeout,
                       const void *sigmask);

typedef struct ixland_epitem {
    int fd;
    struct linux_epoll_event event;
    uint32_t registered_events;
    bool is_registered;
    bool edge_triggered;
    struct ixland_epitem *next;
    struct ixland_epitem *prev;
} ixland_epitem_t;

typedef struct ixland_epoll_instance {
    int kq;
    uint32_t flags;
    int size;
    atomic_int ref_count;
    pthread_mutex_t lock;
    ixland_epitem_t **items;
    int hash_size;
    int item_count;
    uint64_t total_events;
} ixland_epoll_instance_t;

static inline int ixland_epoll_hash(ixland_epoll_instance_t *ep, int fd) {
    return fd & (ep->hash_size - 1);
}

static ixland_epitem_t *ixland_epoll_find_item(ixland_epoll_instance_t *ep, int fd) {
    int idx = ixland_epoll_hash(ep, fd);
    ixland_epitem_t *item = ep->items[idx];

    while (item) {
        if (item->fd == fd) {
            return item;
        }
        item = item->next;
    }
    return NULL;
}

static void ixland_epoll_add_item(ixland_epoll_instance_t *ep, ixland_epitem_t *item) {
    int idx = ixland_epoll_hash(ep, item->fd);
    item->next = ep->items[idx];
    item->prev = NULL;
    if (ep->items[idx]) {
        ep->items[idx]->prev = item;
    }
    ep->items[idx] = item;
    ep->item_count++;
}

static void ixland_epoll_remove_item(ixland_epoll_instance_t *ep, ixland_epitem_t *item) {
    if (item->prev) {
        item->prev->next = item->next;
    } else {
        int idx = ixland_epoll_hash(ep, item->fd);
        ep->items[idx] = item->next;
    }
    if (item->next) {
        item->next->prev = item->prev;
    }
    ep->item_count--;
}

static uint32_t ixland_epoll_to_kqueue_flags(uint32_t epoll_events) {
    uint32_t kflags = 0;

    if (epoll_events & EPOLLET) {
        kflags |= EV_CLEAR;
    }
    if (epoll_events & EPOLLONESHOT) {
        kflags |= EV_ONESHOT;
    }

    return kflags;
}

static int ixland_epoll_build_kevents(struct kevent *changes, int max_changes, int fd,
                                      uint32_t epoll_events, void *udata, bool add_mode) {
    int n = 0;
    uint32_t kflags = ixland_epoll_to_kqueue_flags(epoll_events);

    if (epoll_events & (EPOLLIN | EPOLLRDNORM | EPOLLRDBAND | EPOLLPRI)) {
        if (n < max_changes) {
            EV_SET(&changes[n], fd, EVFILT_READ, add_mode ? EV_ADD : EV_DELETE, 0, 0, udata);
            if (add_mode) {
                changes[n].flags |= kflags;
            }
            n++;
        }
    }

    if (epoll_events & (EPOLLOUT | EPOLLWRNORM | EPOLLWRBAND)) {
        if (n < max_changes) {
            EV_SET(&changes[n], fd, EVFILT_WRITE, add_mode ? EV_ADD : EV_DELETE, 0, 0, udata);
            if (add_mode) {
                changes[n].flags |= kflags;
            }
            n++;
        }
    }

    return n;
}

static uint32_t ixland_kqueue_to_epoll_events(int16_t filter, uint16_t flags, int data) {
    (void)data;
    uint32_t events = 0;

    if (filter == EVFILT_READ) {
        events |= EPOLLIN;
        if (flags & EV_OOBAND) {
            events |= EPOLLPRI;
        }
    } else if (filter == EVFILT_WRITE) {
        events |= EPOLLOUT;
    }

    if (flags & EV_ERROR) {
        events |= EPOLLERR;
    }
    if (flags & EV_EOF) {
        events |= EPOLLHUP;
        events |= EPOLLRDHUP;
    }

    return events;
}

#define IXLAND_MAX_EPOLL_INSTANCES 256
static ixland_epoll_instance_t *ixland_epoll_table[IXLAND_MAX_EPOLL_INSTANCES];
static pthread_mutex_t ixland_epoll_table_lock = PTHREAD_MUTEX_INITIALIZER;

static int ixland_epoll_register_instance(ixland_epoll_instance_t *ep) {
    pthread_mutex_lock(&ixland_epoll_table_lock);
    for (int i = 0; i < IXLAND_MAX_EPOLL_INSTANCES; i++) {
        if (ixland_epoll_table[i] == NULL) {
            ixland_epoll_table[i] = ep;
            pthread_mutex_unlock(&ixland_epoll_table_lock);
            return i + IXLAND_MAX_FD + 100;
        }
    }
    pthread_mutex_unlock(&ixland_epoll_table_lock);
    return -1;
}

static ixland_epoll_instance_t *ixland_epoll_lookup_instance(int epfd) {
    int idx = epfd - IXLAND_MAX_FD - 100;
    if (idx < 0 || idx >= IXLAND_MAX_EPOLL_INSTANCES) {
        return NULL;
    }
    return ixland_epoll_table[idx];
}

int ixland_epoll_create(int size) {
    if (size <= 0) {
        errno = EINVAL;
        return -1;
    }

    return ixland_epoll_create1(0);
}

int ixland_epoll_create1(int flags) {
    if (flags & ~EPOLL_CLOEXEC) {
        errno = EINVAL;
        return -1;
    }

    ixland_epoll_instance_t *ep = calloc(1, sizeof(ixland_epoll_instance_t));
    if (!ep) {
        errno = ENOMEM;
        return -1;
    }

    ep->kq = kqueue();
    if (ep->kq < 0) {
        free(ep);
        errno = EMFILE;
        return -1;
    }

    if (flags & EPOLL_CLOEXEC) {
        int fd_flags = fcntl(ep->kq, F_GETFD);
        if (fd_flags >= 0) {
            fcntl(ep->kq, F_SETFD, fd_flags | FD_CLOEXEC);
        }
    }

    ep->hash_size = 16;
    ep->items = calloc(ep->hash_size, sizeof(ixland_epitem_t *));
    if (!ep->items) {
        close(ep->kq);
        free(ep);
        errno = ENOMEM;
        return -1;
    }

    ep->flags = (uint32_t)flags;
    ep->item_count = 0;
    ep->total_events = 0;
    atomic_init(&ep->ref_count, 1);
    pthread_mutex_init(&ep->lock, NULL);

    int epfd = ixland_epoll_register_instance(ep);
    if (epfd < 0) {
        free(ep->items);
        close(ep->kq);
        free(ep);
        errno = EMFILE;
        return -1;
    }

    return epfd;
}

int ixland_epoll_ctl(int epfd, int op, int fd, struct linux_epoll_event *event) {
    ixland_epoll_instance_t *ep = ixland_epoll_lookup_instance(epfd);
    if (!ep) {
        errno = EBADF;
        return -1;
    }

    if (epfd == fd) {
        errno = EINVAL;
        return -1;
    }

    if (fd < 0 || fcntl(fd, F_GETFL) < 0) {
        errno = EBADF;
        return -1;
    }

    pthread_mutex_lock(&ep->lock);

    ixland_epitem_t *item = ixland_epoll_find_item(ep, fd);

    switch (op) {
    case EPOLL_CTL_ADD: {
        if (!event) {
            pthread_mutex_unlock(&ep->lock);
            errno = EINVAL;
            return -1;
        }

        if (item != NULL) {
            pthread_mutex_unlock(&ep->lock);
            errno = EEXIST;
            return -1;
        }

        item = calloc(1, sizeof(ixland_epitem_t));
        if (!item) {
            pthread_mutex_unlock(&ep->lock);
            errno = ENOMEM;
            return -1;
        }

        item->fd = fd;
        memcpy(&item->event, event, sizeof(struct linux_epoll_event));
        item->is_registered = true;
        item->edge_triggered = (event->events & EPOLLET) != 0;

        struct kevent changes[2];
        int nchanges =
            ixland_epoll_build_kevents(changes, 2, fd, event->events & ~EPOLLONESHOT, item, true);

        if (nchanges > 0) {
            int ret = kevent(ep->kq, changes, nchanges, NULL, 0, NULL);
            if (ret < 0) {
                free(item);
                pthread_mutex_unlock(&ep->lock);
                errno = EPERM;
                return -1;
            }
        }

        ixland_epoll_add_item(ep, item);
        break;
    }

    case EPOLL_CTL_DEL: {
        if (item == NULL) {
            pthread_mutex_unlock(&ep->lock);
            errno = ENOENT;
            return -1;
        }

        struct kevent changes[2];
        int nchanges =
            ixland_epoll_build_kevents(changes, 2, fd, item->registered_events, item, false);
        if (nchanges > 0) {
            kevent(ep->kq, changes, nchanges, NULL, 0, NULL);
        }

        ixland_epoll_remove_item(ep, item);
        free(item);
        break;
    }

    case EPOLL_CTL_MOD: {
        if (!event) {
            pthread_mutex_unlock(&ep->lock);
            errno = EINVAL;
            return -1;
        }

        if (item == NULL) {
            pthread_mutex_unlock(&ep->lock);
            errno = ENOENT;
            return -1;
        }

        struct kevent changes[2];
        int nchanges =
            ixland_epoll_build_kevents(changes, 2, fd, item->registered_events, item, false);
        if (nchanges > 0) {
            kevent(ep->kq, changes, nchanges, NULL, 0, NULL);
        }

        memcpy(&item->event, event, sizeof(struct linux_epoll_event));
        item->edge_triggered = (event->events & EPOLLET) != 0;

        nchanges =
            ixland_epoll_build_kevents(changes, 2, fd, event->events & ~EPOLLONESHOT, item, true);
        if (nchanges > 0) {
            int ret = kevent(ep->kq, changes, nchanges, NULL, 0, NULL);
            if (ret < 0) {
                pthread_mutex_unlock(&ep->lock);
                errno = EPERM;
                return -1;
            }
        }
        break;
    }

    default:
        pthread_mutex_unlock(&ep->lock);
        errno = EINVAL;
        return -1;
    }

    pthread_mutex_unlock(&ep->lock);
    return 0;
}

int ixland_epoll_wait(int epfd, struct linux_epoll_event *events, int maxevents, int timeout) {
    return ixland_epoll_pwait(epfd, events, maxevents, timeout, NULL);
}

int ixland_epoll_pwait(int epfd, struct linux_epoll_event *events, int maxevents, int timeout,
                       const struct ixland_sigset *sigmask) {
    if (!events || maxevents <= 0) {
        errno = EINVAL;
        return -1;
    }

    ixland_epoll_instance_t *ep = ixland_epoll_lookup_instance(epfd);
    if (!ep) {
        errno = EBADF;
        return -1;
    }

    ixland_native_sigset_t oldmask;
    ixland_native_sigset_t newmask;
    if (sigmask) {
        memset(&newmask, 0, sizeof(newmask));
        memcpy(&newmask, sigmask, sizeof(newmask) < 128 ? sizeof(newmask) : 128);
        pthread_sigmask(SIG_SETMASK, &newmask, &oldmask);
    }

    struct timespec ts;
    struct timespec *tsp = NULL;
    if (timeout >= 0) {
        ts.tv_sec = timeout / 1000;
        ts.tv_nsec = (timeout % 1000) * 1000000;
        tsp = &ts;
    }

    struct kevent *kevents = calloc(maxevents, sizeof(struct kevent));
    if (!kevents) {
        if (sigmask) {
            pthread_sigmask(SIG_SETMASK, &oldmask, NULL);
        }
        errno = ENOMEM;
        return -1;
    }

    int nevents = kevent(ep->kq, NULL, 0, kevents, maxevents, tsp);

    if (nevents < 0) {
        int saved_errno = errno;
        free(kevents);
        if (sigmask) {
            pthread_sigmask(SIG_SETMASK, &oldmask, NULL);
        }
        errno = (saved_errno == EINTR) ? EINTR : EBADF;
        return -1;
    }

    int ready_count = 0;
    for (int i = 0; i < nevents && ready_count < maxevents; i++) {
        ixland_epitem_t *item = (ixland_epitem_t *)kevents[i].udata;
        if (!item)
            continue;

        events[ready_count].events = ixland_kqueue_to_epoll_events(
            kevents[i].filter, kevents[i].flags, (int)kevents[i].data);

        memcpy(&events[ready_count].data, &item->event.data, sizeof(epoll_data_t));

        if (item->event.events & EPOLLONESHOT) {
            pthread_mutex_lock(&ep->lock);
            struct kevent changes[2];
            int nchanges = ixland_epoll_build_kevents(changes, 2, item->fd, item->registered_events,
                                                      item, false);
            if (nchanges > 0) {
                kevent(ep->kq, changes, nchanges, NULL, 0, NULL);
            }
            item->is_registered = false;
            pthread_mutex_unlock(&ep->lock);
        }

        ready_count++;
    }

    free(kevents);

    if (sigmask) {
        pthread_sigmask(SIG_SETMASK, &oldmask, NULL);
    }

    ep->total_events += ready_count;
    return ready_count;
}

int ixland_epoll_pwait2(int epfd, struct linux_epoll_event *events, int maxevents,
                        const struct ixland_timespec *timeout,
                        const struct ixland_sigset *sigmask) {
    int timeout_ms = -1;
    if (timeout) {
        timeout_ms = (int)(timeout->tv_sec * 1000 + timeout->tv_nsec / 1000000);
    }

    return ixland_epoll_pwait(epfd, events, maxevents, timeout_ms,
                              (const struct ixland_sigset *)sigmask);
}
