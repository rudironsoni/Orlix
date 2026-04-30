#include <linux/fcntl.h>

#include "pipe.h"

#include <errno.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#include "internal/ios/fs/sync.h"

#define PIPE_BUFFER_SIZE 65536U

struct pipe_object {
    unsigned char buffer[PIPE_BUFFER_SIZE];
    size_t head;
    size_t len;
    int readers;
    int writers;
    int refs;
    unsigned long long id;
    fs_mutex_t lock;
};

struct pipe_endpoint {
    struct pipe_object *pipe;
    bool read_end;
};

static atomic_ullong next_pipe_id = 1;

static size_t pipe_space_locked(const struct pipe_object *pipe) {
    return PIPE_BUFFER_SIZE - pipe->len;
}

static void pipe_free_endpoint(struct pipe_endpoint *endpoint) {
    free(endpoint);
}

int pipe_create_endpoint_pair(struct pipe_endpoint **read_end, struct pipe_endpoint **write_end) {
    struct pipe_object *pipe;
    struct pipe_endpoint *reader;
    struct pipe_endpoint *writer;

    if (!read_end || !write_end) {
        errno = EINVAL;
        return -1;
    }

    pipe = calloc(1, sizeof(*pipe));
    reader = calloc(1, sizeof(*reader));
    writer = calloc(1, sizeof(*writer));
    if (!pipe || !reader || !writer) {
        free(pipe);
        free(reader);
        free(writer);
        errno = ENOMEM;
        return -1;
    }

    pipe->readers = 1;
    pipe->writers = 1;
    pipe->refs = 2;
    pipe->id = atomic_fetch_add(&next_pipe_id, 1);
    fs_mutex_init(&pipe->lock);

    reader->pipe = pipe;
    reader->read_end = true;
    writer->pipe = pipe;
    writer->read_end = false;

    *read_end = reader;
    *write_end = writer;
    return 0;
}

void pipe_close_endpoint_impl(struct pipe_endpoint *endpoint) {
    struct pipe_object *pipe;
    bool should_free_pipe = false;

    if (!endpoint || !endpoint->pipe) {
        return;
    }

    pipe = endpoint->pipe;
    fs_mutex_lock(&pipe->lock);
    if (endpoint->read_end) {
        if (pipe->readers > 0) {
            pipe->readers--;
        }
    } else {
        if (pipe->writers > 0) {
            pipe->writers--;
        }
    }
    pipe->refs--;
    should_free_pipe = pipe->refs == 0;
    fs_mutex_unlock(&pipe->lock);

    pipe_free_endpoint(endpoint);
    if (should_free_pipe) {
        fs_mutex_destroy(&pipe->lock);
        free(pipe);
    }
}

unsigned long long pipe_endpoint_id_impl(struct pipe_endpoint *endpoint) {
    if (!endpoint || !endpoint->pipe) {
        return 0;
    }
    return endpoint->pipe->id;
}

bool pipe_endpoint_is_read_end_impl(struct pipe_endpoint *endpoint) {
    return endpoint && endpoint->read_end;
}

ssize_t pipe_read_endpoint_impl(struct pipe_endpoint *endpoint, void *buf, size_t count, bool nonblock) {
    struct pipe_object *pipe;
    size_t to_read;
    size_t first;

    if (!endpoint || !endpoint->pipe || !endpoint->read_end) {
        errno = EBADF;
        return -1;
    }
    if (!buf && count > 0) {
        errno = EFAULT;
        return -1;
    }
    if (count == 0) {
        return 0;
    }

    pipe = endpoint->pipe;
    fs_mutex_lock(&pipe->lock);
    if (pipe->len == 0) {
        if (pipe->writers == 0) {
            fs_mutex_unlock(&pipe->lock);
            return 0;
        }
        fs_mutex_unlock(&pipe->lock);
        errno = EAGAIN;
        (void)nonblock;
        return -1;
    }

    to_read = count < pipe->len ? count : pipe->len;
    first = to_read;
    if (first > PIPE_BUFFER_SIZE - pipe->head) {
        first = PIPE_BUFFER_SIZE - pipe->head;
    }
    memcpy(buf, pipe->buffer + pipe->head, first);
    if (first < to_read) {
        memcpy((unsigned char *)buf + first, pipe->buffer, to_read - first);
    }
    pipe->head = (pipe->head + to_read) % PIPE_BUFFER_SIZE;
    pipe->len -= to_read;
    fs_mutex_unlock(&pipe->lock);
    return (ssize_t)to_read;
}

ssize_t pipe_write_endpoint_impl(struct pipe_endpoint *endpoint, const void *buf, size_t count, bool nonblock) {
    struct pipe_object *pipe;
    size_t space;
    size_t to_write;
    size_t tail;
    size_t first;

    if (!endpoint || !endpoint->pipe || endpoint->read_end) {
        errno = EBADF;
        return -1;
    }
    if (!buf && count > 0) {
        errno = EFAULT;
        return -1;
    }
    if (count == 0) {
        return 0;
    }

    pipe = endpoint->pipe;
    fs_mutex_lock(&pipe->lock);
    if (pipe->readers == 0) {
        fs_mutex_unlock(&pipe->lock);
        errno = EPIPE;
        return -1;
    }

    space = pipe_space_locked(pipe);
    if (space == 0) {
        fs_mutex_unlock(&pipe->lock);
        errno = EAGAIN;
        (void)nonblock;
        return -1;
    }

    to_write = count < space ? count : space;
    tail = (pipe->head + pipe->len) % PIPE_BUFFER_SIZE;
    first = to_write;
    if (first > PIPE_BUFFER_SIZE - tail) {
        first = PIPE_BUFFER_SIZE - tail;
    }
    memcpy(pipe->buffer + tail, buf, first);
    if (first < to_write) {
        memcpy(pipe->buffer, (const unsigned char *)buf + first, to_write - first);
    }
    pipe->len += to_write;
    fs_mutex_unlock(&pipe->lock);
    return (ssize_t)to_write;
}

short pipe_poll_revents_impl(struct pipe_endpoint *endpoint, short events) {
    struct pipe_object *pipe;
    short revents = 0;

    if (!endpoint || !endpoint->pipe) {
        return POLLNVAL;
    }

    pipe = endpoint->pipe;
    fs_mutex_lock(&pipe->lock);
    if (endpoint->read_end) {
        if ((events & (POLLIN | POLLRDNORM)) && pipe->len > 0) {
            revents |= events & (POLLIN | POLLRDNORM);
        }
        if (pipe->writers == 0) {
            revents |= POLLHUP;
        }
    } else {
        if (pipe->readers == 0) {
            revents |= POLLERR;
        } else if ((events & (POLLOUT | POLLWRNORM)) && pipe_space_locked(pipe) > 0) {
            revents |= events & (POLLOUT | POLLWRNORM);
        }
    }
    fs_mutex_unlock(&pipe->lock);
    return revents;
}

int pipe2_impl(int pipefd[2], int flags) {
    struct pipe_endpoint *read_end = NULL;
    struct pipe_endpoint *write_end = NULL;
    int read_fd;
    int write_fd;
    int supported_flags = O_CLOEXEC | O_NONBLOCK;

    if (!pipefd) {
        errno = EFAULT;
        return -1;
    }
    if (flags & ~supported_flags) {
        errno = EINVAL;
        return -1;
    }
    if (pipe_create_endpoint_pair(&read_end, &write_end) != 0) {
        return -1;
    }

    read_fd = alloc_fd_impl();
    if (read_fd < 0) {
        pipe_close_endpoint_impl(read_end);
        pipe_close_endpoint_impl(write_end);
        return -1;
    }
    if (init_pipe_fd_entry_impl(read_fd, O_RDONLY | (flags & supported_flags), read_end) != 0) {
        free_fd_impl(read_fd);
        pipe_close_endpoint_impl(read_end);
        pipe_close_endpoint_impl(write_end);
        return -1;
    }

    write_fd = alloc_fd_impl();
    if (write_fd < 0) {
        free_fd_impl(read_fd);
        pipe_close_endpoint_impl(write_end);
        return -1;
    }
    if (init_pipe_fd_entry_impl(write_fd, O_WRONLY | (flags & supported_flags), write_end) != 0) {
        free_fd_impl(read_fd);
        free_fd_impl(write_fd);
        pipe_close_endpoint_impl(write_end);
        return -1;
    }

    pipefd[0] = read_fd;
    pipefd[1] = write_fd;
    return 0;
}

int pipe_impl(int pipefd[2]) {
    return pipe2_impl(pipefd, 0);
}

__attribute__((visibility("default"))) int pipe(int pipefd[2]) {
    return pipe_impl(pipefd);
}

__attribute__((visibility("default"))) int pipe2(int pipefd[2], int flags) {
    return pipe2_impl(pipefd, flags);
}
