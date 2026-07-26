// SPDX-License-Identifier: GPL-2.0
#include <stdbool.h>
#include <stdint.h>

#include "orlix_kselftest_user.h"

#define ORLIX_TCTI_FPCR_WRITABLE_MASK	(((uint64_t)0x1f << 22) | \
				 ((uint64_t)1 << 15) | \
				 ((uint64_t)0x1f << 8))
#define ORLIX_TCTI_FPSR_WRITABLE_MASK	(((uint64_t)1 << 27) | \
				 ((uint64_t)1 << 7) | 0x1f)

static uint64_t read_tpidr_el0(void)
{
	uint64_t value;

	__asm__ volatile("mrs %0, tpidr_el0" : "=r" (value));
	return value;
}

static uint64_t read_tpidrro_el0(void)
{
	uint64_t value;

	__asm__ volatile("mrs %0, tpidrro_el0" : "=r" (value));
	return value;
}

static uint64_t read_ctr_el0(void)
{
	uint64_t value;

	__asm__ volatile("mrs %0, ctr_el0" : "=r" (value));
	return value;
}

static uint64_t read_dczid_el0(void)
{
	uint64_t value;

	__asm__ volatile("mrs %0, dczid_el0" : "=r" (value));
	return value;
}

static uint64_t read_cntfrq_el0(void)
{
	uint64_t value;

	__asm__ volatile("mrs %0, cntfrq_el0" : "=r" (value));
	return value;
}

static uint64_t read_cntvct_el0(void)
{
	uint64_t value;

	__asm__ volatile("mrs %0, cntvct_el0" : "=r" (value));
	return value;
}

static uint64_t read_fpcr(void)
{
	uint64_t value;

	__asm__ volatile("mrs %0, fpcr" : "=r" (value));
	return value;
}

static void write_fpcr(uint64_t value)
{
	__asm__ volatile("msr fpcr, %0" : : "r" (value));
}

static uint64_t read_fpsr(void)
{
	uint64_t value;

	__asm__ volatile("mrs %0, fpsr" : "=r" (value));
	return value;
}

static void write_fpsr(uint64_t value)
{
	__asm__ volatile("msr fpsr, %0" : : "r" (value));
}

static bool system_registers_match_profile(void)
{
	uint64_t ctr = read_ctr_el0();
	uint64_t dczid = read_dczid_el0();

	return ((ctr >> 29) & 1) == 1 &&
	       ((ctr >> 28) & 1) == 1 &&
	       ((ctr >> 16) & 0xf) == 4 &&
	       ((ctr >> 14) & 0x3) == 3 &&
	       (ctr & 0xf) == 4 &&
	       ((dczid >> 4) & 1) == 1 &&
	       read_cntfrq_el0() == 1000000000ULL &&
	       read_tpidrro_el0() == 0;
}

static bool counter_is_monotonic(void)
{
	uint64_t before = read_cntvct_el0();
	uint64_t after = read_cntvct_el0();

	return after >= before;
}

static bool thread_pointer_is_stable(void)
{
	uint64_t before = read_tpidr_el0();
	uint64_t after = read_tpidr_el0();

	return after == before;
}

static bool fpcr_mask_is_enforced(void)
{
	uint64_t saved = read_fpcr();
	uint64_t observed;

	write_fpcr(UINT64_MAX);
	observed = read_fpcr();
	write_fpcr(saved);
	return observed == ORLIX_TCTI_FPCR_WRITABLE_MASK;
}

static bool fpsr_mask_is_enforced(void)
{
	uint64_t saved = read_fpsr();
	uint64_t observed;

	write_fpsr(UINT64_MAX);
	observed = read_fpsr();
	write_fpsr(saved);
	return observed == ORLIX_TCTI_FPSR_WRITABLE_MASK;
}

static bool barriers_resume_at_following_instruction(void)
{
	uint64_t marker = 0;

	__asm__ volatile("mov %0, #1\n\t"
		"dmb sy\n\t"
		"add %0, %0, #1\n\t"
		"dsb sy\n\t"
		"add %0, %0, #1\n\t"
		"isb\n\t"
		"add %0, %0, #1"
		: "+r" (marker)
		:
		: "memory");
	return marker == 4;
}

static bool cache_maintenance_resumes_at_following_instruction(void)
{
	uint64_t marker = 0;
	uint64_t target = 0;

	__asm__ volatile("mov %0, #1\n\t"
		"dc cvau, %1\n\t"
		"add %0, %0, #1\n\t"
		"dc cvac, %1\n\t"
		"add %0, %0, #1\n\t"
		"dc civac, %1\n\t"
		"add %0, %0, #1\n\t"
		"ic ivau, %1\n\t"
		"add %0, %0, #1"
		: "+r" (marker)
		: "r" (&target)
		: "memory");
	return marker == 5;
}

static bool clrex_immediates_resume_at_following_instruction(void)
{
	uint64_t marker = 0;

	__asm__ volatile("mov %0, #1\n\t"
		"clrex #0\n\t"
		"add %0, %0, #1\n\t"
		"clrex #15\n\t"
		"add %0, %0, #1"
		: "+r" (marker)
		:
		: "memory");
	return marker == 3;
}

static bool yield_hints_resume_at_following_instruction(void)
{
	uint64_t marker = 0;

	__asm__ volatile("mov %0, #1\n\t"
		"yield\n\t"
		"add %0, %0, #1\n\t"
		"wfe\n\t"
		"add %0, %0, #1\n\t"
		"wfi\n\t"
		"add %0, %0, #1"
		: "+r" (marker)
		:
		: "memory");
	return marker == 4;
}

int main(void)
{
	orlix_test_plan(9);
	orlix_test_result(system_registers_match_profile(),
			  "EL0 system registers match the exposed profile");
	orlix_test_result(counter_is_monotonic(),
			  "CNTVCT_EL0 is monotonic at the advertised frequency");
	orlix_test_result(thread_pointer_is_stable(),
			  "TPIDR_EL0 remains stable across reads");
	orlix_test_result(fpcr_mask_is_enforced(),
			  "FPCR rejects reserved writable bits");
	orlix_test_result(fpsr_mask_is_enforced(),
			  "FPSR rejects reserved writable bits");
	orlix_test_result(barriers_resume_at_following_instruction(),
			  "DMB DSB and ISB preserve ordered execution");
	orlix_test_result(cache_maintenance_resumes_at_following_instruction(),
			  "cache maintenance resumes at the following instruction");
	orlix_test_result(clrex_immediates_resume_at_following_instruction(),
			  "CLREX immediate forms resume at the following instruction");
	orlix_test_result(yield_hints_resume_at_following_instruction(),
			  "YIELD WFE and WFI resume at the following instruction");
	orlix_test_exit();
}
