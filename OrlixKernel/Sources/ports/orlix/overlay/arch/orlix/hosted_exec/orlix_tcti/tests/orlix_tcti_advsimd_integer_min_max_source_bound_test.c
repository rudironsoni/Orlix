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
#include <asm/orlix_tcti.h>

#include "../decode_aarch64.h"
#include "target_instruction_artifact.h"

#define ORLIX_TCTI_ADVSIMD_MIN_MAX_SVC 0xd4000001U
#define ORLIX_TCTI_ADVSIMD_MIN_MAX_VARIABLE_MASK 0x40df03ffU

struct orlix_tcti_advsimd_min_max_leaf {
	u16 source_ordinal;
	const char *source_name;
	const char *source_mnemonic;
	const char *source_operation;
	u32 source_mask;
	u32 source_pattern;
	enum orlix_tcti_simd_vector_arithmetic_op operation;
	bool is_unsigned;
	bool minimum;
};

static const struct orlix_tcti_advsimd_min_max_leaf orlix_tcti_advsimd_min_max_leaves[] = {
	{ 3906U, "SMAX_asimdsame_only", "SMAX", "SMAX_advsimd", 0xbf20fc00U,
	  0x0e206400U, ORLIX_TCTI_SIMD_ARITH_SMAX, false, false },
	{ 3907U, "SMIN_asimdsame_only", "SMIN", "SMIN_advsimd", 0xbf20fc00U,
	  0x0e206c00U, ORLIX_TCTI_SIMD_ARITH_SMIN, false, true },
	{ 3948U, "UMAX_asimdsame_only", "UMAX", "UMAX_advsimd", 0xbf20fc00U,
	  0x2e206400U, ORLIX_TCTI_SIMD_ARITH_UMAX, true, false },
	{ 3949U, "UMIN_asimdsame_only", "UMIN", "UMIN_advsimd", 0xbf20fc00U,
	  0x2e206c00U, ORLIX_TCTI_SIMD_ARITH_UMIN, true, true },
};

struct orlix_tcti_advsimd_min_max_context {
	struct mm_struct *mm;
	unsigned long instructions;
	u64 simd[ARRAY_SIZE(current->thread.user_simd)];
	unsigned long simd_valid;
	unsigned long fpcr;
	unsigned long fpsr;
};

struct orlix_tcti_advsimd_min_max_registers {
	u8 rd;
	u8 rn;
	u8 rm;
};

static const struct orlix_tcti_advsimd_min_max_registers
	orlix_tcti_advsimd_min_max_overlap_cases[] = {
		{ 3U, 4U, 5U },	   { 6U, 6U, 7U },    { 8U, 9U, 8U },
		{ 10U, 11U, 11U }, { 12U, 12U, 12U }, { 31U, 0U, 30U },
	};

static const char *orlix_tcti_advsimd_min_max_artifact_string(
	const struct orlix_tcti_target_instruction_artifact *artifact, u32 offset)
{
	if (offset >= artifact->string_pool_size)
		return NULL;

	return (const char *)artifact->string_pool + offset;
}

static int orlix_tcti_advsimd_min_max_test_init(struct kunit *test)
{
	struct orlix_tcti_advsimd_min_max_context *context;

	context = kunit_kzalloc(test, sizeof(*context), GFP_KERNEL);
	if (!context)
		return -ENOMEM;
	context->mm = mm_alloc();
	if (!context->mm)
		return -ENOMEM;
	kthread_use_mm(context->mm);
	context->instructions =
		ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
				MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (IS_ERR_VALUE(context->instructions)) {
		kthread_unuse_mm(context->mm);
		mmput(context->mm);
		return (int)context->instructions;
	}
	memcpy(context->simd, current->thread.user_simd, sizeof(context->simd));
	context->simd_valid = current->thread.user_simd_valid;
	context->fpcr = current->thread.user_fpcr;
	context->fpsr = current->thread.user_fpsr;
	test->priv = context;
	return 0;
}

static void orlix_tcti_advsimd_min_max_test_exit(struct kunit *test)
{
	struct orlix_tcti_advsimd_min_max_context *context = test->priv;

	memcpy(current->thread.user_simd, context->simd, sizeof(context->simd));
	current->thread.user_simd_valid = context->simd_valid;
	current->thread.user_fpcr = context->fpcr;
	current->thread.user_fpsr = context->fpsr;
	vm_munmap(context->instructions, PAGE_SIZE);
	kthread_unuse_mm(context->mm);
	mmput(context->mm);
}

static u32
orlix_tcti_advsimd_min_max_instruction(const struct orlix_tcti_advsimd_min_max_leaf *leaf,
				 u8 q, u8 size, u8 rd, u8 rn, u8 rm)
{
	return leaf->source_pattern | ((u32)q << 30) | ((u32)size << 22) |
	       ((u32)rm << 16) | ((u32)rn << 5) | rd;
}

static int orlix_tcti_advsimd_min_max_load_instruction(struct kunit *test,
						 u32 instruction)
{
	struct orlix_tcti_advsimd_min_max_context *context = test->priv;
	const u32 program[] = { instruction, ORLIX_TCTI_ADVSIMD_MIN_MAX_SVC };
	int ret;

	ret = sys_mprotect(context->instructions, PAGE_SIZE,
			   PROT_READ | PROT_WRITE);
	if (ret)
		return ret;
	ret = orlix_tcti_write_user_data(current->mm, context->instructions, program,
				   sizeof(program));
	if (ret)
		return ret;
	return sys_mprotect(context->instructions, PAGE_SIZE,
			    PROT_READ | PROT_EXEC);
}

static void orlix_tcti_advsimd_min_max_seed_regs(struct pt_regs *regs,
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

static void orlix_tcti_advsimd_min_max_expected(
	u64 result[2], const struct orlix_tcti_advsimd_min_max_leaf *leaf,
	const u64 left[2], const u64 right[2], u8 lane_bytes, u8 result_bytes)
{
	u64 mask = GENMASK_ULL(lane_bytes * 8 - 1, 0);
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
		u64 selected;

		if (leaf->is_unsigned) {
			selected =
				leaf->minimum ?
					(left_lane < right_lane ? left_lane :
								  right_lane) :
					(left_lane > right_lane ? left_lane :
								  right_lane);
		} else {
			s64 signed_left =
				sign_extend64(left_lane, lane_bytes * 8 - 1);
			s64 signed_right =
				sign_extend64(right_lane, lane_bytes * 8 - 1);

			selected = leaf->minimum ? (signed_left < signed_right ?
							    left_lane :
							    right_lane) :
						   (signed_left > signed_right ?
							    left_lane :
							    right_lane);
		}
		result[word] |= selected << shift;
	}
}

static void orlix_tcti_advsimd_min_max_source_bindings(struct kunit *test)
{
	const struct orlix_tcti_target_instruction_artifact *artifact =
		orlix_tcti_target_instruction_artifact_canonical();
	struct orlix_tcti_target_instruction_artifact_validation_result validation;
	size_t index;

	KUNIT_ASSERT_NOT_NULL(test, artifact);
	KUNIT_ASSERT_EQ(test, 0,
			orlix_tcti_target_instruction_artifact_validate(artifact,
								  &validation));
	KUNIT_ASSERT_GT(test, artifact->leaf_count, 3949U);
	KUNIT_ASSERT_EQ(test, 4U, ARRAY_SIZE(orlix_tcti_advsimd_min_max_leaves));
	for (index = 0; index < ARRAY_SIZE(orlix_tcti_advsimd_min_max_leaves);
	     index++) {
		const struct orlix_tcti_advsimd_min_max_leaf *leaf =
			&orlix_tcti_advsimd_min_max_leaves[index];
		const struct orlix_tcti_target_instruction_artifact_leaf *source =
			&artifact->leaves[leaf->source_ordinal];
		const char *name = orlix_tcti_advsimd_min_max_artifact_string(
			artifact, source->name_offset);
		const char *mnemonic = orlix_tcti_advsimd_min_max_artifact_string(
			artifact, source->mnemonic_offset);
		const char *operation = orlix_tcti_advsimd_min_max_artifact_string(
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
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_ADVSIMD_MIN_MAX_VARIABLE_MASK,
				~source->encoding_mask);
	}
}

static void orlix_tcti_advsimd_min_max_all_legal_fields_decode(struct kunit *test)
{
	size_t leaf_index;
	u8 q;
	u8 size;
	u8 reg;

	for (leaf_index = 0;
	     leaf_index < ARRAY_SIZE(orlix_tcti_advsimd_min_max_leaves);
	     leaf_index++) {
		const struct orlix_tcti_advsimd_min_max_leaf *leaf =
			&orlix_tcti_advsimd_min_max_leaves[leaf_index];

		for (q = 0; q < 2; q++) {
			for (size = 0; size < 3; size++) {
				for (reg = 0; reg < 32; reg++) {
					struct orlix_tcti_decoded_instruction decoded =
						orlix_tcti_decode_aarch64(
							orlix_tcti_advsimd_min_max_instruction(
								leaf, q, size,
								reg,
								(reg + 1U) &
									31U,
								(reg + 2U) &
									31U));

					KUNIT_EXPECT_EQ(
						test,
						ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC,
						decoded.decode_class);
					KUNIT_EXPECT_EQ(
						test, leaf->operation,
						decoded.simd_arithmetic_op);
					KUNIT_EXPECT_EQ(test, reg, decoded.rd);
					KUNIT_EXPECT_EQ(test, (reg + 1U) & 31U,
							decoded.rn);
					KUNIT_EXPECT_EQ(test, (reg + 2U) & 31U,
							decoded.rm);
					KUNIT_EXPECT_EQ(test, BIT(size),
							decoded.access_size);
					KUNIT_EXPECT_EQ(test,
							q ? 2 * sizeof(u64) :
							    sizeof(u64),
							decoded.result_size);
				}
			}
		}
	}
}

static void orlix_tcti_advsimd_min_max_reserved_forms_reject(struct kunit *test)
{
	struct orlix_tcti_advsimd_min_max_context *context = test->priv;
	size_t leaf_index;
	u8 q;

	for (leaf_index = 0;
	     leaf_index < ARRAY_SIZE(orlix_tcti_advsimd_min_max_leaves);
	     leaf_index++) {
		const struct orlix_tcti_advsimd_min_max_leaf *leaf =
			&orlix_tcti_advsimd_min_max_leaves[leaf_index];

		for (q = 0; q < 2; q++) {
			u32 instruction = orlix_tcti_advsimd_min_max_instruction(
				leaf, q, 3, 3, 4, 5);
			const u32 expected_program[] = {
				instruction,
				ORLIX_TCTI_ADVSIMD_MIN_MAX_SVC,
			};
			u32 before_program[ARRAY_SIZE(expected_program)];
			u32 after_program[ARRAY_SIZE(expected_program)];
			u64 before_simd[ARRAY_SIZE(current->thread.user_simd)];
			struct pt_regs regs;
			struct pt_regs before_regs;
			struct orlix_tcti_result result;
			unsigned int simd_index;
			int ret;

			KUNIT_EXPECT_EQ(
				test, ORLIX_TCTI_DECODE_UNSUPPORTED,
				orlix_tcti_decode_aarch64(instruction).decode_class);
			ret = orlix_tcti_advsimd_min_max_load_instruction(
				test, instruction);
			KUNIT_ASSERT_EQ(test, 0, ret);
			ret = orlix_tcti_read_user_data(current->mm,
						  context->instructions,
						  before_program,
						  sizeof(before_program));
			KUNIT_ASSERT_EQ(test, 0, ret);
			KUNIT_EXPECT_MEMEQ(test, expected_program,
					   before_program,
					   sizeof(expected_program));

			for (simd_index = 0;
			     simd_index < ARRAY_SIZE(current->thread.user_simd);
			     simd_index++)
				current->thread.user_simd[simd_index] =
					0xd6e8feb86659fd93ULL ^ simd_index;
			memcpy(before_simd, current->thread.user_simd,
			       sizeof(before_simd));
			current->thread.user_simd_valid = 1;
			current->thread.user_fpcr = BIT(22) | BIT(24);
			current->thread.user_fpsr = BIT(27) | BIT(4);
			orlix_tcti_advsimd_min_max_seed_regs(
				&regs, context->instructions, instruction);
			before_regs = regs;

			result = orlix_tcti_resume_user(current, &regs, current->mm);

			KUNIT_EXPECT_EQ_MSG(test,
					    ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
					    result.reason, "%s q=%u",
					    leaf->source_name, q);
			KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, result.status);
			KUNIT_EXPECT_EQ(test, 0UL, result.fault_address);
			KUNIT_EXPECT_EQ(test, ORLIX_TCTI_ACCESS_FETCH,
					result.fault_access);
			KUNIT_EXPECT_EQ(test, context->instructions, result.pc);
			KUNIT_EXPECT_EQ(test, instruction, result.instruction);
			KUNIT_EXPECT_MEMEQ(test, &before_regs, &regs,
					   sizeof(before_regs));
			KUNIT_EXPECT_MEMEQ(test, before_simd,
					   current->thread.user_simd,
					   sizeof(before_simd));
			KUNIT_EXPECT_EQ(test, 1UL,
					current->thread.user_simd_valid);
			KUNIT_EXPECT_EQ(test, BIT(22) | BIT(24),
					current->thread.user_fpcr);
			KUNIT_EXPECT_EQ(test, BIT(27) | BIT(4),
					current->thread.user_fpsr);

			ret = orlix_tcti_read_user_data(current->mm,
						  context->instructions,
						  after_program,
						  sizeof(after_program));
			KUNIT_ASSERT_EQ(test, 0, ret);
			KUNIT_EXPECT_MEMEQ(test, expected_program,
					   after_program,
					   sizeof(expected_program));
		}
	}
}

static void orlix_tcti_advsimd_min_max_source_leaves_resume(struct kunit *test)
{
	struct orlix_tcti_advsimd_min_max_context *context = test->priv;
	static const u64 left_seed[2] = {
		0x7fff8001ff020100ULL,
		0xffff000180ff7f01ULL,
	};
	static const u64 right_seed[2] = {
		0x80017fff0201ff02ULL,
		0x0001ffff7f0180ffULL,
	};
	size_t leaf_index;
	u8 q;
	u8 size;
	size_t overlap_index;

	for (leaf_index = 0;
	     leaf_index < ARRAY_SIZE(orlix_tcti_advsimd_min_max_leaves);
	     leaf_index++) {
		const struct orlix_tcti_advsimd_min_max_leaf *leaf =
			&orlix_tcti_advsimd_min_max_leaves[leaf_index];

		for (q = 0; q < 2; q++) {
			for (size = 0; size < 3; size++) {
				for (overlap_index = 0;
				     overlap_index <
				     ARRAY_SIZE(
					     orlix_tcti_advsimd_min_max_overlap_cases);
				     overlap_index++) {
					const struct orlix_tcti_advsimd_min_max_registers
						*registers =
							&orlix_tcti_advsimd_min_max_overlap_cases
								[overlap_index];
					u32 instruction =
						orlix_tcti_advsimd_min_max_instruction(
							leaf, q, size,
							registers->rd,
							registers->rn,
							registers->rm);
					const u32 expected_program[] = {
						instruction,
						ORLIX_TCTI_ADVSIMD_MIN_MAX_SVC,
					};
					u32 before_program[ARRAY_SIZE(
						expected_program)];
					u32 after_program[ARRAY_SIZE(
						expected_program)];
					u64 expected_simd[ARRAY_SIZE(
						current->thread.user_simd)];
					u64 left[2];
					u64 right[2];
					u64 expected_result[2];
					struct pt_regs regs;
					struct pt_regs expected_regs;
					struct orlix_tcti_result result;
					unsigned int simd_index;
					int ret;

					ret = orlix_tcti_advsimd_min_max_load_instruction(
						test, instruction);
					KUNIT_ASSERT_EQ(test, 0, ret);
					ret = orlix_tcti_read_user_data(
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
							0x6a09e667f3bcc909ULL ^
							((u64)(simd_index +
							       1U) *
							 0x0102040810204081ULL);
					current->thread
						.user_simd[registers->rn * 2U] =
						left_seed[0];
					current->thread
						.user_simd[registers->rn * 2U +
							   1U] = left_seed[1];
					current->thread
						.user_simd[registers->rm * 2U] =
						right_seed[0];
					current->thread
						.user_simd[registers->rm * 2U +
							   1U] = right_seed[1];
					left[0] = current->thread.user_simd
							  [registers->rn * 2U];
					left[1] = current->thread.user_simd
							  [registers->rn * 2U +
							   1U];
					right[0] = current->thread.user_simd
							   [registers->rm * 2U];
					right[1] = current->thread.user_simd
							   [registers->rm * 2U +
							    1U];
					memcpy(expected_simd,
					       current->thread.user_simd,
					       sizeof(expected_simd));
					orlix_tcti_advsimd_min_max_expected(
						expected_result, leaf, left,
						right, BIT(size),
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
					orlix_tcti_advsimd_min_max_seed_regs(
						&regs, context->instructions,
						instruction);
					expected_regs = regs;
					expected_regs.pc += sizeof(u32);

					result = orlix_tcti_resume_user(
						current, &regs, current->mm);

					KUNIT_EXPECT_EQ_MSG(
						test, ORLIX_TCTI_EXIT_SYSCALL,
						result.reason,
						"%s q=%u size=%u overlap=%zu",
						leaf->source_name, q, size,
						overlap_index);
					KUNIT_EXPECT_EQ(test, 0L,
							result.status);
					KUNIT_EXPECT_EQ(test, 0UL,
							result.fault_address);
					KUNIT_EXPECT_EQ(test, ORLIX_TCTI_ACCESS_FETCH,
							result.fault_access);
					KUNIT_EXPECT_EQ(test,
							context->instructions +
								sizeof(u32),
							result.pc);
					KUNIT_EXPECT_EQ(
						test, ORLIX_TCTI_ADVSIMD_MIN_MAX_SVC,
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

					ret = orlix_tcti_read_user_data(
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

static struct kunit_case orlix_tcti_advsimd_min_max_source_bound_cases[] = {
	KUNIT_CASE(orlix_tcti_advsimd_min_max_source_bindings),
	KUNIT_CASE(orlix_tcti_advsimd_min_max_all_legal_fields_decode),
	KUNIT_CASE(orlix_tcti_advsimd_min_max_reserved_forms_reject),
	KUNIT_CASE(orlix_tcti_advsimd_min_max_source_leaves_resume),
	{}
};

static struct kunit_suite orlix_tcti_advsimd_min_max_source_bound_suite = {
	.name = "orlix-tcti-advsimd-integer-min-max-source-bound",
	.init = orlix_tcti_advsimd_min_max_test_init,
	.exit = orlix_tcti_advsimd_min_max_test_exit,
	.test_cases = orlix_tcti_advsimd_min_max_source_bound_cases,
};

kunit_test_suite(orlix_tcti_advsimd_min_max_source_bound_suite);

MODULE_LICENSE("GPL");
