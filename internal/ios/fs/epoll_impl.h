/* internal/ios/fs/epoll_impl.h
 * Internal epoll implementation declarations for Linux-owner code
 *
 * This header declares the internal _impl functions that are implemented
 * in fs/eventpoll.c and called by the host-facing bridge.
 */

#ifndef EPOLL_IMPL_H
#define EPOLL_IMPL_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* epoll_data_t union (internal representation) */
typedef union epoll_data_internal {
    void *ptr;
    int fd;
    uint32_t u32;
    uint64_t u64;
} epoll_data_internal_t;

/* epoll_event structure (internal representation) */
typedef struct epoll_event_internal {
    uint32_t events;
    epoll_data_internal_t data;
} epoll_event_internal_t;

/* Internal epoll implementation functions called by bridge */
int epoll_create1_impl(int flags);
int epoll_ctl_impl(int epfd, int op, int fd, epoll_event_internal_t *event);
int epoll_pwait_impl(int epfd, epoll_event_internal_t *events, int maxevents, int timeout);
int epoll_close_impl(int epfd);

#ifdef __cplusplus
}
#endif

#endif /* EPOLL_IMPL_H */
