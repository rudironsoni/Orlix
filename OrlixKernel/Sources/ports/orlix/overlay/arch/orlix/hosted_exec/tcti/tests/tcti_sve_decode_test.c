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
	u32 class;
	u8 opcode;
	enum tcti_sve_integer_binary_op op;
};

static const struct tcti_sve_decode_case tcti_sve_decode_cases[] = {
	{ AARCH64_SVE_PREDICATED_ARITHMETIC, 0, TCTI_SVE_INTEGER_ADD },
	{ AARCH64_SVE_PREDICATED_ARITHMETIC, 1, TCTI_SVE_INTEGER_SUB },
	{ AARCH64_SVE_PREDICATED_LOGICAL, 0, TCTI_SVE_INTEGER_ORR },
	{ AARCH64_SVE_PREDICATED_LOGICAL, 1, TCTI_SVE_INTEGER_EOR },
	{ AARCH64_SVE_PREDICATED_LOGICAL, 2, TCTI_SVE_INTEGER_AND },
};

static u32 tcti_sve_encode_predicated_integer_binary(u32 class, u8 opcode,
					      u8 size, u8 pg, u8 zm, u8 zd)
{
	return class | ((u32)size << 22) | ((u32)opcode << 16) |
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
								test_case->class,
								test_case->opcode,
								size, pg, zm, zd);
						struct tcti_sve_predicated_integer_binary decoded;

						KUNIT_ASSERT_EQ(test, 0,
							tcti_decode_sve_predicated_integer_binary(
								instruction, &decoded));
						KUNIT_EXPECT_EQ(test, test_case->op, decoded.op);
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
	static const u32 classes[] = {
		AARCH64_SVE_PREDICATED_ARITHMETIC,
		AARCH64_SVE_PREDICATED_LOGICAL,
	};
	u8 size;

	for (size = 0; size < 4; size++) {
		u8 class_index;

		for (class_index = 0; class_index < ARRAY_SIZE(classes);
		     class_index++) {
			u8 opcode;

			for (opcode = 0; opcode < 8; opcode++) {
				bool legal = (class_index == 0 && opcode < 2) ||
					(class_index == 1 && opcode < 3);
				u8 pg;

				if (legal)
					continue;
				for (pg = 0; pg < 8; pg++) {
					u8 zm;

					for (zm = 0; zm < 32; zm++) {
						u8 zd;

						for (zd = 0; zd < 32; zd++) {
							u32 instruction =
								tcti_sve_encode_predicated_integer_binary(
									classes[class_index], opcode,
									size, pg, zm, zd);

							KUNIT_EXPECT_EQ(test, -EINVAL,
								tcti_decode_sve_predicated_integer_binary(
									instruction,
									&(struct tcti_sve_predicated_integer_binary){}));
						}
					}
				}
			}
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
		AARCH64_SVE_PREDICATED_ARITHMETIC, 0, 3, 7, 31, 31);
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
			AARCH64_SVE_PREDICATED_ARITHMETIC, NULL));
}

static void tcti_sve_main_decoder_preserves_owned_and_reserved_boundaries(
	struct kunit *test)
{
	struct tcti_decoded_instruction decoded;
	u32 instruction;

	instruction = tcti_sve_encode_predicated_integer_binary(
		AARCH64_SVE_PREDICATED_LOGICAL, 2, 3, 7, 29, 13);
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
		AARCH64_SVE_PREDICATED_ARITHMETIC, 2, 0, 0, 0, 0);
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
