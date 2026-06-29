/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _INTERNAL_ASM_ORLIX_HOST_BOOT_PROGRESS_H
#define _INTERNAL_ASM_ORLIX_HOST_BOOT_PROGRESS_H

#if defined(ORLIX_APP_HOSTED_BOOT)

enum orlix_host_boot_progress_stage {
	ORLIX_HOST_BOOT_STAGE_BOOT_CONFIG_VALIDATED = 50,
	ORLIX_HOST_BOOT_STAGE_HOST_RESOURCES_READY = 60,
	ORLIX_HOST_BOOT_STAGE_KERNEL_HANDOFF = 70,
	ORLIX_HOST_BOOT_STAGE_ARCH_ENTRY = 80,
	ORLIX_HOST_BOOT_STAGE_LINUX_START_KERNEL = 100,
	ORLIX_HOST_BOOT_STAGE_FAILED = 1000,
};

void orlix_host_boot_progress_record(
	unsigned int stage,
	int status,
	int mach_kern_return,
	int posix_errno);

static inline void orlix_host_boot_progress_note(unsigned int stage)
{
	orlix_host_boot_progress_record(stage, 0, 0, 0);
}

static inline void orlix_host_boot_progress_fail(int status)
{
	orlix_host_boot_progress_record(
		ORLIX_HOST_BOOT_STAGE_FAILED, status, 0, 0);
}

#else

static inline void orlix_host_boot_progress_note(unsigned int stage)
{
}

static inline void orlix_host_boot_progress_fail(int status)
{
}

#endif

#endif /* _INTERNAL_ASM_ORLIX_HOST_BOOT_PROGRESS_H */
