// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <asm/ptrace.h>

#include "../block_cache.h"
#include "../decode_aarch64.h"
#include "../gadget_program.h"
#include "../switch_debug.h"
#include "../tlb.h"

static void tcti_decode_recognizes_svc_zero(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;

	decoded = tcti_decode_aarch64(0xd4000001U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SVC, decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 0xd4000001U, decoded.instruction);
}

static void tcti_decode_rejects_unknown_instruction(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;

	decoded = tcti_decode_aarch64(0xffffffffU);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_UNSUPPORTED, decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 0xffffffffU, decoded.instruction);
}

static void tcti_decode_recognizes_hint_class(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;

	decoded = tcti_decode_aarch64(0xd503201fU);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_HINT, decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 0xd503201fU, decoded.instruction);

	decoded = tcti_decode_aarch64(0xd503203fU);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_HINT, decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 0xd503203fU, decoded.instruction);
}

static void tcti_decode_recognizes_add_sub_immediate_class(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;

	decoded = tcti_decode_aarch64(0x910003e0U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_ADD_SUB_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_TRUE(test, decoded.is_64bit);
	KUNIT_EXPECT_FALSE(test, decoded.subtract);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 31U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 0U, decoded.imm12);
	KUNIT_EXPECT_EQ(test, 0U, decoded.shift);

	decoded = tcti_decode_aarch64(0xd1001483U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_ADD_SUB_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_TRUE(test, decoded.is_64bit);
	KUNIT_EXPECT_TRUE(test, decoded.subtract);
	KUNIT_EXPECT_EQ(test, 3U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 4U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 5U, decoded.imm12);
	KUNIT_EXPECT_EQ(test, 0U, decoded.shift);

	decoded = tcti_decode_aarch64(0x7100111fU);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_ADD_SUB_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_FALSE(test, decoded.is_64bit);
	KUNIT_EXPECT_TRUE(test, decoded.subtract);
	KUNIT_EXPECT_TRUE(test, decoded.set_flags);
	KUNIT_EXPECT_EQ(test, 31U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 8U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 4U, decoded.imm12);
}

static void tcti_decode_recognizes_add_sub_shifted_register_class(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;

	decoded = tcti_decode_aarch64(0x8b000273U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_ADD_SUB_SHIFTED_REGISTER,
			decoded.decode_class);
	KUNIT_EXPECT_TRUE(test, decoded.is_64bit);
	KUNIT_EXPECT_FALSE(test, decoded.subtract);
	KUNIT_EXPECT_FALSE(test, decoded.set_flags);
	KUNIT_EXPECT_EQ(test, 19U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 19U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rm);
	KUNIT_EXPECT_EQ(test, 0U, decoded.shift);
	KUNIT_EXPECT_EQ(test, 0U, decoded.shift_amount);

	decoded = tcti_decode_aarch64(0xcb000294U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_ADD_SUB_SHIFTED_REGISTER,
			decoded.decode_class);
	KUNIT_EXPECT_TRUE(test, decoded.is_64bit);
	KUNIT_EXPECT_TRUE(test, decoded.subtract);
	KUNIT_EXPECT_EQ(test, 20U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 20U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rm);
}

static void tcti_decode_recognizes_add_sub_extended_register_class(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;

	decoded = tcti_decode_aarch64(0x8b284128U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_ADD_SUB_EXTENDED_REGISTER,
			decoded.decode_class);
	KUNIT_EXPECT_TRUE(test, decoded.is_64bit);
	KUNIT_EXPECT_FALSE(test, decoded.subtract);
	KUNIT_EXPECT_FALSE(test, decoded.set_flags);
	KUNIT_EXPECT_EQ(test, 8U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 9U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 8U, decoded.rm);
	KUNIT_EXPECT_EQ(test, 2U, decoded.offset_extend);
	KUNIT_EXPECT_EQ(test, 0U, decoded.shift_amount);

	decoded = tcti_decode_aarch64(0xeb23203fU);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_ADD_SUB_EXTENDED_REGISTER,
			decoded.decode_class);
	KUNIT_EXPECT_TRUE(test, decoded.is_64bit);
	KUNIT_EXPECT_TRUE(test, decoded.subtract);
	KUNIT_EXPECT_TRUE(test, decoded.set_flags);
	KUNIT_EXPECT_EQ(test, 31U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 1U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 3U, decoded.rm);
	KUNIT_EXPECT_EQ(test, 0U, decoded.offset_extend);
	KUNIT_EXPECT_EQ(test, 0U, decoded.shift_amount);
}

static void tcti_decode_recognizes_pc_relative_address_class(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;

	decoded = tcti_decode_aarch64(0x10000041U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_PC_RELATIVE_ADDRESS,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 1U, decoded.rd);
	KUNIT_EXPECT_FALSE(test, decoded.page_relative);
	KUNIT_EXPECT_EQ(test, 8LL, decoded.pc_relative_imm);

	decoded = tcti_decode_aarch64(0x90000021U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_PC_RELATIVE_ADDRESS,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 1U, decoded.rd);
	KUNIT_EXPECT_TRUE(test, decoded.page_relative);
	KUNIT_EXPECT_EQ(test, 0x4000LL, decoded.pc_relative_imm);

	decoded = tcti_decode_aarch64(0x10ffffe2U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_PC_RELATIVE_ADDRESS,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 2U, decoded.rd);
	KUNIT_EXPECT_FALSE(test, decoded.page_relative);
	KUNIT_EXPECT_EQ(test, -4LL, decoded.pc_relative_imm);
}

static void tcti_decode_recognizes_unconditional_branch_immediate_class(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;

	decoded = tcti_decode_aarch64(0x94002283U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_UNCONDITIONAL_BRANCH_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_TRUE(test, decoded.link);
	KUNIT_EXPECT_EQ(test, 0x8a0cLL, decoded.branch_imm);

	decoded = tcti_decode_aarch64(0x14000002U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_UNCONDITIONAL_BRANCH_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_FALSE(test, decoded.link);
	KUNIT_EXPECT_EQ(test, 8LL, decoded.branch_imm);

	decoded = tcti_decode_aarch64(0x17fffff3U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_UNCONDITIONAL_BRANCH_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_FALSE(test, decoded.link);
	KUNIT_EXPECT_EQ(test, -52LL, decoded.branch_imm);
}

static void tcti_decode_recognizes_branch_register_class(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;

	decoded = tcti_decode_aarch64(0xd63f0260U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_UNCONDITIONAL_BRANCH_REGISTER,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_BRANCH_REGISTER_BLR,
			decoded.branch_register_op);
	KUNIT_EXPECT_TRUE(test, decoded.link);
	KUNIT_EXPECT_EQ(test, 19U, decoded.rn);

	decoded = tcti_decode_aarch64(0xd65f03c0U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_UNCONDITIONAL_BRANCH_REGISTER,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_BRANCH_REGISTER_RET,
			decoded.branch_register_op);
	KUNIT_EXPECT_FALSE(test, decoded.link);
	KUNIT_EXPECT_EQ(test, 30U, decoded.rn);
}

static void tcti_decode_recognizes_conditional_branch_classes(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;

	decoded = tcti_decode_aarch64(0xb40001d4U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_COMPARE_BRANCH_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_TRUE(test, decoded.is_64bit);
	KUNIT_EXPECT_FALSE(test, decoded.nonzero);
	KUNIT_EXPECT_EQ(test, 20U, decoded.rt);
	KUNIT_EXPECT_EQ(test, 56LL, decoded.branch_imm);

	decoded = tcti_decode_aarch64(0xb6f80080U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_TEST_BRANCH_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_TRUE(test, decoded.nonzero);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rt);
	KUNIT_EXPECT_EQ(test, 63U, decoded.test_bit);
	KUNIT_EXPECT_EQ(test, 16LL, decoded.branch_imm);

	decoded = tcti_decode_aarch64(0x54ffff00U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_CONDITIONAL_BRANCH_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 0U, decoded.condition);
	KUNIT_EXPECT_EQ(test, -32LL, decoded.branch_imm);
}

static void tcti_decode_recognizes_conditional_compare_class(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;

	decoded = tcti_decode_aarch64(0xfa560102U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_CONDITIONAL_COMPARE,
			decoded.decode_class);
	KUNIT_EXPECT_TRUE(test, decoded.is_64bit);
	KUNIT_EXPECT_TRUE(test, decoded.subtract);
	KUNIT_EXPECT_FALSE(test, decoded.immediate);
	KUNIT_EXPECT_EQ(test, 8U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 22U, decoded.rm);
	KUNIT_EXPECT_EQ(test, 0U, decoded.condition);
	KUNIT_EXPECT_EQ(test, 2U, decoded.nzcv);

	decoded = tcti_decode_aarch64(0x7a401900U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_CONDITIONAL_COMPARE,
			decoded.decode_class);
	KUNIT_EXPECT_FALSE(test, decoded.is_64bit);
	KUNIT_EXPECT_TRUE(test, decoded.subtract);
	KUNIT_EXPECT_TRUE(test, decoded.immediate);
	KUNIT_EXPECT_EQ(test, 8U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 0U, decoded.imm12);
	KUNIT_EXPECT_EQ(test, 1U, decoded.condition);
	KUNIT_EXPECT_EQ(test, 0U, decoded.nzcv);

	decoded = tcti_decode_aarch64(0xba422028U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_CONDITIONAL_COMPARE,
			decoded.decode_class);
	KUNIT_EXPECT_TRUE(test, decoded.is_64bit);
	KUNIT_EXPECT_FALSE(test, decoded.subtract);
	KUNIT_EXPECT_FALSE(test, decoded.immediate);
	KUNIT_EXPECT_EQ(test, 1U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 2U, decoded.rm);
	KUNIT_EXPECT_EQ(test, 2U, decoded.condition);
	KUNIT_EXPECT_EQ(test, 8U, decoded.nzcv);
}

static void tcti_decode_recognizes_conditional_select_class(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;

	decoded = tcti_decode_aarch64(0x5a9f316aU);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_CONDITIONAL_SELECT,
			decoded.decode_class);
	KUNIT_EXPECT_FALSE(test, decoded.is_64bit);
	KUNIT_EXPECT_EQ(test, TCTI_CONDITIONAL_SELECT_CSINV,
			decoded.conditional_select_op);
	KUNIT_EXPECT_EQ(test, 10U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 11U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 31U, decoded.rm);
	KUNIT_EXPECT_EQ(test, 3U, decoded.condition);

	decoded = tcti_decode_aarch64(0x1a9fc517U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_CONDITIONAL_SELECT,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_CONDITIONAL_SELECT_CSINC,
			decoded.conditional_select_op);
	KUNIT_EXPECT_EQ(test, 23U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 8U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 31U, decoded.rm);
	KUNIT_EXPECT_EQ(test, 12U, decoded.condition);
}

static void tcti_decode_recognizes_load_store_pair_class(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;

	decoded = tcti_decode_aarch64(0xa9be7bfdU);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_LOAD_STORE_PAIR,
			decoded.decode_class);
	KUNIT_EXPECT_FALSE(test, decoded.load);
	KUNIT_EXPECT_EQ(test, 29U, decoded.rt);
	KUNIT_EXPECT_EQ(test, 30U, decoded.rt2);
	KUNIT_EXPECT_EQ(test, 31U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 8U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, -32LL, decoded.memory_offset);
	KUNIT_EXPECT_EQ(test, TCTI_MEMORY_INDEX_PRE,
			decoded.memory_index_mode);

	decoded = tcti_decode_aarch64(0xa9016ffcU);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_LOAD_STORE_PAIR,
			decoded.decode_class);
	KUNIT_EXPECT_FALSE(test, decoded.load);
	KUNIT_EXPECT_EQ(test, 28U, decoded.rt);
	KUNIT_EXPECT_EQ(test, 27U, decoded.rt2);
	KUNIT_EXPECT_EQ(test, 31U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 8U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 16LL, decoded.memory_offset);
	KUNIT_EXPECT_EQ(test, TCTI_MEMORY_INDEX_SIGNED_OFFSET,
			decoded.memory_index_mode);
}

static void tcti_decode_recognizes_load_store_unsigned_class(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;

	decoded = tcti_decode_aarch64(0xf9000bf3U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_LOAD_STORE_UNSIGNED_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_FALSE(test, decoded.load);
	KUNIT_EXPECT_EQ(test, 19U, decoded.rt);
	KUNIT_EXPECT_EQ(test, 31U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 8U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 16LL, decoded.memory_offset);

	decoded = tcti_decode_aarch64(0xf9456929U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_LOAD_STORE_UNSIGNED_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_TRUE(test, decoded.load);
	KUNIT_EXPECT_EQ(test, 9U, decoded.rt);
	KUNIT_EXPECT_EQ(test, 9U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 8U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 0xad0LL, decoded.memory_offset);

	decoded = tcti_decode_aarch64(0x39470408U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_LOAD_STORE_UNSIGNED_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_TRUE(test, decoded.load);
	KUNIT_EXPECT_EQ(test, 8U, decoded.rt);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 1U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 0x1c1LL, decoded.memory_offset);
}

static void tcti_decode_recognizes_load_store_signed_immediate_class(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;

	decoded = tcti_decode_aarch64(0x3800150aU);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_LOAD_STORE_SIGNED_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_FALSE(test, decoded.load);
	KUNIT_EXPECT_EQ(test, 10U, decoded.rt);
	KUNIT_EXPECT_EQ(test, 8U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 1U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 1LL, decoded.memory_offset);
	KUNIT_EXPECT_EQ(test, TCTI_MEMORY_INDEX_POST,
			decoded.memory_index_mode);

	decoded = tcti_decode_aarch64(0x385fe109U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_LOAD_STORE_SIGNED_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_TRUE(test, decoded.load);
	KUNIT_EXPECT_EQ(test, 9U, decoded.rt);
	KUNIT_EXPECT_EQ(test, 8U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 1U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, -2LL, decoded.memory_offset);
	KUNIT_EXPECT_EQ(test, TCTI_MEMORY_INDEX_SIGNED_OFFSET,
			decoded.memory_index_mode);

	decoded = tcti_decode_aarch64(0xf81d03bfU);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_LOAD_STORE_SIGNED_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_FALSE(test, decoded.load);
	KUNIT_EXPECT_EQ(test, 31U, decoded.rt);
	KUNIT_EXPECT_EQ(test, 29U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 8U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, -48LL, decoded.memory_offset);

	decoded = tcti_decode_aarch64(0xf85d03a9U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_LOAD_STORE_SIGNED_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_TRUE(test, decoded.load);
	KUNIT_EXPECT_EQ(test, 9U, decoded.rt);
	KUNIT_EXPECT_EQ(test, 29U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 8U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, -48LL, decoded.memory_offset);
}

static void tcti_decode_recognizes_load_store_register_offset_class(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;

	decoded = tcti_decode_aarch64(0xb8796b48U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_LOAD_STORE_REGISTER_OFFSET,
			decoded.decode_class);
	KUNIT_EXPECT_TRUE(test, decoded.load);
	KUNIT_EXPECT_FALSE(test, decoded.sign_extend_load);
	KUNIT_EXPECT_EQ(test, 8U, decoded.rt);
	KUNIT_EXPECT_EQ(test, 26U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 25U, decoded.rm);
	KUNIT_EXPECT_EQ(test, 4U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 4U, decoded.result_size);
	KUNIT_EXPECT_EQ(test, 3U, decoded.offset_extend);
	KUNIT_EXPECT_FALSE(test, decoded.offset_shift);

	decoded = tcti_decode_aarch64(0xb8396b48U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_LOAD_STORE_REGISTER_OFFSET,
			decoded.decode_class);
	KUNIT_EXPECT_FALSE(test, decoded.load);
	KUNIT_EXPECT_EQ(test, 8U, decoded.rt);
	KUNIT_EXPECT_EQ(test, 26U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 25U, decoded.rm);
	KUNIT_EXPECT_EQ(test, 4U, decoded.access_size);

	decoded = tcti_decode_aarch64(0xb8b96b48U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_LOAD_STORE_REGISTER_OFFSET,
			decoded.decode_class);
	KUNIT_EXPECT_TRUE(test, decoded.load);
	KUNIT_EXPECT_TRUE(test, decoded.sign_extend_load);
	KUNIT_EXPECT_EQ(test, 8U, decoded.rt);
	KUNIT_EXPECT_EQ(test, 8U, decoded.result_size);
	KUNIT_EXPECT_EQ(test, 4U, decoded.access_size);
}

static void tcti_decode_recognizes_logical_shifted_register_class(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;

	decoded = tcti_decode_aarch64(0xaa0103f3U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_LOGICAL_SHIFTED_REGISTER,
			decoded.decode_class);
	KUNIT_EXPECT_TRUE(test, decoded.is_64bit);
	KUNIT_EXPECT_EQ(test, TCTI_LOGICAL_ORR, decoded.logical_op);
	KUNIT_EXPECT_EQ(test, 19U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 31U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 1U, decoded.rm);
	KUNIT_EXPECT_EQ(test, 0U, decoded.shift);
	KUNIT_EXPECT_EQ(test, 0U, decoded.shift_amount);

	decoded = tcti_decode_aarch64(0x2a0803e0U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_LOGICAL_SHIFTED_REGISTER,
			decoded.decode_class);
	KUNIT_EXPECT_FALSE(test, decoded.is_64bit);
	KUNIT_EXPECT_EQ(test, TCTI_LOGICAL_ORR, decoded.logical_op);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 31U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 8U, decoded.rm);

	decoded = tcti_decode_aarch64(0xea0b015fU);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_LOGICAL_SHIFTED_REGISTER,
			decoded.decode_class);
	KUNIT_EXPECT_TRUE(test, decoded.is_64bit);
	KUNIT_EXPECT_EQ(test, TCTI_LOGICAL_AND, decoded.logical_op);
	KUNIT_EXPECT_TRUE(test, decoded.set_flags);
	KUNIT_EXPECT_EQ(test, 31U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 10U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 11U, decoded.rm);

	decoded = tcti_decode_aarch64(0x2a2b03e9U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_LOGICAL_SHIFTED_REGISTER,
			decoded.decode_class);
	KUNIT_EXPECT_FALSE(test, decoded.is_64bit);
	KUNIT_EXPECT_EQ(test, TCTI_LOGICAL_ORR, decoded.logical_op);
	KUNIT_EXPECT_TRUE(test, decoded.invert_second_operand);
	KUNIT_EXPECT_EQ(test, 9U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 31U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 11U, decoded.rm);
}

static void tcti_decode_recognizes_logical_immediate_class(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;

	decoded = tcti_decode_aarch64(0x12007808U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_LOGICAL_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_FALSE(test, decoded.is_64bit);
	KUNIT_EXPECT_EQ(test, TCTI_LOGICAL_AND, decoded.logical_op);
	KUNIT_EXPECT_EQ(test, 8U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 0x7fffffffULL, decoded.logical_immediate);

	decoded = tcti_decode_aarch64(0x32190108U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_LOGICAL_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_FALSE(test, decoded.is_64bit);
	KUNIT_EXPECT_EQ(test, TCTI_LOGICAL_ORR, decoded.logical_op);
	KUNIT_EXPECT_EQ(test, 8U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 8U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 0x80ULL, decoded.logical_immediate);

	decoded = tcti_decode_aarch64(0x7200191fU);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_LOGICAL_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_FALSE(test, decoded.is_64bit);
	KUNIT_EXPECT_EQ(test, TCTI_LOGICAL_AND, decoded.logical_op);
	KUNIT_EXPECT_TRUE(test, decoded.set_flags);
	KUNIT_EXPECT_EQ(test, 31U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 8U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 0x7fULL, decoded.logical_immediate);
}

static void tcti_decode_recognizes_bitfield_class(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;

	decoded = tcti_decode_aarch64(0x53083ed7U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_BITFIELD, decoded.decode_class);
	KUNIT_EXPECT_FALSE(test, decoded.is_64bit);
	KUNIT_EXPECT_EQ(test, TCTI_BITFIELD_UBFM, decoded.bitfield_op);
	KUNIT_EXPECT_EQ(test, 23U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 22U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 8U, decoded.bitfield_immr);
	KUNIT_EXPECT_EQ(test, 15U, decoded.bitfield_imms);

	decoded = tcti_decode_aarch64(0xd350ff09U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_BITFIELD, decoded.decode_class);
	KUNIT_EXPECT_TRUE(test, decoded.is_64bit);
	KUNIT_EXPECT_EQ(test, TCTI_BITFIELD_UBFM, decoded.bitfield_op);
	KUNIT_EXPECT_EQ(test, 9U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 24U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 16U, decoded.bitfield_immr);
	KUNIT_EXPECT_EQ(test, 63U, decoded.bitfield_imms);

	decoded = tcti_decode_aarch64(0x13000100U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_BITFIELD, decoded.decode_class);
	KUNIT_EXPECT_FALSE(test, decoded.is_64bit);
	KUNIT_EXPECT_EQ(test, TCTI_BITFIELD_SBFM, decoded.bitfield_op);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 8U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 0U, decoded.bitfield_immr);
	KUNIT_EXPECT_EQ(test, 0U, decoded.bitfield_imms);

	decoded = tcti_decode_aarch64(0x33181c41U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_BITFIELD, decoded.decode_class);
	KUNIT_EXPECT_FALSE(test, decoded.is_64bit);
	KUNIT_EXPECT_EQ(test, TCTI_BITFIELD_BFM, decoded.bitfield_op);
	KUNIT_EXPECT_EQ(test, 1U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 2U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 24U, decoded.bitfield_immr);
	KUNIT_EXPECT_EQ(test, 7U, decoded.bitfield_imms);
}

static void tcti_decode_recognizes_extract_class(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;

	decoded = tcti_decode_aarch64(0x93c80508U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_EXTRACT, decoded.decode_class);
	KUNIT_EXPECT_TRUE(test, decoded.is_64bit);
	KUNIT_EXPECT_EQ(test, 8U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 8U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 8U, decoded.rm);
	KUNIT_EXPECT_EQ(test, 1U, decoded.shift_amount);

	decoded = tcti_decode_aarch64(0x138b7dacU);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_EXTRACT, decoded.decode_class);
	KUNIT_EXPECT_FALSE(test, decoded.is_64bit);
	KUNIT_EXPECT_EQ(test, 12U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 13U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 11U, decoded.rm);
	KUNIT_EXPECT_EQ(test, 31U, decoded.shift_amount);
}

static void tcti_decode_recognizes_data_processing_1source_class(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;

	decoded = tcti_decode_aarch64(0xdac01109U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_DATA_PROCESSING_1SOURCE,
			decoded.decode_class);
	KUNIT_EXPECT_TRUE(test, decoded.is_64bit);
	KUNIT_EXPECT_EQ(test, TCTI_DP1_CLZ, decoded.dp1_op);
	KUNIT_EXPECT_EQ(test, 9U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 8U, decoded.rn);

	decoded = tcti_decode_aarch64(0x5ac0114aU);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_DATA_PROCESSING_1SOURCE,
			decoded.decode_class);
	KUNIT_EXPECT_FALSE(test, decoded.is_64bit);
	KUNIT_EXPECT_EQ(test, TCTI_DP1_CLZ, decoded.dp1_op);
	KUNIT_EXPECT_EQ(test, 10U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 10U, decoded.rn);
}

static void tcti_decode_recognizes_data_processing_2source_class(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;

	decoded = tcti_decode_aarch64(0x9ac92149U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_DATA_PROCESSING_2SOURCE,
			decoded.decode_class);
	KUNIT_EXPECT_TRUE(test, decoded.is_64bit);
	KUNIT_EXPECT_EQ(test, TCTI_DP2_LSLV, decoded.dp2_op);
	KUNIT_EXPECT_EQ(test, 9U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 10U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 9U, decoded.rm);

	decoded = tcti_decode_aarch64(0x1ad42508U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_DATA_PROCESSING_2SOURCE,
			decoded.decode_class);
	KUNIT_EXPECT_FALSE(test, decoded.is_64bit);
	KUNIT_EXPECT_EQ(test, TCTI_DP2_LSRV, decoded.dp2_op);
	KUNIT_EXPECT_EQ(test, 8U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 8U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 20U, decoded.rm);

	decoded = tcti_decode_aarch64(0x9acb082dU);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_DATA_PROCESSING_2SOURCE,
			decoded.decode_class);
	KUNIT_EXPECT_TRUE(test, decoded.is_64bit);
	KUNIT_EXPECT_EQ(test, TCTI_DP2_UDIV, decoded.dp2_op);
	KUNIT_EXPECT_EQ(test, 13U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 1U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 11U, decoded.rm);

	decoded = tcti_decode_aarch64(0x1ac90d07U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_DATA_PROCESSING_2SOURCE,
			decoded.decode_class);
	KUNIT_EXPECT_FALSE(test, decoded.is_64bit);
	KUNIT_EXPECT_EQ(test, TCTI_DP2_SDIV, decoded.dp2_op);
	KUNIT_EXPECT_EQ(test, 7U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 8U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 9U, decoded.rm);
}

static void tcti_decode_recognizes_multiply_add_sub_class(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;

	decoded = tcti_decode_aarch64(0x9b174d21U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_MULTIPLY_ADD_SUB,
			decoded.decode_class);
	KUNIT_EXPECT_TRUE(test, decoded.is_64bit);
	KUNIT_EXPECT_EQ(test, TCTI_MUL_MADD, decoded.mul_op);
	KUNIT_EXPECT_EQ(test, 1U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 9U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 23U, decoded.rm);
	KUNIT_EXPECT_EQ(test, 19U, decoded.ra);

	decoded = tcti_decode_aarch64(0x9b0b85aeU);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_MULTIPLY_ADD_SUB,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_MUL_MSUB, decoded.mul_op);
	KUNIT_EXPECT_EQ(test, 14U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 13U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 11U, decoded.rm);
	KUNIT_EXPECT_EQ(test, 1U, decoded.ra);

	decoded = tcti_decode_aarch64(0x9b3b4f5aU);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_MULTIPLY_ADD_SUB,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_MUL_SMADDL, decoded.mul_op);
	KUNIT_EXPECT_EQ(test, 26U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 26U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 27U, decoded.rm);
	KUNIT_EXPECT_EQ(test, 19U, decoded.ra);

	decoded = tcti_decode_aarch64(0x9ba82688U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_MULTIPLY_ADD_SUB,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_MUL_UMADDL, decoded.mul_op);
	KUNIT_EXPECT_EQ(test, 8U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 20U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 8U, decoded.rm);
	KUNIT_EXPECT_EQ(test, 9U, decoded.ra);

	decoded = tcti_decode_aarch64(0x9b457c83U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_MULTIPLY_ADD_SUB,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_MUL_SMULH, decoded.mul_op);
	KUNIT_EXPECT_EQ(test, 3U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 4U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 5U, decoded.rm);
	KUNIT_EXPECT_EQ(test, 31U, decoded.ra);

	decoded = tcti_decode_aarch64(0x9bc07e31U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_MULTIPLY_ADD_SUB,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_MUL_UMULH, decoded.mul_op);
	KUNIT_EXPECT_EQ(test, 17U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 17U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rm);
	KUNIT_EXPECT_EQ(test, 31U, decoded.ra);
}

static void tcti_decode_recognizes_move_wide_immediate_class(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;

	decoded = tcti_decode_aarch64(0x52800334U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_MOVE_WIDE_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_FALSE(test, decoded.is_64bit);
	KUNIT_EXPECT_EQ(test, TCTI_MOVE_WIDE_MOVZ, decoded.move_wide_op);
	KUNIT_EXPECT_EQ(test, 20U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 25U, decoded.imm16);
	KUNIT_EXPECT_EQ(test, 0U, decoded.halfword_shift);

	decoded = tcti_decode_aarch64(0xd2a00019U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_MOVE_WIDE_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_TRUE(test, decoded.is_64bit);
	KUNIT_EXPECT_EQ(test, TCTI_MOVE_WIDE_MOVZ, decoded.move_wide_op);
	KUNIT_EXPECT_EQ(test, 25U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 0U, decoded.imm16);
	KUNIT_EXPECT_EQ(test, 16U, decoded.halfword_shift);

	decoded = tcti_decode_aarch64(0xf2800319U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_MOVE_WIDE_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_TRUE(test, decoded.is_64bit);
	KUNIT_EXPECT_EQ(test, TCTI_MOVE_WIDE_MOVK, decoded.move_wide_op);
	KUNIT_EXPECT_EQ(test, 25U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 24U, decoded.imm16);
	KUNIT_EXPECT_EQ(test, 0U, decoded.halfword_shift);
}

static void tcti_decode_recognizes_system_register_class(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;

	decoded = tcti_decode_aarch64(0xd53bd05aU);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SYSTEM_REGISTER,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_SYSTEM_REGISTER_TPIDR_EL0,
			decoded.system_register);
	KUNIT_EXPECT_FALSE(test, decoded.system_register_write);
	KUNIT_EXPECT_EQ(test, 26U, decoded.rt);

	decoded = tcti_decode_aarch64(0xd51bd040U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SYSTEM_REGISTER,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_SYSTEM_REGISTER_TPIDR_EL0,
			decoded.system_register);
	KUNIT_EXPECT_TRUE(test, decoded.system_register_write);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rt);

	decoded = tcti_decode_aarch64(0xd53b4203U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SYSTEM_REGISTER,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_SYSTEM_REGISTER_NZCV,
			decoded.system_register);
	KUNIT_EXPECT_FALSE(test, decoded.system_register_write);
	KUNIT_EXPECT_EQ(test, 3U, decoded.rt);
}

static void tcti_decode_recognizes_exclusive_monitor_clear(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;

	decoded = tcti_decode_aarch64(0xd5033f5fU);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_EXCLUSIVE_MONITOR_CLEAR,
			decoded.decode_class);
}

static void tcti_decode_recognizes_load_store_exclusive_class(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;

	decoded = tcti_decode_aarch64(0x885f7e69U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_LOAD_STORE_EXCLUSIVE,
			decoded.decode_class);
	KUNIT_EXPECT_TRUE(test, decoded.load);
	KUNIT_EXPECT_TRUE(test, decoded.exclusive);
	KUNIT_EXPECT_FALSE(test, decoded.acquire);
	KUNIT_EXPECT_EQ(test, 4U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 9U, decoded.rt);
	KUNIT_EXPECT_EQ(test, 19U, decoded.rn);

	decoded = tcti_decode_aarch64(0x885ffe61U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_LOAD_STORE_EXCLUSIVE,
			decoded.decode_class);
	KUNIT_EXPECT_TRUE(test, decoded.load);
	KUNIT_EXPECT_TRUE(test, decoded.exclusive);
	KUNIT_EXPECT_TRUE(test, decoded.acquire);
	KUNIT_EXPECT_EQ(test, 1U, decoded.rt);
	KUNIT_EXPECT_EQ(test, 19U, decoded.rn);

	decoded = tcti_decode_aarch64(0x880a7e68U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_LOAD_STORE_EXCLUSIVE,
			decoded.decode_class);
	KUNIT_EXPECT_FALSE(test, decoded.load);
	KUNIT_EXPECT_TRUE(test, decoded.exclusive);
	KUNIT_EXPECT_FALSE(test, decoded.release);
	KUNIT_EXPECT_EQ(test, 10U, decoded.rs);
	KUNIT_EXPECT_EQ(test, 8U, decoded.rt);
	KUNIT_EXPECT_EQ(test, 19U, decoded.rn);

	decoded = tcti_decode_aarch64(0x8808fe9fU);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_LOAD_STORE_EXCLUSIVE,
			decoded.decode_class);
	KUNIT_EXPECT_FALSE(test, decoded.load);
	KUNIT_EXPECT_TRUE(test, decoded.exclusive);
	KUNIT_EXPECT_TRUE(test, decoded.release);
	KUNIT_EXPECT_EQ(test, 8U, decoded.rs);
	KUNIT_EXPECT_EQ(test, 31U, decoded.rt);
	KUNIT_EXPECT_EQ(test, 20U, decoded.rn);

	decoded = tcti_decode_aarch64(0x08dffd08U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_LOAD_STORE_EXCLUSIVE,
			decoded.decode_class);
	KUNIT_EXPECT_TRUE(test, decoded.load);
	KUNIT_EXPECT_FALSE(test, decoded.exclusive);
	KUNIT_EXPECT_TRUE(test, decoded.acquire);
	KUNIT_EXPECT_EQ(test, 1U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 8U, decoded.rt);
	KUNIT_EXPECT_EQ(test, 8U, decoded.rn);
}

static void tcti_switch_executes_hint_as_noop(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	int ret;

	regs.regs[0] = 0x7039cebe80ULL;
	regs.sp = 0x7039cebe80ULL;
	regs.pc = 0x702a3b54a8ULL;

	decoded = tcti_decode_aarch64(0xd503201fU);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x7039cebe80ULL, regs.regs[0]);
	KUNIT_EXPECT_EQ(test, 0x7039cebe80ULL, regs.sp);
	KUNIT_EXPECT_EQ(test, 0x702a3b54acULL, regs.pc);
}

static void tcti_gadget_program_lowers_hint_as_data_stream(struct kunit *test)
{
	struct tcti_gadget_word program[TCTI_SINGLE_INSTRUCTION_PROGRAM_WORDS];
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = { 0 };
	unsigned long fault_address = 0;
	size_t word_count = 0;
	int ret;

	regs.pc = 0x4000;
	decoded = tcti_decode_aarch64(0xd503201fU);

	ret = tcti_lower_decoded_instruction(&decoded, program,
					     ARRAY_SIZE(program),
					     &word_count);
	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, TCTI_SINGLE_INSTRUCTION_PROGRAM_WORDS,
			word_count);

	ret = tcti_execute_gadget_program(NULL, &regs, program, word_count,
					  &fault_address);
	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x4004ULL, regs.pc);
}

static void tcti_gadget_program_executes_extract(struct kunit *test)
{
	struct tcti_gadget_word program[TCTI_SINGLE_INSTRUCTION_PROGRAM_WORDS];
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = { 0 };
	unsigned long fault_address = 0;
	size_t word_count = 0;
	int ret;

	regs.regs[8] = 0x8000000000000001ULL;
	regs.pc = 0x5000;

	decoded = tcti_decode_aarch64(0x93c80508U);
	ret = tcti_lower_decoded_instruction(&decoded, program,
					     ARRAY_SIZE(program), &word_count);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, TCTI_SINGLE_INSTRUCTION_PROGRAM_WORDS,
			word_count);

	ret = tcti_execute_gadget_program(NULL, &regs, program, word_count,
					  &fault_address);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0xc000000000000000ULL, regs.regs[8]);
	KUNIT_EXPECT_EQ(test, 0x5004ULL, regs.pc);
}

static void tcti_gadget_program_rejects_svc_lowering(struct kunit *test)
{
	struct tcti_gadget_word program[TCTI_SINGLE_INSTRUCTION_PROGRAM_WORDS];
	struct tcti_decoded_instruction decoded;
	size_t word_count = 0;
	int ret;

	decoded = tcti_decode_aarch64(0xd4000001U);
	ret = tcti_lower_decoded_instruction(&decoded, program,
					     ARRAY_SIZE(program),
					     &word_count);

	KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, ret);
	KUNIT_EXPECT_EQ(test, 0UL, word_count);
}

static void tcti_gadget_program_executes_init001_movz_prefix(struct kunit *test)
{
	struct tcti_gadget_word program[TCTI_SINGLE_INSTRUCTION_PROGRAM_WORDS];
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = { 0 };
	unsigned long fault_address = 0;
	size_t word_count = 0;
	int ret;

	regs.pc = 0x210120;

	decoded = tcti_decode_aarch64(0xd2800ba8U);
	ret = tcti_lower_decoded_instruction(&decoded, program,
					     ARRAY_SIZE(program),
					     &word_count);
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_ASSERT_EQ(test, TCTI_SINGLE_INSTRUCTION_PROGRAM_WORDS,
			word_count);
	ret = tcti_execute_gadget_program(NULL, &regs, program, word_count,
					  &fault_address);
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 93ULL, regs.regs[8]);
	KUNIT_EXPECT_EQ(test, 0x210124ULL, regs.pc);

	decoded = tcti_decode_aarch64(0xd2800540U);
	ret = tcti_lower_decoded_instruction(&decoded, program,
					     ARRAY_SIZE(program),
					     &word_count);
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_ASSERT_EQ(test, TCTI_SINGLE_INSTRUCTION_PROGRAM_WORDS,
			word_count);
	ret = tcti_execute_gadget_program(NULL, &regs, program, word_count,
					  &fault_address);
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 42ULL, regs.regs[0]);
	KUNIT_EXPECT_EQ(test, 0x210128ULL, regs.pc);

	decoded = tcti_decode_aarch64(0xd4000001U);
	ret = tcti_lower_decoded_instruction(&decoded, program,
					     ARRAY_SIZE(program),
					     &word_count);
	KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, ret);
}

static void tcti_gadget_program_matches_switch_debug_init001_movz_prefix(struct kunit *test)
{
	u32 instructions[] = {
		0xd2800ba8U,
		0xd2800540U,
	};
	struct tcti_gadget_word program[TCTI_SINGLE_INSTRUCTION_PROGRAM_WORDS];
	struct pt_regs reference = { 0 };
	struct pt_regs candidate = { 0 };
	unsigned long fault_address = 0;
	size_t word_count = 0;
	int i;

	reference.pc = 0x210120;
	candidate.pc = 0x210120;

	for (i = 0; i < ARRAY_SIZE(instructions); i++) {
		struct tcti_decoded_instruction decoded;
		u64 reference_nzcv;
		u64 candidate_nzcv;
		int ret;

		decoded = tcti_decode_aarch64(instructions[i]);

		ret = tcti_switch_debug_execute_decoded(NULL, &reference,
							&decoded, NULL);
		KUNIT_ASSERT_EQ(test, 0, ret);

		ret = tcti_lower_decoded_instruction(&decoded, program,
						     ARRAY_SIZE(program),
						     &word_count);
		KUNIT_ASSERT_EQ(test, 0, ret);
		KUNIT_ASSERT_EQ(test, TCTI_SINGLE_INSTRUCTION_PROGRAM_WORDS,
				word_count);

		ret = tcti_execute_gadget_program(NULL, &candidate, program,
						  word_count, &fault_address);
		KUNIT_ASSERT_EQ(test, 0, ret);

		reference_nzcv = reference.pstate & (PSR_N_BIT | PSR_Z_BIT |
						     PSR_C_BIT | PSR_V_BIT);
		candidate_nzcv = candidate.pstate & (PSR_N_BIT | PSR_Z_BIT |
						     PSR_C_BIT | PSR_V_BIT);

		KUNIT_EXPECT_EQ(test, reference.regs[0], candidate.regs[0]);
		KUNIT_EXPECT_EQ(test, reference.regs[8], candidate.regs[8]);
		KUNIT_EXPECT_EQ(test, reference.sp, candidate.sp);
		KUNIT_EXPECT_EQ(test, reference.pc, candidate.pc);
		KUNIT_EXPECT_EQ(test, reference_nzcv, candidate_nzcv);
	}

	KUNIT_EXPECT_EQ(test, 42ULL, candidate.regs[0]);
	KUNIT_EXPECT_EQ(test, 93ULL, candidate.regs[8]);
	KUNIT_EXPECT_EQ(test, 0x210128ULL, candidate.pc);
}

static void tcti_block_cache_returns_cached_program(struct kunit *test)
{
	struct tcti_gadget_word program[TCTI_SINGLE_INSTRUCTION_PROGRAM_WORDS];
	struct tcti_decoded_instruction decoded;
	struct mm_struct *mm = (struct mm_struct *)0x1000UL;
	struct tcti_block *block;
	struct pt_regs regs = {};
	unsigned long fault_address = 0;
	size_t word_count = 0;
	u32 generation;
	int ret;

	tcti_block_cache_reset_for_tests();
	decoded = tcti_decode_aarch64(0xd503201fU);
	ret = tcti_lower_decoded_instruction(&decoded, program,
					     ARRAY_SIZE(program),
					     &word_count);
	KUNIT_ASSERT_EQ(test, 0, ret);

	generation = tcti_code_generation(mm);
	ret = tcti_block_cache_insert(mm, 0x4000, 0x4004, generation, 1,
				      program, word_count, NULL);
	KUNIT_ASSERT_EQ(test, 0, ret);

	block = tcti_block_cache_lookup(mm, 0x4000, generation);
	KUNIT_ASSERT_NOT_NULL(test, block);
	KUNIT_EXPECT_EQ(test, 1U, block->instruction_count);
	KUNIT_EXPECT_EQ(test, (u32)word_count, block->program_words);

	regs.pc = 0x4000;
	ret = tcti_execute_gadget_program(NULL, &regs, block->program,
					  block->program_words, &fault_address);
	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x4004ULL, regs.pc);
	tcti_block_put(block);

	tcti_block_cache_reset_for_tests();
}

static void tcti_block_cache_is_bounded(struct kunit *test)
{
	struct tcti_gadget_word program[TCTI_SINGLE_INSTRUCTION_PROGRAM_WORDS];
	struct tcti_decoded_instruction decoded;
	struct mm_struct *mm = (struct mm_struct *)0x2000UL;
	size_t word_count = 0;
	u32 generation;
	u32 index;
	int ret = 0;

	tcti_block_cache_reset_for_tests();
	decoded = tcti_decode_aarch64(0xd503201fU);
	ret = tcti_lower_decoded_instruction(&decoded, program,
					     ARRAY_SIZE(program),
					     &word_count);
	KUNIT_ASSERT_EQ(test, 0, ret);

	generation = tcti_code_generation(mm);
	for (index = 0; index < TCTI_BLOCK_CACHE_MAX_BLOCKS; index++) {
		ret = tcti_block_cache_insert(mm, 0x100000 + index * 4,
					      0x100004 + index * 4,
					      generation, 1, program,
					      word_count, NULL);
		KUNIT_ASSERT_EQ(test, 0, ret);
	}

	KUNIT_EXPECT_EQ(test, TCTI_BLOCK_CACHE_MAX_BLOCKS,
			tcti_block_cache_count_for_tests());
	ret = tcti_block_cache_insert(mm, 0x200000, 0x200004, generation, 1,
				      program, word_count, NULL);
	KUNIT_EXPECT_EQ(test, -ENOSPC, ret);

	tcti_block_cache_reset_for_tests();
}

static void tcti_block_cache_invalidation_bumps_generation(struct kunit *test)
{
	struct tcti_gadget_word program[TCTI_SINGLE_INSTRUCTION_PROGRAM_WORDS];
	struct tcti_decoded_instruction decoded;
	struct mm_struct *mm = (struct mm_struct *)0x3000UL;
	struct tcti_block *block;
	size_t word_count = 0;
	u32 old_generation;
	u32 new_generation;
	int ret;

	tcti_block_cache_reset_for_tests();
	decoded = tcti_decode_aarch64(0xd503201fU);
	ret = tcti_lower_decoded_instruction(&decoded, program,
					     ARRAY_SIZE(program),
					     &word_count);
	KUNIT_ASSERT_EQ(test, 0, ret);

	old_generation = tcti_code_generation(mm);
	ret = tcti_block_cache_insert(mm, 0x5000, 0x5004, old_generation, 1,
				      program, word_count, NULL);
	KUNIT_ASSERT_EQ(test, 0, ret);

	tcti_block_cache_invalidate_mm(mm);
	new_generation = tcti_code_generation(mm);

	KUNIT_EXPECT_NE(test, old_generation, new_generation);
	KUNIT_EXPECT_EQ(test, 0U, tcti_block_cache_count_for_tests());
	block = tcti_block_cache_lookup(mm, 0x5000, old_generation);
	KUNIT_EXPECT_NULL(test, block);

	tcti_block_cache_reset_for_tests();
}

static void tcti_tlb_separates_access_classes(struct kunit *test)
{
	static struct tcti_tlb tlb;
	struct mm_struct *mm = (struct mm_struct *)0x4000UL;
	struct tcti_user_page page = {
		.user_page = 0x7000,
		.host_data = (void *)0x100000,
		.translation_generation = 9,
	};
	void *host;
	int ret;

	tcti_tlb_init(&tlb);
	ret = tcti_tlb_fill(&tlb, mm, 0x7123, TCTI_ACCESS_FETCH, &page);
	KUNIT_ASSERT_EQ(test, 0, ret);

	host = tcti_tlb_lookup(&tlb, mm, 0x7123, TCTI_ACCESS_FETCH, 9);
	KUNIT_EXPECT_EQ(test, 0x100123UL, (unsigned long)host);
	KUNIT_EXPECT_EQ(test, 1ULL, tlb.fetch_hits);

	host = tcti_tlb_lookup(&tlb, mm, 0x7123, TCTI_ACCESS_READ, 9);
	KUNIT_EXPECT_NULL(test, host);
	KUNIT_EXPECT_EQ(test, 1ULL, tlb.read_misses);
}

static void tcti_tlb_flushes_on_generation_change(struct kunit *test)
{
	static struct tcti_tlb tlb;
	struct mm_struct *mm = (struct mm_struct *)0x5000UL;
	struct tcti_user_page page = {
		.user_page = 0x9000,
		.host_data = (void *)0x200000,
		.translation_generation = 3,
	};
	void *host;
	int ret;

	tcti_tlb_init(&tlb);
	ret = tcti_tlb_fill(&tlb, mm, 0x9008, TCTI_ACCESS_READ, &page);
	KUNIT_ASSERT_EQ(test, 0, ret);

	host = tcti_tlb_lookup(&tlb, mm, 0x9008, TCTI_ACCESS_READ, 3);
	KUNIT_EXPECT_EQ(test, 0x200008UL, (unsigned long)host);

	host = tcti_tlb_lookup(&tlb, mm, 0x9008, TCTI_ACCESS_READ, 4);
	KUNIT_EXPECT_NULL(test, host);
	KUNIT_EXPECT_EQ(test, 2ULL, tlb.generation_flushes);
	KUNIT_EXPECT_EQ(test, 1ULL, tlb.read_misses);
}

static void tcti_switch_executes_add_immediate_with_sp_source(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	int ret;

	regs.sp = 0x7136d77e80ULL;
	regs.pc = 0x71274454a4ULL;

	decoded = tcti_decode_aarch64(0x910003e0U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x7136d77e80ULL, regs.regs[0]);
	KUNIT_EXPECT_EQ(test, 0x7136d77e80ULL, regs.sp);
	KUNIT_EXPECT_EQ(test, 0x71274454a8ULL, regs.pc);
}

static void tcti_switch_executes_add_sub_immediate_variants(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	int ret;

	regs.regs[1] = 0x1000;
	regs.regs[4] = 0x20;
	regs.regs[6] = 0xffffffffffffffffULL;
	regs.pc = 0x4000;

	decoded = tcti_decode_aarch64(0x91400421U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x2000ULL, regs.regs[1]);
	KUNIT_EXPECT_EQ(test, 0x4004ULL, regs.pc);

	decoded = tcti_decode_aarch64(0xd1001483U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x1bULL, regs.regs[3]);
	KUNIT_EXPECT_EQ(test, 0x4008ULL, regs.pc);

	decoded = tcti_decode_aarch64(0x110004c5U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0U, regs.regs[5]);
	KUNIT_EXPECT_EQ(test, 0x400cULL, regs.pc);

	regs.regs[8] = 4;
	decoded = tcti_decode_aarch64(0x7100111fU);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_NE(test, 0ULL, regs.pstate & PSR_Z_BIT);
	KUNIT_EXPECT_EQ(test, 0x4010ULL, regs.pc);
}

static void tcti_switch_executes_add_sub_shifted_register(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	int ret;

	regs.regs[0] = 3;
	regs.regs[19] = 5;
	regs.regs[20] = 9;
	regs.pc = 0x1aa20;

	decoded = tcti_decode_aarch64(0x8b000273U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 8ULL, regs.regs[19]);
	KUNIT_EXPECT_EQ(test, 0x1aa24ULL, regs.pc);

	decoded = tcti_decode_aarch64(0xcb000294U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 6ULL, regs.regs[20]);
	KUNIT_EXPECT_EQ(test, 0x1aa28ULL, regs.pc);
}

static void tcti_switch_executes_add_sub_extended_register(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	int ret;

	regs.regs[8] = 0xffffffff80000010ULL;
	regs.regs[9] = 0x100000000ULL;
	regs.regs[1] = 0x40ULL;
	regs.regs[3] = 0xffffffffU;
	regs.pc = 0x2400;

	decoded = tcti_decode_aarch64(0x8b284128U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x180000010ULL, regs.regs[8]);
	KUNIT_EXPECT_EQ(test, 0x2404ULL, regs.pc);

	decoded = tcti_decode_aarch64(0xcb23c021U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x41ULL, regs.regs[1]);
	KUNIT_EXPECT_EQ(test, 0x2408ULL, regs.pc);
}

static void tcti_switch_executes_pc_relative_address_variants(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	int ret;

	regs.pc = 0x706a8654acULL;

	decoded = tcti_decode_aarch64(0x10000041U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x706a8654b4ULL, regs.regs[1]);
	KUNIT_EXPECT_EQ(test, 0x706a8654b0ULL, regs.pc);

	decoded = tcti_decode_aarch64(0x90000021U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x706a869000ULL, regs.regs[1]);
	KUNIT_EXPECT_EQ(test, 0x706a8654b4ULL, regs.pc);

	decoded = tcti_decode_aarch64(0x10ffffe2U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x706a8654b0ULL, regs.regs[2]);
	KUNIT_EXPECT_EQ(test, 0x706a8654b8ULL, regs.pc);
}

static void tcti_switch_executes_unconditional_branch_immediate(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	int ret;

	regs.pc = 0x1a9b0;

	decoded = tcti_decode_aarch64(0x94002283U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x1a9b4ULL, regs.regs[30]);
	KUNIT_EXPECT_EQ(test, 0x233bcULL, regs.pc);

	decoded = tcti_decode_aarch64(0x17fffff3U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x23388ULL, regs.pc);
}

static void tcti_switch_executes_branch_register(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	int ret;

	regs.regs[19] = 0x1a9b4;
	regs.pc = 0x23404;

	decoded = tcti_decode_aarch64(0xd63f0260U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x23408ULL, regs.regs[30]);
	KUNIT_EXPECT_EQ(test, 0x1a9b4ULL, regs.pc);
}

static void tcti_switch_executes_conditional_branches(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	int ret;

	regs.regs[20] = 0;
	regs.regs[0] = BIT_ULL(63);
	regs.pc = 0x1a9f4;

	decoded = tcti_decode_aarch64(0xb40001d4U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x1aa2cULL, regs.pc);

	regs.pc = 0x1aa08;
	decoded = tcti_decode_aarch64(0xb6f80080U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x1aa18ULL, regs.pc);

	regs.pstate = PSR_Z_BIT;
	regs.pc = 0x1aa14;
	decoded = tcti_decode_aarch64(0x54ffff00U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x1a9f4ULL, regs.pc);
}

static void tcti_switch_executes_conditional_compare(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	int ret;

	regs.regs[8] = 9;
	regs.regs[22] = 9;
	regs.pstate = PSR_Z_BIT;
	regs.pc = 0x7200;

	decoded = tcti_decode_aarch64(0xfa560102U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_NE(test, 0ULL, regs.pstate & PSR_Z_BIT);
	KUNIT_EXPECT_NE(test, 0ULL, regs.pstate & PSR_C_BIT);
	KUNIT_EXPECT_EQ(test, 0ULL, regs.pstate & PSR_N_BIT);
	KUNIT_EXPECT_EQ(test, 0x7204ULL, regs.pc);

	regs.pstate = 0;
	decoded = tcti_decode_aarch64(0xfa560102U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0ULL, regs.pstate & PSR_Z_BIT);
	KUNIT_EXPECT_NE(test, 0ULL, regs.pstate & PSR_C_BIT);
	KUNIT_EXPECT_EQ(test, 0x7208ULL, regs.pc);

	regs.regs[8] = 0;
	regs.pstate = 0;
	decoded = tcti_decode_aarch64(0x7a401900U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_NE(test, 0ULL, regs.pstate & PSR_Z_BIT);
	KUNIT_EXPECT_NE(test, 0ULL, regs.pstate & PSR_C_BIT);
	KUNIT_EXPECT_EQ(test, 0x720cULL, regs.pc);

	regs.regs[1] = 1;
	regs.regs[2] = 2;
	regs.pstate = PSR_C_BIT;
	decoded = tcti_decode_aarch64(0xba422028U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0ULL, regs.pstate & PSR_Z_BIT);
	KUNIT_EXPECT_EQ(test, 0ULL, regs.pstate & PSR_C_BIT);
	KUNIT_EXPECT_EQ(test, 0x7210ULL, regs.pc);
}

static void tcti_switch_executes_conditional_select(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	int ret;

	regs.regs[11] = 0x1234;
	regs.pstate = PSR_C_BIT;
	regs.pc = 0x1ad08;

	decoded = tcti_decode_aarch64(0x5a9f316aU);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x1234ULL, regs.regs[10]);
	KUNIT_EXPECT_EQ(test, 0x1ad0cULL, regs.pc);

	regs.pstate = 0;
	decoded = tcti_decode_aarch64(0x5a9f316aU);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0xffffffffULL, regs.regs[10]);
	KUNIT_EXPECT_EQ(test, 0x1ad10ULL, regs.pc);

	regs.regs[8] = 0x77;
	regs.pstate = PSR_Z_BIT;
	decoded = tcti_decode_aarch64(0x1a9f17e9U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 1ULL, regs.regs[9]);
	KUNIT_EXPECT_EQ(test, 0x1ad14ULL, regs.pc);
}

static void tcti_switch_executes_logical_shifted_register(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	int ret;

	regs.regs[1] = 0x123456789abcdef0ULL;
	regs.regs[8] = 0xffffffffffffffffULL;
	regs.pc = 0x233c8;

	decoded = tcti_decode_aarch64(0xaa0103f3U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x123456789abcdef0ULL, regs.regs[19]);
	KUNIT_EXPECT_EQ(test, 0x233ccULL, regs.pc);

	decoded = tcti_decode_aarch64(0x2a0803e0U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0xffffffffULL, regs.regs[0]);
	KUNIT_EXPECT_EQ(test, 0x233d0ULL, regs.pc);

	regs.regs[10] = 0xf0;
	regs.regs[11] = 0xf;
	regs.pstate = PSR_C_BIT | PSR_V_BIT;
	regs.pc = 0x22994;
	decoded = tcti_decode_aarch64(0xea0b015fU);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0xf0ULL, regs.regs[10]);
	KUNIT_EXPECT_NE(test, 0ULL, regs.pstate & PSR_Z_BIT);
	KUNIT_EXPECT_EQ(test, 0ULL, regs.pstate & PSR_C_BIT);
	KUNIT_EXPECT_EQ(test, 0x22998ULL, regs.pc);

	regs.regs[11] = 0x0000000fULL;
	regs.pc = 0x31060;
	decoded = tcti_decode_aarch64(0x2a2b03e9U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0xfffffff0ULL, regs.regs[9]);
	KUNIT_EXPECT_EQ(test, 0x31064ULL, regs.pc);
}

static void tcti_switch_executes_logical_immediate(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	int ret;

	regs.regs[0] = 0xffffffffU;
	regs.regs[8] = 0x40U;
	regs.pc = 0x1ae14;

	decoded = tcti_decode_aarch64(0x12007808U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x7fffffffULL, regs.regs[8]);
	KUNIT_EXPECT_EQ(test, 0x1ae18ULL, regs.pc);

	decoded = tcti_decode_aarch64(0x32190108U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x7fffffffULL, regs.regs[8]);
	KUNIT_EXPECT_EQ(test, 0x1ae1cULL, regs.pc);

	regs.regs[8] = 0x40U;
	regs.pstate = PSR_C_BIT | PSR_V_BIT;
	decoded = tcti_decode_aarch64(0x7200191fU);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x40ULL, regs.regs[8]);
	KUNIT_EXPECT_EQ(test, 0ULL, regs.pstate & PSR_Z_BIT);
	KUNIT_EXPECT_EQ(test, 0ULL, regs.pstate & PSR_C_BIT);
	KUNIT_EXPECT_EQ(test, 0x1ae20ULL, regs.pc);
}

static void tcti_switch_executes_bitfield(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	int ret;

	regs.regs[22] = 0x12345678ULL;
	regs.regs[24] = 0x123456789abcdef0ULL;
	regs.regs[8] = 1;
	regs.regs[1] = 0xffff0000ULL;
	regs.regs[2] = 0xabcdULL;
	regs.pc = 0x6000;

	decoded = tcti_decode_aarch64(0x53083ed7U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x56ULL, regs.regs[23]);
	KUNIT_EXPECT_EQ(test, 0x6004ULL, regs.pc);

	decoded = tcti_decode_aarch64(0xd350ff09U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x123456789abcULL, regs.regs[9]);
	KUNIT_EXPECT_EQ(test, 0x6008ULL, regs.pc);

	decoded = tcti_decode_aarch64(0x13000100U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0xffffffffULL, regs.regs[0]);
	KUNIT_EXPECT_EQ(test, 0x600cULL, regs.pc);

	decoded = tcti_decode_aarch64(0x33181c41U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0xffffcd00ULL, regs.regs[1]);
	KUNIT_EXPECT_EQ(test, 0x6010ULL, regs.pc);
}

static void tcti_switch_executes_extract(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	int ret;

	regs.regs[8] = 0x8000000000000001ULL;
	regs.regs[11] = 0x40000000ULL;
	regs.regs[13] = 0x00000001ULL;
	regs.pc = 0x8000;

	decoded = tcti_decode_aarch64(0x93c80508U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0xc000000000000000ULL, regs.regs[8]);
	KUNIT_EXPECT_EQ(test, 0x8004ULL, regs.pc);

	decoded = tcti_decode_aarch64(0x138b7dacU);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x80000002ULL, regs.regs[12]);
	KUNIT_EXPECT_EQ(test, 0x8008ULL, regs.pc);
}

static void tcti_switch_executes_data_processing_1source(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	int ret;

	regs.regs[8] = 0x0000f00000000000ULL;
	regs.regs[10] = 0x00000100U;
	regs.pc = 0x8100;

	decoded = tcti_decode_aarch64(0xdac01109U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 16ULL, regs.regs[9]);
	KUNIT_EXPECT_EQ(test, 0x8104ULL, regs.pc);

	decoded = tcti_decode_aarch64(0x5ac0114aU);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 23ULL, regs.regs[10]);
	KUNIT_EXPECT_EQ(test, 0x8108ULL, regs.pc);
}

static void tcti_switch_executes_data_processing_2source(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	int ret;

	regs.regs[10] = 0x1234;
	regs.regs[9] = 4;
	regs.regs[8] = 0x80000000U;
	regs.regs[20] = 8;
	regs.regs[1] = 100;
	regs.regs[11] = 9;
	regs.pc = 0x7000;

	decoded = tcti_decode_aarch64(0x9ac92149U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x12340ULL, regs.regs[9]);
	KUNIT_EXPECT_EQ(test, 0x7004ULL, regs.pc);

	decoded = tcti_decode_aarch64(0x1ad42508U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x800000ULL, regs.regs[8]);
	KUNIT_EXPECT_EQ(test, 0x7008ULL, regs.pc);

	decoded = tcti_decode_aarch64(0x9acb082dU);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 11ULL, regs.regs[13]);
	KUNIT_EXPECT_EQ(test, 0x700cULL, regs.pc);

	regs.regs[2] = 0xfffffffffffffff0ULL;
	regs.regs[3] = 2;
	decoded = tcti_decode_aarch64(0x9ac32841U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0xfffffffffffffffcULL, regs.regs[1]);
	KUNIT_EXPECT_EQ(test, 0x7010ULL, regs.pc);

	regs.regs[5] = 0x80000001U;
	regs.regs[6] = 4;
	decoded = tcti_decode_aarch64(0x1ac62ca4U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x18000000ULL, regs.regs[4]);
	KUNIT_EXPECT_EQ(test, 0x7014ULL, regs.pc);

	regs.regs[8] = (u32)-42;
	regs.regs[9] = 5;
	decoded = tcti_decode_aarch64(0x1ac90d07U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, (u32)-8, regs.regs[7]);
	KUNIT_EXPECT_EQ(test, 0x7018ULL, regs.pc);

	regs.regs[11] = 0;
	decoded = tcti_decode_aarch64(0x9acb082dU);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0ULL, regs.regs[13]);
	KUNIT_EXPECT_EQ(test, 0x701cULL, regs.pc);
}

static void tcti_switch_executes_multiply_add_sub(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	int ret;

	regs.regs[9] = 6;
	regs.regs[23] = 7;
	regs.regs[19] = 5;
	regs.regs[13] = 20;
	regs.regs[11] = 3;
	regs.regs[1] = 100;
	regs.pc = 0x8000;

	decoded = tcti_decode_aarch64(0x9b174d21U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 47ULL, regs.regs[1]);
	KUNIT_EXPECT_EQ(test, 0x8004ULL, regs.pc);

	regs.regs[1] = 100;
	decoded = tcti_decode_aarch64(0x9b0b85aeU);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 40ULL, regs.regs[14]);
	KUNIT_EXPECT_EQ(test, 0x8008ULL, regs.pc);

	decoded = tcti_decode_aarch64(0x9b0a7d2cU);
	regs.regs[10] = 11;
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 66ULL, regs.regs[12]);
	KUNIT_EXPECT_EQ(test, 0x800cULL, regs.pc);

	regs.regs[26] = (u32)-2;
	regs.regs[27] = 4;
	regs.regs[19] = 20;
	decoded = tcti_decode_aarch64(0x9b3b4f5aU);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 12ULL, regs.regs[26]);
	KUNIT_EXPECT_EQ(test, 0x8010ULL, regs.pc);

	regs.regs[20] = 3;
	regs.regs[8] = 5;
	regs.regs[9] = 7;
	decoded = tcti_decode_aarch64(0x9ba82688U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 22ULL, regs.regs[8]);
	KUNIT_EXPECT_EQ(test, 0x8014ULL, regs.pc);

	regs.regs[5] = 9;
	regs.regs[6] = 4;
	regs.regs[7] = 50;
	decoded = tcti_decode_aarch64(0x9ba69ca4U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 14ULL, regs.regs[4]);
	KUNIT_EXPECT_EQ(test, 0x8018ULL, regs.pc);

	regs.regs[4] = -2ULL;
	regs.regs[5] = 3;
	decoded = tcti_decode_aarch64(0x9b457c83U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, ~0ULL, regs.regs[3]);
	KUNIT_EXPECT_EQ(test, 0x801cULL, regs.pc);

	regs.regs[17] = ~0ULL;
	regs.regs[0] = 2;
	decoded = tcti_decode_aarch64(0x9bc07e31U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 1ULL, regs.regs[17]);
	KUNIT_EXPECT_EQ(test, 0x8020ULL, regs.pc);
}

static void tcti_switch_executes_move_wide_immediate(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	int ret;

	regs.regs[25] = 0xffffffffffffffffULL;
	regs.pc = 0x1a9d8;

	decoded = tcti_decode_aarch64(0xd2a00019U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0ULL, regs.regs[25]);
	KUNIT_EXPECT_EQ(test, 0x1a9dcULL, regs.pc);

	decoded = tcti_decode_aarch64(0xf2800319U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 24ULL, regs.regs[25]);
	KUNIT_EXPECT_EQ(test, 0x1a9e0ULL, regs.pc);

	decoded = tcti_decode_aarch64(0x52800334U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 25ULL, regs.regs[20]);
	KUNIT_EXPECT_EQ(test, 0x1a9e4ULL, regs.pc);
}

static void tcti_switch_executes_system_registers(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
#if defined(ORLIX_APP_HOSTED_BOOT)
	unsigned long old_tls = current->thread.user_tls;
#endif
	int ret;

	regs.regs[4] = PSR_N_BIT | PSR_C_BIT;
	regs.pc = 0x5000;

	decoded = tcti_decode_aarch64(0xd51b4204U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, PSR_N_BIT | PSR_C_BIT,
			regs.pstate & (PSR_N_BIT | PSR_Z_BIT |
				       PSR_C_BIT | PSR_V_BIT));
	KUNIT_EXPECT_EQ(test, 0x5004ULL, regs.pc);

	decoded = tcti_decode_aarch64(0xd53b4203U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, PSR_N_BIT | PSR_C_BIT, regs.regs[3]);
	KUNIT_EXPECT_EQ(test, 0x5008ULL, regs.pc);

#if defined(ORLIX_APP_HOSTED_BOOT)
	current->thread.user_tls = 0x700000123000ULL;
	regs.pc = 0x1a9e8;

	decoded = tcti_decode_aarch64(0xd53bd05aU);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x700000123000ULL, regs.regs[26]);
	KUNIT_EXPECT_EQ(test, 0x1a9ecULL, regs.pc);

	regs.regs[0] = 0x700000456000ULL;
	decoded = tcti_decode_aarch64(0xd51bd040U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x700000456000ULL, current->thread.user_tls);
	KUNIT_EXPECT_EQ(test, 0x1a9f0ULL, regs.pc);

	current->thread.user_tls = old_tls;
#endif
}

static void tcti_switch_executes_exclusive_monitor_clear(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	int ret;

	current->thread.user_exclusive_address = 0x70000040ULL;
	current->thread.user_exclusive_size = sizeof(u32);
	current->thread.user_exclusive_valid = 1;
	regs.pc = 0x8200;

	decoded = tcti_decode_aarch64(0xd5033f5fU);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0UL, current->thread.user_exclusive_address);
	KUNIT_EXPECT_EQ(test, 0, current->thread.user_exclusive_valid);
	KUNIT_EXPECT_EQ(test, 0x8204ULL, regs.pc);
}

static void tcti_switch_fails_store_exclusive_without_reservation(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	int ret;

	current->thread.user_exclusive_address = 0;
	current->thread.user_exclusive_size = 0;
	current->thread.user_exclusive_valid = 0;
	regs.regs[19] = 0x70001000ULL;
	regs.regs[8] = 0x1234;
	regs.pc = 0x8300;

	decoded = tcti_decode_aarch64(0x880a7e68U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 1ULL, regs.regs[10]);
	KUNIT_EXPECT_EQ(test, 0, current->thread.user_exclusive_valid);
	KUNIT_EXPECT_EQ(test, 0x8304ULL, regs.pc);
}

static struct kunit_case tcti_decode_test_cases[] = {
	KUNIT_CASE(tcti_decode_recognizes_svc_zero),
	KUNIT_CASE(tcti_decode_rejects_unknown_instruction),
	KUNIT_CASE(tcti_decode_recognizes_hint_class),
	KUNIT_CASE(tcti_decode_recognizes_add_sub_immediate_class),
	KUNIT_CASE(tcti_decode_recognizes_add_sub_shifted_register_class),
	KUNIT_CASE(tcti_decode_recognizes_add_sub_extended_register_class),
	KUNIT_CASE(tcti_decode_recognizes_pc_relative_address_class),
	KUNIT_CASE(tcti_decode_recognizes_unconditional_branch_immediate_class),
	KUNIT_CASE(tcti_decode_recognizes_branch_register_class),
	KUNIT_CASE(tcti_decode_recognizes_conditional_branch_classes),
	KUNIT_CASE(tcti_decode_recognizes_conditional_compare_class),
	KUNIT_CASE(tcti_decode_recognizes_conditional_select_class),
	KUNIT_CASE(tcti_decode_recognizes_load_store_pair_class),
	KUNIT_CASE(tcti_decode_recognizes_load_store_unsigned_class),
	KUNIT_CASE(tcti_decode_recognizes_load_store_signed_immediate_class),
	KUNIT_CASE(tcti_decode_recognizes_load_store_register_offset_class),
	KUNIT_CASE(tcti_decode_recognizes_logical_shifted_register_class),
	KUNIT_CASE(tcti_decode_recognizes_logical_immediate_class),
	KUNIT_CASE(tcti_decode_recognizes_bitfield_class),
	KUNIT_CASE(tcti_decode_recognizes_extract_class),
	KUNIT_CASE(tcti_decode_recognizes_data_processing_1source_class),
	KUNIT_CASE(tcti_decode_recognizes_data_processing_2source_class),
	KUNIT_CASE(tcti_decode_recognizes_multiply_add_sub_class),
	KUNIT_CASE(tcti_decode_recognizes_move_wide_immediate_class),
	KUNIT_CASE(tcti_decode_recognizes_system_register_class),
	KUNIT_CASE(tcti_decode_recognizes_exclusive_monitor_clear),
	KUNIT_CASE(tcti_decode_recognizes_load_store_exclusive_class),
	KUNIT_CASE(tcti_gadget_program_lowers_hint_as_data_stream),
	KUNIT_CASE(tcti_gadget_program_executes_extract),
	KUNIT_CASE(tcti_gadget_program_rejects_svc_lowering),
	KUNIT_CASE(tcti_gadget_program_executes_init001_movz_prefix),
	KUNIT_CASE(tcti_gadget_program_matches_switch_debug_init001_movz_prefix),
	KUNIT_CASE(tcti_block_cache_returns_cached_program),
	KUNIT_CASE(tcti_block_cache_is_bounded),
	KUNIT_CASE(tcti_block_cache_invalidation_bumps_generation),
	KUNIT_CASE(tcti_tlb_separates_access_classes),
	KUNIT_CASE(tcti_tlb_flushes_on_generation_change),
	KUNIT_CASE(tcti_switch_executes_hint_as_noop),
	KUNIT_CASE(tcti_switch_executes_add_immediate_with_sp_source),
	KUNIT_CASE(tcti_switch_executes_add_sub_immediate_variants),
	KUNIT_CASE(tcti_switch_executes_add_sub_shifted_register),
	KUNIT_CASE(tcti_switch_executes_add_sub_extended_register),
	KUNIT_CASE(tcti_switch_executes_pc_relative_address_variants),
	KUNIT_CASE(tcti_switch_executes_unconditional_branch_immediate),
	KUNIT_CASE(tcti_switch_executes_branch_register),
	KUNIT_CASE(tcti_switch_executes_conditional_branches),
	KUNIT_CASE(tcti_switch_executes_conditional_compare),
	KUNIT_CASE(tcti_switch_executes_conditional_select),
	KUNIT_CASE(tcti_switch_executes_logical_shifted_register),
	KUNIT_CASE(tcti_switch_executes_logical_immediate),
	KUNIT_CASE(tcti_switch_executes_bitfield),
	KUNIT_CASE(tcti_switch_executes_extract),
	KUNIT_CASE(tcti_switch_executes_data_processing_1source),
	KUNIT_CASE(tcti_switch_executes_data_processing_2source),
	KUNIT_CASE(tcti_switch_executes_multiply_add_sub),
	KUNIT_CASE(tcti_switch_executes_move_wide_immediate),
	KUNIT_CASE(tcti_switch_executes_system_registers),
	KUNIT_CASE(tcti_switch_executes_exclusive_monitor_clear),
	KUNIT_CASE(tcti_switch_fails_store_exclusive_without_reservation),
	{}
};

static struct kunit_suite tcti_decode_test_suite = {
	.name = "orlix-tcti-decode",
	.test_cases = tcti_decode_test_cases,
};

kunit_test_suite(tcti_decode_test_suite);

MODULE_LICENSE("GPL");
