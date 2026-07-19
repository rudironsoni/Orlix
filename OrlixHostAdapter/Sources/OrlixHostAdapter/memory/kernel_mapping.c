#include "OrlixHostAdapter/memory/kernel_mapping.h"
#include "OrlixHostAdapter/observability/log.h"
#include "OrlixHostAdapter/runtime/host_tls.h"
#include "internal/asm/host_trap.h"

#include <libkern/OSCacheControl.h>
#include <mach/mach.h>
#include <mach/vm_map.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

struct OrlixHostIOMapping {
    void *address;
    unsigned long physical_address;
    vm_size_t length;
    struct OrlixHostIOMapping *next;
};

struct OrlixHostUserMapping {
    unsigned long target_address;
    vm_size_t length;
    vm_prot_t protection;
    bool writable;
    struct OrlixHostKernelShadowSegment *segments;
    struct OrlixHostUserMapping *next;
};

struct OrlixHostKernelShadowSegment {
    unsigned long target_address;
    const void *source_page;
    vm_size_t length;
    struct OrlixHostKernelShadowSegment *next;
};

struct OrlixHostKernelShadowMapping {
    unsigned long target_address;
    vm_size_t length;
    struct OrlixHostKernelShadowSegment *segments;
    struct OrlixHostKernelShadowMapping *next;
};

static struct OrlixHostIOMapping *OrlixHostIOMappings;
static struct OrlixHostKernelShadowMapping *OrlixHostKernelShadowMappings;
static struct OrlixHostUserMapping *OrlixHostUserMappings;

#define ORLIX_HOST_IOMEM_MAX_SIZE 0x0000000010000000UL
#define ORLIX_HOST_HOSTED_USER_BASE_DEFAULT 0x0000000100000000UL
#define ORLIX_HOST_HOSTED_STACK_TOP_DEFAULT 0x0000000200000000UL
#define ORLIX_HOST_HOSTED_KERNEL_MAX_DEFAULT 0x0000000300000000UL
#define ORLIX_HOST_VM_ANYWHERE_ATTEMPTS 128U
#define ORLIX_HOST_VM_SCRATCH_MAX 8U
#define ORLIX_HOST_VM_SCRATCH_MAX_BYTES 0x0000000020000000UL

#ifdef VM_FLAGS_RANDOM_ADDR
#define ORLIX_HOST_VM_ANYWHERE_FLAGS (VM_FLAGS_ANYWHERE | VM_FLAGS_RANDOM_ADDR)
#else
#define ORLIX_HOST_VM_ANYWHERE_FLAGS VM_FLAGS_ANYWHERE
#endif

static unsigned long OrlixHostHostedUserBase =
    ORLIX_HOST_HOSTED_USER_BASE_DEFAULT;
static unsigned long OrlixHostHostedStackTop =
    ORLIX_HOST_HOSTED_STACK_TOP_DEFAULT;
static unsigned long OrlixHostHostedKernelMax =
    ORLIX_HOST_HOSTED_KERNEL_MAX_DEFAULT;
static bool OrlixHostHostedUserWindowReserved;

static unsigned long OrlixHostPageStart(unsigned long address);
static unsigned long OrlixHostPageEnd(unsigned long address,
                                      unsigned long length);
static void OrlixHostUnmapPages(unsigned long target_address,
                                unsigned long length);

static void OrlixHostUserMemoryBarrier(void)
{
    __asm__ volatile("dmb ish" ::: "memory");
}

static void OrlixHostInvalidateInstructionCache(unsigned long address,
                                                unsigned long length)
{
    if (address == 0 || length == 0) {
        return;
    }
    sys_icache_invalidate((void *)address, (size_t)length);
}

static struct orlix_host_user_mapping_failure OrlixHostLastUserMappingFailure;

static unsigned long OrlixHostUserMappingFailureOperation(const char *reason)
{
    if (!reason) {
        return ORLIX_HOST_USER_MAPPING_FAILURE_NONE;
    }
    if (strcmp(reason, "invalid-argument") == 0) {
        return ORLIX_HOST_USER_MAPPING_FAILURE_INVALID_ARGUMENT;
    }
    if (strcmp(reason, "create-mapping") == 0 ||
        strcmp(reason, "refresh-window-create-mapping") == 0) {
        return ORLIX_HOST_USER_MAPPING_FAILURE_CREATE_MAPPING;
    }
    if (strcmp(reason, "missing-mapping") == 0 ||
        strcmp(reason, "refresh-window-missing-mapping") == 0) {
        return ORLIX_HOST_USER_MAPPING_FAILURE_MISSING_MAPPING;
    }
    if (strcmp(reason, "copy-protect") == 0 ||
        strcmp(reason, "refresh-window-copy-protect") == 0) {
        return ORLIX_HOST_USER_MAPPING_FAILURE_COPY_PROTECT;
    }
    if (strcmp(reason, "refresh-window-clear-protect") == 0) {
        return ORLIX_HOST_USER_MAPPING_FAILURE_CLEAR_PROTECT;
    }
    if (strcmp(reason, "refresh-window-segment-protect") == 0) {
        return ORLIX_HOST_USER_MAPPING_FAILURE_SEGMENT_PROTECT;
    }
    return ORLIX_HOST_USER_MAPPING_FAILURE_NONE;
}

static void OrlixHostTraceShadowUserPageFailure(const char *reason,
                                                unsigned long target_address,
                                                unsigned long length,
                                                unsigned long mapping_address,
                                                unsigned long mapping_length,
                                                vm_prot_t requested_protection,
                                                vm_prot_t copy_protection,
                                                kern_return_t status)
{
    OrlixHostLastUserMappingFailure =
        (struct orlix_host_user_mapping_failure) {
            .operation = OrlixHostUserMappingFailureOperation(reason),
            .target_address = target_address,
            .length = length,
            .mapping_address = mapping_address,
            .mapping_length = mapping_length,
            .requested_protection = requested_protection,
            .attempted_protection = copy_protection,
            .host_status = status,
        };

#if DEBUG || ORLIX_BETA_OBSERVABILITY
    orlix_host_trace_printf(ORLIX_HOST_TRACE_CATEGORY_HOST_VM,
                            ORLIX_HOST_TRACE_LEVEL_ERROR,
                            ORLIX_HOST_TRACE_SINK_OS_LOG |
                                ORLIX_HOST_TRACE_SINK_STDERR,
                            "shadow user page map failed reason=%s target=0x%lx length=0x%lx mapping=0x%lx mapping_length=0x%lx requested_prot=0x%x copy_prot=0x%x status=%d",
                            reason,
                            target_address,
                            length,
                            mapping_address,
                            mapping_length,
                            requested_protection,
                            copy_protection,
                            status);
#else
    (void)reason;
    (void)target_address;
    (void)length;
    (void)mapping_address;
    (void)mapping_length;
    (void)requested_protection;
    (void)copy_protection;
    (void)status;
#endif
}

static bool OrlixHostRangeIntersects(unsigned long start,
                                     unsigned long length,
                                     unsigned long range_start,
                                     unsigned long range_end)
{
    unsigned long end;

    if (length == 0 || start > (unsigned long)-1 - length) {
        return true;
    }

    end = start + length;
    return start < range_end && end > range_start;
}

static bool OrlixHostRangeInsideHostedUserWindow(unsigned long start,
                                                  unsigned long end)
{
    return OrlixHostHostedUserWindowReserved &&
           start >= OrlixHostHostedUserBase && end <= OrlixHostHostedStackTop;
}

static void OrlixHostReleaseUserRange(unsigned long target_address,
                                      unsigned long length)
{
    unsigned long start = OrlixHostPageStart(target_address);
    unsigned long end = OrlixHostPageEnd(target_address, length);

    if (target_address == 0 || end == 0 || end <= start) {
        return;
    }

    if (OrlixHostRangeInsideHostedUserWindow(start, end)) {
        (void)vm_protect(mach_task_self(),
                         (vm_address_t)start,
                         (vm_size_t)(end - start),
                         false,
                         VM_PROT_NONE);
        return;
    }

    OrlixHostUnmapPages(start, end - start);
}

__attribute__((visibility("hidden"))) unsigned long orlix_host_memory_page_size(void)
{
    return (unsigned long)vm_page_size;
}

static vm_size_t OrlixHostRoundPageLength(unsigned long length)
{
    vm_size_t page_size = (vm_size_t)orlix_host_memory_page_size();
    vm_size_t requested = (vm_size_t)length;

    if (requested == 0 || requested > (vm_size_t)-1 - (page_size - 1)) {
        return 0;
    }

    return (requested + page_size - 1) & ~(page_size - 1);
}

static unsigned long OrlixHostAlignUp(unsigned long value,
                                      unsigned long alignment)
{
    if (alignment == 0 || (alignment & (alignment - 1UL)) != 0) {
        return 0;
    }
    if (value > (unsigned long)-1 - (alignment - 1UL)) {
        return 0;
    }
    return (value + alignment - 1UL) & ~(alignment - 1UL);
}

static int OrlixHostReserveFixedRange(unsigned long base,
                                      unsigned long length,
                                      kern_return_t *allocate_status,
                                      vm_address_t *returned_address,
                                      kern_return_t *protect_status)
{
    vm_address_t target = (vm_address_t)base;
    kern_return_t status;

    if (allocate_status) {
        *allocate_status = KERN_SUCCESS;
    }
    if (returned_address) {
        *returned_address = target;
    }
    if (protect_status) {
        *protect_status = KERN_SUCCESS;
    }

    status = vm_allocate(mach_task_self(),
                         &target,
                         (vm_size_t)length,
                         VM_FLAGS_FIXED);
    if (allocate_status) {
        *allocate_status = status;
    }
    if (returned_address) {
        *returned_address = target;
    }
    if (status != KERN_SUCCESS || target != (vm_address_t)base) {
        if (status == KERN_SUCCESS) {
            (void)vm_deallocate(mach_task_self(), target, (vm_size_t)length);
        }
        return -1;
    }

    status = vm_protect(mach_task_self(),
                        target,
                         (vm_size_t)length,
                         false,
                         VM_PROT_NONE);
    if (protect_status) {
        *protect_status = status;
    }
    if (status != KERN_SUCCESS) {
#if DEBUG || ORLIX_BETA_OBSERVABILITY
        orlix_host_trace_printf(ORLIX_HOST_TRACE_CATEGORY_HOST_VM,
                                ORLIX_HOST_TRACE_LEVEL_ERROR,
                                ORLIX_HOST_TRACE_SINK_OS_LOG |
                                    ORLIX_HOST_TRACE_SINK_STDERR,
                                "fixed reservation protect failed base=0x%lx length=0x%lx status=%d",
                                base,
                                length,
                                status);
#endif
        (void)vm_deallocate(mach_task_self(), target, (vm_size_t)length);
        return -1;
    }

#if DEBUG || ORLIX_BETA_OBSERVABILITY
    orlix_host_trace_printf(ORLIX_HOST_TRACE_CATEGORY_HOST_VM,
                            ORLIX_HOST_TRACE_LEVEL_INFO,
                            ORLIX_HOST_TRACE_SINK_OS_LOG |
                                ORLIX_HOST_TRACE_SINK_STDERR,
                            "fixed reservation ready base=0x%lx length=0x%lx",
                            base,
                            length);
#endif

    return 0;
}

static int OrlixHostReserveFirstAvailableRangeInGap(unsigned long gap_start,
                                                    unsigned long gap_end,
                                                    unsigned long length,
                                                    unsigned long alignment,
                                                    unsigned long *base_address)
{
    unsigned long probe_address = OrlixHostAlignUp(gap_start, alignment);
#if DEBUG || ORLIX_BETA_OBSERVABILITY
    unsigned long first_probe = probe_address;
    unsigned long last_probe = 0;
    unsigned long probe_count = 0;
    kern_return_t last_allocate_status = KERN_SUCCESS;
    kern_return_t last_protect_status = KERN_SUCCESS;
    vm_address_t last_returned_address = 0;
#endif

    if (!base_address || probe_address == 0 || gap_end <= gap_start ||
        length > gap_end - gap_start) {
        return -1;
    }

    while (probe_address <= gap_end - length) {
#if DEBUG || ORLIX_BETA_OBSERVABILITY
        last_probe = probe_address;
        probe_count++;
#endif
        if (OrlixHostReserveFixedRange(probe_address,
                                       length,
#if DEBUG || ORLIX_BETA_OBSERVABILITY
                                       &last_allocate_status,
                                       &last_returned_address,
                                       &last_protect_status
#else
                                       NULL,
                                       NULL,
                                       NULL
#endif
                                       ) == 0) {
            *base_address = probe_address;
#if DEBUG || ORLIX_BETA_OBSERVABILITY
            orlix_host_trace_printf(ORLIX_HOST_TRACE_CATEGORY_HOST_VM,
                                    ORLIX_HOST_TRACE_LEVEL_INFO,
                                    ORLIX_HOST_TRACE_SINK_OS_LOG |
                                        ORLIX_HOST_TRACE_SINK_STDERR,
                                    "gap reservation selected base=0x%lx length=0x%lx gapStart=0x%lx gapEnd=0x%lx alignment=0x%lx",
                                    probe_address,
                                    length,
                                    gap_start,
                                    gap_end,
                                    alignment);
#endif
            return 0;
        }
        if (probe_address > (unsigned long)-1 - alignment) {
            break;
        }
        probe_address += alignment;
    }

#if DEBUG || ORLIX_BETA_OBSERVABILITY
    orlix_host_trace_printf(ORLIX_HOST_TRACE_CATEGORY_HOST_VM,
                            ORLIX_HOST_TRACE_LEVEL_ERROR,
                            ORLIX_HOST_TRACE_SINK_OS_LOG |
                                ORLIX_HOST_TRACE_SINK_STDERR,
                            "gap reservation failed length=0x%lx gapStart=0x%lx gapEnd=0x%lx alignment=0x%lx probes=%lu firstProbe=0x%lx lastProbe=0x%lx lastAllocateStatus=%d lastReturned=0x%lx lastProtectStatus=%d",
                            length,
                            gap_start,
                            gap_end,
                            alignment,
                            probe_count,
                            first_probe,
                            last_probe,
                            last_allocate_status,
                            (unsigned long)last_returned_address,
                            last_protect_status);
#endif

    return -1;
}

static int OrlixHostReserveAnyAvailableRange(unsigned long minimum_address,
                                             unsigned long maximum_address,
                                             unsigned long length,
                                             unsigned long alignment,
                                             unsigned long *base_address)
{
    vm_address_t scratch_addresses[ORLIX_HOST_VM_SCRATCH_MAX] = {0};
    vm_size_t scratch_lengths[ORLIX_HOST_VM_SCRATCH_MAX] = {0};
    unsigned int scratch_count = 0;
    unsigned long scratch_bytes = 0;
    unsigned int allocation_failures = 0;
    unsigned int user_collisions = 0;
    unsigned int unusable_candidates = 0;
    unsigned int protect_failures = 0;
    kern_return_t last_allocate_status = KERN_SUCCESS;
    kern_return_t last_protect_status = KERN_SUCCESS;

    if (!base_address || length == 0 || alignment == 0 ||
        (alignment & (alignment - 1UL)) != 0) {
        return -1;
    }

    for (unsigned int attempt = 0; attempt < ORLIX_HOST_VM_ANYWHERE_ATTEMPTS; attempt++) {
        vm_address_t target = 0;
        vm_size_t reservation_length;
        kern_return_t status = vm_allocate(mach_task_self(),
                                           &target,
                                           (vm_size_t)length + alignment,
                                           ORLIX_HOST_VM_ANYWHERE_FLAGS);
        unsigned long reservation_start = (unsigned long)target;
        unsigned long address;
        unsigned long reservation_end;
        unsigned long selected_end;

        if (status != KERN_SUCCESS || target == 0) {
            allocation_failures++;
            last_allocate_status = status;
            continue;
        }

        if (length > (unsigned long)-1 - alignment ||
            reservation_start > (unsigned long)-1 - length - alignment) {
            unusable_candidates++;
            (void)vm_deallocate(mach_task_self(), target, (vm_size_t)length + alignment);
            continue;
        }

        reservation_length = (vm_size_t)length + alignment;
        reservation_end = reservation_start + reservation_length;
        address = OrlixHostAlignUp(reservation_start, alignment);
        selected_end = address + length;

        if (address == 0 || selected_end > reservation_end ||
            OrlixHostRangeIntersects(address,
                                     length,
                                     OrlixHostHostedUserBase,
                                     OrlixHostHostedStackTop)) {
            if (OrlixHostRangeIntersects(address,
                                         length,
                                         OrlixHostHostedUserBase,
                                         OrlixHostHostedStackTop)) {
                user_collisions++;
            } else {
                unusable_candidates++;
            }
            if (scratch_count < ORLIX_HOST_VM_SCRATCH_MAX &&
                scratch_bytes <= ORLIX_HOST_VM_SCRATCH_MAX_BYTES &&
                reservation_length <= ORLIX_HOST_VM_SCRATCH_MAX_BYTES - scratch_bytes) {
                scratch_addresses[scratch_count] = target;
                scratch_lengths[scratch_count] = reservation_length;
                scratch_count++;
                scratch_bytes += reservation_length;
            } else {
                (void)vm_deallocate(mach_task_self(), target, reservation_length);
            }
            continue;
        }

        if (address > reservation_start) {
            (void)vm_deallocate(mach_task_self(),
                                (vm_address_t)reservation_start,
                                (vm_size_t)(address - reservation_start));
        }
        if (selected_end < reservation_end) {
            (void)vm_deallocate(mach_task_self(),
                                (vm_address_t)selected_end,
                                (vm_size_t)(reservation_end - selected_end));
        }

        status = vm_protect(mach_task_self(),
                            (vm_address_t)address,
                            (vm_size_t)length,
                            false,
                            VM_PROT_NONE);
        if (status != KERN_SUCCESS) {
            protect_failures++;
            last_protect_status = status;
            if (scratch_count < ORLIX_HOST_VM_SCRATCH_MAX &&
                scratch_bytes <= ORLIX_HOST_VM_SCRATCH_MAX_BYTES &&
                length <= ORLIX_HOST_VM_SCRATCH_MAX_BYTES - scratch_bytes) {
                scratch_addresses[scratch_count] = (vm_address_t)address;
                scratch_lengths[scratch_count] = (vm_size_t)length;
                scratch_count++;
                scratch_bytes += length;
            } else {
                (void)vm_deallocate(mach_task_self(),
                                    (vm_address_t)address,
                                    (vm_size_t)length);
            }
            continue;
        }

        *base_address = address;
        for (unsigned int index = 0; index < scratch_count; index++) {
            (void)vm_deallocate(mach_task_self(),
                                scratch_addresses[index],
                                scratch_lengths[index]);
        }
#if DEBUG || ORLIX_BETA_OBSERVABILITY
        orlix_host_trace_printf(ORLIX_HOST_TRACE_CATEGORY_HOST_VM,
                                ORLIX_HOST_TRACE_LEVEL_INFO,
                                ORLIX_HOST_TRACE_SINK_OS_LOG |
                                    ORLIX_HOST_TRACE_SINK_STDERR,
                                "anywhere reservation selected base=0x%lx length=0x%lx preferredStart=0x%lx preferredEnd=0x%lx alignment=0x%lx attempts=%u allocationFailures=%u collisions=%u unusable=%u protectFailures=%u scratchBytes=0x%lx flags=0x%x",
                                address,
                                length,
                                minimum_address,
                                maximum_address,
                                alignment,
                                attempt + 1,
                                allocation_failures,
                                user_collisions,
                                unusable_candidates,
                                protect_failures,
                                scratch_bytes,
                                ORLIX_HOST_VM_ANYWHERE_FLAGS);
#endif
        return 0;
    }

    for (unsigned int index = 0; index < scratch_count; index++) {
        (void)vm_deallocate(mach_task_self(),
                            scratch_addresses[index],
                            scratch_lengths[index]);
    }

#if DEBUG || ORLIX_BETA_OBSERVABILITY
    orlix_host_trace_printf(ORLIX_HOST_TRACE_CATEGORY_HOST_VM,
                            ORLIX_HOST_TRACE_LEVEL_ERROR,
                            ORLIX_HOST_TRACE_SINK_OS_LOG |
                                ORLIX_HOST_TRACE_SINK_STDERR,
                            "anywhere reservation failed length=0x%lx preferredStart=0x%lx preferredEnd=0x%lx alignment=0x%lx attempts=%u allocationFailures=%u collisions=%u unusable=%u protectFailures=%u lastAllocateStatus=%d lastProtectStatus=%d flags=0x%x",
                            length,
                            minimum_address,
                            maximum_address,
                            alignment,
                            ORLIX_HOST_VM_ANYWHERE_ATTEMPTS,
                            allocation_failures,
                            user_collisions,
                            unusable_candidates,
                            protect_failures,
                            last_allocate_status,
                            last_protect_status,
                            ORLIX_HOST_VM_ANYWHERE_FLAGS);
#endif
    return -1;
}

static int OrlixHostAllocateIOMapping(vm_size_t length, vm_address_t *mapped)
{
    vm_address_t scratch_addresses[ORLIX_HOST_VM_SCRATCH_MAX] = {0};
    vm_size_t scratch_lengths[ORLIX_HOST_VM_SCRATCH_MAX] = {0};
    unsigned int scratch_count = 0;
    unsigned long scratch_bytes = 0;
    unsigned int collisions = 0;
    unsigned int allocation_failures = 0;
    kern_return_t last_allocate_status = KERN_SUCCESS;

    if (!mapped || length == 0 || length > ORLIX_HOST_IOMEM_MAX_SIZE) {
        return -1;
    }

    for (unsigned int attempt = 0; attempt < ORLIX_HOST_VM_ANYWHERE_ATTEMPTS; attempt++) {
        vm_address_t target = 0;
        kern_return_t status = vm_allocate(mach_task_self(),
                                           &target,
                                           length,
                                           ORLIX_HOST_VM_ANYWHERE_FLAGS);

        if (status != KERN_SUCCESS || target == 0) {
            allocation_failures++;
            last_allocate_status = status;
            continue;
        }

        if (OrlixHostRangeIntersects((unsigned long)target,
                                     length,
                                     OrlixHostHostedUserBase,
                                     OrlixHostHostedStackTop) ||
            OrlixHostRangeIntersects((unsigned long)target,
                                     length,
                                     OrlixHostHostedStackTop,
                                     OrlixHostHostedKernelMax)) {
            collisions++;
            if (scratch_count < ORLIX_HOST_VM_SCRATCH_MAX &&
                scratch_bytes <= ORLIX_HOST_VM_SCRATCH_MAX_BYTES &&
                length <= ORLIX_HOST_VM_SCRATCH_MAX_BYTES - scratch_bytes) {
                scratch_addresses[scratch_count] = target;
                scratch_lengths[scratch_count] = length;
                scratch_count++;
                scratch_bytes += length;
            } else {
                (void)vm_deallocate(mach_task_self(), target, length);
            }
            continue;
        }

        *mapped = target;
        for (unsigned int index = 0; index < scratch_count; index++) {
            (void)vm_deallocate(mach_task_self(),
                                scratch_addresses[index],
                                scratch_lengths[index]);
        }
#if DEBUG || ORLIX_BETA_OBSERVABILITY
        orlix_host_trace_printf(ORLIX_HOST_TRACE_CATEGORY_HOST_VM,
                                ORLIX_HOST_TRACE_LEVEL_INFO,
                                ORLIX_HOST_TRACE_SINK_OS_LOG |
                                    ORLIX_HOST_TRACE_SINK_STDERR,
                                "iomem reservation selected base=0x%lx length=0x%lx attempts=%u allocationFailures=%u collisions=%u scratchBytes=0x%lx flags=0x%x",
                                (unsigned long)target,
                                (unsigned long)length,
                                attempt + 1,
                                allocation_failures,
                                collisions,
                                scratch_bytes,
                                ORLIX_HOST_VM_ANYWHERE_FLAGS);
#endif
        return 0;
    }

    for (unsigned int index = 0; index < scratch_count; index++) {
        (void)vm_deallocate(mach_task_self(),
                            scratch_addresses[index],
                            scratch_lengths[index]);
    }

#if DEBUG || ORLIX_BETA_OBSERVABILITY
    orlix_host_trace_printf(ORLIX_HOST_TRACE_CATEGORY_HOST_VM,
                            ORLIX_HOST_TRACE_LEVEL_ERROR,
                            ORLIX_HOST_TRACE_SINK_OS_LOG |
                                ORLIX_HOST_TRACE_SINK_STDERR,
                            "iomem reservation failed length=0x%lx attempts=%u allocationFailures=%u collisions=%u lastAllocateStatus=%d flags=0x%x",
                            (unsigned long)length,
                            ORLIX_HOST_VM_ANYWHERE_ATTEMPTS,
                            allocation_failures,
                            collisions,
                            last_allocate_status,
                            ORLIX_HOST_VM_ANYWHERE_FLAGS);
#endif

    return -1;
}

static void OrlixHostUnmapPages(unsigned long target_address,
                                unsigned long length);
static void OrlixHostUserRemoveSegmentsInRange(
    struct OrlixHostUserMapping *mapping,
    unsigned long target_address,
    unsigned long length,
    bool copy_back_replaced_segments);
static struct OrlixHostUserMapping *OrlixHostUserFindMapping(
    unsigned long target_address,
    unsigned long length,
    vm_prot_t protection,
    bool writable);
static int OrlixHostUserCreateMapping(unsigned long target_address,
                                      unsigned long length,
                                      vm_prot_t protection,
                                      bool writable,
                                      struct OrlixHostUserMapping **out_mapping);

static unsigned long OrlixHostPageStart(unsigned long address)
{
    unsigned long page_size = orlix_host_memory_page_size();

    return address & ~(page_size - 1UL);
}

static unsigned long OrlixHostPageEnd(unsigned long address,
                                      unsigned long length)
{
    unsigned long page_size = orlix_host_memory_page_size();
    unsigned long end;

    if (length == 0 || address > (unsigned long)-1 - length) {
        return 0;
    }

    end = address + length;
    if (end > (unsigned long)-1 - (page_size - 1UL)) {
        return 0;
    }

    return (end + page_size - 1UL) & ~(page_size - 1UL);
}

static int OrlixHostMapPageWithProtection(unsigned long target_address,
                                          const void *source_page,
                                          unsigned long length,
                                          vm_prot_t requested_protection)
{
    vm_address_t target = (vm_address_t)target_address;
    vm_prot_t current_protection = VM_PROT_NONE;
    vm_prot_t maximum_protection = VM_PROT_NONE;
    kern_return_t status;

    if (target_address == 0 || !source_page || length == 0) {
        return -1;
    }

    status = vm_remap(mach_task_self(),
                      &target,
                      (vm_size_t)length,
                      0,
                      VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE,
                      mach_task_self(),
                      (vm_address_t)source_page,
                      false,
                      &current_protection,
                      &maximum_protection,
                      VM_INHERIT_NONE);
    if (status != KERN_SUCCESS || target != (vm_address_t)target_address) {
        return -1;
    }

    if (requested_protection != VM_PROT_NONE) {
        status = vm_protect(mach_task_self(),
                            target,
                            (vm_size_t)length,
                            false,
                            requested_protection);
        if (status != KERN_SUCCESS) {
            (void)vm_deallocate(mach_task_self(), target, (vm_size_t)length);
            return -1;
        }
    }

    return 0;
}

static void OrlixHostKernelCopyBackMapping(
    struct OrlixHostKernelShadowMapping *mapping)
{
    if (!mapping) {
        return;
    }

    for (struct OrlixHostKernelShadowSegment *segment = mapping->segments;
         segment;
         segment = segment->next) {
        if (!segment->source_page || segment->length == 0) {
            continue;
        }

        memcpy((void *)segment->source_page,
               (const void *)segment->target_address,
               (size_t)segment->length);
    }
}

static void OrlixHostKernelFreeShadowSegments(
    struct OrlixHostKernelShadowMapping *mapping)
{
    if (!mapping) {
        return;
    }

    while (mapping->segments) {
        struct OrlixHostKernelShadowSegment *segment = mapping->segments;

        mapping->segments = segment->next;
        free(segment);
    }
}

static void OrlixHostKernelRemoveShadowSegmentsInRange(
    struct OrlixHostKernelShadowMapping *mapping,
    unsigned long target_address,
    unsigned long length)
{
    unsigned long end;
    struct OrlixHostKernelShadowSegment **link;

    if (!mapping || target_address == 0 || length == 0 ||
        target_address > (unsigned long)-1 - length) {
        return;
    }

    end = target_address + length;
    link = &mapping->segments;
    while (*link) {
        struct OrlixHostKernelShadowSegment *segment = *link;
        unsigned long segment_end = segment->target_address + segment->length;

        if (target_address < segment_end && segment->target_address < end) {
            memcpy((void *)segment->source_page,
                   (const void *)segment->target_address,
                   (size_t)segment->length);
            *link = segment->next;
            free(segment);
            continue;
        }

        link = &segment->next;
    }
}

static void OrlixHostKernelUnmapShadowRange(unsigned long target_address,
                                            unsigned long length)
{
    unsigned long start = OrlixHostPageStart(target_address);
    unsigned long end = OrlixHostPageEnd(target_address, length);
    struct OrlixHostKernelShadowMapping **link =
        &OrlixHostKernelShadowMappings;

    if (target_address == 0 || end == 0 || end <= start) {
        return;
    }

    while (*link) {
        struct OrlixHostKernelShadowMapping *mapping = *link;
        unsigned long mapping_end =
            mapping->target_address + mapping->length;

        if (start < mapping_end && mapping->target_address < end) {
            OrlixHostKernelCopyBackMapping(mapping);
            OrlixHostKernelFreeShadowSegments(mapping);
            (void)vm_deallocate(mach_task_self(),
                                (vm_address_t)mapping->target_address,
                                (vm_size_t)mapping->length);
            *link = mapping->next;
            free(mapping);
            continue;
        }

        link = &mapping->next;
    }
}

static struct OrlixHostKernelShadowMapping *
OrlixHostKernelFindShadowMapping(unsigned long target_address,
                                 unsigned long length)
{
    unsigned long start = OrlixHostPageStart(target_address);
    unsigned long end = OrlixHostPageEnd(target_address, length);

    if (target_address == 0 || end == 0 || end <= start) {
        return NULL;
    }

    for (struct OrlixHostKernelShadowMapping *mapping =
             OrlixHostKernelShadowMappings;
         mapping;
         mapping = mapping->next) {
        unsigned long mapping_end =
            mapping->target_address + mapping->length;

        if (mapping->target_address <= target_address &&
            mapping_end >= target_address + length) {
            return mapping;
        }
    }

    return NULL;
}

static int OrlixHostKernelCreateShadowMapping(unsigned long target_address,
                                              unsigned long length,
                                              struct OrlixHostKernelShadowMapping **out_mapping)
{
    unsigned long start = OrlixHostPageStart(target_address);
    unsigned long end = OrlixHostPageEnd(target_address, length);
    vm_address_t target = (vm_address_t)start;
    struct OrlixHostKernelShadowMapping *mapping;
    kern_return_t status;

    if (!out_mapping || target_address == 0 || end == 0 || end <= start) {
        return -1;
    }

    OrlixHostKernelUnmapShadowRange(start, end - start);
    OrlixHostUnmapPages(start, end - start);

    status = vm_allocate(mach_task_self(),
                         &target,
                         (vm_size_t)(end - start),
                         VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE);
    if (status != KERN_SUCCESS || target != (vm_address_t)start) {
        return -1;
    }

    mapping = malloc(sizeof(*mapping));
    if (!mapping) {
        (void)vm_deallocate(mach_task_self(), target, (vm_size_t)(end - start));
        return -1;
    }

    mapping->target_address = start;
    mapping->length = (vm_size_t)(end - start);
    mapping->segments = NULL;
    mapping->next = OrlixHostKernelShadowMappings;
    OrlixHostKernelShadowMappings = mapping;
    *out_mapping = mapping;
    return 0;
}

__attribute__((visibility("hidden"))) int orlix_host_kernel_reserve_window(
    unsigned long minimum_address,
    unsigned long maximum_address,
    unsigned long length,
    unsigned long alignment,
    unsigned long *base_address)
{
    mach_port_t task = mach_task_self();
    vm_address_t cursor;
    unsigned long active_tls;

    if (!base_address || minimum_address == 0 || maximum_address <= minimum_address ||
        length == 0 || length > maximum_address - minimum_address ||
        alignment == 0 || (alignment & (alignment - 1UL)) != 0) {
        return -1;
    }

    *base_address = 0;
    cursor = (vm_address_t)OrlixHostAlignUp(minimum_address, alignment);
    if (cursor == 0 || cursor > maximum_address - length) {
        return -1;
    }

    active_tls = OrlixHostEnterHostTls();
    while (cursor <= (vm_address_t)(maximum_address - length)) {
        vm_address_t region_address = cursor;
        vm_size_t region_size = 0;
        vm_region_basic_info_data_64_t info;
        mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
        memory_object_name_t object_name = MACH_PORT_NULL;
        kern_return_t status;
        vm_address_t region_end;

        status = vm_region_64(task,
                              &region_address,
                              &region_size,
                              VM_REGION_BASIC_INFO_64,
                              (vm_region_info_t)&info,
                              &count,
                              &object_name);
        if (object_name != MACH_PORT_NULL) {
            mach_port_deallocate(task, object_name);
        }
        if (status != KERN_SUCCESS || region_address >= maximum_address) {
            if (OrlixHostReserveFirstAvailableRangeInGap((unsigned long)cursor,
                                                         maximum_address,
                                                         length,
                                                         alignment,
                                                         base_address) == 0) {
                OrlixHostLeaveHostTls(active_tls);
                return 0;
            }
            break;
        }

        if (region_address > cursor &&
            OrlixHostReserveFirstAvailableRangeInGap((unsigned long)cursor,
                                                     (unsigned long)region_address,
                                                     length,
                                                     alignment,
                                                     base_address) == 0) {
            OrlixHostLeaveHostTls(active_tls);
            return 0;
        }

        if (region_size > (vm_size_t)-1 - region_address) {
            break;
        }
        region_end = region_address + region_size;
        if (region_end <= cursor) {
            break;
        }
        cursor = (vm_address_t)OrlixHostAlignUp((unsigned long)region_end,
                                                alignment);
        if (cursor == 0) {
            break;
        }
    }

    if (OrlixHostReserveAnyAvailableRange(minimum_address,
                                          maximum_address,
                                          length,
                                          alignment,
                                          base_address) == 0) {
        OrlixHostLeaveHostTls(active_tls);
        return 0;
    }

    OrlixHostLeaveHostTls(active_tls);
    return -1;
}

__attribute__((visibility("hidden"))) int orlix_host_user_reserve_window(
    unsigned long length,
    unsigned long alignment,
    unsigned long *base_address,
    unsigned long *limit_address)
{
    unsigned long base = 0;
    unsigned long active_tls;
    int result;

    if (!base_address || !limit_address || length == 0 ||
        alignment == 0 || length > (unsigned long)-1 - alignment) {
        return -1;
    }

    if (OrlixHostHostedUserBase != ORLIX_HOST_HOSTED_USER_BASE_DEFAULT ||
        OrlixHostHostedStackTop != ORLIX_HOST_HOSTED_STACK_TOP_DEFAULT) {
        *base_address = OrlixHostHostedUserBase;
        *limit_address = OrlixHostHostedStackTop;
        return 0;
    }

    active_tls = OrlixHostEnterHostTls();
    result = OrlixHostReserveAnyAvailableRange(0,
                                               (unsigned long)-1,
                                               length,
                                               alignment,
                                               &base);
    OrlixHostLeaveHostTls(active_tls);

    if (result != 0 || base == 0 || length > ((unsigned long)-1 / 2UL) ||
        base > (unsigned long)-1 - (length * 2UL)) {
        return -1;
    }

    OrlixHostHostedUserBase = base;
    OrlixHostHostedStackTop = base + length;
    OrlixHostHostedKernelMax = OrlixHostHostedStackTop + length;
    OrlixHostHostedUserWindowReserved = true;
    *base_address = OrlixHostHostedUserBase;
    *limit_address = OrlixHostHostedStackTop;

#if DEBUG || ORLIX_BETA_OBSERVABILITY
    orlix_host_trace_printf(ORLIX_HOST_TRACE_CATEGORY_HOST_VM,
                            ORLIX_HOST_TRACE_LEVEL_INFO,
                            ORLIX_HOST_TRACE_SINK_OS_LOG |
                                ORLIX_HOST_TRACE_SINK_STDERR,
                            "hosted user window selected base=0x%lx limit=0x%lx length=0x%lx alignment=0x%lx",
                            OrlixHostHostedUserBase,
                            OrlixHostHostedStackTop,
                            length,
                            alignment);
#endif
    return 0;
}

static int OrlixHostMapShadowKernelPage(unsigned long target_address,
                                        const void *source_page,
                                        unsigned long length)
{
    struct OrlixHostKernelShadowMapping *mapping;
    struct OrlixHostKernelShadowSegment *segment;
    unsigned long offset;

    if (target_address == 0 || !source_page || length == 0 ||
        target_address > (unsigned long)-1 - length) {
        return -1;
    }

    mapping = OrlixHostKernelFindShadowMapping(target_address, length);
    if (!mapping &&
        OrlixHostKernelCreateShadowMapping(target_address,
                                           length,
                                           &mapping) != 0) {
        return -1;
    }
    if (!mapping) {
        return -1;
    }

    offset = target_address - mapping->target_address;
    OrlixHostKernelRemoveShadowSegmentsInRange(mapping,
                                               target_address,
                                               length);
    memcpy((void *)(mapping->target_address + offset),
           source_page,
           (size_t)length);

    segment = malloc(sizeof(*segment));
    if (!segment) {
        return -1;
    }
    segment->target_address = target_address;
    segment->source_page = source_page;
    segment->length = (vm_size_t)length;
    segment->next = mapping->segments;
    mapping->segments = segment;
    return 0;
}

static bool OrlixHostKernelRequiresShadowMapping(unsigned long target_address,
                                                 const void *source_page,
                                                 unsigned long length)
{
    unsigned long page_size = orlix_host_memory_page_size();

    return length != page_size ||
           (target_address & (page_size - 1UL)) != 0 ||
           ((unsigned long)source_page & (page_size - 1UL)) != 0;
}

static int OrlixHostMapShadowUserPages(unsigned long target_address,
                                       const void *source_page,
                                       unsigned long length,
                                       vm_prot_t requested_protection,
                                       bool executable,
                                       bool translate_executable,
                                       bool copy_back_replaced_segments)
{
    struct OrlixHostUserMapping *mapping;
    struct OrlixHostKernelShadowSegment *segment;
    unsigned long offset;
    vm_prot_t copy_protection = VM_PROT_READ | VM_PROT_WRITE;
    kern_return_t status;

    if (target_address == 0 || !source_page || length == 0 ||
        target_address > (unsigned long)-1 - length) {
        OrlixHostTraceShadowUserPageFailure("invalid-argument",
                                            target_address,
                                            length,
                                            0,
                                            0,
                                            requested_protection,
                                            copy_protection,
                                            KERN_INVALID_ARGUMENT);
        return -1;
    }

    mapping = OrlixHostUserFindMapping(target_address,
                                       length,
                                       requested_protection,
                                       requested_protection & VM_PROT_WRITE);
    if (!mapping &&
        OrlixHostUserCreateMapping(target_address,
                                   length,
                                   requested_protection,
                                   requested_protection & VM_PROT_WRITE,
                                   &mapping) != 0) {
        OrlixHostTraceShadowUserPageFailure("create-mapping",
                                            target_address,
                                            length,
                                            0,
                                            0,
                                            requested_protection,
                                            copy_protection,
                                            KERN_FAILURE);
        return -1;
    }
    if (!mapping) {
        OrlixHostTraceShadowUserPageFailure("missing-mapping",
                                            target_address,
                                            length,
                                            0,
                                            0,
                                            requested_protection,
                                            copy_protection,
                                            KERN_FAILURE);
        return -1;
    }

    status = vm_protect(mach_task_self(),
                        (vm_address_t)mapping->target_address,
                        mapping->length,
                        false,
                        copy_protection);
    if (status != KERN_SUCCESS) {
        OrlixHostTraceShadowUserPageFailure("copy-protect",
                                            target_address,
                                            length,
                                            mapping->target_address,
                                            mapping->length,
                                            requested_protection,
                                            copy_protection,
                                            status);
        return -1;
    }

    offset = target_address - mapping->target_address;
    OrlixHostUserRemoveSegmentsInRange(mapping,
                                       target_address,
                                       length,
                                       copy_back_replaced_segments);
    memcpy((void *)(mapping->target_address + offset),
           source_page,
           (size_t)length);
    (void)translate_executable;

    if (executable) {
        OrlixHostInvalidateInstructionCache(mapping->target_address + offset,
                                            length);
    }

    segment = malloc(sizeof(*segment));
    if (!segment) {
        OrlixHostTraceShadowUserPageFailure("segment-alloc",
                                            target_address,
                                            length,
                                            mapping->target_address,
                                            mapping->length,
                                            requested_protection,
                                            copy_protection,
                                            KERN_RESOURCE_SHORTAGE);
        return -1;
    }
    segment->target_address = target_address;
    segment->source_page = source_page;
    segment->length = (vm_size_t)length;
    segment->next = mapping->segments;
    mapping->segments = segment;

    status = vm_protect(mach_task_self(),
                        (vm_address_t)mapping->target_address,
                        mapping->length,
                        false,
                        requested_protection);
    if (status != KERN_SUCCESS) {
        OrlixHostTraceShadowUserPageFailure("final-protect",
                                            target_address,
                                            length,
                                            mapping->target_address,
                                            mapping->length,
                                            requested_protection,
                                            copy_protection,
                                            status);
        return -1;
    }

    mapping->protection = requested_protection;
    mapping->writable = !!(requested_protection & VM_PROT_WRITE);
    return 0;
}

static void OrlixHostUnmapPages(unsigned long target_address,
                                unsigned long length)
{
    unsigned long start = OrlixHostPageStart(target_address);
    unsigned long end = OrlixHostPageEnd(target_address, length);

    if (target_address == 0 || length == 0 || end == 0 || end <= start) {
        return;
    }

    (void)vm_deallocate(mach_task_self(),
                        (vm_address_t)start,
                        (vm_size_t)(end - start));
}

static bool OrlixHostUserMappingMatches(unsigned long target_address,
                                        const void *source_page,
                                        unsigned long length,
                                        vm_prot_t protection,
                                        bool writable)
{
    for (struct OrlixHostUserMapping *mapping = OrlixHostUserMappings;
         mapping;
         mapping = mapping->next) {
        unsigned long mapping_end = mapping->target_address + mapping->length;

        if (mapping->target_address <= target_address &&
            mapping_end >= target_address + length &&
            mapping->protection == protection &&
            mapping->writable == writable) {
            for (struct OrlixHostKernelShadowSegment *segment =
                     mapping->segments;
                 segment;
                 segment = segment->next) {
                if (segment->target_address == target_address &&
                    segment->source_page == source_page &&
                    segment->length == length) {
                    return true;
                }
            }
        }
    }

    return false;
}

static void OrlixHostUserCopyBackMapping(struct OrlixHostUserMapping *mapping)
{
    if (!mapping || !mapping->writable) {
        return;
    }

    OrlixHostUserMemoryBarrier();
    for (struct OrlixHostKernelShadowSegment *segment = mapping->segments;
         segment;
         segment = segment->next) {
        if (!segment->source_page || segment->length == 0) {
            continue;
        }

        memcpy((void *)segment->source_page,
               (const void *)segment->target_address,
               (size_t)segment->length);
    }
    OrlixHostUserMemoryBarrier();
}

static void OrlixHostUserFreeSegments(struct OrlixHostUserMapping *mapping)
{
    if (!mapping) {
        return;
    }

    while (mapping->segments) {
        struct OrlixHostKernelShadowSegment *segment = mapping->segments;

        mapping->segments = segment->next;
        free(segment);
    }
}

static void OrlixHostUserRemoveSegmentsInRange(
    struct OrlixHostUserMapping *mapping,
    unsigned long target_address,
    unsigned long length,
    bool copy_back_replaced_segments)
{
    unsigned long end;
    struct OrlixHostKernelShadowSegment **link;

    if (!mapping || target_address == 0 || length == 0 ||
        target_address > (unsigned long)-1 - length) {
        return;
    }

    end = target_address + length;
    link = &mapping->segments;
    while (*link) {
        struct OrlixHostKernelShadowSegment *segment = *link;
        unsigned long segment_end = segment->target_address + segment->length;

        if (target_address < segment_end && segment->target_address < end) {
            if (copy_back_replaced_segments &&
                mapping->writable &&
                segment->source_page) {
                memcpy((void *)segment->source_page,
                       (const void *)segment->target_address,
                       (size_t)segment->length);
            }
            *link = segment->next;
            free(segment);
            continue;
        }

        link = &segment->next;
    }
}

static void OrlixHostUserUnmapMappedRange(unsigned long target_address,
                                          unsigned long length)
{
    unsigned long start = OrlixHostPageStart(target_address);
    unsigned long end = OrlixHostPageEnd(target_address, length);
    struct OrlixHostUserMapping **link = &OrlixHostUserMappings;

    if (target_address == 0 || end == 0 || end <= start) {
        return;
    }

    while (*link) {
        struct OrlixHostUserMapping *mapping = *link;
        unsigned long mapping_end = mapping->target_address + mapping->length;

        if (start < mapping_end && mapping->target_address < end) {
            bool uses_hosted_user_reservation =
                OrlixHostRangeInsideHostedUserWindow(mapping->target_address,
                                                     mapping_end);

            OrlixHostUserCopyBackMapping(mapping);
            OrlixHostUserFreeSegments(mapping);
            if (uses_hosted_user_reservation) {
                (void)vm_protect(mach_task_self(),
                                 (vm_address_t)mapping->target_address,
                                 mapping->length,
                                 false,
                                 VM_PROT_NONE);
            } else {
                (void)vm_deallocate(mach_task_self(),
                                    (vm_address_t)mapping->target_address,
                                    (vm_size_t)mapping->length);
            }
            *link = mapping->next;
            free(mapping);
            continue;
        }

        link = &mapping->next;
    }
}

static struct OrlixHostUserMapping *OrlixHostUserFindMapping(
    unsigned long target_address,
    unsigned long length,
    vm_prot_t protection,
    bool writable)
{
    (void)protection;
    (void)writable;

    for (struct OrlixHostUserMapping *mapping = OrlixHostUserMappings;
         mapping;
         mapping = mapping->next) {
        unsigned long mapping_end = mapping->target_address + mapping->length;

        if (mapping->target_address <= target_address &&
            mapping_end >= target_address + length) {
            return mapping;
        }
    }

    return NULL;
}

static int OrlixHostUserCreateMapping(unsigned long target_address,
                                      unsigned long length,
                                      vm_prot_t protection,
                                      bool writable,
                                      struct OrlixHostUserMapping **out_mapping)
{
    unsigned long start = OrlixHostPageStart(target_address);
    unsigned long end = OrlixHostPageEnd(target_address, length);
    vm_address_t target = (vm_address_t)start;
    struct OrlixHostUserMapping *mapping;
    bool uses_hosted_user_reservation;
    kern_return_t status;

    if (!out_mapping || target_address == 0 || end == 0 || end <= start) {
        return -1;
    }

    uses_hosted_user_reservation =
        OrlixHostRangeInsideHostedUserWindow(start, end);

    OrlixHostUserUnmapMappedRange(start, end - start);
    if (uses_hosted_user_reservation) {
        status = vm_protect(mach_task_self(),
                            target,
                            (vm_size_t)(end - start),
                            false,
                            VM_PROT_READ | VM_PROT_WRITE);
        if (status != KERN_SUCCESS) {
            OrlixHostTraceShadowUserPageFailure("create-reservation-protect",
                                                target_address,
                                                length,
                                                start,
                                                end - start,
                                                protection,
                                                VM_PROT_READ | VM_PROT_WRITE,
                                                status);
            return -1;
        }
    } else {
        OrlixHostUnmapPages(start, end - start);
        status = vm_allocate(mach_task_self(),
                             &target,
                             (vm_size_t)(end - start),
                             VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE);
        if (status != KERN_SUCCESS || target != (vm_address_t)start) {
            OrlixHostTraceShadowUserPageFailure("create-fixed-allocate",
                                                target_address,
                                                length,
                                                start,
                                                end - start,
                                                protection,
                                                VM_PROT_READ | VM_PROT_WRITE,
                                                status);
            return -1;
        }
    }

    mapping = malloc(sizeof(*mapping));
    if (!mapping) {
        if (uses_hosted_user_reservation) {
            (void)vm_protect(mach_task_self(),
                             target,
                             (vm_size_t)(end - start),
                             false,
                             VM_PROT_NONE);
        } else {
            (void)vm_deallocate(mach_task_self(),
                                target,
                                (vm_size_t)(end - start));
        }
        return -1;
    }

    mapping->target_address = start;
    mapping->length = (vm_size_t)(end - start);
    mapping->protection = protection;
    mapping->writable = writable;
    mapping->segments = NULL;
    mapping->next = OrlixHostUserMappings;
    OrlixHostUserMappings = mapping;
    *out_mapping = mapping;
    return 0;
}

__attribute__((visibility("hidden"))) int orlix_host_kernel_map_page(
    unsigned long target_address,
    const void *source_page,
    unsigned long length)
{
    unsigned long active_tls = OrlixHostEnterHostTls();
    int result;

    if (OrlixHostKernelRequiresShadowMapping(target_address, source_page, length) ||
        OrlixHostKernelFindShadowMapping(target_address, length)) {
        result = OrlixHostMapShadowKernelPage(target_address,
                                             source_page,
                                             length);
    } else {
        result = OrlixHostMapPageWithProtection(target_address,
                                                source_page,
                                                length,
                                                VM_PROT_NONE);
        if (result != 0) {
            result = OrlixHostMapShadowKernelPage(target_address,
                                                 source_page,
                                                 length);
        }
    }

    OrlixHostLeaveHostTls(active_tls);
    return result;
}

__attribute__((visibility("hidden"))) void orlix_host_kernel_unmap_pages(
    unsigned long target_address,
    unsigned long length)
{
    unsigned long active_tls = OrlixHostEnterHostTls();

    OrlixHostKernelUnmapShadowRange(target_address, length);
    OrlixHostUnmapPages(target_address, length);
    OrlixHostLeaveHostTls(active_tls);
}

__attribute__((visibility("hidden"))) int orlix_host_user_map_page(
    unsigned long target_address,
    const void *source_page,
    unsigned long length,
    int writable,
    int executable)
{
    vm_prot_t protection = VM_PROT_READ;

    if (executable) {
        return -1;
    }
    if (writable) {
        protection |= VM_PROT_WRITE;
    }
    unsigned long active_tls = OrlixHostEnterHostTls();
    int result;

    if (!executable &&
        OrlixHostUserMappingMatches(target_address,
                                    source_page,
                                    length,
                                    protection,
                                    writable != 0)) {
        OrlixHostLeaveHostTls(active_tls);
        return 0;
    }

    result = OrlixHostMapShadowUserPages(target_address,
                                         source_page,
                                         length,
                                         protection,
                                         executable != 0,
                                         true,
                                         true);
    OrlixHostLeaveHostTls(active_tls);
    return result;
}

__attribute__((visibility("hidden"))) int orlix_host_user_map_trusted_executable_page(
    unsigned long target_address,
    const void *source_page,
    unsigned long length)
{
    unsigned long active_tls = OrlixHostEnterHostTls();
    int result = OrlixHostMapShadowUserPages(target_address,
                                             source_page,
                                             length,
                                             VM_PROT_READ | VM_PROT_EXECUTE,
                                             true,
                                             false,
                                             true);
    OrlixHostLeaveHostTls(active_tls);
    return result;
}

__attribute__((visibility("hidden"))) int orlix_host_user_refresh_page(
    unsigned long target_address,
    const void *source_page,
    unsigned long length,
    int writable,
    int executable)
{
    vm_prot_t protection = VM_PROT_READ;
    unsigned long active_tls;
    int result;

    if (executable) {
        return -1;
    }
    if (writable) {
        protection |= VM_PROT_WRITE;
    }
    active_tls = OrlixHostEnterHostTls();
    result = OrlixHostMapShadowUserPages(target_address,
                                         source_page,
                                         length,
                                         protection,
                                         executable != 0,
                                         true,
                                         false);
    OrlixHostLeaveHostTls(active_tls);
    return result;
}

__attribute__((visibility("hidden"))) int orlix_host_user_refresh_window(
    unsigned long target_address,
    unsigned long length,
    const struct orlix_host_user_page_segment *segments,
    unsigned long segment_count)
{
    vm_prot_t protection = VM_PROT_READ;
    vm_prot_t copy_protection = VM_PROT_READ | VM_PROT_WRITE;
    unsigned long end;
    unsigned long active_tls;
    struct OrlixHostUserMapping *mapping;
    kern_return_t status;
    bool writable = false;
    bool executable = false;

    if (target_address == 0 || length == 0 || !segments ||
        segment_count == 0 || target_address > (unsigned long)-1 - length) {
        return -1;
    }

    end = target_address + length;
    for (unsigned long index = 0; index < segment_count; index++) {
        const struct orlix_host_user_page_segment *segment = &segments[index];
        unsigned long segment_end;

        if (!segment->source_page || segment->length == 0 ||
            segment->target_address < target_address ||
            segment->target_address > (unsigned long)-1 - segment->length) {
            return -1;
        }
        segment_end = segment->target_address + segment->length;
        if (segment_end > end) {
            return -1;
        }
        if (segment->writable) {
            writable = true;
        }
        if (segment->executable) {
            executable = true;
        }
    }

    if (writable) {
        protection |= VM_PROT_WRITE;
    }
    active_tls = OrlixHostEnterHostTls();
    mapping = OrlixHostUserFindMapping(target_address,
                                       length,
                                       protection,
                                       writable);
	if (!mapping &&
	    OrlixHostUserCreateMapping(target_address,
				       length,
				       protection,
				       writable,
				       &mapping) != 0) {
		OrlixHostTraceShadowUserPageFailure("refresh-window-create-mapping",
						    target_address,
						    length,
						    0,
						    0,
						    protection,
						    protection,
						    KERN_FAILURE);
		OrlixHostLeaveHostTls(active_tls);
		return -1;
	}
	if (!mapping) {
		OrlixHostTraceShadowUserPageFailure("refresh-window-missing-mapping",
						    target_address,
						    length,
						    0,
						    0,
						    protection,
						    protection,
						    KERN_FAILURE);
		OrlixHostLeaveHostTls(active_tls);
		return -1;
	}

    status = vm_protect(mach_task_self(),
                        (vm_address_t)mapping->target_address,
                        mapping->length,
                        false,
                        copy_protection);
	if (status != KERN_SUCCESS) {
		OrlixHostTraceShadowUserPageFailure("refresh-window-copy-protect",
						    target_address,
						    length,
						    mapping->target_address,
						    mapping->length,
						    protection,
						    copy_protection,
						    status);
		OrlixHostLeaveHostTls(active_tls);
		return -1;
	}

    OrlixHostUserRemoveSegmentsInRange(mapping,
                                       target_address,
                                       length,
                                       true);
    for (unsigned long index = 0; index < segment_count; index++) {
        const struct orlix_host_user_page_segment *input = &segments[index];
        struct OrlixHostKernelShadowSegment *segment;
        unsigned long offset = input->target_address - mapping->target_address;

        memcpy((void *)(mapping->target_address + offset),
               input->source_page,
               (size_t)input->length);
        if (input->executable) {
            OrlixHostUserUnmapMappedRange(mapping->target_address,
                                          mapping->length);
            OrlixHostLeaveHostTls(active_tls);
            return -1;
        }

        segment = malloc(sizeof(*segment));
        if (!segment) {
            OrlixHostUserUnmapMappedRange(mapping->target_address,
                                          mapping->length);
            OrlixHostLeaveHostTls(active_tls);
            return -1;
        }
        segment->target_address = input->target_address;
        segment->source_page = input->source_page;
        segment->length = (vm_size_t)input->length;
        segment->next = mapping->segments;
        mapping->segments = segment;
    }

    status = vm_protect(mach_task_self(),
                        (vm_address_t)mapping->target_address,
                        mapping->length,
                        false,
                        VM_PROT_NONE);
    if (status != KERN_SUCCESS) {
        OrlixHostTraceShadowUserPageFailure("refresh-window-clear-protect",
                                            target_address,
                                            length,
                                            mapping->target_address,
                                            mapping->length,
                                            protection,
                                            VM_PROT_NONE,
                                            status);
        OrlixHostUserUnmapMappedRange(mapping->target_address,
                                      mapping->length);
        OrlixHostLeaveHostTls(active_tls);
        return -1;
    }

    for (unsigned long index = 0; index < segment_count; index++) {
        const struct orlix_host_user_page_segment *input = &segments[index];
        vm_prot_t segment_protection = VM_PROT_READ;

        if (input->writable) {
            segment_protection |= VM_PROT_WRITE;
        }
        status = vm_protect(mach_task_self(),
                            (vm_address_t)input->target_address,
                            (vm_size_t)input->length,
                            false,
                            segment_protection);
        if (status != KERN_SUCCESS) {
            OrlixHostTraceShadowUserPageFailure("refresh-window-segment-protect",
                                                input->target_address,
                                                input->length,
                                                mapping->target_address,
                                                mapping->length,
                                                segment_protection,
                                                copy_protection,
                                                status);
            OrlixHostUserUnmapMappedRange(mapping->target_address,
                                          mapping->length);
            OrlixHostLeaveHostTls(active_tls);
            return -1;
        }
    }

    mapping->protection = protection;
    mapping->writable = writable;
    OrlixHostLeaveHostTls(active_tls);
    return 0;
}

__attribute__((visibility("hidden"))) int orlix_host_user_mapping_last_failure(
    struct orlix_host_user_mapping_failure *failure)
{
    if (!failure || OrlixHostLastUserMappingFailure.operation ==
                        ORLIX_HOST_USER_MAPPING_FAILURE_NONE) {
        return -1;
    }

    *failure = OrlixHostLastUserMappingFailure;
    return 0;
}

__attribute__((visibility("hidden"))) void orlix_host_user_unmap_pages(
    unsigned long target_address,
    unsigned long length)
{
    unsigned long active_tls = OrlixHostEnterHostTls();

    OrlixHostUserUnmapMappedRange(target_address, length);
    OrlixHostReleaseUserRange(target_address, length);
    OrlixHostLeaveHostTls(active_tls);
}

__attribute__((visibility("hidden"))) void orlix_host_user_sync_writable_mappings(void)
{
    unsigned long active_tls = OrlixHostEnterHostTls();

    for (struct OrlixHostUserMapping *mapping = OrlixHostUserMappings;
         mapping;
         mapping = mapping->next) {
        OrlixHostUserCopyBackMapping(mapping);
    }

    OrlixHostLeaveHostTls(active_tls);
}

__attribute__((visibility("hidden"))) void *orlix_host_ioremap(
    unsigned long physical_address,
    unsigned long length)
{
    struct OrlixHostIOMapping *mapping;
    vm_address_t mapped = 0;
    vm_size_t rounded_length = OrlixHostRoundPageLength(length);
    kern_return_t status;
    unsigned long active_tls;
    void *result = NULL;

    if (physical_address == 0 || rounded_length == 0) {
        return NULL;
    }

    active_tls = OrlixHostEnterHostTls();
    status = OrlixHostAllocateIOMapping(rounded_length, &mapped);
    if (status != 0 || mapped == 0) {
        goto out;
    }

    mapping = malloc(sizeof(*mapping));
    if (!mapping) {
        (void)vm_deallocate(mach_task_self(), mapped, rounded_length);
        goto out;
    }

    memset((void *)mapped, 0, rounded_length);
    mapping->address = (void *)mapped;
    mapping->physical_address = physical_address;
    mapping->length = rounded_length;
    mapping->next = OrlixHostIOMappings;
    OrlixHostIOMappings = mapping;
    result = mapping->address;

out:
    OrlixHostLeaveHostTls(active_tls);
    return result;
}

__attribute__((visibility("hidden"))) void orlix_host_iounmap(
    void *mapped_address)
{
    struct OrlixHostIOMapping **cursor = &OrlixHostIOMappings;
    unsigned long active_tls;

    if (!mapped_address) {
        return;
    }

    active_tls = OrlixHostEnterHostTls();
    while (*cursor) {
        struct OrlixHostIOMapping *mapping = *cursor;

        if (mapping->address == mapped_address) {
            *cursor = mapping->next;
            (void)vm_deallocate(mach_task_self(),
                                (vm_address_t)mapping->address,
                                mapping->length);
            free(mapping);
            OrlixHostLeaveHostTls(active_tls);
            return;
        }

        cursor = &mapping->next;
    }

    OrlixHostLeaveHostTls(active_tls);
}

__attribute__((visibility("hidden"))) int orlix_host_iomem_physical_address(
    const void *mapped_address,
    unsigned long *physical_address)
{
    const unsigned char *address = mapped_address;
    struct OrlixHostIOMapping *mapping = OrlixHostIOMappings;

    if (!address || !physical_address) {
        return -1;
    }

    while (mapping) {
        const unsigned char *base = mapping->address;
        const unsigned char *end = base + mapping->length;

        if (address >= base && address < end) {
            *physical_address = mapping->physical_address +
                                (unsigned long)(address - base);
            return 0;
        }

        mapping = mapping->next;
    }

    return -1;
}
