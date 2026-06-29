#ifndef ORLIX_HOST_ADAPTER_BOOT_PROGRESS_H
#define ORLIX_HOST_ADAPTER_BOOT_PROGRESS_H

#include <stdint.h>

typedef enum {
    ORLIX_HOST_BOOT_STAGE_UNKNOWN = 0,
    ORLIX_HOST_BOOT_STAGE_SESSION_CREATED = 10,
    ORLIX_HOST_BOOT_STAGE_PAYLOAD_REGISTERING = 20,
    ORLIX_HOST_BOOT_STAGE_PAYLOAD_REGISTERED = 30,
    ORLIX_HOST_BOOT_STAGE_BOOTLOADER_ENTERED = 40,
    ORLIX_HOST_BOOT_STAGE_BOOT_CONFIG_VALIDATED = 50,
    ORLIX_HOST_BOOT_STAGE_HOST_RESOURCES_READY = 60,
    ORLIX_HOST_BOOT_STAGE_KERNEL_HANDOFF = 70,
    ORLIX_HOST_BOOT_STAGE_ARCH_ENTRY = 80,
    ORLIX_HOST_BOOT_STAGE_EARLY_CONSOLE_READY = 90,
    ORLIX_HOST_BOOT_STAGE_LINUX_START_KERNEL = 100,
    ORLIX_HOST_BOOT_STAGE_FIRST_CONSOLE_OUTPUT = 110,
    ORLIX_HOST_BOOT_STAGE_FAILED = 1000
} orlix_host_boot_stage_t;

typedef struct {
    uint64_t sequence;
    uint64_t monotonic_ns;
    uint32_t stage;
    int32_t status;
    int32_t mach_kern_return;
    int32_t posix_errno;
} orlix_host_boot_progress_event_t;

__attribute__((visibility("default"))) void orlix_host_boot_progress_reset(void);

__attribute__((visibility("default"))) void orlix_host_boot_progress_record(
    uint32_t stage,
    int32_t status,
    int32_t mach_kern_return,
    int32_t posix_errno
);

__attribute__((visibility("default"))) uint32_t orlix_host_boot_progress_snapshot(
    orlix_host_boot_progress_event_t *events,
    uint32_t capacity
);

__attribute__((visibility("default"))) int orlix_host_boot_progress_latest(
    orlix_host_boot_progress_event_t *event
);

#endif
