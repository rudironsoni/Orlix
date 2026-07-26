// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
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
#include "target_instruction_artifact.h"

#define TCTI_ADVSIMD_COMPARE_SVC	0xd4000001U
#define TCTI_ADVSIMD_COMPARE_VARIABLE_MASK	0x40df03ffU

struct tcti_advsimd_compare_leaf {
	u16 source_ordinal;
	const char *source_name;
	const char *source_mnemonic;
	const char *source_operation;
	u32 source_mask;
	u32 source_pattern;
	enum tcti_simd_vector_compare_op operation;
};

static const struct tcti_advsimd_compare_leaf tcti_advsimd_compare_leaves[] = {
	{ 3900U, "CMGT_asimdsame_only", "CMGT", "CMGT_advsimd_reg",
	  0xbf20fc00U, 0x0e203400U, TCTI_SIMD_COMPARE_CMGT },
	{ 3901U, "CMGE_asimdsame_only", "CMGE", "CMGE_advsimd_reg",
	  0xbf20fc00U, 0x0e203c00U, TCTI_SIMD_COMPARE_CMGE },
	{ 3911U, "CMTST_asimdsame_only", "CMTST", "CMTST_advsimd",
	  0xbf20fc00U, 0x0e208c00U, TCTI_SIMD_COMPARE_CMTST },
	{ 3942U, "CMHI_asimdsame_only", "CMHI", "CMHI_advsimd",
	  0xbf20fc00U, 0x2e203400U, TCTI_SIMD_COMPARE_CMHI },
	{ 3943U, "CMHS_asimdsame_only", "CMHS", "CMHS_advsimd",
	  0xbf20fc00U, 0x2e203c00U, TCTI_SIMD_COMPARE_CMHS },
	{ 3953U, "CMEQ_asimdsame_only", "CMEQ", "CMEQ_advsimd_reg",
	  0xbf20fc00U, 0x2e208c00U, TCTI_SIMD_COMPARE_CMEQ },
};

struct tcti_advsimd_compare_context {
	struct mm_struct *mm;
	unsigned long instructions;
	u64 simd[ARRAY_SIZE(current->thread.user_simd)];
	unsigned long simd_valid;
	unsigned long fpcr;
	unsigned long fpsr;
};

struct tcti_advsimd_compare_registers {
	u8 rd;
	u8 rn;
	u8 rm;
};

struct tcti_advsimd_compare_operands {
	u64 left[2];
	u64 right[2];
};

static const struct tcti_advsimd_compare_registers
tcti_advsimd_compare_overlap_cases[] = {
	{ 3U, 4U, 5U },
	{ 6U, 6U, 7U },
	{ 8U, 9U, 8U },
	{ 10U, 11U, 11U },
	{ 12U, 12U, 12U },
	{ 31U, 0U, 30U },
};

static const struct tcti_advsimd_compare_operands
tcti_advsimd_compare_operand_cases[] = {
	{
		.left = { 0x7f80000080017fffULL, 0x800000007fffffffULL },
		.right = { 0x807fffff7fff8001ULL, 0x7fffffff80000000ULL },
	},
	{
		.left = { 0x1122334455667788ULL, 0x99aabbccddeeff00ULL },
		.right = { 0x1122334455667788ULL, 0x99aabbccddeeff01ULL },
	},
};

static const char *tcti_advsimd_compare_artifact_string(
	const struct tcti_target_instruction_artifact *artifact, u32 offset)
{
	if (offset >= artifact->string_pool_size)
		return NULL;

	return (const char *)artifact->string_pool + offset;
}

static int tcti_advsimd_compare_test_init(struct kunit *test)
{
	struct tcti_advsimd_compare_context *context;

	context = kunit_kzalloc(test, sizeof(*context), GFP_KERNEL);
	if (!context)
		return -ENOMEM;
	context->mm = mm_alloc();
	if (!context->mm)
		return -ENOMEM;
	kthread_use_mm(context->mm);
	context->instructions = ksys_mmap_pgoff(
		0, PAGE_SIZE, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (IS_ERR_VALUE(context->instructions)) {
		kthread_unuse_mm(context->mm);
		mmput(context->mm);
		return (int)context->instructions;
	}
	memcpy(context->simd, current->thread.user_simd,
	       sizeof(context->simd));
	context->simd_valid = current->thread.user_simd_valid;
	context->fpcr = current->thread.user_fpcr;
	context->fpsr = current->thread.user_fpsr;
	test->priv = context;
	return 0;
}

static void tcti_advsimd_compare_test_exit(struct kunit *test)
{
	struct tcti_advsimd_compare_context *context = test->priv;

	memcpy(current->thread.user_simd, context->simd,
	       sizeof(context->simd));
	current->thread.user_simd_valid = context->simd_valid;
	current->thread.user_fpcr = context->fpcr;
	current->thread.user_fpsr = context->fpsr;
	vm_munmap(context->instructions, PAGE_SIZE);
	kthread_unuse_mm(context->mm);
	mmput(context->mm);
}

static u32 tcti_advsimd_compare_instruction(
	const struct tcti_advsimd_compare_leaf *leaf, u8 q, u8 size, u8 rd,
	u8 rn, u8 rm)
{
	return leaf->source_pattern | ((u32)q << 30) | ((u32)size << 22) |
		((u32)rm << 16) | ((u32)rn << 5) | rd;
}

static int tcti_advsimd_compare_load_instruction(struct kunit *test,
						 u32 instruction)
{
	struct tcti_advsimd_compare_context *context = test->priv;
	const u32 program[] = { instruction, TCTI_ADVSIMD_COMPARE_SVC };
	int ret;

	ret = sys_mprotect(context->instructions, PAGE_SIZE,
			   PROT_READ | PROT_WRITE);
	if (ret)
		return ret;
	ret = tcti_write_user_data(current->mm, context->instructions, program,
				   sizeof(program));
	if (ret)
		return ret;
	return sys_mprotect(context->instructions, PAGE_SIZE,
			    PROT_READ | PROT_EXEC);
}

static void tcti_advsimd_compare_seed_regs(struct pt_regs *regs,
					   unsigned long pc, u32 instruction)
{
	unsigned int index;

	memset(regs, 0, sizeof(*regs));
	for (index = 0; index < ARRAY_SIZE(regs->regs); index++)
		regs->regs[index] = 0x9e3779b97f4a7c15ULL ^
			((u64)instruction << (index & 15U)) ^ index;
	regs->sp = 0x00000001fffffff0ULL;
	regs->pc = pc;
	regs->pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT |
		       PSR_V_BIT | PSR_D_BIT;
	regs->orig_x0 = 0x13579bdf2468ace0ULL;
	regs->syscallno = NO_SYSCALL;
	regs->unused = 0x76543210U;
}

static u64 tcti_advsimd_compare_lane_mask(u8 lane_bytes)
{
	return lane_bytes == sizeof(u64) ? U64_MAX :
		GENMASK_ULL(lane_bytes * 8 - 1, 0);
}

static bool tcti_advsimd_compare_lane_matches(
	enum tcti_simd_vector_compare_op operation, u64 left, u64 right,
	u8 lane_bits)
{
	switch (operation) {
	case TCTI_SIMD_COMPARE_CMEQ:
		return left == right;
	case TCTI_SIMD_COMPARE_CMHI:
		return left > right;
	case TCTI_SIMD_COMPARE_CMTST:
		return (left & right) != 0;
	case TCTI_SIMD_COMPARE_CMGT:
		return sign_extend64(left, lane_bits - 1) >
			sign_extend64(right, lane_bits - 1);
	case TCTI_SIMD_COMPARE_CMGE:
		return sign_extend64(left, lane_bits - 1) >=
			sign_extend64(right, lane_bits - 1);
	case TCTI_SIMD_COMPARE_CMHS:
		return left >= right;
	case TCTI_SIMD_COMPARE_CMLE:
	case TCTI_SIMD_COMPARE_CMLT:
		break;
	}

	return false;
}

static void tcti_advsimd_compare_expected(
	u64 result[2], enum tcti_simd_vector_compare_op operation,
	const u64 left[2], const u64 right[2], u8 lane_bytes, u8 result_bytes)
{
	u64 mask = tcti_advsimd_compare_lane_mask(lane_bytes);
	u8 lane_count = result_bytes / lane_bytes;
	u8 lane;

	result[0] = 0;
	result[1] = 0;
	for (lane = 0; lane < lane_count; lane++) {
		u8 byte = lane * lane_bytes;
		u8 word = byte / sizeof(u64);
		u8 shift = (byte % sizeof(u64)) * 8;
		u64 left_lane = (left[word] >> shift) & mask;
		u64 right_lane = (right[word] >> shift) & mask;

		if (tcti_advsimd_compare_lane_matches(operation, left_lane,
						       right_lane,
						       lane_bytes * 8))
			result[word] |= mask << shift;
	}
}

static void tcti_advsimd_compare_source_bindings(struct kunit *test)
{
	const struct tcti_target_instruction_artifact *artifact =
		tcti_target_instruction_artifact_canonical();
	struct tcti_target_instruction_artifact_validation_result validation;
	size_t index;

	KUNIT_ASSERT_NOT_NULL(test, artifact);
	KUNIT_ASSERT_EQ(test, 0,
			tcti_target_instruction_artifact_validate(artifact,
							  &validation));
	KUNIT_ASSERT_GT(test, artifact->leaf_count, 3953U);
	KUNIT_ASSERT_EQ(test, 6U, ARRAY_SIZE(tcti_advsimd_compare_leaves));
	for (index = 0; index < ARRAY_SIZE(tcti_advsimd_compare_leaves);
	     index++) {
		const struct tcti_advsimd_compare_leaf *leaf =
			&tcti_advsimd_compare_leaves[index];
		const struct tcti_target_instruction_artifact_leaf *source =
			&artifact->leaves[leaf->source_ordinal];
		const char *name = tcti_advsimd_compare_artifact_string(
			artifact, source->name_offset);
		const char *mnemonic = tcti_advsimd_compare_artifact_string(
			artifact, source->mnemonic_offset);
		const char *operation = tcti_advsimd_compare_artifact_string(
			artifact, source->operation_offset);

		KUNIT_ASSERT_NOT_NULL(test, name);
		KUNIT_ASSERT_NOT_NULL(test, mnemonic);
		KUNIT_ASSERT_NOT_NULL(test, operation);
		KUNIT_EXPECT_STREQ(test, leaf->source_name, name);
		KUNIT_EXPECT_STREQ(test, leaf->source_mnemonic, mnemonic);
		KUNIT_EXPECT_STREQ(test, leaf->source_operation, operation);
		KUNIT_EXPECT_EQ(test, leaf->source_mask, source->encoding_mask);
		KUNIT_EXPECT_EQ(test, leaf->source_pattern,
				source->encoding_pattern);
		KUNIT_EXPECT_EQ(test, TCTI_ADVSIMD_COMPARE_VARIABLE_MASK,
				~source->encoding_mask);
	}
}

static void tcti_advsimd_compare_operand_corpus_distinguishes_predicates(
	struct kunit *test)
{
	size_t leaf_index;
	u8 q;
	u8 size;

	for (leaf_index = 0;
	     leaf_index < ARRAY_SIZE(tcti_advsimd_compare_leaves);
	     leaf_index++) {
		const struct tcti_advsimd_compare_leaf *leaf =
			&tcti_advsimd_compare_leaves[leaf_index];

		for (q = 0; q < 2; q++) {
			for (size = 0; size < 4; size++) {
				bool saw_match = false;
				bool saw_mismatch = false;
				size_t operand_index;

				if (!q && size == 3)
					continue;
				for (operand_index = 0;
				     operand_index < ARRAY_SIZE(
					tcti_advsimd_compare_operand_cases);
				     operand_index++) {
					const struct tcti_advsimd_compare_operands *operands =
						&tcti_advsimd_compare_operand_cases[
							operand_index];
					u8 lane_bytes = BIT(size);
					u8 result_bytes = q ? 2 * sizeof(u64) :
							      sizeof(u64);
					u64 mask = tcti_advsimd_compare_lane_mask(
						lane_bytes);
					u8 lane;

					for (lane = 0; lane < result_bytes / lane_bytes;
					     lane++) {
						u8 byte = lane * lane_bytes;
						u8 word = byte / sizeof(u64);
						u8 shift =
							(byte % sizeof(u64)) * 8;
						u64 left =
							(operands->left[word] >> shift) &
							mask;
						u64 right =
							(operands->right[word] >> shift) &
							mask;
						bool matches =
							tcti_advsimd_compare_lane_matches(
								leaf->operation, left, right,
								lane_bytes * 8);

						saw_match |= matches;
						saw_mismatch |= !matches;
					}
				}
				KUNIT_EXPECT_TRUE_MSG(
					test, saw_match, "%s q=%u size=%u",
					leaf->source_name, q, size);
				KUNIT_EXPECT_TRUE_MSG(
					test, saw_mismatch, "%s q=%u size=%u",
					leaf->source_name, q, size);
			}
		}
	}
}

static void tcti_advsimd_compare_legal_fields_decode(struct kunit *test)
{
	size_t leaf_index;
	u8 q;
	u8 size;
	u8 reg;

	for (leaf_index = 0;
	     leaf_index < ARRAY_SIZE(tcti_advsimd_compare_leaves);
	     leaf_index++) {
		const struct tcti_advsimd_compare_leaf *leaf =
			&tcti_advsimd_compare_leaves[leaf_index];

		for (q = 0; q < 2; q++) {
			for (size = 0; size < 4; size++) {
				for (reg = 0; reg < 32; reg++) {
					struct tcti_decoded_instruction decoded =
						tcti_decode_aarch64(
							tcti_advsimd_compare_instruction(
								leaf, q, size, reg,
								(reg + 1U) & 31U,
								(reg + 2U) & 31U));

					if (!q && size == 3) {
						KUNIT_EXPECT_EQ(
							test, TCTI_DECODE_UNSUPPORTED,
							decoded.decode_class);
						continue;
					}
					KUNIT_EXPECT_EQ(
						test,
						TCTI_DECODE_SIMD_VECTOR_COMPARE,
						decoded.decode_class);
					KUNIT_EXPECT_EQ(test, leaf->operation,
							decoded.simd_compare_op);
					KUNIT_EXPECT_EQ(test, reg, decoded.rd);
					KUNIT_EXPECT_EQ(test,
							(reg + 1U) & 31U,
							decoded.rn);
					KUNIT_EXPECT_EQ(test,
							(reg + 2U) & 31U,
							decoded.rm);
					KUNIT_EXPECT_EQ(test, BIT(size),
							decoded.access_size);
					KUNIT_EXPECT_EQ(
						test,
						q ? 2 * sizeof(u64) : sizeof(u64),
						decoded.result_size);
				}
			}
		}
	}
}

static void tcti_advsimd_compare_reserved_1d_rejected(struct kunit *test)
{
	struct tcti_advsimd_compare_context *context = test->priv;
	size_t leaf_index;

	for (leaf_index = 0;
	     leaf_index < ARRAY_SIZE(tcti_advsimd_compare_leaves);
	     leaf_index++) {
		const struct tcti_advsimd_compare_leaf *leaf =
			&tcti_advsimd_compare_leaves[leaf_index];
		u32 instruction = tcti_advsimd_compare_instruction(
			leaf, 0, 3, 3, 4, 5);
		const u32 expected_program[] = {
			instruction, TCTI_ADVSIMD_COMPARE_SVC,
		};
		u32 after_program[ARRAY_SIZE(expected_program)];
		u64 expected_simd[ARRAY_SIZE(current->thread.user_simd)];
		struct pt_regs regs;
		struct pt_regs expected_regs;
		struct tcti_result result;
		unsigned int simd_index;
		int ret;

		ret = tcti_advsimd_compare_load_instruction(test, instruction);
		KUNIT_ASSERT_EQ(test, 0, ret);
		for (simd_index = 0;
		     simd_index < ARRAY_SIZE(current->thread.user_simd);
		     simd_index++)
			current->thread.user_simd[simd_index] =
				0xbb67ae8584caa73bULL ^
				((u64)simd_index * 0x0102040810204081ULL);
		memcpy(expected_simd, current->thread.user_simd,
		       sizeof(expected_simd));
		current->thread.user_simd_valid = 0;
		current->thread.user_fpcr = BIT(22) | BIT(24);
		current->thread.user_fpsr = BIT(27) | BIT(4);
		tcti_advsimd_compare_seed_regs(&regs, context->instructions,
						instruction);
		expected_regs = regs;

		result = tcti_resume_user(current, &regs, current->mm);

		KUNIT_EXPECT_EQ_MSG(test, TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
				    result.reason, "%s", leaf->source_name);
		KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, result.status);
		KUNIT_EXPECT_EQ(test, 0UL, result.fault_address);
		KUNIT_EXPECT_EQ(test, TCTI_ACCESS_FETCH, result.fault_access);
		KUNIT_EXPECT_EQ(test, context->instructions, result.pc);
		KUNIT_EXPECT_EQ(test, instruction, result.instruction);
		KUNIT_EXPECT_MEMEQ(test, &expected_regs, &regs,
				   sizeof(expected_regs));
		KUNIT_EXPECT_MEMEQ(test, expected_simd,
				   current->thread.user_simd,
				   sizeof(expected_simd));
		KUNIT_EXPECT_EQ(test, 0UL, current->thread.user_simd_valid);
		KUNIT_EXPECT_EQ(test, BIT(22) | BIT(24),
				current->thread.user_fpcr);
		KUNIT_EXPECT_EQ(test, BIT(27) | BIT(4),
				current->thread.user_fpsr);
		ret = tcti_read_user_data(current->mm, context->instructions,
					  after_program, sizeof(after_program));
		KUNIT_ASSERT_EQ(test, 0, ret);
		KUNIT_EXPECT_MEMEQ(test, expected_program, after_program,
				   sizeof(expected_program));
	}
}

static void tcti_advsimd_compare_source_leaves_resume(struct kunit *test)
{
	struct tcti_advsimd_compare_context *context = test->priv;
	size_t leaf_index;
	u8 q;
	u8 size;
	size_t case_index;

	for (leaf_index = 0;
	     leaf_index < ARRAY_SIZE(tcti_advsimd_compare_leaves);
	     leaf_index++) {
		const struct tcti_advsimd_compare_leaf *leaf =
			&tcti_advsimd_compare_leaves[leaf_index];

		for (q = 0; q < 2; q++) {
			for (size = 0; size < 4; size++) {
				if (!q && size == 3)
					continue;
				for (case_index = 0;
				     case_index <
				     ARRAY_SIZE(
					     tcti_advsimd_compare_overlap_cases) *
					     ARRAY_SIZE(
						     tcti_advsimd_compare_operand_cases);
				     case_index++) {
					size_t overlap_index =
						case_index /
						ARRAY_SIZE(
							tcti_advsimd_compare_operand_cases);
					size_t operand_index =
						case_index %
						ARRAY_SIZE(
							tcti_advsimd_compare_operand_cases);
					const struct tcti_advsimd_compare_registers
						*registers =
							&tcti_advsimd_compare_overlap_cases
								[overlap_index];
					const struct tcti_advsimd_compare_operands
						*operands =
							&tcti_advsimd_compare_operand_cases
								[operand_index];
					u32 instruction =
						tcti_advsimd_compare_instruction(
							leaf, q, size,
							registers->rd,
							registers->rn,
							registers->rm);
					const u32 expected_program[] = {
						instruction,
						TCTI_ADVSIMD_COMPARE_SVC,
					};
					u32 before_program[ARRAY_SIZE(
						expected_program)];
					u32 after_program[ARRAY_SIZE(
						expected_program)];
					u64 expected_simd[ARRAY_SIZE(
						current->thread.user_simd)];
					u64 expected_result[2];
					struct pt_regs regs;
					struct pt_regs expected_regs;
					struct tcti_result result;
					unsigned int simd_index;
					int ret;

					ret = tcti_advsimd_compare_load_instruction(
						test, instruction);
					KUNIT_ASSERT_EQ(test, 0, ret);
					ret = tcti_read_user_data(
						current->mm,
						context->instructions,
						before_program,
						sizeof(before_program));
					KUNIT_ASSERT_EQ(test, 0, ret);
					KUNIT_EXPECT_MEMEQ(
						test, expected_program,
						before_program,
						sizeof(expected_program));

					for (simd_index = 0;
					     simd_index <
					     ARRAY_SIZE(
						     current->thread.user_simd);
					     simd_index++)
						current->thread
							.user_simd[simd_index] =
							0x3c6ef372fe94f82bULL ^
							((u64)(simd_index +
							       1U) *
							 0x0102040810204081ULL);
					current->thread
						.user_simd[registers->rn * 2U] =
						operands->left[0];
					current->thread
						.user_simd[registers->rn * 2U +
							   1U] =
						operands->left[1];
					if (registers->rm != registers->rn) {
						current->thread.user_simd
							[registers->rm * 2U] =
							operands->right[0];
						current->thread.user_simd
							[registers->rm * 2U +
							 1U] =
							operands->right[1];
					}
					memcpy(expected_simd,
					       current->thread.user_simd,
					       sizeof(expected_simd));
					tcti_advsimd_compare_expected(
						expected_result,
						leaf->operation,
						&expected_simd[registers->rn *
							       2U],
						&expected_simd[registers->rm *
							       2U],
						BIT(size),
						q ? 2 * sizeof(u64) :
						    sizeof(u64));
					expected_simd[registers->rd * 2U] =
						expected_result[0];
					expected_simd[registers->rd * 2U + 1U] =
						expected_result[1];

					current->thread.user_simd_valid = 0;
					current->thread.user_fpcr = BIT(22) |
								    BIT(24);
					current->thread.user_fpsr = BIT(27) |
								    BIT(4);
					tcti_advsimd_compare_seed_regs(
						&regs, context->instructions,
						instruction);
					expected_regs = regs;
					expected_regs.pc += sizeof(u32);

					result = tcti_resume_user(
						current, &regs, current->mm);

					KUNIT_EXPECT_EQ_MSG(
						test, TCTI_EXIT_SYSCALL,
						result.reason,
						"%s q=%u size=%u overlap=%zu operands=%zu",
						leaf->source_name, q, size,
						overlap_index, operand_index);
					KUNIT_EXPECT_EQ(test, 0L,
							result.status);
					KUNIT_EXPECT_EQ(test, 0UL,
							result.fault_address);
					KUNIT_EXPECT_EQ(test, TCTI_ACCESS_FETCH,
							result.fault_access);
					KUNIT_EXPECT_EQ(test,
							context->instructions +
								sizeof(u32),
							result.pc);
					KUNIT_EXPECT_EQ(
						test, TCTI_ADVSIMD_COMPARE_SVC,
						result.instruction);
					KUNIT_EXPECT_MEMEQ(
						test, expected_simd,
						current->thread.user_simd,
						sizeof(expected_simd));
					KUNIT_EXPECT_EQ(
						test, 1UL,
						current->thread.user_simd_valid);
					KUNIT_EXPECT_EQ(
						test, BIT(22) | BIT(24),
						current->thread.user_fpcr);
					KUNIT_EXPECT_EQ(
						test, BIT(27) | BIT(4),
						current->thread.user_fpsr);
					KUNIT_EXPECT_MEMEQ(
						test, &expected_regs, &regs,
						sizeof(expected_regs));

					ret = tcti_read_user_data(
						current->mm,
						context->instructions,
						after_program,
						sizeof(after_program));
					KUNIT_ASSERT_EQ(test, 0, ret);
					KUNIT_EXPECT_MEMEQ(
						test, expected_program,
						after_program,
						sizeof(expected_program));
				}
			}
		}
	}
}

static struct kunit_case tcti_advsimd_compare_source_bound_cases[] = {
	KUNIT_CASE(tcti_advsimd_compare_source_bindings),
	KUNIT_CASE(tcti_advsimd_compare_operand_corpus_distinguishes_predicates),
	KUNIT_CASE(tcti_advsimd_compare_legal_fields_decode),
	KUNIT_CASE(tcti_advsimd_compare_reserved_1d_rejected),
	KUNIT_CASE(tcti_advsimd_compare_source_leaves_resume),
	{}
};

static struct kunit_suite tcti_advsimd_compare_source_bound_suite = {
	.name = "orlix-tcti-advsimd-compare-register-source-bound",
	.init = tcti_advsimd_compare_test_init,
	.exit = tcti_advsimd_compare_test_exit,
	.test_cases = tcti_advsimd_compare_source_bound_cases,
};

kunit_test_suite(tcti_advsimd_compare_source_bound_suite);

MODULE_LICENSE("GPL");
