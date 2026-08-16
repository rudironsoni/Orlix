/* SPDX-License-Identifier: GPL-2.0-only */
#include <kunit/test.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/unaligned.h>

#include <asm/orlix_tcti.h>
#include <asm/ptrace.h>

#include "../decode_aarch64.h"
#include "../sme_fp.h"

/* Independent IEEE-754 single-precision values: 1.0f, 2.0f, 3.0f, 4.0f. */
static void orlix_tcti_sme_fp_fmax_2x1_vector(struct kunit *test)
{
	struct pt_regs regs = { .pc = 0x6000 };
	struct orlix_tcti_decoded_instruction decoded = {
		.instruction = 0xc1a1a100U, /* FMAX size=10 (single), Zm=Z1. */
		.decode_class = ORLIX_TCTI_DECODE_SME_FP,
		.sme_fp_source_ordinal = 2000U,
	};

	memset(&current->thread.user_sve, 0, sizeof(current->thread.user_sve));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sve_state_reset(&current->thread.user_sve,
		current->thread.user_simd, 16U));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sme_state_reset(&current->thread.user_sme,
		16U, true, false, false));
	put_unaligned_le32(0x3f800000U, &current->thread.user_sve.z[0][0]);
	put_unaligned_le32(0x40400000U, &current->thread.user_sve.z[0][4]);
	put_unaligned_le32(0x40000000U, &current->thread.user_sve.z[1][0]);
	put_unaligned_le32(0x40800000U, &current->thread.user_sve.z[1][4]);

	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_sme_fp(&regs, &decoded));
	KUNIT_EXPECT_EQ(test, 0x40000000U,
		get_unaligned_le32(&current->thread.user_sve.z[0][0]));
	KUNIT_EXPECT_EQ(test, 0x40800000U,
		get_unaligned_le32(&current->thread.user_sve.z[0][4]));
	KUNIT_EXPECT_EQ(test, 0x6004UL, regs.pc);
	orlix_tcti_sme_state_release(&current->thread.user_sme);
}

/* FMAX { Z2.S-Z3.S }, { Z2.S-Z3.S }, Z1.S.  Zdn is bits [4:1]: the
 * previous raw-low-bit decode selected Z4-Z5 instead.  The half-precision
 * lane separately rejects the old 1 << size byte mapping. */
static void orlix_tcti_sme_fp_minmax_group_and_fp16_vectors(struct kunit *test)
{
	struct pt_regs regs = { .pc = 0x6080 };
	struct orlix_tcti_decoded_instruction decoded = {
		.instruction = 0xc1a1a102U,
		.decode_class = ORLIX_TCTI_DECODE_SME_FP,
		.sme_fp_source_ordinal = 2000U,
	};

	memset(&current->thread.user_sve, 0, sizeof(current->thread.user_sve));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sve_state_reset(&current->thread.user_sve,
		current->thread.user_simd, 16U));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sme_state_reset(&current->thread.user_sme,
		16U, true, false, false));
	put_unaligned_le32(0x3f800000U, &current->thread.user_sve.z[2][0]);
	put_unaligned_le32(0x40400000U, &current->thread.user_sve.z[3][0]);
	put_unaligned_le32(0x40000000U, &current->thread.user_sve.z[1][0]);
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_sme_fp(&regs, &decoded));
	KUNIT_EXPECT_EQ(test, 0x40000000U,
		get_unaligned_le32(&current->thread.user_sve.z[2][0]));
	KUNIT_EXPECT_EQ(test, 0x40400000U,
		get_unaligned_le32(&current->thread.user_sve.z[3][0]));
	KUNIT_EXPECT_EQ(test, 0U, get_unaligned_le32(&current->thread.user_sve.z[4][0]));

	decoded.instruction = 0xc161a100U; /* FMAX { Z0.H-Z1.H }, Z1.H. */
	put_unaligned_le16(0x3c00U, &current->thread.user_sve.z[0][0]);
	put_unaligned_le16(0x4000U, &current->thread.user_sve.z[1][0]);
	put_unaligned_le16(0x4200U, &current->thread.user_sve.z[1][2]);
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_sme_fp(&regs, &decoded));
	KUNIT_EXPECT_EQ(test, 0x4000U,
		get_unaligned_le16(&current->thread.user_sve.z[0][0]));
	KUNIT_EXPECT_EQ(test, 0x4200U,
		get_unaligned_le16(&current->thread.user_sve.z[0][2]));
	KUNIT_EXPECT_EQ(test, 0x6088UL, regs.pc);

	orlix_tcti_sme_state_release(&current->thread.user_sme);
}

/* FCLAMP { Z0.H-Z1.H }, Z2.H, Z3.H: max(minimum, Zd), then min(maximum,
 * result).  This must use the architectural FP16 helper, rather than pass a
 * two-byte element width to the FP32/FP64 helper. */
static void orlix_tcti_sme_fp_fclamp_fp16_vector(struct kunit *test)
{
	struct pt_regs regs = { .pc = 0x60a0 };
	struct orlix_tcti_decoded_instruction decoded = {
		.instruction = 0xc163c040U,
		.decode_class = ORLIX_TCTI_DECODE_SME_FP,
		.sme_fp_source_ordinal = 2070U,
	};

	memset(&current->thread.user_sve, 0, sizeof(current->thread.user_sve));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sve_state_reset(&current->thread.user_sve,
		current->thread.user_simd, 16U));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sme_state_reset(&current->thread.user_sme,
		16U, true, false, false));
	put_unaligned_le16(0x3c00U, &current->thread.user_sve.z[0][0]); /* 1 */
	put_unaligned_le16(0x4400U, &current->thread.user_sve.z[1][0]); /* 4 */
	put_unaligned_le16(0x4000U, &current->thread.user_sve.z[2][0]); /* 2 */
	put_unaligned_le16(0x4200U, &current->thread.user_sve.z[3][0]); /* 3 */
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_sme_fp(&regs, &decoded));
	KUNIT_EXPECT_EQ(test, 0x4000U,
		get_unaligned_le16(&current->thread.user_sve.z[0][0]));
	KUNIT_EXPECT_EQ(test, 0x4200U,
		get_unaligned_le16(&current->thread.user_sve.z[1][0]));
	KUNIT_EXPECT_EQ(test, 0x60a4UL, regs.pc);
	orlix_tcti_sme_state_release(&current->thread.user_sme);
}

/* FAMAX clears AH/FIZ/FZ/FZ16 before its FPAbsMax comparison.  With FZ set,
 * the smallest negative single remains greater in magnitude than +0, rather
 * than flushing to the zero source.  FPAbsMax returns that original signed
 * operand, not its positive comparison magnitude. */
static void orlix_tcti_sme_fp_famax_forces_denormal_controls_off(struct kunit *test)
{
	struct pt_regs regs = { .pc = 0x60c0 };
	struct orlix_tcti_decoded_instruction decoded = {
		.instruction = 0xc1a2b140U, /* FAMAX { Z0.S-Z1.S }, { Z2.S-Z3.S } */
		.decode_class = ORLIX_TCTI_DECODE_SME_FP,
		.sme_fp_source_ordinal = 2044U,
	};

	memset(&current->thread.user_sve, 0, sizeof(current->thread.user_sve));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sve_state_reset(&current->thread.user_sve,
		current->thread.user_simd, 16U));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sme_state_reset(&current->thread.user_sme,
		16U, true, false, false));
	current->thread.user_fpcr = BIT(24) | BIT(19) | BIT(0) | BIT(26);
	put_unaligned_le32(0x80000001U, &current->thread.user_sve.z[0][0]);
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_sme_fp(&regs, &decoded));
	KUNIT_EXPECT_EQ(test, 0x80000001U,
		get_unaligned_le32(&current->thread.user_sve.z[0][0]));
	KUNIT_EXPECT_EQ(test, 0x60c4UL, regs.pc);
	current->thread.user_fpcr = 0;
	orlix_tcti_sme_state_release(&current->thread.user_sme);
}

static void orlix_tcti_sme_fp_requires_streaming_state(struct kunit *test)
{
	struct pt_regs regs = { .pc = 0x6100 };
	struct orlix_tcti_decoded_instruction decoded = {
		.instruction = 0xc1a1a100U,
		.decode_class = ORLIX_TCTI_DECODE_SME_FP,
		.sme_fp_source_ordinal = 2000U,
	};

	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sme_state_reset(&current->thread.user_sme,
		16U, false, false, false));
	KUNIT_EXPECT_EQ(test, -EINVAL, orlix_tcti_execute_sme_fp(&regs, &decoded));
	KUNIT_EXPECT_EQ(test, 0x6100UL, regs.pc);
	orlix_tcti_sme_state_release(&current->thread.user_sme);
}

/* Independent values for Arm fadd_za_zw_2x2: 1+2=3 and 4+5=9. */
static void orlix_tcti_sme_fp_fadd_za_2x2_vector(struct kunit *test)
{
	struct pt_regs regs = { .pc = 0x6200 };
	struct orlix_tcti_decoded_instruction decoded = {
		.instruction = 0xc1a01c00U,
		.decode_class = ORLIX_TCTI_DECODE_SME_FP,
		.sme_fp_source_ordinal = 1946U,
	};

	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sve_state_reset(&current->thread.user_sve,
		current->thread.user_simd, 16U));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sme_state_reset(&current->thread.user_sme,
		16U, true, true, false));
	put_unaligned_le32(0x3f800000U, current->thread.user_sme.za);
	put_unaligned_le32(0x40800000U, current->thread.user_sme.za + 16U);
	put_unaligned_le32(0x40000000U, &current->thread.user_sve.z[0][0]);
	put_unaligned_le32(0x40a00000U, &current->thread.user_sve.z[1][0]);

	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_sme_fp(&regs, &decoded));
	KUNIT_EXPECT_EQ(test, 0x40400000U,
		get_unaligned_le32(current->thread.user_sme.za));
	KUNIT_EXPECT_EQ(test, 0x41100000U,
		get_unaligned_le32(current->thread.user_sme.za + 16U));
	KUNIT_EXPECT_EQ(test, 0x6204UL, regs.pc);
	orlix_tcti_sme_state_release(&current->thread.user_sme);
}

/* FADD ZA.S[w10, 0], { Z2.S-Z3.S }.  The explicit Zm group and Rv=2
 * distinguish the Arm fields [9:6] and [14:13] from the previous shifts. */
static void orlix_tcti_sme_fp_za_group_and_row_fields_vector(struct kunit *test)
{
	struct pt_regs regs = { .pc = 0x6280 };
	struct orlix_tcti_decoded_instruction decoded = {
		.instruction = 0xc1a05c40U,
		.decode_class = ORLIX_TCTI_DECODE_SME_FP,
		.sme_fp_source_ordinal = 1946U,
	};

	memset(&current->thread.user_sve, 0, sizeof(current->thread.user_sve));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sve_state_reset(&current->thread.user_sve,
		current->thread.user_simd, 32U));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sme_state_reset(&current->thread.user_sme,
		32U, true, true, false));
	put_unaligned_le32(0x3f800000U, current->thread.user_sme.za);
	put_unaligned_le32(0x3f800000U, current->thread.user_sme.za + 32U);
	put_unaligned_le32(0x40000000U, &current->thread.user_sve.z[2][0]);
	regs.regs[10] = 1U;
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_sme_fp(&regs, &decoded));
	KUNIT_EXPECT_EQ(test, 0x3f800000U,
		get_unaligned_le32(current->thread.user_sme.za));
	KUNIT_EXPECT_EQ(test, 0x40400000U,
		get_unaligned_le32(current->thread.user_sme.za + 32U));
	KUNIT_EXPECT_EQ(test, 0x6284UL, regs.pc);
	orlix_tcti_sme_state_release(&current->thread.user_sme);
}

/* Independent BF16 vectors: 1 + 0.5 = 1.5, and +Inf + -Inf is default NaN. */
static void orlix_tcti_sme_fp_bfadd_za_special_values(struct kunit *test)
{
	struct pt_regs regs = { .pc = 0x6300 };
	struct orlix_tcti_decoded_instruction decoded = {
		.instruction = 0xc1e41c00U,
		.decode_class = ORLIX_TCTI_DECODE_SME_FP,
		.sme_fp_source_ordinal = 1951U,
	};

	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sve_state_reset(&current->thread.user_sve,
		current->thread.user_simd, 16U));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sme_state_reset(&current->thread.user_sme,
		16U, true, true, false));
	put_unaligned_le16(0x3f80U, current->thread.user_sme.za);
	put_unaligned_le16(0x7f80U, current->thread.user_sme.za + 2U);
	put_unaligned_le16(0x3f00U, &current->thread.user_sve.z[0][0]);
	put_unaligned_le16(0xff80U, &current->thread.user_sve.z[0][2]);

	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_sme_fp(&regs, &decoded));
	KUNIT_EXPECT_EQ(test, 0x3fc0U,
		get_unaligned_le16(current->thread.user_sme.za));
	KUNIT_EXPECT_EQ(test, 0x7fc0U,
		get_unaligned_le16(current->thread.user_sme.za + 2U));
	KUNIT_EXPECT_EQ(test, 0x6304UL, regs.pc);
	orlix_tcti_sme_state_release(&current->thread.user_sme);
}

/* BFADD ZA.H[w8, 0, VGx4], {Z4.H-Z7.H}.  All four ZA vectors are
 * independently observable, so this rejects both a 2-register treatment of
 * the 4x4 encoding and overwriting the BF16 helper's direct ZA result. */
static void orlix_tcti_sme_fp_bfadd_za_4x4_vectors(struct kunit *test)
{
	struct pt_regs regs = { .pc = 0x6380 };
	struct orlix_tcti_decoded_instruction decoded = {
		.instruction = 0xc1e51c80U,
		.decode_class = ORLIX_TCTI_DECODE_SME_FP,
		.sme_fp_source_ordinal = 1991U,
	};
	u8 register_index;

	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sve_state_reset(&current->thread.user_sve,
		current->thread.user_simd, 32U));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sme_state_reset(&current->thread.user_sme,
		32U, true, true, false));
	for (register_index = 0; register_index < 4U; register_index++) {
		put_unaligned_le16(0x3f80U, current->thread.user_sme.za +
			(size_t)register_index * 32U);
		put_unaligned_le16((u16)(0x3f80U + register_index * 0x0080U),
			&current->thread.user_sve.z[4U + register_index][0]);
	}

	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_sme_fp(&regs, &decoded));
	KUNIT_EXPECT_EQ(test, 0x4000U,
		get_unaligned_le16(current->thread.user_sme.za));
	KUNIT_EXPECT_EQ(test, 0x4040U,
		get_unaligned_le16(current->thread.user_sme.za + 32U));
	KUNIT_EXPECT_EQ(test, 0x40a0U,
		get_unaligned_le16(current->thread.user_sme.za + 64U));
	KUNIT_EXPECT_EQ(test, 0x4110U,
		get_unaligned_le16(current->thread.user_sme.za + 96U));
	KUNIT_EXPECT_EQ(test, 0x6384UL, regs.pc);

	orlix_tcti_sme_state_release(&current->thread.user_sme);
}

/* Each source-bound ZA add/sub leaf writes a literal, independently checked
 * result.  The 16-bit rows exercise both IEEE FP16 and BF16 execution. */
static void orlix_tcti_sme_fp_za_add_sub_source_rows(struct kunit *test)
{
	static const struct {
		u16 ordinal;
		u32 instruction;
		u32 accumulator;
		u32 source;
		u32 expected;
		bool half;
	} rows[] = {
		{ 1946U, 0xc1a01c00U, 0x3f800000U, 0x40000000U,
		  0x40400000U, false },
		{ 1947U, 0xc1a01c08U, 0x40a00000U, 0x40000000U,
		  0x40400000U, false },
		{ 1950U, 0xc1a41c00U, 0x00003c00U, 0x00003800U,
		  0x00003e00U, true },
		{ 1951U, 0xc1e41c00U, 0x00003f80U, 0x00003f00U,
		  0x00003fc0U, true },
		{ 1952U, 0xc1a41c08U, 0x00003e00U, 0x00003800U,
		  0x00003c00U, true },
		{ 1953U, 0xc1e41c08U, 0x00003fc0U, 0x00003f00U,
		  0x00003f80U, true },
	};
	unsigned int index;

	for (index = 0; index < ARRAY_SIZE(rows); index++) {
		struct pt_regs regs = { .pc = 0x6380U + index * sizeof(u32) };
		struct orlix_tcti_decoded_instruction decoded = {
			.instruction = rows[index].instruction,
			.decode_class = ORLIX_TCTI_DECODE_SME_FP,
			.sme_fp_source_ordinal = rows[index].ordinal,
		};

		memset(&current->thread.user_sve, 0, sizeof(current->thread.user_sve));
		KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sve_state_reset(
			&current->thread.user_sve, current->thread.user_simd, 16U));
		KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sme_state_reset(
			&current->thread.user_sme, 16U, true, true, false));
		if (rows[index].half) {
			put_unaligned_le16((u16)rows[index].accumulator,
				current->thread.user_sme.za);
			put_unaligned_le16((u16)rows[index].source,
				&current->thread.user_sve.z[0][0]);
		} else {
			put_unaligned_le32(rows[index].accumulator,
				current->thread.user_sme.za);
			put_unaligned_le32(rows[index].source,
				&current->thread.user_sve.z[0][0]);
		}

		KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_sme_fp(&regs, &decoded));
		if (rows[index].half)
			KUNIT_EXPECT_EQ(test, (u16)rows[index].expected,
				get_unaligned_le16(current->thread.user_sme.za));
		else
			KUNIT_EXPECT_EQ(test, rows[index].expected,
				get_unaligned_le32(current->thread.user_sme.za));
		KUNIT_EXPECT_EQ(test, 0x6384UL + index * sizeof(u32), regs.pc);
		orlix_tcti_sme_state_release(&current->thread.user_sme);
	}
}

/* Each 4x4 widening matrix leaf uses independent ZA slices and source lanes. */
static void orlix_tcti_sme_fp_long_matrix_source_rows(struct kunit *test)
{
	static const struct {
		u16 ordinal;
		u32 instruction;
		u32 accumulator;
		u32 expected;
		bool bf16;
	} rows[] = {
		{ 1960U, 0xc1a10810U, 0x3f800000U, 0x40000000U, true },
		{ 1961U, 0xc1a10800U, 0x40000000U, 0x40400000U, false },
		{ 1962U, 0xc1a10818U, 0x40400000U, 0x40000000U, true },
		{ 1963U, 0xc1a10808U, 0x40400000U, 0x40000000U, false },
	};
	unsigned int index;

	for (index = 0; index < ARRAY_SIZE(rows); index++) {
		struct pt_regs regs = { .pc = 0x63c0U + index * sizeof(u32) };
		struct orlix_tcti_decoded_instruction decoded = {
			.instruction = rows[index].instruction,
			.decode_class = ORLIX_TCTI_DECODE_SME_FP,
			.sme_fp_source_ordinal = rows[index].ordinal,
		};
		u8 source_register;
		u8 lane;

		memset(&current->thread.user_sve, 0, sizeof(current->thread.user_sve));
		KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sve_state_reset(
			&current->thread.user_sve, current->thread.user_simd, 16U));
		KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sme_state_reset(
			&current->thread.user_sme, 16U, true, true, false));
		for (source_register = 0; source_register < 4U; source_register++)
			for (lane = 0; lane < 8U; lane++)
				put_unaligned_le16(rows[index].bf16 ? 0x3f80U : 0x3c00U,
					&current->thread.user_sve.z[source_register][lane * 2U]);
		for (source_register = 0; source_register < 4U; source_register++) {
			u16 slice = source_register * 4U;

			put_unaligned_le32(rows[index].accumulator,
				current->thread.user_sme.za + (size_t)slice * 16U);
			put_unaligned_le32(rows[index].accumulator,
				current->thread.user_sme.za + (size_t)(slice + 1U) * 16U);
		}

		KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_sme_fp(&regs, &decoded));
		KUNIT_EXPECT_EQ(test, rows[index].expected,
			get_unaligned_le32(current->thread.user_sme.za));
		KUNIT_EXPECT_EQ(test, rows[index].expected,
			get_unaligned_le32(current->thread.user_sme.za + 16U));
		KUNIT_EXPECT_EQ(test, rows[index].expected,
			get_unaligned_le32(current->thread.user_sme.za + 12U * 16U));
		KUNIT_EXPECT_EQ(test, 0x63c4UL + index * sizeof(u32), regs.pc);
		orlix_tcti_sme_state_release(&current->thread.user_sme);
	}
}

/* Source-bound 4x4 dot and non-widening matrix leaves use every Z group. */
static void orlix_tcti_sme_fp_matrix_4x4_source_rows(struct kunit *test)
{
	static const struct {
		u16 ordinal;
		u32 instruction;
		bool dot;
		bool bf16;
		bool subtract;
	} rows[] = {
		{ 1969U, 0xc1a11000U, true, false, false },
		{ 1970U, 0xc1a11010U, true, true, false },
		{ 1973U, 0xc1a11008U, false, false, false },
		{ 1974U, 0xc1e11008U, false, true, false },
		{ 1975U, 0xc1a11018U, false, false, true },
		{ 1976U, 0xc1e11018U, false, true, true },
	};
	unsigned int index;

	for (index = 0; index < ARRAY_SIZE(rows); index++) {
		struct pt_regs regs = { .pc = 0x63e0U + index * sizeof(u32) };
		struct orlix_tcti_decoded_instruction decoded = {
			.instruction = rows[index].instruction,
			.decode_class = ORLIX_TCTI_DECODE_SME_FP,
			.sme_fp_source_ordinal = rows[index].ordinal,
		};
		u8 register_index;
		u8 lane;

		memset(&current->thread.user_sve, 0, sizeof(current->thread.user_sve));
		KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sve_state_reset(
			&current->thread.user_sve, current->thread.user_simd, 32U));
		KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sme_state_reset(
			&current->thread.user_sme, 32U, true, true, false));
		for (register_index = 0; register_index < 8U; register_index++)
			for (lane = 0; lane < 16U; lane++)
				put_unaligned_le16(rows[index].bf16 ? 0x3f80U : 0x3c00U,
					&current->thread.user_sve.z[register_index][lane * 2U]);
		if (rows[index].dot) {
			for (register_index = 0; register_index < 4U; register_index++)
				put_unaligned_le32(0x3f800000U,
					current->thread.user_sme.za + (size_t)register_index * 32U);
		} else {
			u16 accumulator = rows[index].subtract ? 0x4200U : 0x3c00U;

			for (register_index = 0; register_index < 4U; register_index++)
				put_unaligned_le16(accumulator,
					current->thread.user_sme.za + (size_t)register_index * 32U);
		}

		KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_sme_fp(&regs, &decoded));
		if (rows[index].dot)
			KUNIT_EXPECT_EQ(test, 0x40400000U,
				get_unaligned_le32(current->thread.user_sme.za));
		else
			KUNIT_EXPECT_EQ(test, rows[index].subtract ? 0x3c00U : 0x4000U,
				get_unaligned_le16(current->thread.user_sme.za));
		KUNIT_EXPECT_EQ(test, 0x63e4UL + index * sizeof(u32), regs.pc);
		orlix_tcti_sme_state_release(&current->thread.user_sme);
	}
}

/* Pinned BF16 SME2 rows: every group form takes the selected source layout. */
static void orlix_tcti_sme_fp_bf16_family_all_ordinals(struct kunit *test)
{
	static const struct {
		u16 ordinal;
		u32 instruction;
	} cases[] = {
		{ 2001U, 0xc120a100U }, { 2003U, 0xc120a101U },
		{ 2005U, 0xc120a120U }, { 2007U, 0xc120a121U },
		{ 2009U, 0xc120a180U }, { 2019U, 0xc120a900U },
		{ 2021U, 0xc120a901U }, { 2023U, 0xc120a920U },
		{ 2025U, 0xc120a921U }, { 2027U, 0xc120a980U },
		{ 2037U, 0xc120b100U }, { 2039U, 0xc120b101U },
		{ 2041U, 0xc120b120U }, { 2043U, 0xc120b121U },
		{ 2047U, 0xc120b180U }, { 2056U, 0xc120b900U },
		{ 2058U, 0xc120b901U }, { 2060U, 0xc120b920U },
		{ 2062U, 0xc120b921U }, { 2066U, 0xc120b980U },
	};
	struct pt_regs regs = { .pc = 0x6400 };
	unsigned int index;

	for (index = 0; index < ARRAY_SIZE(cases); index++) {
		struct orlix_tcti_decoded_instruction decoded = {
			.instruction = cases[index].instruction,
			.decode_class = ORLIX_TCTI_DECODE_SME_FP,
			.sme_fp_source_ordinal = cases[index].ordinal,
		};

		memset(&current->thread.user_sve, 0, sizeof(current->thread.user_sve));
		KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sve_state_reset(&current->thread.user_sve,
			current->thread.user_simd, 16U));
		KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sme_state_reset(&current->thread.user_sme,
			16U, true, false, false));
		put_unaligned_le16(0x3f80U, &current->thread.user_sve.z[0][0]);
		put_unaligned_le16(0x4000U, &current->thread.user_sve.z[1][0]);
		KUNIT_EXPECT_EQ(test, 0, orlix_tcti_execute_sme_fp(&regs, &decoded));
		KUNIT_EXPECT_EQ(test, 0x6404UL + index * sizeof(u32), regs.pc);
		orlix_tcti_sme_state_release(&current->thread.user_sme);
	}
}

/* BFMAX selects +0, BFMIN selects -0, *NUM ignores one quiet NaN, and
 * BFSCALE preserves the guest FPSR sticky flags while reporting overflow. */
static void orlix_tcti_sme_fp_bf16_special_values(struct kunit *test)
{
	struct pt_regs regs = { .pc = 0x6500 };
	struct orlix_tcti_decoded_instruction decoded = {
		.instruction = 0xc120a100U,
		.decode_class = ORLIX_TCTI_DECODE_SME_FP,
		.sme_fp_source_ordinal = 2001U,
	};

	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sve_state_reset(&current->thread.user_sve,
		current->thread.user_simd, 16U));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sme_state_reset(&current->thread.user_sme,
		16U, true, false, false));
	put_unaligned_le16(0x8000U, &current->thread.user_sve.z[0][0]);
	put_unaligned_le16(0x0000U, &current->thread.user_sve.z[1][0]);
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_sme_fp(&regs, &decoded));
	KUNIT_EXPECT_EQ(test, 0x0000U, get_unaligned_le16(&current->thread.user_sve.z[0][0]));
	decoded.sme_fp_source_ordinal = 2007U;
	decoded.instruction = 0xc121a121U;
	put_unaligned_le16(0x7fc1U, &current->thread.user_sve.z[0][0]);
	put_unaligned_le16(0x3f80U, &current->thread.user_sve.z[1][0]);
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_sme_fp(&regs, &decoded));
	KUNIT_EXPECT_EQ(test, 0x3f80U, get_unaligned_le16(&current->thread.user_sve.z[0][0]));
	decoded.sme_fp_source_ordinal = 2009U;
	decoded.instruction = 0xc121a180U;
	put_unaligned_le16(0x7f7fU, &current->thread.user_sve.z[0][0]);
	put_unaligned_le16(1U, &current->thread.user_sve.z[1][0]);
	current->thread.user_fpsr = BIT(27);
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_sme_fp(&regs, &decoded));
	KUNIT_EXPECT_EQ(test, 0x7f80U, get_unaligned_le16(&current->thread.user_sve.z[0][0]));
	KUNIT_EXPECT_EQ(test, BIT(27) | BIT(2) | BIT(4), current->thread.user_fpsr);
	orlix_tcti_sme_state_release(&current->thread.user_sme);
}

/* Pinned zzw field expansion: Zm=15 is Z30-Z31 for 2x2 and Zm=7 is
 * Z28-Z31 for 4x4.  These high groups caught the former double expansion. */
static void orlix_tcti_sme_fp_bf16_zzw_high_source_groups(struct kunit *test)
{
	struct pt_regs regs = { .pc = 0x6600 };
	struct orlix_tcti_decoded_instruction decoded = {
		.instruction = 0xc13eb100U, /* BFMAX zzw_2x2, Zm=15 -> Z30-Z31. */
		.decode_class = ORLIX_TCTI_DECODE_SME_FP,
		.sme_fp_source_ordinal = 2037U,
	};
	u8 register_index;

	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sve_state_reset(&current->thread.user_sve,
		current->thread.user_simd, 16U));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sme_state_reset(&current->thread.user_sme,
		16U, true, false, false));
	put_unaligned_le16(0x3f80U, &current->thread.user_sve.z[0][0]);
	put_unaligned_le16(0x4000U, &current->thread.user_sve.z[1][0]);
	put_unaligned_le16(0x4040U, &current->thread.user_sve.z[30][0]);
	put_unaligned_le16(0x4080U, &current->thread.user_sve.z[31][0]);
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_sme_fp(&regs, &decoded));
	KUNIT_EXPECT_EQ(test, 0x4040U, get_unaligned_le16(&current->thread.user_sve.z[0][0]));
	KUNIT_EXPECT_EQ(test, 0x4080U, get_unaligned_le16(&current->thread.user_sve.z[1][0]));

	decoded.instruction = 0xc13cb900U; /* BFMAX zzw_4x4, Zm=7 -> Z28-Z31. */
	decoded.sme_fp_source_ordinal = 2056U;
	for (register_index = 0; register_index < 4U; register_index++) {
		put_unaligned_le16(0x3f80U, &current->thread.user_sve.z[register_index][2]);
		put_unaligned_le16(0x4040U + register_index * 0x40U,
			&current->thread.user_sve.z[28U + register_index][2]);
	}
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_sme_fp(&regs, &decoded));
	for (register_index = 0; register_index < 4U; register_index++)
		KUNIT_EXPECT_EQ(test, 0x4040U + register_index * 0x40U,
			get_unaligned_le16(&current->thread.user_sve.z[register_index][2]));
	KUNIT_EXPECT_EQ(test, 0x6608UL, regs.pc);
	orlix_tcti_sme_state_release(&current->thread.user_sme);
}

/* Independent IEEE-754 vectors: trunc(1.75)=1, trunc(-2.5)=-2 and
 * FRINTN(2.5)=2 (ties-to-even).  These are literal architectural results,
 * not values calculated by the execution helper. */
static void orlix_tcti_sme_fp_conversion_and_rounding_vectors(struct kunit *test)
{
	struct pt_regs regs = { .pc = 0x6700 };
	struct orlix_tcti_decoded_instruction decoded = {
		.instruction = 0xc121e040U, /* FCVTZS MZ2 base 0, source base 2. */
		.decode_class = ORLIX_TCTI_DECODE_SME_FP,
		.sme_fp_source_ordinal = 2095U,
	};

	memset(&current->thread.user_sve, 0, sizeof(current->thread.user_sve));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sve_state_reset(&current->thread.user_sve,
		current->thread.user_simd, 16U));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sme_state_reset(&current->thread.user_sme,
		16U, true, false, false));
	put_unaligned_le32(0x3fe00000U, &current->thread.user_sve.z[2][0]);
	put_unaligned_le32(0xc0200000U, &current->thread.user_sve.z[2][4]);
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_sme_fp(&regs, &decoded));
	KUNIT_EXPECT_EQ(test, 1U, get_unaligned_le32(&current->thread.user_sve.z[0][0]));
	KUNIT_EXPECT_EQ(test, 0xfffffffeU,
		get_unaligned_le32(&current->thread.user_sve.z[0][4]));

	decoded.instruction = 0xc1a8e040U; /* FRINTN MZ2 base 0, source base 2. */
	decoded.sme_fp_source_ordinal = 2114U;
	put_unaligned_le32(0x40200000U, &current->thread.user_sve.z[2][0]);
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_sme_fp(&regs, &decoded));
	KUNIT_EXPECT_EQ(test, 0x40000000U,
		get_unaligned_le32(&current->thread.user_sve.z[0][0]));
	KUNIT_EXPECT_EQ(test, 0x6708UL, regs.pc);
	orlix_tcti_sme_state_release(&current->thread.user_sme);
}

/* FCVT and FCVTL read one ordinary Zn, not a two-register source group.
 * FCVT concatenates widened lanes across Zd/Zd+1; FCVTL de-interleaves even
 * and odd input lanes.  Both observations fail the previous group-copy loop. */
static void orlix_tcti_sme_fp_width_conversion_layout_vectors(struct kunit *test)
{
	struct pt_regs regs = { .pc = 0x6740 };
	struct orlix_tcti_decoded_instruction decoded = {
		.instruction = 0xc1a0e040U, /* FCVT { Z0.S, Z1.S }, Z2.H */
		.decode_class = ORLIX_TCTI_DECODE_SME_FP,
		.sme_fp_source_ordinal = 2118U,
	};
	u16 lane;

	memset(&current->thread.user_sve, 0, sizeof(current->thread.user_sve));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sve_state_reset(&current->thread.user_sve,
		current->thread.user_simd, 16U));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sme_state_reset(&current->thread.user_sme,
		16U, true, false, false));
	for (lane = 0; lane < 8U; lane++)
		put_unaligned_le16(0x3c00U + lane * 0x400U,
			&current->thread.user_sve.z[2][lane * sizeof(u16)]);
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_sme_fp(&regs, &decoded));
	KUNIT_EXPECT_EQ(test, 0x3f800000U,
		get_unaligned_le32(&current->thread.user_sve.z[0][0]));
	KUNIT_EXPECT_EQ(test, 0x40800000U,
		get_unaligned_le32(&current->thread.user_sve.z[0][12]));
	KUNIT_EXPECT_EQ(test, 0x40800000U,
		get_unaligned_le32(&current->thread.user_sve.z[1][0]));
	KUNIT_EXPECT_EQ(test, 0x41400000U,
		get_unaligned_le32(&current->thread.user_sve.z[1][12]));

	decoded.instruction = 0xc1a0e041U; /* FCVTL { Z0.S, Z1.S }, Z2.H */
	decoded.sme_fp_source_ordinal = 2119U;
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_sme_fp(&regs, &decoded));
	KUNIT_EXPECT_EQ(test, 0x3f800000U,
		get_unaligned_le32(&current->thread.user_sve.z[0][0]));
	KUNIT_EXPECT_EQ(test, 0x40000000U,
		get_unaligned_le32(&current->thread.user_sve.z[0][4]));
	KUNIT_EXPECT_EQ(test, 0x3fc00000U,
		get_unaligned_le32(&current->thread.user_sve.z[1][0]));
	KUNIT_EXPECT_EQ(test, 0x40400000U,
		get_unaligned_le32(&current->thread.user_sve.z[1][4]));
	KUNIT_EXPECT_EQ(test, 0x6748UL, regs.pc);
	orlix_tcti_sme_state_release(&current->thread.user_sme);
}

/* The MZ4 conversion fields are Zd[4:2] and Zn[9:7].  Encode distinct
 * groups so the prior shared MZ2 shifts would read Z0 and overwrite Z8. */
static void orlix_tcti_sme_fp_mz4_group_fields_vector(struct kunit *test)
{
	struct pt_regs regs = { .pc = 0x6760 };
	struct orlix_tcti_decoded_instruction decoded = {
		.instruction = 0xc131e104U, /* FCVTZS { Z4.S-Z7.S }, { Z8.S-Z11.S } */
		.decode_class = ORLIX_TCTI_DECODE_SME_FP,
		.sme_fp_source_ordinal = 2120U,
	};
	static const u32 source[] = { 0x3fc00000U, 0x40200000U,
		0x40600000U, 0x40900000U };
	u8 register_index;

	memset(&current->thread.user_sve, 0, sizeof(current->thread.user_sve));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sve_state_reset(&current->thread.user_sve,
		current->thread.user_simd, 16U));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sme_state_reset(&current->thread.user_sme,
		16U, true, false, false));
	for (register_index = 0; register_index < 4U; register_index++)
		put_unaligned_le32(source[register_index],
			&current->thread.user_sve.z[8U + register_index][0]);
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_sme_fp(&regs, &decoded));
	for (register_index = 0; register_index < 4U; register_index++) {
		KUNIT_EXPECT_EQ(test, 1U + register_index,
			get_unaligned_le32(&current->thread.user_sve.z[4U + register_index][0]));
		KUNIT_EXPECT_EQ(test, source[register_index],
			get_unaligned_le32(&current->thread.user_sve.z[8U + register_index][0]));
	}
	KUNIT_EXPECT_EQ(test, 0x6764UL, regs.pc);
	orlix_tcti_sme_state_release(&current->thread.user_sme);
}

/* Independent BF16 narrow results: 1.0 -> 0x3f80 and 1.5 -> 0x3fc0.
 * BFCVT concatenates its two source vectors; BFCVTN interleaves them from
 * the first half-width destination lane. */
static void orlix_tcti_sme_fp_bf16_narrow_vectors(struct kunit *test)
{
	struct pt_regs regs = { .pc = 0x6800 };
	struct orlix_tcti_decoded_instruction decoded = {
		.instruction = 0xc160e040U, /* BFCVT MZ2 base 0, source base 2. */
		.decode_class = ORLIX_TCTI_DECODE_SME_FP,
		.sme_fp_source_ordinal = 2091U,
	};

	memset(&current->thread.user_sve, 0, sizeof(current->thread.user_sve));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sve_state_reset(&current->thread.user_sve,
		current->thread.user_simd, 16U));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sme_state_reset(&current->thread.user_sme,
		16U, true, false, false));
	put_unaligned_le32(0x3f800000U, &current->thread.user_sve.z[2][0]);
	put_unaligned_le32(0x3fc00000U, &current->thread.user_sve.z[2][4]);
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_sme_fp(&regs, &decoded));
	KUNIT_EXPECT_EQ(test, 0x3f80U, get_unaligned_le16(&current->thread.user_sve.z[0][0]));
	KUNIT_EXPECT_EQ(test, 0x3fc0U, get_unaligned_le16(&current->thread.user_sve.z[0][2]));

	decoded.instruction = 0xc160e060U; /* BFCVTN, same MZ2 fields. */
	decoded.sme_fp_source_ordinal = 2093U;
	put_unaligned_le32(0x3fc00000U, &current->thread.user_sve.z[3][0]);
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_sme_fp(&regs, &decoded));
	KUNIT_EXPECT_EQ(test, 0x3f80U, get_unaligned_le16(&current->thread.user_sve.z[0][0]));
	KUNIT_EXPECT_EQ(test, 0x3fc0U, get_unaligned_le16(&current->thread.user_sve.z[0][2]));
	KUNIT_EXPECT_EQ(test, 0x6808UL, regs.pc);
	orlix_tcti_sme_state_release(&current->thread.user_sme);
}

/* BFCVT/BFCVTN read the two-vector Zn group but write one ordinary Zd.
 * BFCVT concatenates source results; BFCVTN interleaves them.  Zd=Z1 makes
 * the former erroneous grouped-destination implementation observable. */
static void orlix_tcti_sme_fp_narrow_single_destination_layout(struct kunit *test)
{
	struct pt_regs regs = { .pc = 0x6840 };
	struct orlix_tcti_decoded_instruction decoded = {
		.instruction = 0xc160e081U, /* BFCVT Z1.H, { Z2.S, Z3.S } */
		.decode_class = ORLIX_TCTI_DECODE_SME_FP,
		.sme_fp_source_ordinal = 2091U,
	};

	memset(&current->thread.user_sve, 0, sizeof(current->thread.user_sve));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sve_state_reset(&current->thread.user_sve,
		current->thread.user_simd, 16U));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sme_state_reset(&current->thread.user_sme,
		16U, true, false, false));
	put_unaligned_le32(0x3f800000U, &current->thread.user_sve.z[2][0]);
	put_unaligned_le32(0x40000000U, &current->thread.user_sve.z[3][0]);
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_sme_fp(&regs, &decoded));
	KUNIT_EXPECT_EQ(test, 0x3f80U,
		get_unaligned_le16(&current->thread.user_sve.z[1][0]));
	KUNIT_EXPECT_EQ(test, 0x4000U,
		get_unaligned_le16(&current->thread.user_sve.z[1][8]));

	decoded.instruction = 0xc160e0a1U; /* BFCVTN Z1.H, { Z2.S, Z3.S } */
	decoded.sme_fp_source_ordinal = 2093U;
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_sme_fp(&regs, &decoded));
	KUNIT_EXPECT_EQ(test, 0x3f80U,
		get_unaligned_le16(&current->thread.user_sve.z[1][0]));
	KUNIT_EXPECT_EQ(test, 0x4000U,
		get_unaligned_le16(&current->thread.user_sve.z[1][2]));
	KUNIT_EXPECT_EQ(test, 0x6848UL, regs.pc);
	orlix_tcti_sme_state_release(&current->thread.user_sme);
}

static struct kunit_case orlix_tcti_sme_fp_semantics_test_cases[] = {
	KUNIT_CASE(orlix_tcti_sme_fp_fmax_2x1_vector),
	KUNIT_CASE(orlix_tcti_sme_fp_minmax_group_and_fp16_vectors),
	KUNIT_CASE(orlix_tcti_sme_fp_fclamp_fp16_vector),
	KUNIT_CASE(orlix_tcti_sme_fp_famax_forces_denormal_controls_off),
	KUNIT_CASE(orlix_tcti_sme_fp_requires_streaming_state),
	KUNIT_CASE(orlix_tcti_sme_fp_fadd_za_2x2_vector),
	KUNIT_CASE(orlix_tcti_sme_fp_za_group_and_row_fields_vector),
	KUNIT_CASE(orlix_tcti_sme_fp_bfadd_za_special_values),
	KUNIT_CASE(orlix_tcti_sme_fp_bfadd_za_4x4_vectors),
	KUNIT_CASE(orlix_tcti_sme_fp_za_add_sub_source_rows),
	KUNIT_CASE(orlix_tcti_sme_fp_long_matrix_source_rows),
	KUNIT_CASE(orlix_tcti_sme_fp_matrix_4x4_source_rows),
	KUNIT_CASE(orlix_tcti_sme_fp_bf16_family_all_ordinals),
	KUNIT_CASE(orlix_tcti_sme_fp_bf16_special_values),
	KUNIT_CASE(orlix_tcti_sme_fp_bf16_zzw_high_source_groups),
	KUNIT_CASE(orlix_tcti_sme_fp_conversion_and_rounding_vectors),
	KUNIT_CASE(orlix_tcti_sme_fp_width_conversion_layout_vectors),
	KUNIT_CASE(orlix_tcti_sme_fp_mz4_group_fields_vector),
	KUNIT_CASE(orlix_tcti_sme_fp_bf16_narrow_vectors),
	KUNIT_CASE(orlix_tcti_sme_fp_narrow_single_destination_layout),
	{}
};

static struct kunit_suite orlix_tcti_sme_fp_semantics_test_suite = {
	.name = "orlix-tcti-sme-fp-semantics",
	.test_cases = orlix_tcti_sme_fp_semantics_test_cases,
};

kunit_test_suite(orlix_tcti_sme_fp_semantics_test_suite);

MODULE_LICENSE("GPL");
