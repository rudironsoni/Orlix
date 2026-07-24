// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/bitops.h>
#include <linux/limits.h>
#include <asm/ptrace.h>

#include "../decode_aarch64.h"
#include "../switch_debug.h"
#include "tcti_test_suites.h"

#define CSSC_MIN_MAX_IMMEDIATE_COMMON_MASK 0x7ff00000U
#define CSSC_MIN_MAX_IMMEDIATE_COMMON_PATTERN 0x11c00000U

struct tcti_cssc_min_max_immediate_leaf {
	const char *source_name;
	u32 source_mask;
	u32 source_pattern;
	enum tcti_min_max_immediate_op op;
	bool is_64bit;
};

/*
 * Fingerprints from the pinned AARCHMRS 2026-06 source manifest. Every
 * source row conditioned on FEAT_CSSC appears exactly once in this table.
 *
 * Semantics are from Arm ARM DDI 0602, 2026-06, Base Instructions,
 * "SMAX (immediate)", "SMIN (immediate)", "UMAX (immediate)", and
 * "UMIN (immediate)": signed forms use SInt(imm8), unsigned forms use
 * UInt(imm8), and neither form changes PSTATE.
 */
static const struct tcti_cssc_min_max_immediate_leaf
tcti_cssc_min_max_immediate_leaves[] = {
	{ "SMAX_32_minmax_imm", 0xfffc0000U, 0x11c00000U,
	  TCTI_MIN_MAX_IMMEDIATE_SMAX, false },
	{ "UMAX_32U_minmax_imm", 0xfffc0000U, 0x11c40000U,
	  TCTI_MIN_MAX_IMMEDIATE_UMAX, false },
	{ "SMIN_32_minmax_imm", 0xfffc0000U, 0x11c80000U,
	  TCTI_MIN_MAX_IMMEDIATE_SMIN, false },
	{ "UMIN_32U_minmax_imm", 0xfffc0000U, 0x11cc0000U,
	  TCTI_MIN_MAX_IMMEDIATE_UMIN, false },
	{ "SMAX_64_minmax_imm", 0xfffc0000U, 0x91c00000U,
	  TCTI_MIN_MAX_IMMEDIATE_SMAX, true },
	{ "UMAX_64U_minmax_imm", 0xfffc0000U, 0x91c40000U,
	  TCTI_MIN_MAX_IMMEDIATE_UMAX, true },
	{ "SMIN_64_minmax_imm", 0xfffc0000U, 0x91c80000U,
	  TCTI_MIN_MAX_IMMEDIATE_SMIN, true },
	{ "UMIN_64U_minmax_imm", 0xfffc0000U, 0x91cc0000U,
	  TCTI_MIN_MAX_IMMEDIATE_UMIN, true },
};

static u32 tcti_cssc_min_max_immediate_instruction(
	const struct tcti_cssc_min_max_immediate_leaf *leaf, u8 immediate,
	u8 rn, u8 rd)
{
	return leaf->source_pattern | ((u32)immediate << 10) |
		((u32)rn << 5) | rd;
}

static u64 tcti_cssc_min_max_immediate_expected(
	const struct tcti_cssc_min_max_immediate_leaf *leaf, u64 source,
	u8 immediate)
{
	if (leaf->is_64bit) {
		s64 signed_source = source;
		s64 signed_immediate = (s8)immediate;

		switch (leaf->op) {
		case TCTI_MIN_MAX_IMMEDIATE_SMAX:
			return signed_source > signed_immediate ? source :
				(u64)signed_immediate;
		case TCTI_MIN_MAX_IMMEDIATE_UMAX:
			return source > immediate ? source : immediate;
		case TCTI_MIN_MAX_IMMEDIATE_SMIN:
			return signed_source < signed_immediate ? source :
				(u64)signed_immediate;
		case TCTI_MIN_MAX_IMMEDIATE_UMIN:
			return source < immediate ? source : immediate;
		}
	}

	{
		u32 source32 = source;
		s32 signed_source = source32;
		s32 signed_immediate = (s8)immediate;

		switch (leaf->op) {
		case TCTI_MIN_MAX_IMMEDIATE_SMAX:
			return signed_source > signed_immediate ? source32 :
				(u32)signed_immediate;
		case TCTI_MIN_MAX_IMMEDIATE_UMAX:
			return source32 > immediate ? source32 : immediate;
		case TCTI_MIN_MAX_IMMEDIATE_SMIN:
			return signed_source < signed_immediate ? source32 :
				(u32)signed_immediate;
		case TCTI_MIN_MAX_IMMEDIATE_UMIN:
			return source32 < immediate ? source32 : immediate;
		}
	}

	return 0;
}

static size_t tcti_cssc_min_max_immediate_sources(
	const struct tcti_cssc_min_max_immediate_leaf *leaf, u8 immediate,
	u64 values[6])
{
	if (leaf->op == TCTI_MIN_MAX_IMMEDIATE_SMAX ||
	    leaf->op == TCTI_MIN_MAX_IMMEDIATE_SMIN) {
		s64 signed_immediate = (s8)immediate;

		values[0] = leaf->is_64bit ? (u64)S64_MIN : (u32)S32_MIN;
		values[1] = leaf->is_64bit ? (u64)(signed_immediate - 1) :
			(u32)(s32)(signed_immediate - 1);
		values[2] = leaf->is_64bit ? (u64)signed_immediate :
			(u32)(s32)signed_immediate;
		values[3] = leaf->is_64bit ? (u64)(signed_immediate + 1) :
			(u32)(s32)(signed_immediate + 1);
		values[4] = leaf->is_64bit ? (u64)S64_MAX : (u32)S32_MAX;
		values[5] = leaf->is_64bit ? U64_MAX : U32_MAX;
		return 6;
	}

	values[0] = 0;
	values[1] = immediate ? immediate - 1 : 0;
	values[2] = immediate;
	values[3] = immediate + 1;
	values[4] = 255;
	values[5] = leaf->is_64bit ? U64_MAX : U32_MAX;
	return 6;
}

static void tcti_cssc_min_max_immediate_source_fingerprints(struct kunit *test)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(tcti_cssc_min_max_immediate_leaves); i++) {
		const struct tcti_cssc_min_max_immediate_leaf *leaf =
			&tcti_cssc_min_max_immediate_leaves[i];
		u32 instruction = tcti_cssc_min_max_immediate_instruction(
			leaf, 0x80, 2, 1);
		struct tcti_decoded_instruction decoded =
			tcti_decode_aarch64(instruction);

		KUNIT_EXPECT_EQ_MSG(test, leaf->source_pattern,
				    leaf->source_pattern & leaf->source_mask,
				    "%s source fingerprint", leaf->source_name);
		KUNIT_ASSERT_EQ_MSG(test, TCTI_DECODE_MIN_MAX_IMMEDIATE,
				    decoded.decode_class, "%s %#x", leaf->source_name,
				    instruction);
		KUNIT_EXPECT_EQ(test, leaf->op, decoded.min_max_immediate_op);
		KUNIT_EXPECT_EQ(test, leaf->is_64bit, decoded.is_64bit);
		KUNIT_EXPECT_EQ(test, 0x80, decoded.min_max_immediate);
		KUNIT_EXPECT_EQ(test, 2, decoded.rn);
		KUNIT_EXPECT_EQ(test, 1, decoded.rd);
	}
}

static void tcti_cssc_min_max_immediate_all_legal_fields(struct kunit *test)
{
	size_t leaf_index;
	u16 immediate;
	u8 rn;
	u8 rd;

	for (leaf_index = 0;
	     leaf_index < ARRAY_SIZE(tcti_cssc_min_max_immediate_leaves);
	     leaf_index++) {
		const struct tcti_cssc_min_max_immediate_leaf *leaf =
			&tcti_cssc_min_max_immediate_leaves[leaf_index];

		for (immediate = 0; immediate <= U8_MAX; immediate++) {
			for (rn = 0; rn < 32; rn++) {
				for (rd = 0; rd < 32; rd++) {
					u32 instruction =
						tcti_cssc_min_max_immediate_instruction(
							leaf, immediate, rn, rd);
					struct tcti_decoded_instruction decoded =
						tcti_decode_aarch64(instruction);

					KUNIT_ASSERT_EQ_MSG(test,
						TCTI_DECODE_MIN_MAX_IMMEDIATE,
						decoded.decode_class, "%s %#x",
						leaf->source_name, instruction);
					KUNIT_EXPECT_EQ(test, leaf->op,
						decoded.min_max_immediate_op);
					KUNIT_EXPECT_EQ(test, leaf->is_64bit,
						decoded.is_64bit);
					KUNIT_EXPECT_EQ(test, immediate,
						decoded.min_max_immediate);
					KUNIT_EXPECT_EQ(test, rn, decoded.rn);
					KUNIT_EXPECT_EQ(test, rd, decoded.rd);
				}
			}
		}
	}
}

static void tcti_cssc_min_max_immediate_execute_all_immediates(
	struct kunit *test)
{
	size_t leaf_index;
	u16 immediate;

	for (leaf_index = 0;
	     leaf_index < ARRAY_SIZE(tcti_cssc_min_max_immediate_leaves);
	     leaf_index++) {
		const struct tcti_cssc_min_max_immediate_leaf *leaf =
			&tcti_cssc_min_max_immediate_leaves[leaf_index];
		size_t source_index;

		for (immediate = 0; immediate <= U8_MAX; immediate++) {
			u64 sources[6];
			size_t source_count = tcti_cssc_min_max_immediate_sources(
				leaf, immediate, sources);

			for (source_index = 0; source_index < source_count;
			     source_index++) {
				struct pt_regs regs = {};
				struct tcti_decoded_instruction decoded =
					tcti_decode_aarch64(
						tcti_cssc_min_max_immediate_instruction(
							leaf, immediate, 2, 1));
				u64 pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_C_BIT |
					PSR_V_BIT;
				u64 expected = tcti_cssc_min_max_immediate_expected(
					leaf, sources[source_index], immediate);
				int ret;

				regs.regs[2] = sources[source_index];
				regs.regs[1] = U64_MAX;
				regs.pstate = pstate;
				regs.pc = 0x1000;
				ret = tcti_switch_debug_execute_decoded(NULL, &regs,
								      &decoded, NULL);
				KUNIT_ASSERT_EQ_MSG(test, 0, ret, "%s imm %#x",
						     leaf->source_name, immediate);
				KUNIT_EXPECT_EQ(test, expected, regs.regs[1]);
				KUNIT_EXPECT_EQ(test, pstate, regs.pstate);
				KUNIT_EXPECT_EQ(test, 0x1004ULL, regs.pc);
			}
		}
	}
}

static void tcti_cssc_min_max_immediate_zero_registers_and_pstate(
	struct kunit *test)
{
	size_t leaf_index;

	for (leaf_index = 0;
	     leaf_index < ARRAY_SIZE(tcti_cssc_min_max_immediate_leaves);
	     leaf_index++) {
		const struct tcti_cssc_min_max_immediate_leaf *leaf =
			&tcti_cssc_min_max_immediate_leaves[leaf_index];
		struct pt_regs regs = {};
		struct tcti_decoded_instruction decoded = tcti_decode_aarch64(
			tcti_cssc_min_max_immediate_instruction(leaf, 0x80, 31, 31));
		u64 pstate = PSR_MODE_EL0t | PSR_Z_BIT | PSR_C_BIT;
		int ret;

		regs.regs[0] = U64_MAX;
		regs.pstate = pstate;
		regs.pc = 0x2000;
		ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded,
							      NULL);
		KUNIT_ASSERT_EQ(test, 0, ret);
		KUNIT_EXPECT_EQ(test, pstate, regs.pstate);
		KUNIT_EXPECT_EQ(test, 0x2004ULL, regs.pc);
		KUNIT_EXPECT_EQ(test, U64_MAX, regs.regs[0]);
	}
}

static void tcti_cssc_min_max_immediate_rejects_non_cssc_encodings(
	struct kunit *test)
{
	static const u32 instructions[] = {
		0x11d00041U,
		0x11e00041U,
		0x11f00041U,
		0x91d00041U,
		0x91e00041U,
		0x91f00041U,
	};
	size_t i;

	for (i = 0; i < ARRAY_SIZE(instructions); i++) {
		struct tcti_decoded_instruction decoded =
			tcti_decode_aarch64(instructions[i]);

		KUNIT_EXPECT_NE_MSG(test, TCTI_DECODE_MIN_MAX_IMMEDIATE,
				    decoded.decode_class, "instruction %#x",
				    instructions[i]);
	}
}

static struct kunit_case tcti_cssc_min_max_immediate_test_cases[] = {
	KUNIT_CASE(tcti_cssc_min_max_immediate_source_fingerprints),
	KUNIT_CASE(tcti_cssc_min_max_immediate_all_legal_fields),
	KUNIT_CASE(tcti_cssc_min_max_immediate_execute_all_immediates),
	KUNIT_CASE(tcti_cssc_min_max_immediate_zero_registers_and_pstate),
	KUNIT_CASE(tcti_cssc_min_max_immediate_rejects_non_cssc_encodings),
	{}
};

struct kunit_suite tcti_cssc_min_max_immediate_test_suite = {
	.name = "orlix-tcti-cssc-min-max-immediate",
	.test_cases = tcti_cssc_min_max_immediate_test_cases,
};

kunit_test_suite(tcti_cssc_min_max_immediate_test_suite);
