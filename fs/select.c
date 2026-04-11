#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/event.h>
#include <sys/time.h>
#include <unistd.h>

typedef sigset_t ixland_native_sigset_t;

#include "../src/ixland/internal/ixland_internal.h"

static short ixland_kfilter_to_poll_revents(int16_t filter, uint16_t flags) {
    short revents = 0;

    if (filter == EVFILT_READ) {
        revents |= IXLAND_POLLIN;
    } else if (filter == EVFILT_WRITE) {
        revents |= IXLAND_POLLOUT;
    }

    if (flags & EV_ERROR) {
        revents |= IXLAND_POLLERR;
    }

    if (flags & EV_EOF) {
        revents |= IXLAND_POLLHUP;
    }

    return revents;
}

static int ixland_poll_kqueue(struct linux_pollfd *fds, unsigned int nfds, int timeout_ms) {
    if (!fds) {
        errno = EFAULT;
        return -1;
    }

    if (nfds == 0) {
        if (timeout_ms > 0) {
            usleep(timeout_ms * 1000);
        } else if (timeout_ms < 0) {
            pause();
            errno = EINTR;
            return -1;
        }
        return 0;
    }

    int kq = kqueue();
    if (kq < 0) {
        errno = ENOMEM;
        return -1;
    }

    struct kevent *changelist = calloc(nfds * 2, sizeof(struct kevent));
    if (!changelist) {
        close(kq);
        errno = ENOMEM;
        return -1;
    }

    int nchanges = 0;
    for (unsigned int i = 0; i < nfds; i++) {
        fds[i].revents = 0;

        if (fds[i].fd < 0) {
            fds[i].revents = IXLAND_POLLNVAL;
            continue;
        }

        if (fcntl(fds[i].fd, F_GETFL) < 0) {
            fds[i].revents = IXLAND_POLLNVAL;
            continue;
        }

        if (fds[i].events &
            (IXLAND_POLLIN | IXLAND_POLLRDNORM | IXLAND_POLLRDBAND | IXLAND_POLLPRI)) {
            EV_SET(&changelist[nchanges], fds[i].fd, EVFILT_READ, EV_ADD, 0, 0,
                   (void *)(uintptr_t)i);
            nchanges++;
        }

        if (fds[i].events & (IXLAND_POLLOUT | IXLAND_POLLWRNORM | IXLAND_POLLWRBAND)) {
            EV_SET(&changelist[nchanges], fds[i].fd, EVFILT_WRITE, EV_ADD, 0, 0,
                   (void *)(uintptr_t)i);
            nchanges++;
        }
    }

    if (nchanges == 0) {
        free(changelist);
        close(kq);

        for (unsigned int i = 0; i < nfds; i++) {
            if (fds[i].revents & IXLAND_POLLNVAL) {
                return 1;
            }
        }
        return 0;
    }

    struct timespec ts;
    struct timespec *tsp = NULL;
    if (timeout_ms >= 0) {
        ts.tv_sec = timeout_ms / 1000;
        ts.tv_nsec = (timeout_ms % 1000) * 1000000;
        tsp = &ts;
    }

    struct kevent *eventlist = calloc(nfds * 2, sizeof(struct kevent));
    if (!eventlist) {
        free(changelist);
        close(kq);
        errno = ENOMEM;
        return -1;
    }

    int nevents = kevent(kq, changelist, nchanges, eventlist, nfds * 2, tsp);

    if (nevents < 0) {
        int saved_errno = errno;
        free(changelist);
        free(eventlist);
        close(kq);
        errno = (saved_errno == EINTR) ? EINTR : EFAULT;
        return -1;
    }

    int ready_count = 0;
    for (int i = 0; i < nevents; i++) {
        unsigned int idx = (unsigned int)(uintptr_t)eventlist[i].udata;
        if (idx < nfds) {
            fds[idx].revents |=
                ixland_kfilter_to_poll_revents(eventlist[i].filter, eventlist[i].flags);
            ready_count++;
        }
    }

    free(changelist);
    free(eventlist);
    close(kq);

    return ready_count;
}

int ixland_poll(struct linux_pollfd *fds, unsigned int nfds, int timeout) {
    if (!fds) {
        errno = EFAULT;
        return -1;
    }

    return ixland_poll_kqueue(fds, nfds, timeout);
}

int ixland_ppoll(struct linux_pollfd *fds, unsigned int nfds, const struct linux_timespec *timeout,
                 const linux_sigset_t *sigmask) {
    int timeout_ms = -1;
    if (timeout) {
        timeout_ms = (int)(timeout->tv_sec * 1000 + timeout->tv_nsec / 1000000);
    }

    ixland_native_sigset_t oldmask;
    ixland_native_sigset_t newmask;
    if (sigmask) {
        memset(&newmask, 0, sizeof(newmask));
        memcpy(&newmask, sigmask, sizeof(newmask) < 128 ? sizeof(newmask) : 128);
        pthread_sigmask(SIG_SETMASK, &newmask, &oldmask);
    }

    int result = ixland_poll(fds, nfds, timeout_ms);

    if (sigmask) {
        pthread_sigmask(SIG_SETMASK, &oldmask, NULL);
    }

    return result;
}

int ixland_select(int nfds, linux_fd_set_t *readfds, linux_fd_set_t *writefds,
                  linux_fd_set_t *exceptfds, struct linux_timeval *timeout) {
    if (nfds < 0 || nfds > IXLAND_FD_SETSIZE) {
        errno = EINVAL;
        return -1;
    }

    if (nfds == 0 || (readfds == NULL && writefds == NULL && exceptfds == NULL)) {
        if (timeout) {
            struct timespec ts;
            ts.tv_sec = timeout->tv_sec;
            ts.tv_nsec = timeout->tv_usec * 1000;
            nanosleep(&ts, NULL);
        }
        return 0;
    }

    for (int fd = 0; fd < nfds; fd++) {
        bool requested = false;

        if (readfds && IXLAND_FD_ISSET(fd, readfds)) {
            requested = true;
        }
        if (writefds && IXLAND_FD_ISSET(fd, writefds)) {
            requested = true;
        }
        if (exceptfds && IXLAND_FD_ISSET(fd, exceptfds)) {
            requested = true;
        }

        if (requested && fcntl(fd, F_GETFL) < 0) {
            errno = EBADF;
            return -1;
        }
    }

    int kq = kqueue();
    if (kq < 0) {
        errno = ENOMEM;
        return -1;
    }

    int max_changes = nfds * 3;
    struct kevent *changelist = calloc(max_changes, sizeof(struct kevent));
    if (!changelist) {
        close(kq);
        errno = ENOMEM;
        return -1;
    }

    int nchanges = 0;

    for (int fd = 0; fd < nfds; fd++) {
        if (readfds && IXLAND_FD_ISSET(fd, readfds)) {
            if (fcntl(fd, F_GETFL) < 0) {
                free(changelist);
                close(kq);
                errno = EBADF;
                return -1;
            }
            EV_SET(&changelist[nchanges], fd, EVFILT_READ, EV_ADD, 0, 0,
                   (void *)(intptr_t)(fd + 1));
            nchanges++;
        }

        if (writefds && IXLAND_FD_ISSET(fd, writefds)) {
            if (fcntl(fd, F_GETFL) < 0) {
                free(changelist);
                close(kq);
                errno = EBADF;
                return -1;
            }
            EV_SET(&changelist[nchanges], fd, EVFILT_WRITE, EV_ADD, 0, 0,
                   (void *)(intptr_t)(fd + 1));
            nchanges++;
        }

        if (exceptfds && IXLAND_FD_ISSET(fd, exceptfds)) {
            if (fcntl(fd, F_GETFL) < 0) {
                free(changelist);
                close(kq);
                errno = EBADF;
                return -1;
            }
            EV_SET(&changelist[nchanges], fd, EVFILT_READ, EV_ADD | EV_OOBAND, 0, 0,
                   (void *)(intptr_t)(fd + 1));
            nchanges++;
        }
    }

    if (nchanges == 0) {
        free(changelist);
        close(kq);
        if (timeout) {
            struct timespec ts;
            ts.tv_sec = timeout->tv_sec;
            ts.tv_nsec = timeout->tv_usec * 1000;
            nanosleep(&ts, NULL);
        }
        return 0;
    }

    struct timespec ts;
    struct timespec *tsp = NULL;
    if (timeout) {
        ts.tv_sec = timeout->tv_sec;
        ts.tv_nsec = timeout->tv_usec * 1000;
        tsp = &ts;
    }

    struct kevent *eventlist = calloc(nchanges, sizeof(struct kevent));
    if (!eventlist) {
        free(changelist);
        close(kq);
        errno = ENOMEM;
        return -1;
    }

    if (readfds)
        IXLAND_FD_ZERO(readfds);
    if (writefds)
        IXLAND_FD_ZERO(writefds);
    if (exceptfds)
        IXLAND_FD_ZERO(exceptfds);

    int nevents = kevent(kq, changelist, nchanges, eventlist, nchanges, tsp);

    if (nevents < 0) {
        int saved_errno = errno;
        free(changelist);
        free(eventlist);
        close(kq);
        errno = (saved_errno == EINTR) ? EINTR : EBADF;
        return -1;
    }

    int ready_count = 0;
    for (int i = 0; i < nevents; i++) {
        int fd = (int)(intptr_t)eventlist[i].udata - 1;
        if (fd < 0 || fd >= nfds)
            continue;

        if (eventlist[i].filter == EVFILT_READ) {
            if (eventlist[i].flags & EV_OOBAND) {
                if (exceptfds)
                    IXLAND_FD_SET(fd, exceptfds);
            } else {
                if (readfds)
                    IXLAND_FD_SET(fd, readfds);
            }
            ready_count++;
        } else if (eventlist[i].filter == EVFILT_WRITE) {
            if (writefds)
                IXLAND_FD_SET(fd, writefds);
            ready_count++;
        }
    }

    free(changelist);
    free(eventlist);
    close(kq);

    return ready_count;
}

int ixland_pselect(int nfds, linux_fd_set_t *readfds, linux_fd_set_t *writefds,
                   linux_fd_set_t *exceptfds, const struct linux_timespec *timeout,
                   const linux_sigset_t *sigmask) {
    struct linux_timeval tv;
    struct linux_timeval *tvp = NULL;
    if (timeout) {
        tv.tv_sec = timeout->tv_sec;
        tv.tv_usec = timeout->tv_nsec / 1000;
        tvp = &tv;
    }

    ixland_native_sigset_t oldmask;
    ixland_native_sigset_t newmask;
    if (sigmask) {
        memset(&newmask, 0, sizeof(newmask));
        memcpy(&newmask, sigmask, sizeof(newmask) < 128 ? sizeof(newmask) : 128);
        pthread_sigmask(SIG_SETMASK, &newmask, &oldmask);
    }

    int result = ixland_select(nfds, readfds, writefds, exceptfds, tvp);

    if (sigmask) {
        pthread_sigmask(SIG_SETMASK, &oldmask, NULL);
    }

    return result;
}
