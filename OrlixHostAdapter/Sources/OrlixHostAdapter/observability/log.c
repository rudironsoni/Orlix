#include "OrlixHostAdapter/observability/log.h"

#include <os/log.h>
#include <stdarg.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define ORLIX_HOST_TRACE_MESSAGE_BYTES 2048U
#define ORLIX_HOST_TRACE_BYTES_CHUNK 1024U

static const char *OrlixHostTraceCategoryName(
    orlix_host_trace_category_t category)
{
    switch (category) {
    case ORLIX_HOST_TRACE_CATEGORY_BOOT_PROGRESS:
        return "boot-progress";
    case ORLIX_HOST_TRACE_CATEGORY_LINUX_CONSOLE:
        return "linux-console";
    case ORLIX_HOST_TRACE_CATEGORY_HOST_VM:
        return "host-vm";
    }
    return "host";
}

static os_log_type_t OrlixHostTraceOSLogType(orlix_host_trace_level_t level)
{
    switch (level) {
    case ORLIX_HOST_TRACE_LEVEL_ERROR:
        return OS_LOG_TYPE_ERROR;
    case ORLIX_HOST_TRACE_LEVEL_INFO:
    default:
        return OS_LOG_TYPE_INFO;
    }
}

static os_log_t OrlixHostTraceLog(orlix_host_trace_category_t category)
{
    static os_log_t logs[3];
    size_t index = (size_t)category;

    if (index >= sizeof(logs) / sizeof(logs[0])) {
        index = 0;
    }
    if (!logs[index]) {
        logs[index] = os_log_create("com.rudironsoni.Orlix",
                                    OrlixHostTraceCategoryName(category));
    }
    return logs[index];
}

static int OrlixHostTraceShouldWriteStderr(unsigned int sinks)
{
#if DEBUG || ORLIX_BETA_OBSERVABILITY || ORLIX_ENABLE_CONSOLE_STDERR_MIRROR
    return (sinks & ORLIX_HOST_TRACE_SINK_STDERR) != 0;
#else
    (void)sinks;
    return 0;
#endif
}

__attribute__((visibility("hidden"))) void orlix_host_trace_printf(
    orlix_host_trace_category_t category,
    orlix_host_trace_level_t level,
    unsigned int sinks,
    const char *format,
    ...)
{
    char message[ORLIX_HOST_TRACE_MESSAGE_BYTES];
    va_list args;

    if (!format || sinks == 0) {
        return;
    }

    va_start(args, format);
    (void)vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    message[sizeof(message) - 1] = '\0';

    if (sinks & ORLIX_HOST_TRACE_SINK_OS_LOG) {
        os_log_with_type(OrlixHostTraceLog(category),
                         OrlixHostTraceOSLogType(level),
                         "%{public}s",
                         message);
    }
    if (OrlixHostTraceShouldWriteStderr(sinks)) {
        (void)dprintf(STDERR_FILENO,
                      "[%s] %s\n",
                      OrlixHostTraceCategoryName(category),
                      message);
    }
}

__attribute__((visibility("hidden"))) void orlix_host_trace_bytes(
    orlix_host_trace_category_t category,
    orlix_host_trace_level_t level,
    unsigned int sinks,
    const void *bytes,
    size_t length)
{
    const char *text = bytes;
    size_t offset = 0;

    if (!bytes || length == 0 || sinks == 0) {
        return;
    }

    if (OrlixHostTraceShouldWriteStderr(sinks)) {
        (void)write(STDERR_FILENO, bytes, length);
    }

    if (!(sinks & ORLIX_HOST_TRACE_SINK_OS_LOG)) {
        return;
    }

    while (offset < length) {
        size_t remaining = length - offset;
        size_t chunk = remaining > ORLIX_HOST_TRACE_BYTES_CHUNK
                           ? ORLIX_HOST_TRACE_BYTES_CHUNK
                           : remaining;
        int chunk_length = chunk > (size_t)INT_MAX ? INT_MAX : (int)chunk;

        os_log_with_type(OrlixHostTraceLog(category),
                         OrlixHostTraceOSLogType(level),
                         "%{public}.*s",
                         chunk_length,
                         text + offset);
        offset += (size_t)chunk_length;
    }
}
