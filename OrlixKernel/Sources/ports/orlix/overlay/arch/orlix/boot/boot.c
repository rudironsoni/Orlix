// SPDX-License-Identifier: GPL-2.0-only
#include <asm/boot.h>
#include <asm/hosted_exec.h>
#include <asm/page.h>
#include <asm/thread_info.h>
#include <internal/asm/host_boot_progress.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/start_kernel.h>

#if defined(ORLIX_APP_HOSTED_BOOT)
#define __orlix_boot_init
#define ORLIX_APP_HOSTED_BOOT_MEMORY_SIZE	(1024UL * 1024 * 1024)
static unsigned long app_hosted_boot_memory[ORLIX_APP_HOSTED_BOOT_MEMORY_SIZE /
					    sizeof(unsigned long)]
	__aligned(PAGE_SIZE);
static struct boot_params app_hosted_boot_params;
unsigned long orlix_hosted_user_base = 0x0000000100000000UL;
unsigned long orlix_hosted_stack_top = 0x0000000200000000UL;
unsigned long orlix_hosted_syscall_gate_address = 0x00000001fff00000UL;
unsigned long orlix_hosted_kernel_window_max = 0x0000000300000000UL;
#else
#define __orlix_boot_init __init
#endif

static const struct boot_params *last_boot_params;
static int boot_handoff_count;

static int arch_boot_power_of_two(unsigned long value)
{
	return value && !(value & (value - 1));
}

#if defined(ORLIX_APP_HOSTED_BOOT)
static int arch_boot_apply_hosted_user_window(const struct boot_params *params)
{
	unsigned long base;
	unsigned long limit;

	if (!params)
		return -1;

	base = params->hosted_user_base;
	limit = params->hosted_user_limit;
	if (!base || limit <= base || !IS_ALIGNED(base, PAGE_SIZE) ||
	    !IS_ALIGNED(limit, PAGE_SIZE) || limit > ~0UL - (limit - base))
		return -1;

	orlix_hosted_user_base = base;
	orlix_hosted_stack_top = limit;
	orlix_hosted_syscall_gate_address = limit - PAGE_SIZE;
	orlix_hosted_kernel_window_max = limit + (limit - base);
	return 0;
}
#endif

#if defined(ORLIX_APP_HOSTED_BOOT)
extern unsigned long init_stack[THREAD_SIZE / sizeof(unsigned long)];

static __attribute__((noreturn)) void arch_boot_start_kernel(void)
{
	unsigned long stack_top = (unsigned long)init_stack + THREAD_SIZE;
	void (*entry)(void) = start_kernel;

	orlix_hosted_capture_host_context();
	orlix_host_boot_progress_note(
		ORLIX_HOST_BOOT_STAGE_LINUX_START_KERNEL);
	asm volatile("mov x29, xzr\n"
		     "mov sp, %0\n"
		     "blr %1\n"
		     "brk #0\n"
		     :
		     : "r" (stack_top), "r" (entry)
		     : "memory");
	__builtin_unreachable();
}
#else
static void arch_boot_start_kernel(void)
{
	start_kernel();
}
#endif

static const struct boot_params *
arch_boot_materialize_handoff(const struct boot_params *params)
{
#if defined(ORLIX_APP_HOSTED_BOOT)
	app_hosted_boot_params = *params;
	if (!app_hosted_boot_params.memory_size) {
		app_hosted_boot_params.memory_base = __pa(app_hosted_boot_memory);
		app_hosted_boot_params.memory_size = sizeof(app_hosted_boot_memory);
	}
	if (!arch_boot_power_of_two(app_hosted_boot_params.host_page_size) ||
	    app_hosted_boot_params.host_page_size < PAGE_SIZE)
		app_hosted_boot_params.host_page_size = PAGE_SIZE;
	return &app_hosted_boot_params;
#else
	return params;
#endif
}

static void arch_boot_record_handoff(const struct boot_params *params)
{
	last_boot_params = arch_boot_materialize_handoff(params);
	boot_handoff_count++;
}

static int arch_boot_params_valid(const struct boot_params *params)
{
	return params && params->cmdline && params->cmdline[0] &&
	       params->dtb_base && params->dtb_size &&
	       params->root_device && params->root_device[0] &&
	       params->console_device && params->console_device[0];
}

int arch_boot_prepare_entry(const struct boot_params *params)
{
	if (!arch_boot_params_valid(params))
		return ORLIX_ARCH_BOOT_INVALID_CONFIG;

#if defined(ORLIX_APP_HOSTED_BOOT)
	if (arch_boot_apply_hosted_user_window(params))
		return ORLIX_ARCH_BOOT_INVALID_CONFIG;
	if (arch_boot_prepare_hosted_vmalloc_window())
		return ORLIX_ARCH_BOOT_UNAVAILABLE;
#endif

	arch_boot_record_handoff(params);
	return ORLIX_ARCH_BOOT_OK;
}

int __orlix_boot_init arch_boot_entry(const struct boot_params *params)
{
	int status = arch_boot_prepare_entry(params);

	orlix_host_boot_progress_note(ORLIX_HOST_BOOT_STAGE_ARCH_ENTRY);
	if (status != ORLIX_ARCH_BOOT_OK)
		return status;

	arch_boot_start_kernel();

	return ORLIX_ARCH_BOOT_OK;
}

const struct boot_params *arch_boot_params(void)
{
	return last_boot_params;
}

unsigned long arch_boot_host_page_size(void)
{
	const struct boot_params *params = arch_boot_params();

	if (!params || !arch_boot_power_of_two(params->host_page_size) ||
	    params->host_page_size < PAGE_SIZE)
		return PAGE_SIZE;
	return params->host_page_size;
}

#if defined(CONFIG_ORLIX_BOOT_KUNIT_TEST) || defined(ORLIX_APP_HOSTED_BOOT)
void arch_boot_test_record_handoff(const struct boot_params *params)
{
	arch_boot_record_handoff(params);
}

int arch_boot_handoff_count(void)
{
	return boot_handoff_count;
}

const struct boot_params *arch_boot_last_params(void)
{
	return last_boot_params;
}

void arch_boot_reset_handoff(void)
{
	last_boot_params = 0;
	boot_handoff_count = 0;
}
#endif
