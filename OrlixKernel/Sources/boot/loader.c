#include "boot/handoff.h"
#include <internal/asm/host_boot_progress.h>

static int OrlixBootStarted;

int OrlixBoot(const struct OrlixBootConfig *config) {
    struct OrlixBootInput input;
    int status;

    if (OrlixPrepareBootInput(config, &input) != ORLIX_BOOT_STATUS_OK) {
        orlix_host_boot_progress_fail(ORLIX_BOOT_STATUS_INVALID_CONFIG);
        return ORLIX_BOOT_STATUS_INVALID_CONFIG;
    }
    orlix_host_boot_progress_note(
        ORLIX_HOST_BOOT_STAGE_BOOT_CONFIG_VALIDATED);

    if (!__sync_bool_compare_and_swap(&OrlixBootStarted, 0, 1)) {
        orlix_host_boot_progress_fail(ORLIX_BOOT_STATUS_ALREADY_STARTED);
        return ORLIX_BOOT_STATUS_ALREADY_STARTED;
    }

    status = OrlixBootHandoff(&input);
    if (status == ORLIX_BOOT_STATUS_INVALID_CONFIG) {
        (void)__sync_bool_compare_and_swap(&OrlixBootStarted, 1, 0);
    }
    if (status != ORLIX_BOOT_STATUS_OK) {
        orlix_host_boot_progress_fail(status);
    }
    return status;
}
