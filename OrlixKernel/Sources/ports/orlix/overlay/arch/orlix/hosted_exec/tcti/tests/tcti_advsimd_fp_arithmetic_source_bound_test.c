// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/bitfield.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/syscalls.h>
#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/tcti.h>

#include "../decode_aarch64.h"

/*
 * Pinned AARCHMRS 2026-06 required non-FP16 AdvSIMD source leaves:
 *
 * FMLA_asimdsame_only  -> FMLA_advsimd_vec, source ordinal 3919
 * FADD_asimdsame_only  -> FADD_advsimd,     source ordinal 3920
 * FCMEQ_asimdsame_only -> FCMEQ_advsimd_reg, source ordinal 3922
 * FMAX_asimdsame_only  -> FMAX_advsimd,     source ordinal 3923
 * FSUB_asimdsame_only  -> FSUB_advsimd,     source ordinal 3930
 * FMIN_asimdsame_only  -> FMIN_advsimd,     source ordinal 3932
 * FMUL_asimdsame_only  -> FMUL_advsimd_vec, source ordinal 3961
 * FDIV_asimdsame_only  -> FDIV_advsimd,     source ordinal 3965
 *
 * The literal results below are fixed proof cases, not derived by a host
 * floating point calculation.  Each case runs the production decoder and
 * executor.  The target ASL ledger currently records these leaves as shared
 * ASL absent, so this suite adds execution evidence but cannot discharge that
 * separate provenance obligation.
 */
#define TCTI_FPSR_IOC	BIT(0)
#define TCTI_FPSR_DZC	BIT(1)
#define TCTI_FPSR_QC	BIT(27)

#define TCTI_ADVSIMD_FP_RD	0U
#define TCTI_ADVSIMD_FP_RN	1U
#define TCTI_ADVSIMD_FP_RM	2U
#define TCTI_ADVSIMD_FP_REG_MASK	(GENMASK(4, 0) | GENMASK(9, 5) | \
					 GENMASK(20, 16))
#define TCTI_ADVSIMD_FP_SVC	0xd4000001U

struct tcti_advsimd_fp_arithmetic_context {
	struct mm_struct *mm;
	unsigned long instructions;
	u64 simd[ARRAY_SIZE(current->thread.user_simd)];
	unsigned long simd_valid;
	unsigned long fpcr;
	unsigned long fpsr;
};

static int tcti_advsimd_fp_arithmetic_test_init(struct kunit *test)
{
	struct tcti_advsimd_fp_arithmetic_context *context;

	context = kunit_kzalloc(test, sizeof(*context), GFP_KERNEL);
	if (!context)
		return -ENOMEM;
	context->mm = mm_alloc();
	if (!context->mm)
		return -ENOMEM;
	kthread_use_mm(context->mm);
	context->instructions = ksys_mmap_pgoff(0, PAGE_SIZE,
						PROT_READ | PROT_WRITE,
						MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (IS_ERR_VALUE(context->instructions)) {
		kthread_unuse_mm(context->mm);
		mmput(context->mm);
		return -ENOMEM;
	}

	memcpy(context->simd, current->thread.user_simd, sizeof(context->simd));
	context->simd_valid = current->thread.user_simd_valid;
	context->fpcr = current->thread.user_fpcr;
	context->fpsr = current->thread.user_fpsr;
	test->priv = context;
	return 0;
}

static void tcti_advsimd_fp_arithmetic_test_exit(struct kunit *test)
{
	struct tcti_advsimd_fp_arithmetic_context *context = test->priv;

	if (context->instructions)
		vm_munmap(context->instructions, PAGE_SIZE);
	if (context->mm) {
		kthread_unuse_mm(context->mm);
		mmput(context->mm);
	}
	memcpy(current->thread.user_simd, context->simd, sizeof(context->simd));
	current->thread.user_simd_valid = context->simd_valid;
	current->thread.user_fpcr = context->fpcr;
	current->thread.user_fpsr = context->fpsr;
}

static u32 tcti_advsimd_fp_instruction(u32 pattern, u8 rd, u8 rn, u8 rm)
{
	return (pattern & ~TCTI_ADVSIMD_FP_REG_MASK) | rd | ((u32)rn << 5) |
		((u32)rm << 16);
}

static void tcti_advsimd_fp_seed_registers(struct pt_regs *regs, u32 instruction)
{
	unsigned int index;

	for (index = 0; index < ARRAY_SIZE(current->thread.user_simd); index++)
		current->thread.user_simd[index] = 0xa5a5a5a500000000ULL ^
			((u64)instruction << 7) ^ index;
	for (index = 0; index < ARRAY_SIZE(regs->regs); index++)
		regs->regs[index] = 0xd1d1d1d100000000ULL ^ index;
	regs->sp = 0x12345678a000ULL;
	regs->pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_C_BIT | PSR_V_BIT;
}

static struct tcti_result
tcti_advsimd_fp_resume_instruction(struct kunit *test, struct pt_regs *regs,
					  u32 instruction)
{
	struct tcti_advsimd_fp_arithmetic_context *context = test->priv;
	const u32 program[] = { instruction, TCTI_ADVSIMD_FP_SVC };
	int ret;

	ret = sys_mprotect(context->instructions, PAGE_SIZE,
			  PROT_READ | PROT_WRITE);
	KUNIT_ASSERT_EQ(test, 0, ret);
	ret = tcti_write_user_data(current->mm, context->instructions, program,
				   sizeof(program));
	KUNIT_ASSERT_EQ(test, 0, ret);
	ret = sys_mprotect(context->instructions, PAGE_SIZE, PROT_READ | PROT_EXEC);
	KUNIT_ASSERT_EQ(test, 0, ret);
	regs->pc = context->instructions;
	regs->pstate |= PSR_MODE_EL0t;
	regs->syscallno = NO_SYSCALL;
	return tcti_resume_user(current, regs, current->mm);
}

static void tcti_advsimd_fp_expect_resume_success(struct kunit *test,
		const struct tcti_result *result, const struct pt_regs *before,
		const struct pt_regs *after)
{
	struct tcti_advsimd_fp_arithmetic_context *context = test->priv;

	KUNIT_EXPECT_EQ(test, TCTI_EXIT_SYSCALL, result->reason);
	KUNIT_EXPECT_EQ(test, 0L, result->status);
	KUNIT_EXPECT_EQ(test, context->instructions + sizeof(u32), result->pc);
	KUNIT_EXPECT_EQ(test, TCTI_ADVSIMD_FP_SVC, result->instruction);
	KUNIT_EXPECT_EQ(test, context->instructions + sizeof(u32), after->pc);
	KUNIT_EXPECT_MEMEQ(test, before->regs, after->regs, sizeof(before->regs));
	KUNIT_EXPECT_EQ(test, before->sp, after->sp);
	KUNIT_EXPECT_EQ(test, before->pstate, after->pstate);
}

static void tcti_advsimd_fp_execute_2s(struct kunit *test, u32 pattern,
	enum tcti_simd_vector_arithmetic_op operation, u32 left0, u32 left1,
	u32 right0, u32 right1, u32 accumulator0, u32 accumulator1,
	u32 expected0, u32 expected1, unsigned long fpcr,
	unsigned long initial_fpsr, unsigned long expected_fpsr)
{
	u32 instruction = tcti_advsimd_fp_instruction(pattern,
		TCTI_ADVSIMD_FP_RD, TCTI_ADVSIMD_FP_RN, TCTI_ADVSIMD_FP_RM);
	struct tcti_decoded_instruction decoded = tcti_decode_aarch64(instruction);
	struct pt_regs regs = {};
	struct pt_regs before_regs;
	struct tcti_result result;
	u64 before_simd[ARRAY_SIZE(current->thread.user_simd)];
	u64 expected_simd[ARRAY_SIZE(current->thread.user_simd)];

	KUNIT_ASSERT_EQ(test, TCTI_DECODE_SIMD_VECTOR_ARITHMETIC,
		decoded.decode_class);
	KUNIT_ASSERT_EQ(test, operation, decoded.simd_arithmetic_op);
	KUNIT_ASSERT_FALSE(test, decoded.simd_scalar);
	KUNIT_ASSERT_FALSE(test, decoded.simd_q);
	KUNIT_ASSERT_EQ(test, sizeof(u32), decoded.access_size);
	KUNIT_ASSERT_EQ(test, sizeof(u64), decoded.result_size);

	tcti_advsimd_fp_seed_registers(&regs, instruction);
	current->thread.user_simd[TCTI_ADVSIMD_FP_RD * 2] = accumulator0 |
		((u64)accumulator1 << 32);
	current->thread.user_simd[TCTI_ADVSIMD_FP_RN * 2] = left0 |
		((u64)left1 << 32);
	current->thread.user_simd[TCTI_ADVSIMD_FP_RM * 2] = right0 |
		((u64)right1 << 32);
	memcpy(before_simd, current->thread.user_simd, sizeof(before_simd));
	memcpy(expected_simd, before_simd, sizeof(expected_simd));
	expected_simd[TCTI_ADVSIMD_FP_RD * 2] = expected0 |
		((u64)expected1 << 32);
	expected_simd[TCTI_ADVSIMD_FP_RD * 2 + 1] = 0;
	current->thread.user_simd_valid = 0;
	current->thread.user_fpcr = fpcr;
	current->thread.user_fpsr = initial_fpsr;
	before_regs = regs;
	before_regs.pc = ((struct tcti_advsimd_fp_arithmetic_context *)test->priv)->instructions;
	result = tcti_advsimd_fp_resume_instruction(test, &regs, instruction);
	KUNIT_EXPECT_MEMEQ(test, expected_simd, current->thread.user_simd,
			   sizeof(expected_simd));
	KUNIT_EXPECT_EQ(test, 1UL, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, fpcr, current->thread.user_fpcr);
	KUNIT_EXPECT_EQ(test, expected_fpsr, current->thread.user_fpsr);
	tcti_advsimd_fp_expect_resume_success(test, &result, &before_regs, &regs);
}

static void tcti_advsimd_fp_execute_2d(struct kunit *test, u32 pattern,
	enum tcti_simd_vector_arithmetic_op operation, u64 left0, u64 left1,
	u64 right0, u64 right1, u64 accumulator0, u64 accumulator1,
	u64 expected0, u64 expected1)
{
	u32 instruction = tcti_advsimd_fp_instruction(pattern | BIT(22),
		TCTI_ADVSIMD_FP_RD, TCTI_ADVSIMD_FP_RN, TCTI_ADVSIMD_FP_RM);
	struct tcti_decoded_instruction decoded = tcti_decode_aarch64(instruction);
	struct pt_regs regs = {};
	struct pt_regs before_regs;
	struct tcti_result result;
	u64 before_simd[ARRAY_SIZE(current->thread.user_simd)];
	u64 expected_simd[ARRAY_SIZE(current->thread.user_simd)];

	KUNIT_ASSERT_EQ(test, TCTI_DECODE_SIMD_VECTOR_ARITHMETIC,
		decoded.decode_class);
	KUNIT_ASSERT_EQ(test, operation, decoded.simd_arithmetic_op);
	KUNIT_ASSERT_FALSE(test, decoded.simd_scalar);
	KUNIT_ASSERT_TRUE(test, decoded.simd_q);
	KUNIT_ASSERT_EQ(test, sizeof(u64), decoded.access_size);
	KUNIT_ASSERT_EQ(test, 2 * sizeof(u64), decoded.result_size);

	tcti_advsimd_fp_seed_registers(&regs, instruction);
	current->thread.user_simd[TCTI_ADVSIMD_FP_RD * 2] = accumulator0;
	current->thread.user_simd[TCTI_ADVSIMD_FP_RD * 2 + 1] = accumulator1;
	current->thread.user_simd[TCTI_ADVSIMD_FP_RN * 2] = left0;
	current->thread.user_simd[TCTI_ADVSIMD_FP_RN * 2 + 1] = left1;
	current->thread.user_simd[TCTI_ADVSIMD_FP_RM * 2] = right0;
	current->thread.user_simd[TCTI_ADVSIMD_FP_RM * 2 + 1] = right1;
	memcpy(before_simd, current->thread.user_simd, sizeof(before_simd));
	memcpy(expected_simd, before_simd, sizeof(expected_simd));
	expected_simd[TCTI_ADVSIMD_FP_RD * 2] = expected0;
	expected_simd[TCTI_ADVSIMD_FP_RD * 2 + 1] = expected1;
	current->thread.user_simd_valid = 0;
	current->thread.user_fpcr = 0;
	current->thread.user_fpsr = TCTI_FPSR_QC;
	before_regs = regs;
	before_regs.pc = ((struct tcti_advsimd_fp_arithmetic_context *)test->priv)->instructions;
	result = tcti_advsimd_fp_resume_instruction(test, &regs, instruction);
	KUNIT_EXPECT_MEMEQ(test, expected_simd, current->thread.user_simd,
			   sizeof(expected_simd));
	KUNIT_EXPECT_EQ(test, TCTI_FPSR_QC, current->thread.user_fpsr);
	tcti_advsimd_fp_expect_resume_success(test, &result, &before_regs, &regs);
}

static void tcti_advsimd_fp_required_source_leaves_execute_exact_bits(
	struct kunit *test)
{
	struct tcti_advsimd_fp_case {
		u32 pattern;
		enum tcti_simd_vector_arithmetic_op operation;
		u32 expected0;
		u32 expected1;
	};
	static const struct tcti_advsimd_fp_case cases[] = {
		{ 0x0e20cc00U, TCTI_SIMD_ARITH_FMLA, 0x41400000U, 0x40e00000U },
		{ 0x0e20d400U, TCTI_SIMD_ARITH_FADD, 0x40400000U, 0x40000000U },
		{ 0x0e20e400U, TCTI_SIMD_ARITH_FCMEQ, 0, 0 },
		{ 0x0e20f400U, TCTI_SIMD_ARITH_FMAX, 0x40000000U, 0x40400000U },
		{ 0x0ea0d400U, TCTI_SIMD_ARITH_FSUB, 0xbf800000U, 0x40800000U },
		{ 0x0ea0f400U, TCTI_SIMD_ARITH_FMIN, 0x3f800000U, 0xbf800000U },
		{ 0x2e20dc00U, TCTI_SIMD_ARITH_FMUL, 0x40000000U, 0xc0400000U },
		{ 0x2e20fc00U, TCTI_SIMD_ARITH_FDIV, 0x3f000000U, 0xc0400000U },
	};
	size_t index;

	for (index = 0; index < ARRAY_SIZE(cases); index++)
		tcti_advsimd_fp_execute_2s(test, cases[index].pattern,
			cases[index].operation, 0x3f800000U, 0x40400000U,
			0x40000000U, 0xbf800000U, 0x41200000U, 0x41200000U,
			cases[index].expected0, cases[index].expected1,
			0, TCTI_FPSR_QC, TCTI_FPSR_QC);
}

static void tcti_advsimd_fp_2d_required_variants_execute_exact_bits(
	struct kunit *test)
{
	struct tcti_advsimd_fp_case {
		u32 pattern;
		enum tcti_simd_vector_arithmetic_op operation;
		u64 expected0;
		u64 expected1;
	};
	static const struct tcti_advsimd_fp_case cases[] = {
		{ 0x0e20cc00U, TCTI_SIMD_ARITH_FMLA,
		  0x4028000000000000ULL, 0x401c000000000000ULL },
		{ 0x0e20d400U, TCTI_SIMD_ARITH_FADD,
		  0x4008000000000000ULL, 0x4000000000000000ULL },
		{ 0x0e20e400U, TCTI_SIMD_ARITH_FCMEQ, 0, 0 },
		{ 0x0e20f400U, TCTI_SIMD_ARITH_FMAX,
		  0x4000000000000000ULL, 0x4008000000000000ULL },
		{ 0x0ea0d400U, TCTI_SIMD_ARITH_FSUB,
		  0xbff0000000000000ULL, 0x4010000000000000ULL },
		{ 0x0ea0f400U, TCTI_SIMD_ARITH_FMIN,
		  0x3ff0000000000000ULL, 0xbff0000000000000ULL },
		{ 0x2e20fc00U, TCTI_SIMD_ARITH_FDIV,
		  0x3fe0000000000000ULL, 0xc008000000000000ULL },
	};
	size_t index;

	for (index = 0; index < ARRAY_SIZE(cases); index++)
		tcti_advsimd_fp_execute_2d(test, cases[index].pattern,
			cases[index].operation, 0x3ff0000000000000ULL,
			0x4008000000000000ULL, 0x4000000000000000ULL,
			0xbff0000000000000ULL, 0x4024000000000000ULL,
			0x4024000000000000ULL, cases[index].expected0,
			cases[index].expected1);
}

static void tcti_advsimd_fp_special_values_update_fpsr_exactly(
	struct kunit *test)
{
	/* FDIV: 1 / 0 raises DZC, while 0 / 0 also raises IOC. */
	tcti_advsimd_fp_execute_2s(test, 0x2e20fc00U,
		TCTI_SIMD_ARITH_FDIV, 0x3f800000U, 0,
		0, 0, 0, 0, 0x7f800000U, 0x7fc00000U, 0,
		TCTI_FPSR_QC, TCTI_FPSR_QC | TCTI_FPSR_DZC | TCTI_FPSR_IOC);

	/* FADD quiets sNaN and records IOC without disturbing the other lane. */
	tcti_advsimd_fp_execute_2s(test, 0x0e20d400U,
		TCTI_SIMD_ARITH_FADD, 0x7f800001U, 0x3f800000U,
		0x3f800000U, 0x40000000U, 0, 0,
		0x7fc00001U, 0x40400000U, 0,
		TCTI_FPSR_QC, TCTI_FPSR_QC | TCTI_FPSR_IOC);

	/* FMAX and FMIN retain their distinct signed-zero choice. */
	tcti_advsimd_fp_execute_2s(test, 0x0e20f400U,
		TCTI_SIMD_ARITH_FMAX, BIT(31), 0x3f800000U,
		0, 0x3f800000U, 0, 0, 0, 0x3f800000U, 0,
		TCTI_FPSR_QC, TCTI_FPSR_QC);
	tcti_advsimd_fp_execute_2s(test, 0x0ea0f400U,
		TCTI_SIMD_ARITH_FMIN, BIT(31), 0x3f800000U,
		0, 0x3f800000U, 0, 0, BIT(31), 0x3f800000U, 0,
		TCTI_FPSR_QC, TCTI_FPSR_QC);
}

static void tcti_advsimd_fp_fmla_register_alias_reads_accumulator_first(
	struct kunit *test)
{
	u32 instruction = tcti_advsimd_fp_instruction(0x0e20cc00U, 1, 1, 2);
	struct tcti_decoded_instruction decoded = tcti_decode_aarch64(instruction);
	struct pt_regs regs = {};
	struct pt_regs before_regs;
	struct tcti_result result;
	u64 before_simd[ARRAY_SIZE(current->thread.user_simd)];

	KUNIT_ASSERT_EQ(test, TCTI_DECODE_SIMD_VECTOR_ARITHMETIC,
		decoded.decode_class);
	KUNIT_ASSERT_EQ(test, TCTI_SIMD_ARITH_FMLA, decoded.simd_arithmetic_op);
	tcti_advsimd_fp_seed_registers(&regs, instruction);
	current->thread.user_simd[2] = 0x4000000040000000ULL; /* v1.2s = 2, 2 */
	current->thread.user_simd[4] = 0x4040000040400000ULL; /* v2.2s = 3, 3 */
	memcpy(before_simd, current->thread.user_simd, sizeof(before_simd));
	current->thread.user_fpcr = BIT(22);
	current->thread.user_fpsr = TCTI_FPSR_QC;
	before_regs = regs;
	before_regs.pc = ((struct tcti_advsimd_fp_arithmetic_context *)test->priv)->instructions;
	result = tcti_advsimd_fp_resume_instruction(test, &regs, instruction);
	KUNIT_EXPECT_EQ(test, 0x4100000041000000ULL,
		current->thread.user_simd[2]); /* 2 + 2 * 3 = 8 */
	KUNIT_EXPECT_EQ(test, 0ULL, current->thread.user_simd[3]);
	KUNIT_EXPECT_EQ(test, before_simd[4], current->thread.user_simd[4]);
	KUNIT_EXPECT_EQ(test, BIT(22), current->thread.user_fpcr);
	KUNIT_EXPECT_EQ(test, TCTI_FPSR_QC, current->thread.user_fpsr);
	tcti_advsimd_fp_expect_resume_success(test, &result, &before_regs, &regs);
}

static void tcti_advsimd_fp_reserved_form_is_rejected_without_state_change(
	struct kunit *test)
{
	/* FADD vector D form with Q == 0 is reserved. */
	const u32 instruction = 0x0e20d400U | BIT(22);
	struct tcti_decoded_instruction decoded = tcti_decode_aarch64(instruction);
	struct pt_regs regs = {};
	struct pt_regs before_regs;
	struct tcti_result result;
	u64 before_simd[ARRAY_SIZE(current->thread.user_simd)];

	KUNIT_ASSERT_EQ(test, TCTI_DECODE_UNSUPPORTED, decoded.decode_class);
	tcti_advsimd_fp_seed_registers(&regs, instruction);
	current->thread.user_fpcr = BIT(22);
	current->thread.user_fpsr = TCTI_FPSR_QC;
	memcpy(before_simd, current->thread.user_simd, sizeof(before_simd));
	before_regs = regs;
	before_regs.pc = ((struct tcti_advsimd_fp_arithmetic_context *)test->priv)->instructions;
	result = tcti_advsimd_fp_resume_instruction(test, &regs, instruction);

	KUNIT_EXPECT_EQ(test, TCTI_EXIT_UNSUPPORTED_INSTRUCTION, result.reason);
	KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, result.status);
	KUNIT_EXPECT_EQ(test, before_regs.pc, result.pc);
	KUNIT_EXPECT_EQ(test, instruction, result.instruction);
	KUNIT_EXPECT_MEMEQ(test, before_simd, current->thread.user_simd,
			   sizeof(before_simd));
	KUNIT_EXPECT_EQ(test, BIT(22), current->thread.user_fpcr);
	KUNIT_EXPECT_EQ(test, TCTI_FPSR_QC, current->thread.user_fpsr);
	KUNIT_EXPECT_MEMEQ(test, before_regs.regs, regs.regs,
			   sizeof(regs.regs));
	KUNIT_EXPECT_EQ(test, before_regs.sp, regs.sp);
	KUNIT_EXPECT_EQ(test, before_regs.pstate, regs.pstate);
	KUNIT_EXPECT_EQ(test, before_regs.pc, regs.pc);
}

static struct kunit_case tcti_advsimd_fp_arithmetic_test_cases[] = {
	KUNIT_CASE(tcti_advsimd_fp_required_source_leaves_execute_exact_bits),
	KUNIT_CASE(tcti_advsimd_fp_2d_required_variants_execute_exact_bits),
	KUNIT_CASE(tcti_advsimd_fp_special_values_update_fpsr_exactly),
	KUNIT_CASE(tcti_advsimd_fp_fmla_register_alias_reads_accumulator_first),
	KUNIT_CASE(tcti_advsimd_fp_reserved_form_is_rejected_without_state_change),
	{}
};

static struct kunit_suite tcti_advsimd_fp_arithmetic_test_suite = {
	.name = "orlix-tcti-advsimd-fp-arithmetic-source-bound",
	.init = tcti_advsimd_fp_arithmetic_test_init,
	.exit = tcti_advsimd_fp_arithmetic_test_exit,
	.test_cases = tcti_advsimd_fp_arithmetic_test_cases,
};

kunit_test_suite(tcti_advsimd_fp_arithmetic_test_suite);

MODULE_LICENSE("GPL");
