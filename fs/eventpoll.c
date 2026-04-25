/* iOS Subsystem for Linux - epoll Implementation
 *
 * Linux epoll API using kqueue as the underlying mechanism.
 */

#include <linux/fcntl.h>

#include <errno.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/event.h>
#include <time.h>

#include "internal/ios/fs/sync.h"
#include "internal/ios/fs/epoll_impl.h"

/* Private epoll definitions - internal types not matching Linux UAPI */
#ifndef EPOLL_EVENT_DEFINED
#define EPOLL_EVENT_DEFINED

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
#define EPOLL_CLOEXEC O_CLOEXEC

#endif /* EPOLL_EVENT_DEFINED */

/* FD table internal API */
#include "fdtable.h"

typedef struct epitem {
    int fd;
    epoll_event_internal_t event;
    uint32_t registered_events;
    bool is_registered;
    bool edge_triggered;
    struct epitem *next;
    struct epitem *prev;
} epitem_t;

/* Static helper forward declarations */
static uint32_t kqueue_to_epoll_events(int16_t filter, uint16_t flags, int data);
static int epoll_build_kevents(struct kevent *kev, int max_kev, int fd, uint32_t epoll_events,
                                epitem_t *item, bool add);

typedef struct epoll_instance {
    int kq;
    uint32_t flags;
    int size;
    atomic_int ref_count;
    fs_mutex_t lock;
    epitem_t **items;
    int hash_size;
    int item_count;
    uint64_t total_events;
} epoll_instance_t;

static inline int epoll_hash(epoll_instance_t *epi, int fd) {
    return fd & (epi->hash_size - 1);
}

static int epoll_instance_fd = -1;
static fs_mutex_t epoll_instance_lock = FS_MUTEX_INITIALIZER;
static epoll_instance_t **epoll_instances = NULL;
static int epoll_instance_capacity = 0;

/* Forward declarations for host backing I/O (declared elsewhere) */
int host_fcntl_impl(int fd, int cmd, ...);
int host_close_impl(int fd);

static int epoll_register_instance(epoll_instance_t *epi) {
    if (!epi) {
        errno = EINVAL;
        return -1;
    }

    file_init_impl();

    fs_mutex_lock(&epoll_instance_lock);

    if (!epoll_instances) {
        epoll_instance_capacity = 16;
        epoll_instances = calloc(epoll_instance_capacity, sizeof(epoll_instance_t *));
        if (!epoll_instances) {
            fs_mutex_unlock(&epoll_instance_lock);
            errno = ENOMEM;
            return -1;
        }
        epoll_instance_fd = 1000;
    }

    for (int i = 0; i < epoll_instance_capacity; i++) {
        int fd = epoll_instance_fd + i;
        int idx = fd % epoll_instance_capacity;
        if (!epoll_instances[idx]) {
            epoll_instances[idx] = epi;
            atomic_fetch_add(&epi->ref_count, 1);
            fs_mutex_unlock(&epoll_instance_lock);
            return fd;
        }
    }

    int new_capacity = epoll_instance_capacity * 2;
    epoll_instance_t **new_instances = realloc(epoll_instances, new_capacity * sizeof(epoll_instance_t *));
    if (!new_instances) {
        fs_mutex_unlock(&epoll_instance_lock);
        errno = ENOMEM;
        return -1;
    }

    memset(&new_instances[epoll_instance_capacity], 0, (new_capacity - epoll_instance_capacity) * sizeof(epoll_instance_t *));
    epoll_instances = new_instances;
    epoll_instance_capacity = new_capacity;

    int fd = epoll_instance_fd;
    epoll_instances[fd % epoll_instance_capacity] = epi;
    atomic_fetch_add(&epi->ref_count, 1);

    fs_mutex_unlock(&epoll_instance_lock);
    return fd;
}

static epoll_instance_t *epoll_lookup_instance(int epfd) {
    if (epfd < epoll_instance_fd) {
        return NULL;
    }

    fs_mutex_lock(&epoll_instance_lock);

    if (!epoll_instances) {
        fs_mutex_unlock(&epoll_instance_lock);
        return NULL;
    }

    int idx = epfd % epoll_instance_capacity;
    epoll_instance_t *epi = epoll_instances[idx];

    if (epi) {
        atomic_fetch_add(&epi->ref_count, 1);
    }

    fs_mutex_unlock(&epoll_instance_lock);
    return epi;
}

static void epoll_release_instance(epoll_instance_t *epi) {
    if (!epi) {
        return;
    }

    if (atomic_fetch_sub(&epi->ref_count, 1) == 1) {
        if (epi->items) {
            for (int i = 0; i < epi->hash_size; i++) {
                epitem_t *item = epi->items[i];
                while (item) {
                    epitem_t *next = item->next;
                    free(item);
                    item = next;
                }
            }
            free(epi->items);
        }
        if (epi->kq >= 0) {
            host_close_impl(epi->kq);
        }
        fs_mutex_destroy(&epi->lock);
        free(epi);
    }
}

static int epoll_grow_items(epoll_instance_t *epi) {
    if (!epi) {
        errno = EINVAL;
        return -1;
    }

    int new_hash_size = epi->hash_size * 2;
    epitem_t **new_items = calloc(new_hash_size, sizeof(epitem_t *));
    if (!new_items) {
        errno = ENOMEM;
        return -1;
    }

    for (int i = 0; i < epi->hash_size; i++) {
        epitem_t *item = epi->items[i];
        while (item) {
            epitem_t *next = item->next;
            int new_idx = item->fd & (new_hash_size - 1);
            item->next = new_items[new_idx];
            item->prev = NULL;
            if (new_items[new_idx]) {
                new_items[new_idx]->prev = item;
            }
            new_items[new_idx] = item;
            item = next;
        }
    }

    free(epi->items);
    epi->items = new_items;
    epi->hash_size = new_hash_size;

    return 0;
}

static epitem_t *epoll_find_item(epoll_instance_t *epi, int fd) {
    if (!epi || fd < 0) {
        return NULL;
    }

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

static epitem_t *epoll_add_item(epoll_instance_t *epi, int fd) {
    if (!epi || fd < 0) {
        errno = EINVAL;
        return NULL;
    }

    if (epi->item_count >= epi->hash_size) {
        if (epoll_grow_items(epi) < 0) {
            return NULL;
        }
    }

    epitem_t *item = calloc(1, sizeof(epitem_t));
    if (!item) {
        errno = ENOMEM;
        return NULL;
    }

    item->fd = fd;
    int idx = epoll_hash(epi, fd);
    item->next = epi->items[idx];
    item->prev = NULL;
    if (epi->items[idx]) {
        epi->items[idx]->prev = item;
    }
    epi->items[idx] = item;
    epi->item_count++;

    return item;
}

static void epoll_remove_item(epoll_instance_t *epi, epitem_t *item) {
    if (!epi || !item) {
        return;
    }

    int idx = epoll_hash(epi, item->fd);

    if (item->prev) {
        item->prev->next = item->next;
    } else {
        epi->items[idx] = item->next;
    }

    if (item->next) {
        item->next->prev = item->prev;
    }

    epi->item_count--;
    free(item);
}

int epoll_create1_impl(int flags) {
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
        int fd_flags = host_fcntl_impl(epi->kq, F_GETFD, 0);
        if (fd_flags >= 0) {
            host_fcntl_impl(epi->kq, F_SETFD, fd_flags | FD_CLOEXEC);
        }
    }

    epi->hash_size = 16;
    epi->items = calloc(epi->hash_size, sizeof(epitem_t *));
    if (!epi->items) {
        host_close_impl(epi->kq);
        free(epi);
        errno = ENOMEM;
        return -1;
    }

    epi->flags = (uint32_t)flags;
    epi->item_count = 0;
    epi->total_events = 0;
    atomic_init(&epi->ref_count, 1);
    fs_mutex_init(&epi->lock);

    int epfd = epoll_register_instance(epi);
    if (epfd < 0) {
        free(epi->items);
        host_close_impl(epi->kq);
        free(epi);
        errno = EMFILE;
        return -1;
    }

    return epfd;
}

int epoll_ctl_impl(int epfd, int op, int fd, epoll_event_internal_t *event) {
    epoll_instance_t *epi = epoll_lookup_instance(epfd);
    if (!epi) {
        errno = EBADF;
        return -1;
    }

    if (fd < 0) {
        epoll_release_instance(epi);
        errno = EBADF;
        return -1;
    }

    fs_mutex_lock(&epi->lock);

    epitem_t *item = epoll_find_item(epi, fd);

    switch (op) {
    case EPOLL_CTL_ADD: {
        if (item) {
            fs_mutex_unlock(&epi->lock);
            epoll_release_instance(epi);
            errno = EEXIST;
            return -1;
        }

        item = epoll_add_item(epi, fd);
        if (!item) {
            fs_mutex_unlock(&epi->lock);
            epoll_release_instance(epi);
            return -1;
        }

        if (event) {
            item->event = *event;
            item->edge_triggered = (event->events & EPOLLET) != 0;
        }

        struct kevent kev[2];
        int nkev = 0;

        if (event && (event->events & (EPOLLIN | EPOLLRDNORM))) {
            EV_SET(&kev[nkev++], fd, EVFILT_READ, EV_ADD | (item->edge_triggered ? EV_CLEAR : 0), 0, 0, NULL);
        }
        if (event && (event->events & (EPOLLOUT | EPOLLWRNORM))) {
            EV_SET(&kev[nkev++], fd, EVFILT_WRITE, EV_ADD | (item->edge_triggered ? EV_CLEAR : 0), 0, 0, NULL);
        }

        if (nkev > 0) {
            if (kevent(epi->kq, kev, nkev, NULL, 0, NULL) < 0) {
                epoll_remove_item(epi, item);
                fs_mutex_unlock(&epi->lock);
                epoll_release_instance(epi);
                errno = EBADF;
                return -1;
            }
        }

        item->is_registered = true;
        item->registered_events = event ? event->events : 0;
        break;
    }

    case EPOLL_CTL_DEL: {
        if (!item) {
            fs_mutex_unlock(&epi->lock);
            epoll_release_instance(epi);
            errno = ENOENT;
            return -1;
        }

        struct kevent kev[2];
        int nkev = 0;

        EV_SET(&kev[nkev++], fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
        EV_SET(&kev[nkev++], fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);

        kevent(epi->kq, kev, nkev, NULL, 0, NULL);

        epoll_remove_item(epi, item);
        break;
    }

    case EPOLL_CTL_MOD: {
        if (!item) {
            fs_mutex_unlock(&epi->lock);
            epoll_release_instance(epi);
            errno = ENOENT;
            return -1;
        }

        struct kevent kev[2];
        int nkev = 0;

        EV_SET(&kev[nkev++], fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
        EV_SET(&kev[nkev++], fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);

        kevent(epi->kq, kev, nkev, NULL, 0, NULL);

        if (event) {
            item->event = *event;
            item->edge_triggered = (event->events & EPOLLET) != 0;
        }

        nkev = 0;
        if (event && (event->events & (EPOLLIN | EPOLLRDNORM))) {
            EV_SET(&kev[nkev++], fd, EVFILT_READ, EV_ADD | (item->edge_triggered ? EV_CLEAR : 0), 0, 0, NULL);
        }
        if (event && (event->events & (EPOLLOUT | EPOLLWRNORM))) {
            EV_SET(&kev[nkev++], fd, EVFILT_WRITE, EV_ADD | (item->edge_triggered ? EV_CLEAR : 0), 0, 0, NULL);
        }

        if (nkev > 0) {
            if (kevent(epi->kq, kev, nkev, NULL, 0, NULL) < 0) {
                fs_mutex_unlock(&epi->lock);
                epoll_release_instance(epi);
                errno = EBADF;
                return -1;
            }
        }

        item->registered_events = event ? event->events : 0;
        break;
    }

    default:
        fs_mutex_unlock(&epi->lock);
        epoll_release_instance(epi);
        errno = EINVAL;
        return -1;
    }

    fs_mutex_unlock(&epi->lock);
    epoll_release_instance(epi);
    return 0;
}

int epoll_pwait_impl(int epfd, epoll_event_internal_t *events, int maxevents, int timeout) {
    if (!events || maxevents <= 0) {
        errno = EINVAL;
        return -1;
    }

    epoll_instance_t *epi = epoll_lookup_instance(epfd);
    if (!epi) {
        errno = EBADF;
        return -1;
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
        errno = ENOMEM;
        return -1;
    }

    int nevents = kevent(epi->kq, NULL, 0, kevents, maxevents, tsp);

    if (nevents < 0) {
        int saved_errno = errno;
        free(kevents);
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

        memcpy(&events[ready_count].data, &item->event.data, sizeof(epoll_data_internal_t));

        if (item->event.events & EPOLLONESHOT) {
            fs_mutex_lock(&epi->lock);
            struct kevent changes[2];
            int nchanges =
                epoll_build_kevents(changes, 2, item->fd, item->registered_events, item, false);
            if (nchanges > 0) {
                kevent(epi->kq, changes, nchanges, NULL, 0, NULL);
            }
            item->is_registered = false;
            fs_mutex_unlock(&epi->lock);
        }

        ready_count++;
    }

    free(kevents);

    epi->total_events += ready_count;
    epoll_release_instance(epi);
    return ready_count;
}

int epoll_close_impl(int epfd) {
    epoll_instance_t *epi = epoll_lookup_instance(epfd);
    if (!epi) {
        errno = EBADF;
        return -1;
    }

    fs_mutex_lock(&epoll_instance_lock);
    int idx = epfd % epoll_instance_capacity;
    if (epoll_instances[idx] == epi) {
        epoll_instances[idx] = NULL;
    }
    fs_mutex_unlock(&epoll_instance_lock);

    epoll_release_instance(epi);
    return 0;
}

/* Map kqueue filter+flags to epoll events */
static uint32_t kqueue_to_epoll_events(int16_t filter, uint16_t flags, int data) {
    uint32_t epoll_events = 0;

    if (filter == EVFILT_READ) {
        epoll_events |= EPOLLIN;
        if (data > 0) {
            epoll_events |= EPOLLRDNORM;
        }
    } else if (filter == EVFILT_WRITE) {
        epoll_events |= EPOLLOUT;
        if (data > 0) {
            epoll_events |= EPOLLWRNORM;
        }
    }

    if (flags & EV_ERROR) {
        epoll_events |= EPOLLERR;
    }

    if (flags & EV_EOF) {
        epoll_events |= EPOLLHUP;
    }

    return epoll_events;
}

/* Build kqueue events from epoll events */
static int epoll_build_kevents(struct kevent *kev, int max_kev, int fd, uint32_t epoll_events,
                                epitem_t *item, bool add) {
    int nkev = 0;

    if (epoll_events & (EPOLLIN | EPOLLRDNORM)) {
        if (nkev < max_kev) {
            EV_SET(&kev[nkev], fd, EVFILT_READ, add ? EV_ADD : EV_DELETE, 0, 0, item);
            if (item && item->edge_triggered) {
                kev[nkev].flags |= EV_CLEAR;
            }
            nkev++;
        }
    }

    if (epoll_events & (EPOLLOUT | EPOLLWRNORM)) {
        if (nkev < max_kev) {
            EV_SET(&kev[nkev], fd, EVFILT_WRITE, add ? EV_ADD : EV_DELETE, 0, 0, item);
            if (item && item->edge_triggered) {
                kev[nkev].flags |= EV_CLEAR;
            }
            nkev++;
        }
    }

    return nkev;
}
