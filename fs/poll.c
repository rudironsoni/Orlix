/* IXLandSystem/fs/poll.c
 * Linux-shaped poll() and select() for synthetic fds
 */
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>

#include "fdtable.h"
#include "vfs.h"

#define MAX_POLL_FDS 256

/* Synthetic fd readiness classification */
static bool is_synthetic_fd_class(int fd) {
    if (fd < 0 || fd >= NR_OPEN_DEFAULT) {
        return false;
    }
    
    void *entry = get_fd_entry_impl(fd);
    if (!entry) {
        return false;
    }
    
    bool is_synthetic = get_fd_is_synthetic_proc_file_impl(entry) ||
                        get_fd_is_synthetic_dir_impl(entry) ||
                        get_fd_is_synthetic_dev_impl(entry);
    
    put_fd_entry_impl(entry);
    return is_synthetic;
}

/* Check if synthetic fd is immediately ready for read */
static bool synthetic_fd_read_ready(int fd) {
    void *entry = get_fd_entry_impl(fd);
    if (!entry) {
        return false;
    }
    
    /* All synthetic procfs files, directories, and dev nodes are immediately readable */
    bool ready = get_fd_is_synthetic_proc_file_impl(entry) ||
                 get_fd_is_synthetic_dir_impl(entry) ||
                 get_fd_is_synthetic_dev_impl(entry);
    
    put_fd_entry_impl(entry);
    return ready;
}

/* Check if synthetic fd is immediately ready for write */
static bool synthetic_fd_write_ready(int fd) {
    void *entry = get_fd_entry_impl(fd);
    if (!entry) {
        return false;
    }
    
    /* Synthetic procfs files are not writable (read-only) */
    /* Synthetic directories are not writable */
    /* /dev/null is always writable (discard writes) */
    /* /dev/zero and /dev/urandom are not writable */
    
    if (get_fd_is_synthetic_proc_file_impl(entry)) {
        put_fd_entry_impl(entry);
        return false;
    }
    
    if (get_fd_is_synthetic_dir_impl(entry)) {
        put_fd_entry_impl(entry);
        return false;
    }
    
    if (get_fd_is_synthetic_dev_impl(entry)) {
        synthetic_dev_node_t dev = get_fd_synthetic_dev_node_impl(entry);
        put_fd_entry_impl(entry);
        /* Only /dev/null accepts writes (they're discarded) */
        return dev == SYNTHETIC_DEV_NULL;
    }
    
    put_fd_entry_impl(entry);
    return false;
}

int poll_impl(struct pollfd *fds, nfds_t nfds, int timeout) {
    if (!fds) {
        errno = EFAULT;
        return -1;
    }
    
    if (nfds == 0) {
        if (timeout == 0) {
            return 0;
        }
        /* For non-zero timeout, we'd need a wait mechanism */
        errno = EINVAL;
        return -1;
    }
    
    int ready_count = 0;
    int host_fds_count = 0;
    struct pollfd *host_fds = NULL;
    int *fd_map = NULL; /* Map host_fds index to original fds index */
    
    if (nfds > 0) {
        host_fds = calloc(nfds, sizeof(struct pollfd));
        fd_map = calloc(nfds, sizeof(int));
        if (!host_fds || !fd_map) {
            free(host_fds);
            free(fd_map);
            errno = ENOMEM;
            return -1;
        }
    }
    
    /* First pass: handle synthetic fds, collect host fds */
    for (nfds_t i = 0; i < nfds; i++) {
        int fd = fds[i].fd;
        short events = fds[i].events;
        short revents = 0;
        
        if (fd < 0 || fd >= NR_OPEN_DEFAULT) {
            fds[i].revents = 0;
            continue;
        }
        
        void *entry = get_fd_entry_impl(fd);
        if (!entry) {
            fds[i].revents = 0;
            continue;
        }
        
        bool is_synthetic = get_fd_is_synthetic_proc_file_impl(entry) ||
                           get_fd_is_synthetic_dir_impl(entry) ||
                           get_fd_is_synthetic_dev_impl(entry);
        put_fd_entry_impl(entry);
        
        if (is_synthetic) {
            /* Handle synthetic fd immediately */
            if (events & POLLIN) {
                if (synthetic_fd_read_ready(fd)) {
                    revents |= POLLIN;
                }
            }
            if (events & POLLOUT) {
                if (synthetic_fd_write_ready(fd)) {
                    revents |= POLLOUT;
                }
            }
            /* Synthetic fds always report POLLHUP if not readable/writable */
            if (revents == 0) {
                revents |= POLLHUP;
            }
            
            fds[i].revents = revents;
            if (revents != 0) {
                ready_count++;
            }
        } else {
            /* Collect host fd for polling */
            host_fds[host_fds_count].fd = fd;
            host_fds[host_fds_count].events = events;
            host_fds[host_fds_count].revents = 0;
            fd_map[host_fds_count] = (int)i;
            host_fds_count++;
        }
    }
    
    /* If we have host fds and no synthetic fds ready, poll host fds */
    if (host_fds_count > 0 && ready_count == 0) {
        int host_ready = poll(host_fds, (nfds_t)host_fds_count, timeout);
        if (host_ready < 0) {
            free(host_fds);
            free(fd_map);
            return -1;
        }
        
        /* Copy host fd results back to original fds */
        for (int i = 0; i < host_ready; i++) {
            int orig_idx = fd_map[i];
            if (orig_idx >= 0 && orig_idx < (int)nfds) {
                fds[orig_idx].revents = host_fds[i].revents;
                ready_count++;
            }
        }
    } else if (host_fds_count > 0 && ready_count > 0) {
        /* Non-blocking poll on host fds */
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 0;
        
        fd_set read_fds;
        fd_set write_fds;
        fd_set error_fds;
        FD_ZERO(&read_fds);
        FD_ZERO(&write_fds);
        FD_ZERO(&error_fds);
        
        int max_fd = -1;
        for (int i = 0; i < host_fds_count; i++) {
            FD_SET(host_fds[i].fd, &read_fds);
            FD_SET(host_fds[i].fd, &write_fds);
            FD_SET(host_fds[i].fd, &error_fds);
            if (host_fds[i].fd > max_fd) {
                max_fd = host_fds[i].fd;
            }
        }
        
        select(max_fd + 1, &read_fds, &write_fds, &error_fds, &tv);
        
        for (int i = 0; i < host_fds_count; i++) {
            int fd = host_fds[i].fd;
            short revents = 0;
            if (FD_ISSET(fd, &read_fds)) {
                revents |= POLLIN;
            }
            if (FD_ISSET(fd, &write_fds)) {
                revents |= POLLOUT;
            }
            if (FD_ISSET(fd, &error_fds)) {
                revents |= POLLERR;
            }
            
            int orig_idx = fd_map[i];
            if (orig_idx >= 0 && orig_idx < (int)nfds) {
                fds[orig_idx].revents = revents;
                if (revents != 0) {
                    ready_count++;
                }
            }
        }
    }
    
    free(host_fds);
    free(fd_map);
    
    return ready_count;
}

int select_impl(int nfds, fd_set *readfds, fd_set *writefds, fd_set *errorfds, struct timeval *timeout) {
    if (nfds <= 0) {
        errno = EINVAL;
        return -1;
    }
    
    fd_set host_readfds;
    fd_set host_writefds;
    fd_set host_errorfds;
    FD_ZERO(&host_readfds);
    FD_ZERO(&host_writefds);
    FD_ZERO(&host_errorfds);
    
    int host_max_fd = -1;
    int synthetic_ready = 0;
    
    /* Process each fd */
    for (int fd = 0; fd < nfds; fd++) {
        bool in_read = readfds && FD_ISSET(fd, readfds);
        bool in_write = writefds && FD_ISSET(fd, writefds);
        bool in_error = errorfds && FD_ISSET(fd, errorfds);
        
        if (!in_read && !in_write && !in_error) {
            continue;
        }
        
        if (fd < 0 || fd >= NR_OPEN_DEFAULT) {
            if (readfds) FD_CLR(fd, readfds);
            if (writefds) FD_CLR(fd, writefds);
            if (errorfds) FD_CLR(fd, errorfds);
            continue;
        }
        
        void *entry = get_fd_entry_impl(fd);
        if (!entry) {
            if (readfds) FD_CLR(fd, readfds);
            if (writefds) FD_CLR(fd, writefds);
            if (errorfds) FD_CLR(fd, errorfds);
            continue;
        }
        
        bool is_synthetic = get_fd_is_synthetic_proc_file_impl(entry) ||
                           get_fd_is_synthetic_dir_impl(entry) ||
                           get_fd_is_synthetic_dev_impl(entry);
        put_fd_entry_impl(entry);
        
        if (is_synthetic) {
            /* Handle synthetic fd immediately */
            bool read_ready = in_read && synthetic_fd_read_ready(fd);
            bool write_ready = in_write && synthetic_fd_write_ready(fd);
            
            if (readfds && !read_ready) FD_CLR(fd, readfds);
            if (writefds && !write_ready) FD_CLR(fd, writefds);
            if (errorfds) FD_CLR(fd, errorfds);
            
            if (read_ready || write_ready) {
                synthetic_ready++;
            }
        } else {
            /* Add to host fd sets */
            if (in_read) FD_SET(fd, &host_readfds);
            if (in_write) FD_SET(fd, &host_writefds);
            if (in_error) FD_SET(fd, &host_errorfds);
            if (fd > host_max_fd) {
                host_max_fd = fd;
            }
        }
    }
    
    /* If we have host fds, poll them */
    int host_ready = 0;
    if (host_max_fd >= 0) {
        struct timeval tv;
        struct timeval *tv_ptr = NULL;
        if (timeout) {
            tv = *timeout;
            tv_ptr = &tv;
        }
        
        host_ready = select(host_max_fd + 1, &host_readfds, &host_writefds, &host_errorfds, tv_ptr);
        if (host_ready < 0) {
            return -1;
        }
    }
    
    /* Copy host fd results back */
    if (readfds) *readfds = host_readfds;
    if (writefds) *writefds = host_writefds;
    if (errorfds) *errorfds = host_errorfds;
    
    /* Clear synthetic fds that aren't ready */
    for (int fd = 0; fd < nfds; fd++) {
        void *entry = get_fd_entry_impl(fd);
        if (!entry) continue;
        bool is_synthetic = get_fd_is_synthetic_proc_file_impl(entry) ||
                           get_fd_is_synthetic_dir_impl(entry) ||
                           get_fd_is_synthetic_dev_impl(entry);
        put_fd_entry_impl(entry);
        
        if (is_synthetic) {
            bool read_ready = synthetic_fd_read_ready(fd);
            bool write_ready = synthetic_fd_write_ready(fd);
            
            if (readfds && !read_ready) FD_CLR(fd, readfds);
            if (writefds && !write_ready) FD_CLR(fd, writefds);
            if (errorfds) FD_CLR(fd, errorfds);
        }
    }
    
    return host_ready + (synthetic_ready > 0 ? 1 : 0);
}

__attribute__((visibility("default"))) int poll(struct pollfd *fds, nfds_t nfds, int timeout) {
    return poll_impl(fds, nfds, timeout);
}

__attribute__((visibility("default"))) int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *errorfds, struct timeval *timeout) {
    return select_impl(nfds, readfds, writefds, errorfds, timeout);
}
