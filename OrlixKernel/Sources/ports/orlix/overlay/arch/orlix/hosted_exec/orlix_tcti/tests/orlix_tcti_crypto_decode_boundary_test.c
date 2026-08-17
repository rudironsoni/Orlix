// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/bitops.h>
#include <linux/sched.h>
#include <asm/ptrace.h>

#include "../decode_aarch64.h"
#include "../switch_debug.h"
#include "orlix_tcti_test_suites.h"

/*
 * These are the current classic AdvSIMD crypto source leaves, expressed as
 * source-derived fixed-bit fingerprints.  A sibling source fingerprint is a
 * legal neighbour only when it resolves to another entry in this table.
 */
struct orlix_tcti_crypto_decode_leaf {
	u32 source_ordinal;
	const char *source_id;
	const char *mnemonic;
	u32 fixed_mask;
	u32 value;
	enum orlix_tcti_simd_vector_arithmetic_op operation;
	u8 access_size;
	u8 result_size;
	bool scalar;
	bool has_rm;
};

static const struct orlix_tcti_crypto_decode_leaf orlix_tcti_crypto_decode_leaves[] = {
	{ 3507U, "AESE_B_cryptoaes", "AESE", 0xfffffc00U, 0x4e284800U,
	  ORLIX_TCTI_SIMD_ARITH_AESE,
	  16, 16, false, false },
	{ 3508U, "AESD_B_cryptoaes", "AESD", 0xfffffc00U, 0x4e285800U,
	  ORLIX_TCTI_SIMD_ARITH_AESD,
	  16, 16, false, false },
	{ 3509U, "AESMC_B_cryptoaes", "AESMC", 0xfffffc00U, 0x4e286800U,
	  ORLIX_TCTI_SIMD_ARITH_AESMC,
	  16, 16, false, false },
	{ 3510U, "AESIMC_B_cryptoaes", "AESIMC", 0xfffffc00U, 0x4e287800U,
	  ORLIX_TCTI_SIMD_ARITH_AESIMC,
	  16, 16, false, false },
	{ 3511U, "SHA1C_QSV_cryptosha3", "SHA1C", 0xffe0fc00U, 0x5e000000U,
	  ORLIX_TCTI_SIMD_ARITH_SHA1C,
	  16, 16, false, true },
	{ 3512U, "SHA1P_QSV_cryptosha3", "SHA1P", 0xffe0fc00U, 0x5e001000U,
	  ORLIX_TCTI_SIMD_ARITH_SHA1P,
	  16, 16, false, true },
	{ 3513U, "SHA1M_QSV_cryptosha3", "SHA1M", 0xffe0fc00U, 0x5e002000U,
	  ORLIX_TCTI_SIMD_ARITH_SHA1M,
	  16, 16, false, true },
	{ 3514U, "SHA1SU0_VVV_cryptosha3", "SHA1SU0", 0xffe0fc00U,
	  0x5e003000U, ORLIX_TCTI_SIMD_ARITH_SHA1SU0,
	  16, 16, false, true },
	{ 3518U, "SHA1H_SS_cryptosha2", "SHA1H", 0xfffffc00U, 0x5e280800U,
	  ORLIX_TCTI_SIMD_ARITH_SHA1H,
	  4, 4, true, false },
	{ 3519U, "SHA1SU1_VV_cryptosha2", "SHA1SU1", 0xfffffc00U,
	  0x5e281800U, ORLIX_TCTI_SIMD_ARITH_SHA1SU1,
	  16, 16, false, false },
	{ 3515U, "SHA256H_QQV_cryptosha3", "SHA256H", 0xffe0fc00U,
	  0x5e004000U, ORLIX_TCTI_SIMD_ARITH_SHA256H,
	  16, 16, false, true },
	{ 3516U, "SHA256H2_QQV_cryptosha3", "SHA256H2", 0xffe0fc00U,
	  0x5e005000U, ORLIX_TCTI_SIMD_ARITH_SHA256H2,
	  16, 16, false, true },
	{ 3517U, "SHA256SU1_VVV_cryptosha3", "SHA256SU1", 0xffe0fc00U,
	  0x5e006000U, ORLIX_TCTI_SIMD_ARITH_SHA256SU1,
	  16, 16, false, true },
	{ 3520U, "SHA256SU0_VV_cryptosha2", "SHA256SU0", 0xfffffc00U,
	  0x5e282800U, ORLIX_TCTI_SIMD_ARITH_SHA256SU0,
	  16, 16, false, false },
	{ 3883U, "PMULL_asimddiff_L", "PMULL", 0xbf20fc00U, 0x0e20e000U,
	  ORLIX_TCTI_SIMD_ARITH_PMULL,
	  1, 16, false, true },
	{ 3955U, "PMUL_asimdsame_only", "PMUL", 0xbf20fc00U, 0x2e209c00U,
	  ORLIX_TCTI_SIMD_ARITH_PMUL,
	  1, 8, false, true },
};

static const struct orlix_tcti_crypto_decode_leaf *
orlix_tcti_crypto_decode_expected_leaf(u32 instruction)
{
	unsigned int index;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_crypto_decode_leaves);
	     index++) {
		const struct orlix_tcti_crypto_decode_leaf *leaf =
			&orlix_tcti_crypto_decode_leaves[index];

		if ((instruction & leaf->fixed_mask) == leaf->value)
			return leaf;
	}

	return NULL;
}

static void orlix_tcti_crypto_decode_expect_leaf(struct kunit *test,
	const struct orlix_tcti_crypto_decode_leaf *leaf, u32 instruction,
	u8 expected_access_size, u8 expected_result_size, u8 expected_source_index)
{
	struct orlix_tcti_decoded_instruction decoded =
		orlix_tcti_decode_aarch64(instruction);

	KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC,
			    decoded.decode_class, "%s (%u, %s) instruction %#x",
			    leaf->mnemonic, leaf->source_ordinal, leaf->source_id,
			    instruction);
	KUNIT_EXPECT_EQ_MSG(test, leaf->operation, decoded.simd_arithmetic_op,
			    "%s (%u, %s) instruction %#x", leaf->mnemonic,
			    leaf->source_ordinal, leaf->source_id, instruction);
	KUNIT_EXPECT_EQ_MSG(test, 17U, decoded.rd, "%s instruction %#x",
			    leaf->mnemonic, instruction);
	KUNIT_EXPECT_EQ_MSG(test, 9U, decoded.rn, "%s instruction %#x",
			    leaf->mnemonic, instruction);
	if (leaf->has_rm)
		KUNIT_EXPECT_EQ_MSG(test, 3U, decoded.rm, "%s instruction %#x",
				    leaf->mnemonic, instruction);
	else
		KUNIT_EXPECT_EQ_MSG(test, 0U, decoded.rm, "%s instruction %#x",
				    leaf->mnemonic, instruction);
	KUNIT_EXPECT_EQ_MSG(test, expected_access_size, decoded.access_size,
			    "%s instruction %#x", leaf->mnemonic, instruction);
	KUNIT_EXPECT_EQ_MSG(test, expected_result_size, decoded.result_size,
			    "%s instruction %#x", leaf->mnemonic, instruction);
	KUNIT_EXPECT_EQ_MSG(test, expected_source_index,
			    decoded.simd_source_index, "%s instruction %#x",
			    leaf->mnemonic, instruction);
	KUNIT_EXPECT_TRUE_MSG(test, decoded.simd_fp, "%s instruction %#x",
			      leaf->mnemonic, instruction);
	KUNIT_EXPECT_EQ_MSG(test, leaf->scalar, decoded.simd_scalar,
			    "%s instruction %#x", leaf->mnemonic, instruction);
}

static u32 orlix_tcti_crypto_decode_with_registers(
	const struct orlix_tcti_crypto_decode_leaf *leaf, u32 instruction)
{
	instruction |= 17U | (9U << 5);
	if (leaf->has_rm)
		instruction |= 3U << 16;
	return instruction;
}

static void orlix_tcti_crypto_decode_legal_source_fingerprints(struct kunit *test)
{
	unsigned int index;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_crypto_decode_leaves);
	     index++) {
		const struct orlix_tcti_crypto_decode_leaf *leaf =
			&orlix_tcti_crypto_decode_leaves[index];
		u32 instruction = orlix_tcti_crypto_decode_with_registers(leaf,
			leaf->value);

		orlix_tcti_crypto_decode_expect_leaf(test, leaf, instruction,
			leaf->access_size, leaf->result_size, 0);
	}
}

static void orlix_tcti_crypto_decode_legal_polynomial_variants(struct kunit *test)
{
	const struct orlix_tcti_crypto_decode_leaf *pmull =
		&orlix_tcti_crypto_decode_leaves[14];
	const struct orlix_tcti_crypto_decode_leaf *pmul =
		&orlix_tcti_crypto_decode_leaves[15];
	u8 source_index;

	for (source_index = 0; source_index < 2; source_index++) {
		u32 q = source_index ? BIT(30) : 0;

		orlix_tcti_crypto_decode_expect_leaf(test, pmul,
			orlix_tcti_crypto_decode_with_registers(pmul, pmul->value | q),
			1, source_index ? 16 : 8, source_index);
		orlix_tcti_crypto_decode_expect_leaf(test, pmull,
			orlix_tcti_crypto_decode_with_registers(pmull, pmull->value | q),
			1, 16, source_index);
		orlix_tcti_crypto_decode_expect_leaf(test, pmull,
			orlix_tcti_crypto_decode_with_registers(pmull,
				pmull->value | q | (3U << 22)),
			8, 16, source_index);
	}
}

static void orlix_tcti_crypto_decode_rejects_fixed_bit_neighbours(struct kunit *test)
{
	unsigned int index;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_crypto_decode_leaves);
	     index++) {
		const struct orlix_tcti_crypto_decode_leaf *leaf =
			&orlix_tcti_crypto_decode_leaves[index];
		u8 bit;

		for (bit = 0; bit < 32; bit++) {
			u32 mutation;
			const struct orlix_tcti_crypto_decode_leaf *expected;
			struct orlix_tcti_decoded_instruction decoded;

			if (!(leaf->fixed_mask & BIT(bit)))
				continue;
			mutation = orlix_tcti_crypto_decode_with_registers(leaf,
				leaf->value ^ BIT(bit));
			expected = orlix_tcti_crypto_decode_expected_leaf(mutation);
			decoded = orlix_tcti_decode_aarch64(mutation);
			if (expected) {
				KUNIT_EXPECT_EQ_MSG(test,
					ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC,
					decoded.decode_class,
					"legal neighbour %#x of %s (%u, %s)",
					mutation, leaf->mnemonic, leaf->source_ordinal,
					leaf->source_id);
				KUNIT_EXPECT_EQ_MSG(test, expected->operation,
					decoded.simd_arithmetic_op,
					"legal neighbour %#x of %s (%u, %s)",
					mutation, leaf->mnemonic, leaf->source_ordinal,
					leaf->source_id);
				continue;
			}
			/*
			 * A single fixed-bit flip can become a different legal
			 * A64 encoding (LDR literal, TBL, THREE_SAME). Keep
			 * the original crypto leaf from surviving that flip.
			 */
			KUNIT_EXPECT_TRUE_MSG(test,
				decoded.decode_class !=
					ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC ||
				decoded.simd_arithmetic_op != leaf->operation,
				"retained %s after reserved mutation %#x",
				leaf->mnemonic, mutation);
		}
	}
}

static void orlix_tcti_crypto_decode_rejects_reserved_polynomial_sizes(
	struct kunit *test)
{
	const struct orlix_tcti_crypto_decode_leaf *pmull =
		&orlix_tcti_crypto_decode_leaves[14];
	const struct orlix_tcti_crypto_decode_leaf *pmul =
		&orlix_tcti_crypto_decode_leaves[15];
	u8 source_index;
	u8 size;

	for (source_index = 0; source_index < 2; source_index++) {
		u32 q = source_index ? BIT(30) : 0;

		for (size = 1; size < 4; size++) {
			u32 instruction = orlix_tcti_crypto_decode_with_registers(pmul,
				pmul->value | q | ((u32)size << 22));

			KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
				orlix_tcti_decode_aarch64(instruction).decode_class,
				"reserved PMUL (%u, %s) size=%u q=%u",
				pmul->source_ordinal, pmul->source_id, size,
				source_index);
		}
		for (size = 1; size < 3; size++) {
			u32 instruction = orlix_tcti_crypto_decode_with_registers(pmull,
				pmull->value | q | ((u32)size << 22));

			KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
				orlix_tcti_decode_aarch64(instruction).decode_class,
				"reserved PMULL (%u, %s) size=%u q=%u",
				pmull->source_ordinal, pmull->source_id, size,
				source_index);
		}
	}
}

static void orlix_tcti_crypto_decode_executor_accepts_legal_fingerprints(
	struct kunit *test)
{
	unsigned int index;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_crypto_decode_leaves);
	     index++) {
		const struct orlix_tcti_crypto_decode_leaf *leaf =
			&orlix_tcti_crypto_decode_leaves[index];
		struct orlix_tcti_decoded_instruction decoded = orlix_tcti_decode_aarch64(
			orlix_tcti_crypto_decode_with_registers(leaf, leaf->value));
		struct pt_regs regs = { .pc = 0x4000 };
		unsigned int word;

		for (word = 0; word < ARRAY_SIZE(current->thread.user_simd);
		     word++)
			current->thread.user_simd[word] =
				0x0102030405060708ULL ^ ((u64)word << 32);
		current->thread.user_simd_valid = 0;
		KUNIT_ASSERT_EQ_MSG(test, 0,
			orlix_tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL),
			"%s", leaf->mnemonic);
		KUNIT_EXPECT_EQ_MSG(test, 0x4004ULL, regs.pc, "%s",
			leaf->mnemonic);
		KUNIT_EXPECT_EQ_MSG(test, 1, current->thread.user_simd_valid,
			"%s", leaf->mnemonic);
	}
}

static struct kunit_case orlix_tcti_crypto_decode_boundary_test_cases[] = {
	KUNIT_CASE(orlix_tcti_crypto_decode_legal_source_fingerprints),
	KUNIT_CASE(orlix_tcti_crypto_decode_legal_polynomial_variants),
	KUNIT_CASE(orlix_tcti_crypto_decode_rejects_fixed_bit_neighbours),
	KUNIT_CASE(orlix_tcti_crypto_decode_rejects_reserved_polynomial_sizes),
	KUNIT_CASE(orlix_tcti_crypto_decode_executor_accepts_legal_fingerprints),
	{}
};

struct kunit_suite orlix_tcti_crypto_decode_boundary_test_suite = {
	.name = "orlix-tcti-crypto-decode-boundary",
	.test_cases = orlix_tcti_crypto_decode_boundary_test_cases,
};

kunit_test_suite(orlix_tcti_crypto_decode_boundary_test_suite);
