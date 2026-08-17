// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/limits.h>
#include <linux/mm.h>
#include <linux/sched/mm.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/syscalls.h>
#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/orlix_tcti.h>
#include <asm/elf.h>

#include "../decode_aarch64.h"
#include "../gadget_program.h"
#include "../switch_debug.h"
#include "orlix_tcti_test_suites.h"

#define CSSC_MIN_MAX_IMMEDIATE_COMMON_MASK 0x7ff00000U
#define CSSC_MIN_MAX_IMMEDIATE_COMMON_PATTERN 0x11c00000U
#define CSSC_MIN_MAX_REGISTER_LEAF_COUNT 8U
#define CSSC_SVC 0xd4000001U

struct orlix_tcti_cssc_min_max_immediate_leaf {
	u32 ordinal;
	const char *source_name;
	u32 source_mask;
	u32 source_pattern;
	enum orlix_tcti_min_max_immediate_op op;
	bool is_64bit;
};

struct orlix_tcti_cssc_data_processing_leaf {
	u32 ordinal;
	const char *source_name;
	u32 source_mask;
	u32 source_pattern;
	bool is_64bit;
	bool two_source;
	enum orlix_tcti_data_processing_1source_op dp1_op;
	enum orlix_tcti_data_processing_2source_op dp2_op;
};

struct orlix_tcti_cssc_minmax_overlap_operands {
	u8 rd;
	u8 rn;
	u8 rm;
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
static const struct orlix_tcti_cssc_min_max_immediate_leaf
orlix_tcti_cssc_min_max_immediate_leaves[] = {
	{ 2183U, "SMAX_32_minmax_imm", 0xfffc0000U, 0x11c00000U,
	  ORLIX_TCTI_MIN_MAX_IMMEDIATE_SMAX, false },
	{ 2184U, "UMAX_32U_minmax_imm", 0xfffc0000U, 0x11c40000U,
	  ORLIX_TCTI_MIN_MAX_IMMEDIATE_UMAX, false },
	{ 2185U, "SMIN_32_minmax_imm", 0xfffc0000U, 0x11c80000U,
	  ORLIX_TCTI_MIN_MAX_IMMEDIATE_SMIN, false },
	{ 2186U, "UMIN_32U_minmax_imm", 0xfffc0000U, 0x11cc0000U,
	  ORLIX_TCTI_MIN_MAX_IMMEDIATE_UMIN, false },
	{ 2187U, "SMAX_64_minmax_imm", 0xfffc0000U, 0x91c00000U,
	  ORLIX_TCTI_MIN_MAX_IMMEDIATE_SMAX, true },
	{ 2188U, "UMAX_64U_minmax_imm", 0xfffc0000U, 0x91c40000U,
	  ORLIX_TCTI_MIN_MAX_IMMEDIATE_UMAX, true },
	{ 2189U, "SMIN_64_minmax_imm", 0xfffc0000U, 0x91c80000U,
	  ORLIX_TCTI_MIN_MAX_IMMEDIATE_SMIN, true },
	{ 2190U, "UMIN_64U_minmax_imm", 0xfffc0000U, 0x91cc0000U,
	  ORLIX_TCTI_MIN_MAX_IMMEDIATE_UMIN, true },
};

/*
 * Fingerprints from the pinned AARCHMRS 2026-06 source manifest. These
 * forms share the production data-processing decoders with baseline
 * instructions, but FEAT_CSSC owns the previously-reserved opcodes.
 */
static const struct orlix_tcti_cssc_data_processing_leaf
orlix_tcti_cssc_data_processing_leaves[] = {
	{ 3368U, "SMAX_32_dp_2src", 0xffe0fc00U, 0x1ac06000U, false, true,
	  0, ORLIX_TCTI_DP2_SMAX },
	{ 3369U, "UMAX_32_dp_2src", 0xffe0fc00U, 0x1ac06400U, false, true,
	  0, ORLIX_TCTI_DP2_UMAX },
	{ 3370U, "SMIN_32_dp_2src", 0xffe0fc00U, 0x1ac06800U, false, true,
	  0, ORLIX_TCTI_DP2_SMIN },
	{ 3371U, "UMIN_32_dp_2src", 0xffe0fc00U, 0x1ac06c00U, false, true,
	  0, ORLIX_TCTI_DP2_UMIN },
	{ 3384U, "SMAX_64_dp_2src", 0xffe0fc00U, 0x9ac06000U, true, true,
	  0, ORLIX_TCTI_DP2_SMAX },
	{ 3385U, "UMAX_64_dp_2src", 0xffe0fc00U, 0x9ac06400U, true, true,
	  0, ORLIX_TCTI_DP2_UMAX },
	{ 3386U, "SMIN_64_dp_2src", 0xffe0fc00U, 0x9ac06800U, true, true,
	  0, ORLIX_TCTI_DP2_SMIN },
	{ 3387U, "UMIN_64_dp_2src", 0xffe0fc00U, 0x9ac06c00U, true, true,
	  0, ORLIX_TCTI_DP2_UMIN },
	{ 3394U, "CTZ_32_dp_1src", 0xfffffc00U, 0x5ac01800U, false, false,
	  ORLIX_TCTI_DP1_CTZ, 0 },
	{ 3395U, "CNT_32_dp_1src", 0xfffffc00U, 0x5ac01c00U, false, false,
	  ORLIX_TCTI_DP1_CNT, 0 },
	{ 3396U, "ABS_32_dp_1src", 0xfffffc00U, 0x5ac02000U, false, false,
	  ORLIX_TCTI_DP1_ABS, 0 },
	{ 3403U, "CTZ_64_dp_1src", 0xfffffc00U, 0xdac01800U, true, false,
	  ORLIX_TCTI_DP1_CTZ, 0 },
	{ 3404U, "CNT_64_dp_1src", 0xfffffc00U, 0xdac01c00U, true, false,
	  ORLIX_TCTI_DP1_CNT, 0 },
	{ 3405U, "ABS_64_dp_1src", 0xfffffc00U, 0xdac02000U, true, false,
	  ORLIX_TCTI_DP1_ABS, 0 },
};

static u32 orlix_tcti_cssc_data_processing_instruction(
	const struct orlix_tcti_cssc_data_processing_leaf *leaf, u8 rm, u8 rn, u8 rd)
{
	return leaf->source_pattern | (leaf->two_source ? (u32)rm << 16 : 0) |
		((u32)rn << 5) | rd;
}

static u64 orlix_tcti_cssc_data_processing_expected(
	const struct orlix_tcti_cssc_data_processing_leaf *leaf, u64 left, u64 right)
{
	u64 mask = leaf->is_64bit ? U64_MAX : U32_MAX;

	left &= mask;
	right &= mask;
	if (leaf->two_source) {
		switch (leaf->dp2_op) {
		case ORLIX_TCTI_DP2_SMAX:
			return leaf->is_64bit ?
				((s64)left > (s64)right ? left : right) :
				((s32)(u32)left > (s32)(u32)right ? left : right);
		case ORLIX_TCTI_DP2_UMAX:
			return left > right ? left : right;
		case ORLIX_TCTI_DP2_SMIN:
			return leaf->is_64bit ?
				((s64)left < (s64)right ? left : right) :
				((s32)(u32)left < (s32)(u32)right ? left : right);
		case ORLIX_TCTI_DP2_UMIN:
			return left < right ? left : right;
		default:
			return 0;
		}
	}

	switch (leaf->dp1_op) {
	case ORLIX_TCTI_DP1_CTZ:
		return left ? (leaf->is_64bit ? __ffs64(left) : __ffs((u32)left)) :
			(leaf->is_64bit ? 64 : 32);
	case ORLIX_TCTI_DP1_CNT:
		return leaf->is_64bit ? hweight64(left) : hweight32(left);
	case ORLIX_TCTI_DP1_ABS:
		return left & BIT_ULL(leaf->is_64bit ? 63 : 31) ? (-left) & mask :
			left;
	default:
		return 0;
	}
}

static u32 orlix_tcti_cssc_min_max_immediate_instruction(
	const struct orlix_tcti_cssc_min_max_immediate_leaf *leaf, u8 immediate,
	u8 rn, u8 rd)
{
	return leaf->source_pattern | ((u32)immediate << 10) |
		((u32)rn << 5) | rd;
}

static int orlix_tcti_cssc_execute_fixed_gadget(
	struct pt_regs *regs, const struct orlix_tcti_decoded_instruction *decoded)
{
	struct orlix_tcti_gadget_word
		program[ORLIX_TCTI_SINGLE_INSTRUCTION_PROGRAM_WORDS];
	size_t word_count;
	int ret;

	ret = orlix_tcti_lower_decoded_instruction(decoded, program,
					     ARRAY_SIZE(program), &word_count);
	if (ret)
		return ret;
	return orlix_tcti_execute_gadget_program(NULL, regs, program, word_count,
						 NULL);
}

static void orlix_tcti_integer_minmax_exact_source_cohort(struct kunit *test)
{
	static const u32 expected_ordinals[] = {
		2183U, 2184U, 2185U, 2186U, 2187U, 2188U, 2189U, 2190U,
		3368U, 3369U, 3370U, 3371U, 3384U, 3385U, 3386U, 3387U,
	};
	size_t index;
	size_t other;

	KUNIT_ASSERT_EQ(test, CSSC_MIN_MAX_REGISTER_LEAF_COUNT,
			ARRAY_SIZE(orlix_tcti_cssc_min_max_immediate_leaves));
	KUNIT_ASSERT_EQ(test, CSSC_MIN_MAX_REGISTER_LEAF_COUNT,
			ARRAY_SIZE(orlix_tcti_cssc_data_processing_leaves) - 6);
	for (index = 0; index < ARRAY_SIZE(expected_ordinals); index++) {
		u32 ordinal = index < ARRAY_SIZE(orlix_tcti_cssc_min_max_immediate_leaves) ?
			orlix_tcti_cssc_min_max_immediate_leaves[index].ordinal :
			orlix_tcti_cssc_data_processing_leaves[index -
				ARRAY_SIZE(orlix_tcti_cssc_min_max_immediate_leaves)].ordinal;

		KUNIT_EXPECT_EQ(test, expected_ordinals[index], ordinal);
		for (other = 0; other < index; other++)
			KUNIT_EXPECT_NE(test, expected_ordinals[other], ordinal);
	}
}

static u64 orlix_tcti_cssc_min_max_immediate_expected(
	const struct orlix_tcti_cssc_min_max_immediate_leaf *leaf, u64 source,
	u8 immediate)
{
	if (leaf->is_64bit) {
		s64 signed_source = source;
		s64 signed_immediate = (s8)immediate;

		switch (leaf->op) {
		case ORLIX_TCTI_MIN_MAX_IMMEDIATE_SMAX:
			return signed_source > signed_immediate ? source :
				(u64)signed_immediate;
		case ORLIX_TCTI_MIN_MAX_IMMEDIATE_UMAX:
			return source > immediate ? source : immediate;
		case ORLIX_TCTI_MIN_MAX_IMMEDIATE_SMIN:
			return signed_source < signed_immediate ? source :
				(u64)signed_immediate;
		case ORLIX_TCTI_MIN_MAX_IMMEDIATE_UMIN:
			return source < immediate ? source : immediate;
		}
	}

	{
		u32 source32 = source;
		s32 signed_source = source32;
		s32 signed_immediate = (s8)immediate;

		switch (leaf->op) {
		case ORLIX_TCTI_MIN_MAX_IMMEDIATE_SMAX:
			return signed_source > signed_immediate ? source32 :
				(u32)signed_immediate;
		case ORLIX_TCTI_MIN_MAX_IMMEDIATE_UMAX:
			return source32 > immediate ? source32 : immediate;
		case ORLIX_TCTI_MIN_MAX_IMMEDIATE_SMIN:
			return signed_source < signed_immediate ? source32 :
				(u32)signed_immediate;
		case ORLIX_TCTI_MIN_MAX_IMMEDIATE_UMIN:
			return source32 < immediate ? source32 : immediate;
		}
	}

	return 0;
}

static size_t orlix_tcti_cssc_min_max_immediate_sources(
	const struct orlix_tcti_cssc_min_max_immediate_leaf *leaf, u8 immediate,
	u64 values[6])
{
	if (leaf->op == ORLIX_TCTI_MIN_MAX_IMMEDIATE_SMAX ||
	    leaf->op == ORLIX_TCTI_MIN_MAX_IMMEDIATE_SMIN) {
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

static void orlix_tcti_cssc_min_max_immediate_source_fingerprints(struct kunit *test)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(orlix_tcti_cssc_min_max_immediate_leaves); i++) {
		const struct orlix_tcti_cssc_min_max_immediate_leaf *leaf =
			&orlix_tcti_cssc_min_max_immediate_leaves[i];
		u32 instruction = orlix_tcti_cssc_min_max_immediate_instruction(
			leaf, 0x80, 2, 1);
		struct orlix_tcti_decoded_instruction decoded =
			orlix_tcti_decode_aarch64(instruction);

		KUNIT_EXPECT_EQ_MSG(test, leaf->source_pattern,
				    leaf->source_pattern & leaf->source_mask,
				    "%s source fingerprint", leaf->source_name);
		KUNIT_ASSERT_EQ_MSG(test, ORLIX_TCTI_DECODE_MIN_MAX_IMMEDIATE,
				    decoded.decode_class, "%s %#x", leaf->source_name,
				    instruction);
		KUNIT_EXPECT_EQ(test, leaf->op, decoded.min_max_immediate_op);
		KUNIT_EXPECT_EQ(test, leaf->is_64bit, decoded.is_64bit);
		KUNIT_EXPECT_EQ(test, 0x80, decoded.min_max_immediate);
		KUNIT_EXPECT_EQ(test, 2, decoded.rn);
		KUNIT_EXPECT_EQ(test, 1, decoded.rd);
	}
}

static void orlix_tcti_cssc_min_max_immediate_all_legal_fields(struct kunit *test)
{
	size_t leaf_index;
	u16 immediate;
	u8 rn;
	u8 rd;

	for (leaf_index = 0;
	     leaf_index < ARRAY_SIZE(orlix_tcti_cssc_min_max_immediate_leaves);
	     leaf_index++) {
		const struct orlix_tcti_cssc_min_max_immediate_leaf *leaf =
			&orlix_tcti_cssc_min_max_immediate_leaves[leaf_index];

		for (immediate = 0; immediate <= U8_MAX; immediate++) {
			for (rn = 0; rn < 32; rn++) {
				for (rd = 0; rd < 32; rd++) {
					u32 instruction =
						orlix_tcti_cssc_min_max_immediate_instruction(
							leaf, immediate, rn, rd);
					struct orlix_tcti_decoded_instruction decoded =
						orlix_tcti_decode_aarch64(instruction);

					KUNIT_ASSERT_EQ_MSG(test,
						ORLIX_TCTI_DECODE_MIN_MAX_IMMEDIATE,
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

static void orlix_tcti_cssc_min_max_immediate_execute_all_immediates(
	struct kunit *test)
{
	size_t leaf_index;
	u16 immediate;

	for (leaf_index = 0;
	     leaf_index < ARRAY_SIZE(orlix_tcti_cssc_min_max_immediate_leaves);
	     leaf_index++) {
		const struct orlix_tcti_cssc_min_max_immediate_leaf *leaf =
			&orlix_tcti_cssc_min_max_immediate_leaves[leaf_index];
		size_t source_index;

		for (immediate = 0; immediate <= U8_MAX; immediate++) {
			u64 sources[6];
			size_t source_count = orlix_tcti_cssc_min_max_immediate_sources(
				leaf, immediate, sources);

			for (source_index = 0; source_index < source_count;
			     source_index++) {
				struct pt_regs regs = {};
				struct orlix_tcti_decoded_instruction decoded =
					orlix_tcti_decode_aarch64(
						orlix_tcti_cssc_min_max_immediate_instruction(
							leaf, immediate, 2, 1));
				u64 pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_C_BIT |
					PSR_V_BIT;
				u64 expected = orlix_tcti_cssc_min_max_immediate_expected(
					leaf, sources[source_index], immediate);
				int ret;

				regs.regs[2] = sources[source_index];
				regs.regs[1] = U64_MAX;
				regs.pstate = pstate;
				regs.pc = 0x1000;
				ret = orlix_tcti_cssc_execute_fixed_gadget(&regs, &decoded);
				KUNIT_ASSERT_EQ_MSG(test, 0, ret, "%s imm %#x",
						     leaf->source_name, immediate);
				KUNIT_EXPECT_EQ(test, expected, regs.regs[1]);
				KUNIT_EXPECT_EQ(test, pstate, regs.pstate);
				KUNIT_EXPECT_EQ(test, 0x1004ULL, regs.pc);
			}
		}
	}
}

static void orlix_tcti_cssc_min_max_immediate_zero_registers_and_pstate(
	struct kunit *test)
{
	size_t leaf_index;

	for (leaf_index = 0;
	     leaf_index < ARRAY_SIZE(orlix_tcti_cssc_min_max_immediate_leaves);
	     leaf_index++) {
		const struct orlix_tcti_cssc_min_max_immediate_leaf *leaf =
			&orlix_tcti_cssc_min_max_immediate_leaves[leaf_index];
		struct pt_regs regs = {};
		struct orlix_tcti_decoded_instruction decoded = orlix_tcti_decode_aarch64(
			orlix_tcti_cssc_min_max_immediate_instruction(leaf, 0x80, 31, 31));
		u64 pstate = PSR_MODE_EL0t | PSR_Z_BIT | PSR_C_BIT;
		int ret;

		regs.regs[0] = U64_MAX;
		regs.pstate = pstate;
		regs.pc = 0x2000;
		ret = orlix_tcti_cssc_execute_fixed_gadget(&regs, &decoded);
		KUNIT_ASSERT_EQ(test, 0, ret);
		KUNIT_EXPECT_EQ(test, pstate, regs.pstate);
		KUNIT_EXPECT_EQ(test, 0x2004ULL, regs.pc);
		KUNIT_EXPECT_EQ(test, U64_MAX, regs.regs[0]);
	}
}

static void orlix_tcti_cssc_min_max_immediate_overlap(struct kunit *test)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_cssc_min_max_immediate_leaves);
	     index++) {
		const struct orlix_tcti_cssc_min_max_immediate_leaf *leaf =
			&orlix_tcti_cssc_min_max_immediate_leaves[index];
		struct pt_regs regs = {};
		struct orlix_tcti_decoded_instruction decoded = orlix_tcti_decode_aarch64(
			orlix_tcti_cssc_min_max_immediate_instruction(leaf, 0x80, 1, 1));
		u64 source = leaf->is_64bit ? (u64)S64_MIN : 0x80000000ULL;
		u64 pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT;
		int ret;

		regs.regs[1] = source;
		regs.pstate = pstate;
		regs.pc = 0x2800;
		ret = orlix_tcti_cssc_execute_fixed_gadget(&regs, &decoded);
		KUNIT_ASSERT_EQ_MSG(test, 0, ret, "%u %s", leaf->ordinal,
				    leaf->source_name);
		KUNIT_EXPECT_EQ(test,
			orlix_tcti_cssc_min_max_immediate_expected(leaf, source, 0x80),
			regs.regs[1]);
		KUNIT_EXPECT_EQ(test, pstate, regs.pstate);
		KUNIT_EXPECT_EQ(test, 0x2804ULL, regs.pc);
	}
}

static void orlix_tcti_cssc_min_max_immediate_rejects_non_cssc_encodings(
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
		struct orlix_tcti_decoded_instruction decoded =
			orlix_tcti_decode_aarch64(instructions[i]);

		KUNIT_EXPECT_NE_MSG(test, ORLIX_TCTI_DECODE_MIN_MAX_IMMEDIATE,
				    decoded.decode_class, "instruction %#x",
				    instructions[i]);
	}
}

static void orlix_tcti_cssc_data_processing_source_fingerprints(struct kunit *test)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_cssc_data_processing_leaves);
	     index++) {
		const struct orlix_tcti_cssc_data_processing_leaf *leaf =
			&orlix_tcti_cssc_data_processing_leaves[index];
		struct orlix_tcti_decoded_instruction decoded = orlix_tcti_decode_aarch64(
			orlix_tcti_cssc_data_processing_instruction(leaf, 3, 2, 1));

		KUNIT_EXPECT_EQ_MSG(test, leaf->source_pattern,
				    leaf->source_pattern & leaf->source_mask,
				    "%s source fingerprint", leaf->source_name);
		KUNIT_ASSERT_EQ_MSG(test,
			leaf->two_source ? ORLIX_TCTI_DECODE_DATA_PROCESSING_2SOURCE :
			ORLIX_TCTI_DECODE_DATA_PROCESSING_1SOURCE, decoded.decode_class,
			"%s", leaf->source_name);
		KUNIT_EXPECT_EQ(test, leaf->is_64bit, decoded.is_64bit);
		KUNIT_EXPECT_EQ(test, 2, decoded.rn);
		KUNIT_EXPECT_EQ(test, 1, decoded.rd);
		if (leaf->two_source) {
			KUNIT_EXPECT_EQ(test, 3, decoded.rm);
			KUNIT_EXPECT_EQ(test, leaf->dp2_op, decoded.dp2_op);
		} else {
			KUNIT_EXPECT_EQ(test, leaf->dp1_op, decoded.dp1_op);
		}
	}
}

static void orlix_tcti_cssc_data_processing_all_legal_register_fields(
	struct kunit *test)
{
	size_t leaf_index;
	u8 rm;
	u8 rn;
	u8 rd;

	for (leaf_index = 0;
	     leaf_index < ARRAY_SIZE(orlix_tcti_cssc_data_processing_leaves);
	     leaf_index++) {
		const struct orlix_tcti_cssc_data_processing_leaf *leaf =
			&orlix_tcti_cssc_data_processing_leaves[leaf_index];

		for (rm = 0; rm < (leaf->two_source ? 32 : 1); rm++) {
			for (rn = 0; rn < 32; rn++) {
				for (rd = 0; rd < 32; rd++) {
					struct orlix_tcti_decoded_instruction decoded =
						orlix_tcti_decode_aarch64(
							orlix_tcti_cssc_data_processing_instruction(
								leaf, rm, rn, rd));

					KUNIT_ASSERT_EQ_MSG(test,
						leaf->two_source ?
						ORLIX_TCTI_DECODE_DATA_PROCESSING_2SOURCE :
						ORLIX_TCTI_DECODE_DATA_PROCESSING_1SOURCE,
						decoded.decode_class, "%s", leaf->source_name);
					KUNIT_EXPECT_EQ(test, leaf->is_64bit, decoded.is_64bit);
					KUNIT_EXPECT_EQ(test, rn, decoded.rn);
					KUNIT_EXPECT_EQ(test, rd, decoded.rd);
					if (leaf->two_source) {
						KUNIT_EXPECT_EQ(test, rm, decoded.rm);
						KUNIT_EXPECT_EQ(test, leaf->dp2_op,
							decoded.dp2_op);
					} else {
						KUNIT_EXPECT_EQ(test, leaf->dp1_op,
							decoded.dp1_op);
					}
				}
			}
		}
	}
}

static void orlix_tcti_cssc_data_processing_execute_boundaries(struct kunit *test)
{
	static const u64 values[] = {
		0, 1, 0x7fffffffULL, 0x80000000ULL, U32_MAX,
		S64_MAX, (u64)S64_MIN, U64_MAX,
	};
	size_t leaf_index;
	size_t left_index;
	size_t right_index;

	for (leaf_index = 0;
	     leaf_index < ARRAY_SIZE(orlix_tcti_cssc_data_processing_leaves);
	     leaf_index++) {
		const struct orlix_tcti_cssc_data_processing_leaf *leaf =
			&orlix_tcti_cssc_data_processing_leaves[leaf_index];

		for (left_index = 0; left_index < ARRAY_SIZE(values); left_index++) {
			for (right_index = 0;
			     right_index < (leaf->two_source ? ARRAY_SIZE(values) : 1);
			     right_index++) {
				struct pt_regs regs = {};
				struct orlix_tcti_decoded_instruction decoded = orlix_tcti_decode_aarch64(
					orlix_tcti_cssc_data_processing_instruction(leaf, 3, 2, 1));
				u64 pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_C_BIT |
					PSR_V_BIT;
				u64 expected = orlix_tcti_cssc_data_processing_expected(
					leaf, values[left_index], values[right_index]);
				int ret;

				regs.regs[2] = values[left_index];
				regs.regs[3] = values[right_index];
				regs.pstate = pstate;
				regs.pc = 0x4000;
				ret = orlix_tcti_cssc_execute_fixed_gadget(&regs, &decoded);
				KUNIT_ASSERT_EQ_MSG(test, 0, ret, "%s", leaf->source_name);
				KUNIT_EXPECT_EQ(test, expected, regs.regs[1]);
				KUNIT_EXPECT_EQ(test, pstate, regs.pstate);
				KUNIT_EXPECT_EQ(test, 0x4004ULL, regs.pc);
			}
		}
	}
}

static void orlix_tcti_cssc_data_processing_zero_registers(struct kunit *test)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_cssc_data_processing_leaves);
	     index++) {
		const struct orlix_tcti_cssc_data_processing_leaf *leaf =
			&orlix_tcti_cssc_data_processing_leaves[index];
		struct pt_regs regs = {};
		struct orlix_tcti_decoded_instruction decoded = orlix_tcti_decode_aarch64(
			orlix_tcti_cssc_data_processing_instruction(leaf, 31, 31, 31));
		u64 pstate = PSR_MODE_EL0t | PSR_Z_BIT | PSR_C_BIT;
		int ret;

		regs.regs[0] = U64_MAX;
		regs.pstate = pstate;
		regs.pc = 0x5000;
		ret = orlix_tcti_cssc_execute_fixed_gadget(&regs, &decoded);
		KUNIT_ASSERT_EQ_MSG(test, 0, ret, "%s", leaf->source_name);
		KUNIT_EXPECT_EQ(test, pstate, regs.pstate);
		KUNIT_EXPECT_EQ(test, 0x5004ULL, regs.pc);
		KUNIT_EXPECT_EQ(test, U64_MAX, regs.regs[0]);
	}
}

static void orlix_tcti_cssc_data_processing_minmax_overlap(struct kunit *test)
{
	static const struct orlix_tcti_cssc_minmax_overlap_operands operands[] = {
		{ 1, 1, 2 }, { 2, 1, 2 }, { 1, 1, 1 },
	};
	size_t leaf_index;
	size_t operand_index;

	for (leaf_index = 0; leaf_index < CSSC_MIN_MAX_REGISTER_LEAF_COUNT;
	     leaf_index++) {
		const struct orlix_tcti_cssc_data_processing_leaf *leaf =
			&orlix_tcti_cssc_data_processing_leaves[leaf_index];

		for (operand_index = 0; operand_index < ARRAY_SIZE(operands);
		     operand_index++) {
			const struct orlix_tcti_cssc_minmax_overlap_operands *operand =
				&operands[operand_index];
			struct pt_regs regs = {};
			u64 left = leaf->is_64bit ? (u64)S64_MIN : 0x80000000ULL;
			u64 right = leaf->is_64bit ? 1 : U32_MAX;
			struct orlix_tcti_decoded_instruction decoded =
				orlix_tcti_decode_aarch64(
					orlix_tcti_cssc_data_processing_instruction(
						leaf, operand->rm, operand->rn, operand->rd));
			u64 pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT;
			u64 expected;
			int ret;

			regs.regs[operand->rn] = left;
			regs.regs[operand->rm] = right;
			expected = orlix_tcti_cssc_data_processing_expected(
				leaf, regs.regs[operand->rn], regs.regs[operand->rm]);
			regs.pstate = pstate;
			regs.pc = 0x5800;
			ret = orlix_tcti_cssc_execute_fixed_gadget(&regs, &decoded);
			KUNIT_ASSERT_EQ_MSG(test, 0, ret, "%u %s", leaf->ordinal,
					    leaf->source_name);
			KUNIT_EXPECT_EQ(test, expected, regs.regs[operand->rd]);
			KUNIT_EXPECT_EQ(test, pstate, regs.pstate);
			KUNIT_EXPECT_EQ(test, 0x5804ULL, regs.pc);
		}
	}
}

static unsigned long orlix_tcti_cssc_data_processing_map_program(
	struct kunit *test, u32 instruction)
{
	const u32 program[] = { instruction, CSSC_SVC };
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
		KUNIT_FAIL(test, "could not write CSSC program: %d", ret);
		return 0;
	}
	ret = sys_mprotect(mapped, PAGE_SIZE, PROT_READ | PROT_EXEC);
	if (ret) {
		vm_munmap(mapped, PAGE_SIZE);
		KUNIT_FAIL(test, "could not protect CSSC program: %d", ret);
		return 0;
	}
	return mapped;
}

static void orlix_tcti_cssc_data_processing_resume_user_rejects_unavailable_feature(
	struct kunit *test)
{
	size_t index;

	KUNIT_ASSERT_EQ(test, 0UL, (unsigned long)ELF_HWCAP2);

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_cssc_data_processing_leaves);
	     index++) {
		const struct orlix_tcti_cssc_data_processing_leaf *leaf =
			&orlix_tcti_cssc_data_processing_leaves[index];
		u64 left = leaf->is_64bit ? (u64)S64_MIN : 0x80000000ULL;
		u64 right = leaf->is_64bit ? 1 : U32_MAX;
		unsigned long mapped = orlix_tcti_cssc_data_processing_map_program(
			test, orlix_tcti_cssc_data_processing_instruction(leaf, 3, 2, 1));
		struct pt_regs regs = {};
		struct pt_regs before;
		struct orlix_tcti_result result;
		u64 pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_C_BIT | PSR_V_BIT;

		KUNIT_ASSERT_NE(test, 0UL, mapped);
		regs.regs[1] = U64_MAX;
		regs.regs[2] = left;
		regs.regs[3] = right;
		regs.pc = mapped;
		regs.pstate = pstate;
		regs.syscallno = NO_SYSCALL;
		before = regs;
		result = orlix_tcti_resume_user(current, &regs, current->mm);

		KUNIT_ASSERT_EQ_MSG(test, ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
				    result.reason,
				    "%s", leaf->source_name);
		KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, result.status);
		KUNIT_EXPECT_EQ(test, mapped, result.pc);
		KUNIT_EXPECT_EQ(test,
			orlix_tcti_cssc_data_processing_instruction(leaf, 3, 2, 1),
			result.instruction);
		KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
	}
}

static void
orlix_tcti_cssc_min_max_immediate_resume_user_rejects_unavailable_feature(
	struct kunit *test)
{
	size_t index;

	KUNIT_ASSERT_EQ(test, 0UL, (unsigned long)ELF_HWCAP2);

	for (index = 0;
	     index < ARRAY_SIZE(orlix_tcti_cssc_min_max_immediate_leaves); index++) {
		const struct orlix_tcti_cssc_min_max_immediate_leaf *leaf =
			&orlix_tcti_cssc_min_max_immediate_leaves[index];
		u8 immediate = 0x80;
		u64 source = leaf->is_64bit ? (u64)S64_MIN : 0x80000000ULL;
		unsigned long mapped = orlix_tcti_cssc_data_processing_map_program(
			test, orlix_tcti_cssc_min_max_immediate_instruction(leaf, immediate,
								      2, 1));
		struct pt_regs regs = {};
		struct pt_regs before;
		struct orlix_tcti_result result;
		u64 pstate = PSR_MODE_EL0t | PSR_Z_BIT | PSR_C_BIT;

		KUNIT_ASSERT_NE(test, 0UL, mapped);
		regs.regs[1] = U64_MAX;
		regs.regs[2] = source;
		regs.pc = mapped;
		regs.pstate = pstate;
		regs.syscallno = NO_SYSCALL;
		before = regs;
		result = orlix_tcti_resume_user(current, &regs, current->mm);

		KUNIT_ASSERT_EQ_MSG(test, ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
				    result.reason,
				    "%s", leaf->source_name);
		KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, result.status);
		KUNIT_EXPECT_EQ(test, mapped, result.pc);
		KUNIT_EXPECT_EQ(test,
			orlix_tcti_cssc_min_max_immediate_instruction(leaf, immediate, 2, 1),
			result.instruction);
		KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
	}
}

static void orlix_tcti_cssc_data_processing_rejects_reserved_opcodes(
	struct kunit *test)
{
	static const u32 instructions[] = {
		0x5ac02441U, 0x5ac02841U, 0x5ac02c41U,
		0x1ac07041U, 0x1ac07441U, 0x1ac07841U,
		0xdac02441U, 0xdac02841U, 0xdac02c41U,
		0x9ac07041U, 0x9ac07441U, 0x9ac07841U,
	};
	size_t index;

	for (index = 0; index < ARRAY_SIZE(instructions); index++) {
		struct orlix_tcti_decoded_instruction decoded =
			orlix_tcti_decode_aarch64(instructions[index]);

		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
				    decoded.decode_class, "instruction %#x",
				    instructions[index]);
	}
}

static struct kunit_case orlix_tcti_cssc_min_max_immediate_test_cases[] = {
	KUNIT_CASE(orlix_tcti_integer_minmax_exact_source_cohort),
	KUNIT_CASE(orlix_tcti_cssc_min_max_immediate_source_fingerprints),
	KUNIT_CASE(orlix_tcti_cssc_min_max_immediate_all_legal_fields),
	KUNIT_CASE(orlix_tcti_cssc_min_max_immediate_execute_all_immediates),
	KUNIT_CASE(orlix_tcti_cssc_min_max_immediate_zero_registers_and_pstate),
	KUNIT_CASE(orlix_tcti_cssc_min_max_immediate_overlap),
	KUNIT_CASE(orlix_tcti_cssc_min_max_immediate_resume_user_rejects_unavailable_feature),
	KUNIT_CASE(orlix_tcti_cssc_min_max_immediate_rejects_non_cssc_encodings),
	KUNIT_CASE(orlix_tcti_cssc_data_processing_source_fingerprints),
	KUNIT_CASE(orlix_tcti_cssc_data_processing_all_legal_register_fields),
	KUNIT_CASE(orlix_tcti_cssc_data_processing_execute_boundaries),
	KUNIT_CASE(orlix_tcti_cssc_data_processing_zero_registers),
	KUNIT_CASE(orlix_tcti_cssc_data_processing_minmax_overlap),
	KUNIT_CASE(orlix_tcti_cssc_data_processing_resume_user_rejects_unavailable_feature),
	KUNIT_CASE(orlix_tcti_cssc_data_processing_rejects_reserved_opcodes),
	{}
};

static int orlix_tcti_cssc_min_max_immediate_test_init(struct kunit *test)
{
	struct mm_struct *mm = mm_alloc();

	if (!mm)
		return -ENOMEM;
	kthread_use_mm(mm);
	test->priv = mm;
	return 0;
}

static void orlix_tcti_cssc_min_max_immediate_test_exit(struct kunit *test)
{
	struct mm_struct *mm = test->priv;

	if (!mm)
		return;
	kthread_unuse_mm(mm);
	mmput(mm);
}

struct kunit_suite orlix_tcti_cssc_min_max_immediate_test_suite = {
	.name = "orlix-tcti-cssc-min-max-immediate",
	.init = orlix_tcti_cssc_min_max_immediate_test_init,
	.exit = orlix_tcti_cssc_min_max_immediate_test_exit,
	.test_cases = orlix_tcti_cssc_min_max_immediate_test_cases,
};

kunit_test_suite(orlix_tcti_cssc_min_max_immediate_test_suite);
