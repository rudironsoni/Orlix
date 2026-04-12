/* ixland_network.c - Network syscall implementation for iOS
 *
 * Provides BSD socket API compatibility layer for iOS
 * Maps Linux socket calls to iOS Network.framework and BSD sockets
 */

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/* iOS Network framework includes */
#import <Network/Network.h>

/* ============================================================================
 * NETWORK STATE
 * ============================================================================ */

/* Socket table entry - maps virtual fds to real iOS sockets */
typedef struct {
    int used;
    int domain;                 /* AF_INET, AF_INET6, AF_UNIX */
    int type;                   /* SOCK_STREAM, SOCK_DGRAM */
    int protocol;               /* IPPROTO_TCP, IPPROTO_UDP */
    nw_connection_t connection; /* iOS Network connection */
    nw_listener_t listener;     /* iOS Network listener (for server) */
    struct sockaddr_storage local_addr;
    struct sockaddr_storage remote_addr;
    socklen_t local_addr_len;
    socklen_t remote_addr_len;
} ixland_socket_entry_t;

#define IXLAND_MAX_SOCKETS 256
static ixland_socket_entry_t socket_table[IXLAND_MAX_SOCKETS];
static pthread_mutex_t socket_table_lock = PTHREAD_MUTEX_INITIALIZER;

/* Network initialization state */
static atomic_int network_initialized = 0;

/* ============================================================================
 * SOCKET TABLE MANAGEMENT
 * ============================================================================ */

static int ixland_socket_alloc(void) {
    pthread_mutex_lock(&socket_table_lock);
    for (int i = 0; i < IXLAND_MAX_SOCKETS; i++) {
        if (!socket_table[i].used) {
            socket_table[i].used = 1;
            pthread_mutex_unlock(&socket_table_lock);
            return i;
        }
    }
    pthread_mutex_unlock(&socket_table_lock);
    errno = EMFILE;
    return -1;
}

static void ixland_socket_free(int fd) {
    if (fd < 0 || fd >= IXLAND_MAX_SOCKETS)
        return;

    pthread_mutex_lock(&socket_table_lock);
    if (socket_table[fd].used) {
        /* Release iOS Network resources */
        if (socket_table[fd].connection) {
            nw_connection_cancel(socket_table[fd].connection);
            socket_table[fd].connection = NULL;
        }
        if (socket_table[fd].listener) {
            nw_listener_cancel(socket_table[fd].listener);
            socket_table[fd].listener = NULL;
        }
        memset(&socket_table[fd], 0, sizeof(ixland_socket_entry_t));
    }
    pthread_mutex_unlock(&socket_table_lock);
}

static ixland_socket_entry_t *ixland_socket_get(int fd) {
    if (fd < 0 || fd >= IXLAND_MAX_SOCKETS) {
        errno = EBADF;
        return NULL;
    }
    if (!socket_table[fd].used) {
        errno = EBADF;
        return NULL;
    }
    return &socket_table[fd];
}

/* ============================================================================
 * NETWORK INITIALIZATION
 * ============================================================================ */

int __ixland_network_init_impl(void) {
    if (atomic_load(&network_initialized)) {
        return 0;
    }

    /* Initialize socket table */
    memset(socket_table, 0, sizeof(socket_table));

    atomic_store(&network_initialized, 1);
    return 0;
}

int __ixland_network_deinit_impl(void) {
    if (!atomic_load(&network_initialized)) {
        return 0;
    }

    /* Close all sockets */
    pthread_mutex_lock(&socket_table_lock);
    for (int i = 0; i < IXLAND_MAX_SOCKETS; i++) {
        if (socket_table[i].used) {
            if (socket_table[i].connection) {
                nw_connection_cancel(socket_table[i].connection);
            }
            if (socket_table[i].listener) {
                nw_listener_cancel(socket_table[i].listener);
            }
        }
    }
    memset(socket_table, 0, sizeof(socket_table));
    pthread_mutex_unlock(&socket_table_lock);

    atomic_store(&network_initialized, 0);
    return 0;
}

/* ============================================================================
 * SOCKET CREATION
 * ============================================================================ */

int __ixland_socket_impl(int domain, int type, int protocol) {
    int fd = ixland_socket_alloc();
    if (fd < 0) {
        return -1;
    }

    ixland_socket_entry_t *sock = &socket_table[fd];
    sock->domain = domain;
    sock->type = type;
    sock->protocol = protocol;

    /* Map to iOS Network framework based on domain/type */
    switch (domain) {
    case AF_INET:
    case AF_INET6:
        /* TCP/UDP sockets - use BSD sockets for now */
        /* TODO: Implement using Network.framework for modern iOS */
        break;

    case AF_UNIX:
        /* Unix domain sockets - not supported on iOS */
        errno = EAFNOSUPPORT;
        ixland_socket_free(fd);
        return -1;

    default:
        errno = EAFNOSUPPORT;
        ixland_socket_free(fd);
        return -1;
    }

    return fd;
}

int __ixland_socketpair_impl(int domain, int type, int protocol, int sv[2]) {
    /* Socket pairs not supported on iOS (sandbox restriction) */
    (void)domain;
    (void)type;
    (void)protocol;
    (void)sv;
    errno = EOPNOTSUPP;
    return -1;
}

/* ============================================================================
 * CONNECTION MANAGEMENT
 * ============================================================================ */

int __ixland_connect_impl(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    ixland_socket_entry_t *sock = ixland_socket_get(sockfd);
    if (!sock) {
        return -1;
    }

    /* Validate address */
    if (!addr || addrlen == 0) {
        errno = EINVAL;
        return -1;
    }

    /* Copy remote address */
    if (addrlen > sizeof(sock->remote_addr)) {
        addrlen = sizeof(sock->remote_addr);
    }
    memcpy(&sock->remote_addr, addr, addrlen);
    sock->remote_addr_len = addrlen;

    /* For TCP sockets, create Network.framework connection */
    if (sock->type == SOCK_STREAM) {
        /* Create endpoint from address */
        nw_endpoint_t endpoint = NULL;

        if (addr->sa_family == AF_INET) {
            struct sockaddr_in *sin = (struct sockaddr_in *)addr;
            char addr_str[INET_ADDRSTRLEN];
            char port_str[6];
            inet_ntop(AF_INET, &sin->sin_addr, addr_str, sizeof(addr_str));
            snprintf(port_str, sizeof(port_str), "%u", ntohs(sin->sin_port));
            endpoint = nw_endpoint_create_host(addr_str, port_str);
        } else if (addr->sa_family == AF_INET6) {
            struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)addr;
            char addr_str[INET6_ADDRSTRLEN];
            char port_str[6];
            inet_ntop(AF_INET6, &sin6->sin6_addr, addr_str, sizeof(addr_str));
            snprintf(port_str, sizeof(port_str), "%u", ntohs(sin6->sin6_port));
            endpoint = nw_endpoint_create_host(addr_str, port_str);
        }

        if (!endpoint) {
            errno = EINVAL;
            return -1;
        }

        /* Create TCP parameters */
        nw_parameters_t params = nw_parameters_create_secure_tcp(
            NW_PARAMETERS_DISABLE_PROTOCOL, NW_PARAMETERS_DEFAULT_CONFIGURATION);

        /* Create connection */
        sock->connection = nw_connection_create(endpoint, params);
        nw_release(endpoint);
        nw_release(params);

        if (!sock->connection) {
            errno = ENOMEM;
            return -1;
        }

        /* Start connection */
        nw_connection_set_queue(sock->connection, dispatch_get_main_queue());

        dispatch_semaphore_t sem = dispatch_semaphore_create(0);
        __block int connect_error = 0;

        nw_connection_set_state_changed_handler(
            sock->connection, ^(nw_connection_state_t state, nw_error_t error) {
              (void)error;
              if (state == nw_connection_state_ready) {
                  connect_error = 0;
              } else if (state == nw_connection_state_failed ||
                         state == nw_connection_state_cancelled) {
                  connect_error = -1;
              }
              dispatch_semaphore_signal(sem);
            });

        nw_connection_start(sock->connection);

        /* Wait for connection with timeout */
        dispatch_time_t timeout = dispatch_time(DISPATCH_TIME_NOW, 30 * NSEC_PER_SEC);
        if (dispatch_semaphore_wait(sem, timeout) != 0) {
            nw_connection_cancel(sock->connection);
            sock->connection = NULL;
            errno = ETIMEDOUT;
            return -1;
        }

        if (connect_error != 0) {
            nw_connection_cancel(sock->connection);
            sock->connection = NULL;
            errno = ECONNREFUSED;
            return -1;
        }
    }

    return 0;
}

int __ixland_bind_impl(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    ixland_socket_entry_t *sock = ixland_socket_get(sockfd);
    if (!sock) {
        return -1;
    }

    /* Validate address */
    if (!addr || addrlen == 0) {
        errno = EINVAL;
        return -1;
    }

    /* Copy local address */
    if (addrlen > sizeof(sock->local_addr)) {
        addrlen = sizeof(sock->local_addr);
    }
    memcpy(&sock->local_addr, addr, addrlen);
    sock->local_addr_len = addrlen;

    return 0;
}

int __ixland_listen_impl(int sockfd, int backlog) {
    ixland_socket_entry_t *sock = ixland_socket_get(sockfd);
    if (!sock) {
        return -1;
    }

    (void)backlog;

    /* For TCP sockets, create listener */
    if (sock->type != SOCK_STREAM) {
        errno = EOPNOTSUPP;
        return -1;
    }

    /* Create parameters */
    nw_parameters_t params = nw_parameters_create_secure_tcp(NW_PARAMETERS_DEFAULT_CONFIGURATION,
                                                             NW_PARAMETERS_DISABLE_PROTOCOL);

    /* Create listener */
    sock->listener = nw_listener_create(params);
    nw_release(params);

    if (!sock->listener) {
        errno = ENOMEM;
        return -1;
    }

    /* Configure listener */
    nw_listener_set_queue(sock->listener, dispatch_get_main_queue());
    nw_listener_set_new_connection_handler(sock->listener, ^(nw_connection_t connection) {
      /* Handle new connection - store for accept */
      /* TODO: Implement accept queue */
      nw_release(connection);
    });

    /* Start listener */
    nw_listener_start(sock->listener);

    return 0;
}

int __ixland_accept_impl(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    ixland_socket_entry_t *sock = ixland_socket_get(sockfd);
    if (!sock) {
        return -1;
    }

    (void)addr;
    (void)addrlen;

    if (!sock->listener) {
        errno = EINVAL;
        return -1;
    }

    /* TODO: Implement accept queue */
    errno = EAGAIN;
    return -1;
}

int __ixland_accept4_impl(int sockfd, struct sockaddr *addr, socklen_t *addrlen, int flags) {
    (void)flags;
    return __ixland_accept_impl(sockfd, addr, addrlen);
}

/* ============================================================================
 * DATA TRANSFER
 * ============================================================================ */

ssize_t __ixland_send_impl(int sockfd, const void *buf, size_t len, int flags) {
    ixland_socket_entry_t *sock = ixland_socket_get(sockfd);
    if (!sock) {
        return -1;
    }

    if (!buf || len == 0) {
        return 0;
    }

    (void)flags;

    if (!sock->connection) {
        errno = ENOTCONN;
        return -1;
    }

    /* Create data */
    dispatch_data_t data = dispatch_data_create(buf, len, NULL, DISPATCH_DATA_DESTRUCTOR_DEFAULT);
    if (!data) {
        errno = ENOMEM;
        return -1;
    }

    /* Send data */
    dispatch_semaphore_t sem = dispatch_semaphore_create(0);
    __block ssize_t bytes_sent = 0;

    nw_connection_send(sock->connection, data, NW_CONNECTION_DEFAULT_MESSAGE_CONTEXT, true,
                       ^(nw_error_t error) {
                         if (error) {
                             bytes_sent = -1;
                         } else {
                             bytes_sent = len;
                         }
                         dispatch_semaphore_signal(sem);
                       });

    dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);
    dispatch_release(sem);
    dispatch_release(data);

    if (bytes_sent < 0) {
        errno = EPIPE;
        return -1;
    }

    return bytes_sent;
}

ssize_t __ixland_recv_impl(int sockfd, void *buf, size_t len, int flags) {
    ixland_socket_entry_t *sock = ixland_socket_get(sockfd);
    if (!sock) {
        return -1;
    }

    if (!buf || len == 0) {
        return 0;
    }

    (void)flags;

    if (!sock->connection) {
        errno = ENOTCONN;
        return -1;
    }

    /* Receive data */
    dispatch_semaphore_t sem = dispatch_semaphore_create(0);
    __block ssize_t bytes_received = 0;

    nw_connection_receive(sock->connection, 1, len,
                          ^(dispatch_data_t content, nw_content_context_t context, bool is_complete,
                            nw_error_t error) {
                            (void)context;
                            (void)is_complete;
                            if (content) {
                                const void *data_ptr = NULL;
                                size_t data_len = 0;

                                dispatch_data_t mapped =
                                    dispatch_data_create_map(content, &data_ptr, &data_len);
                                if (mapped && data_ptr && data_len > 0) {
                                    size_t copy_len = data_len < len ? data_len : len;
                                    memcpy(buf, data_ptr, copy_len);
                                    bytes_received = copy_len;
                                    dispatch_release(mapped);
                                }
                            }

                            if (error) {
                                bytes_received = -1;
                            }

                            dispatch_semaphore_signal(sem);
                          });

    dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);
    dispatch_release(sem);

    if (bytes_received < 0) {
        errno = ECONNRESET;
        return -1;
    }

    return bytes_received;
}

ssize_t __ixland_sendto_impl(int sockfd, const void *buf, size_t len, int flags,
                             const struct sockaddr *dest_addr, socklen_t addrlen) {
    /* For connectionless sockets */
    (void)dest_addr;
    (void)addrlen;
    return __ixland_send_impl(sockfd, buf, len, flags);
}

ssize_t __ixland_recvfrom_impl(int sockfd, void *buf, size_t len, int flags,
                               struct sockaddr *src_addr, socklen_t *addrlen) {
    /* For connectionless sockets */
    (void)src_addr;
    (void)addrlen;
    return __ixland_recv_impl(sockfd, buf, len, flags);
}

/* ============================================================================
 * SOCKET OPTIONS
 * ============================================================================ */

int __ixland_setsockopt_impl(int sockfd, int level, int optname, const void *optval,
                             socklen_t optlen) {
    ixland_socket_entry_t *sock = ixland_socket_get(sockfd);
    if (!sock) {
        return -1;
    }

    (void)level;
    (void)optname;
    (void)optval;
    (void)optlen;

    /* Socket options not fully implemented in this version */
    return 0;
}

int __ixland_getsockopt_impl(int sockfd, int level, int optname, void *optval, socklen_t *optlen) {
    ixland_socket_entry_t *sock = ixland_socket_get(sockfd);
    if (!sock) {
        return -1;
    }

    (void)level;
    (void)optname;

    if (optval && optlen && *optlen >= sizeof(int)) {
        *(int *)optval = 0;
        *optlen = sizeof(int);
    }

    return 0;
}

/* ============================================================================
 * ADDRESS CONVERSION
 * ============================================================================ */

int __ixland_inet_pton_impl(int af, const char *src, void *dst) {
    if (af == AF_INET) {
        return inet_pton(AF_INET, src, dst);
    } else if (af == AF_INET6) {
        return inet_pton(AF_INET6, src, dst);
    } else {
        errno = EAFNOSUPPORT;
        return -1;
    }
}

const char *__ixland_inet_ntop_impl(int af, const void *src, char *dst, socklen_t size) {
    if (af == AF_INET) {
        return inet_ntop(AF_INET, src, dst, size);
    } else if (af == AF_INET6) {
        return inet_ntop(AF_INET6, src, dst, size);
    } else {
        errno = EAFNOSUPPORT;
        return NULL;
    }
}

/* ============================================================================
 * SHUTDOWN
 * ============================================================================ */

int __ixland_shutdown_impl(int sockfd, int how) {
    ixland_socket_entry_t *sock = ixland_socket_get(sockfd);
    if (!sock) {
        return -1;
    }

    (void)how;

    if (sock->connection) {
        nw_connection_cancel(sock->connection);
        sock->connection = NULL;
    }

    return 0;
}

/* ============================================================================
 * PUBLIC API WRAPPERS
 * ============================================================================ */

int ixland_socket(int domain, int type, int protocol) {
    return __ixland_socket_impl(domain, type, protocol);
}

int ixland_socketpair(int domain, int type, int protocol, int sv[2]) {
    return __ixland_socketpair_impl(domain, type, protocol, sv);
}

int ixland_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    return __ixland_connect_impl(sockfd, addr, addrlen);
}

int ixland_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    return __ixland_bind_impl(sockfd, addr, addrlen);
}

int ixland_listen(int sockfd, int backlog) {
    return __ixland_listen_impl(sockfd, backlog);
}

int ixland_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    return __ixland_accept_impl(sockfd, addr, addrlen);
}

int ixland_accept4(int sockfd, struct sockaddr *addr, socklen_t *addrlen, int flags) {
    return __ixland_accept4_impl(sockfd, addr, addrlen, flags);
}

ssize_t ixland_send(int sockfd, const void *buf, size_t len, int flags) {
    return __ixland_send_impl(sockfd, buf, len, flags);
}

ssize_t ixland_recv(int sockfd, void *buf, size_t len, int flags) {
    return __ixland_recv_impl(sockfd, buf, len, flags);
}

ssize_t ixland_sendto(int sockfd, const void *buf, size_t len, int flags,
                      const struct sockaddr *dest_addr, socklen_t addrlen) {
    return __ixland_sendto_impl(sockfd, buf, len, flags, dest_addr, addrlen);
}

ssize_t ixland_recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr,
                        socklen_t *addrlen) {
    return __ixland_recvfrom_impl(sockfd, buf, len, flags, src_addr, addrlen);
}

int ixland_setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen) {
    return __ixland_setsockopt_impl(sockfd, level, optname, optval, optlen);
}

int ixland_getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen) {
    return __ixland_getsockopt_impl(sockfd, level, optname, optval, optlen);
}

int ixland_shutdown(int sockfd, int how) {
    return __ixland_shutdown_impl(sockfd, how);
}

int ixland_inet_pton(int af, const char *src, void *dst) {
    return __ixland_inet_pton_impl(af, src, dst);
}

const char *ixland_inet_ntop(int af, const void *src, char *dst, socklen_t size) {
    return __ixland_inet_ntop_impl(af, src, dst, size);
}

/* Network initialization - called from ixland_init */
int ixland_network_init(void) {
    return __ixland_network_init_impl();
}

int ixland_network_deinit(void) {
    return __ixland_network_deinit_impl();
}
