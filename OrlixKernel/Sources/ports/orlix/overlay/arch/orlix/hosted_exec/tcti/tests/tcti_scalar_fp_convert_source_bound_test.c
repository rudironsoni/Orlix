// SPDX-License-Identifier: GPL-2.0-only
/*
 * Production-path proof for the two baseline scalar precision-conversion
 * leaves. Expected values are IEEE-754 bit patterns written by hand from the
 * AArch64 FCVT rules, never values produced by a host floating-point oracle.
 */
#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/tcti.h>
#include <kunit/test.h>
#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/string.h>
#include <linux/syscalls.h>

#include "../decode_aarch64.h"

#define FCVT_SVC		0xd4000001U
#define FCVT_RD		7U
#define FCVT_RN		19U
#define FCVT_FPSR_IOC		BIT(0)
#define FCVT_FPSR_IXC		BIT(4)
#define FCVT_FPSR_QC		BIT(27)
#define FCVT_FPCR_RP		BIT(22)
#define FCVT_FPCR_DN		BIT(25)

struct fcvt_manifest_leaf {
	u32 ordinal;
	const char *name;
	const char *mnemonic;
	const char *operation;
	u32 mask;
	u32 pattern;
	const char *feature;
	u32 offset;
	u32 length;
};

#define TCTI_A64_SOURCE_MANIFEST_SOURCE(...)
#define TCTI_A64_SOURCE_MANIFEST_ROW(ordinal, name, mnemonic, operation, \
	mask, pattern, feature, offset, length) \
	{ ordinal, name, mnemonic, operation, mask, pattern, feature, offset, length },
static const struct fcvt_manifest_leaf fcvt_manifest[] = {
#include "../isa/source_manifest.def"
};
#undef TCTI_A64_SOURCE_MANIFEST_ROW
#undef TCTI_A64_SOURCE_MANIFEST_SOURCE

struct fcvt_leaf {
	u16 ordinal;
	const char *name;
	u32 mask;
	u32 pattern;
	u8 access_size;
	u8 result_size;
};

/* Pinned AARCHMRS 2026-06 leaves 4243 and 4260, both FEAT_FP. */
static const struct fcvt_leaf fcvt_leaves[] = {
	{ 4243U, "FCVT_DS_floatdp1", 0xfffffc00U, 0x1e22c000U,
	  sizeof(u32), sizeof(u64) },
	{ 4260U, "FCVT_SD_floatdp1", 0xfffffc00U, 0x1e624000U,
	  sizeof(u64), sizeof(u32) },
};

struct fcvt_context {
	struct mm_struct *mm;
	u64 simd[ARRAY_SIZE(current->thread.user_simd)];
	unsigned long simd_valid;
	unsigned long fpcr;
	unsigned long fpsr;
};

static int fcvt_test_init(struct kunit *test)
{
	struct fcvt_context *context;

	context = kunit_kzalloc(test, sizeof(*context), GFP_KERNEL);
	if (!context)
		return -ENOMEM;
	context->mm = mm_alloc();
	if (!context->mm)
		return -ENOMEM;
	kthread_use_mm(context->mm);
	memcpy(context->simd, current->thread.user_simd, sizeof(context->simd));
	context->simd_valid = current->thread.user_simd_valid;
	context->fpcr = current->thread.user_fpcr;
	context->fpsr = current->thread.user_fpsr;
	test->priv = context;
	return 0;
}

static void fcvt_test_exit(struct kunit *test)
{
	struct fcvt_context *context = test->priv;

	memcpy(current->thread.user_simd, context->simd, sizeof(context->simd));
	current->thread.user_simd_valid = context->simd_valid;
	current->thread.user_fpcr = context->fpcr;
	current->thread.user_fpsr = context->fpsr;
	kthread_unuse_mm(context->mm);
	mmput(context->mm);
}

static const struct fcvt_manifest_leaf *fcvt_manifest_by_ordinal(u16 ordinal)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(fcvt_manifest); index++)
		if (fcvt_manifest[index].ordinal == ordinal)
			return &fcvt_manifest[index];
	return NULL;
}

static u32 fcvt_instruction(const struct fcvt_leaf *leaf, u8 rd, u8 rn)
{
	return leaf->pattern | rd | ((u32)rn << 5);
}

static unsigned long fcvt_map_program(struct kunit *test, u32 instruction)
{
	const u32 program[] = { instruction, FCVT_SVC };
	unsigned long address;
	int ret;

	address = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(address));
	ret = tcti_write_user_data(current->mm, address, program, sizeof(program));
	KUNIT_ASSERT_EQ(test, 0, ret);
	ret = sys_mprotect(address, PAGE_SIZE, PROT_READ | PROT_EXEC);
	KUNIT_ASSERT_EQ(test, 0, ret);
	return address;
}

static void fcvt_seed(struct pt_regs *regs, u32 instruction, unsigned long pc)
{
	unsigned int index;

	memset(regs, 0, sizeof(*regs));
	for (index = 0; index < ARRAY_SIZE(regs->regs); index++)
		regs->regs[index] = 0x9e3779b97f4a7c15ULL ^ index;
	regs->sp = 0x00000001fffffff0ULL;
	regs->pc = pc;
	regs->pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_C_BIT | PSR_V_BIT;
	regs->syscallno = NO_SYSCALL;
	for (index = 0; index < ARRAY_SIZE(current->thread.user_simd); index++)
		current->thread.user_simd[index] = 0xa5a5000000000000ULL ^
			((u64)instruction << 11) ^ index;
}

static void fcvt_expect_execution(struct kunit *test,
	const struct fcvt_leaf *leaf, u64 source, u64 expected,
	unsigned long fpcr, unsigned long expected_fpsr)
{
	u32 instruction = fcvt_instruction(leaf, FCVT_RD, FCVT_RN);
	const u32 program[] = { instruction, FCVT_SVC };
	struct tcti_decoded_instruction decoded = tcti_decode_aarch64(instruction);
	struct pt_regs regs;
	struct pt_regs before;
	struct tcti_result result;
	u64 expected_simd[ARRAY_SIZE(current->thread.user_simd)];
	unsigned long address;
	int ret;

	KUNIT_ASSERT_EQ(test, TCTI_DECODE_FP_SCALAR_1SOURCE,
		decoded.decode_class);
	KUNIT_ASSERT_EQ(test, TCTI_FP1_FCVT, decoded.fp1_op);
	KUNIT_ASSERT_EQ(test, leaf->access_size, decoded.access_size);
	KUNIT_ASSERT_EQ(test, leaf->result_size, decoded.result_size);
	address = fcvt_map_program(test, instruction);
	fcvt_seed(&regs, instruction, address);
	current->thread.user_simd[FCVT_RN * 2] = source;
	memcpy(expected_simd, current->thread.user_simd, sizeof(expected_simd));
	expected_simd[FCVT_RD * 2] = expected;
	expected_simd[FCVT_RD * 2 + 1] = 0;
	current->thread.user_simd_valid = 0;
	current->thread.user_fpcr = fpcr;
	current->thread.user_fpsr = FCVT_FPSR_QC;
	before = regs;
	result = tcti_resume_user(current, &regs, current->mm);
	before.pc = address + sizeof(u32);
	KUNIT_EXPECT_EQ(test, TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, 0L, result.status);
	KUNIT_EXPECT_EQ(test, address + sizeof(u32), result.pc);
	KUNIT_EXPECT_EQ(test, FCVT_SVC, result.instruction);
	KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
	KUNIT_EXPECT_MEMEQ(test, expected_simd, current->thread.user_simd,
			   sizeof(expected_simd));
	KUNIT_EXPECT_EQ(test, 1UL, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, fpcr, current->thread.user_fpcr);
	KUNIT_EXPECT_EQ(test, expected_fpsr, current->thread.user_fpsr);
	ret = sys_mprotect(address, PAGE_SIZE, PROT_READ | PROT_WRITE);
	KUNIT_ASSERT_EQ(test, 0, ret);
	{
		u32 observed[2] = {};

		ret = tcti_read_user_data(current->mm, address, observed,
					 sizeof(observed));
		KUNIT_EXPECT_EQ(test, 0, ret);
		KUNIT_EXPECT_MEMEQ(test, program, observed, sizeof(program));
	}
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(address, PAGE_SIZE));
}

static void fcvt_leaves_bind_and_decode_exhaustively(struct kunit *test)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(fcvt_leaves); index++) {
		const struct fcvt_leaf *leaf = &fcvt_leaves[index];
		const struct fcvt_manifest_leaf *source =
			fcvt_manifest_by_ordinal(leaf->ordinal);
		u8 rd;

		KUNIT_ASSERT_NOT_NULL(test, source);
		KUNIT_EXPECT_STREQ(test, leaf->name, source->name);
		KUNIT_EXPECT_STREQ(test, "FCVT", source->mnemonic);
		KUNIT_EXPECT_STREQ(test, "FCVT_float", source->operation);
		KUNIT_EXPECT_EQ(test, leaf->mask, source->mask);
		KUNIT_EXPECT_EQ(test, leaf->pattern, source->pattern);
		for (rd = 0; rd < 32; rd++) {
			u8 rn;

			for (rn = 0; rn < 32; rn++) {
				struct tcti_decoded_instruction decoded =
					tcti_decode_aarch64(fcvt_instruction(leaf, rd, rn));

				KUNIT_EXPECT_EQ(test, TCTI_DECODE_FP_SCALAR_1SOURCE,
					decoded.decode_class);
				KUNIT_EXPECT_EQ(test, TCTI_FP1_FCVT, decoded.fp1_op);
				KUNIT_EXPECT_EQ(test, rd, decoded.rd);
				KUNIT_EXPECT_EQ(test, rn, decoded.rn);
				KUNIT_EXPECT_EQ(test, leaf->access_size,
					decoded.access_size);
				KUNIT_EXPECT_EQ(test, leaf->result_size,
					decoded.result_size);
			}
		}
		for (rd = 10; rd < 32; rd++) {
			struct tcti_decoded_instruction decoded = tcti_decode_aarch64(
				fcvt_instruction(leaf, FCVT_RD, FCVT_RN) ^ BIT(rd));

			if (!(leaf->mask & BIT(rd)))
				continue;
			KUNIT_EXPECT_FALSE(test,
				decoded.decode_class == TCTI_DECODE_FP_SCALAR_1SOURCE &&
				decoded.fp1_op == TCTI_FP1_FCVT &&
				decoded.access_size == leaf->access_size &&
				decoded.result_size == leaf->result_size);
		}
	}
}

static void fcvt_leaves_execute_exact_bit_contracts(struct kunit *test)
{
	const struct fcvt_leaf *s_to_d = &fcvt_leaves[0];
	const struct fcvt_leaf *d_to_s = &fcvt_leaves[1];

	/* signed zeros, infinities, exact finite boundaries, and payload mapping */
	fcvt_expect_execution(test, s_to_d, 0x80000000U,
		0x8000000000000000ULL, 0, FCVT_FPSR_QC);
	fcvt_expect_execution(test, s_to_d, 0x7f800000U,
		0x7ff0000000000000ULL, 0, FCVT_FPSR_QC);
	fcvt_expect_execution(test, s_to_d, 0xff800000U,
		0xfff0000000000000ULL, 0, FCVT_FPSR_QC);
	fcvt_expect_execution(test, s_to_d, 0x7f7fffffU,
		0x47efffffe0000000ULL, 0, FCVT_FPSR_QC);
	fcvt_expect_execution(test, s_to_d, 0x7fc00001U,
		0x7ff8000020000000ULL, 0, FCVT_FPSR_QC);
	fcvt_expect_execution(test, s_to_d, 0x7f800001U,
		0x7ff8000020000000ULL, 0, FCVT_FPSR_QC | FCVT_FPSR_IOC);
	fcvt_expect_execution(test, s_to_d, 0x7f800001U,
		0x7ff8000000000000ULL, FCVT_FPCR_DN,
		FCVT_FPSR_QC | FCVT_FPSR_IOC);

	fcvt_expect_execution(test, d_to_s, 0x8000000000000000ULL,
		0x80000000U, 0, FCVT_FPSR_QC);
	fcvt_expect_execution(test, d_to_s, 0x7ff0000000000000ULL,
		0x7f800000U, 0, FCVT_FPSR_QC);
	fcvt_expect_execution(test, d_to_s, 0xfff0000000000000ULL,
		0xff800000U, 0, FCVT_FPSR_QC);
	fcvt_expect_execution(test, d_to_s, 0x47efffffe0000000ULL,
		0x7f7fffffU, 0, FCVT_FPSR_QC);
	fcvt_expect_execution(test, d_to_s, 0x7ff8000020000000ULL,
		0x7fc00001U, 0, FCVT_FPSR_QC);
	fcvt_expect_execution(test, d_to_s, 0x7ff0000020000000ULL,
		0x7fc00001U, 0, FCVT_FPSR_QC | FCVT_FPSR_IOC);
	fcvt_expect_execution(test, d_to_s, 0x7ff0000020000000ULL,
		0x7fc00000U, FCVT_FPCR_DN, FCVT_FPSR_QC | FCVT_FPSR_IOC);

	/* Tie-to-even and round-toward-positive share the same source bits. */
	fcvt_expect_execution(test, d_to_s, 0x3ff0000010000000ULL,
		0x3f800000U, 0, FCVT_FPSR_QC | FCVT_FPSR_IXC);
	fcvt_expect_execution(test, d_to_s, 0x3ff0000010000000ULL,
		0x3f800001U, FCVT_FPCR_RP, FCVT_FPSR_QC | FCVT_FPSR_IXC);
}

static struct kunit_case fcvt_cases[] = {
	KUNIT_CASE(fcvt_leaves_bind_and_decode_exhaustively),
	KUNIT_CASE(fcvt_leaves_execute_exact_bit_contracts),
	{}
};

static struct kunit_suite fcvt_suite = {
	.name = "orlix-tcti-scalar-fp-convert-source-bound",
	.init = fcvt_test_init,
	.exit = fcvt_test_exit,
	.test_cases = fcvt_cases,
};
kunit_test_suite(fcvt_suite);

MODULE_LICENSE("GPL");
