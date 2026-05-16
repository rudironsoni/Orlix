#include "OrlixBootLauncher.h"

#include "OrlixKernel.h"

int OrlixTerminalBootDefaultAppStore(void)
{
    static const char root_image_identifier[] = "orlix.bundle.rootfs";
    static const char terminal_identifier[] = "orlix.terminal.main";
    struct OrlixBootConfig config = {
        .profile = ORLIX_BOOT_PROFILE_APPSTORE,
        .root_image_identifier = root_image_identifier,
        .terminal_identifier = terminal_identifier,
    };

    return OrlixBoot(&config);
}

const char *OrlixTerminalBootStatusMessage(int status)
{
    switch (status) {
    case ORLIX_BOOT_STATUS_OK:
        return "Orlix bootloader accepted the boot request.";
    case ORLIX_BOOT_STATUS_INVALID_CONFIG:
        return "Orlix bootloader rejected the boot config.";
    case ORLIX_BOOT_STATUS_UNAVAILABLE:
        return "Orlix boot handoff is not wired to iOS-hosted Linux execution yet.";
    default:
        return "Orlix bootloader returned an unknown status.";
    }
}
