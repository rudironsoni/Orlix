/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * SVE decoder substrate tests only. These tests are not ISA proof and must not
 * back source-leaf proof or HWCAP promotion until the official pinned ASL and
 * production tcti_resume_user evidence are complete.
 */
#include <kunit/test.h>
#include <linux/errno.h>

#include "../decode_aarch64.h"

struct tcti_sve_decode_case {
	u32 source_ordinal;
	u8 opcode;
	enum tcti_sve_integer_binary_op op;
};

static const struct tcti_sve_decode_case tcti_sve_decode_cases[] = {
	{ 0U, 0, TCTI_SVE_INTEGER_ADD },
	{ 1U, 1, TCTI_SVE_INTEGER_SUB },
	{ 2U, 3, TCTI_SVE_INTEGER_SUBR },
	{ 5U, 8, TCTI_SVE_INTEGER_SMAX },
	{ 6U, 10, TCTI_SVE_INTEGER_SMIN },
	{ 7U, 12, TCTI_SVE_INTEGER_SABD },
	{ 8U, 9, TCTI_SVE_INTEGER_UMAX },
	{ 9U, 11, TCTI_SVE_INTEGER_UMIN },
	{ 10U, 13, TCTI_SVE_INTEGER_UABD },
	{ 11U, 16, TCTI_SVE_INTEGER_MUL },
	{ 12U, 18, TCTI_SVE_INTEGER_SMULH },
	{ 13U, 19, TCTI_SVE_INTEGER_UMULH },
	{ 14U, 20, TCTI_SVE_INTEGER_SDIV },
	{ 15U, 22, TCTI_SVE_INTEGER_SDIVR },
	{ 16U, 21, TCTI_SVE_INTEGER_UDIV },
	{ 17U, 23, TCTI_SVE_INTEGER_UDIVR },
	{ 18U, 24, TCTI_SVE_INTEGER_ORR },
	{ 19U, 25, TCTI_SVE_INTEGER_EOR },
	{ 20U, 26, TCTI_SVE_INTEGER_AND },
	{ 21U, 27, TCTI_SVE_INTEGER_BIC },
};

static u32 tcti_sve_encode_predicated_integer_binary(u8 opcode,
					      u8 size, u8 pg, u8 zm, u8 zd)
{
	return AARCH64_SVE_PREDICATED_INTEGER_BINARY | ((u32)size << 22) |
	       ((u32)opcode << 16) |
	       ((u32)pg << 10) | ((u32)zm << 5) | zd;
}

static void tcti_sve_decode_exhausts_owned_legal_encodings(struct kunit *test)
{
	size_t case_index;

	for (case_index = 0; case_index < ARRAY_SIZE(tcti_sve_decode_cases);
	     case_index++) {
		const struct tcti_sve_decode_case *test_case =
			&tcti_sve_decode_cases[case_index];
		u8 size;

		for (size = 0; size < 4; size++) {
			u8 pg;

			for (pg = 0; pg < 8; pg++) {
				u8 zm;

				for (zm = 0; zm < 32; zm++) {
					u8 zd;

					for (zd = 0; zd < 32; zd++) {
						u32 instruction =
							tcti_sve_encode_predicated_integer_binary(
								test_case->opcode,
								size, pg, zm, zd);
						struct tcti_sve_predicated_integer_binary decoded;

						KUNIT_ASSERT_EQ(test, 0,
							tcti_decode_sve_predicated_integer_binary(
								instruction, &decoded));
						KUNIT_EXPECT_EQ_MSG(test, test_case->op, decoded.op,
							"source ordinal %u", test_case->source_ordinal);
						KUNIT_EXPECT_EQ(test,
							TCTI_SVE_PREDICATE_MERGING,
							decoded.predication);
						KUNIT_EXPECT_EQ(test, 1U << size,
							decoded.element_bytes);
						KUNIT_EXPECT_EQ(test, zd, decoded.zd);
						KUNIT_EXPECT_EQ(test, zd, decoded.zn);
						KUNIT_EXPECT_EQ(test, zm, decoded.zm);
						KUNIT_EXPECT_EQ(test, pg, decoded.pg);
					}
				}
			}
		}
	}
}

static void tcti_sve_decode_rejects_all_reserved_opcodes(struct kunit *test)
{
	u8 size;

	for (size = 0; size < 4; size++) {
		u8 opcode;

		for (opcode = 0; opcode < 32; opcode++) {
			bool legal = opcode == 0 || opcode == 1 || opcode == 3 ||
				opcode == 8 || opcode == 9 || opcode == 10 || opcode == 11 ||
				opcode == 12 || opcode == 13 || opcode == 16 || opcode == 18 ||
				opcode == 19 || opcode == 20 || opcode == 21 || opcode == 22 ||
				opcode == 23 || opcode == 24 || opcode == 25 || opcode == 26 ||
				opcode == 27;

			if (!legal)
				KUNIT_EXPECT_EQ(test, -EINVAL,
					tcti_decode_sve_predicated_integer_binary(
						tcti_sve_encode_predicated_integer_binary(
							opcode, size, 7, 31, 31),
						&(struct tcti_sve_predicated_integer_binary){}));
		}
	}
}

static void tcti_sve_decode_rejects_fixed_field_corruption(struct kunit *test)
{
	static const u32 fixed_bits[] = {
		0x80000000U, 0x40000000U, 0x20000000U, 0x10000000U,
		0x08000000U, 0x04000000U, 0x02000000U, 0x01000000U,
		0x00100000U, 0x00080000U, 0x00008000U, 0x00004000U,
		0x00002000U,
	};
	u32 valid = tcti_sve_encode_predicated_integer_binary(
		0, 3, 7, 31, 31);
	size_t index;

	KUNIT_ASSERT_EQ(test, 0,
		tcti_decode_sve_predicated_integer_binary(valid,
			&(struct tcti_sve_predicated_integer_binary){}));
	for (index = 0; index < ARRAY_SIZE(fixed_bits); index++)
		KUNIT_EXPECT_EQ(test, -ENOENT,
			tcti_decode_sve_predicated_integer_binary(valid ^ fixed_bits[index],
				&(struct tcti_sve_predicated_integer_binary){}));
}

static void tcti_sve_decode_rejects_null_descriptor(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, -EINVAL,
		tcti_decode_sve_predicated_integer_binary(
			AARCH64_SVE_PREDICATED_INTEGER_BINARY, NULL));
}

static void tcti_sve_main_decoder_preserves_owned_and_reserved_boundaries(
	struct kunit *test)
{
	struct tcti_decoded_instruction decoded;
	u32 instruction;

	instruction = tcti_sve_encode_predicated_integer_binary(
		26, 3, 7, 29, 13);
	decoded = tcti_decode_aarch64(instruction);
	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SVE_PREDICATED_INTEGER_BINARY,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_SVE_INTEGER_AND,
			decoded.sve_integer_binary_op);
	KUNIT_EXPECT_EQ(test, TCTI_SVE_PREDICATE_MERGING,
			decoded.sve_predication);
	KUNIT_EXPECT_EQ(test, 13, decoded.rd);
	KUNIT_EXPECT_EQ(test, 13, decoded.rn);
	KUNIT_EXPECT_EQ(test, 29, decoded.rm);
	KUNIT_EXPECT_EQ(test, 7, decoded.sve_pg);
	KUNIT_EXPECT_EQ(test, 8, decoded.sve_element_bytes);

	instruction = tcti_sve_encode_predicated_integer_binary(
		2, 0, 0, 0, 0);
	decoded = tcti_decode_aarch64(instruction);
	KUNIT_EXPECT_EQ(test, TCTI_DECODE_UNSUPPORTED, decoded.decode_class);
}

static struct kunit_case tcti_sve_decode_test_cases[] = {
	KUNIT_CASE(tcti_sve_decode_exhausts_owned_legal_encodings),
	KUNIT_CASE(tcti_sve_decode_rejects_all_reserved_opcodes),
	KUNIT_CASE(tcti_sve_decode_rejects_fixed_field_corruption),
	KUNIT_CASE(tcti_sve_decode_rejects_null_descriptor),
	KUNIT_CASE(tcti_sve_main_decoder_preserves_owned_and_reserved_boundaries),
	{}
};

static struct kunit_suite tcti_sve_decode_test_suite = {
	.name = "orlix-tcti-sve-decode",
	.test_cases = tcti_sve_decode_test_cases,
};

kunit_test_suite(tcti_sve_decode_test_suite);

MODULE_LICENSE("GPL");
