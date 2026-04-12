/* iOS Subsystem for Linux - epoll Implementation
 *
 * Linux epoll API using kqueue as the underlying mechanism.
 */

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/event.h>
#include <time.h>
#include <unistd.h>

#include "epoll_internal.h"

/* Forward declarations */
int epoll_create1(int flags);
int epoll_pwait(int epfd, struct epoll_event *events, int maxevents, int timeout,
                const sigset_t *sigmask);

/* FD table internal API */
#include "fdtable.h"

typedef struct epitem {
    int fd;
    struct epoll_event event;
    uint32_t registered_events;
    bool is_registered;
    bool edge_triggered;
    struct epitem *next;
    struct epitem *prev;
} epitem_t;

typedef struct epoll_instance {
    int kq;
    uint32_t flags;
    int size;
    atomic_int ref_count;
    pthread_mutex_t lock;
    epitem_t **items;
    int hash_size;
    int item_count;
    uint64_t total_events;
} epoll_instance_t;

static inline int epoll_hash(epoll_instance_t *epi, int fd) {
    return fd & (epi->hash_size - 1);
}

static epitem_t *epoll_find_item(epoll_instance_t *epi, int fd) {
    int idx = epoll_hash(epi, fd);
    epitem_t *item = epi->items[idx];

    while (item) {
        if (item->fd == fd) {
            return item;
        }
        item = item->next;
    }
    return NULL;
}

static void epoll_add_item(epoll_instance_t *epi, epitem_t *item) {
    int idx = epoll_hash(epi, item->fd);
    item->next = epi->items[idx];
    item->prev = NULL;
    if (epi->items[idx]) {
        epi->items[idx]->prev = item;
    }
    epi->items[idx] = item;
    epi->item_count++;
}

static void epoll_remove_item(epoll_instance_t *epi, epitem_t *item) {
    if (item->prev) {
        item->prev->next = item->next;
    } else {
        int idx = epoll_hash(epi, item->fd);
        epi->items[idx] = item->next;
    }
    if (item->next) {
        item->next->prev = item->prev;
    }
    epi->item_count--;
}

static uint32_t epoll_to_kqueue_flags(uint32_t epoll_events) {
    uint32_t kflags = 0;

    if (epoll_events & EPOLLET) {
        kflags |= EV_CLEAR;
    }
    if (epoll_events & EPOLLONESHOT) {
        kflags |= EV_ONESHOT;
    }

    return kflags;
}

static int epoll_build_kevents(struct kevent *changes, int max_changes, int fd,
                               uint32_t epoll_events, void *udata, bool add_mode) {
    int n = 0;
    uint32_t kflags = epoll_to_kqueue_flags(epoll_events);

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

static uint32_t kqueue_to_epoll_events(int16_t filter, uint16_t flags, int data) {
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

#define MAX_EPOLL_INSTANCES 256
static epoll_instance_t *epoll_table[MAX_EPOLL_INSTANCES];
static pthread_mutex_t epoll_table_lock = PTHREAD_MUTEX_INITIALIZER;

static int epoll_register_instance(epoll_instance_t *epi) {
    pthread_mutex_lock(&epoll_table_lock);
    for (int i = 0; i < MAX_EPOLL_INSTANCES; i++) {
        if (epoll_table[i] == NULL) {
            epoll_table[i] = epi;
            pthread_mutex_unlock(&epoll_table_lock);
            return i + IXLAND_MAX_FD + 100;
        }
    }
    pthread_mutex_unlock(&epoll_table_lock);
    return -1;
}

static epoll_instance_t *epoll_lookup_instance(int epfd) {
    int idx = epfd - IXLAND_MAX_FD - 100;
    if (idx < 0 || idx >= MAX_EPOLL_INSTANCES) {
        return NULL;
    }
    return epoll_table[idx];
}

static void epoll_unregister_instance(int epfd) {
    int idx = epfd - IXLAND_MAX_FD - 100;
    if (idx >= 0 && idx < MAX_EPOLL_INSTANCES) {
        pthread_mutex_lock(&epoll_table_lock);
        epoll_table[idx] = NULL;
        pthread_mutex_unlock(&epoll_table_lock);
    }
}

int epoll_create(int size) {
    if (size <= 0) {
        errno = EINVAL;
        return -1;
    }

    return epoll_create1(0);
}

int epoll_create1(int flags) {
    if (flags & ~EPOLL_CLOEXEC) {
        errno = EINVAL;
        return -1;
    }

    epoll_instance_t *epi = calloc(1, sizeof(epoll_instance_t));
    if (!epi) {
        errno = ENOMEM;
        return -1;
    }

    epi->kq = kqueue();
    if (epi->kq < 0) {
        free(epi);
        errno = EMFILE;
        return -1;
    }

    if (flags & EPOLL_CLOEXEC) {
        int fd_flags = fcntl(epi->kq, F_GETFD);
        if (fd_flags >= 0) {
            fcntl(epi->kq, F_SETFD, fd_flags | FD_CLOEXEC);
        }
    }

    epi->hash_size = 16;
    epi->items = calloc(epi->hash_size, sizeof(epitem_t *));
    if (!epi->items) {
        close(epi->kq);
        free(epi);
        errno = ENOMEM;
        return -1;
    }

    epi->flags = (uint32_t)flags;
    epi->item_count = 0;
    epi->total_events = 0;
    atomic_init(&epi->ref_count, 1);
    pthread_mutex_init(&epi->lock, NULL);

    int epfd = epoll_register_instance(epi);
    if (epfd < 0) {
        free(epi->items);
        close(epi->kq);
        free(epi);
        errno = EMFILE;
        return -1;
    }

    return epfd;
}

int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event) {
    epoll_instance_t *epi = epoll_lookup_instance(epfd);
    if (!epi) {
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

    pthread_mutex_lock(&epi->lock);

    epitem_t *item = epoll_find_item(epi, fd);

    switch (op) {
    case EPOLL_CTL_ADD: {
        if (!event) {
            pthread_mutex_unlock(&epi->lock);
            errno = EINVAL;
            return -1;
        }

        if (item != NULL) {
            pthread_mutex_unlock(&epi->lock);
            errno = EEXIST;
            return -1;
        }

        item = calloc(1, sizeof(epitem_t));
        if (!item) {
            pthread_mutex_unlock(&epi->lock);
            errno = ENOMEM;
            return -1;
        }

        item->fd = fd;
        memcpy(&item->event, event, sizeof(struct epoll_event));
        item->is_registered = true;
        item->edge_triggered = (event->events & EPOLLET) != 0;

        struct kevent changes[2];
        int nchanges =
            epoll_build_kevents(changes, 2, fd, event->events & ~EPOLLONESHOT, item, true);

        if (nchanges > 0) {
            int ret = kevent(epi->kq, changes, nchanges, NULL, 0, NULL);
            if (ret < 0) {
                free(item);
                pthread_mutex_unlock(&epi->lock);
                errno = EPERM;
                return -1;
            }
        }

        epoll_add_item(epi, item);
        break;
    }

    case EPOLL_CTL_DEL: {
        if (item == NULL) {
            pthread_mutex_unlock(&epi->lock);
            errno = ENOENT;
            return -1;
        }

        struct kevent changes[2];
        int nchanges = epoll_build_kevents(changes, 2, fd, item->registered_events, item, false);
        if (nchanges > 0) {
            kevent(epi->kq, changes, nchanges, NULL, 0, NULL);
        }

        epoll_remove_item(epi, item);
        free(item);
        break;
    }

    case EPOLL_CTL_MOD: {
        if (!event) {
            pthread_mutex_unlock(&epi->lock);
            errno = EINVAL;
            return -1;
        }

        if (item == NULL) {
            pthread_mutex_unlock(&epi->lock);
            errno = ENOENT;
            return -1;
        }

        struct kevent changes[2];
        int nchanges = epoll_build_kevents(changes, 2, fd, item->registered_events, item, false);
        if (nchanges > 0) {
            kevent(epi->kq, changes, nchanges, NULL, 0, NULL);
        }

        memcpy(&item->event, event, sizeof(struct epoll_event));
        item->edge_triggered = (event->events & EPOLLET) != 0;

        nchanges = epoll_build_kevents(changes, 2, fd, event->events & ~EPOLLONESHOT, item, true);
        if (nchanges > 0) {
            int ret = kevent(epi->kq, changes, nchanges, NULL, 0, NULL);
            if (ret < 0) {
                pthread_mutex_unlock(&epi->lock);
                errno = EPERM;
                return -1;
            }
        }
        break;
    }

    default:
        pthread_mutex_unlock(&epi->lock);
        errno = EINVAL;
        return -1;
    }

    pthread_mutex_unlock(&epi->lock);
    return 0;
}

int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout) {
    return epoll_pwait(epfd, events, maxevents, timeout, NULL);
}

int epoll_pwait(int epfd, struct epoll_event *events, int maxevents, int timeout,
                const sigset_t *sigmask) {
    if (!events || maxevents <= 0) {
        errno = EINVAL;
        return -1;
    }

    epoll_instance_t *epi = epoll_lookup_instance(epfd);
    if (!epi) {
        errno = EBADF;
        return -1;
    }

    sigset_t oldmask;
    sigset_t newmask;
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

    int nevents = kevent(epi->kq, NULL, 0, kevents, maxevents, tsp);

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
        epitem_t *item = (epitem_t *)kevents[i].udata;
        if (!item)
            continue;

        events[ready_count].events =
            kqueue_to_epoll_events(kevents[i].filter, kevents[i].flags, (int)kevents[i].data);

        memcpy(&events[ready_count].data, &item->event.data, sizeof(union epoll_data));

        if (item->event.events & EPOLLONESHOT) {
            pthread_mutex_lock(&epi->lock);
            struct kevent changes[2];
            int nchanges =
                epoll_build_kevents(changes, 2, item->fd, item->registered_events, item, false);
            if (nchanges > 0) {
                kevent(epi->kq, changes, nchanges, NULL, 0, NULL);
            }
            item->is_registered = false;
            pthread_mutex_unlock(&epi->lock);
        }

        ready_count++;
    }

    free(kevents);

    if (sigmask) {
        pthread_sigmask(SIG_SETMASK, &oldmask, NULL);
    }

    epi->total_events += ready_count;
    return ready_count;
}
