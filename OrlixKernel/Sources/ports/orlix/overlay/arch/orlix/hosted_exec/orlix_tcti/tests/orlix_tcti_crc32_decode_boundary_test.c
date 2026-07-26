// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/bitops.h>
#include <linux/string.h>
#include <asm/ptrace.h>

#include "../decode_aarch64.h"
#include "../switch_debug.h"
#include "orlix_tcti_test_suites.h"

struct orlix_tcti_crc32_decode_leaf {
	const char *mnemonic;
	u32 fixed_mask;
	u32 value;
	enum orlix_tcti_data_processing_2source_op operation;
	u8 access_size;
	bool is_64bit;
};

static const struct orlix_tcti_crc32_decode_leaf orlix_tcti_crc32_decode_leaves[] = {
	{ "CRC32B",  0xffe0fc00U, 0x1ac04000U, ORLIX_TCTI_DP2_CRC32,  1, false },
	{ "CRC32H",  0xffe0fc00U, 0x1ac04400U, ORLIX_TCTI_DP2_CRC32,  2, false },
	{ "CRC32W",  0xffe0fc00U, 0x1ac04800U, ORLIX_TCTI_DP2_CRC32,  4, false },
	{ "CRC32X",  0xffe0fc00U, 0x9ac04c00U, ORLIX_TCTI_DP2_CRC32,  8, true },
	{ "CRC32CB", 0xffe0fc00U, 0x1ac05000U, ORLIX_TCTI_DP2_CRC32C, 1, false },
	{ "CRC32CH", 0xffe0fc00U, 0x1ac05400U, ORLIX_TCTI_DP2_CRC32C, 2, false },
	{ "CRC32CW", 0xffe0fc00U, 0x1ac05800U, ORLIX_TCTI_DP2_CRC32C, 4, false },
	{ "CRC32CX", 0xffe0fc00U, 0x9ac05c00U, ORLIX_TCTI_DP2_CRC32C, 8, true },
};

static u32 orlix_tcti_crc32_decode_with_registers(u32 instruction)
{
	return instruction | 17U | (9U << 5) | (3U << 16);
}

static const struct orlix_tcti_crc32_decode_leaf *
orlix_tcti_crc32_decode_expected_leaf(u32 instruction)
{
	unsigned int index;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_crc32_decode_leaves); index++) {
		const struct orlix_tcti_crc32_decode_leaf *leaf =
			&orlix_tcti_crc32_decode_leaves[index];

		if ((instruction & leaf->fixed_mask) == leaf->value)
			return leaf;
	}
	return NULL;
}

static u32 orlix_tcti_crc32_decode_update(u32 accumulator, u64 value,
	u8 byte_count, bool castagnoli)
{
	u32 polynomial = castagnoli ? 0x82f63b78U : 0xedb88320U;
	u8 byte;

	for (byte = 0; byte < byte_count; byte++) {
		u8 bit;

		accumulator ^= value >> (byte * 8);
		for (bit = 0; bit < 8; bit++)
			accumulator = (accumulator >> 1) ^
				((accumulator & 1U) ? polynomial : 0);
	}
	return accumulator;
}

static void orlix_tcti_crc32_decode_expect_leaf(struct kunit *test,
	const struct orlix_tcti_crc32_decode_leaf *leaf, u32 instruction)
{
	struct orlix_tcti_decoded_instruction decoded =
		orlix_tcti_decode_aarch64(instruction);

	KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_DECODE_DATA_PROCESSING_2SOURCE,
			    decoded.decode_class, "%s %#x", leaf->mnemonic,
			    instruction);
	KUNIT_EXPECT_EQ_MSG(test, leaf->operation, decoded.dp2_op,
			    "%s %#x", leaf->mnemonic, instruction);
	KUNIT_EXPECT_EQ_MSG(test, 17U, decoded.rd, "%s %#x", leaf->mnemonic,
			    instruction);
	KUNIT_EXPECT_EQ_MSG(test, 9U, decoded.rn, "%s %#x", leaf->mnemonic,
			    instruction);
	KUNIT_EXPECT_EQ_MSG(test, 3U, decoded.rm, "%s %#x", leaf->mnemonic,
			    instruction);
	KUNIT_EXPECT_EQ_MSG(test, leaf->access_size, decoded.access_size,
			    "%s %#x", leaf->mnemonic, instruction);
	KUNIT_EXPECT_EQ_MSG(test, sizeof(u32), decoded.result_size,
			    "%s %#x", leaf->mnemonic, instruction);
	KUNIT_EXPECT_EQ_MSG(test, leaf->is_64bit, decoded.is_64bit,
			    "%s %#x", leaf->mnemonic, instruction);
}

static void orlix_tcti_crc32_decode_accepts_all_source_leaves(struct kunit *test)
{
	unsigned int index;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_crc32_decode_leaves); index++) {
		const struct orlix_tcti_crc32_decode_leaf *leaf =
			&orlix_tcti_crc32_decode_leaves[index];

		orlix_tcti_crc32_decode_expect_leaf(test, leaf,
			orlix_tcti_crc32_decode_with_registers(leaf->value));
	}
}

static void orlix_tcti_crc32_decode_rejects_source_fixed_neighbours(
	struct kunit *test)
{
	unsigned int index;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_crc32_decode_leaves); index++) {
		const struct orlix_tcti_crc32_decode_leaf *leaf =
			&orlix_tcti_crc32_decode_leaves[index];
		u8 bit;

		for (bit = 0; bit < 32; bit++) {
			u32 instruction;
			const struct orlix_tcti_crc32_decode_leaf *expected;
			struct orlix_tcti_decoded_instruction decoded;

			if (!(leaf->fixed_mask & BIT(bit)))
				continue;
			instruction = orlix_tcti_crc32_decode_with_registers(
				leaf->value ^ BIT(bit));
			expected = orlix_tcti_crc32_decode_expected_leaf(instruction);
			decoded = orlix_tcti_decode_aarch64(instruction);
			if (expected) {
				KUNIT_EXPECT_EQ_MSG(test,
					ORLIX_TCTI_DECODE_DATA_PROCESSING_2SOURCE,
					decoded.decode_class,
					"legal neighbour %#x of %s", instruction,
					leaf->mnemonic);
				KUNIT_EXPECT_EQ_MSG(test, expected->operation,
					decoded.dp2_op, "legal neighbour %#x of %s",
					instruction, leaf->mnemonic);
				continue;
			}
			KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
					decoded.decode_class,
					"reserved neighbour %#x of %s", instruction,
					leaf->mnemonic);
		}
	}
}

static void orlix_tcti_crc32_decode_rejects_width_mode_mismatches(
	struct kunit *test)
{
	unsigned int index;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_crc32_decode_leaves); index++) {
		const struct orlix_tcti_crc32_decode_leaf *leaf =
			&orlix_tcti_crc32_decode_leaves[index];
		u32 instruction = orlix_tcti_crc32_decode_with_registers(
			leaf->value ^ BIT(31));

		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
			orlix_tcti_decode_aarch64(instruction).decode_class,
			"invalid width mode for %s", leaf->mnemonic);
	}
}

static void orlix_tcti_crc32_decode_executor_vectors(struct kunit *test)
{
	static const u32 expected[] = {
		0x347cedaaU, 0xe35777ffU, 0xd89f1a03U, 0x4e41e95aU,
		0x19667cf8U, 0xb828afefU, 0x9236d411U, 0x12237ce0U,
	};
	unsigned int index;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_crc32_decode_leaves); index++) {
		const struct orlix_tcti_crc32_decode_leaf *leaf =
			&orlix_tcti_crc32_decode_leaves[index];
		struct orlix_tcti_decoded_instruction decoded = orlix_tcti_decode_aarch64(
			orlix_tcti_crc32_decode_with_registers(leaf->value));
		struct pt_regs regs = {};
		struct pt_regs before;
		u64 expected_registers[ARRAY_SIZE(regs.regs)];
		u32 computed;
		unsigned int reg;

		for (reg = 0; reg < ARRAY_SIZE(regs.regs); reg++)
			regs.regs[reg] = 0x8877665544332211ULL ^
				((u64)reg << 40);
		regs.regs[9] = 0xfeedface12345678ULL;
		regs.regs[3] = 0x8877665544332211ULL;
		regs.sp = 0x706a865abcULL;
		regs.pc = 0x2468ace000ULL;
		regs.pstate = PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT | PSR_V_BIT |
			0x155UL;
		before = regs;
		memcpy(expected_registers, regs.regs, sizeof(expected_registers));
		computed = orlix_tcti_crc32_decode_update((u32)regs.regs[9],
			regs.regs[3], leaf->access_size,
			leaf->operation == ORLIX_TCTI_DP2_CRC32C);
		expected_registers[17] = computed;

		KUNIT_ASSERT_EQ_MSG(test, 0,
			orlix_tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL),
			"%s", leaf->mnemonic);
		KUNIT_EXPECT_EQ_MSG(test, expected[index], computed,
			"known vector expectation for %s", leaf->mnemonic);
		KUNIT_EXPECT_MEMEQ(test, expected_registers, regs.regs,
				   sizeof(expected_registers));
		KUNIT_EXPECT_EQ(test, before.sp, regs.sp);
		KUNIT_EXPECT_EQ(test, before.pstate, regs.pstate);
		KUNIT_EXPECT_EQ(test, before.pc + sizeof(u32), regs.pc);
	}
}

static struct kunit_case orlix_tcti_crc32_decode_boundary_test_cases[] = {
	KUNIT_CASE(orlix_tcti_crc32_decode_accepts_all_source_leaves),
	KUNIT_CASE(orlix_tcti_crc32_decode_rejects_source_fixed_neighbours),
	KUNIT_CASE(orlix_tcti_crc32_decode_rejects_width_mode_mismatches),
	KUNIT_CASE(orlix_tcti_crc32_decode_executor_vectors),
	{}
};

struct kunit_suite orlix_tcti_crc32_decode_boundary_test_suite = {
	.name = "orlix-tcti-crc32-decode-boundary",
	.test_cases = orlix_tcti_crc32_decode_boundary_test_cases,
};

kunit_test_suite(orlix_tcti_crc32_decode_boundary_test_suite);
