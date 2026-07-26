// SPDX-License-Identifier: GPL-2.0-only
/*
 * Production-path regression coverage for the pinned AARCHMRS 2026-06
 * load-literal leaves at ordinals 2700 through 2703. These ordinal traces
 * are diagnostic only. The pinned package lacks the official shared-ASL
 * corpus, so this suite remains an unregistered regression only.
 */
#include <asm/ptrace.h>
#include <asm/orlix_tcti.h>
#include <kunit/test.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched/mm.h>
#include <linux/string.h>
#include <linux/syscalls.h>

#include "../decode_aarch64.h"

#define LITERAL_SVC 0xd4000001U

struct literal_source_leaf {
	u32 ordinal;
	const char *id;
	u32 mask;
	u32 pattern;
};

#define ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE(...)
#define ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW(ordinal, id, mnemonic, operation, mask, \
					     pattern, feature_predicate, offset, length) \
	{ ordinal, id, mask, pattern },
static const struct literal_source_leaf literal_source_manifest[] = {
#include "../isa/source_manifest.def"
};
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE

static const u32 literal_ordinals[] = {
	2700U, /* LDR D, literal */
	2701U, /* LDRSW X, literal */
	2702U, /* LDR Q, literal */
	2703U, /* PRFM, literal */
};

static const struct literal_source_leaf *literal_source_leaf(u32 ordinal)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(literal_source_manifest); index++)
		if (literal_source_manifest[index].ordinal == ordinal)
			return &literal_source_manifest[index];
	return NULL;
}

static int literal_resume_regression_test_init(struct kunit *test)
{
	struct mm_struct *mm = mm_alloc();

	if (!mm)
		return -ENOMEM;
	kthread_use_mm(mm);
	test->priv = mm;
	return 0;
}

static void literal_resume_regression_test_exit(struct kunit *test)
{
	struct mm_struct *mm = test->priv;

	if (!mm)
		return;
	kthread_unuse_mm(mm);
	mmput(mm);
}

static unsigned long literal_map_image(struct kunit *test, u32 instruction,
				       const void *literal, size_t literal_size)
{
	const u32 program[] = { instruction, LITERAL_SVC };
	unsigned long mapped;
	int ret;

	mapped = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));
	ret = orlix_tcti_write_user_data(current->mm, mapped, program, sizeof(program));
	KUNIT_ASSERT_EQ(test, 0, ret);
	if (literal_size) {
		ret = orlix_tcti_write_user_data(current->mm, mapped + sizeof(program),
					   literal, literal_size);
		KUNIT_ASSERT_EQ(test, 0, ret);
	}
	ret = sys_mprotect(mapped, PAGE_SIZE, PROT_READ | PROT_EXEC);
	KUNIT_ASSERT_EQ(test, 0, ret);
	return mapped;
}

static unsigned long literal_map_fault_image(struct kunit *test,
					     u32 instruction)
{
	const u32 program[] = { instruction, LITERAL_SVC };
	unsigned long mapped;
	int ret;

	mapped = ksys_mmap_pgoff(0, 2 * PAGE_SIZE, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));
	ret = orlix_tcti_write_user_data(current->mm, mapped, program, sizeof(program));
	KUNIT_ASSERT_EQ(test, 0, ret);
	ret = sys_mprotect(mapped, PAGE_SIZE, PROT_READ | PROT_EXEC);
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_ASSERT_EQ(test, 0, vm_munmap(mapped + PAGE_SIZE, PAGE_SIZE));
	return mapped;
}

static void literal_expect_exit(struct kunit *test, unsigned long pc,
				u32 instruction, struct pt_regs *regs)
{
	struct orlix_tcti_result result;

	regs->pc = pc;
	regs->pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_C_BIT;
	regs->syscallno = NO_SYSCALL;
	result = orlix_tcti_resume_user(current, regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, 0L, result.status);
	KUNIT_EXPECT_EQ(test, pc + sizeof(u32), result.pc);
	KUNIT_EXPECT_EQ(test, LITERAL_SVC, result.instruction);
	KUNIT_EXPECT_EQ(test, pc + sizeof(u32), regs->pc);
	KUNIT_EXPECT_EQ(test, PSR_MODE_EL0t | PSR_N_BIT | PSR_C_BIT,
			regs->pstate);
	KUNIT_EXPECT_NE(test, instruction, result.instruction);
}

static void literal_ordinal_tuples_decode_as_the_expected_forms(struct kunit *test)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(literal_ordinals); index++) {
		const struct literal_source_leaf *leaf =
			literal_source_leaf(literal_ordinals[index]);
		struct orlix_tcti_decoded_instruction decoded;

		KUNIT_ASSERT_NOT_NULL(test, leaf);
		decoded = orlix_tcti_decode_aarch64(leaf->pattern | (2U << 5) | 5U);
		KUNIT_EXPECT_EQ_MSG(test, leaf->pattern,
			(leaf->pattern | (2U << 5) | 5U) & leaf->mask,
			"source=%u id=%s", leaf->ordinal, leaf->id);
		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_DECODE_LOAD_LITERAL,
			decoded.decode_class, "source=%u id=%s", leaf->ordinal,
			leaf->id);
		KUNIT_EXPECT_EQ_MSG(test, 8LL, decoded.memory_offset,
			"source=%u id=%s", leaf->ordinal, leaf->id);
	}
}

static void literal_resume_executes_d_q_and_ldrsw_regressions(
	struct kunit *test)
{
	const u64 d_value = 0x8877665544332211ULL;
	const u64 q_value[2] = {
		0x0123456789abcdefULL, 0xfedcba9876543210ULL,
	};
	const s32 signed_value = -1234567;
	const struct {
		u32 ordinal;
		u32 instruction;
		const void *value;
		size_t value_size;
		u64 expected_low;
		u64 expected_high;
		bool simd;
		u8 rt;
	} cases[] = {
		{ 2700U, 0x5c000045U, &d_value, sizeof(d_value), d_value, 0,
		  true, 5 },
		{ 2701U, 0x98000047U, &signed_value, sizeof(signed_value),
		  (u64)(s64)signed_value, 0, false, 7 },
		{ 2702U, 0x9c000049U, q_value, sizeof(q_value),
		  q_value[0], q_value[1], true, 9 },
	};
	size_t index;

	for (index = 0; index < ARRAY_SIZE(cases); index++) {
		struct pt_regs regs = {};
		struct pt_regs expected;
		unsigned long simd_before[ARRAY_SIZE(current->thread.user_simd)];
		unsigned long simd_expected[ARRAY_SIZE(current->thread.user_simd)];
		unsigned long mapped = literal_map_image(test, cases[index].instruction,
						  cases[index].value,
						  cases[index].value_size);

		regs.regs[0] = 0x1122334455667788ULL;
		regs.regs[30] = 0x8877665544332211ULL;
		regs.sp = 0x0123456789abcdefULL;
		regs.orig_x0 = 0xfeedfaceULL;
		current->thread.user_simd[cases[index].rt * 2] =
			0xa5a5a5a5a5a5a5a5ULL;
		current->thread.user_simd[cases[index].rt * 2 + 1] =
			0x5a5a5a5a5a5a5a5aULL;
		memcpy(simd_before, current->thread.user_simd, sizeof(simd_before));
		memcpy(simd_expected, simd_before, sizeof(simd_expected));
		if (cases[index].simd) {
			simd_expected[cases[index].rt * 2] = cases[index].expected_low;
			simd_expected[cases[index].rt * 2 + 1] =
				cases[index].expected_high;
		}
		expected = regs;
		expected.pc = mapped + sizeof(u32);
		expected.pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_C_BIT;
		expected.syscallno = NO_SYSCALL;
		if (!cases[index].simd)
			expected.regs[cases[index].rt] = cases[index].expected_low;
		literal_expect_exit(test, mapped, cases[index].instruction, &regs);
		KUNIT_EXPECT_MEMEQ_MSG(test, &expected, &regs, sizeof(regs),
			"source=%u", cases[index].ordinal);
		if (!cases[index].simd) {
			KUNIT_EXPECT_EQ_MSG(test, cases[index].expected_low, regs.regs[7],
				"source=%u", cases[index].ordinal);
		}
		KUNIT_EXPECT_MEMEQ_MSG(test, simd_expected, current->thread.user_simd,
			sizeof(simd_expected), "source=%u", cases[index].ordinal);
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
	}
}

static void literal_resume_executes_prfm_without_a_literal_read(struct kunit *test)
{
	const u32 instruction = 0xd8008000U; /* PRFM pldl1keep, .+4096 */
	struct pt_regs regs = {};
	struct pt_regs expected;
	unsigned long simd_before[ARRAY_SIZE(current->thread.user_simd)];
	unsigned long mapped = literal_map_fault_image(test, instruction);

	regs.regs[0] = 0x1122334455667788ULL;
	regs.regs[30] = 0x8877665544332211ULL;
	regs.sp = 0x0123456789abcdefULL;
	regs.orig_x0 = 0xfeedfaceULL;
	expected = regs;
	expected.pc = mapped + sizeof(u32);
	expected.pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_C_BIT;
	expected.syscallno = NO_SYSCALL;
	memcpy(simd_before, current->thread.user_simd, sizeof(simd_before));
	literal_expect_exit(test, mapped, instruction, &regs);
	KUNIT_EXPECT_MEMEQ(test, &expected, &regs, sizeof(regs));
	KUNIT_EXPECT_MEMEQ(test, simd_before, current->thread.user_simd,
		sizeof(simd_before));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void literal_resume_read_faults_preserve_destination_state(
	struct kunit *test)
{
	const struct {
		u32 ordinal;
		u32 pattern;
		u8 rt;
		bool simd;
	} cases[] = {
		{ 2700U, 0x5c000000U, 5, true },
		{ 2701U, 0x98000000U, 7, false },
		{ 2702U, 0x9c000000U, 9, true },
	};
	size_t index;

	for (index = 0; index < ARRAY_SIZE(cases); index++) {
		const u32 instruction = cases[index].pattern | (1024U << 5) |
			cases[index].rt;
		struct pt_regs regs = {};
		struct pt_regs before;
		struct orlix_tcti_result result;
		unsigned long before_simd[ARRAY_SIZE(current->thread.user_simd)];
		unsigned long mapped = literal_map_fault_image(test, instruction);

		regs.regs[cases[index].rt] = 0x1122334455667788ULL;
		regs.pc = mapped;
		regs.pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_C_BIT;
		regs.syscallno = NO_SYSCALL;
		before = regs;
		memcpy(before_simd, current->thread.user_simd, sizeof(before_simd));

		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_USER_FAULT, result.reason,
			"source=%u", cases[index].ordinal);
		KUNIT_EXPECT_EQ_MSG(test, -EFAULT, result.status, "source=%u",
			cases[index].ordinal);
		KUNIT_EXPECT_EQ_MSG(test, mapped + PAGE_SIZE, result.fault_address,
			"source=%u", cases[index].ordinal);
		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_ACCESS_READ, result.fault_access,
			"source=%u", cases[index].ordinal);
		KUNIT_EXPECT_EQ_MSG(test, mapped, result.pc, "source=%u",
			cases[index].ordinal);
		KUNIT_EXPECT_EQ_MSG(test, instruction, result.instruction,
			"source=%u", cases[index].ordinal);
		KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
		KUNIT_EXPECT_MEMEQ(test, before_simd, current->thread.user_simd,
			sizeof(before_simd));
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
	}
}

static struct kunit_case literal_resume_regression_cases[] = {
	KUNIT_CASE(literal_ordinal_tuples_decode_as_the_expected_forms),
	KUNIT_CASE(literal_resume_executes_d_q_and_ldrsw_regressions),
	KUNIT_CASE(literal_resume_executes_prfm_without_a_literal_read),
	KUNIT_CASE(literal_resume_read_faults_preserve_destination_state),
	{}
};

static struct kunit_suite literal_resume_regression_suite = {
	.name = "orlix-tcti-load-literal-resume-regression",
	.init = literal_resume_regression_test_init,
	.exit = literal_resume_regression_test_exit,
	.test_cases = literal_resume_regression_cases,
};

kunit_test_suite(literal_resume_regression_suite);

MODULE_LICENSE("GPL");
