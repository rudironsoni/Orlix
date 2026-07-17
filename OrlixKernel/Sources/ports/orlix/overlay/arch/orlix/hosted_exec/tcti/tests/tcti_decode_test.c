// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched/mm.h>
#include <linux/syscalls.h>
#include <asm/hosted_exec.h>
#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/unistd.h>
#include <asm/tcti.h>

#include "../block_cache.h"
#include "../decode_aarch64.h"
#include "../engine.h"
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

static void tcti_decode_rejects_brk_one(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;

	decoded = tcti_decode_aarch64(0xd4200020U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_UNSUPPORTED, decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 0xd4200020U, decoded.instruction);
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
	KUNIT_EXPECT_EQ(test, 1U, decoded.offset_extend);
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

	decoded = tcti_decode_aarch64(0xd61f00a0U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_UNCONDITIONAL_BRANCH_REGISTER,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_BRANCH_REGISTER_BR,
			decoded.branch_register_op);
	KUNIT_EXPECT_FALSE(test, decoded.link);
	KUNIT_EXPECT_EQ(test, 5U, decoded.rn);

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

	decoded = tcti_decode_aarch64(0xb7f80080U);

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

	decoded = tcti_decode_aarch64(0xfa4a0100U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_CONDITIONAL_COMPARE,
			decoded.decode_class);
	KUNIT_EXPECT_TRUE(test, decoded.is_64bit);
	KUNIT_EXPECT_TRUE(test, decoded.subtract);
	KUNIT_EXPECT_FALSE(test, decoded.immediate);
	KUNIT_EXPECT_EQ(test, 8U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 10U, decoded.rm);
	KUNIT_EXPECT_EQ(test, 0U, decoded.condition);
	KUNIT_EXPECT_EQ(test, 0U, decoded.nzcv);

	decoded = tcti_decode_aarch64(0xfa4019a4U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_CONDITIONAL_COMPARE,
			decoded.decode_class);
	KUNIT_EXPECT_TRUE(test, decoded.is_64bit);
	KUNIT_EXPECT_TRUE(test, decoded.subtract);
	KUNIT_EXPECT_TRUE(test, decoded.immediate);
	KUNIT_EXPECT_EQ(test, 13U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 0U, decoded.imm12);
	KUNIT_EXPECT_EQ(test, 1U, decoded.condition);
	KUNIT_EXPECT_EQ(test, 4U, decoded.nzcv);

	decoded = tcti_decode_aarch64(0x7a401ae4U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_CONDITIONAL_COMPARE,
			decoded.decode_class);
	KUNIT_EXPECT_FALSE(test, decoded.is_64bit);
	KUNIT_EXPECT_TRUE(test, decoded.subtract);
	KUNIT_EXPECT_TRUE(test, decoded.immediate);
	KUNIT_EXPECT_EQ(test, 23U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 0U, decoded.imm12);
	KUNIT_EXPECT_EQ(test, 1U, decoded.condition);
	KUNIT_EXPECT_EQ(test, 4U, decoded.nzcv);

	decoded = tcti_decode_aarch64(0x3a41b9c4U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_CONDITIONAL_COMPARE,
			decoded.decode_class);
	KUNIT_EXPECT_FALSE(test, decoded.is_64bit);
	KUNIT_EXPECT_FALSE(test, decoded.subtract);
	KUNIT_EXPECT_TRUE(test, decoded.immediate);
	KUNIT_EXPECT_EQ(test, 14U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 1U, decoded.imm12);
	KUNIT_EXPECT_EQ(test, 11U, decoded.condition);
	KUNIT_EXPECT_EQ(test, 4U, decoded.nzcv);
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

	decoded = tcti_decode_aarch64(0x69402c09U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_LOAD_STORE_PAIR,
			decoded.decode_class);
	KUNIT_EXPECT_TRUE(test, decoded.load);
	KUNIT_EXPECT_TRUE(test, decoded.sign_extend_load);
	KUNIT_EXPECT_EQ(test, 9U, decoded.rt);
	KUNIT_EXPECT_EQ(test, 11U, decoded.rt2);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 4U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 8U, decoded.result_size);
	KUNIT_EXPECT_EQ(test, 0LL, decoded.memory_offset);
	KUNIT_EXPECT_EQ(test, TCTI_MEMORY_INDEX_SIGNED_OFFSET,
			decoded.memory_index_mode);

	decoded = tcti_decode_aarch64(0xa8c27bfdU);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_LOAD_STORE_PAIR,
			decoded.decode_class);
	KUNIT_EXPECT_TRUE(test, decoded.load);
	KUNIT_EXPECT_EQ(test, 29U, decoded.rt);
	KUNIT_EXPECT_EQ(test, 30U, decoded.rt2);
	KUNIT_EXPECT_EQ(test, 31U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 8U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 32LL, decoded.memory_offset);
	KUNIT_EXPECT_EQ(test, TCTI_MEMORY_INDEX_POST,
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

	decoded = tcti_decode_aarch64(0x6d0123e9U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_LOAD_STORE_PAIR,
			decoded.decode_class);
	KUNIT_EXPECT_FALSE(test, decoded.load);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);
	KUNIT_EXPECT_EQ(test, 9U, decoded.rt);
	KUNIT_EXPECT_EQ(test, 8U, decoded.rt2);
	KUNIT_EXPECT_EQ(test, 31U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 8U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 16LL, decoded.memory_offset);
	KUNIT_EXPECT_EQ(test, TCTI_MEMORY_INDEX_SIGNED_OFFSET,
			decoded.memory_index_mode);

	decoded = tcti_decode_aarch64(0xad400901U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_LOAD_STORE_PAIR,
			decoded.decode_class);
	KUNIT_EXPECT_TRUE(test, decoded.load);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);
	KUNIT_EXPECT_EQ(test, 1U, decoded.rt);
	KUNIT_EXPECT_EQ(test, 2U, decoded.rt2);
	KUNIT_EXPECT_EQ(test, 8U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 16U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 0LL, decoded.memory_offset);
	KUNIT_EXPECT_EQ(test, TCTI_MEMORY_INDEX_SIGNED_OFFSET,
			decoded.memory_index_mode);

	decoded = tcti_decode_aarch64(0xad0103e2U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_LOAD_STORE_PAIR,
			decoded.decode_class);
	KUNIT_EXPECT_FALSE(test, decoded.load);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);
	KUNIT_EXPECT_EQ(test, 2U, decoded.rt);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rt2);
	KUNIT_EXPECT_EQ(test, 31U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 16U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 32LL, decoded.memory_offset);
	KUNIT_EXPECT_EQ(test, TCTI_MEMORY_INDEX_SIGNED_OFFSET,
			decoded.memory_index_mode);

	decoded = tcti_decode_aarch64(0xadbf03e1U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_LOAD_STORE_PAIR,
			decoded.decode_class);
	KUNIT_EXPECT_FALSE(test, decoded.load);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);
	KUNIT_EXPECT_EQ(test, 1U, decoded.rt);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rt2);
	KUNIT_EXPECT_EQ(test, 31U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 16U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, -32LL, decoded.memory_offset);
	KUNIT_EXPECT_EQ(test, TCTI_MEMORY_INDEX_PRE,
			decoded.memory_index_mode);

	decoded = tcti_decode_aarch64(0xad0583e1U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_LOAD_STORE_PAIR,
			decoded.decode_class);
	KUNIT_EXPECT_FALSE(test, decoded.load);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);
	KUNIT_EXPECT_EQ(test, 1U, decoded.rt);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rt2);
	KUNIT_EXPECT_EQ(test, 31U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 16U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 176LL, decoded.memory_offset);
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

	decoded = tcti_decode_aarch64(0x3dc00100U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_LOAD_STORE_UNSIGNED_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_TRUE(test, decoded.load);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rt);
	KUNIT_EXPECT_EQ(test, 8U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 16U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 0LL, decoded.memory_offset);

	decoded = tcti_decode_aarch64(0x3d8007e1U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_LOAD_STORE_UNSIGNED_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_FALSE(test, decoded.load);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);
	KUNIT_EXPECT_EQ(test, 1U, decoded.rt);
	KUNIT_EXPECT_EQ(test, 31U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 16U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 16LL, decoded.memory_offset);

	decoded = tcti_decode_aarch64(0xfd005beaU);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_LOAD_STORE_UNSIGNED_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_FALSE(test, decoded.load);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);
	KUNIT_EXPECT_EQ(test, 10U, decoded.rt);
	KUNIT_EXPECT_EQ(test, 31U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 8U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 176LL, decoded.memory_offset);

	decoded = tcti_decode_aarch64(0xbd01c260U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_LOAD_STORE_UNSIGNED_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_FALSE(test, decoded.load);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rt);
	KUNIT_EXPECT_EQ(test, 19U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 4U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 0x1c0LL, decoded.memory_offset);
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

	decoded = tcti_decode_aarch64(0x3c9a03a0U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_LOAD_STORE_SIGNED_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_FALSE(test, decoded.load);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rt);
	KUNIT_EXPECT_EQ(test, 29U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 16U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, -96LL, decoded.memory_offset);
	KUNIT_EXPECT_EQ(test, TCTI_MEMORY_INDEX_SIGNED_OFFSET,
			decoded.memory_index_mode);

	decoded = tcti_decode_aarch64(0x3c9f0fe0U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_LOAD_STORE_SIGNED_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_FALSE(test, decoded.load);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rt);
	KUNIT_EXPECT_EQ(test, 31U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 16U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, -16LL, decoded.memory_offset);
	KUNIT_EXPECT_EQ(test, TCTI_MEMORY_INDEX_PRE,
			decoded.memory_index_mode);

	decoded = tcti_decode_aarch64(0x3cc383e0U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_LOAD_STORE_SIGNED_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_TRUE(test, decoded.load);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rt);
	KUNIT_EXPECT_EQ(test, 31U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 16U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 56LL, decoded.memory_offset);
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

	decoded = tcti_decode_aarch64(0xf8286920U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_LOAD_STORE_REGISTER_OFFSET,
			decoded.decode_class);
	KUNIT_EXPECT_FALSE(test, decoded.load);
	KUNIT_EXPECT_FALSE(test, decoded.sign_extend_load);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rt);
	KUNIT_EXPECT_EQ(test, 9U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 8U, decoded.rm);
	KUNIT_EXPECT_EQ(test, 8U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 8U, decoded.result_size);
	KUNIT_EXPECT_EQ(test, 3U, decoded.offset_extend);
	KUNIT_EXPECT_FALSE(test, decoded.offset_shift);

	decoded = tcti_decode_aarch64(0x3ce86920U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_LOAD_STORE_REGISTER_OFFSET,
			decoded.decode_class);
	KUNIT_EXPECT_TRUE(test, decoded.load);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rt);
	KUNIT_EXPECT_EQ(test, 9U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 8U, decoded.rm);
	KUNIT_EXPECT_EQ(test, 16U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 16U, decoded.result_size);
	KUNIT_EXPECT_EQ(test, 3U, decoded.offset_extend);
	KUNIT_EXPECT_FALSE(test, decoded.offset_shift);

	decoded = tcti_decode_aarch64(0x3ca97940U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_LOAD_STORE_REGISTER_OFFSET,
			decoded.decode_class);
	KUNIT_EXPECT_FALSE(test, decoded.load);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rt);
	KUNIT_EXPECT_EQ(test, 10U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 9U, decoded.rm);
	KUNIT_EXPECT_EQ(test, 16U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 16U, decoded.result_size);
	KUNIT_EXPECT_EQ(test, 3U, decoded.offset_extend);
	KUNIT_EXPECT_TRUE(test, decoded.offset_shift);

	decoded = tcti_decode_aarch64(0x3cead900U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_LOAD_STORE_REGISTER_OFFSET,
			decoded.decode_class);
	KUNIT_EXPECT_TRUE(test, decoded.load);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rt);
	KUNIT_EXPECT_EQ(test, 8U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 10U, decoded.rm);
	KUNIT_EXPECT_EQ(test, 16U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 16U, decoded.result_size);
	KUNIT_EXPECT_EQ(test, 6U, decoded.offset_extend);
	KUNIT_EXPECT_TRUE(test, decoded.offset_shift);
}

static void tcti_decode_ldrsw_signed_immediate_writes_x_register(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;

	decoded = tcti_decode_aarch64(0xb9801848U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_LOAD_STORE_UNSIGNED_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_TRUE(test, decoded.load);
	KUNIT_EXPECT_TRUE(test, decoded.sign_extend_load);
	KUNIT_EXPECT_EQ(test, 8U, decoded.rt);
	KUNIT_EXPECT_EQ(test, 2U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 4U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 8U, decoded.result_size);
	KUNIT_EXPECT_EQ(test, 24LL, decoded.memory_offset);
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

	decoded = tcti_decode_aarch64(0x5ac00288U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_DATA_PROCESSING_1SOURCE,
			decoded.decode_class);
	KUNIT_EXPECT_FALSE(test, decoded.is_64bit);
	KUNIT_EXPECT_EQ(test, TCTI_DP1_RBIT, decoded.dp1_op);
	KUNIT_EXPECT_EQ(test, 8U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 20U, decoded.rn);

	decoded = tcti_decode_aarch64(0x5ac00503U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_DATA_PROCESSING_1SOURCE,
			decoded.decode_class);
	KUNIT_EXPECT_FALSE(test, decoded.is_64bit);
	KUNIT_EXPECT_EQ(test, TCTI_DP1_REV16, decoded.dp1_op);
	KUNIT_EXPECT_EQ(test, 3U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 8U, decoded.rn);

	decoded = tcti_decode_aarch64(0x5ac00909U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_DATA_PROCESSING_1SOURCE,
			decoded.decode_class);
	KUNIT_EXPECT_FALSE(test, decoded.is_64bit);
	KUNIT_EXPECT_EQ(test, TCTI_DP1_REV, decoded.dp1_op);
	KUNIT_EXPECT_EQ(test, 9U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 8U, decoded.rn);
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

	decoded = tcti_decode_aarch64(0xd53b4401U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SYSTEM_REGISTER,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_SYSTEM_REGISTER_FPCR,
			decoded.system_register);
	KUNIT_EXPECT_FALSE(test, decoded.system_register_write);
	KUNIT_EXPECT_EQ(test, 1U, decoded.rt);

	decoded = tcti_decode_aarch64(0xd51b4421U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SYSTEM_REGISTER,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_SYSTEM_REGISTER_FPSR,
			decoded.system_register);
	KUNIT_EXPECT_TRUE(test, decoded.system_register_write);
	KUNIT_EXPECT_EQ(test, 1U, decoded.rt);
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

static void tcti_decode_recognizes_simd_modified_immediates(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;

	decoded = tcti_decode_aarch64(0x6f00e400U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_MODIFIED_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 0ULL, decoded.logical_immediate);
	KUNIT_EXPECT_EQ(test, 16U, decoded.access_size);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);

	decoded = tcti_decode_aarch64(0x2f00e400U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_MODIFIED_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 0ULL, decoded.logical_immediate);
	KUNIT_EXPECT_EQ(test, 8U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 8U, decoded.result_size);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);

	decoded = tcti_decode_aarch64(0x6f07e7e0U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_MODIFIED_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rd);
	KUNIT_EXPECT_EQ(test, ~0ULL, decoded.logical_immediate);
	KUNIT_EXPECT_EQ(test, 16U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 16U, decoded.result_size);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);

	decoded = tcti_decode_aarch64(0x0f002420U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_MODIFIED_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 0x0000010000000100ULL,
			decoded.logical_immediate);
	KUNIT_EXPECT_EQ(test, 8U, decoded.access_size);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);

	decoded = tcti_decode_aarch64(0x0f026441U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_MODIFIED_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 1U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 0x4200000042000000ULL,
			decoded.logical_immediate);
	KUNIT_EXPECT_EQ(test, 8U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 8U, decoded.result_size);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);

	decoded = tcti_decode_aarch64(0x4f000421U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_MODIFIED_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 1U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 0x0000000100000001ULL,
			decoded.logical_immediate);
	KUNIT_EXPECT_EQ(test, 16U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 16U, decoded.result_size);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);

	decoded = tcti_decode_aarch64(0x4f06e7e0U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_MODIFIED_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 0xdfdfdfdfdfdfdfdfULL,
			decoded.logical_immediate);
	KUNIT_EXPECT_EQ(test, 16U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 16U, decoded.result_size);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);

	decoded = tcti_decode_aarch64(0x0f00e548U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_MODIFIED_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 8U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 0x0a0a0a0a0a0a0a0aULL,
			decoded.logical_immediate);
	KUNIT_EXPECT_EQ(test, 8U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 8U, decoded.result_size);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);

	decoded = tcti_decode_aarch64(0x4f020420U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_MODIFIED_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 0x0000004100000041ULL,
			decoded.logical_immediate);
	KUNIT_EXPECT_EQ(test, 16U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 16U, decoded.result_size);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);

	decoded = tcti_decode_aarch64(0x6f00e41fU);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_MODIFIED_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 31U, decoded.rd);

	decoded = tcti_decode_aarch64(0x6f00e420U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_MODIFIED_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 0x00000000000000ffULL,
			decoded.logical_immediate);
}

static void tcti_decode_recognizes_basenc_simd_immediates(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;

	decoded = tcti_decode_aarch64(0x0f018560U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_MODIFIED_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 0x002b002b002b002bULL,
			decoded.logical_immediate);
	KUNIT_EXPECT_EQ(test, 8U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 8U, decoded.result_size);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);

	decoded = tcti_decode_aarch64(0x0f018620U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_MODIFIED_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 0x0031003100310031ULL,
			decoded.logical_immediate);
	KUNIT_EXPECT_EQ(test, 8U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 8U, decoded.result_size);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);
}

static void tcti_decode_recognizes_complete_simd_modified_immediate_family(
	struct kunit *test)
{
	struct tcti_decoded_instruction decoded;

	decoded = tcti_decode_aarch64(0x4f002640U);
	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_MODIFIED_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_SIMD_MODIMM_MOVI,
			decoded.simd_modified_immediate_op);
	KUNIT_EXPECT_EQ(test, 0x0000120000001200ULL,
			decoded.logical_immediate);
	KUNIT_EXPECT_EQ(test, 16U, decoded.result_size);

	decoded = tcti_decode_aarch64(0x6f014681U);
	KUNIT_EXPECT_EQ(test, TCTI_SIMD_MODIMM_MVNI,
			decoded.simd_modified_immediate_op);
	KUNIT_EXPECT_EQ(test, 0x0034000000340000ULL,
			decoded.logical_immediate);

	decoded = tcti_decode_aarch64(0x4f0276c2U);
	KUNIT_EXPECT_EQ(test, TCTI_SIMD_MODIMM_ORR,
			decoded.simd_modified_immediate_op);
	KUNIT_EXPECT_EQ(test, 0x5600000056000000ULL,
			decoded.logical_immediate);

	decoded = tcti_decode_aarch64(0x6f03b703U);
	KUNIT_EXPECT_EQ(test, TCTI_SIMD_MODIMM_BIC,
			decoded.simd_modified_immediate_op);
	KUNIT_EXPECT_EQ(test, 0x7800780078007800ULL,
			decoded.logical_immediate);

	decoded = tcti_decode_aarch64(0x4f04c744U);
	KUNIT_EXPECT_EQ(test, TCTI_SIMD_MODIMM_MOVI,
			decoded.simd_modified_immediate_op);
	KUNIT_EXPECT_EQ(test, 0x00009aff00009affULL,
			decoded.logical_immediate);

	decoded = tcti_decode_aarch64(0x6f05d785U);
	KUNIT_EXPECT_EQ(test, TCTI_SIMD_MODIMM_MVNI,
			decoded.simd_modified_immediate_op);
	KUNIT_EXPECT_EQ(test, 0x00bcffff00bcffffULL,
			decoded.logical_immediate);

	decoded = tcti_decode_aarch64(0x6f02e6a6U);
	KUNIT_EXPECT_EQ(test, TCTI_SIMD_MODIMM_MOVI,
			decoded.simd_modified_immediate_op);
	KUNIT_EXPECT_EQ(test, 0x00ff00ff00ff00ffULL,
			decoded.logical_immediate);

	decoded = tcti_decode_aarch64(0x4f03f607U);
	KUNIT_EXPECT_EQ(test, TCTI_SIMD_MODIMM_MOVI,
			decoded.simd_modified_immediate_op);
	KUNIT_EXPECT_EQ(test, 0x3f8000003f800000ULL,
			decoded.logical_immediate);

	decoded = tcti_decode_aarch64(0x6f00f408U);
	KUNIT_EXPECT_EQ(test, TCTI_SIMD_MODIMM_MOVI,
			decoded.simd_modified_immediate_op);
	KUNIT_EXPECT_EQ(test, 0x4000000000000000ULL,
			decoded.logical_immediate);

	decoded = tcti_decode_aarch64(0x2f00f400U);
	KUNIT_EXPECT_EQ(test, TCTI_DECODE_UNSUPPORTED, decoded.decode_class);
}

static void tcti_decode_recognizes_complete_simd_dup_gpr_family(
	struct kunit *test)
{
	static const struct {
		u32 instruction;
		u8 rd;
		u8 rn;
		u8 access_size;
		u8 result_size;
	} cases[] = {
		{ 0x4e010d49U, 9, 10, sizeof(u8), 2 * sizeof(u64) },
		{ 0x4e020d6aU, 10, 11, sizeof(u16), 2 * sizeof(u64) },
		{ 0x0e040d8bU, 11, 12, sizeof(u32), sizeof(u64) },
		{ 0x4e040dacU, 12, 13, sizeof(u32), 2 * sizeof(u64) },
		{ 0x4e080dcdU, 13, 14, sizeof(u64), 2 * sizeof(u64) },
	};
	size_t index;

	for (index = 0; index < ARRAY_SIZE(cases); index++) {
		struct tcti_decoded_instruction decoded =
			tcti_decode_aarch64(cases[index].instruction);

		KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE,
				decoded.decode_class);
		KUNIT_EXPECT_EQ(test, TCTI_SIMD_ELEMENT_MOVE_DUP,
				decoded.simd_element_move_op);
		KUNIT_EXPECT_EQ(test, cases[index].rd, decoded.rd);
		KUNIT_EXPECT_EQ(test, cases[index].rn, decoded.rn);
		KUNIT_EXPECT_EQ(test, cases[index].access_size,
				decoded.access_size);
		KUNIT_EXPECT_EQ(test, cases[index].result_size,
				decoded.result_size);
		KUNIT_EXPECT_TRUE(test, decoded.simd_fp);
		KUNIT_EXPECT_TRUE(test, decoded.immediate);
	}

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_UNSUPPORTED,
			tcti_decode_aarch64(0x0e080c00U).decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_DECODE_UNSUPPORTED,
			tcti_decode_aarch64(0x4e030c00U).decode_class);
}

static void tcti_decode_recognizes_simd_dup_4h_gpr(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;

	decoded = tcti_decode_aarch64(0x0e020d81U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 1U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 12U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 2U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 8U, decoded.result_size);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);
	KUNIT_EXPECT_TRUE(test, decoded.immediate);
}

static void tcti_decode_recognizes_simd_dup_2d_gpr(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;

	decoded = tcti_decode_aarch64(0x6e144401U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 1U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 4U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 4U, decoded.result_size);
	KUNIT_EXPECT_EQ(test, 2U, decoded.simd_destination_index);
	KUNIT_EXPECT_EQ(test, 2U, decoded.simd_source_index);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);
	KUNIT_EXPECT_FALSE(test, decoded.immediate);

	decoded = tcti_decode_aarch64(0x4e080d80U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 12U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 8U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 16U, decoded.result_size);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);
	KUNIT_EXPECT_TRUE(test, decoded.immediate);

	decoded = tcti_decode_aarch64(0x4e040c01U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 1U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 4U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 16U, decoded.result_size);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);
	KUNIT_EXPECT_TRUE(test, decoded.immediate);
}

static void tcti_decode_recognizes_simd_mov_d1_d0(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;

	decoded = tcti_decode_aarch64(0x6e180400U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 8U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 8U, decoded.result_size);
	KUNIT_EXPECT_EQ(test, 1U, decoded.simd_destination_index);
	KUNIT_EXPECT_EQ(test, 0U, decoded.simd_source_index);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);
	KUNIT_EXPECT_FALSE(test, decoded.immediate);
}

static void tcti_decode_recognizes_complete_simd_xtn_family(
	struct kunit *test)
{
	static const struct {
		u32 instruction;
		u8 access_size;
		u8 result_size;
		bool upper;
	} cases[] = {
		{ 0x0e212800U, sizeof(u16), sizeof(u8), false },
		{ 0x4e212800U, sizeof(u16), sizeof(u8), true },
		{ 0x0e612800U, sizeof(u32), sizeof(u16), false },
		{ 0x4e612800U, sizeof(u32), sizeof(u16), true },
		{ 0x0ea12800U, sizeof(u64), sizeof(u32), false },
		{ 0x4ea12800U, sizeof(u64), sizeof(u32), true },
	};
	size_t index;

	for (index = 0; index < ARRAY_SIZE(cases); index++) {
		struct tcti_decoded_instruction decoded =
			tcti_decode_aarch64(cases[index].instruction);

		KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE,
				decoded.decode_class);
		KUNIT_EXPECT_EQ(test, TCTI_SIMD_ELEMENT_MOVE_XTN,
				decoded.simd_element_move_op);
		KUNIT_EXPECT_EQ(test, cases[index].access_size,
				decoded.access_size);
		KUNIT_EXPECT_EQ(test, cases[index].result_size,
				decoded.result_size);
		KUNIT_EXPECT_EQ(test, cases[index].upper,
				!!decoded.simd_destination_index);
	}

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_UNSUPPORTED,
			tcti_decode_aarch64(0x0ee12800U).decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_DECODE_UNSUPPORTED,
			tcti_decode_aarch64(0x4ee12800U).decode_class);
}

static void tcti_decode_recognizes_simd_xtn_4h(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;

	decoded = tcti_decode_aarch64(0x0e612800U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 4U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 2U, decoded.result_size);
	KUNIT_EXPECT_EQ(test, TCTI_SIMD_ELEMENT_MOVE_XTN,
			decoded.simd_element_move_op);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);

	decoded = tcti_decode_aarch64(0x0ea128a5U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 5U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 5U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 8U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 4U, decoded.result_size);
	KUNIT_EXPECT_EQ(test, TCTI_SIMD_ELEMENT_MOVE_XTN,
			decoded.simd_element_move_op);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);

	decoded = tcti_decode_aarch64(0x0e212821U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 1U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 1U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 2U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 1U, decoded.result_size);
	KUNIT_EXPECT_EQ(test, TCTI_SIMD_ELEMENT_MOVE_XTN,
			decoded.simd_element_move_op);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);
}

static void tcti_decode_recognizes_simd_ushll_8h(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;

	decoded = tcti_decode_aarch64(0x2f08a421U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 1U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 1U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 1U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 16U, decoded.result_size);
	KUNIT_EXPECT_EQ(test, TCTI_SIMD_ELEMENT_MOVE_USHLL,
			decoded.simd_element_move_op);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);

	decoded = tcti_decode_aarch64(0x2f10a421U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 1U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 1U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 2U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 16U, decoded.result_size);
	KUNIT_EXPECT_EQ(test, TCTI_SIMD_ELEMENT_MOVE_USHLL,
			decoded.simd_element_move_op);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);

	decoded = tcti_decode_aarch64(0x6f10a485U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 5U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 4U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 2U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 16U, decoded.result_size);
	KUNIT_EXPECT_EQ(test, 4U, decoded.simd_source_index);
	KUNIT_EXPECT_EQ(test, TCTI_SIMD_ELEMENT_MOVE_USHLL,
			decoded.simd_element_move_op);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);

	decoded = tcti_decode_aarch64(0x6f20a4a6U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 6U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 5U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 4U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 16U, decoded.result_size);
	KUNIT_EXPECT_EQ(test, 2U, decoded.simd_source_index);
	KUNIT_EXPECT_EQ(test, TCTI_SIMD_ELEMENT_MOVE_USHLL,
			decoded.simd_element_move_op);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);
}

static void
tcti_decode_recognizes_complete_simd_shift_left_long_family(struct kunit *test)
{
	static const u8 access_sizes[] = {
		sizeof(u8), sizeof(u16), sizeof(u32),
	};
	size_t size_index;
	u8 is_unsigned;
	u8 upper;
	u8 shift;

	for (is_unsigned = 0; is_unsigned < 2; is_unsigned++) {
		for (upper = 0; upper < 2; upper++) {
			for (size_index = 0;
			     size_index < ARRAY_SIZE(access_sizes);
			     size_index++) {
				u8 access_size = access_sizes[size_index];
				u8 element_bits = access_size * 8;

				for (shift = 0; shift < element_bits; shift++) {
					u32 instruction = 0x0f00a400U |
						(is_unsigned ? BIT(29) : 0) |
						(upper ? BIT(30) : 0) |
						((element_bits + shift) << 16) |
						(4U << 5) | 5U;
					struct tcti_decoded_instruction decoded =
						tcti_decode_aarch64(instruction);

					KUNIT_EXPECT_EQ(test,
						TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE,
						decoded.decode_class);
					KUNIT_EXPECT_EQ(test, 5U, decoded.rd);
					KUNIT_EXPECT_EQ(test, 4U, decoded.rn);
					KUNIT_EXPECT_EQ(test, access_size,
							decoded.access_size);
					KUNIT_EXPECT_EQ(test, 16U,
							decoded.result_size);
					KUNIT_EXPECT_EQ(test, shift,
							decoded.shift_amount);
					KUNIT_EXPECT_EQ(test,
						upper ? sizeof(u64) / access_size : 0,
						decoded.simd_source_index);
					KUNIT_EXPECT_EQ(test,
						is_unsigned ?
						TCTI_SIMD_ELEMENT_MOVE_USHLL :
						TCTI_SIMD_ELEMENT_MOVE_SSHLL,
						decoded.simd_element_move_op);
					KUNIT_EXPECT_TRUE(test, decoded.simd_fp);
				}
			}
		}
	}

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_UNSUPPORTED,
			tcti_decode_aarch64(0x0f00a400U).decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_DECODE_UNSUPPORTED,
			tcti_decode_aarch64(0x0f07a400U).decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_DECODE_UNSUPPORTED,
			tcti_decode_aarch64(0x0f40a400U).decode_class);
}

static void tcti_decode_recognizes_simd_and_16b(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;

	decoded = tcti_decode_aarch64(0x0e211c00U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_VECTOR_LOGICAL,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_LOGICAL_AND, decoded.logical_op);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 1U, decoded.rm);
	KUNIT_EXPECT_EQ(test, 8U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 8U, decoded.result_size);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);

	decoded = tcti_decode_aarch64(0x4e211c01U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_VECTOR_LOGICAL,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_LOGICAL_AND, decoded.logical_op);
	KUNIT_EXPECT_EQ(test, 1U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 1U, decoded.rm);
	KUNIT_EXPECT_EQ(test, 16U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 16U, decoded.result_size);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);

	decoded = tcti_decode_aarch64(0x2ea11c60U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_VECTOR_LOGICAL,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_LOGICAL_BIT, decoded.logical_op);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 3U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 1U, decoded.rm);
	KUNIT_EXPECT_EQ(test, 8U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 8U, decoded.result_size);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);

	decoded = tcti_decode_aarch64(0x0e211c01U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_VECTOR_LOGICAL,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_LOGICAL_AND, decoded.logical_op);
	KUNIT_EXPECT_EQ(test, 1U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 1U, decoded.rm);
	KUNIT_EXPECT_EQ(test, 8U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 8U, decoded.result_size);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);
}

static void tcti_decode_recognizes_simd_orr_4s_immediate(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;

	decoded = tcti_decode_aarch64(0x4f011600U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_MODIFIED_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_SIMD_MODIMM_ORR,
			decoded.simd_modified_immediate_op);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 16U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 16U, decoded.result_size);
	KUNIT_EXPECT_EQ(test, 0x0000003000000030ULL,
			decoded.logical_immediate);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);

	decoded = tcti_decode_aarch64(0x6f0717c2U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_MODIFIED_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_SIMD_MODIMM_BIC,
			decoded.simd_modified_immediate_op);
	KUNIT_EXPECT_EQ(test, 2U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 16U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 16U, decoded.result_size);
	KUNIT_EXPECT_EQ(test, 0x000000fe000000feULL,
			decoded.logical_immediate);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);

	decoded = tcti_decode_aarch64(0x6f0317c3U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_MODIFIED_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_SIMD_MODIMM_BIC,
			decoded.simd_modified_immediate_op);
	KUNIT_EXPECT_EQ(test, 3U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 16U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 16U, decoded.result_size);
	KUNIT_EXPECT_EQ(test, 0x0000007e0000007eULL,
			decoded.logical_immediate);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);

	decoded = tcti_decode_aarch64(0x6f0117c4U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_MODIFIED_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_SIMD_MODIMM_BIC,
			decoded.simd_modified_immediate_op);
	KUNIT_EXPECT_EQ(test, 4U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 16U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 16U, decoded.result_size);
	KUNIT_EXPECT_EQ(test, 0x0000003e0000003eULL,
			decoded.logical_immediate);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);

	decoded = tcti_decode_aarch64(0x6f0015c6U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_MODIFIED_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_SIMD_MODIMM_BIC,
			decoded.simd_modified_immediate_op);
	KUNIT_EXPECT_EQ(test, 6U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 16U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 16U, decoded.result_size);
	KUNIT_EXPECT_EQ(test, 0x0000000e0000000eULL,
			decoded.logical_immediate);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);

	decoded = tcti_decode_aarch64(0x6f0014c7U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_MODIFIED_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_SIMD_MODIMM_BIC,
			decoded.simd_modified_immediate_op);
	KUNIT_EXPECT_EQ(test, 7U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 16U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 16U, decoded.result_size);
	KUNIT_EXPECT_EQ(test, 0x0000000600000006ULL,
			decoded.logical_immediate);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);

	decoded = tcti_decode_aarch64(0x6f001442U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_MODIFIED_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_SIMD_MODIMM_BIC,
			decoded.simd_modified_immediate_op);
	KUNIT_EXPECT_EQ(test, 2U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 16U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 16U, decoded.result_size);
	KUNIT_EXPECT_EQ(test, 0x0000000200000002ULL,
			decoded.logical_immediate);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);
}

static void tcti_decode_recognizes_simd_cmeq_4s(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;

	decoded = tcti_decode_aarch64(0x6ea18c64U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_VECTOR_COMPARE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 4U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 3U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 1U, decoded.rm);
	KUNIT_EXPECT_EQ(test, 4U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 16U, decoded.result_size);
	KUNIT_EXPECT_EQ(test, TCTI_SIMD_COMPARE_CMEQ, decoded.simd_compare_op);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);

	decoded = tcti_decode_aarch64(0x2ea18c64U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_VECTOR_COMPARE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 4U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 3U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 1U, decoded.rm);
	KUNIT_EXPECT_EQ(test, 4U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 8U, decoded.result_size);
	KUNIT_EXPECT_EQ(test, TCTI_SIMD_COMPARE_CMEQ, decoded.simd_compare_op);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);

	decoded = tcti_decode_aarch64(0x2e288c84U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_VECTOR_COMPARE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 4U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 4U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 8U, decoded.rm);
	KUNIT_EXPECT_EQ(test, 1U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 8U, decoded.result_size);
	KUNIT_EXPECT_EQ(test, TCTI_SIMD_COMPARE_CMEQ, decoded.simd_compare_op);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);

	decoded = tcti_decode_aarch64(0x4ea09821U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_VECTOR_COMPARE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 1U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 1U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 4U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 16U, decoded.result_size);
	KUNIT_EXPECT_EQ(test, TCTI_SIMD_COMPARE_CMEQ, decoded.simd_compare_op);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);
	KUNIT_EXPECT_TRUE(test, decoded.immediate);
}

static void tcti_decode_recognizes_simd_ld1r_4s(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;

	decoded = tcti_decode_aarch64(0x4d40c900U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_LOAD_REPLICATE,
			decoded.decode_class);
	KUNIT_EXPECT_TRUE(test, decoded.load);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 8U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 4U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 16U, decoded.result_size);
}

static void tcti_decode_recognizes_simd_ext_16b(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;

	decoded = tcti_decode_aarch64(0x6e004001U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE,
			decoded.decode_class);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);
	KUNIT_EXPECT_EQ(test, 1U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rm);
	KUNIT_EXPECT_EQ(test, 8U, decoded.shift_amount);
	KUNIT_EXPECT_EQ(test, 16U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 16U, decoded.result_size);
	KUNIT_EXPECT_EQ(test, TCTI_SIMD_ELEMENT_MOVE_EXT,
			decoded.simd_element_move_op);
}

static void tcti_decode_recognizes_complete_simd_permute_family(
	struct kunit *test)
{
	static const struct {
		u8 encoding;
		enum tcti_simd_element_move_op operation;
	} operations[] = {
		{ 1, TCTI_SIMD_ELEMENT_MOVE_UZP1 },
		{ 5, TCTI_SIMD_ELEMENT_MOVE_UZP2 },
		{ 2, TCTI_SIMD_ELEMENT_MOVE_TRN1 },
		{ 6, TCTI_SIMD_ELEMENT_MOVE_TRN2 },
		{ 3, TCTI_SIMD_ELEMENT_MOVE_ZIP1 },
		{ 7, TCTI_SIMD_ELEMENT_MOVE_ZIP2 },
	};
	size_t operation_index;
	u8 q;
	u8 size;

	for (operation_index = 0; operation_index < ARRAY_SIZE(operations);
	     operation_index++) {
		for (q = 0; q <= 1; q++) {
			for (size = 0; size <= 3; size++) {
				u32 instruction;
				struct tcti_decoded_instruction decoded;

				if (!q && size == 3)
					continue;
				instruction = 0x0e000800U | ((u32)q << 30) |
					      ((u32)size << 22) | (2U << 16) |
					      ((u32)operations[operation_index].encoding << 12) |
					      (1U << 5);
				decoded = tcti_decode_aarch64(instruction);

				KUNIT_EXPECT_EQ(test,
					TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE,
					decoded.decode_class);
				KUNIT_EXPECT_EQ(test,
					operations[operation_index].operation,
					decoded.simd_element_move_op);
				KUNIT_EXPECT_EQ(test, 0U, decoded.rd);
				KUNIT_EXPECT_EQ(test, 1U, decoded.rn);
				KUNIT_EXPECT_EQ(test, 2U, decoded.rm);
				KUNIT_EXPECT_EQ(test, 1U << size,
					decoded.access_size);
				KUNIT_EXPECT_EQ(test, q ? 16U : 8U,
					decoded.result_size);
			}
		}
	}

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_UNSUPPORTED,
			tcti_decode_aarch64(0x0ec01800U).decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_DECODE_UNSUPPORTED,
			tcti_decode_aarch64(0x0e000800U).decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_DECODE_UNSUPPORTED,
			tcti_decode_aarch64(0x0e004800U).decode_class);
}

static void tcti_decode_recognizes_simd_uzp1_4h(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;

	decoded = tcti_decode_aarch64(0x0e4018a6U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 6U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 5U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rm);
	KUNIT_EXPECT_EQ(test, 2U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 8U, decoded.result_size);
	KUNIT_EXPECT_EQ(test, TCTI_SIMD_ELEMENT_MOVE_UZP1,
			decoded.simd_element_move_op);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);

	decoded = tcti_decode_aarch64(0x4e411841U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 1U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 2U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 1U, decoded.rm);
	KUNIT_EXPECT_EQ(test, 2U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 16U, decoded.result_size);
	KUNIT_EXPECT_EQ(test, TCTI_SIMD_ELEMENT_MOVE_UZP1,
			decoded.simd_element_move_op);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);
}

static void tcti_decode_recognizes_simd_umov_w_h0(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;

	decoded = tcti_decode_aarch64(0x0e023ccfU);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 15U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 6U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 2U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 4U, decoded.result_size);
	KUNIT_EXPECT_EQ(test, 0U, decoded.simd_source_index);
	KUNIT_EXPECT_EQ(test, TCTI_SIMD_ELEMENT_MOVE_UMOV,
			decoded.simd_element_move_op);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);
}

static void tcti_decode_recognizes_simd_umov_w_h1(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;

	decoded = tcti_decode_aarch64(0x0e063cafU);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 15U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 5U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 2U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 4U, decoded.result_size);
	KUNIT_EXPECT_EQ(test, 1U, decoded.simd_source_index);
	KUNIT_EXPECT_EQ(test, TCTI_SIMD_ELEMENT_MOVE_UMOV,
			decoded.simd_element_move_op);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);
}

static void tcti_decode_recognizes_simd_umov_x_d1(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;

	decoded = tcti_decode_aarch64(0x4e183c6fU);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 15U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 3U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 8U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 8U, decoded.result_size);
	KUNIT_EXPECT_EQ(test, 1U, decoded.simd_source_index);
	KUNIT_EXPECT_EQ(test, TCTI_SIMD_ELEMENT_MOVE_UMOV,
			decoded.simd_element_move_op);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);
}

static void tcti_decode_recognizes_simd_ins_gpr_s0(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;

	decoded = tcti_decode_aarch64(0x4e041d00U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 8U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 4U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 4U, decoded.result_size);
	KUNIT_EXPECT_EQ(test, 0U, decoded.simd_destination_index);
	KUNIT_EXPECT_EQ(test, TCTI_SIMD_ELEMENT_MOVE_INS_GPR,
			decoded.simd_element_move_op);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);
}

static void tcti_decode_recognizes_simd_umaxv_4s(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;

	decoded = tcti_decode_aarch64(0x6eb0a885U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_VECTOR_REDUCTION,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 5U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 4U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 4U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 4U, decoded.result_size);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);

	decoded = tcti_decode_aarch64(0x6e70a885U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_UNSUPPORTED, decoded.decode_class);
}

static void tcti_decode_recognizes_simd_umaxv_4h(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;

	decoded = tcti_decode_aarch64(0x2e70a885U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_VECTOR_REDUCTION,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_SIMD_REDUCTION_UMAXV,
			decoded.simd_reduction_op);
	KUNIT_EXPECT_EQ(test, 5U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 4U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 2U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 2U, decoded.result_size);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);

	decoded = tcti_decode_aarch64(0x6e70a885U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_UNSUPPORTED, decoded.decode_class);
}

static void tcti_decode_recognizes_simd_addv_4s(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;

	decoded = tcti_decode_aarch64(0x4eb1b800U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_VECTOR_REDUCTION,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_SIMD_REDUCTION_ADDV,
			decoded.simd_reduction_op);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 4U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 4U, decoded.result_size);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);

	decoded = tcti_decode_aarch64(0x0eb1b800U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_UNSUPPORTED, decoded.decode_class);
}

static void tcti_decode_recognizes_simd_addp_d_2d(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;

	decoded = tcti_decode_aarch64(0x5ef1b800U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_VECTOR_REDUCTION,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_SIMD_REDUCTION_ADDP,
			decoded.simd_reduction_op);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 8U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 8U, decoded.result_size);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);
}

static void tcti_decode_recognizes_simd_add_2d(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;

	decoded = tcti_decode_aarch64(0x4ee18441U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_VECTOR_ARITHMETIC,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_SIMD_ARITH_ADD, decoded.simd_arithmetic_op);
	KUNIT_EXPECT_EQ(test, 1U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 2U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 1U, decoded.rm);
	KUNIT_EXPECT_EQ(test, 8U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 16U, decoded.result_size);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);

	decoded = tcti_decode_aarch64(0x4ea08440U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_VECTOR_ARITHMETIC,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_SIMD_ARITH_ADD, decoded.simd_arithmetic_op);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 2U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rm);
	KUNIT_EXPECT_EQ(test, 4U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 16U, decoded.result_size);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);

	decoded = tcti_decode_aarch64(0x6f391420U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_VECTOR_ARITHMETIC,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_SIMD_ARITH_USRA,
			decoded.simd_arithmetic_op);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 1U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 4U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 16U, decoded.result_size);
	KUNIT_EXPECT_EQ(test, 7U, decoded.shift_amount);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);
}

static void tcti_decode_recognizes_simd_sub_2d(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;

	decoded = tcti_decode_aarch64(0x6ee08420U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_VECTOR_ARITHMETIC,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_SIMD_ARITH_SUB,
			decoded.simd_arithmetic_op);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 1U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rm);
	KUNIT_EXPECT_EQ(test, 8U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 16U, decoded.result_size);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);
}

static void tcti_decode_recognizes_simd_fneg_2d(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;

	decoded = tcti_decode_aarch64(0x6ee0f800U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_VECTOR_ARITHMETIC,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_SIMD_ARITH_FNEG,
			decoded.simd_arithmetic_op);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 8U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 16U, decoded.result_size);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);
}

static void tcti_decode_recognizes_simd_cmhi_2d(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;

	decoded = tcti_decode_aarch64(0x6ee23405U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_VECTOR_COMPARE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_SIMD_COMPARE_CMHI, decoded.simd_compare_op);
	KUNIT_EXPECT_EQ(test, 5U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 2U, decoded.rm);
	KUNIT_EXPECT_EQ(test, 8U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 16U, decoded.result_size);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);
}

static void tcti_decode_recognizes_simd_ushl_4s(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;

	decoded = tcti_decode_aarch64(0x6f3f0423U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_VECTOR_ARITHMETIC,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_SIMD_ARITH_USHR,
			decoded.simd_arithmetic_op);
	KUNIT_EXPECT_EQ(test, 3U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 1U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 4U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 16U, decoded.result_size);
	KUNIT_EXPECT_EQ(test, 1U, decoded.shift_amount);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);

	decoded = tcti_decode_aarch64(0x6ea04421U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_VECTOR_ARITHMETIC,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_SIMD_ARITH_USHL,
			decoded.simd_arithmetic_op);
	KUNIT_EXPECT_EQ(test, 1U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 1U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rm);
	KUNIT_EXPECT_EQ(test, 4U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 16U, decoded.result_size);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);

	decoded = tcti_decode_aarch64(0x6ee04421U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_UNSUPPORTED, decoded.decode_class);
}

static void tcti_decode_recognizes_fmov_w_s(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;

	decoded = tcti_decode_aarch64(0x1e2600abU);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_FP_SCALAR_MOVE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 11U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 5U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 4U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 4U, decoded.result_size);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);
	KUNIT_EXPECT_EQ(test, TCTI_FP_MOVE_SIMD_TO_GPR, decoded.fp_move_op);

	decoded = tcti_decode_aarch64(0x1e270100U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_FP_SCALAR_MOVE, decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 8U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 4U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 4U, decoded.result_size);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);
	KUNIT_EXPECT_EQ(test, TCTI_FP_MOVE_GPR_TO_SIMD, decoded.fp_move_op);

	decoded = tcti_decode_aarch64(0x1e604001U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_FP_SCALAR_MOVE, decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 1U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 8U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, TCTI_FP_MOVE_REGISTER, decoded.fp_move_op);

	decoded = tcti_decode_aarch64(0x9e66003cU);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_FP_SCALAR_MOVE, decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 28U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 1U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 8U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, TCTI_FP_MOVE_SIMD_TO_GPR, decoded.fp_move_op);
}

static void tcti_decode_recognizes_fp_scalar_runtime_operations(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;

	decoded = tcti_decode_aarch64(0x1e211800U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_FP_SCALAR_2SOURCE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_FP2_FDIV, decoded.fp2_op);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 1U, decoded.rm);
	KUNIT_EXPECT_EQ(test, 4U, decoded.access_size);

	decoded = tcti_decode_aarch64(0x1e212800U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_FP_SCALAR_2SOURCE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_FP2_FADD, decoded.fp2_op);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 1U, decoded.rm);
	KUNIT_EXPECT_EQ(test, 4U, decoded.access_size);

	decoded = tcti_decode_aarch64(0x1e202908U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_FP_SCALAR_2SOURCE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_FP2_FADD, decoded.fp2_op);
	KUNIT_EXPECT_EQ(test, 8U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 8U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rm);
	KUNIT_EXPECT_EQ(test, 4U, decoded.access_size);

	decoded = tcti_decode_aarch64(0x1e213800U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_FP_SCALAR_2SOURCE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_FP2_FSUB, decoded.fp2_op);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 1U, decoded.rm);
	KUNIT_EXPECT_EQ(test, 4U, decoded.access_size);

	decoded = tcti_decode_aarch64(0x1e620802U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_FP_SCALAR_2SOURCE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_FP2_FMUL, decoded.fp2_op);
	KUNIT_EXPECT_EQ(test, 2U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 2U, decoded.rm);
	KUNIT_EXPECT_EQ(test, 8U, decoded.access_size);

	decoded = tcti_decode_aarch64(0x1e290900U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_FP_SCALAR_2SOURCE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_FP2_FMUL, decoded.fp2_op);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 8U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 9U, decoded.rm);
	KUNIT_EXPECT_EQ(test, 4U, decoded.access_size);

	decoded = tcti_decode_aarch64(0x1e603c40U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_FP_CONDITIONAL_SELECT,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 2U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rm);
	KUNIT_EXPECT_EQ(test, 3U, decoded.condition);
	KUNIT_EXPECT_EQ(test, 8U, decoded.access_size);

	decoded = tcti_decode_aarch64(0x1f501cc7U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_FP_SCALAR_3SOURCE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 7U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 6U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 16U, decoded.rm);
	KUNIT_EXPECT_EQ(test, 7U, decoded.ra);
	KUNIT_EXPECT_EQ(test, 8U, decoded.access_size);

	decoded = tcti_decode_aarch64(0x1f509623U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_FP_SCALAR_3SOURCE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 3U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 17U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 16U, decoded.rm);
	KUNIT_EXPECT_EQ(test, 5U, decoded.ra);
	KUNIT_EXPECT_TRUE(test, decoded.subtract);
	KUNIT_EXPECT_EQ(test, 8U, decoded.access_size);

	decoded = tcti_decode_aarch64(0x1e602000U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_FP_SCALAR_COMPARE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rm);
	KUNIT_EXPECT_EQ(test, 8U, decoded.access_size);

	decoded = tcti_decode_aarch64(0x1e602008U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_FP_SCALAR_COMPARE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rn);
	KUNIT_EXPECT_TRUE(test, decoded.immediate);
	KUNIT_EXPECT_EQ(test, 8U, decoded.access_size);

	decoded = tcti_decode_aarch64(0x1e60c020U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_FP_SCALAR_1SOURCE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_FP1_FABS, decoded.fp1_op);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 1U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 8U, decoded.access_size);

	decoded = tcti_decode_aarch64(0x1e214100U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_FP_SCALAR_1SOURCE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_FP1_FNEG, decoded.fp1_op);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 8U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 4U, decoded.access_size);

	decoded = tcti_decode_aarch64(0x1e201001U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_MODIFIED_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 1U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 0x40000000ULL, decoded.logical_immediate);
	KUNIT_EXPECT_EQ(test, 4U, decoded.access_size);

	decoded = tcti_decode_aarch64(0x1e2e1001U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_MODIFIED_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 1U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 0x3f800000ULL, decoded.logical_immediate);
	KUNIT_EXPECT_EQ(test, 4U, decoded.access_size);

	decoded = tcti_decode_aarch64(0x1e649000U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_MODIFIED_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 0x4024000000000000ULL,
			decoded.logical_immediate);
	KUNIT_EXPECT_EQ(test, 8U, decoded.access_size);

	decoded = tcti_decode_aarch64(0x1e611006U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_MODIFIED_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 6U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 0x4008000000000000ULL,
			decoded.logical_immediate);
	KUNIT_EXPECT_EQ(test, 8U, decoded.access_size);

	decoded = tcti_decode_aarch64(0x1e711004U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_MODIFIED_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 4U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 0xc008000000000000ULL,
			decoded.logical_immediate);
	KUNIT_EXPECT_EQ(test, 8U, decoded.access_size);

	decoded = tcti_decode_aarch64(0x1e701004U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_MODIFIED_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 4U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 0xc000000000000000ULL,
			decoded.logical_immediate);
	KUNIT_EXPECT_EQ(test, 8U, decoded.access_size);

	decoded = tcti_decode_aarch64(0x1e7e1000U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_MODIFIED_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 0xbff0000000000000ULL,
			decoded.logical_immediate);
	KUNIT_EXPECT_EQ(test, 8U, decoded.access_size);

	decoded = tcti_decode_aarch64(0x1e7a1002U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_MODIFIED_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 2U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 0xbfd0000000000000ULL,
			decoded.logical_immediate);
	KUNIT_EXPECT_EQ(test, 8U, decoded.access_size);

	decoded = tcti_decode_aarch64(0xbc1fc3a0U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_LOAD_STORE_SIGNED_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_FALSE(test, decoded.load);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rt);
	KUNIT_EXPECT_EQ(test, 29U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 4U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, -4LL, decoded.memory_offset);

	decoded = tcti_decode_aarch64(0x1e22c000U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_FP_SCALAR_1SOURCE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_FP1_FCVT, decoded.fp1_op);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 4U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 8U, decoded.result_size);

	decoded = tcti_decode_aarch64(0x1e620101U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_FP_INT_CONVERT,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_FP_INT_SCVTF, decoded.fp_int_op);
	KUNIT_EXPECT_EQ(test, 1U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 8U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 4U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 8U, decoded.result_size);

	decoded = tcti_decode_aarch64(0x1e230101U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_FP_INT_CONVERT,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_FP_INT_UCVTF, decoded.fp_int_op);
	KUNIT_EXPECT_EQ(test, 1U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 8U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 4U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 4U, decoded.result_size);

	decoded = tcti_decode_aarch64(0x9e230280U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_FP_INT_CONVERT,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_FP_INT_UCVTF, decoded.fp_int_op);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 20U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 8U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 4U, decoded.result_size);

	decoded = tcti_decode_aarch64(0x1e220101U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_FP_INT_CONVERT,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_FP_INT_SCVTF, decoded.fp_int_op);
	KUNIT_EXPECT_EQ(test, 1U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 8U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 4U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 4U, decoded.result_size);

	decoded = tcti_decode_aarch64(0x9e630161U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_FP_INT_CONVERT,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_FP_INT_UCVTF, decoded.fp_int_op);
	KUNIT_EXPECT_EQ(test, 1U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 11U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 8U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 8U, decoded.result_size);

	decoded = tcti_decode_aarch64(0x9e390014U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_FP_INT_CONVERT,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_FP_INT_FCVTZU, decoded.fp_int_op);
	KUNIT_EXPECT_EQ(test, 20U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 4U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 8U, decoded.result_size);

	decoded = tcti_decode_aarch64(0x1e78001bU);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_FP_INT_CONVERT,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_FP_INT_FCVTZS, decoded.fp_int_op);
	KUNIT_EXPECT_EQ(test, 27U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 8U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 4U, decoded.result_size);

	decoded = tcti_decode_aarch64(0x9e790016U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_FP_INT_CONVERT,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_FP_INT_FCVTZU, decoded.fp_int_op);
	KUNIT_EXPECT_EQ(test, 22U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 8U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 8U, decoded.result_size);

	decoded = tcti_decode_aarch64(0x9e59f00bU);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_FP_INT_CONVERT,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_FP_INT_FCVTZU_FIXED, decoded.fp_int_op);
	KUNIT_EXPECT_EQ(test, 11U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 8U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 8U, decoded.result_size);
	KUNIT_EXPECT_EQ(test, 4U, decoded.shift_amount);

	decoded = tcti_decode_aarch64(0x7ee1b801U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_FP_INT_CONVERT,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_FP_INT_FCVTZU_SIMD, decoded.fp_int_op);
	KUNIT_EXPECT_EQ(test, 1U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 8U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 8U, decoded.result_size);

	decoded = tcti_decode_aarch64(0x7e61d821U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_FP_INT_CONVERT,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_FP_INT_UCVTF_SIMD, decoded.fp_int_op);
	KUNIT_EXPECT_EQ(test, 1U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 1U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 8U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 8U, decoded.result_size);

	decoded = tcti_decode_aarch64(0x6e61d800U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_FP_INT_CONVERT,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_FP_INT_UCVTF_SIMD, decoded.fp_int_op);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 8U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 16U, decoded.result_size);

	decoded = tcti_decode_aarch64(0x6e62dc00U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_FP_SCALAR_2SOURCE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_FP2_FMUL, decoded.fp2_op);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 2U, decoded.rm);
	KUNIT_EXPECT_EQ(test, 8U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 16U, decoded.result_size);
}

static void tcti_decode_recognizes_fcvtzu_w_d(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;

	decoded = tcti_decode_aarch64(0x1e790008U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_FP_INT_CONVERT,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_FP_INT_FCVTZU, decoded.fp_int_op);
	KUNIT_EXPECT_EQ(test, 8U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 8U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 4U, decoded.result_size);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);
}

static void tcti_decode_recognizes_scvtf_d_d(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;

	decoded = tcti_decode_aarch64(0x5e61d800U);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_FP_INT_CONVERT,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_FP_INT_SCVTF_SIMD, decoded.fp_int_op);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 8U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 8U, decoded.result_size);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);
}

static void tcti_decode_recognizes_add_sub_with_carry(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;

	decoded = tcti_decode_aarch64(0xfa03001fU);

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_ADD_SUB_WITH_CARRY,
			decoded.decode_class);
	KUNIT_EXPECT_TRUE(test, decoded.is_64bit);
	KUNIT_EXPECT_TRUE(test, decoded.subtract);
	KUNIT_EXPECT_TRUE(test, decoded.set_flags);
	KUNIT_EXPECT_EQ(test, 31U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 0U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 3U, decoded.rm);
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

static void tcti_syscall_handoff_uses_guest_x8_and_advances_pc(struct kunit *test)
{
	struct pt_regs regs = { 0 };

	regs.regs[0] = 42;
	regs.regs[8] = 93;
	regs.orig_x0 = 0xffff;
	regs.syscallno = NO_SYSCALL;
	regs.pc = 0x210128;

	tcti_prepare_syscall_handoff(&regs);

	KUNIT_EXPECT_EQ(test, 42ULL, regs.orig_x0);
	KUNIT_EXPECT_EQ(test, 93, regs.syscallno);
	KUNIT_EXPECT_EQ(test, 42ULL, regs.regs[0]);
	KUNIT_EXPECT_EQ(test, 93ULL, regs.regs[8]);
	KUNIT_EXPECT_EQ(test, 0x21012cULL, regs.pc);
}

static unsigned long tcti_test_map_instructions(struct kunit *test,
					 const u32 *instructions,
					 size_t instruction_count)
{
	unsigned long mapped;
	int ret;

	KUNIT_ASSERT_NOT_NULL(test, current->mm);
	mapped = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));
	ret = tcti_write_user_data(current->mm, mapped, instructions,
				   instruction_count * sizeof(*instructions));
	if (ret) {
		vm_munmap(mapped, PAGE_SIZE);
		KUNIT_FAIL(test, "could not write TCTI test instructions: %d", ret);
		return 0;
	}
	ret = sys_mprotect(mapped, PAGE_SIZE, PROT_READ | PROT_EXEC);
	if (ret) {
		vm_munmap(mapped, PAGE_SIZE);
		KUNIT_FAIL(test, "could not protect TCTI test instructions: %d",
			   ret);
		return 0;
	}

	return mapped;
}

static void tcti_resume_user_reports_syscall_and_register_state(struct kunit *test)
{
	static const u32 instructions[] = {
		0xd2800540U, /* mov x0, #42 */
		0xd4000001U, /* svc #0 */
	};
	struct tcti_result result;
	struct pt_regs regs = { 0 };
	unsigned long mapped;
	int ret;

	mapped = tcti_test_map_instructions(test, instructions,
					    ARRAY_SIZE(instructions));
	KUNIT_ASSERT_NE(test, 0UL, mapped);
	regs.pc = mapped;
	regs.sp = STACK_TOP - 16;
	regs.pstate = PSR_MODE_EL0t;
	regs.syscallno = NO_SYSCALL;
	regs.regs[8] = __NR_getpid;

	result = tcti_resume_user(current, &regs, current->mm);

	KUNIT_EXPECT_EQ(test, TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, 0L, result.status);
	KUNIT_EXPECT_EQ(test, mapped + sizeof(u32), result.pc);
	KUNIT_EXPECT_EQ(test, instructions[1], result.instruction);
	KUNIT_EXPECT_EQ(test, 42ULL, regs.regs[0]);
	KUNIT_EXPECT_EQ(test, __NR_getpid, (int)regs.regs[8]);
	KUNIT_EXPECT_EQ(test, mapped + sizeof(u32), regs.pc);

	tcti_prepare_syscall_handoff(&regs);
	KUNIT_EXPECT_EQ(test, 42ULL, regs.orig_x0);
	KUNIT_EXPECT_EQ(test, __NR_getpid, regs.syscallno);
	KUNIT_EXPECT_EQ(test, mapped + 2 * sizeof(u32), regs.pc);

	ret = vm_munmap(mapped, PAGE_SIZE);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void tcti_resume_user_reports_fetch_fault(struct kunit *test)
{
	struct tcti_result result;
	struct pt_regs regs = { 0 };

	KUNIT_ASSERT_NOT_NULL(test, current->mm);
	regs.pc = 0;
	regs.sp = STACK_TOP - 16;
	regs.pstate = PSR_MODE_EL0t;
	regs.syscallno = NO_SYSCALL;

	result = tcti_resume_user(current, &regs, current->mm);

	KUNIT_EXPECT_EQ(test, TCTI_EXIT_USER_FAULT, result.reason);
	KUNIT_EXPECT_EQ(test, -EFAULT, result.status);
	KUNIT_EXPECT_EQ(test, 0UL, result.fault_address);
	KUNIT_EXPECT_EQ(test, TCTI_ACCESS_FETCH, result.fault_access);
	KUNIT_EXPECT_EQ(test, 0UL, result.pc);
	KUNIT_EXPECT_EQ(test, 0U, result.instruction);
}

static void tcti_resume_user_reports_read_fault(struct kunit *test)
{
	static const u32 instruction = 0xf9400020U; /* ldr x0, [x1] */
	struct tcti_result result;
	struct pt_regs regs = { 0 };
	unsigned long mapped;
	int ret;

	mapped = tcti_test_map_instructions(test, &instruction, 1);
	KUNIT_ASSERT_NE(test, 0UL, mapped);
	regs.pc = mapped;
	regs.sp = STACK_TOP - 16;
	regs.pstate = PSR_MODE_EL0t;
	regs.syscallno = NO_SYSCALL;
	regs.regs[1] = 0;

	result = tcti_resume_user(current, &regs, current->mm);

	KUNIT_EXPECT_EQ(test, TCTI_EXIT_USER_FAULT, result.reason);
	KUNIT_EXPECT_EQ(test, -EFAULT, result.status);
	KUNIT_EXPECT_EQ(test, 0UL, result.fault_address);
	KUNIT_EXPECT_EQ(test, TCTI_ACCESS_READ, result.fault_access);
	KUNIT_EXPECT_EQ(test, mapped, result.pc);
	KUNIT_EXPECT_EQ(test, instruction, result.instruction);
	KUNIT_EXPECT_EQ(test, mapped, regs.pc);

	ret = vm_munmap(mapped, PAGE_SIZE);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void tcti_resume_user_reports_write_fault(struct kunit *test)
{
	static const u32 instruction = 0xf9000020U; /* str x0, [x1] */
	struct tcti_result result;
	struct pt_regs regs = { 0 };
	unsigned long mapped;
	int ret;

	mapped = tcti_test_map_instructions(test, &instruction, 1);
	KUNIT_ASSERT_NE(test, 0UL, mapped);
	regs.pc = mapped;
	regs.sp = STACK_TOP - 16;
	regs.pstate = PSR_MODE_EL0t;
	regs.syscallno = NO_SYSCALL;
	regs.regs[0] = 42;
	regs.regs[1] = 0;

	result = tcti_resume_user(current, &regs, current->mm);

	KUNIT_EXPECT_EQ(test, TCTI_EXIT_USER_FAULT, result.reason);
	KUNIT_EXPECT_EQ(test, -EFAULT, result.status);
	KUNIT_EXPECT_EQ(test, 0UL, result.fault_address);
	KUNIT_EXPECT_EQ(test, TCTI_ACCESS_WRITE, result.fault_access);
	KUNIT_EXPECT_EQ(test, mapped, result.pc);
	KUNIT_EXPECT_EQ(test, instruction, result.instruction);
	KUNIT_EXPECT_EQ(test, mapped, regs.pc);

	ret = vm_munmap(mapped, PAGE_SIZE);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void tcti_resume_user_reports_unsupported_instruction(struct kunit *test)
{
	static const u32 instruction = 0xffffffffU;
	struct tcti_result result;
	struct pt_regs regs = { 0 };
	unsigned long mapped;
	int ret;

	mapped = tcti_test_map_instructions(test, &instruction, 1);
	KUNIT_ASSERT_NE(test, 0UL, mapped);
	regs.pc = mapped;
	regs.sp = STACK_TOP - 16;
	regs.pstate = PSR_MODE_EL0t;
	regs.syscallno = NO_SYSCALL;

	result = tcti_resume_user(current, &regs, current->mm);

	KUNIT_EXPECT_EQ(test, TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
			result.reason);
	KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, result.status);
	KUNIT_EXPECT_EQ(test, mapped, result.pc);
	KUNIT_EXPECT_EQ(test, instruction, result.instruction);
	KUNIT_EXPECT_EQ(test, mapped, regs.pc);

	ret = vm_munmap(mapped, PAGE_SIZE);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void tcti_resume_user_executes_branch_sequence(struct kunit *test)
{
	static const u32 instructions[] = {
		0xd2800000U, /* mov x0, #0 */
		0xb4000060U, /* cbz x0, .+12 */
		0xd2800020U, /* mov x0, #1 */
		0x14000002U, /* b .+8 */
		0xd2800540U, /* mov x0, #42 */
		0xd4000001U, /* svc #0 */
	};
	struct tcti_result result;
	struct pt_regs regs = { 0 };
	unsigned long mapped;
	int ret;

	mapped = tcti_test_map_instructions(test, instructions,
					    ARRAY_SIZE(instructions));
	KUNIT_ASSERT_NE(test, 0UL, mapped);
	regs.pc = mapped;
	regs.sp = STACK_TOP - 16;
	regs.pstate = PSR_MODE_EL0t;
	regs.syscallno = NO_SYSCALL;

	result = tcti_resume_user(current, &regs, current->mm);

	KUNIT_EXPECT_EQ(test, TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, 0L, result.status);
	KUNIT_EXPECT_EQ(test, mapped + 5 * sizeof(u32), result.pc);
	KUNIT_EXPECT_EQ(test, instructions[5], result.instruction);
	KUNIT_EXPECT_EQ(test, 42ULL, regs.regs[0]);
	KUNIT_EXPECT_EQ(test, mapped + 5 * sizeof(u32), regs.pc);

	ret = vm_munmap(mapped, PAGE_SIZE);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

#if defined(ORLIX_APP_HOSTED_BOOT)
static void tcti_resume_user_updates_tls_register_state(struct kunit *test)
{
	static const u32 instructions[] = {
		0xd51bd041U, /* msr tpidr_el0, x1 */
		0xd53bd040U, /* mrs x0, tpidr_el0 */
		0xd4000001U, /* svc #0 */
	};
	struct tcti_result result;
	struct pt_regs regs = { 0 };
	unsigned long old_tls = current->thread.user_tls;
	unsigned long mapped;
	int ret;

	mapped = tcti_test_map_instructions(test, instructions,
					    ARRAY_SIZE(instructions));
	KUNIT_ASSERT_NE(test, 0UL, mapped);
	regs.pc = mapped;
	regs.sp = STACK_TOP - 16;
	regs.pstate = PSR_MODE_EL0t;
	regs.syscallno = NO_SYSCALL;
	regs.regs[1] = mapped;

	result = tcti_resume_user(current, &regs, current->mm);

	KUNIT_EXPECT_EQ(test, TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, 0L, result.status);
	KUNIT_EXPECT_EQ(test, mapped + 2 * sizeof(u32), result.pc);
	KUNIT_EXPECT_EQ(test, instructions[2], result.instruction);
	KUNIT_EXPECT_EQ(test, mapped, regs.regs[0]);
	KUNIT_EXPECT_EQ(test, mapped, current->thread.user_tls);

	current->thread.user_tls = old_tls;
	ret = vm_munmap(mapped, PAGE_SIZE);
	KUNIT_EXPECT_EQ(test, 0, ret);
}
#endif

static void tcti_resume_user_updates_guest_memory(struct kunit *test)
{
	static const u32 instructions[] = {
		0xd2800540U, /* mov x0, #42 */
		0xf9000020U, /* str x0, [x1] */
		0xf9400022U, /* ldr x2, [x1] */
		0xd4000001U, /* svc #0 */
	};
	struct tcti_result result;
	struct pt_regs regs = { 0 };
	unsigned long instructions_mapped;
	unsigned long data_mapped;
	u64 observed = 0;
	int ret;

	instructions_mapped = tcti_test_map_instructions(
		test, instructions, ARRAY_SIZE(instructions));
	KUNIT_ASSERT_NE(test, 0UL, instructions_mapped);
	data_mapped = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
				       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(data_mapped));

	regs.pc = instructions_mapped;
	regs.sp = STACK_TOP - 16;
	regs.pstate = PSR_MODE_EL0t;
	regs.syscallno = NO_SYSCALL;
	regs.regs[1] = data_mapped;

	result = tcti_resume_user(current, &regs, current->mm);
	ret = tcti_read_user_data(current->mm, data_mapped, &observed,
				  sizeof(observed));

	KUNIT_EXPECT_EQ(test, TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, 0L, result.status);
	KUNIT_EXPECT_EQ(test, instructions_mapped + 3 * sizeof(u32), result.pc);
	KUNIT_EXPECT_EQ(test, instructions[3], result.instruction);
	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 42ULL, observed);
	KUNIT_EXPECT_EQ(test, 42ULL, regs.regs[2]);

	ret = vm_munmap(data_mapped, PAGE_SIZE);
	KUNIT_EXPECT_EQ(test, 0, ret);
	ret = vm_munmap(instructions_mapped, PAGE_SIZE);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void tcti_successful_execve_return_restores_el0_pstate(struct kunit *test)
{
	struct pt_regs regs = { 0 };
#if defined(ORLIX_APP_HOSTED_BOOT)
	unsigned long old_tls = current->thread.user_tls;
#endif

	regs.regs[0] = 0;
	regs.pc = 0x51418;
	regs.sp = STACK_TOP - 16;
	regs.pstate = PSR_MODE_EL1h;
	regs.syscallno = 221;
#if defined(ORLIX_APP_HOSTED_BOOT)
	current->thread.user_tls = STACK_TOP - 32;
#endif

	KUNIT_EXPECT_TRUE(test, tcti_prepare_successful_execve_return(&regs));
	KUNIT_EXPECT_TRUE(test, user_mode(&regs));
	KUNIT_EXPECT_EQ(test, NO_SYSCALL, regs.syscallno);
	KUNIT_EXPECT_EQ(test, 0x51418ULL, regs.pc);
	KUNIT_EXPECT_EQ(test, STACK_TOP - 16, regs.sp);
#if defined(ORLIX_APP_HOSTED_BOOT)
	KUNIT_EXPECT_EQ(test, STACK_TOP - 32, current->thread.user_tls);
	current->thread.user_tls = old_tls;
#endif
}

static void tcti_static_pie_initial_tls_uses_pt_tls(struct kunit *test)
{
	Elf64_Phdr phdr = {
		.p_type = PT_TLS,
		.p_vaddr = 0x270cc,
		.p_memsz = 0x4,
	};
	unsigned long initial_tls = 0;

	KUNIT_EXPECT_TRUE(test, tcti_static_pie_initial_tls(0x100000000,
							    &phdr,
							    &initial_tls));
	KUNIT_EXPECT_EQ(test, 0x100027144UL, initial_tls);

	phdr.p_type = PT_DYNAMIC;
	KUNIT_EXPECT_FALSE(test, tcti_static_pie_initial_tls(0x100000000,
							     &phdr,
							     &initial_tls));

	phdr.p_type = PT_TLS;
	phdr.p_memsz = 0;
	KUNIT_EXPECT_FALSE(test, tcti_static_pie_initial_tls(0x100000000,
							     &phdr,
							     &initial_tls));
}

static void tcti_static_pie_relocation_count_is_bounded(struct kunit *test)
{
	KUNIT_EXPECT_TRUE(test,
			  tcti_static_pie_relocation_count_valid(8957));
	KUNIT_EXPECT_TRUE(test,
			  tcti_static_pie_relocation_count_valid(16384));
	KUNIT_EXPECT_FALSE(test,
			   tcti_static_pie_relocation_count_valid(16385));
}

#if defined(ORLIX_APP_HOSTED_BOOT)
static void tcti_prepare_user_entry_accepts_pt_tls_alignment(struct kunit *test)
{
	unsigned long old_tls = current->thread.user_tls;
	unsigned long initial_tls = ORLIX_HOSTED_USER_BASE + PAGE_SIZE + 4;

	KUNIT_ASSERT_LT(test, initial_tls, ORLIX_HOSTED_STACK_TOP);
	KUNIT_EXPECT_EQ(test, initial_tls,
			orlix_hosted_prepare_user_entry(initial_tls));
	KUNIT_EXPECT_EQ(test, initial_tls, current->thread.user_tls);

	current->thread.user_tls = old_tls;
}
#endif

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

static void tcti_gadget_program_executes_multiple_decoded_instructions(struct kunit *test)
{
	struct tcti_gadget_word program[TCTI_PROGRAM_WORDS_FOR_INSTRUCTIONS(2)];
	struct tcti_decoded_instruction decoded;
	unsigned long fault_address = 0;
	struct pt_regs regs = {};
	size_t word_count = 0;
	int ret;

	decoded = tcti_decode_aarch64(0xd503201fU);
	ret = tcti_append_decoded_instruction(&decoded, program,
					      ARRAY_SIZE(program),
					      &word_count);
	KUNIT_ASSERT_EQ(test, 0, ret);
	ret = tcti_append_decoded_instruction(&decoded, program,
					      ARRAY_SIZE(program),
					      &word_count);
	KUNIT_ASSERT_EQ(test, 0, ret);

	regs.pc = 0x8000;
	ret = tcti_execute_gadget_program(current->mm, &regs, program,
					  word_count, &fault_address);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x8008ULL, regs.pc);
}

static void tcti_gadget_program_executes_branch_register(struct kunit *test)
{
	struct tcti_gadget_word program[TCTI_SINGLE_INSTRUCTION_PROGRAM_WORDS];
	struct tcti_decoded_instruction decoded;
	unsigned long fault_address = 0;
	struct pt_regs regs = {};
	size_t word_count = 0;
	int ret;

	decoded = tcti_decode_aarch64(0xd61f00a0U);
	ret = tcti_lower_decoded_instruction(&decoded, program,
					     ARRAY_SIZE(program),
					     &word_count);
	KUNIT_ASSERT_EQ(test, 0, ret);

	regs.regs[5] = 0x2427c;
	regs.regs[30] = 0xfeedface;
	regs.pc = 0x242f8;

	ret = tcti_execute_gadget_program(current->mm, &regs, program,
					  word_count, &fault_address);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0xfeedfaceULL, regs.regs[30]);
	KUNIT_EXPECT_EQ(test, 0x2427cULL, regs.pc);
}

static void tcti_gadget_program_preserves_flags_between_subs_and_branch(struct kunit *test)
{
	struct tcti_gadget_word program[TCTI_PROGRAM_WORDS_FOR_INSTRUCTIONS(3)];
	unsigned int instructions[] = {
		0xf1000442U, /* subs x2, x2, #1 */
		0x91000421U, /* add x1, x1, #1 */
		0x54000620U, /* b.eq 0x24348 */
	};
	struct pt_regs regs = {};
	unsigned long fault_address = 0;
	size_t word_count = 0;
	int i;
	int ret;

	regs.pc = 0x2427c;
	regs.regs[1] = 0x1000;
	regs.regs[2] = 1;

	for (i = 0; i < ARRAY_SIZE(instructions); i++) {
		struct tcti_decoded_instruction decoded;

		decoded = tcti_decode_aarch64(instructions[i]);
		ret = tcti_append_decoded_instruction(&decoded, program,
						      ARRAY_SIZE(program),
						      &word_count);
		KUNIT_ASSERT_EQ(test, 0, ret);
	}

	ret = tcti_execute_gadget_program(NULL, &regs, program, word_count,
					  &fault_address);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x1001ULL, regs.regs[1]);
	KUNIT_EXPECT_EQ(test, 0ULL, regs.regs[2]);
	KUNIT_EXPECT_NE(test, 0ULL, regs.pstate & PSR_Z_BIT);
	KUNIT_EXPECT_EQ(test, 0x24348ULL, regs.pc);
}

static void tcti_block_cache_is_bounded(struct kunit *test)
{
	struct tcti_gadget_word program[TCTI_SINGLE_INSTRUCTION_PROGRAM_WORDS];
	struct tcti_decoded_instruction decoded;
	struct mm_struct *mm = (struct mm_struct *)0x2000UL;
	struct tcti_block *block;
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
	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, TCTI_BLOCK_CACHE_MAX_BLOCKS,
			tcti_block_cache_count_for_tests());

	block = tcti_block_cache_lookup(mm, 0x100000, generation);
	KUNIT_EXPECT_NULL(test, block);
	if (block)
		tcti_block_put(block);
	block = tcti_block_cache_lookup(mm, 0x200000, generation);
	KUNIT_EXPECT_NOT_NULL(test, block);
	if (block)
		tcti_block_put(block);

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

static void tcti_syscall_mapping_changes_include_brk(struct kunit *test)
{
	KUNIT_EXPECT_TRUE(test,
			  tcti_syscall_changes_user_mappings_for_tests(__NR_brk));
	KUNIT_EXPECT_TRUE(test,
			  tcti_syscall_changes_user_mappings_for_tests(__NR_mmap));
	KUNIT_EXPECT_TRUE(test,
			  tcti_syscall_changes_user_mappings_for_tests(__NR_mprotect));
	KUNIT_EXPECT_TRUE(test,
			  tcti_syscall_changes_user_mappings_for_tests(__NR_munmap));
	KUNIT_EXPECT_TRUE(test,
			  tcti_syscall_changes_user_mappings_for_tests(__NR_mremap));
	KUNIT_EXPECT_FALSE(test,
			   tcti_syscall_changes_user_mappings_for_tests(__NR_read));
}

static void tcti_tlb_separates_access_classes(struct kunit *test)
{
	static struct tcti_tlb tlb;
	struct mm_struct *mm = (struct mm_struct *)0x4000UL;
	struct tcti_user_page page = {
		.user_page = 2 * PAGE_SIZE,
		.host_data = (void *)0x100000,
		.translation_generation = 9,
	};
	void *host;
	int ret;

	tcti_tlb_init(&tlb);
	ret = tcti_tlb_fill(&tlb, mm, 2 * PAGE_SIZE + 0x123,
			    TCTI_ACCESS_FETCH, &page);
	KUNIT_ASSERT_EQ(test, 0, ret);

	host = tcti_tlb_lookup(&tlb, mm, 2 * PAGE_SIZE + 0x123,
			       TCTI_ACCESS_FETCH, 9);
	KUNIT_EXPECT_EQ(test, 0x100123UL, (unsigned long)host);
	KUNIT_EXPECT_EQ(test, 1ULL, tlb.fetch_hits);

	host = tcti_tlb_lookup(&tlb, mm, 2 * PAGE_SIZE + 0x123,
			       TCTI_ACCESS_READ, 9);
	KUNIT_EXPECT_NULL(test, host);
	KUNIT_EXPECT_EQ(test, 1ULL, tlb.read_misses);
}

static void tcti_tlb_flushes_on_generation_change(struct kunit *test)
{
	static struct tcti_tlb tlb;
	struct mm_struct *mm = (struct mm_struct *)0x5000UL;
	struct tcti_user_page page = {
		.user_page = 3 * PAGE_SIZE,
		.host_data = (void *)0x200000,
		.translation_generation = 3,
	};
	void *host;
	int ret;

	tcti_tlb_init(&tlb);
	ret = tcti_tlb_fill(&tlb, mm, 3 * PAGE_SIZE + 8,
			    TCTI_ACCESS_READ, &page);
	KUNIT_ASSERT_EQ(test, 0, ret);

	host = tcti_tlb_lookup(&tlb, mm, 3 * PAGE_SIZE + 8,
			       TCTI_ACCESS_READ, 3);
	KUNIT_EXPECT_EQ(test, 0x200008UL, (unsigned long)host);

	host = tcti_tlb_lookup(&tlb, mm, 3 * PAGE_SIZE + 8,
			       TCTI_ACCESS_READ, 4);
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

static void tcti_switch_executes_add_sub_with_carry_flags(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	int ret;

	regs.regs[12] = 0x01000000ULL;
	regs.regs[13] = 0x01000000ULL;
	regs.pstate = PSR_C_BIT;
	regs.pc = 0x4020;
	decoded = tcti_decode_aarch64(0xfa0d019fU);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x01000000ULL, regs.regs[12]);
	KUNIT_EXPECT_EQ(test, 0x01000000ULL, regs.regs[13]);
	KUNIT_EXPECT_TRUE(test, regs.pstate & PSR_Z_BIT);
	KUNIT_EXPECT_TRUE(test, regs.pstate & PSR_C_BIT);
	KUNIT_EXPECT_FALSE(test, regs.pstate & PSR_N_BIT);
	KUNIT_EXPECT_FALSE(test, regs.pstate & PSR_V_BIT);
	KUNIT_EXPECT_EQ(test, 0x4024ULL, regs.pc);

	regs.regs[0] = 0;
	regs.regs[1] = U64_MAX;
	regs.pstate = 0;
	decoded = tcti_decode_aarch64(0xfa01001fU);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_TRUE(test, regs.pstate & PSR_Z_BIT);
	KUNIT_EXPECT_FALSE(test, regs.pstate & PSR_C_BIT);
	KUNIT_EXPECT_FALSE(test, regs.pstate & PSR_N_BIT);
	KUNIT_EXPECT_FALSE(test, regs.pstate & PSR_V_BIT);
	KUNIT_EXPECT_EQ(test, 0x4028ULL, regs.pc);
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

static void tcti_switch_executes_neg_with_xzr_source(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	int ret;

	regs.regs[9] = 8;
	regs.sp = 0x413620aaba70ULL;
	regs.pc = 0x41361127bd94ULL;

	decoded = tcti_decode_aarch64(0xcb0903edU);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0xfffffffffffffff8ULL, regs.regs[13]);
	KUNIT_EXPECT_EQ(test, 0x413620aaba70ULL, regs.sp);
	KUNIT_EXPECT_EQ(test, 0x41361127bd98ULL, regs.pc);
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

	regs = (struct pt_regs) {};
	regs.pc = 0x62548a56a1e8ULL;

	decoded = tcti_decode_aarch64(0xb00000c8U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x62548a583000ULL, regs.regs[8]);
	KUNIT_EXPECT_EQ(test, 0x62548a56a1ecULL, regs.pc);
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

	regs.pc = 0x3a788;
	decoded = tcti_decode_aarch64(0x97fff3d0U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x3a78cULL, regs.regs[30]);
	KUNIT_EXPECT_EQ(test, 0x376c8ULL, regs.pc);
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

	regs = (struct pt_regs) {};
	regs.regs[5] = 0x2427c;
	regs.regs[30] = 0xfeedface;
	regs.pc = 0x242f8;

	decoded = tcti_decode_aarch64(0xd61f00a0U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0xfeedfaceULL, regs.regs[30]);
	KUNIT_EXPECT_EQ(test, 0x2427cULL, regs.pc);
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
	decoded = tcti_decode_aarch64(0xb7f80080U);
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

static void tcti_switch_cmp_equal_skips_heap_guard_branch(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	int ret;

	regs.pc = 0x1bca0;
	regs.regs[8] = 0x288ca5c2db0ULL;
	regs.regs[12] = 0x288ca5c2db0ULL;
	decoded = tcti_decode_aarch64(0xeb0c011fU);

	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_NE(test, 0ULL, regs.pstate & PSR_Z_BIT);
	KUNIT_EXPECT_EQ(test, 0x1bca4ULL, regs.pc);

	decoded = tcti_decode_aarch64(0x54000441U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x1bca8ULL, regs.pc);
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

static void tcti_switch_executes_mlibc_float_helper_conditional_compares(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	int ret;

	regs.regs[8] = 5;
	regs.regs[10] = 7;
	regs.pstate = PSR_Z_BIT;
	regs.pc = 0x7300;
	decoded = tcti_decode_aarch64(0xfa4a0100U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_TRUE(test, regs.pstate & PSR_N_BIT);
	KUNIT_EXPECT_FALSE(test, regs.pstate & PSR_Z_BIT);
	KUNIT_EXPECT_FALSE(test, regs.pstate & PSR_C_BIT);
	KUNIT_EXPECT_FALSE(test, regs.pstate & PSR_V_BIT);
	KUNIT_EXPECT_EQ(test, 0x7304ULL, regs.pc);

	regs.pstate = 0;
	decoded = tcti_decode_aarch64(0xfa4a0100U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_FALSE(test, regs.pstate & PSR_N_BIT);
	KUNIT_EXPECT_FALSE(test, regs.pstate & PSR_Z_BIT);
	KUNIT_EXPECT_FALSE(test, regs.pstate & PSR_C_BIT);
	KUNIT_EXPECT_FALSE(test, regs.pstate & PSR_V_BIT);
	KUNIT_EXPECT_EQ(test, 0x7308ULL, regs.pc);

	regs.regs[13] = 0;
	regs.pstate = 0;
	decoded = tcti_decode_aarch64(0xfa4019a4U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_FALSE(test, regs.pstate & PSR_N_BIT);
	KUNIT_EXPECT_TRUE(test, regs.pstate & PSR_Z_BIT);
	KUNIT_EXPECT_TRUE(test, regs.pstate & PSR_C_BIT);
	KUNIT_EXPECT_FALSE(test, regs.pstate & PSR_V_BIT);
	KUNIT_EXPECT_EQ(test, 0x730cULL, regs.pc);

	regs.regs[14] = U32_MAX;
	regs.pstate = PSR_N_BIT;
	decoded = tcti_decode_aarch64(0x3a41b9c4U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_FALSE(test, regs.pstate & PSR_N_BIT);
	KUNIT_EXPECT_TRUE(test, regs.pstate & PSR_Z_BIT);
	KUNIT_EXPECT_TRUE(test, regs.pstate & PSR_C_BIT);
	KUNIT_EXPECT_FALSE(test, regs.pstate & PSR_V_BIT);
	KUNIT_EXPECT_EQ(test, 0x7310ULL, regs.pc);
}

static void tcti_switch_executes_conditional_select(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	int ret;

	regs.regs[11] = 0x1234;
	regs.pstate = PSR_C_BIT;
	regs.pc = 0x1ad08;

	decoded = tcti_decode_aarch64(0x5a9f216aU);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x1234ULL, regs.regs[10]);
	KUNIT_EXPECT_EQ(test, 0x1ad0cULL, regs.pc);

	regs.pstate = 0;
	decoded = tcti_decode_aarch64(0x5a9f216aU);
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

#if defined(ORLIX_APP_HOSTED_BOOT)
	current->thread.user_simd[0] = 0x4008000000000000ULL;
	current->thread.user_simd[4] = 0x4018000000000000ULL;
	regs.pstate = 0;
	decoded = tcti_decode_aarch64(0x1e603c40U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x4018000000000000ULL,
			current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, 0x1ad18ULL, regs.pc);

	current->thread.user_simd[0] = 0x4008000000000000ULL;
	current->thread.user_simd[4] = 0x4018000000000000ULL;
	regs.pstate = PSR_C_BIT;
	decoded = tcti_decode_aarch64(0x1e603c40U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x4008000000000000ULL,
			current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, 0x1ad1cULL, regs.pc);
#endif
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
	KUNIT_EXPECT_EQ(test, 0x2ULL, regs.regs[12]);
	KUNIT_EXPECT_EQ(test, 0x8008ULL, regs.pc);

	regs.regs[16] = 0x8000000000000000ULL;
	regs.regs[17] = 0x2ULL;
	decoded = tcti_decode_aarch64(0x93d0fe2aU);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x5ULL, regs.regs[10]);
	KUNIT_EXPECT_EQ(test, 0x800cULL, regs.pc);
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

	regs.regs[20] = 0x00000002U;
	decoded = tcti_decode_aarch64(0x5ac00288U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x40000000ULL, regs.regs[8]);
	KUNIT_EXPECT_EQ(test, 0x810cULL, regs.pc);

	regs.regs[8] = 0x11223344U;
	decoded = tcti_decode_aarch64(0x5ac00503U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x22114433ULL, regs.regs[3]);
	KUNIT_EXPECT_EQ(test, 0x8110ULL, regs.pc);

	regs.regs[8] = 0x01020304U;
	decoded = tcti_decode_aarch64(0x5ac00909U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x04030201ULL, regs.regs[9]);
	KUNIT_EXPECT_EQ(test, 0x8114ULL, regs.pc);
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
	unsigned long old_fpsr = current->thread.user_fpsr;
	unsigned long old_fpcr = current->thread.user_fpcr;
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
	current->thread.user_fpcr = 0x300000ULL;
	decoded = tcti_decode_aarch64(0xd53b4401U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x300000ULL, regs.regs[1]);
	KUNIT_EXPECT_EQ(test, 0x500cULL, regs.pc);

	regs.regs[1] = 0x1fU;
	decoded = tcti_decode_aarch64(0xd51b4421U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x1fUL, current->thread.user_fpsr);
	KUNIT_EXPECT_EQ(test, 0x5010ULL, regs.pc);

	current->thread.user_tls = STACK_TOP - 32;
	regs.pc = 0x1a9e8;

	decoded = tcti_decode_aarch64(0xd53bd05aU);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, STACK_TOP - 32, regs.regs[26]);
	KUNIT_EXPECT_EQ(test, 0x1a9ecULL, regs.pc);

	regs.regs[0] = STACK_TOP - 16;
	decoded = tcti_decode_aarch64(0xd51bd040U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, STACK_TOP - 16, current->thread.user_tls);
	KUNIT_EXPECT_EQ(test, 0x1a9f0ULL, regs.pc);

	current->thread.user_tls = old_tls;
	current->thread.user_fpsr = old_fpsr;
	current->thread.user_fpcr = old_fpcr;
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

#if defined(ORLIX_APP_HOSTED_BOOT)
static void tcti_switch_executes_simd_modified_immediates(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	int ret;

	current->thread.user_simd[0] = 0x123456789abcdef0ULL;
	current->thread.user_simd[1] = 0xfedcba9876543210ULL;
	current->thread.user_simd_valid = 0;
	regs.pc = 0x8400;

	decoded = tcti_decode_aarch64(0x6f00e400U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0ULL, current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, 0ULL, current->thread.user_simd[1]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, 0x8404ULL, regs.pc);

	current->thread.user_simd[0] = 0x123456789abcdef0ULL;
	current->thread.user_simd[1] = 0xfedcba9876543210ULL;
	current->thread.user_simd_valid = 0;

	decoded = tcti_decode_aarch64(0x2f00e400U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0ULL, current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, 0ULL, current->thread.user_simd[1]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, 0x8408ULL, regs.pc);

	current->thread.user_simd[0] = 0;
	current->thread.user_simd[1] = 0;
	current->thread.user_simd_valid = 0;

	decoded = tcti_decode_aarch64(0x6f07e7e0U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, ~0ULL, current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, ~0ULL, current->thread.user_simd[1]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, 0x840cULL, regs.pc);

	current->thread.user_simd[0] = 0;
	current->thread.user_simd[1] = 0xfedcba9876543210ULL;
	current->thread.user_simd_valid = 0;

	decoded = tcti_decode_aarch64(0x0f002420U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x0000010000000100ULL,
			current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, 0ULL, current->thread.user_simd[1]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, 0x8410ULL, regs.pc);

	current->thread.user_simd[2] = 0;
	current->thread.user_simd[3] = 0xfedcba9876543210ULL;
	current->thread.user_simd_valid = 0;

	decoded = tcti_decode_aarch64(0x0f026441U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x4200000042000000ULL,
			current->thread.user_simd[2]);
	KUNIT_EXPECT_EQ(test, 0ULL, current->thread.user_simd[3]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, 0x8414ULL, regs.pc);

	current->thread.user_simd[0] = 0;
	current->thread.user_simd[1] = 0;
	current->thread.user_simd_valid = 0;

	decoded = tcti_decode_aarch64(0x4f000421U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x0000000100000001ULL,
			current->thread.user_simd[2]);
	KUNIT_EXPECT_EQ(test, 0x0000000100000001ULL,
			current->thread.user_simd[3]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, 0x8418ULL, regs.pc);

	current->thread.user_simd[0] = 0;
	current->thread.user_simd[1] = 0;
	current->thread.user_simd[2] = 0;
	current->thread.user_simd[3] = 0;
	current->thread.user_simd_valid = 0;

	decoded = tcti_decode_aarch64(0x4f06e7e0U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0xdfdfdfdfdfdfdfdfULL,
			current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, 0xdfdfdfdfdfdfdfdfULL,
			current->thread.user_simd[1]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, 0x841cULL, regs.pc);

	current->thread.user_simd[16] = 0;
	current->thread.user_simd[17] = 0xfedcba9876543210ULL;
	current->thread.user_simd_valid = 0;

	decoded = tcti_decode_aarch64(0x0f00e548U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x0a0a0a0a0a0a0a0aULL,
			current->thread.user_simd[16]);
	KUNIT_EXPECT_EQ(test, 0ULL, current->thread.user_simd[17]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, 0x8420ULL, regs.pc);

	current->thread.user_simd[0] = 0;
	current->thread.user_simd[1] = 0;
	current->thread.user_simd_valid = 0;

	decoded = tcti_decode_aarch64(0x4f020420U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x0000004100000041ULL,
			current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, 0x0000004100000041ULL,
			current->thread.user_simd[1]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, 0x8424ULL, regs.pc);
}

static void tcti_switch_executes_complete_simd_modified_immediate_family(
	struct kunit *test)
{
	static const struct {
		u32 instruction;
		u64 initial;
		u64 expected;
	} cases[] = {
		{ 0x4f002640U, 0, 0x0000120000001200ULL },
		{ 0x6f014681U, 0, 0xffcbffffffcbffffULL },
		{ 0x4f0276c2U, 0x00ff00ff00ff00ffULL,
		  0x56ff00ff56ff00ffULL },
		{ 0x6f03b703U, ~0ULL, 0x87ff87ff87ff87ffULL },
		{ 0x4f04c744U, 0, 0x00009aff00009affULL },
		{ 0x6f05d785U, 0, 0xff430000ff430000ULL },
		{ 0x6f02e6a6U, 0, 0x00ff00ff00ff00ffULL },
		{ 0x4f03f607U, 0, 0x3f8000003f800000ULL },
		{ 0x6f00f408U, 0, 0x4000000000000000ULL },
	};
	struct pt_regs regs = {};
	size_t index;

	regs.pc = 0x8420;
	for (index = 0; index < ARRAY_SIZE(cases); index++) {
		struct tcti_decoded_instruction decoded;
		u8 rd = cases[index].instruction & 0x1fU;
		int ret;

		current->thread.user_simd[rd * 2] = cases[index].initial;
		current->thread.user_simd[rd * 2 + 1] = cases[index].initial;
		current->thread.user_simd_valid = 0;
		decoded = tcti_decode_aarch64(cases[index].instruction);
		ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded,
							NULL);

		KUNIT_ASSERT_EQ(test, 0, ret);
		KUNIT_EXPECT_EQ(test, cases[index].expected,
				current->thread.user_simd[rd * 2]);
		KUNIT_EXPECT_EQ(test, cases[index].expected,
				current->thread.user_simd[rd * 2 + 1]);
		KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
		KUNIT_EXPECT_EQ(test, 0x8424ULL + index * sizeof(u32), regs.pc);
	}
}

static void tcti_switch_executes_fmov_d_negative_one_immediate(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	int ret;

	current->thread.user_simd[0] = 0;
	current->thread.user_simd[1] = 0x123456789abcdef0ULL;
	current->thread.user_simd_valid = 0;
	regs.pc = 0x8420;

	decoded = tcti_decode_aarch64(0x1e7e1000U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0xbff0000000000000ULL,
			current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, 0ULL, current->thread.user_simd[1]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, 0x8424ULL, regs.pc);
}

static void tcti_switch_executes_basenc_simd_immediates(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	int ret;

	current->thread.user_simd[0] = 0;
	current->thread.user_simd[1] = ~0ULL;
	current->thread.user_simd_valid = 0;
	regs.pc = 0x84f0;

	decoded = tcti_decode_aarch64(0x0f018560U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x002b002b002b002bULL,
			current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, 0ULL, current->thread.user_simd[1]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, 0x84f4ULL, regs.pc);

	decoded = tcti_decode_aarch64(0x0f018620U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x0031003100310031ULL,
			current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, 0ULL, current->thread.user_simd[1]);
	KUNIT_EXPECT_EQ(test, 0x84f8ULL, regs.pc);
}

static void tcti_switch_executes_complete_simd_dup_gpr_family(
	struct kunit *test)
{
	static const struct {
		u32 instruction;
		u64 expected;
		bool high;
	} cases[] = {
		{ 0x4e010d49U, 0x8888888888888888ULL, true },
		{ 0x4e020d6aU, 0x7788778877887788ULL, true },
		{ 0x0e040d8bU, 0x5566778855667788ULL, false },
		{ 0x4e040dacU, 0x5566778855667788ULL, true },
		{ 0x4e080dcdU, 0x1122334455667788ULL, true },
	};
	struct pt_regs regs = {};
	size_t index;

	regs.pc = 0x8440;
	for (index = 0; index < ARRAY_SIZE(cases); index++) {
		struct tcti_decoded_instruction decoded;
		u8 rd = cases[index].instruction & 0x1fU;
		u8 rn = (cases[index].instruction >> 5) & 0x1fU;
		int ret;

		regs.regs[rn] = 0x1122334455667788ULL;
		current->thread.user_simd[rd * 2] = 0;
		current->thread.user_simd[rd * 2 + 1] = ~0ULL;
		current->thread.user_simd_valid = 0;
		decoded = tcti_decode_aarch64(cases[index].instruction);
		ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded,
							NULL);

		KUNIT_ASSERT_EQ(test, 0, ret);
		KUNIT_EXPECT_EQ(test, cases[index].expected,
				current->thread.user_simd[rd * 2]);
		KUNIT_EXPECT_EQ(test,
				cases[index].high ? cases[index].expected : 0,
				current->thread.user_simd[rd * 2 + 1]);
		KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
		KUNIT_EXPECT_EQ(test, 0x8444ULL + index * sizeof(u32), regs.pc);
	}
}

static void tcti_switch_executes_simd_dup_4h_gpr(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	int ret;

	current->thread.user_simd[2] = 0;
	current->thread.user_simd[3] = ~0ULL;
	current->thread.user_simd_valid = 0;
	regs.regs[12] = 0x1122334455667788ULL;
	regs.pc = 0x84f8;

	decoded = tcti_decode_aarch64(0x0e020d81U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x7788778877887788ULL,
			current->thread.user_simd[2]);
	KUNIT_EXPECT_EQ(test, 0ULL, current->thread.user_simd[3]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, 0x84fcULL, regs.pc);
}

static void tcti_switch_executes_simd_dup_2d_gpr(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	int ret;

	current->thread.user_simd[0] = 0;
	current->thread.user_simd[1] = 0;
	current->thread.user_simd_valid = 0;
	regs.regs[12] = 0x1122334455667788ULL;
	regs.pc = 0x8500;

	decoded = tcti_decode_aarch64(0x4e080d80U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x1122334455667788ULL,
			current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, 0x1122334455667788ULL,
			current->thread.user_simd[1]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, 0x8504ULL, regs.pc);

	current->thread.user_simd[2] = 0;
	current->thread.user_simd[3] = 0;
	current->thread.user_simd_valid = 0;
	regs.regs[0] = 0x1122334455667788ULL;
	regs.pc = 0x8504;

	decoded = tcti_decode_aarch64(0x4e040c01U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x5566778855667788ULL,
			current->thread.user_simd[2]);
	KUNIT_EXPECT_EQ(test, 0x5566778855667788ULL,
			current->thread.user_simd[3]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, 0x8508ULL, regs.pc);
}

static void tcti_switch_executes_simd_mov_d1_d0(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	int ret;

	current->thread.user_simd[0] = 0x1122334455667788ULL;
	current->thread.user_simd[1] = 0xaabbccddeeff0011ULL;
	current->thread.user_simd_valid = 0;
	regs.pc = 0x8508;

	decoded = tcti_decode_aarch64(0x6e180400U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x1122334455667788ULL,
			current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, 0x1122334455667788ULL,
			current->thread.user_simd[1]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, 0x850cULL, regs.pc);
}

static void tcti_switch_executes_simd_and_16b(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	int ret;

	current->thread.user_simd[0] = 0xff00ff00ff00ff00ULL;
	current->thread.user_simd[1] = 0xaaaaaaaaaaaaaaaaULL;
	current->thread.user_simd[2] = 0x0f0f0f0f0f0f0f0fULL;
	current->thread.user_simd[3] = 0xbbbbbbbbbbbbbbbbULL;
	current->thread.user_simd_valid = 0;
	regs.pc = 0x85f0;
	decoded = tcti_decode_aarch64(0x0e211c00U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x0f000f000f000f00ULL,
			current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, 0ULL, current->thread.user_simd[1]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, 0x85f4ULL, regs.pc);

	current->thread.user_simd[0] = 0xf0f0f0f0f0f0f0f0ULL;
	current->thread.user_simd[1] = 0xaaaaaaaaaaaaaaaaULL;
	current->thread.user_simd[2] = 0x00ff00ff00ff00ffULL;
	current->thread.user_simd[3] = 0xbbbbbbbbbbbbbbbbULL;
	current->thread.user_simd[6] = 0x1122334455667788ULL;
	current->thread.user_simd[7] = 0xccccccccccccccccULL;
	current->thread.user_simd_valid = 0;

	decoded = tcti_decode_aarch64(0x2ea11c60U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0xf022f044f066f088ULL,
			current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, 0ULL, current->thread.user_simd[1]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, 0x85f8ULL, regs.pc);

	current->thread.user_simd[0] = 0xff00ff00ff00ff00ULL;
	current->thread.user_simd[1] = 0x0f0f0f0f0f0f0f0fULL;
	current->thread.user_simd[2] = 0xffff0000ffff0000ULL;
	current->thread.user_simd[3] = 0x00ff00ff00ff00ffULL;
	current->thread.user_simd_valid = 0;
	regs.pc = 0x8600;

	decoded = tcti_decode_aarch64(0x4e211c01U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0xff000000ff000000ULL,
			current->thread.user_simd[2]);
	KUNIT_EXPECT_EQ(test, 0x000f000f000f000fULL,
			current->thread.user_simd[3]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, 0x8604ULL, regs.pc);

	current->thread.user_simd[2] = 0xffffffffffffffffULL;
	current->thread.user_simd[3] = 0x0000000000000004ULL;
	current->thread.user_simd[4] = 0x0000000000000002ULL;
	current->thread.user_simd[5] = 0x0000000000000003ULL;
	current->thread.user_simd_valid = 0;
	regs.pc = 0x8604;

	decoded = tcti_decode_aarch64(0x4ee18441U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 1ULL, current->thread.user_simd[2]);
	KUNIT_EXPECT_EQ(test, 7ULL, current->thread.user_simd[3]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, 0x8608ULL, regs.pc);

	current->thread.user_simd[0] = 0x00000000ffffffffULL;
	current->thread.user_simd[1] = 0xffffffff00000000ULL;
	current->thread.user_simd[4] = 0x0000000100000001ULL;
	current->thread.user_simd[5] = 0x0000000100000001ULL;
	current->thread.user_simd_valid = 0;
	regs.pc = 0x8608;

	decoded = tcti_decode_aarch64(0x4ea08440U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x0000000100000000ULL,
			current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, 0x0000000000000001ULL,
			current->thread.user_simd[1]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, 0x860cULL, regs.pc);

	current->thread.user_simd[0] = 0x0000000200000001ULL;
	current->thread.user_simd[1] = 0x0000000400000003ULL;
	current->thread.user_simd[2] = 0x0000010000000080ULL;
	current->thread.user_simd[3] = 0xffffffff00000180ULL;
	current->thread.user_simd_valid = 0;
	regs.pc = 0x860c;

	decoded = tcti_decode_aarch64(0x6f391420U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x0000000400000002ULL,
			current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, 0x0200000300000006ULL,
			current->thread.user_simd[1]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, 0x8610ULL, regs.pc);

	current->thread.user_simd[0] = 9;
	current->thread.user_simd[1] = 2;
	current->thread.user_simd[4] = 8;
	current->thread.user_simd[5] = 3;
	current->thread.user_simd[10] = 0;
	current->thread.user_simd[11] = 0;
	current->thread.user_simd_valid = 0;
	regs.pc = 0x8610;

	decoded = tcti_decode_aarch64(0x6ee23405U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, ~0ULL, current->thread.user_simd[10]);
	KUNIT_EXPECT_EQ(test, 0ULL, current->thread.user_simd[11]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, 0x8614ULL, regs.pc);
}

static void tcti_switch_executes_simd_orr_4s_immediate(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	int ret;

	current->thread.user_simd[0] = 0x0000000100000002ULL;
	current->thread.user_simd[1] = 0x0000000400000008ULL;
	current->thread.user_simd_valid = 0;
	regs.pc = 0x8700;

	decoded = tcti_decode_aarch64(0x4f011600U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x0000003100000032ULL,
			current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, 0x0000003400000038ULL,
			current->thread.user_simd[1]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, 0x8704ULL, regs.pc);

	current->thread.user_simd[4] = 0xffffffff12345678ULL;
	current->thread.user_simd[5] = 0x000000ff000000feULL;
	current->thread.user_simd_valid = 0;

	decoded = tcti_decode_aarch64(0x6f0717c2U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0xffffff0112345600ULL,
			current->thread.user_simd[4]);
	KUNIT_EXPECT_EQ(test, 0x0000000100000000ULL,
			current->thread.user_simd[5]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, 0x8708ULL, regs.pc);

	current->thread.user_simd[6] = 0xffffffff12345678ULL;
	current->thread.user_simd[7] = 0x0000007f0000007eULL;
	current->thread.user_simd_valid = 0;

	decoded = tcti_decode_aarch64(0x6f0317c3U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0xffffff8112345600ULL,
			current->thread.user_simd[6]);
	KUNIT_EXPECT_EQ(test, 0x0000000100000000ULL,
			current->thread.user_simd[7]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, 0x870cULL, regs.pc);

	current->thread.user_simd[8] = 0xffffffff12345678ULL;
	current->thread.user_simd[9] = 0x0000003f0000003eULL;
	current->thread.user_simd_valid = 0;

	decoded = tcti_decode_aarch64(0x6f0117c4U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0xffffffc112345640ULL,
			current->thread.user_simd[8]);
	KUNIT_EXPECT_EQ(test, 0x0000000100000000ULL,
			current->thread.user_simd[9]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, 0x8710ULL, regs.pc);

	current->thread.user_simd[12] = 0xffffffff12345678ULL;
	current->thread.user_simd[13] = 0x0000000f0000000eULL;
	current->thread.user_simd_valid = 0;

	decoded = tcti_decode_aarch64(0x6f0015c6U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0xfffffff112345670ULL,
			current->thread.user_simd[12]);
	KUNIT_EXPECT_EQ(test, 0x0000000100000000ULL,
			current->thread.user_simd[13]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, 0x8714ULL, regs.pc);

	current->thread.user_simd[14] = 0xffffffff12345678ULL;
	current->thread.user_simd[15] = 0x0000000700000006ULL;
	current->thread.user_simd_valid = 0;

	decoded = tcti_decode_aarch64(0x6f0014c7U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0xfffffff912345678ULL,
			current->thread.user_simd[14]);
	KUNIT_EXPECT_EQ(test, 0x0000000100000000ULL,
			current->thread.user_simd[15]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, 0x8718ULL, regs.pc);

	current->thread.user_simd[4] = 0xffffffff12345678ULL;
	current->thread.user_simd[5] = 0x0000000300000002ULL;
	current->thread.user_simd_valid = 0;

	decoded = tcti_decode_aarch64(0x6f001442U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0xfffffffd12345678ULL,
			current->thread.user_simd[4]);
	KUNIT_EXPECT_EQ(test, 0x0000000100000000ULL,
			current->thread.user_simd[5]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, 0x871cULL, regs.pc);
}

static void tcti_switch_executes_simd_cmeq_4s(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	int ret;

	current->thread.user_simd[2] = 0x0000000300000001ULL;
	current->thread.user_simd[3] = 0x0000000400000002ULL;
	current->thread.user_simd[6] = 0x0000000300000001ULL;
	current->thread.user_simd[7] = 0x0000000500000002ULL;
	current->thread.user_simd[8] = 0;
	current->thread.user_simd[9] = 0;
	current->thread.user_simd_valid = 0;
	regs.pc = 0x8800;

	decoded = tcti_decode_aarch64(0x6ea18c64U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, ~0ULL,
			current->thread.user_simd[8]);
	KUNIT_EXPECT_EQ(test, 0x00000000ffffffffULL,
			current->thread.user_simd[9]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, 0x8804ULL, regs.pc);

	current->thread.user_simd[8] = 0x0a090a070a050a03ULL;
	current->thread.user_simd[9] = 0x1111111111111111ULL;
	current->thread.user_simd[16] = 0x0a0a0a0a0a0a0a0aULL;
	current->thread.user_simd[17] = 0x2222222222222222ULL;
	current->thread.user_simd_valid = 0;

	decoded = tcti_decode_aarch64(0x2e288c84U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0xff00ff00ff00ff00ULL,
			current->thread.user_simd[8]);
	KUNIT_EXPECT_EQ(test, 0ULL, current->thread.user_simd[9]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, 0x8808ULL, regs.pc);

	current->thread.user_simd[2] = 0x0000000700000000ULL;
	current->thread.user_simd[3] = 0x0000000900000000ULL;
	current->thread.user_simd_valid = 0;

	decoded = tcti_decode_aarch64(0x4ea09821U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x00000000ffffffffULL,
			current->thread.user_simd[2]);
	KUNIT_EXPECT_EQ(test, 0x00000000ffffffffULL,
			current->thread.user_simd[3]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, 0x880cULL, regs.pc);
}

static void tcti_switch_executes_complete_simd_permute_family(
	struct kunit *test)
{
	static const struct {
		u8 encoding;
		u64 expected_low;
		u64 expected_high;
	} cases[] = {
		{ 1, 0x0016001400120010ULL, 0x0026002400220020ULL },
		{ 5, 0x0017001500130011ULL, 0x0027002500230021ULL },
		{ 2, 0x0022001200200010ULL, 0x0026001600240014ULL },
		{ 6, 0x0023001300210011ULL, 0x0027001700250015ULL },
		{ 3, 0x0021001100200010ULL, 0x0023001300220012ULL },
		{ 7, 0x0025001500240014ULL, 0x0027001700260016ULL },
	};
	struct pt_regs regs = {};
	size_t index;

	regs.pc = 0x8480;
	for (index = 0; index < ARRAY_SIZE(cases); index++) {
		u32 instruction = 0x0e000800U | BIT(30) | BIT(22) |
				  (2U << 16) | ((u32)cases[index].encoding << 12) |
				  (1U << 5);
		struct tcti_decoded_instruction decoded;
		int ret;

		current->thread.user_simd[2] = 0x0013001200110010ULL;
		current->thread.user_simd[3] = 0x0017001600150014ULL;
		current->thread.user_simd[4] = 0x0023002200210020ULL;
		current->thread.user_simd[5] = 0x0027002600250024ULL;
		current->thread.user_simd[0] = ~0ULL;
		current->thread.user_simd[1] = ~0ULL;
		current->thread.user_simd_valid = 0;
		decoded = tcti_decode_aarch64(instruction);
		ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded,
							NULL);

		KUNIT_ASSERT_EQ(test, 0, ret);
		KUNIT_EXPECT_EQ(test, cases[index].expected_low,
				current->thread.user_simd[0]);
		KUNIT_EXPECT_EQ(test, cases[index].expected_high,
				current->thread.user_simd[1]);
		KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
		KUNIT_EXPECT_EQ(test, 0x8484ULL + index * sizeof(u32), regs.pc);
	}
}

static void tcti_switch_executes_simd_uzp1_4h(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	int ret;

	current->thread.user_simd[10] = 0x4444333322221111ULL;
	current->thread.user_simd[11] = 0;
	current->thread.user_simd[0] = 0x8888777766665555ULL;
	current->thread.user_simd[1] = 0;
	current->thread.user_simd[12] = 0;
	current->thread.user_simd[13] = 0;
	current->thread.user_simd_valid = 0;
	regs.pc = 0x8808;

	decoded = tcti_decode_aarch64(0x0e4018a6U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x7777555533331111ULL,
			current->thread.user_simd[12]);
	KUNIT_EXPECT_EQ(test, 0ULL, current->thread.user_simd[13]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, 0x880cULL, regs.pc);

	current->thread.user_simd[4] = 0x4444333322221111ULL;
	current->thread.user_simd[5] = 0x8888777766665555ULL;
	current->thread.user_simd[2] = 0xddddccccbbbbaaaaULL;
	current->thread.user_simd[3] = 0x22221111ffffeeeeULL;
	current->thread.user_simd_valid = 0;

	decoded = tcti_decode_aarch64(0x4e411841U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x7777555533331111ULL,
			current->thread.user_simd[2]);
	KUNIT_EXPECT_EQ(test, 0x1111eeeeccccaaaaULL,
			current->thread.user_simd[3]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, 0x8810ULL, regs.pc);
}

static void tcti_switch_executes_simd_umov_w_h0(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	int ret;

	current->thread.user_simd[12] = 0x7777555533331111ULL;
	regs.regs[15] = 0xffffffffffffffffULL;
	regs.pc = 0x8810;

	decoded = tcti_decode_aarch64(0x0e023ccfU);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x1111ULL, regs.regs[15]);
	KUNIT_EXPECT_EQ(test, 0x8814ULL, regs.pc);
}

static void tcti_switch_executes_simd_umov_w_h1(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	int ret;

	current->thread.user_simd[10] = 0x7777555533331111ULL;
	regs.regs[15] = 0xffffffffffffffffULL;
	regs.pc = 0x8820;

	decoded = tcti_decode_aarch64(0x0e063cafU);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x3333ULL, regs.regs[15]);
	KUNIT_EXPECT_EQ(test, 0x8824ULL, regs.pc);
}

static void tcti_switch_executes_simd_umov_x_d1(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	int ret;

	current->thread.user_simd[6] = 0x1111222233334444ULL;
	current->thread.user_simd[7] = 0xaaaabbbbccccddddULL;
	regs.regs[15] = 0xffffffffffffffffULL;
	regs.pc = 0x8828;

	decoded = tcti_decode_aarch64(0x4e183c6fU);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0xaaaabbbbccccddddULL, regs.regs[15]);
	KUNIT_EXPECT_EQ(test, 0x882cULL, regs.pc);
}

static void tcti_switch_executes_simd_ins_gpr_s0(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	int ret;

	current->thread.user_simd[0] = 0xaaaabbbbccccddddULL;
	current->thread.user_simd[1] = 0x1111222233334444ULL;
	current->thread.user_simd_valid = 0;
	regs.regs[8] = 0x123456789abcdef0ULL;
	regs.pc = 0x8828;

	decoded = tcti_decode_aarch64(0x4e041d00U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0xaaaabbbb9abcdef0ULL,
			current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, 0x1111222233334444ULL,
			current->thread.user_simd[1]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, 0x882cULL, regs.pc);
}

static void tcti_switch_executes_simd_umaxv_4s(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	int ret;

	current->thread.user_simd[8] = 0x0000000900000001ULL;
	current->thread.user_simd[9] = 0x0000000400000007ULL;
	current->thread.user_simd[10] = 0xaaaaaaaaaaaaaaaaULL;
	current->thread.user_simd[11] = 0xbbbbbbbbbbbbbbbbULL;
	current->thread.user_simd_valid = 0;
	regs.pc = 0x8900;

	decoded = tcti_decode_aarch64(0x6eb0a885U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 9ULL, current->thread.user_simd[10]);
	KUNIT_EXPECT_EQ(test, 0ULL, current->thread.user_simd[11]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, 0x8904ULL, regs.pc);
}

static void tcti_switch_executes_simd_umaxv_4h(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	int ret;

	current->thread.user_simd[8] = 0x0004000700090001ULL;
	current->thread.user_simd[9] = 0xaaaaaaaaaaaaaaaaULL;
	current->thread.user_simd[10] = 0xbbbbbbbbbbbbbbbbULL;
	current->thread.user_simd[11] = 0xccccccccccccccccULL;
	current->thread.user_simd_valid = 0;
	regs.pc = 0x8e00;

	decoded = tcti_decode_aarch64(0x2e70a885U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 9ULL, current->thread.user_simd[10]);
	KUNIT_EXPECT_EQ(test, 0ULL, current->thread.user_simd[11]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, 0x8e04ULL, regs.pc);
}

static void tcti_switch_executes_simd_sub_2d(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	int ret;

	current->thread.user_simd[0] = 3;
	current->thread.user_simd[1] = 5;
	current->thread.user_simd[2] = 10;
	current->thread.user_simd[3] = 20;
	current->thread.user_simd_valid = 0;
	regs.pc = 0x8af0;

	decoded = tcti_decode_aarch64(0x6ee08420U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 7ULL, current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, 15ULL, current->thread.user_simd[1]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, 0x8af4ULL, regs.pc);
}

static void tcti_switch_executes_simd_fneg_2d(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	int ret;

	current->thread.user_simd[0] = 0x3ff0000000000000ULL;
	current->thread.user_simd[1] = 0xc000000000000000ULL;
	current->thread.user_simd_valid = 0;
	regs.pc = 0x8ae0;

	decoded = tcti_decode_aarch64(0x6ee0f800U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0xbff0000000000000ULL,
			current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, 0x4000000000000000ULL,
			current->thread.user_simd[1]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, 0x8ae4ULL, regs.pc);
}

static void tcti_switch_executes_simd_addv_4s(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	int ret;

	current->thread.user_simd[0] = 0x0000000200000001ULL;
	current->thread.user_simd[1] = 0x0000000400000003ULL;
	current->thread.user_simd_valid = 0;
	regs.pc = 0x8b00;

	decoded = tcti_decode_aarch64(0x4eb1b800U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 10ULL, current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, 0ULL, current->thread.user_simd[1]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, 0x8b04ULL, regs.pc);
}

static void tcti_switch_executes_simd_addp_d_2d(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	int ret;

	current->thread.user_simd[0] = 0x000000000000000aULL;
	current->thread.user_simd[1] = 0x0000000000000008ULL;
	current->thread.user_simd_valid = 0;
	regs.pc = 0x8b10;

	decoded = tcti_decode_aarch64(0x5ef1b800U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 18ULL, current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, 0ULL, current->thread.user_simd[1]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, 0x8b14ULL, regs.pc);
}

static void tcti_switch_executes_simd_ushl_4s(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	int ret;

	current->thread.user_simd[2] = 0x8000000000000008ULL;
	current->thread.user_simd[3] = 0x0000001000000001ULL;
	current->thread.user_simd[6] = 0;
	current->thread.user_simd[7] = 0;
	current->thread.user_simd_valid = 0;
	regs.pc = 0x8b04;

	decoded = tcti_decode_aarch64(0x6f3f0423U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x4000000000000004ULL,
			current->thread.user_simd[6]);
	KUNIT_EXPECT_EQ(test, 0x0000000800000000ULL,
			current->thread.user_simd[7]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, 0x8b08ULL, regs.pc);

	current->thread.user_simd[0] = 0xffffffff00000001ULL;
	current->thread.user_simd[1] = 0xfffffffc0000001fULL;
	current->thread.user_simd[2] = 0x8000000000000008ULL;
	current->thread.user_simd[3] = 0x0000001000000001ULL;
	current->thread.user_simd_valid = 0;
	regs.pc = 0x8b08;

	decoded = tcti_decode_aarch64(0x6ea04421U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x4000000000000010ULL,
			current->thread.user_simd[2]);
	KUNIT_EXPECT_EQ(test, 0x0000000180000000ULL,
			current->thread.user_simd[3]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, 0x8b0cULL, regs.pc);
}

static void tcti_switch_executes_complete_simd_xtn_family(struct kunit *test)
{
	static const struct {
		u32 instruction;
		u64 narrowed;
		bool upper;
	} cases[] = {
		{ 0x0e212801U, 0x0000000088664422ULL, false },
		{ 0x4e212801U, 0x0000000088664422ULL, true },
		{ 0x0e612801U, 0xdd00990055661122ULL, false },
		{ 0x4e612801U, 0xdd00990055661122ULL, true },
		{ 0x0ea12801U, 0xbb00990033441122ULL, false },
		{ 0x4ea12801U, 0xbb00990033441122ULL, true },
	};
	struct pt_regs regs = {};
	size_t index;

	regs.pc = 0x8460;
	for (index = 0; index < ARRAY_SIZE(cases); index++) {
		struct tcti_decoded_instruction decoded;
		int ret;

		current->thread.user_simd[0] = 0x7788556633441122ULL;
		current->thread.user_simd[1] = 0xff00dd00bb009900ULL;
		current->thread.user_simd[2] = 0xdeadbeefcafebabeULL;
		current->thread.user_simd[3] = 0xfacefeed01234567ULL;
		current->thread.user_simd_valid = 0;
		decoded = tcti_decode_aarch64(cases[index].instruction);
		ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded,
							NULL);

		KUNIT_ASSERT_EQ(test, 0, ret);
		KUNIT_EXPECT_EQ(test,
				cases[index].upper ? 0xdeadbeefcafebabeULL :
						     cases[index].narrowed,
				current->thread.user_simd[2]);
		KUNIT_EXPECT_EQ(test,
				cases[index].upper ? cases[index].narrowed : 0,
				current->thread.user_simd[3]);
		KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
		KUNIT_EXPECT_EQ(test, 0x8464ULL + index * sizeof(u32), regs.pc);
	}
}

static void tcti_switch_executes_simd_xtn_4h(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	int ret;

	current->thread.user_simd[0] = 0x2222333311110001ULL;
	current->thread.user_simd[1] = 0x444455553333ffffULL;
	current->thread.user_simd_valid = 0;
	regs.pc = 0x8d00;

	decoded = tcti_decode_aarch64(0x0e612800U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x5555ffff33330001ULL,
			current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, 0ULL, current->thread.user_simd[1]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, 0x8d04ULL, regs.pc);

	current->thread.user_simd[10] = 0x11223344aabbccddULL;
	current->thread.user_simd[11] = 0x5566778801020304ULL;
	current->thread.user_simd_valid = 0;

	decoded = tcti_decode_aarch64(0x0ea128a5U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x01020304aabbccddULL,
			current->thread.user_simd[10]);
	KUNIT_EXPECT_EQ(test, 0ULL, current->thread.user_simd[11]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, 0x8d08ULL, regs.pc);

	current->thread.user_simd[2] = 0x8877665544332211ULL;
	current->thread.user_simd[3] = 0xffeeddccbbaa9988ULL;
	current->thread.user_simd_valid = 0;

	decoded = tcti_decode_aarch64(0x0e212821U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0xeeccaa8877553311ULL,
			current->thread.user_simd[2]);
	KUNIT_EXPECT_EQ(test, 0ULL, current->thread.user_simd[3]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, 0x8d0cULL, regs.pc);
}

static void tcti_switch_executes_simd_ushll_8h(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	int ret;

	current->thread.user_simd[2] = 0x0807060504030201ULL;
	current->thread.user_simd[3] = 0xaabbccddeeff0011ULL;
	current->thread.user_simd_valid = 0;
	regs.pc = 0x8d10;

	decoded = tcti_decode_aarch64(0x2f08a421U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x0004000300020001ULL,
			current->thread.user_simd[2]);
	KUNIT_EXPECT_EQ(test, 0x0008000700060005ULL,
			current->thread.user_simd[3]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, 0x8d14ULL, regs.pc);

	current->thread.user_simd[2] = 0x0004000300020001ULL;
	current->thread.user_simd[3] = 0xaabbccddeeff0011ULL;
	current->thread.user_simd_valid = 0;

	decoded = tcti_decode_aarch64(0x2f10a421U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x0000000200000001ULL,
			current->thread.user_simd[2]);
	KUNIT_EXPECT_EQ(test, 0x0000000400000003ULL,
			current->thread.user_simd[3]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, 0x8d18ULL, regs.pc);
}

static void
tcti_switch_executes_complete_simd_shift_left_long_family(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	int ret;

	current->thread.user_simd[2] = 0x807f02ff0180fe01ULL;
	current->thread.user_simd[3] = 0x80017fff0002ffffULL;
	current->thread.user_simd_valid = 0;
	regs.pc = 0x8d20;

	decoded = tcti_decode_aarch64(0x0f09a420U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x0002ff00fffc0002ULL,
			current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, 0xff0000fe0004fffeULL,
			current->thread.user_simd[1]);
	KUNIT_EXPECT_EQ(test, 0x8d24ULL, regs.pc);

	decoded = tcti_decode_aarch64(0x2f0ba420U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x0008040007f00008ULL,
			current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, 0x040003f8001007f8ULL,
			current->thread.user_simd[1]);
	KUNIT_EXPECT_EQ(test, 0x8d28ULL, regs.pc);

	decoded = tcti_decode_aarch64(0x4f1fa420U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x00010000ffff8000ULL,
			current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, 0xc00080003fff8000ULL,
			current->thread.user_simd[1]);
	KUNIT_EXPECT_EQ(test, 0x8d2cULL, regs.pc);

	decoded = tcti_decode_aarch64(0x6f1fa420U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x000100007fff8000ULL,
			current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, 0x400080003fff8000ULL,
			current->thread.user_simd[1]);
	KUNIT_EXPECT_EQ(test, 0x8d30ULL, regs.pc);

	current->thread.user_simd[3] = 0x80000000ffffffffULL;
	decoded = tcti_decode_aarch64(0x4f3fa420U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0xffffffff80000000ULL,
			current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, 0xc000000000000000ULL,
			current->thread.user_simd[1]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, 0x8d34ULL, regs.pc);
}

static void tcti_switch_executes_simd_ext_16b(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	int ret;

	current->thread.user_simd[0] = 0x0706050403020100ULL;
	current->thread.user_simd[1] = 0x0f0e0d0c0b0a0908ULL;
	current->thread.user_simd[2] = 0xffffffffffffffffULL;
	current->thread.user_simd[3] = 0xffffffffffffffffULL;
	current->thread.user_simd_valid = 0;
	regs.pc = 0x8d18;

	decoded = tcti_decode_aarch64(0x6e004001U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x0f0e0d0c0b0a0908ULL,
			current->thread.user_simd[2]);
	KUNIT_EXPECT_EQ(test, 0x0706050403020100ULL,
			current->thread.user_simd[3]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, 0x8d1cULL, regs.pc);
}

static void tcti_switch_executes_mlibc_cpuset_popcount_sequence(struct kunit *test)
{
	static const u32 instructions[] = {
		0x2f08a421U, /* ushll v1.8h, v1.8b, #0 */
		0x2f10a421U, /* ushll v1.4s, v1.4h, #0 */
		0x4ea11c22U, /* mov v2.16b, v1.16b */
		0x6f3f0423U, /* ushr v3.4s, v1.4s, #1 */
		0x6f3e0424U, /* ushr v4.4s, v1.4s, #2 */
		0x6f3d0425U, /* ushr v5.4s, v1.4s, #3 */
		0x6f3c0426U, /* ushr v6.4s, v1.4s, #4 */
		0x6f3b0427U, /* ushr v7.4s, v1.4s, #5 */
		0x6f0717c2U, /* bic v2.4s, #0xfe */
		0x6f0317c3U, /* bic v3.4s, #0x7e */
		0x6f0117c4U, /* bic v4.4s, #0x3e */
		0x6f0017c5U, /* bic v5.4s, #0x1e */
		0x6f0015c6U, /* bic v6.4s, #0xe */
		0x6f0014c7U, /* bic v7.4s, #0x6 */
		0x4ea08440U, /* add v0.4s, v2.4s, v0.4s */
		0x6f3a0422U, /* ushr v2.4s, v1.4s, #6 */
		0x4ea08460U, /* add v0.4s, v3.4s, v0.4s */
		0x4ea484a3U, /* add v3.4s, v5.4s, v4.4s */
		0x4ea684e4U, /* add v4.4s, v7.4s, v6.4s */
		0x6f001442U, /* bic v2.4s, #0x2 */
		0x4ea08460U, /* add v0.4s, v3.4s, v0.4s */
		0x4ea48442U, /* add v2.4s, v2.4s, v4.4s */
		0x4ea08440U, /* add v0.4s, v2.4s, v0.4s */
		0x6f391420U, /* usra v0.4s, v1.4s, #7 */
		0x4eb1b800U, /* addv s0, v0.4s */
	};
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	int ret;
	u32 i;

	current->thread.user_simd[0] = 0;
	current->thread.user_simd[1] = 0;
	current->thread.user_simd[2] = 0x0000000000000800ULL;
	current->thread.user_simd[3] = 0;
	current->thread.user_simd_valid = 0;
	regs.pc = 0x8d20;

	for (i = 0; i < ARRAY_SIZE(instructions); i++) {
		decoded = tcti_decode_aarch64(instructions[i]);
		KUNIT_ASSERT_NE(test, TCTI_DECODE_UNSUPPORTED,
				decoded.decode_class);
		ret = tcti_switch_debug_execute_decoded(NULL, &regs,
							&decoded, NULL);
		KUNIT_ASSERT_EQ(test, 0, ret);
	}

	KUNIT_EXPECT_EQ(test, 1ULL, current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, 0ULL, current->thread.user_simd[1]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, 0x8d84ULL, regs.pc);
}

static void tcti_switch_executes_mlibc_cpuset_count_from_mapped_mm(struct kunit *test)
{
	static const u32 instructions[] = {
		0x6f00e400U, /* movi v0.2d, #0 */
		0xbc404541U, /* ldr s1, [x10], #0x4 */
		0x2f08a421U, /* ushll v1.8h, v1.8b, #0 */
		0x2f10a421U, /* ushll v1.4s, v1.4h, #0 */
		0x4ea11c22U, /* mov v2.16b, v1.16b */
		0x6f3f0423U, /* ushr v3.4s, v1.4s, #1 */
		0x6f3e0424U, /* ushr v4.4s, v1.4s, #2 */
		0x6f3d0425U, /* ushr v5.4s, v1.4s, #3 */
		0x6f3c0426U, /* ushr v6.4s, v1.4s, #4 */
		0x6f3b0427U, /* ushr v7.4s, v1.4s, #5 */
		0x6f0717c2U, /* bic v2.4s, #0xfe */
		0x6f0317c3U, /* bic v3.4s, #0x7e */
		0x6f0117c4U, /* bic v4.4s, #0x3e */
		0x6f0017c5U, /* bic v5.4s, #0x1e */
		0x6f0015c6U, /* bic v6.4s, #0xe */
		0x6f0014c7U, /* bic v7.4s, #0x6 */
		0x4ea08440U, /* add v0.4s, v2.4s, v0.4s */
		0x6f3a0422U, /* ushr v2.4s, v1.4s, #6 */
		0x4ea08460U, /* add v0.4s, v3.4s, v0.4s */
		0x4ea484a3U, /* add v3.4s, v5.4s, v4.4s */
		0x4ea684e4U, /* add v4.4s, v7.4s, v6.4s */
		0x6f001442U, /* bic v2.4s, #0x2 */
		0x4ea08460U, /* add v0.4s, v3.4s, v0.4s */
		0x4ea48442U, /* add v2.4s, v2.4s, v4.4s */
		0x4ea08440U, /* add v0.4s, v2.4s, v0.4s */
		0x6f391420U, /* usra v0.4s, v1.4s, #7 */
		0x4eb1b800U, /* addv s0, v0.4s */
		0x1e260008U, /* fmov w8, s0 */
	};
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	u8 cpuset[8] = {};
	u8 observed[8] = {};
	unsigned long mapped;
	int ret;
	u32 i;

	KUNIT_ASSERT_NOT_NULL(test, current->mm);
	mapped = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));

	cpuset[1] = 0x08;
	ret = tcti_write_user_data(current->mm, mapped, cpuset, sizeof(cpuset));
	KUNIT_ASSERT_EQ(test, 0, ret);

	current->thread.user_simd[0] = 0xffffffffffffffffULL;
	current->thread.user_simd[1] = 0xffffffffffffffffULL;
	current->thread.user_simd[2] = 0;
	current->thread.user_simd[3] = 0;
	current->thread.user_simd_valid = 0;
	regs.regs[10] = mapped;
	regs.regs[8] = 0xffffffffffffffffULL;
	regs.pc = 0x8da0;

	for (i = 0; i < ARRAY_SIZE(instructions); i++) {
		decoded = tcti_decode_aarch64(instructions[i]);
		KUNIT_ASSERT_NE(test, TCTI_DECODE_UNSUPPORTED,
				decoded.decode_class);
		ret = tcti_switch_debug_execute_decoded(current->mm, &regs,
							&decoded, NULL);
		KUNIT_ASSERT_EQ(test, 0, ret);
	}

	ret = tcti_read_user_data(current->mm, mapped, observed,
				  sizeof(observed));
	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x08U, observed[1]);
	KUNIT_EXPECT_EQ(test, mapped + 4, regs.regs[10]);
	KUNIT_EXPECT_EQ(test, 1ULL, regs.regs[8]);
	KUNIT_EXPECT_EQ(test, 1ULL, current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, 0ULL, current->thread.user_simd[1]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, 0x8e10ULL, regs.pc);

	ret = vm_munmap(mapped, PAGE_SIZE);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void tcti_switch_executes_simd_ld1r_4s_from_mapped_mm(struct kunit *test)
{
	static const u8 lane[] = { 0x44, 0x33, 0x22, 0x11 };
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	unsigned long fault_address = 0;
	unsigned long mapped;
	u64 packed;
	int ret;

	KUNIT_ASSERT_NOT_NULL(test, current->mm);
	mapped = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));

	ret = tcti_write_user_data(current->mm, mapped, lane, sizeof(lane));
	KUNIT_ASSERT_EQ(test, 0, ret);

	current->thread.user_simd[0] = 0xffffffffffffffffULL;
	current->thread.user_simd[1] = 0xffffffffffffffffULL;
	current->thread.user_simd_valid = 0;
	regs.regs[8] = mapped;
	regs.pc = 0x8e20;

	decoded = tcti_decode_aarch64(0x4d40c900U);
	ret = tcti_switch_debug_execute_decoded(current->mm, &regs, &decoded,
						&fault_address);

	packed = 0x1122334411223344ULL;
	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, mapped, fault_address);
	KUNIT_EXPECT_EQ(test, packed, current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, packed, current->thread.user_simd[1]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, 0x8e24ULL, regs.pc);

	ret = vm_munmap(mapped, PAGE_SIZE);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void tcti_switch_executes_ldrsw_sign_extension_from_mapped_mm(struct kunit *test)
{
	const u32 value = 0xffffffcdU;
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	unsigned long fault_address = 0;
	unsigned long mapped;
	int ret;

	KUNIT_ASSERT_NOT_NULL(test, current->mm);
	mapped = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));

	ret = tcti_write_user_data(current->mm, mapped + 24, &value,
				   sizeof(value));
	KUNIT_ASSERT_EQ(test, 0, ret);

	regs.regs[2] = mapped;
	regs.regs[8] = 0;
	regs.pc = 0x8e40;
	decoded = tcti_decode_aarch64(0xb9801848U);

	ret = tcti_switch_debug_execute_decoded(current->mm, &regs, &decoded,
						&fault_address);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, mapped + 24, fault_address);
	KUNIT_EXPECT_EQ(test, 0xffffffffffffffcdULL, regs.regs[8]);
	KUNIT_EXPECT_EQ(test, 0x8e44ULL, regs.pc);

	ret = vm_munmap(mapped, PAGE_SIZE);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void tcti_switch_stores_simd_s_register_to_mapped_mm(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	unsigned long fault_address = 0;
	unsigned long mapped;
	u32 observed = 0;
	int ret;

	KUNIT_ASSERT_NOT_NULL(test, current->mm);
	mapped = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));

	current->thread.user_simd[0] = 0x8877665544332211ULL;
	current->thread.user_simd[1] = 0;
	current->thread.user_simd_valid = 1;
	regs.regs[19] = mapped;
	regs.pc = 0x8e60;
	decoded = tcti_decode_aarch64(0xbd01c260U);

	ret = tcti_switch_debug_execute_decoded(current->mm, &regs, &decoded,
						&fault_address);
	KUNIT_ASSERT_EQ(test, 0, ret);
	ret = tcti_read_user_data(current->mm, mapped + 0x1c0, &observed,
				  sizeof(observed));

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x44332211U, observed);
	KUNIT_EXPECT_EQ(test, mapped + 0x1c0, fault_address);
	KUNIT_EXPECT_EQ(test, 0x8e64ULL, regs.pc);

	ret = vm_munmap(mapped, PAGE_SIZE);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void tcti_switch_executes_mlibc_cpuset_count_small_loop(struct kunit *test)
{
	static const u32 instructions[] = {
		0x6f00e400U, /* movi v0.2d, #0 */
		0xaa0903eaU, /* mov x10, x9 */
		0x927ef409U, /* and x9, x0, #0xfffffffffffffffc */
		0x4e041d00U, /* mov v0.s[0], w8 */
		0xcb090148U, /* sub x8, x10, x9 */
		0x8b0a002aU, /* add x10, x1, x10 */
		0xbc404541U, /* ldr s1, [x10], #0x4 */
		0xb1001108U, /* adds x8, x8, #0x4 */
		0x2f08a421U, /* ushll v1.8h, v1.8b, #0 */
		0x2f10a421U, /* ushll v1.4s, v1.4h, #0 */
		0x4ea11c22U, /* mov v2.16b, v1.16b */
		0x6f3f0423U, /* ushr v3.4s, v1.4s, #1 */
		0x6f3e0424U, /* ushr v4.4s, v1.4s, #2 */
		0x6f3d0425U, /* ushr v5.4s, v1.4s, #3 */
		0x6f3c0426U, /* ushr v6.4s, v1.4s, #4 */
		0x6f3b0427U, /* ushr v7.4s, v1.4s, #5 */
		0x6f0717c2U, /* bic v2.4s, #0xfe */
		0x6f0317c3U, /* bic v3.4s, #0x7e */
		0x6f0117c4U, /* bic v4.4s, #0x3e */
		0x6f0017c5U, /* bic v5.4s, #0x1e */
		0x6f0015c6U, /* bic v6.4s, #0xe */
		0x6f0014c7U, /* bic v7.4s, #0x6 */
		0x4ea08440U, /* add v0.4s, v2.4s, v0.4s */
		0x6f3a0422U, /* ushr v2.4s, v1.4s, #6 */
		0x4ea08460U, /* add v0.4s, v3.4s, v0.4s */
		0x4ea484a3U, /* add v3.4s, v5.4s, v4.4s */
		0x4ea684e4U, /* add v4.4s, v7.4s, v6.4s */
		0x6f001442U, /* bic v2.4s, #0x2 */
		0x4ea08460U, /* add v0.4s, v3.4s, v0.4s */
		0x4ea48442U, /* add v2.4s, v2.4s, v4.4s */
		0x4ea08440U, /* add v0.4s, v2.4s, v0.4s */
		0x6f391420U, /* usra v0.4s, v1.4s, #7 */
		0x54fffcc1U, /* b.ne back to ldr s1 */
		0x4eb1b800U, /* addv s0, v0.4s */
		0xeb09001fU, /* cmp x0, x9 */
		0x1e260008U, /* fmov w8, s0 */
	};
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	u8 cpuset[8] = {};
	unsigned long mapped;
	unsigned long base = 0x8e40;
	int ret;
	u32 step;

	KUNIT_ASSERT_NOT_NULL(test, current->mm);
	mapped = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));

	cpuset[1] = 0x08;
	ret = tcti_write_user_data(current->mm, mapped, cpuset, sizeof(cpuset));
	KUNIT_ASSERT_EQ(test, 0, ret);

	regs.regs[0] = sizeof(cpuset);
	regs.regs[1] = mapped;
	regs.regs[8] = 0;
	regs.regs[9] = 0;
	regs.pc = base;
	current->thread.user_simd_valid = 0;

	for (step = 0; step < 96 && regs.pc != base + sizeof(instructions);
	     step++) {
		unsigned long offset = regs.pc - base;
		u32 index = offset / sizeof(u32);

		KUNIT_ASSERT_EQ(test, 0UL, offset % sizeof(u32));
		KUNIT_ASSERT_LT(test, index, (u32)ARRAY_SIZE(instructions));
		decoded = tcti_decode_aarch64(instructions[index]);
		KUNIT_ASSERT_NE(test, TCTI_DECODE_UNSUPPORTED,
				decoded.decode_class);
		ret = tcti_switch_debug_execute_decoded(current->mm, &regs,
							&decoded, NULL);
		KUNIT_ASSERT_EQ(test, 0, ret);
	}

	KUNIT_EXPECT_LT(test, step, 96U);
	KUNIT_EXPECT_EQ(test, 1ULL, regs.regs[8]);
	KUNIT_EXPECT_EQ(test, 8ULL, regs.regs[9]);
	KUNIT_EXPECT_EQ(test, mapped + 8, regs.regs[10]);
	KUNIT_EXPECT_EQ(test, 1ULL, current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, base + sizeof(instructions), regs.pc);

	ret = vm_munmap(mapped, PAGE_SIZE);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void tcti_switch_executes_fmov_w_s(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	int ret;

	current->thread.user_simd[10] = 0x1122334455667788ULL;
	current->thread.user_simd[11] = 0xaabbccddeeff0011ULL;
	regs.regs[11] = 0xffffffffffffffffULL;
	regs.pc = 0x8a00;

	decoded = tcti_decode_aarch64(0x1e2600abU);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x55667788ULL, regs.regs[11]);
	KUNIT_EXPECT_EQ(test, 0xaabbccddeeff0011ULL,
			current->thread.user_simd[11]);
	KUNIT_EXPECT_EQ(test, 0x8a04ULL, regs.pc);

	regs.regs[8] = 0x428a0000ULL;
	decoded = tcti_decode_aarch64(0x1e270100U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x428a0000ULL, current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, 0x8a08ULL, regs.pc);

	current->thread.user_simd[0] = 0x7ff8000000000000ULL;
	decoded = tcti_decode_aarch64(0x1e604001U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x7ff8000000000000ULL,
			current->thread.user_simd[2]);
	KUNIT_EXPECT_EQ(test, 0x8a0cULL, regs.pc);

	decoded = tcti_decode_aarch64(0x9e66003cU);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x7ff8000000000000ULL, regs.regs[28]);
	KUNIT_EXPECT_EQ(test, 0x8a10ULL, regs.pc);

	current->thread.user_simd[2] = 0xbff0000000000000ULL;
	decoded = tcti_decode_aarch64(0x1e60c020U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x3ff0000000000000ULL,
			current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, 0x8a14ULL, regs.pc);

	decoded = tcti_decode_aarch64(0x1e602000U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_TRUE(test, regs.pstate & PSR_Z_BIT);
	KUNIT_EXPECT_TRUE(test, regs.pstate & PSR_C_BIT);
	KUNIT_EXPECT_EQ(test, 0x8a18ULL, regs.pc);

	current->thread.user_simd[0] = 0xbff0000000000000ULL;
	regs.pc = 0x8b00;
	decoded = tcti_decode_aarch64(0x1e602008U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_TRUE(test, regs.pstate & PSR_N_BIT);
	KUNIT_EXPECT_FALSE(test, regs.pstate & PSR_Z_BIT);
	KUNIT_EXPECT_FALSE(test, regs.pstate & PSR_C_BIT);
	KUNIT_EXPECT_EQ(test, 0x8b04ULL, regs.pc);

	regs.pc = 0x8a18;
	current->thread.user_simd[0] = 0x428a0000ULL;
	current->thread.user_simd[2] = 0;
	current->thread.user_fpsr = 0;
	decoded = tcti_decode_aarch64(0x1e211800U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x7f800000ULL, current->thread.user_simd[0]);
	KUNIT_EXPECT_TRUE(test, current->thread.user_fpsr & BIT(1));
	KUNIT_EXPECT_EQ(test, 0x8a1cULL, regs.pc);

	current->thread.user_simd[0] = 0x40c00000ULL;
	current->thread.user_simd[20] = 0x40000000ULL;
	current->thread.user_fpsr = 0;
	decoded = tcti_decode_aarch64(0x1e2a1800U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x40400000ULL, current->thread.user_simd[0]);
	KUNIT_EXPECT_FALSE(test, current->thread.user_fpsr & BIT(1));
	KUNIT_EXPECT_EQ(test, 0x8a20ULL, regs.pc);

	current->thread.user_simd[16] = 0x3f800000ULL;
	current->thread.user_simd[0] = 0x40000000ULL;
	current->thread.user_fpsr = 0;
	decoded = tcti_decode_aarch64(0x1e202908U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x40400000ULL, current->thread.user_simd[16]);
	KUNIT_EXPECT_FALSE(test, current->thread.user_fpsr & BIT(4));
	KUNIT_EXPECT_EQ(test, 0x8a24ULL, regs.pc);

	current->thread.user_simd[0] = 0x40c00000ULL;
	current->thread.user_simd[2] = 0x40000000ULL;
	decoded = tcti_decode_aarch64(0x1e213800U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x40800000ULL, current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, 0x8a28ULL, regs.pc);

	regs.pc = 0x8a1c;
	current->thread.user_simd[0] = 0x3ffc0000ULL;
	current->thread.user_simd[2] = 0x4b000000ULL;
	current->thread.user_fpcr = BIT(22);
	current->thread.user_fpsr = 0;
	decoded = tcti_decode_aarch64(0x1e212800U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x4b000002ULL, current->thread.user_simd[0]);
	KUNIT_EXPECT_TRUE(test, current->thread.user_fpsr & BIT(4));
	KUNIT_EXPECT_EQ(test, 0x8a20ULL, regs.pc);

	current->thread.user_simd[0] = 0x3ffc0000ULL;
	current->thread.user_simd[2] = 0x4b000000ULL;
	current->thread.user_fpcr = BIT(23);
	current->thread.user_fpsr = 0;
	decoded = tcti_decode_aarch64(0x1e212800U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x4b000001ULL, current->thread.user_simd[0]);
	KUNIT_EXPECT_TRUE(test, current->thread.user_fpsr & BIT(4));
	KUNIT_EXPECT_EQ(test, 0x8a24ULL, regs.pc);

	current->thread.user_fpcr = 0;
	current->thread.user_simd[0] = 0x4b000002ULL;
	current->thread.user_simd[2] = 0x4b000000ULL;
	decoded = tcti_decode_aarch64(0x1e213800U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x40000000ULL, current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, 0x8a28ULL, regs.pc);

	current->thread.user_simd[0] = 0x4b000001ULL;
	current->thread.user_simd[2] = 0x4b000000ULL;
	decoded = tcti_decode_aarch64(0x1e213800U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x3f800000ULL, current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, 0x8a2cULL, regs.pc);

	decoded = tcti_decode_aarch64(0x1e201001U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x40000000ULL, current->thread.user_simd[2]);
	KUNIT_EXPECT_EQ(test, 0x8a30ULL, regs.pc);

	decoded = tcti_decode_aarch64(0x1e649000U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x4024000000000000ULL,
			current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, 0x8a34ULL, regs.pc);

	current->thread.user_simd[0] = 0x4018000000000000ULL;
	current->thread.user_simd[4] = 0x4024000000000000ULL;
	decoded = tcti_decode_aarch64(0x1e620802U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x404e000000000000ULL,
			current->thread.user_simd[4]);
	KUNIT_EXPECT_EQ(test, 0x8a38ULL, regs.pc);

	current->thread.user_simd[4] = 0x4018000000000000ULL;
	current->thread.user_simd[6] = 0x4004000000000000ULL;
	decoded = tcti_decode_aarch64(0x1e632840U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x4021000000000000ULL,
			current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, 0x8a3cULL, regs.pc);

	decoded = tcti_decode_aarch64(0x1e623865U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0xc00c000000000000ULL,
			current->thread.user_simd[10]);
	KUNIT_EXPECT_EQ(test, 0x8a40ULL, regs.pc);

	current->thread.user_simd[4] = 0x3ff8000000000000ULL;
	current->thread.user_simd[6] = 0x3ff4000000040000ULL;
	decoded = tcti_decode_aarch64(0x1e623865U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0xbfcfffffffe00000ULL,
			current->thread.user_simd[10]);
	KUNIT_EXPECT_EQ(test, 0x8a44ULL, regs.pc);

	current->thread.user_simd[0] = 0x3ff8000000000000ULL;
	current->thread.user_simd[8] = 0x4008000000000000ULL;
	decoded = tcti_decode_aarch64(0x1e601884U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x4000000000000000ULL,
			current->thread.user_simd[8]);
	KUNIT_EXPECT_EQ(test, 0x8a48ULL, regs.pc);

	current->thread.user_simd[16] = 0x40000000ULL;
	current->thread.user_simd[18] = 0x40400000ULL;
	decoded = tcti_decode_aarch64(0x1e290900U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x40c00000ULL,
			current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, 0x8a4cULL, regs.pc);

	current->thread.user_simd[12] = 0x4000000000000000ULL;
	current->thread.user_simd[14] = 0x4010000000000000ULL;
	current->thread.user_simd[32] = 0x4008000000000000ULL;
	decoded = tcti_decode_aarch64(0x1f501cc7U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x4024000000000000ULL,
			current->thread.user_simd[14]);
	KUNIT_EXPECT_EQ(test, 0x8a50ULL, regs.pc);

	current->thread.user_simd[10] = 0x4024000000000000ULL;
	current->thread.user_simd[32] = 0x4000000000000000ULL;
	current->thread.user_simd[34] = 0x4008000000000000ULL;
	decoded = tcti_decode_aarch64(0x1f509623U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x4010000000000000ULL,
			current->thread.user_simd[6]);
	KUNIT_EXPECT_EQ(test, 0x8a54ULL, regs.pc);

	regs.regs[8] = 42;
	decoded = tcti_decode_aarch64(0x1e220101U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x42280000ULL, current->thread.user_simd[2]);
	KUNIT_EXPECT_EQ(test, 0x8a58ULL, regs.pc);

	regs.regs[8] = 67;
	decoded = tcti_decode_aarch64(0x1e230101U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x42860000ULL, current->thread.user_simd[2]);
	KUNIT_EXPECT_EQ(test, 0x8a5cULL, regs.pc);

	regs.pc = 0x8a58;
	current->thread.user_simd[16] = 0x42280000ULL;
	decoded = tcti_decode_aarch64(0x1e214100U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0xc2280000ULL, current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, 0x8a5cULL, regs.pc);

	regs.pc = 0x8a50;
	current->thread.user_simd[0] = 0x3f800000ULL;
	decoded = tcti_decode_aarch64(0x1e22c000U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x3ff0000000000000ULL,
			current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, 0x8a54ULL, regs.pc);

	regs.regs[8] = 6;
	decoded = tcti_decode_aarch64(0x1e620101U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x4018000000000000ULL,
			current->thread.user_simd[2]);
	KUNIT_EXPECT_EQ(test, 0x8a58ULL, regs.pc);

	regs.regs[11] = 42;
	decoded = tcti_decode_aarch64(0x9e630161U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x4045000000000000ULL,
			current->thread.user_simd[2]);
	KUNIT_EXPECT_EQ(test, 0x8a5cULL, regs.pc);

	current->thread.user_simd[0] = 0xc01a800000000000ULL;
	regs.regs[27] = 0;
	decoded = tcti_decode_aarch64(0x1e78001bU);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, (u64)(u32)-6, regs.regs[27]);
	KUNIT_EXPECT_EQ(test, 0x8a60ULL, regs.pc);

	current->thread.user_simd[0] = 0x4018000000000000ULL;
	regs.regs[22] = 0xffffffffffffffffULL;
	decoded = tcti_decode_aarch64(0x9e790016U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 6ULL, regs.regs[22]);
	KUNIT_EXPECT_EQ(test, 0x8a64ULL, regs.pc);

	current->thread.user_simd[0] = 0x3ff8000000000000ULL;
	regs.regs[11] = 0;
	decoded = tcti_decode_aarch64(0x9e59f00bU);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 24ULL, regs.regs[11]);
	KUNIT_EXPECT_EQ(test, 0x8a68ULL, regs.pc);

	current->thread.user_simd[2] = 0xffffffffffffffffULL;
	current->thread.user_simd[3] = 0xffffffffffffffffULL;
	current->thread.user_simd[0] = 0x4018000000000000ULL;

	decoded = tcti_decode_aarch64(0x7ee1b801U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 6ULL, current->thread.user_simd[2]);
	KUNIT_EXPECT_EQ(test, 0ULL, current->thread.user_simd[3]);
	KUNIT_EXPECT_EQ(test, 0x8a6cULL, regs.pc);

	decoded = tcti_decode_aarch64(0x7e61d821U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x4018000000000000ULL, current->thread.user_simd[2]);
	KUNIT_EXPECT_EQ(test, 0ULL, current->thread.user_simd[3]);
	KUNIT_EXPECT_EQ(test, 0x8a70ULL, regs.pc);

	regs.regs[0] = 3;
	regs.regs[3] = 4;
	regs.pstate = PSR_C_BIT;
	decoded = tcti_decode_aarch64(0xfa03001fU);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_TRUE(test, regs.pstate & PSR_N_BIT);
	KUNIT_EXPECT_FALSE(test, regs.pstate & PSR_C_BIT);
	KUNIT_EXPECT_EQ(test, 0x8a74ULL, regs.pc);
}

static void tcti_switch_executes_fcvtzu_w_d(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	int ret;

	current->thread.user_simd[0] = 0x404be00000000000ULL;
	regs.pc = 0x9200;
	decoded = tcti_decode_aarch64(0x1e790008U);

	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 55ULL, regs.regs[8]);
	KUNIT_EXPECT_EQ(test, 0x9204ULL, regs.pc);
}

static void tcti_switch_executes_scvtf_d_d(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	int ret;

	current->thread.user_simd[0] = (u64)-2LL;
	regs.pc = 0x9210;
	decoded = tcti_decode_aarch64(0x5e61d800U);

	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0xc000000000000000ULL,
			current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, 0x9214ULL, regs.pc);
}

static void tcti_switch_executes_ucvtf_2d(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	int ret;

	current->thread.user_simd[0] = 6;
	current->thread.user_simd[1] = 42;
	current->thread.user_simd_valid = 0;
	regs.pc = 0x8e00;

	decoded = tcti_decode_aarch64(0x6e61d800U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x4018000000000000ULL,
			current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, 0x4045000000000000ULL,
			current->thread.user_simd[1]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, 0x8e04ULL, regs.pc);
}

static void tcti_switch_executes_ucvtf_s_x(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	int ret;

	regs.regs[20] = 61;
	regs.pc = 0x8e40;

	decoded = tcti_decode_aarch64(0x9e230280U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x42740000ULL, current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, 0x8e44ULL, regs.pc);

	current->thread.user_simd[0] = 0x5f800000ULL;
	regs.regs[20] = 0;

	decoded = tcti_decode_aarch64(0x9e390014U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, U64_MAX, regs.regs[20]);
	KUNIT_EXPECT_EQ(test, 0x8e48ULL, regs.pc);
}

static void tcti_switch_executes_fmul_2d(struct kunit *test)
{
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	int ret;

	current->thread.user_simd[0] = 0x4018000000000000ULL;
	current->thread.user_simd[1] = 0x4045000000000000ULL;
	current->thread.user_simd[4] = 0x4000000000000000ULL;
	current->thread.user_simd[5] = 0x4008000000000000ULL;
	current->thread.user_simd_valid = 0;
	regs.pc = 0x8f00;

	decoded = tcti_decode_aarch64(0x6e62dc00U);
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x4028000000000000ULL,
			current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, 0x405f800000000000ULL,
			current->thread.user_simd[1]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, 0x8f04ULL, regs.pc);
}

static void tcti_gadget_program_executes_simd_movi_2d_zero(struct kunit *test)
{
	struct tcti_gadget_word program[TCTI_SINGLE_INSTRUCTION_PROGRAM_WORDS];
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	unsigned long fault_address = 0;
	size_t word_count = 0;
	int ret;

	current->thread.user_simd[0] = 0x123456789abcdef0ULL;
	current->thread.user_simd[1] = 0xfedcba9876543210ULL;
	current->thread.user_simd_valid = 0;
	regs.pc = 0x8500;

	decoded = tcti_decode_aarch64(0x6f00e400U);
	ret = tcti_lower_decoded_instruction(&decoded, program,
					     ARRAY_SIZE(program),
					     &word_count);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, TCTI_SINGLE_INSTRUCTION_PROGRAM_WORDS,
			word_count);

	ret = tcti_execute_gadget_program(NULL, &regs, program, word_count,
					  &fault_address);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0ULL, current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, 0ULL, current->thread.user_simd[1]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, 0x8504ULL, regs.pc);
}

static void tcti_gadget_program_executes_simd_ushll2_4s(struct kunit *test)
{
	struct tcti_gadget_word program[TCTI_SINGLE_INSTRUCTION_PROGRAM_WORDS];
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	unsigned long fault_address = 0;
	size_t word_count = 0;
	int ret;

	current->thread.user_simd[8] = 0x3333444455556666ULL;
	current->thread.user_simd[9] = 0x000a000800040001ULL;
	current->thread.user_simd_valid = 0;
	regs.pc = 0x8700;

	decoded = tcti_decode_aarch64(0x6f10a485U);
	ret = tcti_lower_decoded_instruction(&decoded, program,
					     ARRAY_SIZE(program),
					     &word_count);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, TCTI_SINGLE_INSTRUCTION_PROGRAM_WORDS,
			word_count);

	ret = tcti_execute_gadget_program(NULL, &regs, program, word_count,
					  &fault_address);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x0000000400000001ULL,
			current->thread.user_simd[10]);
	KUNIT_EXPECT_EQ(test, 0x0000000a00000008ULL,
			current->thread.user_simd[11]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, 0x8704ULL, regs.pc);
}

static void tcti_gadget_program_executes_simd_ushll2_2d(struct kunit *test)
{
	struct tcti_gadget_word program[TCTI_SINGLE_INSTRUCTION_PROGRAM_WORDS];
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	unsigned long fault_address = 0;
	size_t word_count = 0;
	int ret;

	current->thread.user_simd[10] = 0x3333333344444444ULL;
	current->thread.user_simd[11] = 0x0000000a00000001ULL;
	current->thread.user_simd_valid = 0;
	regs.pc = 0x8800;

	decoded = tcti_decode_aarch64(0x6f20a4a6U);
	ret = tcti_lower_decoded_instruction(&decoded, program,
					     ARRAY_SIZE(program),
					     &word_count);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, TCTI_SINGLE_INSTRUCTION_PROGRAM_WORDS,
			word_count);

	ret = tcti_execute_gadget_program(NULL, &regs, program, word_count,
					  &fault_address);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x0000000000000001ULL,
			current->thread.user_simd[12]);
	KUNIT_EXPECT_EQ(test, 0x000000000000000aULL,
			current->thread.user_simd[13]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, 0x8804ULL, regs.pc);
}

static void tcti_gadget_program_executes_simd_cmeq_8b(struct kunit *test)
{
	struct tcti_gadget_word program[TCTI_SINGLE_INSTRUCTION_PROGRAM_WORDS];
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	unsigned long fault_address = 0;
	size_t word_count = 0;
	int ret;

	current->thread.user_simd[8] = 0x0a090a070a050a03ULL;
	current->thread.user_simd[9] = 0x1111111111111111ULL;
	current->thread.user_simd[16] = 0x0a0a0a0a0a0a0a0aULL;
	current->thread.user_simd[17] = 0x2222222222222222ULL;
	current->thread.user_simd_valid = 0;
	regs.pc = 0x8600;

	decoded = tcti_decode_aarch64(0x2e288c84U);
	ret = tcti_lower_decoded_instruction(&decoded, program,
					     ARRAY_SIZE(program),
					     &word_count);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, TCTI_SINGLE_INSTRUCTION_PROGRAM_WORDS,
			word_count);

	ret = tcti_execute_gadget_program(NULL, &regs, program, word_count,
					  &fault_address);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0xff00ff00ff00ff00ULL,
			current->thread.user_simd[8]);
	KUNIT_EXPECT_EQ(test, 0ULL, current->thread.user_simd[9]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, 0x8604ULL, regs.pc);
}
#endif

static int tcti_decode_test_init(struct kunit *test)
{
	struct mm_struct *mm;

	mm = mm_alloc();
	if (!mm)
		return -ENOMEM;
	kthread_use_mm(mm);
	test->priv = mm;
	return 0;
}

static void tcti_decode_test_exit(struct kunit *test)
{
	struct mm_struct *mm = test->priv;

	kthread_unuse_mm(mm);
	mmput(mm);
}

static struct kunit_case tcti_decode_test_cases[] = {
	KUNIT_CASE(tcti_decode_recognizes_svc_zero),
	KUNIT_CASE(tcti_decode_rejects_unknown_instruction),
	KUNIT_CASE(tcti_decode_rejects_brk_one),
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
	KUNIT_CASE(tcti_decode_ldrsw_signed_immediate_writes_x_register),
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
	KUNIT_CASE(tcti_decode_recognizes_simd_modified_immediates),
	KUNIT_CASE(tcti_decode_recognizes_basenc_simd_immediates),
	KUNIT_CASE(tcti_decode_recognizes_complete_simd_modified_immediate_family),
	KUNIT_CASE(tcti_decode_recognizes_complete_simd_dup_gpr_family),
	KUNIT_CASE(tcti_decode_recognizes_simd_dup_4h_gpr),
	KUNIT_CASE(tcti_decode_recognizes_simd_dup_2d_gpr),
	KUNIT_CASE(tcti_decode_recognizes_simd_mov_d1_d0),
	KUNIT_CASE(tcti_decode_recognizes_complete_simd_xtn_family),
	KUNIT_CASE(tcti_decode_recognizes_simd_xtn_4h),
	KUNIT_CASE(tcti_decode_recognizes_simd_ushll_8h),
	KUNIT_CASE(tcti_decode_recognizes_complete_simd_shift_left_long_family),
	KUNIT_CASE(tcti_decode_recognizes_simd_and_16b),
	KUNIT_CASE(tcti_decode_recognizes_simd_orr_4s_immediate),
	KUNIT_CASE(tcti_decode_recognizes_simd_cmeq_4s),
	KUNIT_CASE(tcti_decode_recognizes_simd_ld1r_4s),
	KUNIT_CASE(tcti_decode_recognizes_simd_ext_16b),
	KUNIT_CASE(tcti_decode_recognizes_complete_simd_permute_family),
	KUNIT_CASE(tcti_decode_recognizes_simd_uzp1_4h),
	KUNIT_CASE(tcti_decode_recognizes_simd_umov_w_h0),
	KUNIT_CASE(tcti_decode_recognizes_simd_umov_w_h1),
	KUNIT_CASE(tcti_decode_recognizes_simd_umov_x_d1),
	KUNIT_CASE(tcti_decode_recognizes_simd_ins_gpr_s0),
	KUNIT_CASE(tcti_decode_recognizes_simd_umaxv_4s),
	KUNIT_CASE(tcti_decode_recognizes_simd_umaxv_4h),
	KUNIT_CASE(tcti_decode_recognizes_simd_addv_4s),
	KUNIT_CASE(tcti_decode_recognizes_simd_addp_d_2d),
	KUNIT_CASE(tcti_decode_recognizes_simd_add_2d),
	KUNIT_CASE(tcti_decode_recognizes_simd_sub_2d),
	KUNIT_CASE(tcti_decode_recognizes_simd_fneg_2d),
	KUNIT_CASE(tcti_decode_recognizes_simd_cmhi_2d),
	KUNIT_CASE(tcti_decode_recognizes_simd_ushl_4s),
	KUNIT_CASE(tcti_decode_recognizes_fmov_w_s),
	KUNIT_CASE(tcti_decode_recognizes_fp_scalar_runtime_operations),
	KUNIT_CASE(tcti_decode_recognizes_add_sub_with_carry),
	KUNIT_CASE(tcti_decode_recognizes_fcvtzu_w_d),
	KUNIT_CASE(tcti_decode_recognizes_scvtf_d_d),
	KUNIT_CASE(tcti_gadget_program_lowers_hint_as_data_stream),
	KUNIT_CASE(tcti_gadget_program_executes_extract),
	KUNIT_CASE(tcti_gadget_program_rejects_svc_lowering),
#if defined(ORLIX_APP_HOSTED_BOOT)
	KUNIT_CASE(tcti_gadget_program_executes_simd_movi_2d_zero),
#endif
	KUNIT_CASE(tcti_gadget_program_executes_init001_movz_prefix),
	KUNIT_CASE(tcti_gadget_program_matches_switch_debug_init001_movz_prefix),
	KUNIT_CASE(tcti_syscall_handoff_uses_guest_x8_and_advances_pc),
	KUNIT_CASE(tcti_resume_user_reports_syscall_and_register_state),
	KUNIT_CASE(tcti_resume_user_reports_fetch_fault),
	KUNIT_CASE(tcti_resume_user_reports_read_fault),
	KUNIT_CASE(tcti_resume_user_reports_write_fault),
	KUNIT_CASE(tcti_resume_user_reports_unsupported_instruction),
	KUNIT_CASE(tcti_resume_user_executes_branch_sequence),
#if defined(ORLIX_APP_HOSTED_BOOT)
	KUNIT_CASE(tcti_resume_user_updates_tls_register_state),
#endif
	KUNIT_CASE(tcti_resume_user_updates_guest_memory),
	KUNIT_CASE(tcti_successful_execve_return_restores_el0_pstate),
	KUNIT_CASE(tcti_static_pie_initial_tls_uses_pt_tls),
	KUNIT_CASE(tcti_static_pie_relocation_count_is_bounded),
#if defined(ORLIX_APP_HOSTED_BOOT)
	KUNIT_CASE(tcti_prepare_user_entry_accepts_pt_tls_alignment),
#endif
	KUNIT_CASE(tcti_block_cache_returns_cached_program),
	KUNIT_CASE(tcti_gadget_program_executes_multiple_decoded_instructions),
	KUNIT_CASE(tcti_gadget_program_executes_branch_register),
	KUNIT_CASE(tcti_gadget_program_preserves_flags_between_subs_and_branch),
	KUNIT_CASE(tcti_block_cache_is_bounded),
	KUNIT_CASE(tcti_block_cache_invalidation_bumps_generation),
	KUNIT_CASE(tcti_syscall_mapping_changes_include_brk),
	KUNIT_CASE(tcti_tlb_separates_access_classes),
	KUNIT_CASE(tcti_tlb_flushes_on_generation_change),
	KUNIT_CASE(tcti_switch_executes_hint_as_noop),
	KUNIT_CASE(tcti_switch_executes_add_immediate_with_sp_source),
	KUNIT_CASE(tcti_switch_executes_add_sub_immediate_variants),
	KUNIT_CASE(tcti_switch_executes_add_sub_with_carry_flags),
	KUNIT_CASE(tcti_switch_executes_add_sub_shifted_register),
	KUNIT_CASE(tcti_switch_executes_neg_with_xzr_source),
	KUNIT_CASE(tcti_switch_executes_add_sub_extended_register),
	KUNIT_CASE(tcti_switch_executes_pc_relative_address_variants),
	KUNIT_CASE(tcti_switch_executes_unconditional_branch_immediate),
	KUNIT_CASE(tcti_switch_executes_branch_register),
	KUNIT_CASE(tcti_switch_executes_conditional_branches),
	KUNIT_CASE(tcti_switch_cmp_equal_skips_heap_guard_branch),
	KUNIT_CASE(tcti_switch_executes_conditional_compare),
	KUNIT_CASE(tcti_switch_executes_mlibc_float_helper_conditional_compares),
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
#if defined(ORLIX_APP_HOSTED_BOOT)
	KUNIT_CASE(tcti_switch_executes_simd_modified_immediates),
	KUNIT_CASE(tcti_switch_executes_complete_simd_modified_immediate_family),
	KUNIT_CASE(tcti_switch_executes_fmov_d_negative_one_immediate),
	KUNIT_CASE(tcti_switch_executes_basenc_simd_immediates),
	KUNIT_CASE(tcti_switch_executes_complete_simd_dup_gpr_family),
	KUNIT_CASE(tcti_switch_executes_simd_dup_4h_gpr),
	KUNIT_CASE(tcti_switch_executes_simd_dup_2d_gpr),
	KUNIT_CASE(tcti_switch_executes_simd_mov_d1_d0),
	KUNIT_CASE(tcti_switch_executes_complete_simd_xtn_family),
	KUNIT_CASE(tcti_switch_executes_simd_and_16b),
	KUNIT_CASE(tcti_switch_executes_simd_orr_4s_immediate),
	KUNIT_CASE(tcti_switch_executes_simd_cmeq_4s),
	KUNIT_CASE(tcti_switch_executes_complete_simd_permute_family),
	KUNIT_CASE(tcti_switch_executes_simd_uzp1_4h),
	KUNIT_CASE(tcti_switch_executes_simd_umov_w_h0),
	KUNIT_CASE(tcti_switch_executes_simd_umov_w_h1),
	KUNIT_CASE(tcti_switch_executes_simd_umov_x_d1),
	KUNIT_CASE(tcti_switch_executes_simd_ins_gpr_s0),
	KUNIT_CASE(tcti_switch_executes_simd_umaxv_4s),
	KUNIT_CASE(tcti_switch_executes_simd_umaxv_4h),
	KUNIT_CASE(tcti_switch_executes_simd_sub_2d),
	KUNIT_CASE(tcti_switch_executes_simd_fneg_2d),
	KUNIT_CASE(tcti_switch_executes_simd_addv_4s),
	KUNIT_CASE(tcti_switch_executes_simd_addp_d_2d),
	KUNIT_CASE(tcti_switch_executes_simd_ushl_4s),
	KUNIT_CASE(tcti_switch_executes_simd_xtn_4h),
	KUNIT_CASE(tcti_switch_executes_simd_ushll_8h),
	KUNIT_CASE(tcti_switch_executes_complete_simd_shift_left_long_family),
	KUNIT_CASE(tcti_switch_executes_simd_ext_16b),
	KUNIT_CASE(tcti_switch_executes_mlibc_cpuset_popcount_sequence),
	KUNIT_CASE(tcti_switch_executes_mlibc_cpuset_count_from_mapped_mm),
	KUNIT_CASE(tcti_switch_executes_simd_ld1r_4s_from_mapped_mm),
	KUNIT_CASE(tcti_switch_executes_ldrsw_sign_extension_from_mapped_mm),
	KUNIT_CASE(tcti_switch_stores_simd_s_register_to_mapped_mm),
	KUNIT_CASE(tcti_switch_executes_mlibc_cpuset_count_small_loop),
	KUNIT_CASE(tcti_switch_executes_fmov_w_s),
	KUNIT_CASE(tcti_switch_executes_ucvtf_2d),
	KUNIT_CASE(tcti_switch_executes_fcvtzu_w_d),
	KUNIT_CASE(tcti_switch_executes_scvtf_d_d),
	KUNIT_CASE(tcti_switch_executes_ucvtf_s_x),
	KUNIT_CASE(tcti_switch_executes_fmul_2d),
	KUNIT_CASE(tcti_gadget_program_executes_simd_ushll2_4s),
	KUNIT_CASE(tcti_gadget_program_executes_simd_ushll2_2d),
	KUNIT_CASE(tcti_gadget_program_executes_simd_cmeq_8b),
#endif
	{}
};

static struct kunit_suite tcti_decode_test_suite = {
	.name = "orlix-tcti-decode",
	.init = tcti_decode_test_init,
	.exit = tcti_decode_test_exit,
	.test_cases = tcti_decode_test_cases,
};

#if defined(ORLIX_APP_HOSTED_BOOT)
int kunit_run_all_tests(void)
{
	struct kunit_suite *suites[] = { &tcti_decode_test_suite };

	return __kunit_test_suites_init(suites, ARRAY_SIZE(suites));
}
#else
kunit_test_suite(tcti_decode_test_suite);
#endif

MODULE_LICENSE("GPL");
