// SPDX-License-Identifier: GPL-2.0-only
/*
 * Production-path proof for the six AARCHMRS AdvSIMD permutation leaves.
 * The byte-lane oracle below is independent from the word/lane executor.
 */
#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/orlix_tcti.h>
#include <kunit/test.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/syscalls.h>

#include "../decode_aarch64.h"
#include "target_instruction_artifact.h"

#define ORLIX_TCTI_ADVSIMD_PERMUTE_SVC 0xd4000001U

struct orlix_tcti_advsimd_permute_leaf {
	u16 ordinal;
	const char *name;
	const char *mnemonic;
	const char *operation;
	u32 mask;
	u32 pattern;
	enum orlix_tcti_simd_element_move_op decoded_operation;
};

static const struct orlix_tcti_advsimd_permute_leaf orlix_tcti_advsimd_permute_leaves[] = {
	{ 3684U, "UZP1_asimdperm_only", "UZP1", "UZP1_advsimd",
	  0xbf20fc00U, 0x0e001800U, ORLIX_TCTI_SIMD_ELEMENT_MOVE_UZP1 },
	{ 3685U, "TRN1_asimdperm_only", "TRN1", "TRN1_advsimd",
	  0xbf20fc00U, 0x0e002800U, ORLIX_TCTI_SIMD_ELEMENT_MOVE_TRN1 },
	{ 3686U, "ZIP1_asimdperm_only", "ZIP1", "ZIP1_advsimd",
	  0xbf20fc00U, 0x0e003800U, ORLIX_TCTI_SIMD_ELEMENT_MOVE_ZIP1 },
	{ 3687U, "UZP2_asimdperm_only", "UZP2", "UZP2_advsimd",
	  0xbf20fc00U, 0x0e005800U, ORLIX_TCTI_SIMD_ELEMENT_MOVE_UZP2 },
	{ 3688U, "TRN2_asimdperm_only", "TRN2", "TRN2_advsimd",
	  0xbf20fc00U, 0x0e006800U, ORLIX_TCTI_SIMD_ELEMENT_MOVE_TRN2 },
	{ 3689U, "ZIP2_asimdperm_only", "ZIP2", "ZIP2_advsimd",
	  0xbf20fc00U, 0x0e007800U, ORLIX_TCTI_SIMD_ELEMENT_MOVE_ZIP2 },
};

struct orlix_tcti_advsimd_permute_context {
	u64 simd[ARRAY_SIZE(current->thread.user_simd)];
	unsigned long simd_valid;
	unsigned long fpcr;
	unsigned long fpsr;
};

static int orlix_tcti_advsimd_permute_test_init(struct kunit *test)
{
	struct orlix_tcti_advsimd_permute_context *context;

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

static void orlix_tcti_advsimd_permute_test_exit(struct kunit *test)
{
	struct orlix_tcti_advsimd_permute_context *context = test->priv;

	memcpy(current->thread.user_simd, context->simd, sizeof(context->simd));
	current->thread.user_simd_valid = context->simd_valid;
	current->thread.user_fpcr = context->fpcr;
	current->thread.user_fpsr = context->fpsr;
}

static const char *orlix_tcti_advsimd_permute_artifact_string(
	const struct orlix_tcti_target_instruction_artifact *artifact, u32 offset)
{
	if (offset >= artifact->string_pool_size)
		return NULL;
	return (const char *)artifact->string_pool + offset;
}

static u32 orlix_tcti_advsimd_permute_instruction(const struct orlix_tcti_advsimd_permute_leaf *leaf, u8 q, u8 size,
			       u8 rd, u8 rn, u8 rm)
{
	return leaf->pattern | ((u32)q << 30) | ((u32)size << 22) |
		((u32)rm << 16) | ((u32)rn << 5) | rd;
}

static unsigned long orlix_tcti_advsimd_permute_map_instruction(struct kunit *test, u32 instruction)
{
	const u32 program[] = { instruction, ORLIX_TCTI_ADVSIMD_PERMUTE_SVC };
	unsigned long mapped;
	int ret;

	mapped = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));
	ret = orlix_tcti_write_user_data(current->mm, mapped, program, sizeof(program));
	KUNIT_ASSERT_EQ(test, 0, ret);
	ret = sys_mprotect(mapped, PAGE_SIZE, PROT_READ | PROT_EXEC);
	KUNIT_ASSERT_EQ(test, 0, ret);
	return mapped;
}

static void orlix_tcti_advsimd_permute_seed_regs(struct pt_regs *regs, unsigned long pc,
			      u32 instruction)
{
	u8 reg;

	memset(regs, 0x5a, sizeof(*regs));
	for (reg = 0; reg < ARRAY_SIZE(regs->regs); reg++)
		regs->regs[reg] = 0x9e3779b97f4a7c15ULL ^
			((u64)instruction << (reg & 15U)) ^ reg;
	regs->sp = STACK_TOP - 16U;
	regs->pc = pc;
	regs->pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT |
		PSR_V_BIT | PSR_D_BIT;
	regs->orig_x0 = 0x13579bdf2468ace0ULL;
	regs->syscallno = NO_SYSCALL;
	regs->unused = 0x76543210U;
}

static u8 orlix_tcti_advsimd_permute_get_byte(const u64 simd[], u8 vector, u8 byte)
{
	return (simd[vector * 2 + byte / sizeof(u64)] >>
		((byte % sizeof(u64)) * 8U)) & 0xffU;
}

static void orlix_tcti_advsimd_permute_set_byte(u64 simd[], u8 vector, u8 byte, u8 value)
{
	u64 *word = &simd[vector * 2 + byte / sizeof(u64)];
	u8 shift = (byte % sizeof(u64)) * 8U;

	*word &= ~(0xffULL << shift);
	*word |= (u64)value << shift;
}

static void orlix_tcti_advsimd_permute_oracle(const struct orlix_tcti_advsimd_permute_leaf *leaf, u8 q, u8 size,
			   u8 rd, u8 rn, u8 rm, const u64 before[], u64 expected[])
{
	u8 lane_bytes = BIT(size);
	u8 vector_bytes = q ? 16U : 8U;
	u8 lane_count = vector_bytes / lane_bytes;
	u8 half = lane_count / 2U;
	u8 destination_lane;

	memcpy(expected, before, sizeof(u64) * ARRAY_SIZE(current->thread.user_simd));
	expected[rd * 2] = 0;
	expected[rd * 2 + 1] = 0;
	for (destination_lane = 0; destination_lane < lane_count;
	     destination_lane++) {
		u8 source_vector;
		u8 source_lane;
		u8 source_byte;
		u8 byte;

		switch (leaf->decoded_operation) {
		case ORLIX_TCTI_SIMD_ELEMENT_MOVE_UZP1:
		case ORLIX_TCTI_SIMD_ELEMENT_MOVE_UZP2:
			source_vector = destination_lane >= half ? rm : rn;
			source_lane = (destination_lane % half) * 2U +
				(leaf->decoded_operation == ORLIX_TCTI_SIMD_ELEMENT_MOVE_UZP2);
			break;
		case ORLIX_TCTI_SIMD_ELEMENT_MOVE_TRN1:
		case ORLIX_TCTI_SIMD_ELEMENT_MOVE_TRN2:
			source_vector = destination_lane & 1U ? rm : rn;
			source_lane = (destination_lane / 2U) * 2U +
				(leaf->decoded_operation == ORLIX_TCTI_SIMD_ELEMENT_MOVE_TRN2);
			break;
		case ORLIX_TCTI_SIMD_ELEMENT_MOVE_ZIP1:
		case ORLIX_TCTI_SIMD_ELEMENT_MOVE_ZIP2:
			source_vector = destination_lane & 1U ? rm : rn;
			source_lane = destination_lane / 2U +
				(leaf->decoded_operation == ORLIX_TCTI_SIMD_ELEMENT_MOVE_ZIP2 ?
				 half : 0U);
			break;
		default:
			return;
		}
		for (byte = 0; byte < lane_bytes; byte++) {
			source_byte = source_lane * lane_bytes + byte;
			orlix_tcti_advsimd_permute_set_byte(expected, rd,
				destination_lane * lane_bytes + byte,
				orlix_tcti_advsimd_permute_get_byte(before, source_vector, source_byte));
		}
	}
}

static void orlix_tcti_advsimd_permute_source_binding_and_decode_domains(struct kunit *test)
{
	const struct orlix_tcti_target_instruction_artifact *artifact =
		orlix_tcti_target_instruction_artifact_canonical();
	struct orlix_tcti_target_instruction_artifact_validation_result validation;
	size_t leaf_index;

	KUNIT_ASSERT_NOT_NULL(test, artifact);
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_target_instruction_artifact_validate(artifact, &validation));
	KUNIT_ASSERT_GT(test, artifact->leaf_count, 3689U);
	for (leaf_index = 0; leaf_index < ARRAY_SIZE(orlix_tcti_advsimd_permute_leaves);
	     leaf_index++) {
		const struct orlix_tcti_advsimd_permute_leaf *leaf = &orlix_tcti_advsimd_permute_leaves[leaf_index];
		const struct orlix_tcti_target_instruction_artifact_leaf *source =
			&artifact->leaves[leaf->ordinal];
		const char *name = orlix_tcti_advsimd_permute_artifact_string(artifact,
			source->name_offset);
		const char *mnemonic = orlix_tcti_advsimd_permute_artifact_string(artifact,
			source->mnemonic_offset);
		const char *operation = orlix_tcti_advsimd_permute_artifact_string(artifact,
			source->operation_offset);
		u8 q;
		u8 size;
		u8 reg;

		KUNIT_ASSERT_NOT_NULL(test, name);
		KUNIT_ASSERT_NOT_NULL(test, mnemonic);
		KUNIT_ASSERT_NOT_NULL(test, operation);
		KUNIT_EXPECT_STREQ(test, leaf->name, name);
		KUNIT_EXPECT_STREQ(test, leaf->mnemonic, mnemonic);
		KUNIT_EXPECT_STREQ(test, leaf->operation, operation);
		KUNIT_EXPECT_EQ(test, leaf->mask, source->encoding_mask);
		KUNIT_EXPECT_EQ(test, leaf->pattern, source->encoding_pattern);
		KUNIT_EXPECT_GT(test, source->condition_length, 0U);
		for (q = 0; q < 2; q++) {
			for (size = 0; size < 4; size++) {
				struct orlix_tcti_decoded_instruction decoded;
				u32 instruction = orlix_tcti_advsimd_permute_instruction(leaf, q, size,
					4U, 5U, 6U);

				decoded = orlix_tcti_decode_aarch64(instruction);
				KUNIT_EXPECT_EQ(test, leaf->pattern,
					instruction & leaf->mask);
				if (!q && size == 3U) {
					KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
						decoded.decode_class);
					continue;
				}
				KUNIT_EXPECT_EQ(test,
					ORLIX_TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE,
					decoded.decode_class);
				KUNIT_EXPECT_EQ(test, leaf->decoded_operation,
					decoded.simd_element_move_op);
				KUNIT_EXPECT_EQ(test, BIT(size), decoded.access_size);
				KUNIT_EXPECT_EQ(test, q ? 16U : 8U,
					decoded.result_size);
				KUNIT_EXPECT_TRUE(test, decoded.simd_fp);
			}
		}
		for (reg = 0; reg < 32; reg++) {
			struct orlix_tcti_decoded_instruction decoded;

			decoded = orlix_tcti_decode_aarch64(orlix_tcti_advsimd_permute_instruction(leaf, 1U, 3U,
				reg, 1U, 2U));
			KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE,
				decoded.decode_class);
			KUNIT_EXPECT_EQ(test, reg, decoded.rd);
			decoded = orlix_tcti_decode_aarch64(orlix_tcti_advsimd_permute_instruction(leaf, 1U, 3U,
				0U, reg, 2U));
			KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE,
				decoded.decode_class);
			KUNIT_EXPECT_EQ(test, reg, decoded.rn);
			decoded = orlix_tcti_decode_aarch64(orlix_tcti_advsimd_permute_instruction(leaf, 1U, 3U,
				0U, 1U, reg));
			KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE,
				decoded.decode_class);
			KUNIT_EXPECT_EQ(test, reg, decoded.rm);
		}
	}
}

static void orlix_tcti_advsimd_permute_reserved_size_rejects_without_state_change(struct kunit *test)
{
	size_t leaf_index;

	for (leaf_index = 0; leaf_index < ARRAY_SIZE(orlix_tcti_advsimd_permute_leaves);
	     leaf_index++) {
		const struct orlix_tcti_advsimd_permute_leaf *leaf = &orlix_tcti_advsimd_permute_leaves[leaf_index];
		u8 rd;
		u8 rn;
		u8 rm;

		for (rd = 0; rd < 32; rd++) {
			rn = (rd * 7U + 3U) & 31U;
			rm = (rd * 13U + 5U) & 31U;
			{
				u32 instruction = orlix_tcti_advsimd_permute_instruction(leaf, 0U, 3U,
					rd, rn, rm);
				const u32 expected_code[] = { instruction, ORLIX_TCTI_ADVSIMD_PERMUTE_SVC };
				u32 observed_code[ARRAY_SIZE(expected_code)] = {};
				unsigned long mapped = orlix_tcti_advsimd_permute_map_instruction(test,
					instruction);
				u64 before_simd[ARRAY_SIZE(current->thread.user_simd)];
				struct pt_regs regs;
				struct pt_regs before_regs;
				struct orlix_tcti_result result;
				u8 index;

				for (index = 0; index < ARRAY_SIZE(before_simd); index++)
					current->thread.user_simd[index] =
						0xd6e8feb86659fd93ULL ^ index;
				memcpy(before_simd, current->thread.user_simd,
					sizeof(before_simd));
				current->thread.user_simd_valid = 1;
				current->thread.user_fpcr = BIT(22) | BIT(24);
				current->thread.user_fpsr = BIT(27) | BIT(4);
				orlix_tcti_advsimd_permute_seed_regs(&regs, mapped, instruction);
				before_regs = regs;
				result = orlix_tcti_resume_user(current, &regs, current->mm);

				KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
					result.reason);
				KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, result.status);
				KUNIT_EXPECT_EQ(test, 0UL, result.fault_address);
				KUNIT_EXPECT_EQ(test, ORLIX_TCTI_ACCESS_FETCH,
					result.fault_access);
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
				KUNIT_EXPECT_EQ(test, 0, orlix_tcti_read_user_data(current->mm,
					mapped, observed_code, sizeof(observed_code)));
				KUNIT_EXPECT_MEMEQ(test, expected_code, observed_code,
					sizeof(expected_code));
				KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
			}
		}
	}
}

static void orlix_tcti_advsimd_permute_source_leaves_execute_all_arrangements(struct kunit *test)
{
	static const struct {
		u8 rd;
		u8 rn;
		u8 rm;
	} arrangements[] = {
		{ 4U, 5U, 6U },
		{ 5U, 5U, 6U },
		{ 6U, 5U, 6U },
		{ 4U, 5U, 5U },
		{ 5U, 5U, 5U },
		{ 31U, 31U, 0U },
		{ 31U, 0U, 31U },
	};
	size_t leaf_index;

	for (leaf_index = 0; leaf_index < ARRAY_SIZE(orlix_tcti_advsimd_permute_leaves);
	     leaf_index++) {
		const struct orlix_tcti_advsimd_permute_leaf *leaf = &orlix_tcti_advsimd_permute_leaves[leaf_index];
		u8 q;

		for (q = 0; q < 2; q++) {
			u8 size;

			for (size = 0; size < 4; size++) {
				u8 arrangement_index;

				if (!q && size == 3U)
					continue;
				for (arrangement_index = 0;
				     arrangement_index < ARRAY_SIZE(arrangements);
				     arrangement_index++) {
					u8 rd = arrangements[arrangement_index].rd;
					u8 rn = arrangements[arrangement_index].rn;
					u8 rm = arrangements[arrangement_index].rm;
					u32 instruction = orlix_tcti_advsimd_permute_instruction(leaf, q, size,
						rd, rn, rm);
					const u32 expected_code[] = { instruction, ORLIX_TCTI_ADVSIMD_PERMUTE_SVC };
					u32 observed_code[ARRAY_SIZE(expected_code)] = {};
					unsigned long mapped = orlix_tcti_advsimd_permute_map_instruction(test,
						instruction);
	u64 before_simd[ARRAY_SIZE(current->thread.user_simd)];
	u64 expected_simd[ARRAY_SIZE(current->thread.user_simd)];
					struct pt_regs regs;
					struct pt_regs expected_regs;
					struct orlix_tcti_result result;
					u8 index;

					for (index = 0; index < ARRAY_SIZE(before_simd); index++)
					before_simd[index] =
							0x6a09e667f3bcc909ULL ^
							((u64)instruction << (index & 7U)) ^ index;
					for (index = 0; index < 16; index++) {
						orlix_tcti_advsimd_permute_set_byte(before_simd, rn,
							index, 0x10U + index);
						orlix_tcti_advsimd_permute_set_byte(before_simd, rm,
							index, 0x90U + index);
					}
					memcpy(current->thread.user_simd, before_simd,
						sizeof(before_simd));
					orlix_tcti_advsimd_permute_oracle(leaf, q, size, rd, rn, rm, before_simd,
						expected_simd);
					current->thread.user_simd_valid = 0;
					current->thread.user_fpcr = BIT(22) | BIT(24);
					current->thread.user_fpsr = BIT(27) | BIT(4);
					orlix_tcti_advsimd_permute_seed_regs(&regs, mapped, instruction);
					expected_regs = regs;
					expected_regs.pc += sizeof(u32);
					result = orlix_tcti_resume_user(current, &regs, current->mm);

					KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
					KUNIT_EXPECT_EQ(test, 0L, result.status);
					KUNIT_EXPECT_EQ(test, 0UL, result.fault_address);
					KUNIT_EXPECT_EQ(test, ORLIX_TCTI_ACCESS_FETCH,
						result.fault_access);
					KUNIT_EXPECT_EQ(test, mapped + sizeof(u32), result.pc);
					KUNIT_EXPECT_EQ(test, ORLIX_TCTI_ADVSIMD_PERMUTE_SVC, result.instruction);
					KUNIT_EXPECT_MEMEQ(test, &expected_regs, &regs,
						sizeof(regs));
					KUNIT_EXPECT_MEMEQ(test, expected_simd,
						current->thread.user_simd,
						sizeof(expected_simd));
					KUNIT_EXPECT_EQ(test, 1UL,
						current->thread.user_simd_valid);
					KUNIT_EXPECT_EQ(test, BIT(22) | BIT(24),
						current->thread.user_fpcr);
					KUNIT_EXPECT_EQ(test, BIT(27) | BIT(4),
						current->thread.user_fpsr);
					KUNIT_EXPECT_EQ(test, 0, orlix_tcti_read_user_data(current->mm,
						mapped, observed_code, sizeof(observed_code)));
					KUNIT_EXPECT_MEMEQ(test, expected_code, observed_code,
						sizeof(expected_code));
					KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
				}
			}
		}
	}
}

static struct kunit_case orlix_tcti_advsimd_permute_cases[] = {
	KUNIT_CASE(orlix_tcti_advsimd_permute_source_binding_and_decode_domains),
	KUNIT_CASE(orlix_tcti_advsimd_permute_reserved_size_rejects_without_state_change),
	KUNIT_CASE(orlix_tcti_advsimd_permute_source_leaves_execute_all_arrangements),
	{}
};

static struct kunit_suite orlix_tcti_advsimd_permute_suite = {
	.name = "orlix-tcti-advsimd-permute-source-bound",
	.init = orlix_tcti_advsimd_permute_test_init,
	.exit = orlix_tcti_advsimd_permute_test_exit,
	.test_cases = orlix_tcti_advsimd_permute_cases,
};

kunit_test_suite(orlix_tcti_advsimd_permute_suite);

MODULE_LICENSE("GPL");
