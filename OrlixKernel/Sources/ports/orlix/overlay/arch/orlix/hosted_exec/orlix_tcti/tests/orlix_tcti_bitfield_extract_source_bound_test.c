// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/bitops.h>
#include <linux/limits.h>
#include <linux/string.h>
#include <asm/ptrace.h>
#include <asm/orlix_tcti.h>
#include <asm/elf.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/syscalls.h>

#include "target_instruction_artifact.h"
#include "target_execution_slice_map.h"
#include "target_native_proof_registry_private.h"
#include "target_proof_registry.h"
#include "../decode_aarch64.h"
#include "../engine.h"
#include "../switch_debug.h"

static const struct orlix_tcti_test_feature_profile cssc_test_profile = {
	.hwcap = ELF_HWCAP,
	.hwcap2 = ELF_HWCAP2 | HWCAP2_CSSC,
};

struct source_row {
	u32 ordinal;
	const char *source_id;
	const char *operation;
	u32 mask;
	u32 pattern;
	enum orlix_tcti_decode_class class;
	enum orlix_tcti_bitfield_op op;
};

struct runtime_row {
	const char *source_id;
	const char *operation;
	u32 mask;
	u32 pattern;
	u32 witness;
};

#define ORLIX_TCTI_A64_PROFILE(...)
#define ORLIX_TCTI_A64_SOURCE(...)
#define ORLIX_TCTI_A64_FEATURE(...)
#define ORLIX_TCTI_A64_CONDITION(...)
#define ORLIX_TCTI_A64_RUNTIME_ENCODING(name, mnemonic, operation, mask, pattern, \
					  witness) \
	{ #name, #operation, mask, pattern, witness },
static const struct runtime_row runtime_rows[] = {
#include "../isa/inventory.def"
};
#undef ORLIX_TCTI_A64_RUNTIME_ENCODING
#undef ORLIX_TCTI_A64_CONDITION
#undef ORLIX_TCTI_A64_FEATURE
#undef ORLIX_TCTI_A64_SOURCE
#undef ORLIX_TCTI_A64_PROFILE

static const struct source_row source_rows[] = {
	{ 2169U, "EXTR_32_extract", "EXTR", 0xffe08000U, 0x13800000U,
	  ORLIX_TCTI_DECODE_EXTRACT, 0 },
	{ 2170U, "EXTR_64_extract", "EXTR", 0xffe00000U, 0x93c00000U,
	  ORLIX_TCTI_DECODE_EXTRACT, 0 },
	{ 2205U, "SBFM_32M_bitfield", "SBFM", 0xffc00000U, 0x13000000U,
	  ORLIX_TCTI_DECODE_BITFIELD, ORLIX_TCTI_BITFIELD_SBFM },
	{ 2206U, "BFM_32M_bitfield", "BFM", 0xffc00000U, 0x33000000U,
	  ORLIX_TCTI_DECODE_BITFIELD, ORLIX_TCTI_BITFIELD_BFM },
	{ 2207U, "UBFM_32M_bitfield", "UBFM", 0xffc00000U, 0x53000000U,
	  ORLIX_TCTI_DECODE_BITFIELD, ORLIX_TCTI_BITFIELD_UBFM },
	{ 2208U, "SBFM_64M_bitfield", "SBFM", 0xffc00000U, 0x93400000U,
	  ORLIX_TCTI_DECODE_BITFIELD, ORLIX_TCTI_BITFIELD_SBFM },
	{ 2209U, "BFM_64M_bitfield", "BFM", 0xffc00000U, 0xb3400000U,
	  ORLIX_TCTI_DECODE_BITFIELD, ORLIX_TCTI_BITFIELD_BFM },
	{ 2210U, "UBFM_64M_bitfield", "UBFM", 0xffc00000U, 0xd3400000U,
	  ORLIX_TCTI_DECODE_BITFIELD, ORLIX_TCTI_BITFIELD_UBFM },
};

struct scalar_source_row {
	u32 ordinal;
	const char *leaf;
	const char *operation;
	u32 mask;
	u32 pattern;
	enum orlix_tcti_decode_class class;
	enum orlix_tcti_data_processing_1source_op op;
};

static const struct scalar_source_row dp1_source_rows[] = {
	{ 3389U, "RBIT_32_dp_1src", "RBIT_int", 0xfffffc00U, 0x5ac00000U, ORLIX_TCTI_DECODE_DATA_PROCESSING_1SOURCE, ORLIX_TCTI_DP1_RBIT },
	{ 3390U, "REV16_32_dp_1src", "REV16_int", 0xfffffc00U, 0x5ac00400U, ORLIX_TCTI_DECODE_DATA_PROCESSING_1SOURCE, ORLIX_TCTI_DP1_REV16 },
	{ 3391U, "REV_32_dp_1src", "REV", 0xfffffc00U, 0x5ac00800U, ORLIX_TCTI_DECODE_DATA_PROCESSING_1SOURCE, ORLIX_TCTI_DP1_REV },
	{ 3392U, "CLZ_32_dp_1src", "CLZ_int", 0xfffffc00U, 0x5ac01000U, ORLIX_TCTI_DECODE_DATA_PROCESSING_1SOURCE, ORLIX_TCTI_DP1_CLZ },
	{ 3393U, "CLS_32_dp_1src", "CLS_int", 0xfffffc00U, 0x5ac01400U, ORLIX_TCTI_DECODE_DATA_PROCESSING_1SOURCE, ORLIX_TCTI_DP1_CLS },
	{ 3394U, "CTZ_32_dp_1src", "CTZ", 0xfffffc00U, 0x5ac01800U, ORLIX_TCTI_DECODE_DATA_PROCESSING_1SOURCE, ORLIX_TCTI_DP1_CTZ },
	{ 3395U, "CNT_32_dp_1src", "CNT", 0xfffffc00U, 0x5ac01c00U, ORLIX_TCTI_DECODE_DATA_PROCESSING_1SOURCE, ORLIX_TCTI_DP1_CNT },
	{ 3396U, "ABS_32_dp_1src", "ABS", 0xfffffc00U, 0x5ac02000U, ORLIX_TCTI_DECODE_DATA_PROCESSING_1SOURCE, ORLIX_TCTI_DP1_ABS },
	{ 3397U, "RBIT_64_dp_1src", "RBIT_int", 0xfffffc00U, 0xdac00000U, ORLIX_TCTI_DECODE_DATA_PROCESSING_1SOURCE, ORLIX_TCTI_DP1_RBIT },
	{ 3398U, "REV16_64_dp_1src", "REV16_int", 0xfffffc00U, 0xdac00400U, ORLIX_TCTI_DECODE_DATA_PROCESSING_1SOURCE, ORLIX_TCTI_DP1_REV16 },
	{ 3399U, "REV32_64_dp_1src", "REV32_int", 0xfffffc00U, 0xdac00800U, ORLIX_TCTI_DECODE_DATA_PROCESSING_1SOURCE, ORLIX_TCTI_DP1_REV32 },
	{ 3400U, "REV_64_dp_1src", "REV", 0xfffffc00U, 0xdac00c00U, ORLIX_TCTI_DECODE_DATA_PROCESSING_1SOURCE, ORLIX_TCTI_DP1_REV },
	{ 3401U, "CLZ_64_dp_1src", "CLZ_int", 0xfffffc00U, 0xdac01000U, ORLIX_TCTI_DECODE_DATA_PROCESSING_1SOURCE, ORLIX_TCTI_DP1_CLZ },
	{ 3402U, "CLS_64_dp_1src", "CLS_int", 0xfffffc00U, 0xdac01400U, ORLIX_TCTI_DECODE_DATA_PROCESSING_1SOURCE, ORLIX_TCTI_DP1_CLS },
	{ 3403U, "CTZ_64_dp_1src", "CTZ", 0xfffffc00U, 0xdac01800U, ORLIX_TCTI_DECODE_DATA_PROCESSING_1SOURCE, ORLIX_TCTI_DP1_CTZ },
	{ 3404U, "CNT_64_dp_1src", "CNT", 0xfffffc00U, 0xdac01c00U, ORLIX_TCTI_DECODE_DATA_PROCESSING_1SOURCE, ORLIX_TCTI_DP1_CNT },
	{ 3405U, "ABS_64_dp_1src", "ABS", 0xfffffc00U, 0xdac02000U, ORLIX_TCTI_DECODE_DATA_PROCESSING_1SOURCE, ORLIX_TCTI_DP1_ABS },
};

struct alias_case {
	const char *name;
	bool sf;
	u8 opc;
	u8 immr;
	u8 imms;
	u8 rn;
	u8 rd;
};

static const struct alias_case aliases[] = {
	{ "ASR_W", false, 0, 5, 31, 4, 7 },
	{ "ASR_X", true, 0, 9, 63, 4, 7 },
	{ "SBFIZ_W", false, 0, 24, 7, 4, 7 },
	{ "SBFIZ_X", true, 0, 56, 7, 4, 7 },
	{ "SBFX_W", false, 0, 8, 15, 4, 7 },
	{ "SBFX_X", true, 0, 8, 15, 4, 7 },
	{ "SXTB_W", false, 0, 0, 7, 4, 7 },
	{ "SXTB_X", true, 0, 0, 7, 4, 7 },
	{ "SXTH_W", false, 0, 0, 15, 4, 7 },
	{ "SXTH_X", true, 0, 0, 15, 4, 7 },
	{ "SXTW_X", true, 0, 0, 31, 4, 7 },
	{ "BFC_W", false, 1, 24, 7, 31, 7 },
	{ "BFC_X", true, 1, 56, 7, 31, 7 },
	{ "BFI_W", false, 1, 24, 7, 4, 7 },
	{ "BFI_X", true, 1, 56, 7, 4, 7 },
	{ "BFXIL_W", false, 1, 8, 15, 4, 7 },
	{ "BFXIL_X", true, 1, 8, 15, 4, 7 },
	{ "LSL_W", false, 2, 24, 23, 4, 7 },
	{ "LSL_X", true, 2, 56, 55, 4, 7 },
	{ "LSR_W", false, 2, 9, 31, 4, 7 },
	{ "LSR_X", true, 2, 9, 63, 4, 7 },
	{ "UBFIZ_W", false, 2, 24, 7, 4, 7 },
	{ "UBFIZ_X", true, 2, 56, 7, 4, 7 },
	{ "UBFX_W", false, 2, 8, 15, 4, 7 },
	{ "UBFX_X", true, 2, 8, 15, 4, 7 },
	{ "UXTB_W", false, 2, 0, 7, 4, 7 },
	{ "UXTH_W", false, 2, 0, 15, 4, 7 },
};

static u32 encode_bitfield(bool sf, u8 opc, bool n, u8 immr, u8 imms,
			   u8 rn, u8 rd)
{
	return 0x13000000U | (sf ? BIT(31) : 0) | ((u32)opc << 29) |
		(n ? BIT(22) : 0) | ((u32)immr << 16) |
		((u32)imms << 10) | ((u32)rn << 5) | rd;
}

static u32 encode_extract(bool sf, bool n, bool bit21, u8 rm, u8 shift,
			  u8 rn, u8 rd)
{
	return 0x13800000U | (sf ? BIT(31) : 0) | (n ? BIT(22) : 0) |
		(bit21 ? BIT(21) : 0) | ((u32)rm << 16) |
		((u32)shift << 10) | ((u32)rn << 5) | rd;
}

static u64 width_mask(bool sf)
{
	return sf ? U64_MAX : U32_MAX;
}

static u64 ones(u8 width)
{
	return width == 64 ? U64_MAX : BIT_ULL(width) - 1;
}

static u64 bitfield_expected(const struct orlix_tcti_decoded_instruction *decoded,
			     u64 source, u64 destination)
{
	u8 data_size = decoded->is_64bit ? 64 : 32;
	u8 immr = decoded->bitfield_immr;
	u8 imms = decoded->bitfield_imms;
	u8 field_width;
	u8 lsb;
	u64 mask = width_mask(decoded->is_64bit);
	u64 field_mask;
	u64 value;

	source &= mask;
	destination &= mask;
	if (imms >= immr) {
		field_width = imms - immr + 1;
		lsb = 0;
		value = source >> immr;
	} else {
		field_width = imms + 1;
		lsb = data_size - immr;
		value = source << lsb;
	}
	field_mask = ones(field_width) << lsb;
	value &= field_mask;
	if (decoded->bitfield_op == ORLIX_TCTI_BITFIELD_BFM)
		return ((destination & ~field_mask) | value) & mask;
	if (decoded->bitfield_op == ORLIX_TCTI_BITFIELD_SBFM &&
	    (value & BIT_ULL(lsb + field_width - 1)))
		value |= mask & ~ones(lsb + field_width);
	return value & mask;
}

static void seed_regs(struct pt_regs *regs, u32 instruction)
{
	u8 reg;

	memset(regs, 0, sizeof(*regs));
	for (reg = 0; reg < 31; reg++)
		regs->regs[reg] = 0x9e3779b97f4a7c15ULL ^
			((u64)instruction << (reg & 7)) ^ reg;
	regs->sp = 0x123456789abcdef0ULL;
	regs->pc = 0x2468ace000ULL;
	regs->pstate = PSR_N_BIT | PSR_C_BIT | 0x155UL;
}

static void expect_state(struct kunit *test, const struct pt_regs *before,
			 const struct pt_regs *after, u8 rd, u64 expected)
{
	u8 reg;

	for (reg = 0; reg < 31; reg++)
		KUNIT_EXPECT_EQ(test, reg == rd ? expected : before->regs[reg],
				after->regs[reg]);
	KUNIT_EXPECT_EQ(test, before->sp, after->sp);
	KUNIT_EXPECT_EQ(test, before->pstate, after->pstate);
	KUNIT_EXPECT_EQ(test, before->pc + sizeof(u32), after->pc);
}

static const char *bitfield_extract_artifact_string(
	const struct orlix_tcti_target_instruction_artifact *artifact, u32 offset)
{
	if (offset >= artifact->string_pool_size)
		return NULL;
	return (const char *)artifact->string_pool + offset;
}

static const struct runtime_row *bitfield_extract_runtime_row(
	const struct source_row *source)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(runtime_rows); index++)
		if (!strcmp(runtime_rows[index].source_id, source->source_id))
			return &runtime_rows[index];
	return NULL;
}

static void bitfield_extract_target_binding(struct kunit *test,
	const struct orlix_tcti_target_instruction_artifact *artifact,
	const struct source_row *source)
{
	const struct orlix_tcti_target_instruction_artifact_leaf *leaf;
	const struct runtime_row *runtime;
	const char *source_id;
	const char *operation;

	KUNIT_ASSERT_LT_MSG(test, source->ordinal, artifact->leaf_count,
		"source=%s", source->source_id);
	leaf = &artifact->leaves[source->ordinal];
	source_id = bitfield_extract_artifact_string(artifact, leaf->name_offset);
	operation = bitfield_extract_artifact_string(artifact,
					     leaf->operation_offset);
	KUNIT_ASSERT_NOT_NULL_MSG(test, source_id, "ordinal=%u", source->ordinal);
	KUNIT_ASSERT_NOT_NULL_MSG(test, operation, "ordinal=%u", source->ordinal);
	KUNIT_EXPECT_STREQ_MSG(test, source->source_id, source_id,
		"ordinal=%u", source->ordinal);
	KUNIT_EXPECT_STREQ_MSG(test, source->operation, operation,
		"ordinal=%u", source->ordinal);
	KUNIT_EXPECT_EQ_MSG(test, source->mask, leaf->encoding_mask,
		"ordinal=%u", source->ordinal);
	KUNIT_EXPECT_EQ_MSG(test, source->pattern, leaf->encoding_pattern,
		"ordinal=%u", source->ordinal);

	runtime = bitfield_extract_runtime_row(source);
	KUNIT_ASSERT_NOT_NULL_MSG(test, runtime, "source=%s", source->source_id);
	KUNIT_EXPECT_STREQ(test, source->operation, runtime->operation);
	KUNIT_EXPECT_EQ(test, source->mask, runtime->mask);
	KUNIT_EXPECT_EQ(test, source->pattern, runtime->pattern);
	KUNIT_EXPECT_EQ(test, source->pattern, runtime->witness);
}

static void source_fingerprints(struct kunit *test)
{
	const struct orlix_tcti_target_instruction_artifact *artifact =
		orlix_tcti_target_instruction_artifact_canonical();
	struct orlix_tcti_target_instruction_artifact_validation_result validation;
	size_t i;

	KUNIT_ASSERT_NOT_NULL(test, artifact);
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_target_instruction_artifact_validate(artifact, &validation));
	for (i = 0; i < ARRAY_SIZE(source_rows); i++) {
		struct orlix_tcti_decoded_instruction decoded =
			orlix_tcti_decode_aarch64(source_rows[i].pattern);

		bitfield_extract_target_binding(test, artifact, &source_rows[i]);
		KUNIT_EXPECT_EQ_MSG(test, source_rows[i].pattern,
				    source_rows[i].pattern & source_rows[i].mask,
				    "%s source ordinal %u", source_rows[i].source_id,
				    source_rows[i].ordinal);
		KUNIT_ASSERT_EQ_MSG(test, source_rows[i].class,
				    decoded.decode_class, "%s source ordinal %u",
				    source_rows[i].source_id, source_rows[i].ordinal);
		if (decoded.decode_class == ORLIX_TCTI_DECODE_BITFIELD)
			KUNIT_EXPECT_EQ_MSG(test, source_rows[i].op,
					    decoded.bitfield_op, "%s source ordinal %u",
					    source_rows[i].source_id,
					    source_rows[i].ordinal);
	}
}

static void scalar_source_fingerprints(struct kunit *test)
{
	const struct orlix_tcti_target_instruction_artifact *artifact =
		orlix_tcti_target_instruction_artifact_canonical();
	size_t i;

	KUNIT_ASSERT_NOT_NULL(test, artifact);
	KUNIT_ASSERT_EQ(test, 25U,
		ARRAY_SIZE(source_rows) + ARRAY_SIZE(dp1_source_rows));
	for (i = 0; i < ARRAY_SIZE(dp1_source_rows); i++) {
		const struct scalar_source_row *row = &dp1_source_rows[i];
		const struct orlix_tcti_target_instruction_artifact_leaf *leaf;
		const char *leaf_name;
		const char *operation;
		struct orlix_tcti_decoded_instruction decoded =
			orlix_tcti_decode_aarch64(row->pattern);
		size_t prior;

		KUNIT_ASSERT_LT(test, row->ordinal, artifact->leaf_count);
		leaf = &artifact->leaves[row->ordinal];
		leaf_name = bitfield_extract_artifact_string(artifact,
			leaf->name_offset);
		operation = bitfield_extract_artifact_string(artifact,
			leaf->operation_offset);
		KUNIT_ASSERT_NOT_NULL(test, leaf_name);
		KUNIT_ASSERT_NOT_NULL(test, operation);
		KUNIT_EXPECT_STREQ(test, row->leaf, leaf_name);
		KUNIT_EXPECT_STREQ(test, row->operation, operation);
		KUNIT_EXPECT_EQ(test, row->mask, leaf->encoding_mask);
		KUNIT_EXPECT_EQ(test, row->pattern, leaf->encoding_pattern);
		for (prior = 0; prior < i; prior++)
			KUNIT_EXPECT_NE(test, row->ordinal,
				dp1_source_rows[prior].ordinal);

		KUNIT_EXPECT_EQ_MSG(test, row->pattern, row->pattern & row->mask,
				    "%s source ordinal %u", row->leaf,
				    row->ordinal);
		KUNIT_ASSERT_EQ_MSG(test, row->class, decoded.decode_class,
				    "%s source ordinal %u", row->leaf,
				    row->ordinal);
		KUNIT_EXPECT_EQ_MSG(test, row->op, decoded.dp1_op,
				    "%s source ordinal %u", row->leaf,
				    row->ordinal);
	}
}

static const char *bitfield_unary_source_name(u32 ordinal)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(source_rows); index++)
		if (source_rows[index].ordinal == ordinal)
			return source_rows[index].source_id;
	for (index = 0; index < ARRAY_SIZE(dp1_source_rows); index++)
		if (dp1_source_rows[index].ordinal == ordinal)
			return dp1_source_rows[index].leaf;
	return NULL;
}

static void bitfield_unary_exact_cohort(struct kunit *test)
{
	const struct orlix_tcti_execution_slice_map *map =
		orlix_tcti_execution_slice_map_canonical();
	struct orlix_tcti_execution_slice_map_validation_result validation;
	size_t family_index;
	size_t member_index;
	size_t matched = 0;
	size_t selected_family = SIZE_MAX;

	KUNIT_ASSERT_NOT_NULL(test, map);
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_execution_slice_map_validate(map, &validation));
	KUNIT_ASSERT_EQ(test, 25U,
		ARRAY_SIZE(source_rows) + ARRAY_SIZE(dp1_source_rows));
	for (family_index = 0; family_index < map->counts.family_count;
	     family_index++) {
		const struct orlix_tcti_execution_slice_family *family =
			&map->families[family_index];

		if (family->issue_id != 129U)
			continue;
		KUNIT_EXPECT_EQ(test, SIZE_MAX, selected_family);
		selected_family = family_index;
		KUNIT_EXPECT_STREQ(test, "base-a64-bitfield-unary",
			family->stable_id);
		KUNIT_EXPECT_EQ(test, 25U, family->declared_member_count);
	}
	KUNIT_ASSERT_NE(test, SIZE_MAX, selected_family);
	for (member_index = 0; member_index < map->counts.leaf_count;
	     member_index++) {
		const struct orlix_tcti_execution_slice_member *member =
			&map->members[member_index];
		const char *source_name;

		if (member->family_index != selected_family)
			continue;
		source_name = bitfield_unary_source_name(member->ordinal);
		KUNIT_ASSERT_NOT_NULL_MSG(test, source_name, "ordinal=%u",
			member->ordinal);
		KUNIT_EXPECT_STREQ_MSG(test, source_name, member->source_name,
			"ordinal=%u", member->ordinal);
		matched++;
	}
	KUNIT_EXPECT_EQ(test, 25U, matched);
}

static u64 scalar_reverse_bits(u64 value, u8 width)
{
	u64 result = 0;
	u8 bit;

	for (bit = 0; bit < width; bit++)
		result |= ((value >> bit) & 1) << (width - bit - 1);
	return result;
}

static u64 scalar_reverse_bytes(u64 value, u8 width, u8 lane_width)
{
	u64 result = 0;
	u8 lane;
	u8 byte;

	for (lane = 0; lane < width; lane += lane_width)
		for (byte = 0; byte < lane_width / 8; byte++)
			result |= ((value >> (lane + byte * 8)) & 0xff) <<
				(lane + lane_width - 8 - byte * 8);
	return result;
}

static u64 scalar_count_leading_zeros(u64 value, u8 width)
{
	u64 count = 0;
	int bit;

	for (bit = width - 1; bit >= 0; bit--) {
		if (value & BIT_ULL(bit))
			break;
		count++;
	}
	return count;
}

static u64 scalar_count_leading_sign_bits(u64 value, u8 width)
{
	bool sign = value & BIT_ULL(width - 1);
	u64 count = 0;
	int bit;

	for (bit = width - 2; bit >= 0; bit--) {
		if (!!(value & BIT_ULL(bit)) != sign)
			break;
		count++;
	}
	return count;
}

static u64 scalar_dp1_expected(enum orlix_tcti_data_processing_1source_op op,
			       u64 value, u8 width)
{
	switch (op) {
	case ORLIX_TCTI_DP1_RBIT:
		return scalar_reverse_bits(value, width);
	case ORLIX_TCTI_DP1_REV16:
		return scalar_reverse_bytes(value, width, 16);
	case ORLIX_TCTI_DP1_REV32:
		return scalar_reverse_bytes(value, width, 32);
	case ORLIX_TCTI_DP1_REV:
		return scalar_reverse_bytes(value, width, width);
	case ORLIX_TCTI_DP1_CLZ:
		return scalar_count_leading_zeros(value, width);
	case ORLIX_TCTI_DP1_CLS:
		return scalar_count_leading_sign_bits(value, width);
	case ORLIX_TCTI_DP1_CTZ:
		return value ? __ffs64(value) : width;
	case ORLIX_TCTI_DP1_CNT:
		return width == 64 ? hweight64(value) : hweight32(value);
	case ORLIX_TCTI_DP1_ABS:
		return value & BIT_ULL(width - 1) ? (-value) & ones(width) : value;
	default:
		return 0;
	}
}

static void scalar_source_execution(struct kunit *test)
{
	const u8 rd = 7;
	const u8 rn = 19;
	size_t i;

	for (i = 0; i < ARRAY_SIZE(dp1_source_rows); i++) {
		const struct scalar_source_row *row = &dp1_source_rows[i];
		u32 instruction = row->pattern | ((u32)rn << 5) | rd;
		struct orlix_tcti_decoded_instruction decoded =
			orlix_tcti_decode_aarch64(instruction);
		struct pt_regs regs, before;
		u8 width = decoded.is_64bit ? 64 : 32;
		u64 expected;

		seed_regs(&regs, instruction);
		regs.regs[rn] = 0x8123456789abcdefULL;
		before = regs;
		expected = scalar_dp1_expected(row->op,
			before.regs[rn] & width_mask(decoded.is_64bit), width);
		KUNIT_ASSERT_EQ_MSG(test, 0,
			orlix_tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL),
			"%s source ordinal %u", row->leaf, row->ordinal);
		expect_state(test, &before, &regs, rd,
			     expected & width_mask(decoded.is_64bit));
	}
}

static void exhaustive_bitfield_decode(struct kunit *test)
{
	u32 legal = 0;
	u8 sf, opc, n, immr, imms;

	for (sf = 0; sf < 2; sf++)
		for (opc = 0; opc < 4; opc++)
			for (n = 0; n < 2; n++)
				for (immr = 0; immr < 64; immr++)
					for (imms = 0; imms < 64; imms++) {
						bool valid = opc < 3 && n == sf &&
							(sf || (!(immr & 32) &&
								!(imms & 32)));
						struct orlix_tcti_decoded_instruction decoded =
							orlix_tcti_decode_aarch64(encode_bitfield(
								sf, opc, n, immr, imms, 4, 7));

						if (valid)
							KUNIT_EXPECT_EQ(test,
								ORLIX_TCTI_DECODE_BITFIELD,
								decoded.decode_class);
						else
							KUNIT_EXPECT_EQ(test,
								ORLIX_TCTI_DECODE_UNSUPPORTED,
								decoded.decode_class);
						legal += valid;
					}
	KUNIT_EXPECT_EQ(test, 15360U, legal);
}

static void exhaustive_extract_decode(struct kunit *test)
{
	u32 legal = 0;
	u8 sf, n, bit21, shift;

	for (sf = 0; sf < 2; sf++)
		for (n = 0; n < 2; n++)
			for (bit21 = 0; bit21 < 2; bit21++)
				for (shift = 0; shift < 64; shift++) {
					bool valid = !bit21 && n == sf &&
						(sf || shift < 32);
					struct orlix_tcti_decoded_instruction decoded =
						orlix_tcti_decode_aarch64(encode_extract(
							sf, n, bit21, 5, shift, 4, 7));

					if (valid)
						KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_EXTRACT,
								decoded.decode_class);
					else
						KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
								decoded.decode_class);
					legal += valid;
				}
	KUNIT_EXPECT_EQ(test, 96U, legal);
}

static void exhaustive_bitfield_semantics(struct kunit *test)
{
	u8 sf, opc, immr, imms;

	for (sf = 0; sf < 2; sf++)
		for (opc = 0; opc < 3; opc++)
			for (immr = 0; immr < (sf ? 64 : 32); immr++)
				for (imms = 0; imms < (sf ? 64 : 32); imms++) {
					u32 instruction = encode_bitfield(sf, opc, sf,
						immr, imms, 4, 7);
					struct orlix_tcti_decoded_instruction decoded =
						orlix_tcti_decode_aarch64(instruction);
					struct pt_regs regs, before;
					u64 expected;

					seed_regs(&regs, instruction);
					before = regs;
					expected = bitfield_expected(&decoded,
						before.regs[4], before.regs[7]);
					KUNIT_ASSERT_EQ(test, 0,
						orlix_tcti_switch_debug_execute_decoded(
							NULL, &regs, &decoded, NULL));
					expect_state(test, &before, &regs, 7, expected);
				}
}

static void exhaustive_extract_semantics(struct kunit *test)
{
	u8 sf, shift;

	for (sf = 0; sf < 2; sf++) {
		u8 width = sf ? 64 : 32;

		for (shift = 0; shift < width; shift++) {
			u32 instruction = encode_extract(sf, sf, false, 5,
				shift, 4, 7);
			struct orlix_tcti_decoded_instruction decoded =
				orlix_tcti_decode_aarch64(instruction);
			struct pt_regs regs, before;
			u64 high, low, expected;

			seed_regs(&regs, instruction);
			before = regs;
			high = before.regs[4] & width_mask(sf);
			low = before.regs[5] & width_mask(sf);
			expected = shift ? ((low >> shift) |
				(high << (width - shift))) & width_mask(sf) : low;
			KUNIT_ASSERT_EQ(test, 0,
				orlix_tcti_switch_debug_execute_decoded(
					NULL, &regs, &decoded, NULL));
			expect_state(test, &before, &regs, 7, expected);
		}
	}
}

static void bitfield_alias_state(struct kunit *test)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(aliases); i++) {
		const struct alias_case *alias = &aliases[i];
		u32 instruction = encode_bitfield(alias->sf, alias->opc, alias->sf,
			alias->immr, alias->imms, alias->rn, alias->rd);
		struct orlix_tcti_decoded_instruction decoded =
			orlix_tcti_decode_aarch64(instruction);
		struct pt_regs regs, before;
		u64 source, destination, expected;
		int ret;

		seed_regs(&regs, instruction);
		before = regs;
		source = alias->rn == 31 ? 0 : before.regs[alias->rn];
		destination = alias->rd == 31 ? 0 : before.regs[alias->rd];
		expected = bitfield_expected(&decoded, source, destination);
		ret = orlix_tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);
		KUNIT_ASSERT_EQ_MSG(test, 0, ret, "%s", alias->name);
		expect_state(test, &before, &regs, alias->rd, expected);
	}
}

struct canonical_alias_relation {
	const char *declared;
	const char *resolved;
};

static const struct canonical_alias_relation canonical_alias_relations[] = {
	{ "ASR", "SBFM" }, { "SBFIZ", "SBFM" },
	{ "SBFX", "SBFM" }, { "SXTB", "SBFM" },
	{ "SXTH", "SBFM" }, { "SXTW", "SBFM" },
	{ "BFC", "BFM" }, { "BFI", "BFM" }, { "BFXIL", "BFM" },
	{ "LSL", "UBFM" }, { "LSR", "UBFM" },
	{ "UBFIZ", "UBFM" }, { "UBFX", "UBFM" },
	{ "UXTB", "UBFM" }, { "UXTH", "UBFM" },
	{ "ROR", "EXTR" },
};

static bool bitfield_alias_resolved_operation(const char *operation)
{
	return !strcmp(operation, "SBFM") || !strcmp(operation, "BFM") ||
		!strcmp(operation, "UBFM") || !strcmp(operation, "EXTR");
}

static void bitfield_alias_matrix_and_canonical_relations(struct kunit *test)
{
	const struct orlix_tcti_target_instruction_artifact *artifact =
		orlix_tcti_target_instruction_artifact_canonical();
	size_t index;
	size_t canonical_count = 0;

	KUNIT_ASSERT_NOT_NULL(test, artifact);
	KUNIT_ASSERT_EQ(test, 27U, ARRAY_SIZE(aliases));
	KUNIT_ASSERT_EQ(test, 16U, ARRAY_SIZE(canonical_alias_relations));
	for (index = 0; index < ARRAY_SIZE(aliases); index++) {
		const struct alias_case *alias = &aliases[index];
		u8 width = alias->sf ? 64 : 32;
		size_t name_length = strlen(alias->name);

		KUNIT_ASSERT_GT(test, name_length, 2U);
		KUNIT_EXPECT_EQ_MSG(test, alias->sf ? 'X' : 'W',
			alias->name[name_length - 1], "%s", alias->name);
		if (!strncmp(alias->name, "ASR_", 4) ||
		    !strncmp(alias->name, "LSR_", 4))
			KUNIT_EXPECT_EQ_MSG(test, width - 1, alias->imms,
				"%s", alias->name);
		if (!strncmp(alias->name, "SBFIZ_", 6) ||
		    !strncmp(alias->name, "UBFIZ_", 6) ||
		    !strncmp(alias->name, "BFC_", 4) ||
		    !strncmp(alias->name, "BFI_", 4))
			KUNIT_EXPECT_LT_MSG(test, alias->imms, alias->immr,
				"%s", alias->name);
		if (!strncmp(alias->name, "SBFX_", 5) ||
		    !strncmp(alias->name, "UBFX_", 5) ||
		    !strncmp(alias->name, "BFXIL_", 6))
			KUNIT_EXPECT_GE_MSG(test, alias->imms, alias->immr,
				"%s", alias->name);
		if (!strncmp(alias->name, "BFC_", 4))
			KUNIT_EXPECT_EQ(test, 31, alias->rn);
		if (!strncmp(alias->name, "BFI_", 4))
			KUNIT_EXPECT_NE(test, 31, alias->rn);
		if (!strncmp(alias->name, "LSL_", 4))
			KUNIT_EXPECT_EQ_MSG(test, alias->imms + 1, alias->immr,
				"%s", alias->name);
		if (!strncmp(alias->name, "SXTB_", 5) ||
		    !strncmp(alias->name, "UXTB_", 5)) {
			KUNIT_EXPECT_EQ(test, 0, alias->immr);
			KUNIT_EXPECT_EQ(test, 7, alias->imms);
		}
		if (!strncmp(alias->name, "SXTH_", 5) ||
		    !strncmp(alias->name, "UXTH_", 5)) {
			KUNIT_EXPECT_EQ(test, 0, alias->immr);
			KUNIT_EXPECT_EQ(test, 15, alias->imms);
		}
		if (!strcmp(alias->name, "SXTW_X")) {
			KUNIT_EXPECT_EQ(test, 0, alias->immr);
			KUNIT_EXPECT_EQ(test, 31, alias->imms);
		}
	}
	for (index = 0; index < artifact->instruction_alias_count; index++) {
		const struct orlix_tcti_target_instruction_artifact_instruction_alias
			*alias = &artifact->instruction_aliases[index];
		const char *declared = bitfield_extract_artifact_string(artifact,
			alias->declared_operation_offset);
		const char *resolved = bitfield_extract_artifact_string(artifact,
			alias->resolved_operation_offset);
		size_t relation;
		bool matched = false;

		KUNIT_ASSERT_NOT_NULL(test, declared);
		KUNIT_ASSERT_NOT_NULL(test, resolved);
		if (!bitfield_alias_resolved_operation(resolved))
			continue;
		for (relation = 0;
		     relation < ARRAY_SIZE(canonical_alias_relations); relation++)
			if (!strcmp(declared,
				    canonical_alias_relations[relation].declared) &&
			    !strcmp(resolved,
				    canonical_alias_relations[relation].resolved)) {
				matched = true;
				break;
			}
		KUNIT_ASSERT_TRUE_MSG(test, matched, "%s -> %s", declared, resolved);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_TARGET_ALIAS_RELATION_SEMANTIC,
			alias->relation_kind);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_TARGET_ALIAS_PREDICATE_SOURCE_CONDITION,
			alias->predicate_kind);
		KUNIT_EXPECT_GT(test, alias->condition_length, 0U);
		KUNIT_EXPECT_GT(test, alias->source_length, 0U);
		canonical_count++;
	}
	KUNIT_EXPECT_EQ(test, ARRAY_SIZE(canonical_alias_relations),
		canonical_count);
}

static void extract_ror_alias_state(struct kunit *test)
{
	u8 sf, shift;

	for (sf = 0; sf < 2; sf++) {
		u8 width = sf ? 64 : 32;

		for (shift = 0; shift < width; shift++) {
			u8 rn = shift & 31;
			u8 rd = (shift * 3U) & 31;
			u32 instruction = encode_extract(sf, sf, false, rn, shift,
							 rn, rd);
			struct orlix_tcti_decoded_instruction decoded =
				orlix_tcti_decode_aarch64(instruction);
			struct pt_regs regs, before;
			u64 source, expected;
			int ret;

			seed_regs(&regs, instruction);
			before = regs;
			source = rn == 31 ? 0 : before.regs[rn];
			source &= width_mask(sf);
			expected = shift ?
				((source >> shift) | (source << (width - shift))) &
					width_mask(sf) : source;
			ret = orlix_tcti_switch_debug_execute_decoded(
				NULL, &regs, &decoded, NULL);
			KUNIT_ASSERT_EQ(test, 0, ret);
			expect_state(test, &before, &regs, rd, expected);
		}
	}
}

static void extract_overlap_xzr_state(struct kunit *test)
{
	static const u8 cases[][3] = {
		{ 5, 5, 5 }, { 31, 6, 7 }, { 6, 31, 7 },
		{ 6, 7, 31 }, { 31, 31, 31 }, { 6, 7, 6 }, { 6, 7, 7 },
	};
	size_t i;

	for (i = 0; i < ARRAY_SIZE(cases); i++) {
		u8 rn = cases[i][0], rm = cases[i][1], rd = cases[i][2];
		u8 shift = 13;
		u32 instruction = encode_extract(true, true, false, rm, shift,
						 rn, rd);
		struct orlix_tcti_decoded_instruction decoded =
			orlix_tcti_decode_aarch64(instruction);
		struct pt_regs regs, before;
		u64 high, low, expected;
		int ret;

		seed_regs(&regs, instruction);
		before = regs;
		high = rn == 31 ? 0 : before.regs[rn];
		low = rm == 31 ? 0 : before.regs[rm];
		expected = (low >> shift) | (high << (64 - shift));
		ret = orlix_tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);
		KUNIT_ASSERT_EQ(test, 0, ret);
		expect_state(test, &before, &regs, rd, expected);
	}
}

#define BITFIELD_EXTRACT_SVC 0xd4000001U
#define BITFIELD_UNARY_SOURCE_PATH \
	"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/orlix_tcti_bitfield_extract_source_bound_test.c"

static unsigned long bitfield_extract_map_rx(struct kunit *test, u32 instruction)
{
	const u32 program[] = { instruction, BITFIELD_EXTRACT_SVC };
	unsigned long mapped;
	int ret;

	KUNIT_ASSERT_NOT_NULL(test, current->mm);
	mapped = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));
	ret = orlix_tcti_write_user_data(current->mm, mapped, program, sizeof(program));
	KUNIT_ASSERT_EQ(test, 0, ret);
	ret = sys_mprotect(mapped, PAGE_SIZE, PROT_READ | PROT_EXEC);
	KUNIT_ASSERT_EQ(test, 0, ret);
	return mapped;
}

static void bitfield_extract_seed_resume_regs(struct pt_regs *regs,
					      unsigned long mapped, u32 instruction)
{
	u8 reg;

	memset(regs, 0, sizeof(*regs));
	for (reg = 0; reg < 31; reg++)
		regs->regs[reg] = 0xd1b54a32d192ed03ULL ^
			((u64)instruction << (reg & 7)) ^ reg;
	regs->sp = 0x1fedcba987654320ULL;
	regs->pc = mapped;
	regs->pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_C_BIT | PSR_D_BIT;
	regs->orig_x0 = 0x0f1e2d3c4b5a6978ULL;
	regs->syscallno = NO_SYSCALL;
	regs->unused = 0x5a5a5a5aU;
}

static void bitfield_extract_expect_resume_frame(
	struct kunit *test, const struct pt_regs *before,
	const struct pt_regs *after, u8 rd, u64 expected,
	unsigned long mapped)
{
	u8 reg;

	for (reg = 0; reg < 31; reg++)
		KUNIT_EXPECT_EQ(test, reg == rd ? expected : before->regs[reg],
				after->regs[reg]);
	KUNIT_EXPECT_EQ(test, before->sp, after->sp);
	KUNIT_EXPECT_EQ(test, mapped + sizeof(u32), after->pc);
	KUNIT_EXPECT_EQ(test, before->pstate, after->pstate);
	KUNIT_EXPECT_EQ(test, before->orig_x0, after->orig_x0);
	KUNIT_EXPECT_EQ(test, before->syscallno, after->syscallno);
	KUNIT_EXPECT_EQ(test, before->unused, after->unused);
}

static void bitfield_extract_expect_resume_rejection(
	struct kunit *test, u32 instruction)
{
	struct pt_regs regs, before;
	struct orlix_tcti_result result;
	unsigned long mapped = bitfield_extract_map_rx(test, instruction);

	bitfield_extract_seed_resume_regs(&regs, mapped, instruction);
	before = regs;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION, result.reason);
	KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, result.status);
	KUNIT_EXPECT_EQ(test, 0UL, result.fault_address);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_ACCESS_FETCH, result.fault_access);
	KUNIT_EXPECT_EQ(test, mapped, result.pc);
	KUNIT_EXPECT_EQ(test, instruction, result.instruction);
	KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void bitfield_extract_resume_one(struct kunit *test,
					const struct source_row *row, u32 instruction)
{
	struct orlix_tcti_decoded_instruction decoded = orlix_tcti_decode_aarch64(instruction);
	struct pt_regs regs, before;
	struct orlix_tcti_result result;
	unsigned long mapped;
	u64 source, destination, expected;
	u8 rd = instruction & 31;

	KUNIT_ASSERT_EQ_MSG(test, row->pattern, instruction & row->mask,
			    "%s source ordinal %u", row->source_id, row->ordinal);
	KUNIT_ASSERT_EQ_MSG(test, row->class, decoded.decode_class,
			    "%s (%s)", row->source_id, row->operation);
	mapped = bitfield_extract_map_rx(test, instruction);
	bitfield_extract_seed_resume_regs(&regs, mapped, instruction);
	before = regs;
	if (decoded.decode_class == ORLIX_TCTI_DECODE_BITFIELD) {
		source = decoded.rn == 31 ? 0 : before.regs[decoded.rn];
		destination = decoded.rd == 31 ? 0 : before.regs[decoded.rd];
		expected = bitfield_expected(&decoded, source, destination);
	} else {
		u8 width = decoded.is_64bit ? 64 : 32;
		u64 high = decoded.rn == 31 ? 0 : before.regs[decoded.rn];
		u64 low = decoded.rm == 31 ? 0 : before.regs[decoded.rm];

		high &= width_mask(decoded.is_64bit);
		low &= width_mask(decoded.is_64bit);
		expected = decoded.shift_amount ?
			(low >> decoded.shift_amount) |
			(high << (width - decoded.shift_amount)) : low;
		expected &= width_mask(decoded.is_64bit);
	}
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_ASSERT_EQ_MSG(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason,
			     "%s source ordinal %u", row->source_id, row->ordinal);
	KUNIT_EXPECT_EQ(test, 0L, result.status);
	KUNIT_EXPECT_EQ(test, 0UL, result.fault_address);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_ACCESS_FETCH, result.fault_access);
	KUNIT_EXPECT_EQ(test, mapped + sizeof(u32), result.pc);
	KUNIT_EXPECT_EQ(test, BITFIELD_EXTRACT_SVC, result.instruction);
	bitfield_extract_expect_resume_frame(test, &before, &regs, rd, expected,
					  mapped);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void bitfield_extract_production_path_leaves(struct kunit *test)
{
	static const struct {
		u8 rn;
		u8 rm;
		u8 rd;
		u8 immr32;
		u8 imms32;
		u8 immr64;
		u8 imms64;
	} cases[] = {
		{ 4, 13, 7, 28, 3, 56, 7 },
		{ 7, 7, 7, 24, 7, 56, 7 },
		{ 31, 6, 8, 8, 15, 8, 15 },
		{ 6, 31, 31, 0, 31, 0, 31 },
	};
	size_t row, variant;

	for (row = 0; row < ARRAY_SIZE(source_rows); row++) {
		const struct source_row *source = &source_rows[row];

		for (variant = 0; variant < ARRAY_SIZE(cases); variant++) {
			u32 instruction;
			bool sf = source->ordinal == 2170U || source->ordinal >= 2208U;
			u8 immr = sf ? cases[variant].immr64 :
				cases[variant].immr32;
			u8 imms = sf ? cases[variant].imms64 :
				cases[variant].imms32;

			if (source->class == ORLIX_TCTI_DECODE_BITFIELD)
				instruction = encode_bitfield(sf, source->op, sf,
					immr, imms,
					cases[variant].rn, cases[variant].rd);
			else
				instruction = encode_extract(sf, sf, false,
					cases[variant].rm, immr,
					cases[variant].rn, cases[variant].rd);
			bitfield_extract_resume_one(test, source, instruction);
		}
	}
}

static void bitfield_extract_production_path_aliases(struct kunit *test)
{
	size_t index;

	KUNIT_ASSERT_EQ(test, 27U, ARRAY_SIZE(aliases));
	for (index = 0; index < ARRAY_SIZE(aliases); index++) {
		const struct alias_case *alias = &aliases[index];
		u32 ordinal = 2205U + alias->opc + (alias->sf ? 3U : 0U);
		const struct source_row *row = NULL;
		size_t source;

		for (source = 0; source < ARRAY_SIZE(source_rows); source++)
			if (source_rows[source].ordinal == ordinal)
				row = &source_rows[source];
		KUNIT_ASSERT_NOT_NULL(test, row);
		bitfield_extract_resume_one(test, row,
			encode_bitfield(alias->sf, row->op, alias->sf, alias->immr,
				alias->imms, alias->rn, alias->rd));
	}
	for (index = 0; index < 2; index++) {
		bool sf = index != 0;

		bitfield_extract_resume_one(test, &source_rows[index],
			encode_extract(sf, sf, false, 4, sf ? 9 : 5, 4, 7));
	}
}

static void bitfield_extract_production_path_extract_overlaps(struct kunit *test)
{
	static const struct {
		u8 rn;
		u8 rm;
		u8 rd;
	} cases[] = {
		{ 9, 13, 9 },  /* Rd == Rn */
		{ 9, 13, 13 }, /* Rd == Rm */
	};
	size_t row, index;

	for (row = 0; row < 2; row++) {
		const struct source_row *source = &source_rows[row];
		bool sf = source->ordinal == 2170U;
		u8 shift = sf ? 37 : 19;

		for (index = 0; index < ARRAY_SIZE(cases); index++)
			bitfield_extract_resume_one(test, source,
				encode_extract(sf, sf, false, cases[index].rm, shift,
					cases[index].rn, cases[index].rd));
	}
}

static void bitfield_extract_production_path_reserved_rejection(struct kunit *test)
{
	bitfield_extract_expect_resume_rejection(test,
		encode_bitfield(false, ORLIX_TCTI_BITFIELD_SBFM, true, 0, 7, 4, 7));
	bitfield_extract_expect_resume_rejection(test,
		encode_bitfield(true, ORLIX_TCTI_BITFIELD_BFM, false, 8, 15, 4, 7));
	bitfield_extract_expect_resume_rejection(test,
		encode_bitfield(true, 3, true, 8, 15, 4, 7));
	bitfield_extract_expect_resume_rejection(test,
		encode_extract(true, true, true, 5, 8, 4, 7));
	bitfield_extract_expect_resume_rejection(test,
		encode_extract(false, false, false, 5, 32, 4, 7));
}

static void bitfield_unary_production_path_leaves(struct kunit *test)
{
	static const struct {
		u8 rn;
		u8 rd;
		u64 value;
	} cases[] = {
		{ 4, 7, 0x8123456789abcdefULL },
		{ 7, 7, 0x8000000000000000ULL },
		{ 31, 8, 0xffffffffffffffffULL },
		{ 6, 31, 0x0000000000000001ULL },
	};
	size_t row, variant;

	for (row = 0; row < ARRAY_SIZE(dp1_source_rows); row++) {
		const struct scalar_source_row *source = &dp1_source_rows[row];
		bool cssc = source->op == ORLIX_TCTI_DP1_CTZ ||
			source->op == ORLIX_TCTI_DP1_CNT ||
			source->op == ORLIX_TCTI_DP1_ABS;

		for (variant = 0; variant < ARRAY_SIZE(cases); variant++) {
			u32 instruction = source->pattern |
				((u32)cases[variant].rn << 5) | cases[variant].rd;
			struct orlix_tcti_decoded_instruction decoded =
				orlix_tcti_decode_aarch64(instruction);
			struct pt_regs regs, before;
			struct orlix_tcti_result result;
			unsigned long mapped = bitfield_extract_map_rx(test, instruction);
			u8 width = decoded.is_64bit ? 64 : 32;
			u64 input;
			u64 expected;

			KUNIT_ASSERT_EQ_MSG(test, source->class,
				decoded.decode_class, "%s ordinal %u",
				source->leaf, source->ordinal);
			KUNIT_ASSERT_EQ(test, source->op, decoded.dp1_op);
			bitfield_extract_seed_resume_regs(&regs, mapped, instruction);
			if (cases[variant].rn != 31)
				regs.regs[cases[variant].rn] = cases[variant].value;
			before = regs;
			input = cases[variant].rn == 31 ? 0 :
				before.regs[cases[variant].rn];
			expected = scalar_dp1_expected(source->op,
				input & width_mask(decoded.is_64bit), width) &
				width_mask(decoded.is_64bit);
			result = cssc ?
				orlix_tcti_resume_user_with_feature_profile_for_tests(
					current, &regs, current->mm, &cssc_test_profile) :
				orlix_tcti_resume_user(current, &regs, current->mm);
			KUNIT_ASSERT_EQ_MSG(test, ORLIX_TCTI_EXIT_SYSCALL,
				result.reason, "%s ordinal %u", source->leaf,
				source->ordinal);
			KUNIT_EXPECT_EQ(test, 0L, result.status);
			KUNIT_EXPECT_EQ(test, mapped + sizeof(u32), result.pc);
			KUNIT_EXPECT_EQ(test, BITFIELD_EXTRACT_SVC,
				result.instruction);
			bitfield_extract_expect_resume_frame(test, &before, &regs,
				cases[variant].rd, expected, mapped);
			KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
		}
	}
}

static void bitfield_unary_reserved_rejection(struct kunit *test)
{
	/* REV32 is reserved in the 32-bit one-source encoding. */
	bitfield_extract_expect_resume_rejection(test,
		0x5ac00c00U | (4U << 5) | 7U);
}

static bool bitfield_unary_source_ordinal(u32 ordinal)
{
	return (ordinal >= 2169U && ordinal <= 2170U) ||
		(ordinal >= 2205U && ordinal <= 2210U) ||
		(ordinal >= 3389U && ordinal <= 3405U);
}

static bool bitfield_unary_producer_capture_rows_present(void)
{
	const struct orlix_tcti_native_proof_registry_entry *entries;
	size_t count;
	size_t index;

	entries = orlix_tcti_native_proof_registry_entries(&count);
	for (index = 0; entries && index < count; index++) {
		const struct orlix_tcti_native_proof_registry_entry *entry =
			&entries[index];

		if (entry->production_capture &&
			entry->source.subject_kind ==
				ORLIX_TCTI_NATIVE_SUBJECT_SOURCE_LEAF &&
			bitfield_unary_source_ordinal(entry->source.source_ordinal) &&
			!strcmp(entry->kunit_source, BITFIELD_UNARY_SOURCE_PATH))
			return true;
	}
	return false;
}

static void bitfield_unary_typed_proof_ingestion_unavailable(
	struct kunit *test)
{
	if (bitfield_unary_producer_capture_rows_present()) {
		KUNIT_FAIL(test,
			"#129 producer-capture rows appeared without a reviewed migration");
		return;
	}
	kunit_skip(test,
		"current #120 native registry lacks #129 producer-capture rows");
}

static void bitfield_unary_base_typed_proof_ingestion(struct kunit *test)
{
	bitfield_unary_typed_proof_ingestion_unavailable(test);
}

static void bitfield_unary_cssc_typed_proof_ingestion(struct kunit *test)
{
	bitfield_unary_typed_proof_ingestion_unavailable(test);
}

static void bitfield_unary_linux_disposition(struct kunit *test)
{
	const struct orlix_tcti_target_linux_proof_disposition_row *rows;
	size_t count;
	size_t index;
	size_t matched = 0;

	rows = orlix_tcti_target_linux_proof_dispositions(&count);
	KUNIT_ASSERT_NOT_NULL(test, rows);
	for (index = 0; index < count; index++) {
		u32 ordinal = rows[index].source.source_index;
		bool selected = (ordinal >= 2169U && ordinal <= 2170U) ||
			(ordinal >= 2205U && ordinal <= 2210U) ||
			(ordinal >= 3389U && ordinal <= 3405U);

		if (rows[index].subject_kind !=
		    ORLIX_TCTI_TARGET_LINUX_PROOF_SOURCE_LEAF || !selected)
			continue;
		matched++;
		KUNIT_EXPECT_EQ(test,
			ORLIX_TCTI_TARGET_LINUX_PROOF_NOT_APPLICABLE,
			rows[index].disposition);
		KUNIT_EXPECT_EQ(test,
			ORLIX_TCTI_TARGET_LINUX_NA_ARCHITECTURAL_SEMANTICS_ONLY,
			rows[index].not_applicable_reason);
		KUNIT_EXPECT_EQ(test, 0U, rows[index].linux_owner_mask);
		KUNIT_EXPECT_EQ(test, 0U, rows[index].kselftest_count);
		KUNIT_EXPECT_EQ(test,
			ORLIX_TCTI_TARGET_LINUX_EXECUTION_NOT_OBSERVED,
			rows[index].execution_state);
	}
	KUNIT_EXPECT_EQ(test, 25U, matched);
}

static struct kunit_case source_bound_cases[] = {
	KUNIT_CASE(source_fingerprints),
	KUNIT_CASE(scalar_source_fingerprints),
	KUNIT_CASE(bitfield_unary_exact_cohort),
	KUNIT_CASE(scalar_source_execution),
	KUNIT_CASE(exhaustive_bitfield_decode),
	KUNIT_CASE(exhaustive_extract_decode),
	KUNIT_CASE(exhaustive_bitfield_semantics),
	KUNIT_CASE(exhaustive_extract_semantics),
	KUNIT_CASE(bitfield_alias_state),
	KUNIT_CASE(bitfield_alias_matrix_and_canonical_relations),
	KUNIT_CASE(extract_ror_alias_state),
	KUNIT_CASE(extract_overlap_xzr_state),
	KUNIT_CASE(bitfield_extract_production_path_leaves),
	KUNIT_CASE(bitfield_extract_production_path_aliases),
	KUNIT_CASE(bitfield_extract_production_path_extract_overlaps),
	KUNIT_CASE(bitfield_extract_production_path_reserved_rejection),
	KUNIT_CASE(bitfield_unary_production_path_leaves),
	KUNIT_CASE(bitfield_unary_reserved_rejection),
	KUNIT_CASE(bitfield_unary_base_typed_proof_ingestion),
	KUNIT_CASE(bitfield_unary_cssc_typed_proof_ingestion),
	KUNIT_CASE(bitfield_unary_linux_disposition),
	{}
};

struct kunit_suite orlix_tcti_bitfield_extract_source_bound_test_suite = {
	.name = "orlix-tcti-bitfield-extract-source-bound",
	.test_cases = source_bound_cases,
};

kunit_test_suite(orlix_tcti_bitfield_extract_source_bound_test_suite);
