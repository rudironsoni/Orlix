#include "OrlixHostAdapter/boot/progress.h"

#include <mach/mach_time.h>
#include <os/lock.h>
#include <stddef.h>
#include <string.h>

#define ORLIX_HOST_BOOT_PROGRESS_CAPACITY 64U

static os_unfair_lock OrlixHostBootProgressLock = OS_UNFAIR_LOCK_INIT;
static orlix_host_boot_progress_event_t
    OrlixHostBootProgressEvents[ORLIX_HOST_BOOT_PROGRESS_CAPACITY];
static uint64_t OrlixHostBootProgressNextSequence = 1;
static uint32_t OrlixHostBootProgressStart;
static uint32_t OrlixHostBootProgressCount;
static int OrlixHostBootProgressHasFirstConsoleOutput;

static uint64_t OrlixHostBootProgressMonotonicNanos(void)
{
    static mach_timebase_info_data_t timebase;
    uint64_t ticks = mach_continuous_time();

    if (timebase.denom == 0) {
        (void)mach_timebase_info(&timebase);
    }
    if (timebase.denom == 0) {
        return ticks;
    }

    return ticks * (uint64_t)timebase.numer / (uint64_t)timebase.denom;
}

__attribute__((visibility("default"))) void orlix_host_boot_progress_reset(void)
{
    os_unfair_lock_lock(&OrlixHostBootProgressLock);
    memset(
        OrlixHostBootProgressEvents,
        0,
        sizeof(OrlixHostBootProgressEvents)
    );
    OrlixHostBootProgressNextSequence = 1;
    OrlixHostBootProgressStart = 0;
    OrlixHostBootProgressCount = 0;
    OrlixHostBootProgressHasFirstConsoleOutput = 0;
    os_unfair_lock_unlock(&OrlixHostBootProgressLock);
}

__attribute__((visibility("default"))) void orlix_host_boot_progress_record(
    uint32_t stage,
    int32_t status,
    int32_t mach_kern_return,
    int32_t posix_errno
)
{
    orlix_host_boot_progress_event_t event;
    uint32_t index;

    os_unfair_lock_lock(&OrlixHostBootProgressLock);

    if (stage == ORLIX_HOST_BOOT_STAGE_FIRST_CONSOLE_OUTPUT) {
        if (OrlixHostBootProgressHasFirstConsoleOutput) {
            os_unfair_lock_unlock(&OrlixHostBootProgressLock);
            return;
        }
        OrlixHostBootProgressHasFirstConsoleOutput = 1;
    }

    event.sequence = OrlixHostBootProgressNextSequence++;
    event.monotonic_ns = OrlixHostBootProgressMonotonicNanos();
    event.stage = stage;
    event.status = status;
    event.mach_kern_return = mach_kern_return;
    event.posix_errno = posix_errno;

    if (OrlixHostBootProgressCount < ORLIX_HOST_BOOT_PROGRESS_CAPACITY) {
        index = (OrlixHostBootProgressStart + OrlixHostBootProgressCount) %
            ORLIX_HOST_BOOT_PROGRESS_CAPACITY;
        OrlixHostBootProgressCount++;
    } else {
        index = OrlixHostBootProgressStart;
        OrlixHostBootProgressStart =
            (OrlixHostBootProgressStart + 1) %
            ORLIX_HOST_BOOT_PROGRESS_CAPACITY;
    }

    OrlixHostBootProgressEvents[index] = event;
    os_unfair_lock_unlock(&OrlixHostBootProgressLock);
}

__attribute__((visibility("default"))) uint32_t orlix_host_boot_progress_snapshot(
    orlix_host_boot_progress_event_t *events,
    uint32_t capacity
)
{
    uint32_t copied = 0;

    if (!events || capacity == 0) {
        return 0;
    }

    os_unfair_lock_lock(&OrlixHostBootProgressLock);
    while (copied < OrlixHostBootProgressCount && copied < capacity) {
        uint32_t index = (OrlixHostBootProgressStart + copied) %
            ORLIX_HOST_BOOT_PROGRESS_CAPACITY;
        events[copied] = OrlixHostBootProgressEvents[index];
        copied++;
    }
    os_unfair_lock_unlock(&OrlixHostBootProgressLock);

    return copied;
}

__attribute__((visibility("default"))) int orlix_host_boot_progress_latest(
    orlix_host_boot_progress_event_t *event
)
{
    uint32_t index;

    if (!event) {
        return 0;
    }

    os_unfair_lock_lock(&OrlixHostBootProgressLock);
    if (OrlixHostBootProgressCount == 0) {
        memset(event, 0, sizeof(*event));
        os_unfair_lock_unlock(&OrlixHostBootProgressLock);
        return 0;
    }

    index = (OrlixHostBootProgressStart + OrlixHostBootProgressCount - 1) %
        ORLIX_HOST_BOOT_PROGRESS_CAPACITY;
    *event = OrlixHostBootProgressEvents[index];
    os_unfair_lock_unlock(&OrlixHostBootProgressLock);

    return 1;
}
