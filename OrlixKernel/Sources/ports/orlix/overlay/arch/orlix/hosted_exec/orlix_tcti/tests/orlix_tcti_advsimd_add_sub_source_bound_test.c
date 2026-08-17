// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/sched/mm.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/syscalls.h>

#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/orlix_tcti.h>

#include "../decode_aarch64.h"
#include "target_instruction_artifact.h"

#define ORLIX_TCTI_ADVSIMD_ADD_SUB_SVC 0xd4000001U
#define ORLIX_TCTI_ADVSIMD_ADD_SUB_RD 4U
#define ORLIX_TCTI_ADVSIMD_ADD_SUB_RN 5U
#define ORLIX_TCTI_ADVSIMD_ADD_SUB_RM 6U

struct orlix_tcti_advsimd_add_sub_leaf {
	u16 source_ordinal;
	const char *source_name;
	const char *source_operation;
	u32 source_mask;
	u32 source_pattern;
	enum orlix_tcti_simd_vector_arithmetic_op operation;
	bool scalar;
};

static const struct orlix_tcti_advsimd_add_sub_leaf orlix_tcti_advsimd_add_sub_leaves[] = {
	{ 3610U, "ADD_asisdsame_only", "ADD_advsimd", 0xffe0fc00U, 0x5ee08400U,
	  ORLIX_TCTI_SIMD_ARITH_ADD, true },
	{ 3625U, "SUB_asisdsame_only", "SUB_advsimd", 0xffe0fc00U, 0x7ee08400U,
	  ORLIX_TCTI_SIMD_ARITH_SUB, true },
	{ 3910U, "ADD_asimdsame_only", "ADD_advsimd", 0xbf20fc00U, 0x0e208400U,
	  ORLIX_TCTI_SIMD_ARITH_ADD, false },
	{ 3952U, "SUB_asimdsame_only", "SUB_advsimd", 0xbf20fc00U, 0x2e208400U,
	  ORLIX_TCTI_SIMD_ARITH_SUB, false },
};

static const char *orlix_tcti_advsimd_add_sub_artifact_string(
	const struct orlix_tcti_target_instruction_artifact *artifact, u32 offset)
{
	if (offset >= artifact->string_pool_size)
		return NULL;

	return (const char *)artifact->string_pool + offset;
}

static u32
orlix_tcti_advsimd_add_sub_instruction(const struct orlix_tcti_advsimd_add_sub_leaf *leaf,
				 u8 q, u8 size, u8 rd, u8 rn, u8 rm)
{
	u32 instruction = leaf->source_pattern | ((u32)rm << 16) |
			  ((u32)rn << 5) | rd;

	if (!leaf->scalar)
		instruction |= ((u32)q << 30) | ((u32)size << 22);
	return instruction;
}

static unsigned long orlix_tcti_advsimd_add_sub_map_instruction(struct kunit *test,
							  u32 instruction)
{
	const u32 program[] = { instruction, ORLIX_TCTI_ADVSIMD_ADD_SUB_SVC };
	unsigned long mapped;
	int ret;

	KUNIT_ASSERT_NOT_NULL(test, current->mm);
	mapped = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));
	ret = orlix_tcti_write_user_data(current->mm, mapped, program,
				   sizeof(program));
	if (ret) {
		vm_munmap(mapped, PAGE_SIZE);
		KUNIT_FAIL(test, "could not write AdvSIMD ADD/SUB program: %d",
			   ret);
		return 0;
	}
	ret = sys_mprotect(mapped, PAGE_SIZE, PROT_READ | PROT_EXEC);
	if (ret) {
		vm_munmap(mapped, PAGE_SIZE);
		KUNIT_FAIL(test,
			   "could not protect AdvSIMD ADD/SUB program: %d",
			   ret);
		return 0;
	}
	return mapped;
}

static void orlix_tcti_advsimd_add_sub_expected(
	u64 result[2], enum orlix_tcti_simd_vector_arithmetic_op operation,
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
		u64 lhs = (left[word] >> shift) & mask;
		u64 rhs = (right[word] >> shift) & mask;
		u64 value = operation == ORLIX_TCTI_SIMD_ARITH_ADD ? lhs + rhs :
							       lhs - rhs;

		result[word] |= (value & mask) << shift;
	}
}

static void orlix_tcti_advsimd_add_sub_seed_regs(struct pt_regs *regs,
					   unsigned long pc, u32 instruction)
{
	u8 reg;

	memset(regs, 0, sizeof(*regs));
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

static void orlix_tcti_advsimd_add_sub_source_leaves_decode(struct kunit *test)
{
	const struct orlix_tcti_target_instruction_artifact *artifact =
		orlix_tcti_target_instruction_artifact_canonical();
	struct orlix_tcti_target_instruction_artifact_validation_result validation;
	size_t index;

	KUNIT_ASSERT_NOT_NULL(test, artifact);
	KUNIT_ASSERT_EQ(test, 0,
			orlix_tcti_target_instruction_artifact_validate(artifact,
								  &validation));
	KUNIT_ASSERT_GT(test, artifact->leaf_count, 3952U);
	KUNIT_ASSERT_EQ(test, 4U, ARRAY_SIZE(orlix_tcti_advsimd_add_sub_leaves));
	for (index = 0; index < ARRAY_SIZE(orlix_tcti_advsimd_add_sub_leaves);
	     index++) {
		const struct orlix_tcti_advsimd_add_sub_leaf *leaf =
			&orlix_tcti_advsimd_add_sub_leaves[index];
		const struct orlix_tcti_target_instruction_artifact_leaf *source =
			&artifact->leaves[leaf->source_ordinal];
		const char *source_name = orlix_tcti_advsimd_add_sub_artifact_string(
			artifact, source->name_offset);
		const char *source_operation =
			orlix_tcti_advsimd_add_sub_artifact_string(
				artifact, source->operation_offset);
		u8 q;
		u8 size;

		KUNIT_ASSERT_NOT_NULL(test, source_name);
		KUNIT_ASSERT_NOT_NULL(test, source_operation);
		KUNIT_EXPECT_STREQ(test, leaf->source_name, source_name);
		KUNIT_EXPECT_STREQ(test, leaf->source_operation,
				   source_operation);
		KUNIT_EXPECT_EQ(test, leaf->source_mask, source->encoding_mask);
		KUNIT_EXPECT_EQ(test, leaf->source_pattern,
				source->encoding_pattern);
		if (leaf->scalar) {
			struct orlix_tcti_decoded_instruction decoded =
				orlix_tcti_decode_aarch64(
					orlix_tcti_advsimd_add_sub_instruction(
						leaf, 0, 3,
						ORLIX_TCTI_ADVSIMD_ADD_SUB_RD,
						ORLIX_TCTI_ADVSIMD_ADD_SUB_RN,
						ORLIX_TCTI_ADVSIMD_ADD_SUB_RM));

			KUNIT_EXPECT_EQ(test,
					ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC,
					decoded.decode_class);
			KUNIT_EXPECT_EQ(test, leaf->operation,
					decoded.simd_arithmetic_op);
			KUNIT_EXPECT_TRUE(test, decoded.simd_scalar);
			KUNIT_EXPECT_EQ(test, sizeof(u64), decoded.access_size);
			KUNIT_EXPECT_EQ(test, sizeof(u64), decoded.result_size);
			continue;
		}

		for (q = 0; q < 2; q++) {
			for (size = 0; size < 4; size++) {
				struct orlix_tcti_decoded_instruction decoded =
					orlix_tcti_decode_aarch64(
						orlix_tcti_advsimd_add_sub_instruction(
							leaf, q, size,
							ORLIX_TCTI_ADVSIMD_ADD_SUB_RD,
							ORLIX_TCTI_ADVSIMD_ADD_SUB_RN,
							ORLIX_TCTI_ADVSIMD_ADD_SUB_RM));

				if (!q && size == 3) {
					KUNIT_EXPECT_EQ(test,
							ORLIX_TCTI_DECODE_UNSUPPORTED,
							decoded.decode_class);
					continue;
				}
				KUNIT_EXPECT_EQ(
					test,
					ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC,
					decoded.decode_class);
				KUNIT_EXPECT_EQ(test, leaf->operation,
						decoded.simd_arithmetic_op);
				KUNIT_EXPECT_FALSE(test, decoded.simd_scalar);
				KUNIT_EXPECT_EQ(test, BIT(size),
						decoded.access_size);
				KUNIT_EXPECT_EQ(
					test, q ? 2 * sizeof(u64) : sizeof(u64),
					decoded.result_size);
			}
		}
	}
}

static void
orlix_tcti_advsimd_add_sub_reserved_vector_1d_preserves_state(struct kunit *test)
{
	size_t leaf_index;

	for (leaf_index = 2;
	     leaf_index < ARRAY_SIZE(orlix_tcti_advsimd_add_sub_leaves);
	     leaf_index++) {
		const struct orlix_tcti_advsimd_add_sub_leaf *leaf =
			&orlix_tcti_advsimd_add_sub_leaves[leaf_index];
		u32 instruction = orlix_tcti_advsimd_add_sub_instruction(
			leaf, 0, 3, ORLIX_TCTI_ADVSIMD_ADD_SUB_RD,
			ORLIX_TCTI_ADVSIMD_ADD_SUB_RN, ORLIX_TCTI_ADVSIMD_ADD_SUB_RM);
		unsigned long mapped =
			orlix_tcti_advsimd_add_sub_map_instruction(test, instruction);
		u64 saved_simd[ARRAY_SIZE(current->thread.user_simd)];
		u64 before_simd[ARRAY_SIZE(current->thread.user_simd)];
		unsigned long saved_valid = current->thread.user_simd_valid;
		unsigned long saved_fpcr = current->thread.user_fpcr;
		unsigned long saved_fpsr = current->thread.user_fpsr;
		struct pt_regs regs = {};
		struct pt_regs before;
		struct orlix_tcti_result result;
		unsigned int simd_index;

		KUNIT_ASSERT_FALSE(test, leaf->scalar);
		KUNIT_ASSERT_NE(test, 0UL, mapped);
		memcpy(saved_simd, current->thread.user_simd,
		       sizeof(saved_simd));
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
		orlix_tcti_advsimd_add_sub_seed_regs(&regs, mapped, instruction);
		before = regs;

		result = orlix_tcti_resume_user(current, &regs, current->mm);

		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
				    result.reason, "%s", leaf->source_name);
		KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, result.status);
		KUNIT_EXPECT_EQ(test, mapped, result.pc);
		KUNIT_EXPECT_EQ(test, instruction, result.instruction);
		KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
		KUNIT_EXPECT_MEMEQ(test, before_simd, current->thread.user_simd,
				   sizeof(before_simd));
		KUNIT_EXPECT_EQ(test, 1UL, current->thread.user_simd_valid);
		KUNIT_EXPECT_EQ(test, BIT(22) | BIT(24),
				current->thread.user_fpcr);
		KUNIT_EXPECT_EQ(test, BIT(27) | BIT(4),
				current->thread.user_fpsr);

		memcpy(current->thread.user_simd, saved_simd,
		       sizeof(saved_simd));
		current->thread.user_simd_valid = saved_valid;
		current->thread.user_fpcr = saved_fpcr;
		current->thread.user_fpsr = saved_fpsr;
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
	}
}

static void orlix_tcti_advsimd_add_sub_source_leaves_execute(struct kunit *test)
{
	static const struct {
		u8 rd;
		u8 rn;
		u8 rm;
	} aliases[] = {
		{ ORLIX_TCTI_ADVSIMD_ADD_SUB_RD, ORLIX_TCTI_ADVSIMD_ADD_SUB_RN,
		  ORLIX_TCTI_ADVSIMD_ADD_SUB_RM },
		{ ORLIX_TCTI_ADVSIMD_ADD_SUB_RN, ORLIX_TCTI_ADVSIMD_ADD_SUB_RN,
		  ORLIX_TCTI_ADVSIMD_ADD_SUB_RM },
		{ ORLIX_TCTI_ADVSIMD_ADD_SUB_RM, ORLIX_TCTI_ADVSIMD_ADD_SUB_RN,
		  ORLIX_TCTI_ADVSIMD_ADD_SUB_RM },
	};
	static const u64 left[2] = { 0x7fff8001ff020100ULL,
				     0xffff000180ff7f01ULL };
	static const u64 right[2] = { 0x80017fff0201ff02ULL,
				      0x0001ffff7f0180ffULL };
	size_t leaf_index;

	for (leaf_index = 0;
	     leaf_index < ARRAY_SIZE(orlix_tcti_advsimd_add_sub_leaves);
	     leaf_index++) {
		const struct orlix_tcti_advsimd_add_sub_leaf *leaf =
			&orlix_tcti_advsimd_add_sub_leaves[leaf_index];
		u8 q_limit = leaf->scalar ? 1 : 2;
		u8 q;

		for (q = 0; q < q_limit; q++) {
			u8 first_size = leaf->scalar ? 3 : 0;
			u8 size;

			for (size = first_size; size < 4; size++) {
				u8 result_bytes = leaf->scalar ?
							  sizeof(u64) :
							  (q ? 2 * sizeof(u64) :
							       sizeof(u64));
				u8 alias_index;

				if (!leaf->scalar && !q && size == 3)
					continue;
				for (alias_index = 0;
				     alias_index < ARRAY_SIZE(aliases);
				     alias_index++) {
					u32 instruction =
						orlix_tcti_advsimd_add_sub_instruction(
							leaf, q, size,
							aliases[alias_index].rd,
							aliases[alias_index].rn,
							aliases[alias_index].rm);
					unsigned long mapped =
						orlix_tcti_advsimd_add_sub_map_instruction(
							test, instruction);
					u64 saved_simd[ARRAY_SIZE(
						current->thread.user_simd)];
					u64 expected_simd[ARRAY_SIZE(
						current->thread.user_simd)];
					u64 expected_result[2];
					unsigned long saved_valid =
						current->thread.user_simd_valid;
					unsigned long saved_fpcr =
						current->thread.user_fpcr;
					unsigned long saved_fpsr =
						current->thread.user_fpsr;
					struct pt_regs regs = {};
					struct pt_regs before;
					struct pt_regs expected_regs;
					struct orlix_tcti_result result;
					unsigned int simd_index;

					KUNIT_ASSERT_NE(test, 0UL, mapped);
					memcpy(saved_simd,
					       current->thread.user_simd,
					       sizeof(saved_simd));
					for (simd_index = 0;
					     simd_index <
					     ARRAY_SIZE(
						     current->thread.user_simd);
					     simd_index++)
						current->thread
							.user_simd[simd_index] =
							0x9e3779b97f4a7c15ULL ^
							simd_index;
					current->thread.user_simd
						[aliases[alias_index].rn * 2] =
						left[0];
					current->thread.user_simd
						[aliases[alias_index].rn * 2 +
						 1] = left[1];
					current->thread.user_simd
						[aliases[alias_index].rm * 2] =
						right[0];
					current->thread.user_simd
						[aliases[alias_index].rm * 2 +
						 1] = right[1];
					memcpy(expected_simd,
					       current->thread.user_simd,
					       sizeof(expected_simd));
					orlix_tcti_advsimd_add_sub_expected(
						expected_result,
						leaf->operation, left, right,
						BIT(size), result_bytes);
					expected_simd[aliases[alias_index].rd *
						      2] = expected_result[0];
					expected_simd[aliases[alias_index].rd *
							      2 +
						      1] = expected_result[1];
					current->thread.user_simd_valid = 0;
					current->thread.user_fpcr = BIT(22) |
								    BIT(24);
					current->thread.user_fpsr = BIT(27) |
								    BIT(4);
					orlix_tcti_advsimd_add_sub_seed_regs(
						&regs, mapped, instruction);
					before = regs;
					expected_regs = before;
					expected_regs.pc += sizeof(u32);

					result = orlix_tcti_resume_user(
						current, &regs, current->mm);

					KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL,
							result.reason);
					KUNIT_EXPECT_EQ(test, 0L,
							result.status);
					KUNIT_EXPECT_EQ(test,
							mapped + sizeof(u32),
							result.pc);
					KUNIT_EXPECT_EQ(
						test, ORLIX_TCTI_ADVSIMD_ADD_SUB_SVC,
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
					KUNIT_EXPECT_MEMEQ(test, &expected_regs,
							   &regs, sizeof(regs));

					memcpy(current->thread.user_simd,
					       saved_simd, sizeof(saved_simd));
					current->thread.user_simd_valid =
						saved_valid;
					current->thread.user_fpcr = saved_fpcr;
					current->thread.user_fpsr = saved_fpsr;
					KUNIT_EXPECT_EQ(test, 0,
							vm_munmap(mapped,
								  PAGE_SIZE));
				}
			}
		}
	}
}

static struct kunit_case orlix_tcti_advsimd_add_sub_source_bound_cases[] = {
	KUNIT_CASE(orlix_tcti_advsimd_add_sub_source_leaves_decode),
	KUNIT_CASE(orlix_tcti_advsimd_add_sub_reserved_vector_1d_preserves_state),
	KUNIT_CASE(orlix_tcti_advsimd_add_sub_source_leaves_execute),
	{}
};

static int orlix_tcti_advsimd_add_sub_source_bound_init(struct kunit *test)
{
	struct mm_struct *mm = mm_alloc();

	if (!mm)
		return -ENOMEM;
	kthread_use_mm(mm);
	test->priv = mm;
	return 0;
}

static void orlix_tcti_advsimd_add_sub_source_bound_exit(struct kunit *test)
{
	struct mm_struct *mm = test->priv;

	if (!mm)
		return;
	kthread_unuse_mm(mm);
	mmput(mm);
}

static struct kunit_suite orlix_tcti_advsimd_add_sub_source_bound_suite = {
	.name = "orlix-tcti-advsimd-add-sub-source-bound",
	.init = orlix_tcti_advsimd_add_sub_source_bound_init,
	.exit = orlix_tcti_advsimd_add_sub_source_bound_exit,
	.test_cases = orlix_tcti_advsimd_add_sub_source_bound_cases,
};

kunit_test_suite(orlix_tcti_advsimd_add_sub_source_bound_suite);

MODULE_LICENSE("GPL");
