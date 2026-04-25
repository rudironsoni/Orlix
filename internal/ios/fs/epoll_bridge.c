/* internal/ios/fs/epoll_bridge.c
 * Host bridge for epoll public API wrappers and signal mask operations
 */

#include <signal.h>
#include <stdbool.h>
#include <string.h>

#include "epoll_bridge.h"
#include "epoll_impl.h"
#include "backing_io_decls.h"

typedef struct epoll_sigmask_state_internal {
    sigset_t oldmask;
    bool saved;
} epoll_sigmask_state_internal_t;

/* Ensure internal struct fits in opaque state */
_Static_assert(sizeof(epoll_sigmask_state_internal_t) <= sizeof(epoll_sigmask_state_t),
               "epoll_sigmask_state_t must be large enough for internal state");

bool epoll_sigmask_save(epoll_sigmask_state_t *state, const sigset_t *sigmask) {
    if (!state)
        return false;

    epoll_sigmask_state_internal_t *internal = (epoll_sigmask_state_internal_t *)state;
    internal->saved = false;

    if (sigmask) {
        sigset_t newmask;
        memset(&newmask, 0, sizeof(newmask));
        memcpy(&newmask, sigmask, sizeof(newmask) < 128 ? sizeof(newmask) : 128);
        pthread_sigmask(SIG_SETMASK, &newmask, &internal->oldmask);
        internal->saved = true;
    }

    return internal->saved;
}

void epoll_sigmask_restore(epoll_sigmask_state_t *state) {
    if (!state)
        return;

    epoll_sigmask_state_internal_t *internal = (epoll_sigmask_state_internal_t *)state;

    if (internal->saved) {
        pthread_sigmask(SIG_SETMASK, &internal->oldmask, NULL);
        internal->saved = false;
    }
}

/* Public API wrappers that call internal _impl functions */

int epoll_create1(int flags) {
    return epoll_create1_impl(flags);
}

int epoll_ctl(int epfd, int op, int fd, epoll_event_internal_t *event) {
    return epoll_ctl_impl(epfd, op, fd, event);
}

int epoll_pwait(int epfd, epoll_event_internal_t *events, int maxevents, int timeout,
                const sigset_t *sigmask) {
    epoll_sigmask_state_t sigmask_state;
    bool sigmask_saved = epoll_sigmask_save(&sigmask_state, sigmask);

    int result = epoll_pwait_impl(epfd, events, maxevents, timeout);

    if (sigmask_saved) {
        epoll_sigmask_restore(&sigmask_state);
    }

    return result;
}

int epoll_wait(int epfd, epoll_event_internal_t *events, int maxevents, int timeout) {
    return epoll_pwait(epfd, events, maxevents, timeout, NULL);
}

int epoll_close(int epfd) {
    return epoll_close_impl(epfd);
}
