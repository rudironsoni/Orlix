// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched/mm.h>
#include <linux/string.h>
#include <linux/syscalls.h>
#include <asm/ptrace.h>
#include <asm/orlix_tcti.h>

#include "../decode_aarch64.h"

/*
 * These are mapped-RX programs, not switch-debug unit calls.  They cover
 * each base-LSE ordering encoding for representative scalar CAS, scalar RMW,
 * and pair CAS.  They prove deterministic single-thread state transitions.
 * They do not claim a concurrent memory-ordering stress proof.
 */
#define ORLIX_TCTI_LSE_RESUME_SVC 0xd4000001U

enum orlix_tcti_lse_resume_kind {
	ORLIX_TCTI_LSE_RESUME_CAS,
	ORLIX_TCTI_LSE_RESUME_RMW,
	ORLIX_TCTI_LSE_RESUME_CASP,
};

struct orlix_tcti_lse_resume_case {
	const char *name;
	u32 source_ordinal;
	u32 encoding;
	enum orlix_tcti_lse_resume_kind kind;
	bool acquire;
	bool release;
};

/* AARCHMRS 2026-06 base FEAT_LSE source leaves. */
static const struct orlix_tcti_lse_resume_case orlix_tcti_lse_resume_cases[] = {
	{ "CAS", 2643U, 0xc8a07c00U, ORLIX_TCTI_LSE_RESUME_CAS, false, false },
	{ "CASL", 2644U, 0xc8a0fc00U, ORLIX_TCTI_LSE_RESUME_CAS, false, true },
	{ "CASA", 2645U, 0xc8e07c00U, ORLIX_TCTI_LSE_RESUME_CAS, true, false },
	{ "CASAL", 2646U, 0xc8e0fc00U, ORLIX_TCTI_LSE_RESUME_CAS, true, true },
	{ "LDADD", 3136U, 0xf8200000U, ORLIX_TCTI_LSE_RESUME_RMW, false, false },
	{ "LDADDL", 3149U, 0xf8600000U, ORLIX_TCTI_LSE_RESUME_RMW, false, true },
	{ "LDADDA", 3158U, 0xf8a00000U, ORLIX_TCTI_LSE_RESUME_RMW, true, false },
	{ "LDADDAL", 3168U, 0xf8e00000U, ORLIX_TCTI_LSE_RESUME_RMW, true, true },
	{ "CASP", 2349U, 0x48207c00U, ORLIX_TCTI_LSE_RESUME_CASP, false, false },
	{ "CASPL", 2350U, 0x4820fc00U, ORLIX_TCTI_LSE_RESUME_CASP, false, true },
	{ "CASPA", 2351U, 0x48607c00U, ORLIX_TCTI_LSE_RESUME_CASP, true, false },
	{ "CASPAL", 2352U, 0x4860fc00U, ORLIX_TCTI_LSE_RESUME_CASP, true, true },
};

static int orlix_tcti_lse_resume_production_test_init(struct kunit *test)
{
	struct mm_struct *mm = mm_alloc();

	if (!mm)
		return -ENOMEM;
	kthread_use_mm(mm);
	test->priv = mm;
	return 0;
}

static void orlix_tcti_lse_resume_production_test_exit(struct kunit *test)
{
	struct mm_struct *mm = test->priv;

	if (!mm)
		return;
	kthread_unuse_mm(mm);
	mmput(mm);
}

static unsigned long orlix_tcti_lse_resume_map(struct kunit *test, int prot)
{
	unsigned long address;

	address = ksys_mmap_pgoff(0, PAGE_SIZE, prot,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_EXPECT_FALSE(test, IS_ERR_VALUE(address));
	return address;
}

static int orlix_tcti_lse_resume_write_program(unsigned long address, u32 instruction)
{
	const u32 program[] = { instruction, ORLIX_TCTI_LSE_RESUME_SVC };
	int ret;

	ret = sys_mprotect(address, PAGE_SIZE, PROT_READ | PROT_WRITE);
	if (ret)
		return ret;
	ret = orlix_tcti_write_user_data(current->mm, address, program,
				   sizeof(program));
	if (ret)
		return ret;
	return sys_mprotect(address, PAGE_SIZE, PROT_READ | PROT_EXEC);
}

static u32 orlix_tcti_lse_resume_instruction(const struct orlix_tcti_lse_resume_case *entry)
{
	/* Rs=x6, Rn=x10, Rt=x8.  CASP register pairs are x6:x7 and x8:x9. */
	return entry->encoding | (6U << 16) | (10U << 5) | 8U;
}

static void orlix_tcti_lse_resume_init_regs(struct pt_regs *regs,
				      unsigned long instructions,
				      unsigned long data)
{
	*regs = (struct pt_regs) {};
	regs->regs[10] = data;
	regs->pc = instructions;
	regs->pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_Z_BIT |
		PSR_C_BIT | PSR_V_BIT;
	regs->syscallno = NO_SYSCALL;
}

static void orlix_tcti_lse_resume_expect_order(struct kunit *test,
				 const struct orlix_tcti_lse_resume_case *entry,
				 u32 instruction)
{
	struct orlix_tcti_decoded_instruction decoded = orlix_tcti_decode_aarch64(instruction);

	KUNIT_ASSERT_EQ_MSG(test, ORLIX_TCTI_DECODE_LSE_ATOMIC, decoded.decode_class,
			    "%s source=%u", entry->name, entry->source_ordinal);
	KUNIT_EXPECT_EQ_MSG(test, entry->acquire, decoded.acquire,
			    "%s source=%u", entry->name, entry->source_ordinal);
	KUNIT_EXPECT_EQ_MSG(test, entry->release, decoded.release,
			    "%s source=%u", entry->name, entry->source_ordinal);
	KUNIT_EXPECT_EQ_MSG(test, entry->kind == ORLIX_TCTI_LSE_RESUME_CASP,
			    decoded.pair, "%s source=%u", entry->name,
			    entry->source_ordinal);
}

static void orlix_tcti_lse_resume_expect_exit(struct kunit *test,
				const struct orlix_tcti_lse_resume_case *entry,
				const struct orlix_tcti_result *result,
				unsigned long instructions)
{
	KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_SYSCALL, result->reason,
			    "%s source=%u", entry->name, entry->source_ordinal);
	KUNIT_EXPECT_EQ_MSG(test, 0L, result->status, "%s source=%u",
			    entry->name, entry->source_ordinal);
	KUNIT_EXPECT_EQ_MSG(test, instructions + sizeof(u32), result->pc,
			    "%s source=%u", entry->name, entry->source_ordinal);
	KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_LSE_RESUME_SVC, result->instruction,
			    "%s source=%u", entry->name, entry->source_ordinal);
}

static void orlix_tcti_lse_resume_executes_order_variants(struct kunit *test)
{
	const u64 old = 0x0123456789abcdefULL;
	const u64 old_high = 0xfedcba9876543210ULL;
	const u64 operand = 0x1122334455667788ULL;
	const u64 operand_high = 0x8877665544332211ULL;
	const u64 expected_pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_Z_BIT |
		PSR_C_BIT | PSR_V_BIT;
	unsigned long instructions = orlix_tcti_lse_resume_map(test,
							 PROT_READ | PROT_WRITE);
	unsigned long data = orlix_tcti_lse_resume_map(test, PROT_READ | PROT_WRITE);
	size_t index;

	if (IS_ERR_VALUE(instructions) || IS_ERR_VALUE(data))
		return;
	for (index = 0; index < ARRAY_SIZE(orlix_tcti_lse_resume_cases); index++) {
		const struct orlix_tcti_lse_resume_case *entry =
			&orlix_tcti_lse_resume_cases[index];
		u32 instruction = orlix_tcti_lse_resume_instruction(entry);
		struct pt_regs regs;
		struct orlix_tcti_result result;
		u64 observed[2] = {};
		int ret;

		ret = orlix_tcti_lse_resume_write_program(instructions, instruction);
		KUNIT_ASSERT_EQ_MSG(test, 0, ret, "%s", entry->name);
		orlix_tcti_lse_resume_expect_order(test, entry, instruction);
		if (entry->kind == ORLIX_TCTI_LSE_RESUME_CASP) {
			const u64 initial[] = { old, old_high };

			ret = orlix_tcti_write_user_data(current->mm, data, initial,
						   sizeof(initial));
			KUNIT_ASSERT_EQ_MSG(test, 0, ret, "%s", entry->name);
		} else {
			ret = orlix_tcti_write_user_data(current->mm, data, &old, sizeof(old));
			KUNIT_ASSERT_EQ_MSG(test, 0, ret, "%s", entry->name);
		}
		orlix_tcti_lse_resume_init_regs(&regs, instructions, data);
		if (entry->kind == ORLIX_TCTI_LSE_RESUME_RMW) {
			regs.regs[6] = operand;
			regs.regs[8] = U64_MAX;
		} else {
			regs.regs[6] = old;
			regs.regs[8] = operand;
			if (entry->kind == ORLIX_TCTI_LSE_RESUME_CASP) {
				regs.regs[7] = old_high;
				regs.regs[9] = operand_high;
			}
		}
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		orlix_tcti_lse_resume_expect_exit(test, entry, &result, instructions);
		KUNIT_EXPECT_EQ_MSG(test, instructions + sizeof(u32), regs.pc,
				    "%s", entry->name);
		KUNIT_EXPECT_EQ_MSG(test, expected_pstate, regs.pstate, "%s",
				    entry->name);
		if (entry->kind == ORLIX_TCTI_LSE_RESUME_CAS) {
			ret = orlix_tcti_read_user_data(current->mm, data, observed,
						  sizeof(old));
			KUNIT_ASSERT_EQ_MSG(test, 0, ret, "%s", entry->name);
			KUNIT_EXPECT_EQ(test, old, regs.regs[6]);
			KUNIT_EXPECT_EQ(test, operand, observed[0]);
		} else if (entry->kind == ORLIX_TCTI_LSE_RESUME_RMW) {
			ret = orlix_tcti_read_user_data(current->mm, data, observed,
						  sizeof(old));
			KUNIT_ASSERT_EQ_MSG(test, 0, ret, "%s", entry->name);
			KUNIT_EXPECT_EQ(test, old, regs.regs[8]);
			KUNIT_EXPECT_EQ(test, old + operand, observed[0]);
		} else {
			ret = orlix_tcti_read_user_data(current->mm, data, observed,
						  sizeof(observed));
			KUNIT_ASSERT_EQ_MSG(test, 0, ret, "%s", entry->name);
			KUNIT_EXPECT_EQ(test, old, regs.regs[6]);
			KUNIT_EXPECT_EQ(test, old_high, regs.regs[7]);
			KUNIT_EXPECT_EQ(test, operand, observed[0]);
			KUNIT_EXPECT_EQ(test, operand_high, observed[1]);
		}
	}
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(data, PAGE_SIZE));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(instructions, PAGE_SIZE));
}

static void orlix_tcti_lse_resume_cas_mismatch_preserves_memory(struct kunit *test)
{
	const struct orlix_tcti_lse_resume_case *entry = &orlix_tcti_lse_resume_cases[3];
	const u64 expected = 0x0123456789abcdefULL;
	const u64 actual = 0xfedcba9876543210ULL;
	const u64 desired = 0x1122334455667788ULL;
	unsigned long instructions = orlix_tcti_lse_resume_map(test,
							 PROT_READ | PROT_WRITE);
	unsigned long data = orlix_tcti_lse_resume_map(test, PROT_READ | PROT_WRITE);
	u32 instruction = orlix_tcti_lse_resume_instruction(entry);
	struct pt_regs regs;
	struct orlix_tcti_result result;
	u64 observed = 0;
	int ret;

	if (IS_ERR_VALUE(instructions) || IS_ERR_VALUE(data))
		return;
	ret = orlix_tcti_lse_resume_write_program(instructions, instruction);
	KUNIT_ASSERT_EQ(test, 0, ret);
	ret = orlix_tcti_write_user_data(current->mm, data, &actual, sizeof(actual));
	KUNIT_ASSERT_EQ(test, 0, ret);
	orlix_tcti_lse_resume_init_regs(&regs, instructions, data);
	regs.regs[6] = expected;
	regs.regs[8] = desired;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	orlix_tcti_lse_resume_expect_exit(test, entry, &result, instructions);
	KUNIT_EXPECT_EQ(test, actual, regs.regs[6]);
	ret = orlix_tcti_read_user_data(current->mm, data, &observed, sizeof(observed));
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, actual, observed);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(data, PAGE_SIZE));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(instructions, PAGE_SIZE));
}

static void orlix_tcti_lse_resume_readonly_fault_does_not_mutate(struct kunit *test)
{
	const u64 initial[] = { 0x0123456789abcdefULL, 0xfedcba9876543210ULL };
	unsigned long instructions = orlix_tcti_lse_resume_map(test,
							 PROT_READ | PROT_WRITE);
	unsigned long data = orlix_tcti_lse_resume_map(test, PROT_READ | PROT_WRITE);
	size_t index;

	if (IS_ERR_VALUE(instructions) || IS_ERR_VALUE(data))
		return;
	for (index = 0; index < ARRAY_SIZE(orlix_tcti_lse_resume_cases); index += 4) {
		const struct orlix_tcti_lse_resume_case *entry =
			&orlix_tcti_lse_resume_cases[index];
		u32 instruction = orlix_tcti_lse_resume_instruction(entry);
		struct pt_regs regs;
		struct pt_regs before;
		struct orlix_tcti_result result;
		u64 observed[2] = {};
		size_t bytes = entry->kind == ORLIX_TCTI_LSE_RESUME_CASP ?
			sizeof(initial) : sizeof(initial[0]);
		int ret;

		ret = orlix_tcti_lse_resume_write_program(instructions, instruction);
		KUNIT_ASSERT_EQ_MSG(test, 0, ret, "%s", entry->name);
		ret = orlix_tcti_write_user_data(current->mm, data, initial, bytes);
		KUNIT_ASSERT_EQ_MSG(test, 0, ret, "%s", entry->name);
		orlix_tcti_lse_resume_init_regs(&regs, instructions, data);
		regs.regs[6] = initial[0];
		regs.regs[8] = ~initial[0];
		if (entry->kind == ORLIX_TCTI_LSE_RESUME_CASP) {
			regs.regs[7] = initial[1];
			regs.regs[9] = ~initial[1];
		}
		before = regs;
		KUNIT_ASSERT_EQ(test, 0, sys_mprotect(data, PAGE_SIZE, PROT_READ));
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_USER_FAULT, result.reason,
				    "%s", entry->name);
		KUNIT_EXPECT_EQ_MSG(test, -EACCES, result.status, "%s", entry->name);
		KUNIT_EXPECT_EQ_MSG(test, data, result.fault_address, "%s",
				    entry->name);
		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_ACCESS_WRITE, result.fault_access,
				    "%s", entry->name);
		KUNIT_EXPECT_EQ_MSG(test, instructions, result.pc, "%s", entry->name);
		KUNIT_EXPECT_EQ_MSG(test, instruction, result.instruction, "%s",
				    entry->name);
		KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
		KUNIT_ASSERT_EQ(test, 0, sys_mprotect(data, PAGE_SIZE,
						       PROT_READ | PROT_WRITE));
		ret = orlix_tcti_read_user_data(current->mm, data, observed, bytes);
		KUNIT_ASSERT_EQ_MSG(test, 0, ret, "%s", entry->name);
		KUNIT_EXPECT_MEMEQ(test, initial, observed, bytes);
	}
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(data, PAGE_SIZE));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(instructions, PAGE_SIZE));
}

static struct kunit_case orlix_tcti_lse_resume_production_test_cases[] = {
	KUNIT_CASE(orlix_tcti_lse_resume_executes_order_variants),
	KUNIT_CASE(orlix_tcti_lse_resume_cas_mismatch_preserves_memory),
	KUNIT_CASE(orlix_tcti_lse_resume_readonly_fault_does_not_mutate),
	{}
};

static struct kunit_suite orlix_tcti_lse_resume_production_test_suite = {
	.name = "orlix-tcti-lse-resume-production",
	.init = orlix_tcti_lse_resume_production_test_init,
	.exit = orlix_tcti_lse_resume_production_test_exit,
	.test_cases = orlix_tcti_lse_resume_production_test_cases,
};
kunit_test_suite(orlix_tcti_lse_resume_production_test_suite);

MODULE_LICENSE("GPL");
