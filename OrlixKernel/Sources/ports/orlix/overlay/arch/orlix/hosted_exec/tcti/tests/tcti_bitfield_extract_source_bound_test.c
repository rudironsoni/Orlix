// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/bitops.h>
#include <linux/limits.h>
#include <linux/string.h>
#include <asm/ptrace.h>

#include "../decode_aarch64.h"
#include "../switch_debug.h"

struct source_row {
	u32 ordinal;
	const char *leaf;
	u32 mask;
	u32 pattern;
	enum tcti_decode_class class;
	enum tcti_bitfield_op op;
};

static const struct source_row source_rows[] = {
	{ 2169U, "EXTR_32_extract", 0xffe08000U, 0x13800000U,
	  TCTI_DECODE_EXTRACT, 0 },
	{ 2170U, "EXTR_64_extract", 0xffe00000U, 0x93c00000U,
	  TCTI_DECODE_EXTRACT, 0 },
	{ 2205U, "SBFM_32M_bitfield", 0xffc00000U, 0x13000000U,
	  TCTI_DECODE_BITFIELD, TCTI_BITFIELD_SBFM },
	{ 2206U, "BFM_32M_bitfield", 0xffc00000U, 0x33000000U,
	  TCTI_DECODE_BITFIELD, TCTI_BITFIELD_BFM },
	{ 2207U, "UBFM_32M_bitfield", 0xffc00000U, 0x53000000U,
	  TCTI_DECODE_BITFIELD, TCTI_BITFIELD_UBFM },
	{ 2208U, "SBFM_64M_bitfield", 0xffc00000U, 0x93400000U,
	  TCTI_DECODE_BITFIELD, TCTI_BITFIELD_SBFM },
	{ 2209U, "BFM_64M_bitfield", 0xffc00000U, 0xb3400000U,
	  TCTI_DECODE_BITFIELD, TCTI_BITFIELD_BFM },
	{ 2210U, "UBFM_64M_bitfield", 0xffc00000U, 0xd3400000U,
	  TCTI_DECODE_BITFIELD, TCTI_BITFIELD_UBFM },
};

struct scalar_source_row {
	u32 ordinal;
	const char *leaf;
	u32 mask;
	u32 pattern;
	enum tcti_decode_class class;
	union {
		enum tcti_data_processing_1source_op dp1;
		enum tcti_data_processing_2source_op dp2;
	} op;
};

static const struct scalar_source_row dp1_source_rows[] = {
	{ 3389U, "RBIT_32_dp_1src", 0xfffffc00U, 0x5ac00000U,
	  TCTI_DECODE_DATA_PROCESSING_1SOURCE, { .dp1 = TCTI_DP1_RBIT } },
	{ 3390U, "REV16_32_dp_1src", 0xfffffc00U, 0x5ac00400U,
	  TCTI_DECODE_DATA_PROCESSING_1SOURCE, { .dp1 = TCTI_DP1_REV16 } },
	{ 3391U, "REV_32_dp_1src", 0xfffffc00U, 0x5ac00800U,
	  TCTI_DECODE_DATA_PROCESSING_1SOURCE, { .dp1 = TCTI_DP1_REV } },
	{ 3392U, "CLZ_32_dp_1src", 0xfffffc00U, 0x5ac01000U,
	  TCTI_DECODE_DATA_PROCESSING_1SOURCE, { .dp1 = TCTI_DP1_CLZ } },
	{ 3393U, "CLS_32_dp_1src", 0xfffffc00U, 0x5ac01400U,
	  TCTI_DECODE_DATA_PROCESSING_1SOURCE, { .dp1 = TCTI_DP1_CLS } },
	{ 3397U, "RBIT_64_dp_1src", 0xfffffc00U, 0xdac00000U,
	  TCTI_DECODE_DATA_PROCESSING_1SOURCE, { .dp1 = TCTI_DP1_RBIT } },
	{ 3398U, "REV16_64_dp_1src", 0xfffffc00U, 0xdac00400U,
	  TCTI_DECODE_DATA_PROCESSING_1SOURCE, { .dp1 = TCTI_DP1_REV16 } },
	{ 3399U, "REV32_64_dp_1src", 0xfffffc00U, 0xdac00800U,
	  TCTI_DECODE_DATA_PROCESSING_1SOURCE, { .dp1 = TCTI_DP1_REV32 } },
	{ 3400U, "REV_64_dp_1src", 0xfffffc00U, 0xdac00c00U,
	  TCTI_DECODE_DATA_PROCESSING_1SOURCE, { .dp1 = TCTI_DP1_REV } },
	{ 3401U, "CLZ_64_dp_1src", 0xfffffc00U, 0xdac01000U,
	  TCTI_DECODE_DATA_PROCESSING_1SOURCE, { .dp1 = TCTI_DP1_CLZ } },
	{ 3402U, "CLS_64_dp_1src", 0xfffffc00U, 0xdac01400U,
	  TCTI_DECODE_DATA_PROCESSING_1SOURCE, { .dp1 = TCTI_DP1_CLS } },
};

static const struct scalar_source_row dp2_source_rows[] = {
	{ 3356U, "UDIV_32_dp_2src", 0xffe0fc00U, 0x1ac00800U,
	  TCTI_DECODE_DATA_PROCESSING_2SOURCE, { .dp2 = TCTI_DP2_UDIV } },
	{ 3357U, "SDIV_32_dp_2src", 0xffe0fc00U, 0x1ac00c00U,
	  TCTI_DECODE_DATA_PROCESSING_2SOURCE, { .dp2 = TCTI_DP2_SDIV } },
	{ 3358U, "LSLV_32_dp_2src", 0xffe0fc00U, 0x1ac02000U,
	  TCTI_DECODE_DATA_PROCESSING_2SOURCE, { .dp2 = TCTI_DP2_LSLV } },
	{ 3359U, "LSRV_32_dp_2src", 0xffe0fc00U, 0x1ac02400U,
	  TCTI_DECODE_DATA_PROCESSING_2SOURCE, { .dp2 = TCTI_DP2_LSRV } },
	{ 3360U, "ASRV_32_dp_2src", 0xffe0fc00U, 0x1ac02800U,
	  TCTI_DECODE_DATA_PROCESSING_2SOURCE, { .dp2 = TCTI_DP2_ASRV } },
	{ 3361U, "RORV_32_dp_2src", 0xffe0fc00U, 0x1ac02c00U,
	  TCTI_DECODE_DATA_PROCESSING_2SOURCE, { .dp2 = TCTI_DP2_RORV } },
	{ 3373U, "UDIV_64_dp_2src", 0xffe0fc00U, 0x9ac00800U,
	  TCTI_DECODE_DATA_PROCESSING_2SOURCE, { .dp2 = TCTI_DP2_UDIV } },
	{ 3374U, "SDIV_64_dp_2src", 0xffe0fc00U, 0x9ac00c00U,
	  TCTI_DECODE_DATA_PROCESSING_2SOURCE, { .dp2 = TCTI_DP2_SDIV } },
	{ 3377U, "LSLV_64_dp_2src", 0xffe0fc00U, 0x9ac02000U,
	  TCTI_DECODE_DATA_PROCESSING_2SOURCE, { .dp2 = TCTI_DP2_LSLV } },
	{ 3378U, "LSRV_64_dp_2src", 0xffe0fc00U, 0x9ac02400U,
	  TCTI_DECODE_DATA_PROCESSING_2SOURCE, { .dp2 = TCTI_DP2_LSRV } },
	{ 3379U, "ASRV_64_dp_2src", 0xffe0fc00U, 0x9ac02800U,
	  TCTI_DECODE_DATA_PROCESSING_2SOURCE, { .dp2 = TCTI_DP2_ASRV } },
	{ 3380U, "RORV_64_dp_2src", 0xffe0fc00U, 0x9ac02c00U,
	  TCTI_DECODE_DATA_PROCESSING_2SOURCE, { .dp2 = TCTI_DP2_RORV } },
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

static u64 bitfield_expected(const struct tcti_decoded_instruction *decoded,
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
	if (decoded->bitfield_op == TCTI_BITFIELD_BFM)
		return ((destination & ~field_mask) | value) & mask;
	if (decoded->bitfield_op == TCTI_BITFIELD_SBFM &&
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

static void source_fingerprints(struct kunit *test)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(source_rows); i++) {
		struct tcti_decoded_instruction decoded =
			tcti_decode_aarch64(source_rows[i].pattern);

		KUNIT_EXPECT_EQ_MSG(test, source_rows[i].pattern,
				    source_rows[i].pattern & source_rows[i].mask,
				    "%s source ordinal %u", source_rows[i].leaf,
				    source_rows[i].ordinal);
		KUNIT_ASSERT_EQ_MSG(test, source_rows[i].class,
				    decoded.decode_class, "%s source ordinal %u",
				    source_rows[i].leaf, source_rows[i].ordinal);
		if (decoded.decode_class == TCTI_DECODE_BITFIELD)
			KUNIT_EXPECT_EQ_MSG(test, source_rows[i].op,
					    decoded.bitfield_op, "%s source ordinal %u",
					    source_rows[i].leaf,
					    source_rows[i].ordinal);
	}
}

static void scalar_source_fingerprints(struct kunit *test)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(dp1_source_rows); i++) {
		const struct scalar_source_row *row = &dp1_source_rows[i];
		struct tcti_decoded_instruction decoded =
			tcti_decode_aarch64(row->pattern);

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
		struct tcti_decoded_instruction decoded =
			tcti_decode_aarch64(row->pattern);

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

static u64 scalar_dp1_expected(enum tcti_data_processing_1source_op op,
			       u64 value, u8 width)
{
	switch (op) {
	case TCTI_DP1_RBIT:
		return scalar_reverse_bits(value, width);
	case TCTI_DP1_REV16:
		return scalar_reverse_bytes(value, width, 16);
	case TCTI_DP1_REV32:
		return scalar_reverse_bytes(value, width, 32);
	case TCTI_DP1_REV:
		return scalar_reverse_bytes(value, width, width);
	case TCTI_DP1_CLZ:
		return scalar_count_leading_zeros(value, width);
	case TCTI_DP1_CLS:
		return scalar_count_leading_sign_bits(value, width);
	default:
		return 0;
	}
}

static u64 scalar_dp2_expected(enum tcti_data_processing_2source_op op,
			       bool is_64bit, u64 left, u64 right)
{
	u8 width = is_64bit ? 64 : 32;
	u64 mask = width_mask(is_64bit);
	u8 amount;

	left &= mask;
	right &= mask;
	amount = right & (width - 1);
	switch (op) {
	case TCTI_DP2_UDIV:
		return right ? left / right : 0;
	case TCTI_DP2_SDIV:
		if (!right)
			return 0;
		if (is_64bit)
			return (s64)left == S64_MIN && (s64)right == -1 ?
				left : (u64)((s64)left / (s64)right);
		return (s32)(u32)left == S32_MIN && (s32)(u32)right == -1 ?
			(u32)left : (u32)((s32)(u32)left / (s32)(u32)right);
	case TCTI_DP2_LSLV:
		return left << amount;
	case TCTI_DP2_LSRV:
		return left >> amount;
	case TCTI_DP2_ASRV:
		return is_64bit ? (u64)((s64)left >> amount) :
			(u32)((s32)(u32)left >> amount);
	case TCTI_DP2_RORV:
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
		struct tcti_decoded_instruction decoded =
			tcti_decode_aarch64(instruction);
		struct pt_regs regs, before;
		u8 width = decoded.is_64bit ? 64 : 32;
		u64 expected;

		seed_regs(&regs, instruction);
		regs.regs[rn] = 0x8123456789abcdefULL;
		before = regs;
		expected = scalar_dp1_expected(decoded.dp1_op,
			before.regs[rn] & width_mask(decoded.is_64bit), width);
		KUNIT_ASSERT_EQ_MSG(test, 0,
			tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL),
			"%s source ordinal %u", row->leaf, row->ordinal);
		expect_state(test, &before, &regs, rd,
			     expected & width_mask(decoded.is_64bit));
	}

	for (i = 0; i < ARRAY_SIZE(dp2_source_rows); i++) {
		const struct scalar_source_row *row = &dp2_source_rows[i];
		u32 instruction = row->pattern | ((u32)rm << 16) |
			((u32)rn << 5) | rd;
		struct tcti_decoded_instruction decoded =
			tcti_decode_aarch64(instruction);
		struct pt_regs regs, before;
		u64 expected;

		seed_regs(&regs, instruction);
		regs.regs[rn] = 0x8123456789abcdefULL;
		regs.regs[rm] = 35;
		before = regs;
		expected = scalar_dp2_expected(decoded.dp2_op, decoded.is_64bit,
			before.regs[rn], before.regs[rm]);
		KUNIT_ASSERT_EQ_MSG(test, 0,
			tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL),
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
						struct tcti_decoded_instruction decoded =
							tcti_decode_aarch64(encode_bitfield(
								sf, opc, n, immr, imms, 4, 7));

						if (valid)
							KUNIT_EXPECT_EQ(test,
								TCTI_DECODE_BITFIELD,
								decoded.decode_class);
						else
							KUNIT_EXPECT_EQ(test,
								TCTI_DECODE_UNSUPPORTED,
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
					struct tcti_decoded_instruction decoded =
						tcti_decode_aarch64(encode_extract(
							sf, n, bit21, 5, shift, 4, 7));

					if (valid)
						KUNIT_EXPECT_EQ(test, TCTI_DECODE_EXTRACT,
								decoded.decode_class);
					else
						KUNIT_EXPECT_EQ(test, TCTI_DECODE_UNSUPPORTED,
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
		struct tcti_decoded_instruction decoded =
			tcti_decode_aarch64(instruction);
		struct pt_regs regs, before;
		u64 source, destination, expected;
		int ret;

		seed_regs(&regs, instruction);
		before = regs;
		source = alias->rn == 31 ? 0 : before.regs[alias->rn];
		destination = alias->rd == 31 ? 0 : before.regs[alias->rd];
		expected = bitfield_expected(&decoded, source, destination);
		ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);
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
			struct tcti_decoded_instruction decoded =
				tcti_decode_aarch64(instruction);
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
			ret = tcti_switch_debug_execute_decoded(
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
		struct tcti_decoded_instruction decoded =
			tcti_decode_aarch64(instruction);
		struct pt_regs regs, before;
		u64 high, low, expected;
		int ret;

		seed_regs(&regs, instruction);
		before = regs;
		high = rn == 31 ? 0 : before.regs[rn];
		low = rm == 31 ? 0 : before.regs[rm];
		expected = (low >> shift) | (high << (64 - shift));
		ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);
		KUNIT_ASSERT_EQ(test, 0, ret);
		expect_state(test, &before, &regs, rd, expected);
	}
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
	{}
};

struct kunit_suite tcti_bitfield_extract_source_bound_test_suite = {
	.name = "orlix-tcti-bitfield-extract-source-bound",
	.test_cases = source_bound_cases,
};

kunit_test_suite(tcti_bitfield_extract_source_bound_test_suite);
