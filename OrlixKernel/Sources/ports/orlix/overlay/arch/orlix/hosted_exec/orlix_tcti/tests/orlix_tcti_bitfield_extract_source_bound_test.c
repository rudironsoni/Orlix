// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/bitops.h>
#include <linux/limits.h>
#include <linux/string.h>
#include <asm/ptrace.h>
#include <asm/orlix_tcti.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/syscalls.h>

#include "target_instruction_artifact.h"
#include "../decode_aarch64.h"
#include "../fixed_integer.h"
#include "../switch_debug.h"

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
	u32 mask;
	u32 pattern;
	enum orlix_tcti_decode_class class;
	union {
		enum orlix_tcti_data_processing_1source_op dp1;
		enum orlix_tcti_data_processing_2source_op dp2;
	} op;
};

static const struct scalar_source_row dp1_source_rows[] = {
	{ 3389U, "RBIT_32_dp_1src", 0xfffffc00U, 0x5ac00000U,
	  ORLIX_TCTI_DECODE_DATA_PROCESSING_1SOURCE, { .dp1 = ORLIX_TCTI_DP1_RBIT } },
	{ 3390U, "REV16_32_dp_1src", 0xfffffc00U, 0x5ac00400U,
	  ORLIX_TCTI_DECODE_DATA_PROCESSING_1SOURCE, { .dp1 = ORLIX_TCTI_DP1_REV16 } },
	{ 3391U, "REV_32_dp_1src", 0xfffffc00U, 0x5ac00800U,
	  ORLIX_TCTI_DECODE_DATA_PROCESSING_1SOURCE, { .dp1 = ORLIX_TCTI_DP1_REV } },
	{ 3392U, "CLZ_32_dp_1src", 0xfffffc00U, 0x5ac01000U,
	  ORLIX_TCTI_DECODE_DATA_PROCESSING_1SOURCE, { .dp1 = ORLIX_TCTI_DP1_CLZ } },
	{ 3393U, "CLS_32_dp_1src", 0xfffffc00U, 0x5ac01400U,
	  ORLIX_TCTI_DECODE_DATA_PROCESSING_1SOURCE, { .dp1 = ORLIX_TCTI_DP1_CLS } },
	{ 3397U, "RBIT_64_dp_1src", 0xfffffc00U, 0xdac00000U,
	  ORLIX_TCTI_DECODE_DATA_PROCESSING_1SOURCE, { .dp1 = ORLIX_TCTI_DP1_RBIT } },
	{ 3398U, "REV16_64_dp_1src", 0xfffffc00U, 0xdac00400U,
	  ORLIX_TCTI_DECODE_DATA_PROCESSING_1SOURCE, { .dp1 = ORLIX_TCTI_DP1_REV16 } },
	{ 3399U, "REV32_64_dp_1src", 0xfffffc00U, 0xdac00800U,
	  ORLIX_TCTI_DECODE_DATA_PROCESSING_1SOURCE, { .dp1 = ORLIX_TCTI_DP1_REV32 } },
	{ 3400U, "REV_64_dp_1src", 0xfffffc00U, 0xdac00c00U,
	  ORLIX_TCTI_DECODE_DATA_PROCESSING_1SOURCE, { .dp1 = ORLIX_TCTI_DP1_REV } },
	{ 3401U, "CLZ_64_dp_1src", 0xfffffc00U, 0xdac01000U,
	  ORLIX_TCTI_DECODE_DATA_PROCESSING_1SOURCE, { .dp1 = ORLIX_TCTI_DP1_CLZ } },
	{ 3402U, "CLS_64_dp_1src", 0xfffffc00U, 0xdac01400U,
	  ORLIX_TCTI_DECODE_DATA_PROCESSING_1SOURCE, { .dp1 = ORLIX_TCTI_DP1_CLS } },
};

static const struct scalar_source_row dp2_source_rows[] = {
	{ 3356U, "UDIV_32_dp_2src", 0xffe0fc00U, 0x1ac00800U,
	  ORLIX_TCTI_DECODE_DATA_PROCESSING_2SOURCE, { .dp2 = ORLIX_TCTI_DP2_UDIV } },
	{ 3357U, "SDIV_32_dp_2src", 0xffe0fc00U, 0x1ac00c00U,
	  ORLIX_TCTI_DECODE_DATA_PROCESSING_2SOURCE, { .dp2 = ORLIX_TCTI_DP2_SDIV } },
	{ 3358U, "LSLV_32_dp_2src", 0xffe0fc00U, 0x1ac02000U,
	  ORLIX_TCTI_DECODE_DATA_PROCESSING_2SOURCE, { .dp2 = ORLIX_TCTI_DP2_LSLV } },
	{ 3359U, "LSRV_32_dp_2src", 0xffe0fc00U, 0x1ac02400U,
	  ORLIX_TCTI_DECODE_DATA_PROCESSING_2SOURCE, { .dp2 = ORLIX_TCTI_DP2_LSRV } },
	{ 3360U, "ASRV_32_dp_2src", 0xffe0fc00U, 0x1ac02800U,
	  ORLIX_TCTI_DECODE_DATA_PROCESSING_2SOURCE, { .dp2 = ORLIX_TCTI_DP2_ASRV } },
	{ 3361U, "RORV_32_dp_2src", 0xffe0fc00U, 0x1ac02c00U,
	  ORLIX_TCTI_DECODE_DATA_PROCESSING_2SOURCE, { .dp2 = ORLIX_TCTI_DP2_RORV } },
	{ 3373U, "UDIV_64_dp_2src", 0xffe0fc00U, 0x9ac00800U,
	  ORLIX_TCTI_DECODE_DATA_PROCESSING_2SOURCE, { .dp2 = ORLIX_TCTI_DP2_UDIV } },
	{ 3374U, "SDIV_64_dp_2src", 0xffe0fc00U, 0x9ac00c00U,
	  ORLIX_TCTI_DECODE_DATA_PROCESSING_2SOURCE, { .dp2 = ORLIX_TCTI_DP2_SDIV } },
	{ 3377U, "LSLV_64_dp_2src", 0xffe0fc00U, 0x9ac02000U,
	  ORLIX_TCTI_DECODE_DATA_PROCESSING_2SOURCE, { .dp2 = ORLIX_TCTI_DP2_LSLV } },
	{ 3378U, "LSRV_64_dp_2src", 0xffe0fc00U, 0x9ac02400U,
	  ORLIX_TCTI_DECODE_DATA_PROCESSING_2SOURCE, { .dp2 = ORLIX_TCTI_DP2_LSRV } },
	{ 3379U, "ASRV_64_dp_2src", 0xffe0fc00U, 0x9ac02800U,
	  ORLIX_TCTI_DECODE_DATA_PROCESSING_2SOURCE, { .dp2 = ORLIX_TCTI_DP2_ASRV } },
	{ 3380U, "RORV_64_dp_2src", 0xffe0fc00U, 0x9ac02c00U,
	  ORLIX_TCTI_DECODE_DATA_PROCESSING_2SOURCE, { .dp2 = ORLIX_TCTI_DP2_RORV } },
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
	{ "ASR32", false, 0, 5, 31, 4, 7 },
	{ "ASR64", true, 0, 9, 63, 4, 7 },
	{ "SBFIZ", true, 0, 56, 7, 4, 7 },
	{ "SBFX", true, 0, 8, 15, 4, 7 },
	{ "SXTB", false, 0, 0, 7, 4, 7 },
	{ "SXTH", true, 0, 0, 15, 4, 7 },
	{ "SXTW", true, 0, 0, 31, 4, 7 },
	{ "BFC", true, 1, 56, 7, 31, 7 },
	{ "BFI", true, 1, 56, 7, 4, 7 },
	{ "BFI_OVERLAP", true, 1, 56, 7, 7, 7 },
	{ "BFXIL", true, 1, 8, 15, 4, 7 },
	{ "LSL", false, 2, 24, 23, 4, 7 },
	{ "LSR", true, 2, 9, 63, 4, 7 },
	{ "UBFIZ", true, 2, 56, 7, 4, 7 },
	{ "UBFX", true, 2, 8, 15, 4, 7 },
	{ "UXTB", false, 2, 0, 7, 4, 7 },
	{ "UXTH", false, 2, 0, 15, 4, 7 },
	{ "UBFX_XZR_SOURCE", true, 2, 8, 15, 31, 7 },
	{ "SBFX_XZR_DEST", true, 0, 8, 15, 4, 31 },
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
	size_t i;

	for (i = 0; i < ARRAY_SIZE(dp1_source_rows); i++) {
		const struct scalar_source_row *row = &dp1_source_rows[i];
		struct orlix_tcti_decoded_instruction decoded =
			orlix_tcti_decode_aarch64(row->pattern);

		KUNIT_EXPECT_EQ_MSG(test, row->pattern, row->pattern & row->mask,
				    "%s source ordinal %u", row->leaf,
				    row->ordinal);
		KUNIT_ASSERT_EQ_MSG(test, row->class, decoded.decode_class,
				    "%s source ordinal %u", row->leaf,
				    row->ordinal);
		KUNIT_EXPECT_EQ_MSG(test, row->op.dp1, decoded.dp1_op,
				    "%s source ordinal %u", row->leaf,
				    row->ordinal);
	}

	for (i = 0; i < ARRAY_SIZE(dp2_source_rows); i++) {
		const struct scalar_source_row *row = &dp2_source_rows[i];
		struct orlix_tcti_decoded_instruction decoded =
			orlix_tcti_decode_aarch64(row->pattern);

		KUNIT_EXPECT_EQ_MSG(test, row->pattern, row->pattern & row->mask,
				    "%s source ordinal %u", row->leaf,
				    row->ordinal);
		KUNIT_ASSERT_EQ_MSG(test, row->class, decoded.decode_class,
				    "%s source ordinal %u", row->leaf,
				    row->ordinal);
		KUNIT_EXPECT_EQ_MSG(test, row->op.dp2, decoded.dp2_op,
				    "%s source ordinal %u", row->leaf,
				    row->ordinal);
	}
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
	default:
		return 0;
	}
}

static u64 scalar_dp2_expected(enum orlix_tcti_data_processing_2source_op op,
			       bool is_64bit, u64 left, u64 right)
{
	u8 width = is_64bit ? 64 : 32;
	u64 mask = width_mask(is_64bit);
	u8 amount;

	left &= mask;
	right &= mask;
	amount = right & (width - 1);
	switch (op) {
	case ORLIX_TCTI_DP2_UDIV:
		return right ? left / right : 0;
	case ORLIX_TCTI_DP2_SDIV:
		if (!right)
			return 0;
		if (is_64bit)
			return (s64)left == S64_MIN && (s64)right == -1 ?
				left : (u64)((s64)left / (s64)right);
		return (s32)(u32)left == S32_MIN && (s32)(u32)right == -1 ?
			(u32)left : (u32)((s32)(u32)left / (s32)(u32)right);
	case ORLIX_TCTI_DP2_LSLV:
		return left << amount;
	case ORLIX_TCTI_DP2_LSRV:
		return left >> amount;
	case ORLIX_TCTI_DP2_ASRV:
		return is_64bit ? (u64)((s64)left >> amount) :
			(u32)((s32)(u32)left >> amount);
	case ORLIX_TCTI_DP2_RORV:
		return amount ? (left >> amount) |
			(left << (width - amount)) : left;
	default:
		return 0;
	}
}

static void scalar_source_execution(struct kunit *test)
{
	const u8 rd = 7;
	const u8 rn = 19;
	const u8 rm = 11;
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
		expected = scalar_dp1_expected(decoded.dp1_op,
			before.regs[rn] & width_mask(decoded.is_64bit), width);
		KUNIT_ASSERT_EQ_MSG(test, 0,
			orlix_tcti_fixed_integer_execute(&regs, &decoded),
			"%s source ordinal %u", row->leaf, row->ordinal);
		expect_state(test, &before, &regs, rd,
			     expected & width_mask(decoded.is_64bit));
	}

	for (i = 0; i < ARRAY_SIZE(dp2_source_rows); i++) {
		const struct scalar_source_row *row = &dp2_source_rows[i];
		u32 instruction = row->pattern | ((u32)rm << 16) |
			((u32)rn << 5) | rd;
		struct orlix_tcti_decoded_instruction decoded =
			orlix_tcti_decode_aarch64(instruction);
		struct pt_regs regs, before;
		u64 expected;

		seed_regs(&regs, instruction);
		regs.regs[rn] = 0x8123456789abcdefULL;
		regs.regs[rm] = 35;
		before = regs;
		expected = scalar_dp2_expected(decoded.dp2_op, decoded.is_64bit,
			before.regs[rn], before.regs[rm]);
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
		ret = orlix_tcti_fixed_integer_execute(&regs, &decoded);
		KUNIT_ASSERT_EQ_MSG(test, 0, ret, "%s", alias->name);
		expect_state(test, &before, &regs, alias->rd, expected);
	}
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
			ret = orlix_tcti_fixed_integer_execute(&regs, &decoded);
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
		ret = orlix_tcti_fixed_integer_execute(&regs, &decoded);
		KUNIT_ASSERT_EQ(test, 0, ret);
		expect_state(test, &before, &regs, rd, expected);
	}
}

#define BITFIELD_EXTRACT_SVC 0xd4000001U

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
	static const struct {
		u16 ordinal;
		u8 immr;
		u8 imms;
		u8 rn;
		u8 rd;
	} cases[] = {
		{ 2205U, 5, 31, 4, 7 }, /* ASR */
		{ 2205U, 0, 7, 4, 7 },  /* SXTB */
		{ 2206U, 56, 7, 31, 7 }, /* BFC */
		{ 2206U, 56, 7, 7, 7 },  /* BFI overlap */
		{ 2207U, 24, 23, 4, 7 }, /* LSL */
		{ 2207U, 0, 7, 31, 7 },  /* UXTB XZR */
		{ 2208U, 8, 15, 4, 7 }, /* SBFX */
		{ 2209U, 8, 15, 4, 7 }, /* BFXIL */
		{ 2210U, 9, 63, 4, 7 }, /* LSR */
	};
	size_t index;

	for (index = 0; index < ARRAY_SIZE(cases); index++) {
		const struct source_row *row = NULL;
		size_t source;

		for (source = 0; source < ARRAY_SIZE(source_rows); source++)
			if (source_rows[source].ordinal == cases[index].ordinal)
				row = &source_rows[source];
		KUNIT_ASSERT_NOT_NULL(test, row);
		bitfield_extract_resume_one(test, row,
			encode_bitfield(cases[index].ordinal >= 2208U, row->op,
				cases[index].ordinal >= 2208U, cases[index].immr,
				cases[index].imms, cases[index].rn, cases[index].rd));
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

static struct kunit_case source_bound_cases[] = {
	KUNIT_CASE(source_fingerprints),
	KUNIT_CASE(scalar_source_fingerprints),
	KUNIT_CASE(scalar_source_execution),
	KUNIT_CASE(exhaustive_bitfield_decode),
	KUNIT_CASE(exhaustive_extract_decode),
	KUNIT_CASE(bitfield_alias_state),
	KUNIT_CASE(extract_ror_alias_state),
	KUNIT_CASE(extract_overlap_xzr_state),
	KUNIT_CASE(bitfield_extract_production_path_leaves),
	KUNIT_CASE(bitfield_extract_production_path_aliases),
	KUNIT_CASE(bitfield_extract_production_path_extract_overlaps),
	KUNIT_CASE(bitfield_extract_production_path_reserved_rejection),
	{}
};

struct kunit_suite orlix_tcti_bitfield_extract_source_bound_test_suite = {
	.name = "orlix-tcti-bitfield-extract-source-bound",
	.test_cases = source_bound_cases,
};

kunit_test_suite(orlix_tcti_bitfield_extract_source_bound_test_suite);
