#ifndef ORLIX_HOST_ADAPTER_OBSERVABILITY_LOG_H
#define ORLIX_HOST_ADAPTER_OBSERVABILITY_LOG_H

#include <stddef.h>

typedef enum {
    ORLIX_HOST_TRACE_CATEGORY_BOOT_PROGRESS = 0,
    ORLIX_HOST_TRACE_CATEGORY_LINUX_CONSOLE = 1,
    ORLIX_HOST_TRACE_CATEGORY_HOST_VM = 2,
} orlix_host_trace_category_t;

typedef enum {
    ORLIX_HOST_TRACE_LEVEL_INFO = 0,
    ORLIX_HOST_TRACE_LEVEL_ERROR = 1,
} orlix_host_trace_level_t;

enum {
    ORLIX_HOST_TRACE_SINK_OS_LOG = 1U << 0,
    ORLIX_HOST_TRACE_SINK_STDERR = 1U << 1,
};

__attribute__((visibility("hidden"))) void orlix_host_trace_printf(
    orlix_host_trace_category_t category,
    orlix_host_trace_level_t level,
    unsigned int sinks,
    const char *format,
    ...);

__attribute__((visibility("hidden"))) void orlix_host_trace_bytes(
    orlix_host_trace_category_t category,
    orlix_host_trace_level_t level,
    unsigned int sinks,
    const void *bytes,
    size_t length);

#endif /* ORLIX_HOST_ADAPTER_OBSERVABILITY_LOG_H */
