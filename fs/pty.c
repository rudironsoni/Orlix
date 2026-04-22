#include "pty.h"

#include <errno.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "internal/ios/fs/backing_io.h"

#define PTY_MAX 128
#define PTY_BUFFER_CAPACITY 4096

typedef struct pty_ring_buffer {
    unsigned char data[PTY_BUFFER_CAPACITY];
    size_t head;
    size_t tail;
    size_t len;
} pty_ring_buffer_t;

typedef struct pty_pair {
    bool allocated;
    bool master_open;
    bool slave_open;
    bool slave_locked;
    pty_linux_termios_t termios;
    pty_linux_winsize_t winsize;
    int32_t foreground_pgrp;
    pty_ring_buffer_t master_to_slave;
    pty_ring_buffer_t slave_to_master;
} pty_pair_t;

static pty_pair_t pty_table[PTY_MAX];
static pthread_mutex_t pty_lock = IX_MUTEX_INITIALIZER;
static atomic_uint pty_next_hint = 0;

static void pty_ring_init(pty_ring_buffer_t *ring) {
    ring->head = 0;
    ring->tail = 0;
    ring->len = 0;
}

static size_t pty_ring_write(pty_ring_buffer_t *ring, const unsigned char *src, size_t count) {
    size_t written = 0;
    while (written < count && ring->len < PTY_BUFFER_CAPACITY) {
        ring->data[ring->tail] = src[written];
        ring->tail = (ring->tail + 1) % PTY_BUFFER_CAPACITY;
        ring->len++;
        written++;
    }
    return written;
}

static size_t pty_ring_read(pty_ring_buffer_t *ring, unsigned char *dst, size_t count) {
    size_t read_count = 0;
    while (read_count < count && ring->len > 0) {
        dst[read_count] = ring->data[ring->head];
        ring->head = (ring->head + 1) % PTY_BUFFER_CAPACITY;
        ring->len--;
        read_count++;
    }
    return read_count;
}

static bool pty_valid_index(unsigned int pty_index) {
    return pty_index < PTY_MAX;
}

static void pty_init_defaults(pty_pair_t *pair) {
    memset(&pair->termios, 0, sizeof(pair->termios));
    pair->termios.c_iflag = 0x00000500U;
    pair->termios.c_oflag = 0x00000005U;
    pair->termios.c_cflag = 0x000000BFU;
    pair->termios.c_lflag = 0x00008A3BU;
    pair->termios.c_cc[0] = 3;
    pair->termios.c_cc[1] = 28;
    pair->termios.c_cc[2] = 127;
    pair->termios.c_cc[3] = 21;
    pair->termios.c_cc[4] = 4;
    pair->termios.c_cc[5] = 0;
    pair->termios.c_cc[6] = 1;
    pair->termios.c_cc[8] = 17;
    pair->termios.c_cc[9] = 19;
    pair->termios.c_cc[10] = 26;

    memset(&pair->winsize, 0, sizeof(pair->winsize));
    pair->winsize.ws_row = 24;
    pair->winsize.ws_col = 80;
    pair->foreground_pgrp = 0;

    pty_ring_init(&pair->master_to_slave);
    pty_ring_init(&pair->slave_to_master);
}

int pty_allocate_pair_impl(unsigned int *pty_index) {
    if (!pty_index) {
        errno = EINVAL;
        return -1;
    }

    pthread_mutex_lock(&pty_lock);
    unsigned int hint = atomic_load(&pty_next_hint) % PTY_MAX;
    for (unsigned int i = 0; i < PTY_MAX; i++) {
        unsigned int idx = (hint + i) % PTY_MAX;
        if (pty_table[idx].allocated) {
            continue;
        }

        pty_pair_t *pair = &pty_table[idx];
        memset(pair, 0, sizeof(*pair));
        pair->allocated = true;
        pair->master_open = true;
        pair->slave_open = false;
        pair->slave_locked = true;
        pty_init_defaults(pair);

        atomic_store(&pty_next_hint, idx + 1);
        *pty_index = idx;
        pthread_mutex_unlock(&pty_lock);
        return 0;
    }

    pthread_mutex_unlock(&pty_lock);
    errno = EAGAIN;
    return -1;
}

int pty_format_slave_path_impl(unsigned int pty_index, char *buf, size_t buf_len) {
    if (!buf || buf_len == 0 || !pty_valid_index(pty_index)) {
        errno = EINVAL;
        return -1;
    }

    static const char prefix[] = "/dev/pts/";
    size_t prefix_len = sizeof(prefix) - 1;
    if (buf_len <= prefix_len) {
        errno = ENAMETOOLONG;
        return -1;
    }

    memcpy(buf, prefix, prefix_len);

    char digits[16];
    size_t digit_len = 0;
    unsigned int value = pty_index;
    do {
        digits[digit_len++] = (char)('0' + (value % 10));
        value /= 10;
    } while (value != 0 && digit_len < sizeof(digits));

    if (value != 0 || (prefix_len + digit_len + 1) > buf_len) {
        errno = ENAMETOOLONG;
        return -1;
    }

    for (size_t i = 0; i < digit_len; i++) {
        buf[prefix_len + i] = digits[digit_len - 1 - i];
    }
    buf[prefix_len + digit_len] = '\0';
    return 0;
}

bool pty_is_virtual_slave_path_impl(const char *path) {
    unsigned int idx;
    return pty_open_slave_by_path_impl(path, &idx) == 0;
}

int pty_open_slave_by_path_impl(const char *path, unsigned int *pty_index) {
    if (!path || !pty_index) {
        errno = EINVAL;
        return -1;
    }

    static const char *prefix = "/dev/pts/";
    size_t prefix_len = strlen(prefix);
    if (strncmp(path, prefix, prefix_len) != 0) {
        errno = ENOENT;
        return -1;
    }

    const char *num = path + prefix_len;
    if (*num == '\0') {
        errno = ENOENT;
        return -1;
    }

    char *endptr = NULL;
    unsigned long value = strtoul(num, &endptr, 10);
    if (!endptr || *endptr != '\0' || value >= PTY_MAX) {
        errno = ENOENT;
        return -1;
    }

    unsigned int idx = (unsigned int)value;
    pthread_mutex_lock(&pty_lock);
    pty_pair_t *pair = &pty_table[idx];
    if (!pair->allocated) {
        pthread_mutex_unlock(&pty_lock);
        errno = ENOENT;
        return -1;
    }
    if (pair->slave_locked) {
        pthread_mutex_unlock(&pty_lock);
        errno = EIO;
        return -1;
    }
    pair->slave_open = true;
    *pty_index = idx;
    pthread_mutex_unlock(&pty_lock);
    return 0;
}

int pty_close_end_impl(unsigned int pty_index, bool is_master) {
    if (!pty_valid_index(pty_index)) {
        errno = EINVAL;
        return -1;
    }

    pthread_mutex_lock(&pty_lock);
    pty_pair_t *pair = &pty_table[pty_index];
    if (!pair->allocated) {
        pthread_mutex_unlock(&pty_lock);
        errno = EINVAL;
        return -1;
    }

    if (is_master) {
        pair->master_open = false;
    } else {
        pair->slave_open = false;
    }

    if (!pair->master_open && !pair->slave_open) {
        memset(pair, 0, sizeof(*pair));
    }

    pthread_mutex_unlock(&pty_lock);
    return 0;
}

static ssize_t pty_read_from_ring(pty_ring_buffer_t *ring, bool peer_open, void *buf, size_t count,
                                  bool nonblock) {
    if (count == 0) {
        return 0;
    }
    if (ring->len == 0) {
        if (!peer_open) {
            return 0;
        }
        if (nonblock) {
            errno = EAGAIN;
            return -1;
        }
        errno = EAGAIN;
        return -1;
    }

    return (ssize_t)pty_ring_read(ring, (unsigned char *)buf, count);
}

static ssize_t pty_write_to_ring(pty_ring_buffer_t *ring, bool peer_open, const void *buf, size_t count,
                                 bool nonblock) {
    if (count == 0) {
        return 0;
    }
    if (!peer_open) {
        errno = EIO;
        return -1;
    }

    size_t written = pty_ring_write(ring, (const unsigned char *)buf, count);
    if (written == 0) {
        errno = nonblock ? EAGAIN : EAGAIN;
        return -1;
    }

    return (ssize_t)written;
}

ssize_t pty_read_master_impl(unsigned int pty_index, void *buf, size_t count, bool nonblock) {
    if (!buf || !pty_valid_index(pty_index)) {
        errno = EINVAL;
        return -1;
    }

    pthread_mutex_lock(&pty_lock);
    pty_pair_t *pair = &pty_table[pty_index];
    if (!pair->allocated || !pair->master_open) {
        pthread_mutex_unlock(&pty_lock);
        errno = EIO;
        return -1;
    }

    ssize_t ret = pty_read_from_ring(&pair->slave_to_master, pair->slave_open, buf, count, nonblock);
    pthread_mutex_unlock(&pty_lock);
    return ret;
}

ssize_t pty_write_master_impl(unsigned int pty_index, const void *buf, size_t count, bool nonblock) {
    if (!buf || !pty_valid_index(pty_index)) {
        errno = EINVAL;
        return -1;
    }

    pthread_mutex_lock(&pty_lock);
    pty_pair_t *pair = &pty_table[pty_index];
    if (!pair->allocated || !pair->master_open) {
        pthread_mutex_unlock(&pty_lock);
        errno = EIO;
        return -1;
    }

    ssize_t ret = pty_write_to_ring(&pair->master_to_slave, pair->slave_open, buf, count, nonblock);
    pthread_mutex_unlock(&pty_lock);
    return ret;
}

ssize_t pty_read_slave_impl(unsigned int pty_index, void *buf, size_t count, bool nonblock) {
    if (!buf || !pty_valid_index(pty_index)) {
        errno = EINVAL;
        return -1;
    }

    pthread_mutex_lock(&pty_lock);
    pty_pair_t *pair = &pty_table[pty_index];
    if (!pair->allocated || !pair->slave_open) {
        pthread_mutex_unlock(&pty_lock);
        errno = EIO;
        return -1;
    }

    ssize_t ret = pty_read_from_ring(&pair->master_to_slave, pair->master_open, buf, count, nonblock);
    pthread_mutex_unlock(&pty_lock);
    return ret;
}

ssize_t pty_write_slave_impl(unsigned int pty_index, const void *buf, size_t count, bool nonblock) {
    if (!buf || !pty_valid_index(pty_index)) {
        errno = EINVAL;
        return -1;
    }

    pthread_mutex_lock(&pty_lock);
    pty_pair_t *pair = &pty_table[pty_index];
    if (!pair->allocated || !pair->slave_open) {
        pthread_mutex_unlock(&pty_lock);
        errno = EIO;
        return -1;
    }

    ssize_t ret = pty_write_to_ring(&pair->slave_to_master, pair->master_open, buf, count, nonblock);
    pthread_mutex_unlock(&pty_lock);
    return ret;
}

int pty_get_readable_bytes_impl(unsigned int pty_index, bool is_master, int *bytes) {
    if (!bytes || !pty_valid_index(pty_index)) {
        errno = EINVAL;
        return -1;
    }

    pthread_mutex_lock(&pty_lock);
    pty_pair_t *pair = &pty_table[pty_index];
    if (!pair->allocated) {
        pthread_mutex_unlock(&pty_lock);
        errno = EINVAL;
        return -1;
    }

    pty_ring_buffer_t *read_ring = is_master ? &pair->slave_to_master : &pair->master_to_slave;
    *bytes = (int)read_ring->len;
    pthread_mutex_unlock(&pty_lock);
    return 0;
}

short pty_poll_revents_impl(unsigned int pty_index, bool is_master, short events) {
    if (!pty_valid_index(pty_index)) {
        return POLLNVAL;
    }

    pthread_mutex_lock(&pty_lock);
    pty_pair_t *pair = &pty_table[pty_index];
    if (!pair->allocated) {
        pthread_mutex_unlock(&pty_lock);
        return POLLNVAL;
    }

    pty_ring_buffer_t *read_ring = is_master ? &pair->slave_to_master : &pair->master_to_slave;
    pty_ring_buffer_t *write_ring = is_master ? &pair->master_to_slave : &pair->slave_to_master;
    bool this_open = is_master ? pair->master_open : pair->slave_open;
    bool peer_open = is_master ? pair->slave_open : pair->master_open;

    short revents = 0;
    if (!this_open) {
        revents |= POLLNVAL;
        pthread_mutex_unlock(&pty_lock);
        return revents;
    }

    if (events & (POLLIN | POLLRDNORM)) {
        if (read_ring->len > 0 || !peer_open) {
            revents |= (events & (POLLIN | POLLRDNORM));
        }
    }

    if (events & (POLLOUT | POLLWRNORM)) {
        if (peer_open && write_ring->len < PTY_BUFFER_CAPACITY) {
            revents |= (events & (POLLOUT | POLLWRNORM));
        }
    }

    if (!peer_open) {
        revents |= POLLHUP;
    }

    pthread_mutex_unlock(&pty_lock);
    return revents;
}

int pty_set_lock_impl(unsigned int pty_index, bool locked) {
    if (!pty_valid_index(pty_index)) {
        errno = EINVAL;
        return -1;
    }

    pthread_mutex_lock(&pty_lock);
    pty_pair_t *pair = &pty_table[pty_index];
    if (!pair->allocated) {
        pthread_mutex_unlock(&pty_lock);
        errno = EINVAL;
        return -1;
    }
    pair->slave_locked = locked;
    pthread_mutex_unlock(&pty_lock);
    return 0;
}

int pty_get_lock_impl(unsigned int pty_index, int *locked) {
    if (!locked || !pty_valid_index(pty_index)) {
        errno = EINVAL;
        return -1;
    }

    pthread_mutex_lock(&pty_lock);
    pty_pair_t *pair = &pty_table[pty_index];
    if (!pair->allocated) {
        pthread_mutex_unlock(&pty_lock);
        errno = EINVAL;
        return -1;
    }
    *locked = pair->slave_locked ? 1 : 0;
    pthread_mutex_unlock(&pty_lock);
    return 0;
}

int pty_get_termios_impl(unsigned int pty_index, pty_linux_termios_t *termios) {
    if (!termios || !pty_valid_index(pty_index)) {
        errno = EINVAL;
        return -1;
    }

    pthread_mutex_lock(&pty_lock);
    pty_pair_t *pair = &pty_table[pty_index];
    if (!pair->allocated) {
        pthread_mutex_unlock(&pty_lock);
        errno = EINVAL;
        return -1;
    }
    *termios = pair->termios;
    pthread_mutex_unlock(&pty_lock);
    return 0;
}

int pty_set_termios_impl(unsigned int pty_index, const pty_linux_termios_t *termios) {
    if (!termios || !pty_valid_index(pty_index)) {
        errno = EINVAL;
        return -1;
    }

    pthread_mutex_lock(&pty_lock);
    pty_pair_t *pair = &pty_table[pty_index];
    if (!pair->allocated) {
        pthread_mutex_unlock(&pty_lock);
        errno = EINVAL;
        return -1;
    }
    pair->termios = *termios;
    pthread_mutex_unlock(&pty_lock);
    return 0;
}

int pty_get_winsize_impl(unsigned int pty_index, pty_linux_winsize_t *winsize) {
    if (!winsize || !pty_valid_index(pty_index)) {
        errno = EINVAL;
        return -1;
    }

    pthread_mutex_lock(&pty_lock);
    pty_pair_t *pair = &pty_table[pty_index];
    if (!pair->allocated) {
        pthread_mutex_unlock(&pty_lock);
        errno = EINVAL;
        return -1;
    }
    *winsize = pair->winsize;
    pthread_mutex_unlock(&pty_lock);
    return 0;
}

int pty_set_winsize_impl(unsigned int pty_index, const pty_linux_winsize_t *winsize) {
    if (!winsize || !pty_valid_index(pty_index)) {
        errno = EINVAL;
        return -1;
    }

    pthread_mutex_lock(&pty_lock);
    pty_pair_t *pair = &pty_table[pty_index];
    if (!pair->allocated) {
        pthread_mutex_unlock(&pty_lock);
        errno = EINVAL;
        return -1;
    }
    pair->winsize = *winsize;
    pthread_mutex_unlock(&pty_lock);
    return 0;
}

int pty_get_foreground_pgrp_impl(unsigned int pty_index, int32_t *pgrp) {
    if (!pgrp || !pty_valid_index(pty_index)) {
        errno = EINVAL;
        return -1;
    }

    pthread_mutex_lock(&pty_lock);
    pty_pair_t *pair = &pty_table[pty_index];
    if (!pair->allocated) {
        pthread_mutex_unlock(&pty_lock);
        errno = EINVAL;
        return -1;
    }
    *pgrp = pair->foreground_pgrp;
    pthread_mutex_unlock(&pty_lock);
    return 0;
}

int pty_set_foreground_pgrp_impl(unsigned int pty_index, int32_t pgrp) {
    if (!pty_valid_index(pty_index)) {
        errno = EINVAL;
        return -1;
    }

    pthread_mutex_lock(&pty_lock);
    pty_pair_t *pair = &pty_table[pty_index];
    if (!pair->allocated) {
        pthread_mutex_unlock(&pty_lock);
        errno = EINVAL;
        return -1;
    }
    pair->foreground_pgrp = pgrp;
    pthread_mutex_unlock(&pty_lock);
    return 0;
}
