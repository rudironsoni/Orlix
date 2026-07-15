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
#define ORLIX_HOST_CONSOLE_SOURCE_COUNT 2UL

struct OrlixHostConsoleOutputState {
    os_unfair_lock output_lock;
    int output_fd;
    os_unfair_lock recent_output_lock;
    unsigned char recent_output[ORLIX_HOST_CONSOLE_RECENT_OUTPUT_BYTES];
    unsigned long recent_output_head;
    unsigned long recent_output_length;
};

static struct OrlixHostConsoleOutputState
    OrlixHostConsoleOutputs[ORLIX_HOST_CONSOLE_SOURCE_COUNT] = {
        {
            .output_lock = OS_UNFAIR_LOCK_INIT,
            .output_fd = -1,
            .recent_output_lock = OS_UNFAIR_LOCK_INIT,
        },
        {
            .output_lock = OS_UNFAIR_LOCK_INIT,
            .output_fd = -1,
            .recent_output_lock = OS_UNFAIR_LOCK_INIT,
        },
    };

static os_unfair_lock OrlixHostConsoleInputLock = OS_UNFAIR_LOCK_INIT;
static unsigned char OrlixHostConsoleInput[ORLIX_HOST_CONSOLE_INPUT_BYTES];
static unsigned long OrlixHostConsoleInputHead;
static unsigned long OrlixHostConsoleInputLength;

static struct OrlixHostConsoleOutputState *OrlixHostConsoleOutput(
    enum orlix_host_console_source source)
{
    if ((unsigned long)source >= ORLIX_HOST_CONSOLE_SOURCE_COUNT) {
        return NULL;
    }
    return &OrlixHostConsoleOutputs[(unsigned long)source];
}

static void OrlixHostConsoleWriteFileDescriptor(
    struct OrlixHostConsoleOutputState *output,
    const void *bytes,
    unsigned long length)
{
    const char *cursor = bytes;
    unsigned long offset = 0;
    int fd;
    unsigned int backpressure_retries = 0;

    if (!output || !bytes || length == 0) {
        return;
    }

    os_unfair_lock_lock(&output->output_lock);
    fd = output->output_fd;
    os_unfair_lock_unlock(&output->output_lock);
    if (fd < 0) {
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

static void OrlixHostConsoleRememberRecentOutput(
    struct OrlixHostConsoleOutputState *output,
    const void *bytes,
    unsigned long length)
{
    const unsigned char *cursor = bytes;
    unsigned long offset = 0;

    if (!output || !bytes || length == 0) {
        return;
    }

    os_unfair_lock_lock(&output->recent_output_lock);

    if (length >= ORLIX_HOST_CONSOLE_RECENT_OUTPUT_BYTES) {
        cursor += length - ORLIX_HOST_CONSOLE_RECENT_OUTPUT_BYTES;
        length = ORLIX_HOST_CONSOLE_RECENT_OUTPUT_BYTES;
        output->recent_output_head = 0;
        output->recent_output_length = 0;
    }

    while (offset < length) {
        unsigned long tail =
            (output->recent_output_head +
             output->recent_output_length) %
            ORLIX_HOST_CONSOLE_RECENT_OUTPUT_BYTES;
        unsigned long contiguous =
            ORLIX_HOST_CONSOLE_RECENT_OUTPUT_BYTES - tail;
        unsigned long remaining = length - offset;
        unsigned long chunk = contiguous < remaining ? contiguous : remaining;

        memcpy(&output->recent_output[tail],
               cursor + offset,
               (size_t)chunk);
        offset += chunk;

        if (output->recent_output_length + chunk <=
            ORLIX_HOST_CONSOLE_RECENT_OUTPUT_BYTES) {
            output->recent_output_length += chunk;
        } else {
            unsigned long overflow =
                output->recent_output_length + chunk -
                ORLIX_HOST_CONSOLE_RECENT_OUTPUT_BYTES;
            output->recent_output_head =
                (output->recent_output_head + overflow) %
                ORLIX_HOST_CONSOLE_RECENT_OUTPUT_BYTES;
            output->recent_output_length =
                ORLIX_HOST_CONSOLE_RECENT_OUTPUT_BYTES;
        }
    }

    os_unfair_lock_unlock(&output->recent_output_lock);
}

__attribute__((visibility("default"))) void orlix_host_console_set_output_fd(
    enum orlix_host_console_source source, int fd)
{
    struct OrlixHostConsoleOutputState *output = OrlixHostConsoleOutput(source);
    int flags;

    if (!output) {
        return;
    }

    if (fd >= 0) {
        flags = fcntl(fd, F_GETFL, 0);
        if (flags >= 0) {
            (void)fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        }
    }

    os_unfair_lock_lock(&output->output_lock);
    output->output_fd = fd;
    os_unfair_lock_unlock(&output->output_lock);
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
orlix_host_console_recent_output_clear(enum orlix_host_console_source source)
{
    struct OrlixHostConsoleOutputState *output = OrlixHostConsoleOutput(source);

    if (!output) {
        return;
    }
    os_unfair_lock_lock(&output->recent_output_lock);
    output->recent_output_head = 0;
    output->recent_output_length = 0;
    os_unfair_lock_unlock(&output->recent_output_lock);
}

__attribute__((visibility("default"))) unsigned long
orlix_host_console_recent_output_snapshot(enum orlix_host_console_source source,
                                          void *bytes,
                                          unsigned long capacity)
{
    struct OrlixHostConsoleOutputState *output = OrlixHostConsoleOutput(source);
    unsigned char *cursor = bytes;
    unsigned long copied = 0;

    if (!output || !bytes || capacity == 0) {
        return 0;
    }

    os_unfair_lock_lock(&output->recent_output_lock);
    while (copied < capacity &&
           copied < output->recent_output_length) {
        unsigned long source_offset =
            (output->recent_output_head + copied) %
            ORLIX_HOST_CONSOLE_RECENT_OUTPUT_BYTES;
        unsigned long contiguous =
            ORLIX_HOST_CONSOLE_RECENT_OUTPUT_BYTES - source_offset;
        unsigned long remaining = output->recent_output_length - copied;
        unsigned long available = capacity - copied;
        unsigned long chunk = contiguous;

        if (chunk > remaining) {
            chunk = remaining;
        }
        if (chunk > available) {
            chunk = available;
        }

        memcpy(cursor + copied,
               &output->recent_output[source_offset],
               (size_t)chunk);
        copied += chunk;
    }
    os_unfair_lock_unlock(&output->recent_output_lock);

    return copied;
}

__attribute__((visibility("hidden"))) void orlix_host_console_write(
    enum orlix_host_console_source source,
    const void *bytes,
    unsigned long length)
{
    struct OrlixHostConsoleOutputState *output = OrlixHostConsoleOutput(source);
    unsigned long active_tls;

    if (!output || !bytes || length == 0) {
        return;
    }

    active_tls = OrlixHostEnterHostTls();
    orlix_host_boot_progress_record(
        ORLIX_HOST_BOOT_STAGE_FIRST_CONSOLE_OUTPUT,
        0,
        0,
        0
    );
    OrlixHostConsoleRememberRecentOutput(output, bytes, length);
    OrlixHostConsoleWriteFileDescriptor(output, bytes, length);
    orlix_host_trace_bytes(ORLIX_HOST_TRACE_CATEGORY_LINUX_CONSOLE,
                           ORLIX_HOST_TRACE_LEVEL_INFO,
                           ORLIX_HOST_TRACE_SINK_OS_LOG |
                               ORLIX_HOST_TRACE_SINK_STDERR,
                           bytes,
                           (size_t)length);

    OrlixHostLeaveHostTls(active_tls);
}
