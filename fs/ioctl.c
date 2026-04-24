/* iXland - IOCTL Operations
 *
 * Canonical owner for ioctl:
 * - ioctl()
 *
 * Linux-shaped canonical owner - iOS mediation as implementation detail
 */

#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <sys/ioctl.h>

#include "fdtable.h"
#include "pty.h"
#include "internal/ios/fs/backing_io.h"

#define IX_TCGETS 0x5401
#define IX_TCSETS 0x5402
#define IX_TCSETSW 0x5403
#define IX_TCSETSF 0x5404
#define IX_TIOCSCTTY 0x540E
#define IX_TIOCNOTTY 0x5432
#define IX_TIOCGPGRP 0x540F
#define IX_TIOCSPGRP 0x5410
#define IX_TIOCGWINSZ 0x5413
#define IX_TIOCSWINSZ 0x5414
#define IX_FIONREAD 0x541B
#define IX_TIOCGPTN 0x80045430UL
#define IX_TIOCSPTLCK 0x40045431UL

static int ioctl_host_call_impl(int fd, unsigned long request, void *arg) {
    return host_ioctl_impl(fd, request, arg);
}

static int ioctl_impl(int fd, unsigned long request, void *arg) {
    if (fd < 0 || fd >= NR_OPEN_DEFAULT) {
        errno = EBADF;
        return -1;
    }

    if (fd <= 2) {
        return ioctl_host_call_impl(fd, request, arg);
    }

    void *entry = get_fd_entry_impl(fd);
    if (!entry) {
        errno = EBADF;
        return -1;
    }

    if (get_fd_is_synthetic_pty_impl(entry)) {
        unsigned int pty_index = get_fd_synthetic_pty_index_impl(entry);
        bool is_master = get_fd_is_synthetic_pty_master_impl(entry);
        int result = -1;

        switch (request) {
        case IX_TIOCGPTN:
            if (!is_master || !arg) {
                errno = !arg ? EFAULT : EINVAL;
                break;
            }
            *(unsigned int *)arg = pty_index;
            result = 0;
            break;
        case IX_TIOCSPTLCK:
            if (!is_master || !arg) {
                errno = !arg ? EFAULT : EINVAL;
                break;
            }
            result = pty_set_lock_impl(pty_index, (*(const int *)arg) != 0);
            break;
        case IX_TCGETS:
            if (!arg) {
                errno = EFAULT;
                break;
            }
            result = pty_get_termios_impl(pty_index, (pty_linux_termios_t *)arg);
            break;
        case IX_TCSETS:
            if (!arg) {
                errno = EFAULT;
                break;
            }
            result = pty_set_termios_with_action_impl(pty_index, (const pty_linux_termios_t *)arg,
                                                      PTY_TCSET_ACTION_NOW);
            break;
        case IX_TCSETSW:
            if (!arg) {
                errno = EFAULT;
                break;
            }
            result = pty_set_termios_with_action_impl(pty_index, (const pty_linux_termios_t *)arg,
                                                      PTY_TCSET_ACTION_DRAIN);
            break;
        case IX_TCSETSF:
            if (!arg) {
                errno = EFAULT;
                break;
            }
            result = pty_set_termios_with_action_impl(pty_index, (const pty_linux_termios_t *)arg,
                                                      PTY_TCSET_ACTION_FLUSH);
            break;
        case IX_TIOCGWINSZ:
            if (!arg) {
                errno = EFAULT;
                break;
            }
            result = pty_get_winsize_impl(pty_index, (pty_linux_winsize_t *)arg);
            break;
        case IX_TIOCSWINSZ:
            if (!arg) {
                errno = EFAULT;
                break;
            }
            result = pty_set_winsize_impl(pty_index, (const pty_linux_winsize_t *)arg);
            break;
case IX_TIOCSCTTY:
        result = pty_set_controlling_tty_impl(pty_index, (int)(intptr_t)arg);
        break;
      case IX_TIOCNOTTY:
        result = pty_detach_controlling_tty_impl();
        break;
      case IX_TIOCGPGRP:
            if (!arg) {
                errno = EFAULT;
                break;
            }
            result = pty_get_foreground_pgrp_impl(pty_index, (int32_t *)arg);
            break;
        case IX_TIOCSPGRP:
            if (!arg) {
                errno = EFAULT;
                break;
            }
            result = pty_set_foreground_pgrp_impl(pty_index, *(const int32_t *)arg);
            break;
        case IX_FIONREAD: {
            if (!arg) {
                errno = EFAULT;
                break;
            }
            result = pty_get_readable_bytes_impl(pty_index, is_master, (int *)arg);
            break;
        }
        default:
            errno = ENOTTY;
            break;
        }

        put_fd_entry_impl(entry);
        return result;
    }

    int result;
    if (get_fd_is_synthetic_dev_impl(entry) || get_fd_is_synthetic_dir_impl(entry) ||
        get_fd_is_synthetic_proc_file_impl(entry)) {
        put_fd_entry_impl(entry);
        errno = ENOTTY;
        return -1;
    }

    result = ioctl_host_call_impl(get_real_fd_impl(entry), request, arg);
    put_fd_entry_impl(entry);
    return result;
}

__attribute__((visibility("default"))) int ioctl(int fd, unsigned long request, ...) {
    va_list args;
    va_start(args, request);
    void *arg = va_arg(args, void *);
    va_end(args);
    return ioctl_impl(fd, request, arg);
}
