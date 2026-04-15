#include "readiness.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "../include/ixland/ixland_types.h"

/* ============================================================================
 * POLL - Internal implementation
 * ============================================================================ */

int do_poll(struct pollfd *fds, unsigned int nfds, int timeout) {
    (void)fds;
    (void)nfds;
    (void)timeout;
    /* Abstract poll not implemented - use Darwin bridge */
    errno = ENOSYS;
    return -1;
}

int do_ppoll(struct pollfd *fds, unsigned int nfds, const struct timespec *timeout,
             const sigset_t *sigmask) {
    (void)fds;
    (void)nfds;
    (void)timeout;
    (void)sigmask;
    errno = ENOSYS;
    return -1;
}

/* ============================================================================
 * SELECT - Internal implementation with Linux-shaped fd_set
 * ============================================================================ */

int do_select(int nfds, readiness_fd_set_t *readfds, readiness_fd_set_t *writefds,
              readiness_fd_set_t *exceptfds, struct timeval *timeout) {
    /* Validate arguments */
    if (nfds < 0 || nfds > NR_READINESS_FDS) {
        errno = EINVAL;
        return -1;
    }

    /* Empty fd sets + timeout = just sleep */
    if (nfds == 0 || (readfds == NULL && writefds == NULL && exceptfds == NULL)) {
        if (timeout) {
            struct timespec ts;
            ts.tv_sec = timeout->tv_sec;
            ts.tv_nsec = timeout->tv_usec * 1000L;
            nanosleep(&ts, NULL);
        }
        return 0;
    }

    /* Validate all requested fds */
    for (int fd = 0; fd < nfds; fd++) {
        int requested = 0;
        if (readfds && readiness_fd_isset(fd, readfds)) requested = 1;
        if (writefds && readiness_fd_isset(fd, writefds)) requested = 1;
        if (exceptfds && readiness_fd_isset(fd, exceptfds)) requested = 1;

        if (requested && fcntl(fd, F_GETFL) < 0) {
            errno = EBADF;
            return -1;
        }
    }

    /*
     * For now, return ENOSYS - the actual kqueue-based implementation
     * belongs in a Darwin bridge file that translates between
     * internal readiness_fd_set_t and Darwin fd_set + kevent.
     */
    (void)timeout;
    errno = ENOSYS;
    return -1;
}

int do_pselect(int nfds, readiness_fd_set_t *readfds, readiness_fd_set_t *writefds,
               readiness_fd_set_t *exceptfds, const struct timespec *timeout,
               const sigset_t *sigmask) {
    struct timeval tv;
    struct timeval *tvp = NULL;

    if (timeout) {
        tv.tv_sec = timeout->tv_sec;
        tv.tv_usec = (__darwin_suseconds_t)(timeout->tv_nsec / 1000);
        tvp = &tv;
    }

    /* Mask handling would go here in full implementation */
    (void)sigmask;

    return do_select(nfds, readfds, writefds, exceptfds, tvp);
}
