#include "OrlixHostAdapter/memory/kernel_mapping.h"

#include <mach/mach.h>
#include <mach/vm_map.h>
#include <stdbool.h>

__attribute__((visibility("hidden"))) int orlix_host_kernel_map_page(
    unsigned long target_address,
    const void *source_page,
    unsigned long length)
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

    return 0;
}

__attribute__((visibility("hidden"))) void orlix_host_kernel_unmap_pages(
    unsigned long target_address,
    unsigned long length)
{
    if (target_address == 0 || length == 0) {
        return;
    }

    (void)vm_deallocate(mach_task_self(),
                        (vm_address_t)target_address,
                        (vm_size_t)length);
}
