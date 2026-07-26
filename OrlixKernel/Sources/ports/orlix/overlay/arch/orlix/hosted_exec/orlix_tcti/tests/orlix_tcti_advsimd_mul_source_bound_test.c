// SPDX-License-Identifier: GPL-2.0-only
/*
 * Source-bound production-path proof for MLA, MLS, and MUL AdvSIMD integer
 * vector leaves. The oracle works from the register state consumed by TCTI,
 * so overlapping register arrangements remain independent of the executor.
 */
#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/orlix_tcti.h>
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
#include "target_instruction_artifact.h"

#define ADVSIMD_MUL_SVC	0xd4000001U

struct advsimd_mul_leaf {
	u16 ordinal;
	const char *name;
	const char *mnemonic;
	const char *operation;
	u32 mask;
	u32 pattern;
	enum orlix_tcti_simd_vector_arithmetic_op arithmetic_op;
};

struct advsimd_mul_context {
	struct mm_struct *mm;
	unsigned long simd[ARRAY_SIZE(current->thread.user_simd)];
	unsigned long simd_valid;
	unsigned long fpcr;
	unsigned long fpsr;
};

static const struct advsimd_mul_leaf advsimd_mul_leaves[] = {
	{ 3912U, "MLA_asimdsame_only", "MLA", "MLA_advsimd_vec",
	  0xbf20fc00U, 0x0e209400U, ORLIX_TCTI_SIMD_ARITH_MLA },
	{ 3913U, "MUL_asimdsame_only", "MUL", "MUL_advsimd_vec",
	  0xbf20fc00U, 0x0e209c00U, ORLIX_TCTI_SIMD_ARITH_MUL },
	{ 3954U, "MLS_asimdsame_only", "MLS", "MLS_advsimd_vec",
	  0xbf20fc00U, 0x2e209400U, ORLIX_TCTI_SIMD_ARITH_MLS },
};

static int advsimd_mul_init(struct kunit *test)
{
	struct advsimd_mul_context *context;

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

static void advsimd_mul_exit(struct kunit *test)
{
	struct advsimd_mul_context *context = test->priv;

	memcpy(current->thread.user_simd, context->simd, sizeof(context->simd));
	current->thread.user_simd_valid = context->simd_valid;
	current->thread.user_fpcr = context->fpcr;
	current->thread.user_fpsr = context->fpsr;
	kthread_unuse_mm(context->mm);
	mmput(context->mm);
}

static u32 advsimd_mul_instruction(const struct advsimd_mul_leaf *leaf, u8 q,
				  u8 size, u8 rd, u8 rn, u8 rm)
{
	return leaf->pattern | ((u32)q << 30) | ((u32)size << 22) |
		((u32)rm << 16) | ((u32)rn << 5) | rd;
}

static unsigned long advsimd_mul_map_program(struct kunit *test, u32 instruction)
{
	const u32 program[] = { instruction, ADVSIMD_MUL_SVC };
	unsigned long address;
	int ret;

	address = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(address));
	ret = orlix_tcti_write_user_data(current->mm, address, program, sizeof(program));
	if (ret) {
		vm_munmap(address, PAGE_SIZE);
		KUNIT_FAIL(test, "could not write AdvSIMD multiply program: %d", ret);
		return 0;
	}
	ret = sys_mprotect(address, PAGE_SIZE, PROT_READ | PROT_EXEC);
	if (ret) {
		vm_munmap(address, PAGE_SIZE);
		KUNIT_FAIL(test, "could not protect AdvSIMD multiply program: %d", ret);
		return 0;
	}
	return address;
}

static void advsimd_mul_expect_code(struct kunit *test, unsigned long address,
				    const u32 expected[2])
{
	u32 observed[2] = {};

	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_read_user_data(current->mm, address,
						       observed, sizeof(observed)));
	KUNIT_EXPECT_MEMEQ(test, expected, observed, sizeof(observed));
}

static void advsimd_mul_seed_regs(struct pt_regs *regs, unsigned long pc)
{
	u8 index;

	memset(regs, 0, sizeof(*regs));
	for (index = 0; index < ARRAY_SIZE(regs->regs); index++)
		regs->regs[index] = 0x9e3779b97f4a7c15ULL ^
			((u64)(index + 1) * 0x0101010101010101ULL);
	regs->sp = 0x00000001fffffff0ULL;
	regs->pc = pc;
	regs->pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_C_BIT | PSR_V_BIT;
	regs->orig_x0 = 0xd1b54a32d192ed03ULL;
	regs->syscallno = NO_SYSCALL;
	regs->unused = 0x6d5a56c3U;
}

static void advsimd_mul_seed_simd(u32 instruction)
{
	u8 index;

	for (index = 0; index < ARRAY_SIZE(current->thread.user_simd); index++)
		current->thread.user_simd[index] =
			0xa5a5a5a55a5a5a5aULL ^ ((u64)instruction << 11) ^ index;
	current->thread.user_simd_valid = 0;
	current->thread.user_fpcr = BIT(22) | BIT(24);
	current->thread.user_fpsr = BIT(27) | BIT(4);
}

static void advsimd_mul_fill_register(unsigned long words[2], u8 lane_bytes,
				      u8 result_bytes, u8 salt)
{
	u8 lane;

	words[0] = 0;
	words[1] = 0;
	for (lane = 0; lane < result_bytes / lane_bytes; lane++) {
		u8 byte = lane * lane_bytes;
		u8 word = byte / sizeof(u64);
		u8 shift = (byte % sizeof(u64)) * 8;
		u64 value = 0x0101010101010101ULL * (lane + salt + 1);

		value ^= BIT_ULL(lane_bytes * 8 - 1);
		words[word] |= (value & GENMASK_ULL(lane_bytes * 8 - 1, 0)) << shift;
	}
}

static void advsimd_mul_expected(unsigned long result[2],
				 enum orlix_tcti_simd_vector_arithmetic_op operation,
				 const unsigned long accumulator[2],
				 const unsigned long left[2],
				 const unsigned long right[2],
				 u8 lane_bytes, u8 result_bytes)
{
	u64 mask = GENMASK_ULL(lane_bytes * 8 - 1, 0);
	u8 lane;

	result[0] = 0;
	result[1] = 0;
	for (lane = 0; lane < result_bytes / lane_bytes; lane++) {
		u8 byte = lane * lane_bytes;
		u8 word = byte / sizeof(u64);
		u8 shift = (byte % sizeof(u64)) * 8;
		u64 acc = (accumulator[word] >> shift) & mask;
		u64 lhs = (left[word] >> shift) & mask;
		u64 rhs = (right[word] >> shift) & mask;
		u64 product = (lhs * rhs) & mask;
		u64 value = operation == ORLIX_TCTI_SIMD_ARITH_MLA ? acc + product :
			operation == ORLIX_TCTI_SIMD_ARITH_MLS ? acc - product : product;

		result[word] |= (value & mask) << shift;
	}
}

static void advsimd_mul_bind_canonical_artifacts(struct kunit *test)
{
	const struct orlix_tcti_target_instruction_artifact *artifact =
		orlix_tcti_target_instruction_artifact_canonical();
	struct orlix_tcti_target_instruction_artifact_validation_result validation;
	size_t index;
	u8 q;
	u8 size;

	KUNIT_ASSERT_NOT_NULL(test, artifact);
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_target_instruction_artifact_validate(artifact, &validation));
	for (index = 0; index < ARRAY_SIZE(advsimd_mul_leaves); index++) {
		const struct advsimd_mul_leaf *leaf = &advsimd_mul_leaves[index];
		const struct orlix_tcti_target_instruction_artifact_leaf *source =
			&artifact->leaves[leaf->ordinal];
		const char *name = (const char *)artifact->string_pool + source->name_offset;
		const char *mnemonic = (const char *)artifact->string_pool + source->mnemonic_offset;
		const char *operation = (const char *)artifact->string_pool + source->operation_offset;

		KUNIT_ASSERT_LT(test, leaf->ordinal, artifact->leaf_count);
		KUNIT_EXPECT_STREQ(test, leaf->name, name);
		KUNIT_EXPECT_STREQ(test, leaf->mnemonic, mnemonic);
		KUNIT_EXPECT_STREQ(test, leaf->operation, operation);
		KUNIT_EXPECT_EQ(test, leaf->mask, source->encoding_mask);
		KUNIT_EXPECT_EQ(test, leaf->pattern, source->encoding_pattern);
		for (q = 0; q < 2; q++)
			for (size = 0; size < 4; size++) {
				struct orlix_tcti_decoded_instruction decoded =
					orlix_tcti_decode_aarch64(advsimd_mul_instruction(leaf, q, size,
										  4, 5, 6));

				if (size == 3) {
					KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
							decoded.decode_class);
					continue;
				}
				KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC,
						decoded.decode_class);
				KUNIT_EXPECT_EQ(test, leaf->arithmetic_op,
						decoded.simd_arithmetic_op);
				KUNIT_EXPECT_EQ(test, BIT(size), decoded.access_size);
				KUNIT_EXPECT_EQ(test, q ? 16U : 8U, decoded.result_size);
				KUNIT_EXPECT_FALSE(test, decoded.simd_scalar);
			}
	}
}

static void advsimd_mul_execute_all_aliases(struct kunit *test)
{
	static const struct { u8 rd, rn, rm; } arrangements[] = {
		{ 4, 5, 6 }, { 5, 5, 6 }, { 6, 5, 6 },
		{ 4, 5, 5 }, { 31, 30, 29 }, { 31, 31, 31 },
	};
	size_t leaf_index;
	u8 q;
	u8 size;
	u8 arrangement;

	for (leaf_index = 0; leaf_index < ARRAY_SIZE(advsimd_mul_leaves); leaf_index++)
		for (q = 0; q < 2; q++)
			for (size = 0; size < 3; size++)
				for (arrangement = 0; arrangement < ARRAY_SIZE(arrangements);
				     arrangement++) {
					const struct advsimd_mul_leaf *leaf =
						&advsimd_mul_leaves[leaf_index];
					const u8 lane_bytes = BIT(size);
					const u8 result_bytes = q ? 16 : 8;
					const u32 instruction = advsimd_mul_instruction(leaf, q, size,
						arrangements[arrangement].rd, arrangements[arrangement].rn,
						arrangements[arrangement].rm);
					const u32 program[] = { instruction, ADVSIMD_MUL_SVC };
					unsigned long address = advsimd_mul_map_program(test, instruction);
					unsigned long *saved_simd;
					unsigned long *expected_simd;
					unsigned long accumulator[2], left[2], right[2], expected[2];
					struct pt_regs regs, expected_regs;
					struct orlix_tcti_result result;

					KUNIT_ASSERT_NE(test, 0UL, address);
					saved_simd = kunit_kcalloc(test,
						ARRAY_SIZE(current->thread.user_simd), sizeof(*saved_simd),
						GFP_KERNEL);
					expected_simd = kunit_kcalloc(test,
						ARRAY_SIZE(current->thread.user_simd), sizeof(*expected_simd),
						GFP_KERNEL);
					KUNIT_ASSERT_NOT_NULL(test, saved_simd);
					KUNIT_ASSERT_NOT_NULL(test, expected_simd);
					advsimd_mul_expect_code(test, address, program);
					memcpy(saved_simd, current->thread.user_simd,
					       sizeof(*saved_simd) * ARRAY_SIZE(current->thread.user_simd));
					advsimd_mul_seed_simd(instruction);
					advsimd_mul_fill_register(accumulator, lane_bytes, result_bytes, 1);
					advsimd_mul_fill_register(left, lane_bytes, result_bytes, 3);
					advsimd_mul_fill_register(right, lane_bytes, result_bytes, 5);
					current->thread.user_simd[arrangements[arrangement].rd * 2] = accumulator[0];
					current->thread.user_simd[arrangements[arrangement].rd * 2 + 1] = accumulator[1];
					current->thread.user_simd[arrangements[arrangement].rn * 2] = left[0];
					current->thread.user_simd[arrangements[arrangement].rn * 2 + 1] = left[1];
					current->thread.user_simd[arrangements[arrangement].rm * 2] = right[0];
					current->thread.user_simd[arrangements[arrangement].rm * 2 + 1] = right[1];
					memcpy(expected_simd, current->thread.user_simd,
					       sizeof(*expected_simd) * ARRAY_SIZE(current->thread.user_simd));
					advsimd_mul_expected(expected, leaf->arithmetic_op,
						&expected_simd[arrangements[arrangement].rd * 2],
						&expected_simd[arrangements[arrangement].rn * 2],
						&expected_simd[arrangements[arrangement].rm * 2],
						lane_bytes, result_bytes);
					expected_simd[arrangements[arrangement].rd * 2] = expected[0];
					expected_simd[arrangements[arrangement].rd * 2 + 1] =
						q ? expected[1] : 0;
					advsimd_mul_seed_regs(&regs, address);
					expected_regs = regs;
					expected_regs.pc += sizeof(u32);
					result = orlix_tcti_resume_user(current, &regs, current->mm);
					KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
					KUNIT_EXPECT_EQ(test, 0L, result.status);
					KUNIT_EXPECT_EQ(test, 0UL, result.fault_address);
					KUNIT_EXPECT_EQ(test, ORLIX_TCTI_ACCESS_FETCH, result.fault_access);
					KUNIT_EXPECT_EQ(test, address + sizeof(u32), result.pc);
					KUNIT_EXPECT_EQ(test, ADVSIMD_MUL_SVC, result.instruction);
					KUNIT_EXPECT_MEMEQ(test, &expected_regs, &regs, sizeof(regs));
					KUNIT_EXPECT_MEMEQ(test, expected_simd, current->thread.user_simd,
						sizeof(*expected_simd) * ARRAY_SIZE(current->thread.user_simd));
					KUNIT_EXPECT_EQ(test, 1UL, current->thread.user_simd_valid);
					KUNIT_EXPECT_EQ(test, BIT(22) | BIT(24), current->thread.user_fpcr);
					KUNIT_EXPECT_EQ(test, BIT(27) | BIT(4), current->thread.user_fpsr);
					advsimd_mul_expect_code(test, address, program);
					memcpy(current->thread.user_simd, saved_simd,
					       sizeof(*saved_simd) * ARRAY_SIZE(current->thread.user_simd));
					KUNIT_EXPECT_EQ(test, 0, vm_munmap(address, PAGE_SIZE));
				}
}

static void advsimd_mul_reserved_forms_reject_without_mutation(struct kunit *test)
{
	size_t leaf_index;
	u8 q;

	for (leaf_index = 0; leaf_index < ARRAY_SIZE(advsimd_mul_leaves); leaf_index++)
		for (q = 0; q < 2; q++) {
			const struct advsimd_mul_leaf *leaf = &advsimd_mul_leaves[leaf_index];
			const u32 instruction = advsimd_mul_instruction(leaf, q, 3, 31, 30, 29);
			const u32 program[] = { instruction, ADVSIMD_MUL_SVC };
			unsigned long address = advsimd_mul_map_program(test, instruction);
			unsigned long *before_simd;
			struct pt_regs regs, before_regs;
			struct orlix_tcti_result result;

			KUNIT_ASSERT_NE(test, 0UL, address);
			before_simd = kunit_kcalloc(test, ARRAY_SIZE(current->thread.user_simd),
						    sizeof(*before_simd), GFP_KERNEL);
			KUNIT_ASSERT_NOT_NULL(test, before_simd);
			KUNIT_ASSERT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
				orlix_tcti_decode_aarch64(instruction).decode_class);
			advsimd_mul_seed_simd(instruction);
			memcpy(before_simd, current->thread.user_simd,
			       sizeof(*before_simd) * ARRAY_SIZE(current->thread.user_simd));
			advsimd_mul_seed_regs(&regs, address);
			before_regs = regs;
			advsimd_mul_expect_code(test, address, program);
			result = orlix_tcti_resume_user(current, &regs, current->mm);
			KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION, result.reason);
			KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, result.status);
			KUNIT_EXPECT_EQ(test, 0UL, result.fault_address);
			KUNIT_EXPECT_EQ(test, ORLIX_TCTI_ACCESS_FETCH, result.fault_access);
			KUNIT_EXPECT_EQ(test, address, result.pc);
			KUNIT_EXPECT_EQ(test, instruction, result.instruction);
			KUNIT_EXPECT_MEMEQ(test, &before_regs, &regs, sizeof(regs));
			KUNIT_EXPECT_MEMEQ(test, before_simd, current->thread.user_simd,
				   sizeof(*before_simd) * ARRAY_SIZE(current->thread.user_simd));
			KUNIT_EXPECT_EQ(test, 0UL, current->thread.user_simd_valid);
			KUNIT_EXPECT_EQ(test, BIT(22) | BIT(24), current->thread.user_fpcr);
			KUNIT_EXPECT_EQ(test, BIT(27) | BIT(4), current->thread.user_fpsr);
			advsimd_mul_expect_code(test, address, program);
			KUNIT_EXPECT_EQ(test, 0, vm_munmap(address, PAGE_SIZE));
		}
}

static struct kunit_case advsimd_mul_source_bound_cases[] = {
	KUNIT_CASE(advsimd_mul_bind_canonical_artifacts),
	KUNIT_CASE(advsimd_mul_execute_all_aliases),
	KUNIT_CASE(advsimd_mul_reserved_forms_reject_without_mutation),
	{}
};

static struct kunit_suite advsimd_mul_source_bound_suite = {
	.name = "orlix-tcti-advsimd-mul-source-bound",
	.init = advsimd_mul_init,
	.exit = advsimd_mul_exit,
	.test_cases = advsimd_mul_source_bound_cases,
};

kunit_test_suite(advsimd_mul_source_bound_suite);

MODULE_LICENSE("GPL");
