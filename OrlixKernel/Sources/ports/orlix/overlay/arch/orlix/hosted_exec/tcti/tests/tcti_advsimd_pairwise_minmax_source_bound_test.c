// SPDX-License-Identifier: GPL-2.0-only
/*
 * Production-path evidence for the four classic AdvSIMD integer pairwise
 * minimum and maximum source leaves.  Expected vectors are worked literals,
 * independent of the executor implementation.
 */
#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/tcti.h>
#include <kunit/test.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/syscalls.h>

#include "../decode_aarch64.h"
#include "target_instruction_artifact.h"

#define PAIRWISE_SVC 0xd4000001U
#define PAIRWISE_RD 4U
#define PAIRWISE_RN 5U
#define PAIRWISE_RM 6U

struct pairwise_leaf {
	u16 ordinal;
	const char *name;
	const char *mnemonic;
	const char *operation;
	u32 mask;
	u32 pattern;
	enum tcti_simd_vector_arithmetic_op arithmetic;
	u8 expected_index;
};

static const struct pairwise_leaf pairwise_leaves[] = {
	{ 3914U, "SMAXP_asimdsame_only", "SMAXP", "SMAXP_advsimd", 0xbf20fc00U,
	  0x0e20a400U, TCTI_SIMD_ARITH_SMAXP, 0 },
	{ 3915U, "SMINP_asimdsame_only", "SMINP", "SMINP_advsimd", 0xbf20fc00U,
	  0x0e20ac00U, TCTI_SIMD_ARITH_SMINP, 1 },
	{ 3956U, "UMAXP_asimdsame_only", "UMAXP", "UMAXP_advsimd", 0xbf20fc00U,
	  0x2e20a400U, TCTI_SIMD_ARITH_UMAXP, 2 },
	{ 3957U, "UMINP_asimdsame_only", "UMINP", "UMINP_advsimd", 0xbf20fc00U,
	  0x2e20ac00U, TCTI_SIMD_ARITH_UMINP, 3 },
};

struct pairwise_vector {
	u8 q;
	u8 size;
	u64 left[2];
	u64 right[2];
	u64 expected[4][2];
	u64 same_left_expected[4][2];
};

/* expected[] is ordered SMAXP, SMINP, UMAXP, UMINP. */
static const struct pairwise_vector pairwise_vectors[] = {
	{
		.q = 0, .size = 0,
		.left = { 0x0201818200ff7f80ULL, 0xa5a5a5a5a5a5a5a5ULL },
		.right = { 0x0605807f040301feULL, 0x5a5a5a5a5a5a5a5aULL },
		.expected = {
			{ 0x067f04010282007fULL, 0 },
			{ 0x058003fe0181ff80ULL, 0 },
			{ 0x068004fe0282ff80ULL, 0 },
			{ 0x057f03010181007fULL, 0 },
		},
		.same_left_expected = {
			{ 0x0282007f0282007fULL, 0 },
			{ 0x0181ff800181ff80ULL, 0 },
			{ 0x0282ff800282ff80ULL, 0 },
			{ 0x0181007f0181007fULL, 0 },
		},
	},
	{
		.q = 1, .size = 0,
		.left = { 0x0201818200ff7f80ULL, 0x0201818200ff7f80ULL },
		.right = { 0x0605807f040301feULL, 0x0605807f040301feULL },
		.expected = {
			{ 0x0282007f0282007fULL, 0x067f0401067f0401ULL },
			{ 0x0181ff800181ff80ULL, 0x058003fe058003feULL },
			{ 0x0282ff800282ff80ULL, 0x068004fe068004feULL },
			{ 0x0181007f0181007fULL, 0x057f0301057f0301ULL },
		},
		.same_left_expected = {
			{ 0x0282007f0282007fULL, 0x0282007f0282007fULL },
			{ 0x0181ff800181ff80ULL, 0x0181ff800181ff80ULL },
			{ 0x0282ff800282ff80ULL, 0x0282ff800282ff80ULL },
			{ 0x0181007f0181007fULL, 0x0181007f0181007fULL },
		},
	},
	{
		.q = 0, .size = 1,
		.left = { 0x0000ffff7fff8000ULL, 0xa5a5a5a5a5a5a5a5ULL },
		.right = { 0x000400030001fffeULL, 0x5a5a5a5a5a5a5a5aULL },
		.expected = {
			{ 0x0004000100007fffULL, 0 },
			{ 0x0003fffeffff8000ULL, 0 },
			{ 0x0004fffeffff8000ULL, 0 },
			{ 0x0003000100007fffULL, 0 },
		},
		.same_left_expected = {
			{ 0x00007fff00007fffULL, 0 },
			{ 0xffff8000ffff8000ULL, 0 },
			{ 0xffff8000ffff8000ULL, 0 },
			{ 0x00007fff00007fffULL, 0 },
		},
	},
	{
		.q = 1, .size = 1,
		.left = { 0x0000ffff7fff8000ULL, 0x0000ffff7fff8000ULL },
		.right = { 0x000400030001fffeULL, 0x000400030001fffeULL },
		.expected = {
			{ 0x00007fff00007fffULL, 0x0004000100040001ULL },
			{ 0xffff8000ffff8000ULL, 0x0003fffe0003fffeULL },
			{ 0xffff8000ffff8000ULL, 0x0004fffe0004fffeULL },
			{ 0x00007fff00007fffULL, 0x0003000100030001ULL },
		},
		.same_left_expected = {
			{ 0x00007fff00007fffULL, 0x00007fff00007fffULL },
			{ 0xffff8000ffff8000ULL, 0xffff8000ffff8000ULL },
			{ 0xffff8000ffff8000ULL, 0xffff8000ffff8000ULL },
			{ 0x00007fff00007fffULL, 0x00007fff00007fffULL },
		},
	},
	{
		.q = 0, .size = 2,
		.left = { 0x7fffffff80000000ULL, 0xa5a5a5a5a5a5a5a5ULL },
		.right = { 0x00000001fffffffeULL, 0x5a5a5a5a5a5a5a5aULL },
		.expected = {
			{ 0x000000017fffffffULL, 0 },
			{ 0xfffffffe80000000ULL, 0 },
			{ 0xfffffffe80000000ULL, 0 },
			{ 0x000000017fffffffULL, 0 },
		},
		.same_left_expected = {
			{ 0x7fffffff7fffffffULL, 0 },
			{ 0x8000000080000000ULL, 0 },
			{ 0x8000000080000000ULL, 0 },
			{ 0x7fffffff7fffffffULL, 0 },
		},
	},
	{
		.q = 1, .size = 2,
		.left = { 0x7fffffff80000000ULL, 0x7fffffff80000000ULL },
		.right = { 0x00000001fffffffeULL, 0x00000001fffffffeULL },
		.expected = {
			{ 0x7fffffff7fffffffULL, 0x0000000100000001ULL },
			{ 0x8000000080000000ULL, 0xfffffffefffffffeULL },
			{ 0x8000000080000000ULL, 0xfffffffefffffffeULL },
			{ 0x7fffffff7fffffffULL, 0x0000000100000001ULL },
		},
		.same_left_expected = {
			{ 0x7fffffff7fffffffULL, 0x7fffffff7fffffffULL },
			{ 0x8000000080000000ULL, 0x8000000080000000ULL },
			{ 0x8000000080000000ULL, 0x8000000080000000ULL },
			{ 0x7fffffff7fffffffULL, 0x7fffffff7fffffffULL },
		},
	},
};

struct pairwise_context {
	u64 simd[ARRAY_SIZE(current->thread.user_simd)];
	unsigned long simd_valid;
	unsigned long fpcr;
	unsigned long fpsr;
};

static int pairwise_test_init(struct kunit *test)
{
	struct pairwise_context *context;

	context = kunit_kzalloc(test, sizeof(*context), GFP_KERNEL);
	if (!context)
		return -ENOMEM;
	memcpy(context->simd, current->thread.user_simd, sizeof(context->simd));
	context->simd_valid = current->thread.user_simd_valid;
	context->fpcr = current->thread.user_fpcr;
	context->fpsr = current->thread.user_fpsr;
	test->priv = context;
	return 0;
}

static void pairwise_test_exit(struct kunit *test)
{
	struct pairwise_context *context = test->priv;

	memcpy(current->thread.user_simd, context->simd, sizeof(context->simd));
	current->thread.user_simd_valid = context->simd_valid;
	current->thread.user_fpcr = context->fpcr;
	current->thread.user_fpsr = context->fpsr;
}

static const char *pairwise_artifact_string(
	const struct tcti_target_instruction_artifact *artifact, u32 offset)
{
	if (offset >= artifact->string_pool_size)
		return NULL;
	return (const char *)artifact->string_pool + offset;
}

static u32 pairwise_instruction(const struct pairwise_leaf *leaf, u8 q,
				u8 size, u8 rd, u8 rn, u8 rm)
{
	return leaf->pattern | ((u32)q << 30) | ((u32)size << 22) |
		((u32)rm << 16) | ((u32)rn << 5) | rd;
}

static unsigned long pairwise_map_instruction(struct kunit *test,
					       u32 instruction)
{
	const u32 program[] = { instruction, PAIRWISE_SVC };
	unsigned long mapped;
	int ret;

	mapped = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));
	ret = tcti_write_user_data(current->mm, mapped, program, sizeof(program));
	KUNIT_ASSERT_EQ(test, 0, ret);
	ret = sys_mprotect(mapped, PAGE_SIZE, PROT_READ | PROT_EXEC);
	KUNIT_ASSERT_EQ(test, 0, ret);
	return mapped;
}

static void pairwise_seed_regs(struct pt_regs *regs, unsigned long pc,
			       u32 instruction)
{
	u8 reg;

	memset(regs, 0x5a, sizeof(*regs));
	for (reg = 0; reg < ARRAY_SIZE(regs->regs); reg++)
		regs->regs[reg] = 0x9e3779b97f4a7c15ULL ^
			((u64)instruction << (reg & 15U)) ^ reg;
	regs->sp = 0x00000001fffffff0ULL;
	regs->pc = pc;
	regs->pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT |
		PSR_V_BIT | PSR_D_BIT;
	regs->orig_x0 = 0x13579bdf2468ace0ULL;
	regs->syscallno = NO_SYSCALL;
	regs->unused = 0x76543210U;
}

static void pairwise_source_binding_and_arrangements(struct kunit *test)
{
	const struct tcti_target_instruction_artifact *artifact =
		tcti_target_instruction_artifact_canonical();
	struct tcti_target_instruction_artifact_validation_result validation;
	size_t leaf_index;

	KUNIT_ASSERT_NOT_NULL(test, artifact);
	KUNIT_ASSERT_EQ(test, 0,
		tcti_target_instruction_artifact_validate(artifact, &validation));
	KUNIT_ASSERT_GT(test, artifact->leaf_count, 3957U);
	KUNIT_ASSERT_EQ(test, 4U, ARRAY_SIZE(pairwise_leaves));
	for (leaf_index = 0; leaf_index < ARRAY_SIZE(pairwise_leaves);
	     leaf_index++) {
		const struct pairwise_leaf *leaf = &pairwise_leaves[leaf_index];
		const struct tcti_target_instruction_artifact_leaf *source =
			&artifact->leaves[leaf->ordinal];
		const char *name = pairwise_artifact_string(artifact,
			source->name_offset);
		const char *mnemonic = pairwise_artifact_string(artifact,
			source->mnemonic_offset);
		const char *operation = pairwise_artifact_string(artifact,
			source->operation_offset);
		u8 q;
		u8 reg;
		u8 size;

		KUNIT_ASSERT_NOT_NULL(test, name);
		KUNIT_ASSERT_NOT_NULL(test, mnemonic);
		KUNIT_ASSERT_NOT_NULL(test, operation);
		KUNIT_EXPECT_STREQ(test, leaf->name, name);
		KUNIT_EXPECT_STREQ(test, leaf->mnemonic, mnemonic);
		KUNIT_EXPECT_STREQ(test, leaf->operation, operation);
		KUNIT_EXPECT_EQ(test, leaf->mask, source->encoding_mask);
		KUNIT_EXPECT_EQ(test, leaf->pattern, source->encoding_pattern);
		for (q = 0; q < 2; q++) {
			for (size = 0; size < 4; size++) {
				u32 instruction = pairwise_instruction(leaf, q, size,
					PAIRWISE_RD, PAIRWISE_RN, PAIRWISE_RM);
				struct tcti_decoded_instruction decoded =
					tcti_decode_aarch64(instruction);

				KUNIT_EXPECT_EQ(test, leaf->pattern,
					instruction & leaf->mask);
				if (size == 3) {
					KUNIT_EXPECT_EQ(test, TCTI_DECODE_UNSUPPORTED,
						decoded.decode_class);
					continue;
				}
				KUNIT_EXPECT_EQ(test,
					TCTI_DECODE_SIMD_VECTOR_ARITHMETIC,
					decoded.decode_class);
				KUNIT_EXPECT_EQ(test, leaf->arithmetic,
					decoded.simd_arithmetic_op);
				KUNIT_EXPECT_EQ(test, PAIRWISE_RD, decoded.rd);
				KUNIT_EXPECT_EQ(test, PAIRWISE_RN, decoded.rn);
				KUNIT_EXPECT_EQ(test, PAIRWISE_RM, decoded.rm);
				KUNIT_EXPECT_EQ(test, BIT(size), decoded.access_size);
				KUNIT_EXPECT_EQ(test, q ? 16U : 8U,
					decoded.result_size);
				KUNIT_EXPECT_TRUE(test, decoded.simd_fp);
			}
		}
		for (reg = 0; reg < 32; reg++) {
			struct tcti_decoded_instruction decoded;

			decoded = tcti_decode_aarch64(pairwise_instruction(leaf, 1, 2,
				reg, 1, 2));
			KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_VECTOR_ARITHMETIC,
				decoded.decode_class);
			KUNIT_EXPECT_EQ(test, reg, decoded.rd);
			decoded = tcti_decode_aarch64(pairwise_instruction(leaf, 1, 2,
				0, reg, 2));
			KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_VECTOR_ARITHMETIC,
				decoded.decode_class);
			KUNIT_EXPECT_EQ(test, reg, decoded.rn);
			decoded = tcti_decode_aarch64(pairwise_instruction(leaf, 1, 2,
				0, 1, reg));
			KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_VECTOR_ARITHMETIC,
				decoded.decode_class);
			KUNIT_EXPECT_EQ(test, reg, decoded.rm);
		}
	}
}

static void pairwise_reserved_size_preserves_full_state(struct kunit *test)
{
	size_t leaf_index;

	for (leaf_index = 0; leaf_index < ARRAY_SIZE(pairwise_leaves);
	     leaf_index++) {
		const struct pairwise_leaf *leaf = &pairwise_leaves[leaf_index];
		u8 q;

		for (q = 0; q < 2; q++) {
			u32 instruction = pairwise_instruction(leaf, q, 3,
				PAIRWISE_RD, PAIRWISE_RN, PAIRWISE_RM);
			const u32 expected_code[] = { instruction, PAIRWISE_SVC };
			u32 observed_code[ARRAY_SIZE(expected_code)] = {};
			unsigned long mapped = pairwise_map_instruction(test,
				instruction);
			u64 before_simd[ARRAY_SIZE(current->thread.user_simd)];
			struct pt_regs regs;
			struct pt_regs before_regs;
			struct tcti_result result;
			u8 index;

			for (index = 0; index < ARRAY_SIZE(before_simd); index++)
				current->thread.user_simd[index] =
					0xd6e8feb86659fd93ULL ^ index;
			memcpy(before_simd, current->thread.user_simd,
				sizeof(before_simd));
			current->thread.user_simd_valid = 1;
			current->thread.user_fpcr = BIT(22) | BIT(24);
			current->thread.user_fpsr = BIT(27) | BIT(4);
			pairwise_seed_regs(&regs, mapped, instruction);
			before_regs = regs;

			result = tcti_resume_user(current, &regs, current->mm);

			KUNIT_EXPECT_EQ(test, TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
				result.reason);
			KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, result.status);
			KUNIT_EXPECT_EQ(test, 0UL, result.fault_address);
			KUNIT_EXPECT_EQ(test, TCTI_ACCESS_FETCH, result.fault_access);
			KUNIT_EXPECT_EQ(test, mapped, result.pc);
			KUNIT_EXPECT_EQ(test, instruction, result.instruction);
			KUNIT_EXPECT_MEMEQ(test, &before_regs, &regs, sizeof(regs));
			KUNIT_EXPECT_MEMEQ(test, before_simd,
				current->thread.user_simd, sizeof(before_simd));
			KUNIT_EXPECT_EQ(test, 1UL,
				current->thread.user_simd_valid);
			KUNIT_EXPECT_EQ(test, BIT(22) | BIT(24),
				current->thread.user_fpcr);
			KUNIT_EXPECT_EQ(test, BIT(27) | BIT(4),
				current->thread.user_fpsr);
			KUNIT_ASSERT_EQ(test, 0, tcti_read_user_data(current->mm,
				mapped, observed_code, sizeof(observed_code)));
			KUNIT_EXPECT_MEMEQ(test, expected_code, observed_code,
				sizeof(expected_code));
			KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
		}
	}
}

static void pairwise_source_leaves_execute_with_overlaps(struct kunit *test)
{
	static const struct {
		u8 rd;
		u8 rn;
		u8 rm;
		bool same_source;
	} arrangements[] = {
		{ PAIRWISE_RD, PAIRWISE_RN, PAIRWISE_RM, false },
		{ PAIRWISE_RN, PAIRWISE_RN, PAIRWISE_RM, false },
		{ PAIRWISE_RM, PAIRWISE_RN, PAIRWISE_RM, false },
		{ PAIRWISE_RD, PAIRWISE_RN, PAIRWISE_RN, true },
		{ PAIRWISE_RN, PAIRWISE_RN, PAIRWISE_RN, true },
		{ 31U, 31U, PAIRWISE_RM, false },
	};
	size_t leaf_index;

	for (leaf_index = 0; leaf_index < ARRAY_SIZE(pairwise_leaves);
	     leaf_index++) {
		const struct pairwise_leaf *leaf = &pairwise_leaves[leaf_index];
		size_t vector_index;

		for (vector_index = 0; vector_index < ARRAY_SIZE(pairwise_vectors);
		     vector_index++) {
			const struct pairwise_vector *vector =
				&pairwise_vectors[vector_index];
			u8 arrangement_index;

			for (arrangement_index = 0;
			     arrangement_index < ARRAY_SIZE(arrangements);
			     arrangement_index++) {
				u8 rd = arrangements[arrangement_index].rd;
				u8 rn = arrangements[arrangement_index].rn;
				u8 rm = arrangements[arrangement_index].rm;
				bool same_source =
					arrangements[arrangement_index].same_source;
				u32 instruction = pairwise_instruction(leaf, vector->q,
					vector->size, rd, rn, rm);
				const u64 *expected_result = same_source ?
					vector->same_left_expected[leaf->expected_index] :
					vector->expected[leaf->expected_index];
				const u32 expected_code[] = { instruction, PAIRWISE_SVC };
				u32 observed_code[ARRAY_SIZE(expected_code)] = {};
				unsigned long mapped = pairwise_map_instruction(test,
					instruction);
				u64 expected_simd[ARRAY_SIZE(
					current->thread.user_simd)];
				struct pt_regs regs;
				struct pt_regs expected_regs;
				struct tcti_result result;
				u8 index;
				int ret;

				for (index = 0; index < ARRAY_SIZE(expected_simd); index++)
					current->thread.user_simd[index] =
						0x9e3779b97f4a7c15ULL ^
						((u64)instruction << (index & 7U)) ^ index;
				current->thread.user_simd[rn * 2] = vector->left[0];
				current->thread.user_simd[rn * 2 + 1] = vector->left[1];
				if (!same_source) {
					current->thread.user_simd[rm * 2] = vector->right[0];
					current->thread.user_simd[rm * 2 + 1] =
						vector->right[1];
				}
				memcpy(expected_simd, current->thread.user_simd,
					sizeof(expected_simd));
				expected_simd[rd * 2] = expected_result[0];
				expected_simd[rd * 2 + 1] = expected_result[1];
				current->thread.user_simd_valid = 0;
				current->thread.user_fpcr = BIT(22) | BIT(24);
				current->thread.user_fpsr = BIT(27) | BIT(4);
				pairwise_seed_regs(&regs, mapped, instruction);
				expected_regs = regs;
				expected_regs.pc += sizeof(u32);

				result = tcti_resume_user(current, &regs, current->mm);

				KUNIT_EXPECT_EQ(test, TCTI_EXIT_SYSCALL, result.reason);
				KUNIT_EXPECT_EQ(test, 0L, result.status);
				KUNIT_EXPECT_EQ(test, 0UL, result.fault_address);
				KUNIT_EXPECT_EQ(test, TCTI_ACCESS_FETCH,
					result.fault_access);
				KUNIT_EXPECT_EQ(test, mapped + sizeof(u32), result.pc);
				KUNIT_EXPECT_EQ(test, PAIRWISE_SVC, result.instruction);
				KUNIT_EXPECT_MEMEQ(test, &expected_regs, &regs,
					sizeof(regs));
				KUNIT_EXPECT_MEMEQ(test, expected_simd,
					current->thread.user_simd, sizeof(expected_simd));
				KUNIT_EXPECT_EQ(test, 1UL,
					current->thread.user_simd_valid);
				KUNIT_EXPECT_EQ(test, BIT(22) | BIT(24),
					current->thread.user_fpcr);
				KUNIT_EXPECT_EQ(test, BIT(27) | BIT(4),
					current->thread.user_fpsr);
				ret = tcti_read_user_data(current->mm, mapped,
					observed_code, sizeof(observed_code));
				KUNIT_ASSERT_EQ(test, 0, ret);
				KUNIT_EXPECT_MEMEQ(test, expected_code, observed_code,
					sizeof(expected_code));
				KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
			}
		}
	}
}

static struct kunit_case pairwise_cases[] = {
	KUNIT_CASE(pairwise_source_binding_and_arrangements),
	KUNIT_CASE(pairwise_reserved_size_preserves_full_state),
	KUNIT_CASE(pairwise_source_leaves_execute_with_overlaps),
	{}
};

static struct kunit_suite pairwise_suite = {
	.name = "orlix-tcti-advsimd-pairwise-minmax-source-bound",
	.init = pairwise_test_init,
	.exit = pairwise_test_exit,
	.test_cases = pairwise_cases,
};

kunit_test_suite(pairwise_suite);

MODULE_LICENSE("GPL");
