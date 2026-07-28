// SPDX-License-Identifier: GPL-2.0-only
#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/orlix_tcti.h>
#include <kunit/test.h>
#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/syscalls.h>

#include "../decode_aarch64.h"
#include "../gadget_program.h"
#include "target_execution_slice_map.h"
#include "target_instruction_artifact.h"

#define ORLIX_TCTI_VARIABLE_SHIFT_SOURCE_MASK 0xffe0fc00U
#define ORLIX_TCTI_VARIABLE_SHIFT_SVC 0xd4000001U

struct orlix_tcti_variable_shift_leaf {
	u16 source_ordinal;
	const char *source_name;
	const char *source_mnemonic;
	const char *operation_id;
	u32 source_pattern;
	enum orlix_tcti_data_processing_2source_op operation;
	bool is_64bit;
};

/* Exact direct leaves from pinned Arm AARCHMRS 2026-06. */
static const struct orlix_tcti_variable_shift_leaf orlix_tcti_variable_shift_leaves[] = {
	{ 3358, "LSLV_32_dp_2src", "LSLV", "LSLV", 0x1ac02000U,
	  ORLIX_TCTI_DP2_LSLV, false },
	{ 3359, "LSRV_32_dp_2src", "LSRV", "LSRV", 0x1ac02400U,
	  ORLIX_TCTI_DP2_LSRV, false },
	{ 3360, "ASRV_32_dp_2src", "ASRV", "ASRV", 0x1ac02800U,
	  ORLIX_TCTI_DP2_ASRV, false },
	{ 3361, "RORV_32_dp_2src", "RORV", "RORV", 0x1ac02c00U,
	  ORLIX_TCTI_DP2_RORV, false },
	{ 3377, "LSLV_64_dp_2src", "LSLV", "LSLV", 0x9ac02000U,
	  ORLIX_TCTI_DP2_LSLV, true },
	{ 3378, "LSRV_64_dp_2src", "LSRV", "LSRV", 0x9ac02400U,
	  ORLIX_TCTI_DP2_LSRV, true },
	{ 3379, "ASRV_64_dp_2src", "ASRV", "ASRV", 0x9ac02800U,
	  ORLIX_TCTI_DP2_ASRV, true },
	{ 3380, "RORV_64_dp_2src", "RORV", "RORV", 0x9ac02c00U,
	  ORLIX_TCTI_DP2_RORV, true },
};

static const char *orlix_tcti_variable_shift_artifact_string(
	const struct orlix_tcti_target_instruction_artifact *artifact, u32 offset)
{
	if (offset >= artifact->string_pool_size)
		return NULL;

	return (const char *)artifact->string_pool + offset;
}

static u32 orlix_tcti_variable_shift_instruction(
	const struct orlix_tcti_variable_shift_leaf *leaf, u8 rd, u8 rn, u8 rm)
{
	return leaf->source_pattern | ((u32)rm << 16) | ((u32)rn << 5) | rd;
}

static const struct orlix_tcti_variable_shift_leaf *
orlix_tcti_variable_shift_source_leaf(u32 instruction)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_variable_shift_leaves); index++) {
		const struct orlix_tcti_variable_shift_leaf *leaf =
			&orlix_tcti_variable_shift_leaves[index];

		if ((instruction & ORLIX_TCTI_VARIABLE_SHIFT_SOURCE_MASK) ==
		    leaf->source_pattern)
			return leaf;
	}
	return NULL;
}

static u64 orlix_tcti_variable_shift_mask(bool is_64bit)
{
	return is_64bit ? U64_MAX : U32_MAX;
}

static u64 orlix_tcti_variable_shift_expected(
	const struct orlix_tcti_variable_shift_leaf *leaf, u64 value, u64 count)
{
	u8 width = leaf->is_64bit ? 64 : 32;
	u8 amount = count & (width - 1);
	u64 mask = orlix_tcti_variable_shift_mask(leaf->is_64bit);

	value &= mask;
	switch (leaf->operation) {
	case ORLIX_TCTI_DP2_LSLV:
		return (value << amount) & mask;
	case ORLIX_TCTI_DP2_LSRV:
		return value >> amount;
	case ORLIX_TCTI_DP2_ASRV:
		return leaf->is_64bit ? (u64)((s64)value >> amount) :
			(u32)((s32)(u32)value >> amount);
	case ORLIX_TCTI_DP2_RORV:
		return amount ? ((value >> amount) |
				 (value << (width - amount))) & mask : value;
	default:
		return 0;
	}
}

static unsigned long orlix_tcti_variable_shift_map_program(struct kunit *test,
						      u32 instruction)
{
	const u32 program[] = { instruction, ORLIX_TCTI_VARIABLE_SHIFT_SVC };
	unsigned long mapped;
	int ret;

	KUNIT_ASSERT_NOT_NULL(test, current->mm);
	mapped = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));
	ret = orlix_tcti_write_user_data(current->mm, mapped, program, sizeof(program));
	if (ret) {
		vm_munmap(mapped, PAGE_SIZE);
		KUNIT_FAIL(test, "could not write variable shift program: %d", ret);
		return 0;
	}
	ret = sys_mprotect(mapped, PAGE_SIZE, PROT_READ | PROT_EXEC);
	if (ret) {
		vm_munmap(mapped, PAGE_SIZE);
		KUNIT_FAIL(test, "could not protect variable shift program: %d", ret);
		return 0;
	}
	return mapped;
}

static void orlix_tcti_variable_shift_expect_decode(struct kunit *test,
	const struct orlix_tcti_variable_shift_leaf *leaf, u32 instruction, u8 rd, u8 rn,
	u8 rm)
{
	struct orlix_tcti_decoded_instruction decoded = orlix_tcti_decode_aarch64(instruction);

	KUNIT_ASSERT_EQ_MSG(test, ORLIX_TCTI_DECODE_DATA_PROCESSING_2SOURCE,
			    decoded.decode_class, "%s %#x", leaf->source_name,
			    instruction);
	KUNIT_EXPECT_EQ(test, leaf->operation, decoded.dp2_op);
	KUNIT_EXPECT_EQ(test, leaf->is_64bit, decoded.is_64bit);
	KUNIT_EXPECT_EQ(test, rd, decoded.rd);
	KUNIT_EXPECT_EQ(test, rn, decoded.rn);
	KUNIT_EXPECT_EQ(test, rm, decoded.rm);
}

static void orlix_tcti_variable_shift_execute(struct kunit *test,
	const struct orlix_tcti_variable_shift_leaf *leaf, u8 rd, u8 rn, u8 rm,
	u64 value, u64 count)
{
	u32 instruction = orlix_tcti_variable_shift_instruction(leaf, rd, rn, rm);
	unsigned long mapped = orlix_tcti_variable_shift_map_program(test, instruction);
	struct pt_regs regs = {};
	struct pt_regs before;
	struct pt_regs expected_regs;
	struct orlix_tcti_result result;
	const u64 gpr_x0 = 0x0123456789abcdefULL;
	const unsigned long orig_x0 = 0x13579bdf2468ace0UL;
	const u64 unused_gpr = 0xfedcba9876543210ULL;
	const u32 unused = 0x76543210U;
	u64 expected;
	u8 reg;

	KUNIT_ASSERT_NE(test, 0UL, mapped);
	for (reg = 0; reg < 31; reg++)
		regs.regs[reg] = 0x9e3779b97f4a7c15ULL ^
			((u64)instruction << (reg & 15)) ^ reg;
	if (rn != 31)
		regs.regs[rn] = value;
	if (rm != 31)
		regs.regs[rm] = count;
	regs.regs[0] = gpr_x0;
	regs.regs[29] = unused_gpr;
	regs.orig_x0 = orig_x0;
	regs.unused = unused;
	regs.sp = 0x00000001fffffff0ULL;
	regs.pc = mapped;
	regs.pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT |
		PSR_V_BIT | PSR_D_BIT;
	regs.syscallno = NO_SYSCALL;
	before = regs;
	expected = orlix_tcti_variable_shift_expected(leaf,
		rn == 31 ? 0 : before.regs[rn], rm == 31 ? 0 : before.regs[rm]);
	expected_regs = before;
	if (rd != 31)
		expected_regs.regs[rd] = expected;
	expected_regs.pc += sizeof(u32);

	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_ASSERT_EQ_MSG(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason,
			    "%s %#x", leaf->source_name, instruction);
	KUNIT_EXPECT_EQ(test, 0L, result.status);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_VARIABLE_SHIFT_SVC, result.instruction);
	KUNIT_EXPECT_EQ(test, mapped + sizeof(u32), result.pc);
	KUNIT_EXPECT_EQ(test, NO_SYSCALL, before.syscallno);
	KUNIT_EXPECT_EQ(test, NO_SYSCALL, regs.syscallno);
	KUNIT_EXPECT_EQ(test, gpr_x0, before.regs[0]);
	KUNIT_EXPECT_EQ(test, orig_x0, before.orig_x0);
	KUNIT_EXPECT_EQ(test, unused_gpr, before.regs[29]);
	KUNIT_EXPECT_EQ(test, unused, before.unused);
	KUNIT_EXPECT_MEMEQ(test, &expected_regs, &regs, sizeof(regs));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void orlix_tcti_variable_shift_source_bindings(struct kunit *test)
{
	const struct orlix_tcti_target_instruction_artifact *artifact =
		orlix_tcti_target_instruction_artifact_canonical();
	const struct orlix_tcti_execution_slice_map *slice_map =
		orlix_tcti_execution_slice_map_canonical();
	struct orlix_tcti_target_instruction_artifact_validation_result artifact_result;
	struct orlix_tcti_execution_slice_map_validation_result slice_result;
	static const u16 expected_ordinals[] = {
		3358, 3359, 3360, 3361, 3377, 3378, 3379, 3380,
	};
	u32 family_index = U32_MAX;
	size_t index;

	KUNIT_ASSERT_NOT_NULL(test, artifact);
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_target_instruction_artifact_validate(artifact,
			&artifact_result));
	KUNIT_ASSERT_NOT_NULL(test, slice_map);
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_execution_slice_map_validate(slice_map, &slice_result));
	for (index = 0; index < slice_map->counts.family_count; index++)
		if (!strcmp(slice_map->families[index].stable_id,
			    "base-residual-variable-shift")) {
			family_index = index;
			break;
		}
	KUNIT_ASSERT_NE(test, U32_MAX, family_index);
	KUNIT_EXPECT_EQ(test, 183U, slice_map->families[family_index].issue_id);
	KUNIT_EXPECT_EQ(test, 8U,
			slice_map->families[family_index].declared_member_count);
	KUNIT_ASSERT_EQ(test, 8U, ARRAY_SIZE(orlix_tcti_variable_shift_leaves));
	for (index = 0; index < ARRAY_SIZE(orlix_tcti_variable_shift_leaves); index++) {
		const struct orlix_tcti_variable_shift_leaf *leaf =
			&orlix_tcti_variable_shift_leaves[index];
		const struct orlix_tcti_target_instruction_artifact_leaf *source;
		const struct orlix_tcti_execution_slice_member *slice_member;
		u32 instruction = orlix_tcti_variable_shift_instruction(leaf, 7, 19, 11);

		KUNIT_EXPECT_TRUE(test, leaf->source_name[0]);
		KUNIT_EXPECT_EQ(test, expected_ordinals[index], leaf->source_ordinal);
		KUNIT_ASSERT_LT(test, (size_t)leaf->source_ordinal,
				artifact->leaf_count);
		KUNIT_ASSERT_LT(test, (size_t)leaf->source_ordinal,
				slice_map->counts.leaf_count);
		source = &artifact->leaves[leaf->source_ordinal];
		slice_member = &slice_map->members[leaf->source_ordinal];
		KUNIT_EXPECT_STREQ(test, leaf->source_name,
			orlix_tcti_variable_shift_artifact_string(artifact,
				source->name_offset));
		KUNIT_EXPECT_STREQ(test, leaf->source_mnemonic,
			orlix_tcti_variable_shift_artifact_string(artifact,
				source->mnemonic_offset));
		KUNIT_EXPECT_STREQ(test, leaf->operation_id,
			orlix_tcti_variable_shift_artifact_string(artifact,
				source->operation_offset));
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_VARIABLE_SHIFT_SOURCE_MASK,
				source->encoding_mask);
		KUNIT_EXPECT_EQ(test, leaf->source_pattern,
				source->encoding_pattern);
		KUNIT_EXPECT_EQ(test, leaf->source_ordinal, slice_member->ordinal);
		KUNIT_EXPECT_STREQ(test, leaf->source_name,
				slice_member->source_name);
		KUNIT_EXPECT_EQ(test, family_index, slice_member->family_index);
		KUNIT_EXPECT_EQ(test, leaf->source_pattern,
				leaf->source_pattern & ORLIX_TCTI_VARIABLE_SHIFT_SOURCE_MASK);
		KUNIT_EXPECT_PTR_EQ(test, leaf,
			orlix_tcti_variable_shift_source_leaf(instruction));
		orlix_tcti_variable_shift_expect_decode(test, leaf, instruction, 7, 19,
						  11);
	}
}

static void orlix_tcti_variable_shift_fixed_gadget_lowering(struct kunit *test)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_variable_shift_leaves); index++) {
		const struct orlix_tcti_variable_shift_leaf *leaf =
			&orlix_tcti_variable_shift_leaves[index];
		struct orlix_tcti_decoded_instruction decoded =
			orlix_tcti_decode_aarch64(
				orlix_tcti_variable_shift_instruction(leaf, 7, 19, 11));
		struct orlix_tcti_gadget_word
			program[ORLIX_TCTI_SINGLE_INSTRUCTION_PROGRAM_WORDS] = {};
		size_t word_count = 0;

		KUNIT_ASSERT_EQ(test, 0, orlix_tcti_lower_decoded_instruction(
			&decoded, program, ARRAY_SIZE(program), &word_count));
		KUNIT_EXPECT_EQ(test,
			(size_t)ORLIX_TCTI_SINGLE_INSTRUCTION_PROGRAM_WORDS,
			word_count);
		KUNIT_EXPECT_TRUE_MSG(test,
			orlix_tcti_gadget_program_uses_variable_shift_gadget(
				program, word_count), "%s", leaf->source_name);
	}
}

static void orlix_tcti_variable_shift_all_legal_register_encodings(struct kunit *test)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_variable_shift_leaves); index++) {
		const struct orlix_tcti_variable_shift_leaf *leaf =
			&orlix_tcti_variable_shift_leaves[index];
		u8 rn;

		for (rn = 0; rn < 32; rn++) {
			u8 rm;

			for (rm = 0; rm < 32; rm++) {
				u8 rd;

				for (rd = 0; rd < 32; rd++)
					orlix_tcti_variable_shift_expect_decode(test, leaf,
							orlix_tcti_variable_shift_instruction(leaf, rd,
								rn, rm), rd, rn, rm);
			}
		}
	}
}

static void orlix_tcti_variable_shift_production_path_semantics(struct kunit *test)
{
	static const u64 counts[] = { 0, 1, 31, 32, 33, 63, 64, 65, U64_MAX };
	static const u64 values[] = {
		0, U64_MAX, 0x8123456789abcdefULL, 0x7fffffffffffffffULL,
		0x8000000000000000ULL,
	};
	size_t leaf_index;
	size_t value_index;
	size_t count_index;

	for (leaf_index = 0; leaf_index < ARRAY_SIZE(orlix_tcti_variable_shift_leaves);
	     leaf_index++)
		for (value_index = 0; value_index < ARRAY_SIZE(values); value_index++)
			for (count_index = 0; count_index < ARRAY_SIZE(counts);
			     count_index++)
				orlix_tcti_variable_shift_execute(test,
					&orlix_tcti_variable_shift_leaves[leaf_index], 7, 19,
					11, values[value_index], counts[count_index]);
}

static void orlix_tcti_variable_shift_xzr_source_and_destination(struct kunit *test)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_variable_shift_leaves); index++) {
		const struct orlix_tcti_variable_shift_leaf *leaf =
			&orlix_tcti_variable_shift_leaves[index];

		orlix_tcti_variable_shift_execute(test, leaf, 7, 31, 11,
					0x0123456789abcdefULL, 63);
		orlix_tcti_variable_shift_execute(test, leaf, 7, 19, 31,
					0x0123456789abcdefULL, 63);
		orlix_tcti_variable_shift_execute(test, leaf, 31, 19, 11,
					0x0123456789abcdefULL, 63);
	}
}

static void orlix_tcti_variable_shift_destination_source_aliases(struct kunit *test)
{
	/* Exercise rd == rn and rd == rm at both architectural data widths. */
	orlix_tcti_variable_shift_execute(test, &orlix_tcti_variable_shift_leaves[0], 19, 19,
					11, 0x81234567U, 31);
	orlix_tcti_variable_shift_execute(test, &orlix_tcti_variable_shift_leaves[3], 11, 19,
					11, 0x81234567U, 31);
	orlix_tcti_variable_shift_execute(test, &orlix_tcti_variable_shift_leaves[4], 19, 19,
					11, 0x8123456789abcdefULL, 63);
	orlix_tcti_variable_shift_execute(test, &orlix_tcti_variable_shift_leaves[7], 11, 19,
					11, 0x8123456789abcdefULL, 63);
}

static void orlix_tcti_variable_shift_reserved_structured_exits(struct kunit *test)
{
	static const u8 reserved_opcodes[] = { 0x04, 0x07, 0x0c, 0x0f };
	bool is_64bit;
	size_t index;

	for (is_64bit = false; ; is_64bit = true) {
		for (index = 0; index < ARRAY_SIZE(reserved_opcodes); index++) {
			u32 instruction = (is_64bit ? 0x9ac00000U : 0x1ac00000U) |
				((u32)reserved_opcodes[index] << 10) |
				(11U << 16) | (19U << 5) | 7U;
			struct orlix_tcti_decoded_instruction decoded =
				orlix_tcti_decode_aarch64(instruction);
			unsigned long mapped =
				orlix_tcti_variable_shift_map_program(test, instruction);
			struct pt_regs regs = {};
			struct pt_regs before;
			struct orlix_tcti_result result;
			u8 reg;

			KUNIT_ASSERT_EQ_MSG(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
				decoded.decode_class, "%#x", instruction);
			KUNIT_ASSERT_NE(test, 0UL, mapped);
			for (reg = 0; reg < 31; reg++)
				regs.regs[reg] = 0x517cc1b727220a95ULL ^
					((u64)instruction << (reg & 7)) ^ reg;
			regs.sp = 0x00000001fffffff0ULL;
			regs.pc = mapped;
			regs.pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_C_BIT |
				PSR_D_BIT;
			regs.syscallno = NO_SYSCALL;
			before = regs;

			result = orlix_tcti_resume_user(current, &regs, current->mm);
			KUNIT_EXPECT_EQ_MSG(test,
				ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
				result.reason, "%#x", instruction);
			KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, result.status);
			KUNIT_EXPECT_EQ(test, instruction, result.instruction);
			KUNIT_EXPECT_EQ(test, mapped, result.pc);
			KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
			KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
		}
		if (is_64bit)
			break;
	}
}

static struct kunit_case orlix_tcti_variable_shift_source_bound_test_cases[] = {
	KUNIT_CASE(orlix_tcti_variable_shift_source_bindings),
	KUNIT_CASE(orlix_tcti_variable_shift_fixed_gadget_lowering),
	KUNIT_CASE(orlix_tcti_variable_shift_all_legal_register_encodings),
	KUNIT_CASE(orlix_tcti_variable_shift_production_path_semantics),
	KUNIT_CASE(orlix_tcti_variable_shift_xzr_source_and_destination),
	KUNIT_CASE(orlix_tcti_variable_shift_destination_source_aliases),
	KUNIT_CASE(orlix_tcti_variable_shift_reserved_structured_exits),
	{}
};

struct kunit_suite orlix_tcti_variable_shift_source_bound_test_suite = {
	.name = "orlix-tcti-variable-shift-source-bound",
	.test_cases = orlix_tcti_variable_shift_source_bound_test_cases,
};

kunit_test_suite(orlix_tcti_variable_shift_source_bound_test_suite);
