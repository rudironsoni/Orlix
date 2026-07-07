#include "OrlixHostAdapter/terminal/console.h"
#include "OrlixHostAdapter/boot/progress.h"
#include "OrlixHostAdapter/observability/log.h"
#include "OrlixHostAdapter/runtime/host_tls.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <os/lock.h>
#include <poll.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#define ORLIX_HOST_CONSOLE_INPUT_BYTES 65536UL
#define ORLIX_HOST_CONSOLE_RECENT_OUTPUT_BYTES 65536UL

static int OrlixHostConsoleOutputFD = -1;

static os_unfair_lock OrlixHostConsoleInputLock = OS_UNFAIR_LOCK_INIT;
static unsigned char OrlixHostConsoleInput[ORLIX_HOST_CONSOLE_INPUT_BYTES];
static unsigned long OrlixHostConsoleInputHead;
static unsigned long OrlixHostConsoleInputLength;

static os_unfair_lock OrlixHostConsoleRecentOutputLock =
    OS_UNFAIR_LOCK_INIT;
static unsigned char
    OrlixHostConsoleRecentOutput[ORLIX_HOST_CONSOLE_RECENT_OUTPUT_BYTES];
static unsigned long OrlixHostConsoleRecentOutputHead;
static unsigned long OrlixHostConsoleRecentOutputLength;

static void OrlixHostConsoleWriteFileDescriptor(const void *bytes,
                                                unsigned long length)
{
    const char *cursor = bytes;
    unsigned long offset = 0;
    int fd = OrlixHostConsoleOutputFD;
    unsigned int backpressure_retries = 0;

    if (fd < 0 || !bytes || length == 0) {
        return;
    }

    while (offset < length) {
        unsigned long remaining = length - offset;
        unsigned long chunk = remaining > (unsigned long)SSIZE_MAX ?
            (unsigned long)SSIZE_MAX : remaining;
        ssize_t written = write(fd, cursor + offset, (size_t)chunk);

        if (written < 0 && errno == EINTR) {
            continue;
        }
        if (written < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            struct pollfd poll_fd = {
                .fd = fd,
                .events = POLLOUT,
            };
            int ready = poll(&poll_fd, 1, 100);
            if (ready > 0 && (poll_fd.revents & POLLOUT) != 0) {
                backpressure_retries = 0;
                continue;
            }
            if (++backpressure_retries < 100) {
                continue;
            }
            break;
        }
        if (written <= 0) {
            break;
        }

        backpressure_retries = 0;
        offset += (unsigned long)written;
    }
}

static void OrlixHostConsoleRememberRecentOutput(const void *bytes,
                                                 unsigned long length)
{
    const unsigned char *cursor = bytes;
    unsigned long offset = 0;

    if (!bytes || length == 0) {
        return;
    }

    os_unfair_lock_lock(&OrlixHostConsoleRecentOutputLock);

    if (length >= ORLIX_HOST_CONSOLE_RECENT_OUTPUT_BYTES) {
        cursor += length - ORLIX_HOST_CONSOLE_RECENT_OUTPUT_BYTES;
        length = ORLIX_HOST_CONSOLE_RECENT_OUTPUT_BYTES;
        OrlixHostConsoleRecentOutputHead = 0;
        OrlixHostConsoleRecentOutputLength = 0;
    }

    while (offset < length) {
        unsigned long tail =
            (OrlixHostConsoleRecentOutputHead +
             OrlixHostConsoleRecentOutputLength) %
            ORLIX_HOST_CONSOLE_RECENT_OUTPUT_BYTES;
        unsigned long contiguous =
            ORLIX_HOST_CONSOLE_RECENT_OUTPUT_BYTES - tail;
        unsigned long remaining = length - offset;
        unsigned long chunk = contiguous < remaining ? contiguous : remaining;

        memcpy(&OrlixHostConsoleRecentOutput[tail],
               cursor + offset,
               (size_t)chunk);
        offset += chunk;

        if (OrlixHostConsoleRecentOutputLength + chunk <=
            ORLIX_HOST_CONSOLE_RECENT_OUTPUT_BYTES) {
            OrlixHostConsoleRecentOutputLength += chunk;
        } else {
            unsigned long overflow =
                OrlixHostConsoleRecentOutputLength + chunk -
                ORLIX_HOST_CONSOLE_RECENT_OUTPUT_BYTES;
            OrlixHostConsoleRecentOutputHead =
                (OrlixHostConsoleRecentOutputHead + overflow) %
                ORLIX_HOST_CONSOLE_RECENT_OUTPUT_BYTES;
            OrlixHostConsoleRecentOutputLength =
                ORLIX_HOST_CONSOLE_RECENT_OUTPUT_BYTES;
        }
    }

    os_unfair_lock_unlock(&OrlixHostConsoleRecentOutputLock);
}

__attribute__((visibility("default"))) void orlix_host_console_set_output_fd(
    int fd)
{
    int flags;

    if (fd >= 0) {
        flags = fcntl(fd, F_GETFL, 0);
        if (flags >= 0) {
            (void)fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        }
    }

    OrlixHostConsoleOutputFD = fd;
}

__attribute__((visibility("default"))) unsigned long
orlix_host_console_enqueue_input(const void *bytes, unsigned long length)
{
    const unsigned char *cursor = bytes;
    unsigned long copied = 0;

    if (!bytes || length == 0) {
        return 0;
    }

    os_unfair_lock_lock(&OrlixHostConsoleInputLock);
    while (copied < length &&
           OrlixHostConsoleInputLength < ORLIX_HOST_CONSOLE_INPUT_BYTES) {
        unsigned long tail =
            (OrlixHostConsoleInputHead + OrlixHostConsoleInputLength) %
            ORLIX_HOST_CONSOLE_INPUT_BYTES;
        unsigned long contiguous = ORLIX_HOST_CONSOLE_INPUT_BYTES - tail;
        unsigned long available =
            ORLIX_HOST_CONSOLE_INPUT_BYTES - OrlixHostConsoleInputLength;
        unsigned long remaining = length - copied;
        unsigned long chunk = contiguous;

        if (chunk > available) {
            chunk = available;
        }
        if (chunk > remaining) {
            chunk = remaining;
        }

        memcpy(&OrlixHostConsoleInput[tail], cursor + copied, (size_t)chunk);
        copied += chunk;
        OrlixHostConsoleInputLength += chunk;
    }
    os_unfair_lock_unlock(&OrlixHostConsoleInputLock);

    return copied;
}

__attribute__((visibility("hidden"))) unsigned long
orlix_host_console_read_input(void *bytes, unsigned long length)
{
    unsigned char *cursor = bytes;
    unsigned long copied = 0;

    if (!bytes || length == 0) {
        return 0;
    }

    os_unfair_lock_lock(&OrlixHostConsoleInputLock);
    while (copied < length && OrlixHostConsoleInputLength > 0) {
        unsigned long contiguous =
            ORLIX_HOST_CONSOLE_INPUT_BYTES - OrlixHostConsoleInputHead;
        unsigned long chunk = contiguous;
        unsigned long remaining = length - copied;

        if (chunk > OrlixHostConsoleInputLength) {
            chunk = OrlixHostConsoleInputLength;
        }
        if (chunk > remaining) {
            chunk = remaining;
        }

        memcpy(cursor + copied,
               &OrlixHostConsoleInput[OrlixHostConsoleInputHead],
               (size_t)chunk);
        OrlixHostConsoleInputHead =
            (OrlixHostConsoleInputHead + chunk) %
            ORLIX_HOST_CONSOLE_INPUT_BYTES;
        OrlixHostConsoleInputLength -= chunk;
        copied += chunk;
    }
    os_unfair_lock_unlock(&OrlixHostConsoleInputLock);

    return copied;
}

__attribute__((visibility("default"))) void
orlix_host_console_recent_output_clear(void)
{
    os_unfair_lock_lock(&OrlixHostConsoleRecentOutputLock);
    OrlixHostConsoleRecentOutputHead = 0;
    OrlixHostConsoleRecentOutputLength = 0;
    os_unfair_lock_unlock(&OrlixHostConsoleRecentOutputLock);
}

__attribute__((visibility("default"))) unsigned long
orlix_host_console_recent_output_snapshot(void *bytes, unsigned long capacity)
{
    unsigned char *cursor = bytes;
    unsigned long copied = 0;

    if (!bytes || capacity == 0) {
        return 0;
    }

    os_unfair_lock_lock(&OrlixHostConsoleRecentOutputLock);
    while (copied < capacity &&
           copied < OrlixHostConsoleRecentOutputLength) {
        unsigned long source =
            (OrlixHostConsoleRecentOutputHead + copied) %
            ORLIX_HOST_CONSOLE_RECENT_OUTPUT_BYTES;
        unsigned long contiguous =
            ORLIX_HOST_CONSOLE_RECENT_OUTPUT_BYTES - source;
        unsigned long remaining = OrlixHostConsoleRecentOutputLength - copied;
        unsigned long available = capacity - copied;
        unsigned long chunk = contiguous;

        if (chunk > remaining) {
            chunk = remaining;
        }
        if (chunk > available) {
            chunk = available;
        }

        memcpy(cursor + copied,
               &OrlixHostConsoleRecentOutput[source],
               (size_t)chunk);
        copied += chunk;
    }
    os_unfair_lock_unlock(&OrlixHostConsoleRecentOutputLock);

    return copied;
}

__attribute__((visibility("hidden"))) void orlix_host_console_write(
    const void *bytes,
    unsigned long length)
{
    unsigned long active_tls;

    if (!bytes || length == 0) {
        return;
    }

    active_tls = OrlixHostEnterHostTls();
    orlix_host_boot_progress_record(
        ORLIX_HOST_BOOT_STAGE_FIRST_CONSOLE_OUTPUT,
        0,
        0,
        0
    );
    OrlixHostConsoleRememberRecentOutput(bytes, length);
    OrlixHostConsoleWriteFileDescriptor(bytes, length);
    orlix_host_trace_bytes(ORLIX_HOST_TRACE_CATEGORY_LINUX_CONSOLE,
                           ORLIX_HOST_TRACE_LEVEL_INFO,
                           ORLIX_HOST_TRACE_SINK_OS_LOG |
                               ORLIX_HOST_TRACE_SINK_STDERR,
                           bytes,
                           (size_t)length);

    OrlixHostLeaveHostTls(active_tls);
}
