// SPDX-License-Identifier: GPL-2.0-only
#include <asm/ptrace.h>
#include <kunit/test.h>
#include <linux/bitops.h>
#include <linux/errno.h>
#include <linux/string.h>

#include "../decode_aarch64.h"
#include "../gadget_program.h"
#include "target_execution_slice_map.h"
#include "target_instruction_artifact.h"
#include "target_proof_registry.h"

#define FLAG_NZCV (PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT | PSR_V_BIT)
#define ORLIX_TCTI_FLAG_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/orlix_tcti_flag_manipulation_source_bound_test.c"
#define ORLIX_TCTI_FLAG_SEMANTIC_PROVENANCE_MANIFEST \
	"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/isa/generations/current/manifest"
#define ORLIX_TCTI_FLAG_SEMANTIC_PROVENANCE_MANIFEST_SHA256 \
	"3ac25a4747de516f7a787b37b3661f3a08fbf1bf5cc43f800f8ee7791ab5c6ef"

enum orlix_tcti_flag_linux_visibility {
	ORLIX_TCTI_FLAG_LINUX_VISIBILITY_TYPED_NA_ARCHITECTURAL_PSTATE = 0,
};

struct orlix_tcti_flag_leaf {
	u16 ordinal;
	const char *mnemonic;
	const char *operation;
	u32 mask;
	u32 pattern;
	enum orlix_tcti_flag_manipulation_op op;
	enum orlix_tcti_flag_linux_visibility linux_visibility;
};

struct orlix_tcti_flag_semantic_provenance_row {
	u32 ordinal;
	const char *name;
	const char *relative_file;
	const char *decode_locator;
	const char *decode_sha256;
	const char *execute_locator;
	const char *execute_sha256;
};

#define ORLIX_TCTI_A64_SEMANTIC_PROVENANCE_SOURCE(identity_value) \
	static const char *const orlix_tcti_flag_semantic_source_identity = identity_value;

#define ORLIX_TCTI_A64_DDI0602_PROVENANCE_ROW(...)
#define ORLIX_TCTI_A64_OFFICIAL_SEMANTICS_NOT_SPECIFIED_ROW(...)
#include "../isa/generations/current/target_asl_availability.def"
#undef ORLIX_TCTI_A64_OFFICIAL_SEMANTICS_NOT_SPECIFIED_ROW
#undef ORLIX_TCTI_A64_DDI0602_PROVENANCE_ROW
#undef ORLIX_TCTI_A64_SEMANTIC_PROVENANCE_SOURCE

#define ORLIX_TCTI_A64_SEMANTIC_PROVENANCE_SOURCE(...)
#define ORLIX_TCTI_A64_DDI0602_PROVENANCE_ROW(ordinal_value, leaf_name_value, \
		relative_file_value, decode_locator_value, decode_digest_value, \
		decode_sections_value, decode_helpers_value, decode_closure_value, \
		execute_locator_value, execute_digest_value, execute_sections_value, \
		execute_helpers_value, execute_closure_value) \
	{ ordinal_value, leaf_name_value, relative_file_value, decode_locator_value, \
	  decode_digest_value, execute_locator_value, execute_digest_value },
#define ORLIX_TCTI_A64_OFFICIAL_SEMANTICS_NOT_SPECIFIED_ROW(...)
static const struct orlix_tcti_flag_semantic_provenance_row
	orlix_tcti_flag_semantic_provenance_rows[] = {
#include "../isa/generations/current/target_asl_availability.def"
};
#undef ORLIX_TCTI_A64_OFFICIAL_SEMANTICS_NOT_SPECIFIED_ROW
#undef ORLIX_TCTI_A64_DDI0602_PROVENANCE_ROW
#undef ORLIX_TCTI_A64_SEMANTIC_PROVENANCE_SOURCE

static const struct orlix_tcti_flag_leaf orlix_tcti_flag_leaves[] = {
	{3476, "RMIF", "RMIF", 0xffe07c10U, 0xba000400U,
	 ORLIX_TCTI_FLAG_MANIPULATION_RMIF,
	 ORLIX_TCTI_FLAG_LINUX_VISIBILITY_TYPED_NA_ARCHITECTURAL_PSTATE},
	{3477, "SETF8", "SETF", 0xfffffc1fU, 0x3a00080dU,
	 ORLIX_TCTI_FLAG_MANIPULATION_SETF8,
	 ORLIX_TCTI_FLAG_LINUX_VISIBILITY_TYPED_NA_ARCHITECTURAL_PSTATE},
	{3478, "SETF16", "SETF", 0xfffffc1fU, 0x3a00480dU,
	 ORLIX_TCTI_FLAG_MANIPULATION_SETF16,
	 ORLIX_TCTI_FLAG_LINUX_VISIBILITY_TYPED_NA_ARCHITECTURAL_PSTATE},
};

static const char *orlix_tcti_flag_artifact_string(
	const struct orlix_tcti_target_instruction_artifact *artifact, u32 offset)
{
	return offset < artifact->string_pool_size ?
		(const char *)artifact->string_pool + offset : NULL;
}

static const struct orlix_tcti_flag_semantic_provenance_row *
orlix_tcti_flag_semantic_provenance_row(u16 ordinal)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_flag_semantic_provenance_rows);
	     index++)
		if (orlix_tcti_flag_semantic_provenance_rows[index].ordinal == ordinal)
			return &orlix_tcti_flag_semantic_provenance_rows[index];
	return NULL;
}

static void orlix_tcti_flag_source_and_slice_bindings(struct kunit *test)
{
	const struct orlix_tcti_target_instruction_artifact *artifact =
		orlix_tcti_target_instruction_artifact_canonical();
	const struct orlix_tcti_execution_slice_map *map =
		orlix_tcti_execution_slice_map_canonical();
	struct orlix_tcti_target_instruction_artifact_validation_result artifact_result;
	struct orlix_tcti_execution_slice_map_validation_result map_result;
	size_t index;

	KUNIT_ASSERT_NOT_NULL(test, artifact);
	KUNIT_ASSERT_NOT_NULL(test, map);
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_target_kunit_dependency_validate_for_test(
			ORLIX_TCTI_FLAG_SOURCE,
			ORLIX_TCTI_FLAG_SEMANTIC_PROVENANCE_MANIFEST,
			ORLIX_TCTI_FLAG_SEMANTIC_PROVENANCE_MANIFEST_SHA256));
	KUNIT_ASSERT_NOT_NULL(test, orlix_tcti_flag_semantic_source_identity);
	KUNIT_ASSERT_NOT_NULL(test, strstr(orlix_tcti_flag_semantic_source_identity,
		artifact->source_sha256));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_target_instruction_artifact_validate(
		artifact, &artifact_result));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execution_slice_map_validate(
		map, &map_result));
	KUNIT_ASSERT_EQ(test, (size_t)3, ARRAY_SIZE(orlix_tcti_flag_leaves));

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_flag_leaves); index++) {
		const struct orlix_tcti_flag_leaf *leaf = &orlix_tcti_flag_leaves[index];
		const struct orlix_tcti_target_instruction_artifact_leaf *source =
			&artifact->leaves[leaf->ordinal];
		const struct orlix_tcti_execution_slice_member *member =
			&map->members[leaf->ordinal];
		const struct orlix_tcti_execution_slice_family *family =
			&map->families[member->family_index];
		const struct orlix_tcti_flag_semantic_provenance_row *
			semantic_row = orlix_tcti_flag_semantic_provenance_row(leaf->ordinal);

		KUNIT_ASSERT_NOT_NULL(test, semantic_row);
		KUNIT_EXPECT_EQ(test, (u32)leaf->ordinal, semantic_row->ordinal);
		KUNIT_EXPECT_STREQ(test, semantic_row->name,
			orlix_tcti_flag_artifact_string(artifact, source->name_offset));
		KUNIT_EXPECT_STREQ(test, leaf->mnemonic,
			orlix_tcti_flag_artifact_string(artifact, source->mnemonic_offset));
		KUNIT_EXPECT_STREQ(test, leaf->operation,
			orlix_tcti_flag_artifact_string(artifact, source->operation_offset));
		KUNIT_EXPECT_EQ(test, leaf->mask, source->encoding_mask);
		KUNIT_EXPECT_EQ(test, leaf->pattern, source->encoding_pattern);
		KUNIT_EXPECT_EQ(test, (u32)leaf->ordinal, member->ordinal);
		KUNIT_EXPECT_STREQ(test, semantic_row->name, member->source_name);
		KUNIT_EXPECT_STREQ(test, "base-residual-flags", family->stable_id);
		KUNIT_EXPECT_EQ(test, 181U, family->issue_id);
		KUNIT_EXPECT_EQ(test, 3U, family->declared_member_count);
		KUNIT_EXPECT_NOT_NULL(test, semantic_row->relative_file);
		KUNIT_EXPECT_NOT_NULL(test, semantic_row->decode_locator);
		KUNIT_EXPECT_NOT_NULL(test, semantic_row->decode_sha256);
		KUNIT_EXPECT_NOT_NULL(test, semantic_row->execute_locator);
		KUNIT_EXPECT_NOT_NULL(test, semantic_row->execute_sha256);
		KUNIT_EXPECT_TRUE(test, strcmp(semantic_row->decode_sha256,
			semantic_row->execute_sha256));
		KUNIT_EXPECT_EQ(test,
			ORLIX_TCTI_FLAG_LINUX_VISIBILITY_TYPED_NA_ARCHITECTURAL_PSTATE,
			leaf->linux_visibility);
	}
}

static void orlix_tcti_flag_legal_encoding_matrix(struct kunit *test)
{
	u8 rn;
	u8 imm6;
	u8 mask;

	for (rn = 0; rn < 32; rn++) {
		for (imm6 = 0; imm6 < 64; imm6++) {
			for (mask = 0; mask < 16; mask++) {
				u32 instruction = 0xba000400U | ((u32)imm6 << 15) |
					((u32)rn << 5) | mask;
				struct orlix_tcti_decoded_instruction decoded =
					orlix_tcti_decode_aarch64(instruction);

				KUNIT_ASSERT_EQ(test, ORLIX_TCTI_DECODE_FLAG_MANIPULATION,
					decoded.decode_class);
				KUNIT_EXPECT_EQ(test, ORLIX_TCTI_FLAG_MANIPULATION_RMIF,
					decoded.flag_manipulation_op);
				KUNIT_EXPECT_EQ(test, rn, decoded.rn);
				KUNIT_EXPECT_EQ(test, imm6, decoded.imm6);
				KUNIT_EXPECT_EQ(test, mask, decoded.nzcv);
			}
		}
	}

	for (rn = 0; rn < 32; rn++) {
		struct orlix_tcti_decoded_instruction setf8 =
			orlix_tcti_decode_aarch64(0x3a00080dU | ((u32)rn << 5));
		struct orlix_tcti_decoded_instruction setf16 =
			orlix_tcti_decode_aarch64(0x3a00480dU | ((u32)rn << 5));

		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_FLAG_MANIPULATION,
			setf8.decode_class);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_FLAG_MANIPULATION_SETF8,
			setf8.flag_manipulation_op);
		KUNIT_EXPECT_EQ(test, rn, setf8.rn);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_FLAG_MANIPULATION,
			setf16.decode_class);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_FLAG_MANIPULATION_SETF16,
			setf16.flag_manipulation_op);
		KUNIT_EXPECT_EQ(test, rn, setf16.rn);
	}
}

static bool orlix_tcti_flag_source_allocates(
	const struct orlix_tcti_target_instruction_artifact *artifact, u32 instruction)
{
	u32 ordinal;

	for (ordinal = 0; ordinal < artifact->leaf_count; ordinal++) {
		const struct orlix_tcti_target_instruction_artifact_leaf *leaf =
			&artifact->leaves[ordinal];

		if ((instruction & leaf->encoding_mask) == leaf->encoding_pattern)
			return true;
	}
	return false;
}

static void orlix_tcti_flag_reserved_fixed_bit_matrix(struct kunit *test)
{
	const struct orlix_tcti_target_instruction_artifact *artifact =
		orlix_tcti_target_instruction_artifact_canonical();
	size_t leaf_index;
	u32 rejected = 0;

	KUNIT_ASSERT_NOT_NULL(test, artifact);
	for (leaf_index = 0; leaf_index < ARRAY_SIZE(orlix_tcti_flag_leaves);
	     leaf_index++) {
		const struct orlix_tcti_flag_leaf *leaf =
			&orlix_tcti_flag_leaves[leaf_index];
		u8 bit;

		for (bit = 0; bit < 32; bit++) {
			u32 instruction;

			if (!(leaf->mask & BIT(bit)))
				continue;
			instruction = leaf->pattern ^ BIT(bit);
			if (orlix_tcti_flag_source_allocates(artifact, instruction))
				continue;
			KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
				orlix_tcti_decode_aarch64(instruction).decode_class,
				"%s fixed bit %u", leaf->name, bit);
			rejected++;
		}
	}
	KUNIT_EXPECT_GT(test, rejected, 0U);
}

static unsigned long orlix_tcti_flag_expected_setf(
	u64 source, u8 width, unsigned long old_nzcv)
{
	u64 value = source & (BIT_ULL(width) - 1);
	bool sign = value & BIT_ULL(width - 1);
	bool extension = source & BIT_ULL(width);

	return (sign ? PSR_N_BIT : 0) | (!value ? PSR_Z_BIT : 0) |
		(old_nzcv & PSR_C_BIT) | (sign != extension ? PSR_V_BIT : 0);
}

static void orlix_tcti_flag_expect_lowered_execution(
	struct kunit *test, u32 instruction, u64 source, unsigned long old_nzcv,
	unsigned long expected_nzcv)
{
	struct orlix_tcti_decoded_instruction decoded =
		orlix_tcti_decode_aarch64(instruction);
	struct orlix_tcti_gadget_word
		program[ORLIX_TCTI_SINGLE_INSTRUCTION_PROGRAM_WORDS] = {};
	struct pt_regs regs = {};
	struct pt_regs before;
	unsigned long fault_address = 0xfeedUL;
	size_t word_count = 0;

	KUNIT_ASSERT_EQ(test, ORLIX_TCTI_DECODE_FLAG_MANIPULATION,
		decoded.decode_class);
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_lower_decoded_instruction(
		&decoded, program, ARRAY_SIZE(program), &word_count));
	if (decoded.rn != 31)
		regs.regs[decoded.rn] = source;
	regs.pc = 0x4000;
	regs.sp = 0x8000;
	regs.pstate = PSR_MODE_EL0t | 0x200UL | old_nzcv;
	before = regs;
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_gadget_program(
		NULL, &regs, program, word_count, &fault_address));
	KUNIT_EXPECT_EQ(test, expected_nzcv, regs.pstate & FLAG_NZCV);
	KUNIT_EXPECT_EQ(test, before.pstate & ~FLAG_NZCV,
		regs.pstate & ~FLAG_NZCV);
	KUNIT_EXPECT_EQ(test, before.pc + sizeof(u32), regs.pc);
	KUNIT_EXPECT_EQ(test, before.sp, regs.sp);
	KUNIT_EXPECT_MEMEQ(test, before.regs, regs.regs, sizeof(regs.regs));
	KUNIT_EXPECT_EQ(test, 0xfeedUL, fault_address);
}

static void orlix_tcti_flag_pstate_semantics_matrix(struct kunit *test)
{
	static const u64 setf_values[] = {
		0, 1, 0x3f, 0x40, 0x7f, 0x80, 0xc0, 0xff,
		0x3fff, 0x4000, 0x7fff, 0x8000, 0xc000, 0xffff,
		0xffff000000000000ULL,
	};
	u8 old;
	u8 source_nibble;
	u8 mask;
	size_t index;

	for (old = 0; old < 16; old++) {
		for (source_nibble = 0; source_nibble < 16; source_nibble++) {
			for (mask = 0; mask < 16; mask++) {
				unsigned long old_flags = (unsigned long)old << 28;
				unsigned long source_flags =
					(unsigned long)source_nibble << 28;
				unsigned long selected = (unsigned long)mask << 28;

				orlix_tcti_flag_expect_lowered_execution(test,
					0xba000420U | mask, source_nibble, old_flags,
					(old_flags & ~selected) | (source_flags & selected));
			}
		}
	}

	/* Rotation by 63 places source bit 63 into the low-nibble V position. */
	orlix_tcti_flag_expect_lowered_execution(test, 0xba1f8428U,
		BIT_ULL(63), 0, PSR_V_BIT);
	for (index = 0; index < ARRAY_SIZE(setf_values); index++) {
		u64 source = setf_values[index] | 0xa5a5000000000000ULL;

		orlix_tcti_flag_expect_lowered_execution(test, 0x3a00082dU,
			source, FLAG_NZCV,
			orlix_tcti_flag_expected_setf(source, 8, FLAG_NZCV));
		orlix_tcti_flag_expect_lowered_execution(test, 0x3a00482dU,
			source, FLAG_NZCV,
			orlix_tcti_flag_expected_setf(source, 16, FLAG_NZCV));
	}
	/* Rn == 31 is XZR/WZR, not SP. */
	/* RMIF must read XZR as zero, even when the caller supplies a nonzero source. */
	orlix_tcti_flag_expect_lowered_execution(test, 0xba0007efU,
		U64_MAX, FLAG_NZCV, 0);
	orlix_tcti_flag_expect_lowered_execution(test, 0x3a000bedU,
		U64_MAX, FLAG_NZCV, PSR_Z_BIT | PSR_C_BIT);
}

static void orlix_tcti_flag_non_el0_rejected_without_state_change(
	struct kunit *test)
{
	struct orlix_tcti_decoded_instruction decoded =
		orlix_tcti_decode_aarch64(0xba00042fU);
	struct orlix_tcti_gadget_word
		program[ORLIX_TCTI_SINGLE_INSTRUCTION_PROGRAM_WORDS] = {};
	struct pt_regs regs = {};
	struct pt_regs before;
	unsigned long fault_address = 0x1234;
	size_t word_count = 0;

	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_lower_decoded_instruction(
		&decoded, program, ARRAY_SIZE(program), &word_count));
	regs.regs[1] = U64_MAX;
	regs.pc = 0x4000;
	regs.pstate = 0x5UL | FLAG_NZCV;
	before = regs;
	KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, orlix_tcti_execute_gadget_program(
		NULL, &regs, program, word_count, &fault_address));
	KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
	KUNIT_EXPECT_EQ(test, 0x1234UL, fault_address);
}

static struct kunit_case orlix_tcti_flag_manipulation_test_cases[] = {
	KUNIT_CASE(orlix_tcti_flag_source_and_slice_bindings),
	KUNIT_CASE(orlix_tcti_flag_legal_encoding_matrix),
	KUNIT_CASE(orlix_tcti_flag_reserved_fixed_bit_matrix),
	KUNIT_CASE(orlix_tcti_flag_pstate_semantics_matrix),
	KUNIT_CASE(orlix_tcti_flag_non_el0_rejected_without_state_change),
	{}
};

struct kunit_suite orlix_tcti_flag_manipulation_test_suite = {
	.name = "orlix-tcti-flag-manipulation-source-bound",
	.test_cases = orlix_tcti_flag_manipulation_test_cases,
};

kunit_test_suite(orlix_tcti_flag_manipulation_test_suite);
