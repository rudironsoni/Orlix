// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/bitops.h>
#include <linux/limits.h>
#include <linux/string.h>
#include <asm/ptrace.h>

#include "../decode_aarch64.h"
#include "../switch_debug.h"

struct source_row {
	u32 mask;
	u32 pattern;
	enum tcti_decode_class class;
	enum tcti_bitfield_op op;
};

static const struct source_row source_rows[] = {
	{ 0xffe08000U, 0x13800000U, TCTI_DECODE_EXTRACT, 0 },
	{ 0xffe00000U, 0x93c00000U, TCTI_DECODE_EXTRACT, 0 },
	{ 0xffc00000U, 0x13000000U, TCTI_DECODE_BITFIELD, TCTI_BITFIELD_SBFM },
	{ 0xffc00000U, 0x33000000U, TCTI_DECODE_BITFIELD, TCTI_BITFIELD_BFM },
	{ 0xffc00000U, 0x53000000U, TCTI_DECODE_BITFIELD, TCTI_BITFIELD_UBFM },
	{ 0xffc00000U, 0x93400000U, TCTI_DECODE_BITFIELD, TCTI_BITFIELD_SBFM },
	{ 0xffc00000U, 0xb3400000U, TCTI_DECODE_BITFIELD, TCTI_BITFIELD_BFM },
	{ 0xffc00000U, 0xd3400000U, TCTI_DECODE_BITFIELD, TCTI_BITFIELD_UBFM },
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

		KUNIT_EXPECT_EQ(test, source_rows[i].pattern,
				source_rows[i].pattern & source_rows[i].mask);
		KUNIT_ASSERT_EQ(test, source_rows[i].class, decoded.decode_class);
		if (decoded.decode_class == TCTI_DECODE_BITFIELD)
			KUNIT_EXPECT_EQ(test, source_rows[i].op,
					decoded.bitfield_op);
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
