#include <asm/boot.h>

#if defined(CONFIG_ORLIX_BOOT_KUNIT_TEST)
static const struct boot_params *last_boot_params;
static int boot_handoff_count;
#endif

void arch_boot_entry(const struct boot_params *params)
{
#if defined(CONFIG_ORLIX_BOOT_KUNIT_TEST)
	last_boot_params = params;
	boot_handoff_count++;
#else
	(void)params;
#endif
}

#if defined(CONFIG_ORLIX_BOOT_KUNIT_TEST)
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
