#include "pty.h"

#include <errno.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../kernel/signal.h"
#include "../kernel/task.h"
#include "internal/ios/fs/sync.h"

#define PTY_MAX 128
#define PTY_BUFFER_CAPACITY 4096
#define PTY_SIGWINCH 28
#define PTY_SIGINT 2
#define PTY_SIGQUIT 3
#define PTY_SIGTSTP 20
#define PTY_SIGTTIN 21
#define PTY_SIGTTOU 22

#define PTY_LFLAG_ISIG 0x00000001U
#define PTY_LFLAG_ICANON 0x00000002U
#define PTY_LFLAG_ECHO 0x00000008U

#define PTY_CC_VINTR 0
#define PTY_CC_VQUIT 1
#define PTY_CC_VERASE 2
#define PTY_CC_VKILL 3
#define PTY_CC_VEOF 4
#define PTY_CC_VTIME 5
#define PTY_CC_VMIN 6
#define PTY_CC_VSUSP 10

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
    bool has_controlling_session;
    int32_t controlling_sid;
    pty_linux_termios_t termios;
    pty_linux_winsize_t winsize;
    int32_t foreground_pgrp;
    pty_ring_buffer_t master_to_slave;
    pty_ring_buffer_t slave_to_master;
    unsigned char canonical_pending[PTY_BUFFER_CAPACITY];
    size_t canonical_pending_len;
} pty_pair_t;

static pty_pair_t pty_table[PTY_MAX];
static fs_mutex_t pty_lock = FS_MUTEX_INITIALIZER;
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

static void pty_ring_clear(pty_ring_buffer_t *ring) {
    ring->head = 0;
    ring->tail = 0;
    ring->len = 0;
}

static bool pty_termios_has_flag(const pty_linux_termios_t *termios, uint32_t flag) {
    return (termios->c_lflag & flag) != 0;
}

static void pty_emit_echo_byte_impl(pty_pair_t *pair, unsigned char byte) {
    if (!pty_termios_has_flag(&pair->termios, PTY_LFLAG_ECHO)) {
        return;
    }
    (void)pty_ring_write(&pair->slave_to_master, &byte, 1);
}

static void pty_emit_echo_erase_impl(pty_pair_t *pair) {
    if (!pty_termios_has_flag(&pair->termios, PTY_LFLAG_ECHO)) {
        return;
    }
    static const unsigned char seq[3] = {'\b', ' ', '\b'};
    (void)pty_ring_write(&pair->slave_to_master, seq, sizeof(seq));
}

static bool pty_flush_canonical_pending_impl(pty_pair_t *pair) {
    if (pair->canonical_pending_len == 0) {
        return true;
    }
    size_t written = pty_ring_write(&pair->master_to_slave, pair->canonical_pending, pair->canonical_pending_len);
    if (written != pair->canonical_pending_len) {
        if (written > 0) {
            memmove(pair->canonical_pending, pair->canonical_pending + written, pair->canonical_pending_len - written);
            pair->canonical_pending_len -= written;
        }
        return false;
    }
    pair->canonical_pending_len = 0;
    return true;
}

static bool pty_deliver_signal_char_impl(pty_pair_t *pair, unsigned char byte) {
    if (!pty_termios_has_flag(&pair->termios, PTY_LFLAG_ISIG)) {
        return false;
    }

    int signal_number = 0;
    if (byte == pair->termios.c_cc[PTY_CC_VINTR]) {
        signal_number = PTY_SIGINT;
    } else if (byte == pair->termios.c_cc[PTY_CC_VQUIT]) {
        signal_number = PTY_SIGQUIT;
    } else if (byte == pair->termios.c_cc[PTY_CC_VSUSP]) {
        signal_number = PTY_SIGTSTP;
    }

    if (signal_number == 0) {
        return false;
    }

    if (pair->foreground_pgrp > 0) {
        signal_generate_pgrp(pair->foreground_pgrp, signal_number);
    }
    return true;
}

static bool pty_accept_canonical_byte_impl(pty_pair_t *pair, unsigned char byte) {
    if (pty_deliver_signal_char_impl(pair, byte)) {
        return true;
    }

    if (byte == pair->termios.c_cc[PTY_CC_VERASE]) {
        if (pair->canonical_pending_len > 0) {
            pair->canonical_pending_len--;
            pty_emit_echo_erase_impl(pair);
        }
        return true;
    }

    if (byte == pair->termios.c_cc[PTY_CC_VKILL]) {
        if (pair->canonical_pending_len > 0) {
            pair->canonical_pending_len = 0;
            pty_emit_echo_byte_impl(pair, '\n');
        }
        return true;
    }

    if (byte == pair->termios.c_cc[PTY_CC_VEOF]) {
        return pty_flush_canonical_pending_impl(pair);
    }

    if (pair->canonical_pending_len >= PTY_BUFFER_CAPACITY) {
        return false;
    }

    pair->canonical_pending[pair->canonical_pending_len++] = byte;
    pty_emit_echo_byte_impl(pair, byte);

    if (byte == '\n') {
        return pty_flush_canonical_pending_impl(pair);
    }

    return true;
}

static bool pty_accept_noncanonical_byte_impl(pty_pair_t *pair, unsigned char byte) {
    if (pty_deliver_signal_char_impl(pair, byte)) {
        return true;
    }

    pty_emit_echo_byte_impl(pair, byte);
    return pty_ring_write(&pair->master_to_slave, &byte, 1) == 1;
}

static ssize_t pty_write_master_linedisc_impl(pty_pair_t *pair, const void *buf, size_t count) {
    const unsigned char *src = (const unsigned char *)buf;
    size_t accepted = 0;

    for (size_t i = 0; i < count; i++) {
        bool ok;
        if (pty_termios_has_flag(&pair->termios, PTY_LFLAG_ICANON)) {
            ok = pty_accept_canonical_byte_impl(pair, src[i]);
        } else {
            ok = pty_accept_noncanonical_byte_impl(pair, src[i]);
        }

        if (!ok) {
            break;
        }
        accepted++;
    }

    if (accepted == 0) {
        errno = EAGAIN;
        return -1;
    }
    return (ssize_t)accepted;
}

static bool pty_valid_index(unsigned int pty_index) {
    return pty_index < PTY_MAX;
}

static bool pty_signal_is_ignored(const struct task_struct *task, int signal_number) {
    if (!task || !task->signal || signal_number <= 0 || signal_number >= KERNEL_SIG_NUM) {
        return false;
    }
    return task->signal->actions[signal_number].handler == (sighandler_t)1;
}

static bool pty_is_orphaned_pgrp(int32_t sid, int32_t pgid) {
    bool has_member = false;
    bool orphaned = true;

    kernel_mutex_lock(&task_table_lock);
    for (int i = 0; i < TASK_MAX_TASKS; i++) {
        struct task_struct *candidate = task_table[i];
        while (candidate) {
            if (candidate->sid == sid && candidate->pgid == pgid) {
                has_member = true;
                struct task_struct *parent = candidate->parent;
                if (parent && parent->sid == sid && parent->pgid != pgid) {
                    orphaned = false;
                    break;
                }
            }
            candidate = candidate->hash_next;
        }
        if (!orphaned) {
            break;
        }
    }
    kernel_mutex_unlock(&task_table_lock);

    return has_member && orphaned;
}

static int pty_check_background_read_access(pty_pair_t *pair) {
    struct task_struct *task = get_current();
    if (!task || !task->signal || !pair->has_controlling_session || pair->controlling_sid != task->sid) {
        return 0;
    }

    int32_t fg_pgrp = pair->foreground_pgrp;
    if (fg_pgrp <= 0 || fg_pgrp == task->pgid) {
        return 0;
    }

    if (pty_is_orphaned_pgrp(task->sid, task->pgid)) {
        errno = EIO;
        return -1;
    }

    if (signal_is_blocked(task, PTY_SIGTTIN) || pty_signal_is_ignored(task, PTY_SIGTTIN)) {
        errno = EIO;
        return -1;
    }

    (void)signal_generate_pgrp(task->pgid, PTY_SIGTTIN);
    errno = EINTR;
    return -1;
}

static int pty_check_background_write_access(pty_pair_t *pair, bool enforce_background_stop, bool blocked_or_ignored_is_error) {
    struct task_struct *task = get_current();
    if (!task || !task->signal || !pair->has_controlling_session || pair->controlling_sid != task->sid) {
        return 0;
    }

    int32_t fg_pgrp = pair->foreground_pgrp;
    if (fg_pgrp <= 0 || fg_pgrp == task->pgid) {
        return 0;
    }

    if (!enforce_background_stop) {
        return 0;
    }

    if (pty_is_orphaned_pgrp(task->sid, task->pgid)) {
        errno = EIO;
        return -1;
    }

    bool blocked_or_ignored = signal_is_blocked(task, PTY_SIGTTOU) || pty_signal_is_ignored(task, PTY_SIGTTOU);
    if (blocked_or_ignored) {
        if (blocked_or_ignored_is_error) {
            errno = EIO;
            return -1;
        }
        return 0;
    }

    (void)signal_generate_pgrp(task->pgid, PTY_SIGTTOU);
    errno = EINTR;
    return -1;
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

    fs_mutex_lock(&pty_lock);
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
        fs_mutex_unlock(&pty_lock);
        return 0;
    }

    fs_mutex_unlock(&pty_lock);
    errno = EAGAIN;
    return -1;
}

int pty_format_slave_path_impl(unsigned int pty_index, char *buf, size_t buf_len) {
    if (!buf || buf_len == 0 || !pty_valid_index(pty_index)) {
        errno = EINVAL;
        return -1;
    }

    static const char prefix[] = "/dev/pts/";
    size_t prefklen = sizeof(prefix) - 1;
    if (buf_len <= prefklen) {
        errno = ENAMETOOLONG;
        return -1;
    }

    memcpy(buf, prefix, prefklen);

    char digits[16];
    size_t digit_len = 0;
    unsigned int value = pty_index;
    do {
        digits[digit_len++] = (char)('0' + (value % 10));
        value /= 10;
    } while (value != 0 && digit_len < sizeof(digits));

    if (value != 0 || (prefklen + digit_len + 1) > buf_len) {
        errno = ENAMETOOLONG;
        return -1;
    }

    for (size_t i = 0; i < digit_len; i++) {
        buf[prefklen + i] = digits[digit_len - 1 - i];
    }
    buf[prefklen + digit_len] = '\0';
    return 0;
}

bool pty_is_virtual_slave_path_impl(const char *path) {
    unsigned int idx;
    return pty_open_slave_by_path_impl(path, &idx) == 0;
}

int pty_open_controlling_slave_impl(unsigned int *pty_index) {
    if (!pty_index) {
        errno = EINVAL;
        return -1;
    }

    struct task_struct *task = get_current();
    if (!task) {
        errno = ESRCH;
        return -1;
    }

    fs_mutex_lock(&task->lock);
    struct tty_struct *tty = task->tty;
    if (!tty) {
        fs_mutex_unlock(&task->lock);
        errno = ENXIO;
        return -1;
    }
    unsigned int idx = (unsigned int)tty->index;
    fs_mutex_unlock(&task->lock);

    if (!pty_valid_index(idx)) {
        errno = ENXIO;
        return -1;
    }

    fs_mutex_lock(&pty_lock);
    pty_pair_t *pair = &pty_table[idx];
    if (!pair->allocated || !pair->slave_open) {
        fs_mutex_unlock(&pty_lock);
        errno = EIO;
        return -1;
    }

    *pty_index = idx;
    fs_mutex_unlock(&pty_lock);
    return 0;
}

int pty_open_slave_by_path_impl(const char *path, unsigned int *pty_index) {
    if (!path || !pty_index) {
        errno = EINVAL;
        return -1;
    }

    static const char *prefix = "/dev/pts/";
    size_t prefklen = strlen(prefix);
    if (strncmp(path, prefix, prefklen) != 0) {
        errno = ENOENT;
        return -1;
    }

    const char *num = path + prefklen;
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
    fs_mutex_lock(&pty_lock);
    pty_pair_t *pair = &pty_table[idx];
    if (!pair->allocated) {
        fs_mutex_unlock(&pty_lock);
        errno = ENOENT;
        return -1;
    }
    if (pair->slave_locked) {
        fs_mutex_unlock(&pty_lock);
        errno = EIO;
        return -1;
    }
    pair->slave_open = true;
    *pty_index = idx;
    fs_mutex_unlock(&pty_lock);
    return 0;
}

int pty_close_end_impl(unsigned int pty_index, bool is_master) {
    if (!pty_valid_index(pty_index)) {
        errno = EINVAL;
        return -1;
    }

    fs_mutex_lock(&pty_lock);
    pty_pair_t *pair = &pty_table[pty_index];
    if (!pair->allocated) {
        fs_mutex_unlock(&pty_lock);
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

    fs_mutex_unlock(&pty_lock);
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

    fs_mutex_lock(&pty_lock);
    pty_pair_t *pair = &pty_table[pty_index];
    if (!pair->allocated || !pair->master_open) {
        fs_mutex_unlock(&pty_lock);
        errno = EIO;
        return -1;
    }

    ssize_t ret = pty_read_from_ring(&pair->slave_to_master, pair->slave_open, buf, count, nonblock);
    fs_mutex_unlock(&pty_lock);
    return ret;
}

ssize_t pty_write_master_impl(unsigned int pty_index, const void *buf, size_t count, bool nonblock) {
    if (!buf || !pty_valid_index(pty_index)) {
        errno = EINVAL;
        return -1;
    }

    fs_mutex_lock(&pty_lock);
    pty_pair_t *pair = &pty_table[pty_index];
    if (!pair->allocated || !pair->master_open) {
        fs_mutex_unlock(&pty_lock);
        errno = EIO;
        return -1;
    }

    if (!pair->slave_open) {
        fs_mutex_unlock(&pty_lock);
        errno = EIO;
        return -1;
    }

    ssize_t ret = pty_write_master_linedisc_impl(pair, buf, count);
    if (ret < 0 && !nonblock && errno == EAGAIN) {
        errno = EAGAIN;
    }
    fs_mutex_unlock(&pty_lock);
    return ret;
}

ssize_t pty_read_slave_impl(unsigned int pty_index, void *buf, size_t count, bool nonblock) {
    if (!buf || !pty_valid_index(pty_index)) {
        errno = EINVAL;
        return -1;
    }

    fs_mutex_lock(&pty_lock);
    pty_pair_t *pair = &pty_table[pty_index];
    if (!pair->allocated || !pair->slave_open) {
        fs_mutex_unlock(&pty_lock);
        errno = EIO;
        return -1;
    }

    int access_result = pty_check_background_read_access(pair);
    if (access_result != 0) {
        fs_mutex_unlock(&pty_lock);
        return -1;
    }

    if (!pty_termios_has_flag(&pair->termios, PTY_LFLAG_ICANON)) {
        unsigned char vmin = pair->termios.c_cc[PTY_CC_VMIN];
        unsigned char vtime = pair->termios.c_cc[PTY_CC_VTIME];
        size_t available = pair->master_to_slave.len;

        if (vmin == 0) {
            if (available == 0) {
                if (vtime == 0) {
                    fs_mutex_unlock(&pty_lock);
                    return 0;
                }
                fs_mutex_unlock(&pty_lock);
                errno = EAGAIN;
                return -1;
            }
        } else if (available < vmin) {
            fs_mutex_unlock(&pty_lock);
            errno = EAGAIN;
            return -1;
        }
    }

    ssize_t ret = pty_read_from_ring(&pair->master_to_slave, pair->master_open, buf, count, nonblock);
    fs_mutex_unlock(&pty_lock);
    return ret;
}

ssize_t pty_write_slave_impl(unsigned int pty_index, const void *buf, size_t count, bool nonblock) {
    if (!buf || !pty_valid_index(pty_index)) {
        errno = EINVAL;
        return -1;
    }

    fs_mutex_lock(&pty_lock);
    pty_pair_t *pair = &pty_table[pty_index];
    if (!pair->allocated || !pair->slave_open) {
        fs_mutex_unlock(&pty_lock);
        errno = EIO;
        return -1;
    }

    bool tostop = (pair->termios.c_lflag & PTY_LFLAG_TOSTOP) != 0;
    int access_result = pty_check_background_write_access(pair, tostop, false);
    if (access_result != 0) {
        fs_mutex_unlock(&pty_lock);
        return -1;
    }

    ssize_t ret = pty_write_to_ring(&pair->slave_to_master, pair->master_open, buf, count, nonblock);
    fs_mutex_unlock(&pty_lock);
    return ret;
}

int pty_get_readable_bytes_impl(unsigned int pty_index, bool is_master, int *bytes) {
    if (!bytes || !pty_valid_index(pty_index)) {
        errno = EINVAL;
        return -1;
    }

    fs_mutex_lock(&pty_lock);
    pty_pair_t *pair = &pty_table[pty_index];
    if (!pair->allocated) {
        fs_mutex_unlock(&pty_lock);
        errno = EINVAL;
        return -1;
    }

    pty_ring_buffer_t *read_ring = is_master ? &pair->slave_to_master : &pair->master_to_slave;
    *bytes = (int)read_ring->len;
    fs_mutex_unlock(&pty_lock);
    return 0;
}

short pty_poll_revents_impl(unsigned int pty_index, bool is_master, short events) {
    if (!pty_valid_index(pty_index)) {
        return POLLNVAL;
    }

    fs_mutex_lock(&pty_lock);
    pty_pair_t *pair = &pty_table[pty_index];
    if (!pair->allocated) {
        fs_mutex_unlock(&pty_lock);
        return POLLNVAL;
    }

    pty_ring_buffer_t *read_ring = is_master ? &pair->slave_to_master : &pair->master_to_slave;
    pty_ring_buffer_t *write_ring = is_master ? &pair->master_to_slave : &pair->slave_to_master;
    bool this_open = is_master ? pair->master_open : pair->slave_open;
    bool peer_open = is_master ? pair->slave_open : pair->master_open;

    short revents = 0;
    if (!this_open) {
        revents |= POLLNVAL;
        fs_mutex_unlock(&pty_lock);
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

    fs_mutex_unlock(&pty_lock);
    return revents;
}

int pty_set_lock_impl(unsigned int pty_index, bool locked) {
    if (!pty_valid_index(pty_index)) {
        errno = EINVAL;
        return -1;
    }

    fs_mutex_lock(&pty_lock);
    pty_pair_t *pair = &pty_table[pty_index];
    if (!pair->allocated) {
        fs_mutex_unlock(&pty_lock);
        errno = EINVAL;
        return -1;
    }
    pair->slave_locked = locked;
    fs_mutex_unlock(&pty_lock);
    return 0;
}

int pty_get_lock_impl(unsigned int pty_index, int *locked) {
    if (!locked || !pty_valid_index(pty_index)) {
        errno = EINVAL;
        return -1;
    }

    fs_mutex_lock(&pty_lock);
    pty_pair_t *pair = &pty_table[pty_index];
    if (!pair->allocated) {
        fs_mutex_unlock(&pty_lock);
        errno = EINVAL;
        return -1;
    }
    *locked = pair->slave_locked ? 1 : 0;
    fs_mutex_unlock(&pty_lock);
    return 0;
}

int pty_get_termios_impl(unsigned int pty_index, pty_linux_termios_t *termios) {
    if (!termios || !pty_valid_index(pty_index)) {
        errno = EINVAL;
        return -1;
    }

    fs_mutex_lock(&pty_lock);
    pty_pair_t *pair = &pty_table[pty_index];
    if (!pair->allocated) {
        fs_mutex_unlock(&pty_lock);
        errno = EINVAL;
        return -1;
    }
    *termios = pair->termios;
    fs_mutex_unlock(&pty_lock);
    return 0;
}

int pty_set_termios_with_action_impl(unsigned int pty_index, const pty_linux_termios_t *termios, int action) {
    if (!termios || !pty_valid_index(pty_index)) {
        errno = EINVAL;
        return -1;
    }
    if (action != PTY_TCSET_ACTION_NOW && action != PTY_TCSET_ACTION_DRAIN && action != PTY_TCSET_ACTION_FLUSH) {
        errno = EINVAL;
        return -1;
    }

    fs_mutex_lock(&pty_lock);
    pty_pair_t *pair = &pty_table[pty_index];
    if (!pair->allocated) {
        fs_mutex_unlock(&pty_lock);
        errno = EINVAL;
        return -1;
    }

    int access_result = pty_check_background_write_access(pair, true, false);
    if (access_result != 0) {
        fs_mutex_unlock(&pty_lock);
        return -1;
    }

    if (action == PTY_TCSET_ACTION_FLUSH) {
        pty_ring_clear(&pair->master_to_slave);
        pair->canonical_pending_len = 0;
    }

    pair->termios = *termios;
    fs_mutex_unlock(&pty_lock);
    return 0;
}

int pty_set_termios_impl(unsigned int pty_index, const pty_linux_termios_t *termios) {
    return pty_set_termios_with_action_impl(pty_index, termios, PTY_TCSET_ACTION_NOW);
}

int pty_get_winsize_impl(unsigned int pty_index, pty_linux_winsize_t *winsize) {
    if (!winsize || !pty_valid_index(pty_index)) {
        errno = EINVAL;
        return -1;
    }

    fs_mutex_lock(&pty_lock);
    pty_pair_t *pair = &pty_table[pty_index];
    if (!pair->allocated) {
        fs_mutex_unlock(&pty_lock);
        errno = EINVAL;
        return -1;
    }
    *winsize = pair->winsize;
    fs_mutex_unlock(&pty_lock);
    return 0;
}

int pty_set_winsize_impl(unsigned int pty_index, const pty_linux_winsize_t *winsize) {
    if (!winsize || !pty_valid_index(pty_index)) {
        errno = EINVAL;
        return -1;
    }

    int32_t foreground_pgrp = 0;
    int changed = 0;

    fs_mutex_lock(&pty_lock);
    pty_pair_t *pair = &pty_table[pty_index];
    if (!pair->allocated) {
        fs_mutex_unlock(&pty_lock);
        errno = EINVAL;
        return -1;
    }

    if (memcmp(&pair->winsize, winsize, sizeof(*winsize)) != 0) {
        changed = 1;
    }
    pair->winsize = *winsize;
    foreground_pgrp = pair->foreground_pgrp;
    fs_mutex_unlock(&pty_lock);

    if (changed && foreground_pgrp > 0) {
        signal_generate_pgrp(foreground_pgrp, PTY_SIGWINCH);
    }

    return 0;
}

int pty_set_controlling_tty_impl(unsigned int pty_index, int arg) {
    if (!pty_valid_index(pty_index)) {
        errno = EINVAL;
        return -1;
    }

    struct task_struct *task = get_current();
    if (!task) {
        errno = ESRCH;
        return -1;
    }

    if (arg != 0) {
        errno = EPERM;
        return -1;
    }

    fs_mutex_lock(&task->lock);

    if (task->sid <= 0 || task->pid != task->sid) {
        fs_mutex_unlock(&task->lock);
        errno = EPERM;
        return -1;
    }

    if (task->tty && task->tty->index == (int)pty_index) {
        fs_mutex_unlock(&task->lock);
        return 0;
    }

    if (task->tty) {
        fs_mutex_unlock(&task->lock);
        errno = EPERM;
        return -1;
    }

    struct tty_struct *tty = calloc(1, sizeof(*tty));
    if (!tty) {
        fs_mutex_unlock(&task->lock);
        errno = ENOMEM;
        return -1;
    }

    fs_mutex_lock(&pty_lock);
    pty_pair_t *pair = &pty_table[pty_index];
    if (!pair->allocated) {
        fs_mutex_unlock(&pty_lock);
        fs_mutex_unlock(&task->lock);
        free(tty);
        errno = EINVAL;
        return -1;
    }

    if (pair->has_controlling_session && pair->controlling_sid != task->sid) {
        fs_mutex_unlock(&pty_lock);
        fs_mutex_unlock(&task->lock);
        free(tty);
        errno = EPERM;
        return -1;
    }

    pair->has_controlling_session = true;
    pair->controlling_sid = task->sid;
    pair->foreground_pgrp = task->pgid;
    fs_mutex_unlock(&pty_lock);

    tty->index = (int)pty_index;
    tty->foreground_pgrp = task->pgid;
    atomic_init(&tty->refs, 1);
    task->tty = tty;

    fs_mutex_unlock(&task->lock);
    return 0;
}

int pty_get_foreground_pgrp_impl(unsigned int pty_index, int32_t *pgrp) {
    if (!pgrp || !pty_valid_index(pty_index)) {
        errno = EINVAL;
        return -1;
    }

    struct task_struct *task = get_current();
    if (!task) {
        errno = ESRCH;
        return -1;
    }

    fs_mutex_lock(&pty_lock);
    pty_pair_t *pair = &pty_table[pty_index];
    if (!pair->allocated) {
        fs_mutex_unlock(&pty_lock);
        errno = EINVAL;
        return -1;
    }

    if (!pair->has_controlling_session || pair->controlling_sid != task->sid) {
        fs_mutex_unlock(&pty_lock);
        errno = ENOTTY;
        return -1;
    }

    *pgrp = pair->foreground_pgrp;
    fs_mutex_unlock(&pty_lock);
    return 0;
}

int pty_set_foreground_pgrp_impl(unsigned int pty_index, int32_t pgrp) {
  if (!pty_valid_index(pty_index) || pgrp <= 0) {
    errno = EINVAL;
    return -1;
  }

  struct task_struct *task = get_current();
  if (!task) {
    errno = ESRCH;
    return -1;
  }

  if (!task_session_has_pgrp_impl(task->sid, pgrp)) {
    errno = EPERM;
    return -1;
  }

  fs_mutex_lock(&pty_lock);
  pty_pair_t *pair = &pty_table[pty_index];
  if (!pair->allocated) {
    fs_mutex_unlock(&pty_lock);
    errno = EINVAL;
    return -1;
  }

  if (!pair->has_controlling_session || pair->controlling_sid != task->sid) {
    fs_mutex_unlock(&pty_lock);
    errno = ENOTTY;
    return -1;
  }

  int32_t fg_pgrp = pair->foreground_pgrp;
  if (fg_pgrp > 0 && fg_pgrp != task->pgid) {
    if (pty_signal_is_ignored(task, PTY_SIGTTOU)) {
    } else if (signal_is_blocked(task, PTY_SIGTTOU)) {
      fs_mutex_unlock(&pty_lock);
      errno = EINTR;
      return -1;
    } else {
      (void)signal_generate_pgrp(task->pgid, PTY_SIGTTOU);
      fs_mutex_unlock(&pty_lock);
      errno = EINTR;
      return -1;
    }
  }

  pair->foreground_pgrp = pgrp;
  fs_mutex_unlock(&pty_lock);
  return 0;
}

int pty_detach_controlling_tty_impl(void) {
  struct task_struct *task = get_current();
  if (!task) {
    errno = ESRCH;
    return -1;
  }

  fs_mutex_lock(&task->lock);

  if (!task->tty) {
    fs_mutex_unlock(&task->lock);
    errno = ENOTTY;
    return -1;
  }

  unsigned int pty_index = (unsigned int)task->tty->index;
  int32_t old_fg_pgrp = task->tty->foreground_pgrp;
  int32_t current_sid = task->sid;

  if (task->tty) {
    atomic_fetch_sub(&task->tty->refs, 1);
  }
  task->tty = NULL;

  fs_mutex_lock(&pty_lock);
  pty_pair_t *pair = &pty_table[pty_index];
  if (pair->allocated && pair->has_controlling_session && pair->controlling_sid == current_sid) {
    if (old_fg_pgrp > 0 && old_fg_pgrp != task->pgid) {
      pair->foreground_pgrp = task->pgid;
    }
    if (current_sid == pair->controlling_sid) {
      pair->has_controlling_session = false;
      pair->controlling_sid = 0;
      if (old_fg_pgrp > 0) {
        signal_generate_pgrp(old_fg_pgrp, 1);
      }
    }
  }
  fs_mutex_unlock(&pty_lock);

  fs_mutex_unlock(&task->lock);
  return 0;
}
