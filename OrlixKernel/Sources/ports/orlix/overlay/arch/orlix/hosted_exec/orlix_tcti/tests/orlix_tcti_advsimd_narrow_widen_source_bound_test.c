// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/bitfield.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/syscalls.h>
#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/orlix_tcti.h>

#include "../decode_aarch64.h"

/*
 * Pinned AARCHMRS 2026-06 source leaves:
 *
 * XTN_asimdmisc_N:  mask 0xbf3ffc00, pattern 0x0e212800
 * SHLL_asimdmisc_S: mask 0xbf3ffc00, pattern 0x2e213800
 * SSHLL_asimdshf_L: mask 0xbf80fc00, pattern 0x0f00a400
 * USHLL_asimdshf_L: mask 0xbf80fc00, pattern 0x2f00a400
 *
 * These tests execute the production decoder and switch executor. They do not
 * model a substitute instruction path.
 */
#define ORLIX_TCTI_TEST_XTN_MASK		0xbf3ffc00U
#define ORLIX_TCTI_TEST_XTN_PATTERN		0x0e212800U
#define ORLIX_TCTI_TEST_SHLL_MASK		0xbf3ffc00U
#define ORLIX_TCTI_TEST_SHLL_PATTERN		0x2e213800U
#define ORLIX_TCTI_TEST_SHIFT_LONG_MASK	0xbf80fc00U
#define ORLIX_TCTI_TEST_SSHLL_PATTERN		0x0f00a400U
#define ORLIX_TCTI_TEST_USHLL_PATTERN		0x2f00a400U
#define ORLIX_TCTI_TEST_SQXTN_PATTERN		0x0e214800U
#define ORLIX_TCTI_TEST_SQXTUN_PATTERN		0x0e212800U
#define ORLIX_TCTI_TEST_SCALAR_SQXTN_PATTERN	0x5e214800U
#define ORLIX_TCTI_TEST_SCALAR_SQXTUN_PATTERN	0x5e212800U
#define ORLIX_TCTI_TEST_SATURATING_NARROW_MASK	0x9f3ffc00U
#define ORLIX_TCTI_TEST_SCALAR_SATURATING_NARROW_MASK	0xdf3ffc00U
#define ORLIX_TCTI_TEST_FPSR_QC			BIT(27)
#define ORLIX_TCTI_TEST_SVC				0xd4000001U

struct orlix_tcti_advsimd_narrow_widen_leaf {
	u16 source_ordinal;
	const char *source_name;
	const char *source_mnemonic;
	const char *source_operation;
	u32 source_mask;
	u32 source_pattern;
};

struct orlix_tcti_advsimd_narrow_widen_manifest_leaf {
	u32 ordinal;
	const char *id;
	const char *mnemonic;
	const char *operation;
	u32 mask;
	u32 pattern;
};

#define ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE(...)
#define ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW(ordinal, id, mnemonic, operation, mask, \
					     pattern, feature_predicate, offset, length) \
	{ ordinal, id, mnemonic, operation, mask, pattern },
static const struct orlix_tcti_advsimd_narrow_widen_manifest_leaf
	orlix_tcti_advsimd_narrow_widen_manifest[] = {
#include "../isa/source_manifest.def"
};
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE

static const struct orlix_tcti_advsimd_narrow_widen_leaf
	orlix_tcti_advsimd_narrow_widen_leaves[] = {
	{ 3559U, "SQXTN_asisdmisc_N", "SQXTN", "SQXTN_advsimd",
	  0xff3ffc00U, 0x5e214800U },
	{ 3576U, "SQXTUN_asisdmisc_N", "SQXTUN", "SQXTUN_advsimd",
	  0xff3ffc00U, 0x7e212800U },
	{ 3577U, "UQXTN_asisdmisc_N", "UQXTN", "UQXTN_advsimd",
	  0xff3ffc00U, 0x7e214800U },
	{ 3795U, "XTN_asimdmisc_N", "XTN", "XTN_advsimd",
	  0xbf3ffc00U, 0x0e212800U },
	{ 3796U, "SQXTN_asimdmisc_N", "SQXTN", "SQXTN_advsimd",
	  0xbf3ffc00U, 0x0e214800U },
	{ 3827U, "SQXTUN_asimdmisc_N", "SQXTUN", "SQXTUN_advsimd",
	  0xbf3ffc00U, 0x2e212800U },
	{ 3828U, "SHLL_asimdmisc_S", "SHLL", "SHLL_advsimd",
	  0xbf3ffc00U, 0x2e213800U },
	{ 3829U, "UQXTN_asimdmisc_N", "UQXTN", "UQXTN_advsimd",
	  0xbf3ffc00U, 0x2e214800U },
	{ 4005U, "SSHLL_asimdshf_L", "SSHLL", "SSHLL_advsimd",
	  0xbf80fc00U, 0x0f00a400U },
	{ 4020U, "USHLL_asimdshf_L", "USHLL", "USHLL_advsimd",
	  0xbf80fc00U, 0x2f00a400U },
};

static const struct orlix_tcti_advsimd_narrow_widen_leaf *
orlix_tcti_advsimd_narrow_widen_leaf(u32 ordinal)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_advsimd_narrow_widen_leaves);
	     index++)
		if (orlix_tcti_advsimd_narrow_widen_leaves[index].source_ordinal == ordinal)
			return &orlix_tcti_advsimd_narrow_widen_leaves[index];
	return NULL;
}

static const struct orlix_tcti_advsimd_narrow_widen_manifest_leaf *
orlix_tcti_advsimd_narrow_widen_manifest_leaf(u32 ordinal)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_advsimd_narrow_widen_manifest);
	     index++)
		if (orlix_tcti_advsimd_narrow_widen_manifest[index].ordinal == ordinal)
			return &orlix_tcti_advsimd_narrow_widen_manifest[index];
	return NULL;
}

static void orlix_tcti_advsimd_narrow_widen_expect_source_encoding(
	struct kunit *test, u32 ordinal, u32 instruction)
{
	const struct orlix_tcti_advsimd_narrow_widen_leaf *leaf =
		orlix_tcti_advsimd_narrow_widen_leaf(ordinal);

	KUNIT_ASSERT_NOT_NULL(test, leaf);
	KUNIT_EXPECT_EQ(test, leaf->source_pattern,
			instruction & leaf->source_mask);
}

struct orlix_tcti_advsimd_narrow_widen_context {
	struct mm_struct *mm;
	unsigned long instructions;
	u64 simd[ARRAY_SIZE(current->thread.user_simd)];
	unsigned long simd_valid;
	unsigned long fpcr;
	unsigned long fpsr;
};

static void orlix_tcti_advsimd_narrow_widen_source_manifest_is_exact(
	struct kunit *test)
{
	size_t index;

	KUNIT_ASSERT_EQ(test, 10U,
			ARRAY_SIZE(orlix_tcti_advsimd_narrow_widen_leaves));
	for (index = 0; index < ARRAY_SIZE(orlix_tcti_advsimd_narrow_widen_leaves);
	     index++) {
		const struct orlix_tcti_advsimd_narrow_widen_leaf *leaf =
			&orlix_tcti_advsimd_narrow_widen_leaves[index];
		const struct orlix_tcti_advsimd_narrow_widen_manifest_leaf *source =
			orlix_tcti_advsimd_narrow_widen_manifest_leaf(leaf->source_ordinal);

		KUNIT_ASSERT_NOT_NULL(test, source);
		KUNIT_EXPECT_STREQ(test, leaf->source_name, source->id);
		KUNIT_EXPECT_STREQ(test, leaf->source_mnemonic, source->mnemonic);
		KUNIT_EXPECT_STREQ(test, leaf->source_operation, source->operation);
		KUNIT_EXPECT_EQ(test, leaf->source_mask, source->mask);
		KUNIT_EXPECT_EQ(test, leaf->source_pattern, source->pattern);
	}
}

static int orlix_tcti_advsimd_narrow_widen_test_init(struct kunit *test)
{
	struct orlix_tcti_advsimd_narrow_widen_context *context;

	context = kunit_kzalloc(test, sizeof(*context), GFP_KERNEL);
	if (!context)
		return -ENOMEM;

	context->mm = mm_alloc();
	if (!context->mm)
		return -ENOMEM;
	kthread_use_mm(context->mm);
	context->instructions = ksys_mmap_pgoff(0, PAGE_SIZE,
						PROT_READ | PROT_WRITE,
						MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (IS_ERR_VALUE(context->instructions)) {
		kthread_unuse_mm(context->mm);
		mmput(context->mm);
		return -ENOMEM;
	}
	memcpy(context->simd, current->thread.user_simd,
	       sizeof(context->simd));
	context->simd_valid = current->thread.user_simd_valid;
	context->fpcr = current->thread.user_fpcr;
	context->fpsr = current->thread.user_fpsr;
	test->priv = context;
	return 0;
}

static void orlix_tcti_advsimd_narrow_widen_test_exit(struct kunit *test)
{
	struct orlix_tcti_advsimd_narrow_widen_context *context = test->priv;

	if (context->instructions)
		vm_munmap(context->instructions, PAGE_SIZE);
	if (context->mm) {
		kthread_unuse_mm(context->mm);
		mmput(context->mm);
	}
	memcpy(current->thread.user_simd, context->simd,
	       sizeof(context->simd));
	current->thread.user_simd_valid = context->simd_valid;
	current->thread.user_fpcr = context->fpcr;
	current->thread.user_fpsr = context->fpsr;
}

static int orlix_tcti_test_load_instruction(struct kunit *test, u32 instruction)
{
	struct orlix_tcti_advsimd_narrow_widen_context *context = test->priv;
	const u32 program[] = { instruction, ORLIX_TCTI_TEST_SVC };
	int ret;

	ret = sys_mprotect(context->instructions, PAGE_SIZE, PROT_READ | PROT_WRITE);
	if (ret)
		return ret;
	ret = orlix_tcti_write_user_data(current->mm, context->instructions, program,
				   sizeof(program));
	if (ret)
		return ret;
	return sys_mprotect(context->instructions, PAGE_SIZE, PROT_READ | PROT_EXEC);
}

static struct orlix_tcti_result
orlix_tcti_test_resume_instruction(struct kunit *test, struct pt_regs *regs,
				     u32 instruction)
{
	struct orlix_tcti_advsimd_narrow_widen_context *context = test->priv;
	const u32 expected_program[] = { instruction, ORLIX_TCTI_TEST_SVC };
	u32 before_program[2];
	u32 after_program[2];
	struct orlix_tcti_result result;
	int ret;

	ret = orlix_tcti_test_load_instruction(test, instruction);
	KUNIT_EXPECT_EQ(test, 0, ret);
	if (ret)
		return (struct orlix_tcti_result) {
			.status = ret,
		};
	KUNIT_EXPECT_MEMEQ(test, expected_program, before_program,
			   sizeof(expected_program));
	ret = orlix_tcti_read_user_data(current->mm, context->instructions,
				  before_program, sizeof(before_program));
	KUNIT_EXPECT_EQ(test, 0, ret);
	if (ret)
		return (struct orlix_tcti_result) {
			.status = ret,
		};
	regs->pc = context->instructions;
	regs->pstate |= PSR_MODE_EL0t;
	regs->syscallno = NO_SYSCALL;
	result = orlix_tcti_resume_user(current, regs, current->mm);
	ret = orlix_tcti_read_user_data(current->mm, context->instructions,
				  after_program, sizeof(after_program));
	KUNIT_EXPECT_EQ(test, 0, ret);
	if (!ret)
		KUNIT_EXPECT_MEMEQ(test, expected_program, after_program,
				   sizeof(expected_program));
	return result;
}

static void orlix_tcti_test_expect_resume_success(struct kunit *test,
					    const struct orlix_tcti_result *result,
					    const struct pt_regs *before,
					    const struct pt_regs *after)
{
	struct orlix_tcti_advsimd_narrow_widen_context *context = test->priv;
	struct pt_regs expected = *before;

	expected.pc = context->instructions + sizeof(u32);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result->reason);
	KUNIT_EXPECT_EQ(test, 0L, result->status);
	KUNIT_EXPECT_EQ(test, 0UL, result->fault_address);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_ACCESS_FETCH, result->fault_access);
	KUNIT_EXPECT_EQ(test, context->instructions + sizeof(u32), result->pc);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_TEST_SVC, result->instruction);
	KUNIT_EXPECT_MEMEQ(test, &expected, after, sizeof(expected));
}

static void orlix_tcti_test_initialize_registers(struct pt_regs *regs,
					   u32 instruction);

static void orlix_tcti_test_expect_rejected_instruction(struct kunit *test,
						  u32 instruction)
{
	struct orlix_tcti_advsimd_narrow_widen_context *context = test->priv;
	struct pt_regs regs = {};
	struct pt_regs before;
	struct orlix_tcti_result result;
	u64 before_simd[ARRAY_SIZE(current->thread.user_simd)];
	unsigned long before_simd_valid;
	unsigned long before_fpcr;
	unsigned long before_fpsr;

	orlix_tcti_test_initialize_registers(&regs, instruction);
	regs.pc = context->instructions;
	regs.pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_C_BIT;
	regs.syscallno = NO_SYSCALL;
	before = regs;
	memcpy(before_simd, current->thread.user_simd, sizeof(before_simd));
	before_simd_valid = current->thread.user_simd_valid;
	before_fpcr = current->thread.user_fpcr;
	before_fpsr = current->thread.user_fpsr;
	result = orlix_tcti_test_resume_instruction(test, &regs, instruction);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION, result.reason);
	KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, result.status);
	KUNIT_EXPECT_EQ(test, 0UL, result.fault_address);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_ACCESS_FETCH, result.fault_access);
	KUNIT_EXPECT_EQ(test, context->instructions, result.pc);
	KUNIT_EXPECT_EQ(test, instruction, result.instruction);
	KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
	KUNIT_EXPECT_MEMEQ(test, before_simd, current->thread.user_simd,
			   sizeof(before_simd));
	KUNIT_EXPECT_EQ(test, before_simd_valid,
			current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, before_fpcr, current->thread.user_fpcr);
	KUNIT_EXPECT_EQ(test, before_fpsr, current->thread.user_fpsr);
}

static u64 orlix_tcti_test_lane_mask(u8 size)
{
	return size == sizeof(u64) ? ~0ULL : BIT_ULL(size * 8) - 1;
}

static u64 orlix_tcti_test_read_lane(const u64 words[2], u8 size, u8 lane)
{
	u8 byte = lane * size;
	u8 word = byte / sizeof(u64);
	u8 shift = (byte % sizeof(u64)) * 8;

	return (words[word] >> shift) & orlix_tcti_test_lane_mask(size);
}

static void orlix_tcti_test_write_lane(u64 words[2], u8 size, u8 lane, u64 value)
{
	u8 byte = lane * size;
	u8 word = byte / sizeof(u64);
	u8 shift = (byte % sizeof(u64)) * 8;
	u64 mask = orlix_tcti_test_lane_mask(size) << shift;

	words[word] = (words[word] & ~mask) |
		      ((value & orlix_tcti_test_lane_mask(size)) << shift);
}

static void orlix_tcti_test_initialize_registers(struct pt_regs *regs, u32 instruction)
{
	unsigned int index;

	for (index = 0; index < ARRAY_SIZE(regs->regs); index++)
		regs->regs[index] = 0x8192a3b4c5d6e700ULL ^
				    ((u64)instruction << 9) ^ index;
	regs->pc = 0x123456789000ULL;
	regs->sp = 0x23456789a000ULL;
	regs->pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_C_BIT;
	regs->orig_x0 = 0xd1b54a32d192ed03ULL;
	regs->syscallno = NO_SYSCALL;
	regs->unused = 0x6d5a56c3U;

	for (index = 0; index < ARRAY_SIZE(current->thread.user_simd); index++)
		current->thread.user_simd[index] =
			0xfedcba9876543210ULL ^
			((u64)instruction << 13) ^
			(0x0101010101010101ULL * index);
	current->thread.user_fpcr = BIT(22) | BIT(24);
	current->thread.user_fpsr = BIT(4);
}

static void orlix_tcti_test_execute_xtn_case(struct kunit *test, u8 size,
				       bool upper, u8 rn, u8 rd)
{
	u32 instruction = ORLIX_TCTI_TEST_XTN_PATTERN | ((u32)size << 22) |
			  (upper ? BIT(30) : 0) | ((u32)rn << 5) | rd;
	struct orlix_tcti_decoded_instruction decoded =
		orlix_tcti_decode_aarch64(instruction);
	struct pt_regs regs = {};
	struct pt_regs before_regs;
	u64 before_simd[ARRAY_SIZE(current->thread.user_simd)];
	u64 narrowed[2] = {};
	u64 expected_simd[ARRAY_SIZE(current->thread.user_simd)];
	u8 source_size = 2U << size;
	u8 result_size = 1U << size;
	u8 lane_count = 2 * sizeof(u64) / source_size;
	u8 lane;
	struct orlix_tcti_result result;

	orlix_tcti_advsimd_narrow_widen_expect_source_encoding(test, 3795U,
							 instruction);
	KUNIT_ASSERT_EQ(test, ORLIX_TCTI_TEST_XTN_PATTERN,
			instruction & ORLIX_TCTI_TEST_XTN_MASK);
	KUNIT_ASSERT_EQ(test, ORLIX_TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_SIMD_ELEMENT_MOVE_XTN,
			decoded.simd_element_move_op);
	KUNIT_EXPECT_EQ(test, source_size, decoded.access_size);
	KUNIT_EXPECT_EQ(test, result_size, decoded.result_size);
	KUNIT_EXPECT_EQ(test, upper, decoded.simd_destination_index != 0);

	orlix_tcti_test_initialize_registers(&regs, instruction);
	memcpy(before_simd, current->thread.user_simd, sizeof(before_simd));
	memcpy(expected_simd, before_simd, sizeof(expected_simd));
	regs.pc = ((struct orlix_tcti_advsimd_narrow_widen_context *)test->priv)->instructions;
	regs.pstate |= PSR_MODE_EL0t;
	regs.syscallno = NO_SYSCALL;
	before_regs = regs;

	for (lane = 0; lane < lane_count; lane++) {
		u64 source[2] = {
			before_simd[rn * 2],
			before_simd[rn * 2 + 1],
		};
		u64 value = orlix_tcti_test_read_lane(source, source_size, lane);

		orlix_tcti_test_write_lane(narrowed, result_size, lane, value);
	}
	if (upper)
		expected_simd[rd * 2 + 1] = narrowed[0];
	else {
		expected_simd[rd * 2] = narrowed[0];
		expected_simd[rd * 2 + 1] = 0;
	}
	current->thread.user_simd_valid = 0;

	result = orlix_tcti_test_resume_instruction(test, &regs, instruction);

	orlix_tcti_test_expect_resume_success(test, &result, &before_regs, &regs);
	KUNIT_EXPECT_MEMEQ(test, expected_simd, current->thread.user_simd,
			   sizeof(expected_simd));
	KUNIT_EXPECT_EQ(test, 1UL, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, BIT(22) | BIT(24), current->thread.user_fpcr);
	KUNIT_EXPECT_EQ(test, BIT(4), current->thread.user_fpsr);
}

static void orlix_tcti_xtn_source_leaf_executes_all_legal_variants(struct kunit *test)
{
	u8 size;
	u8 upper;
	u8 reg;

	for (size = 0; size < 3; size++) {
		for (upper = 0; upper < 2; upper++) {
			for (reg = 0; reg < 32; reg++)
				orlix_tcti_test_execute_xtn_case(
					test, size, upper, (reg * 7 + 3) & 0x1fU,
					reg);
			orlix_tcti_test_execute_xtn_case(test, size, upper, 17, 17);
		}
	}
}

static void orlix_tcti_xtn_source_leaf_rejects_reserved_size(struct kunit *test)
{
	u8 upper;
	u8 rn;
	u8 rd;

	for (upper = 0; upper < 2; upper++) {
		for (rn = 0; rn < 32; rn++) {
			for (rd = 0; rd < 32; rd += 31) {
				u32 instruction = ORLIX_TCTI_TEST_XTN_PATTERN |
					(3U << 22) | (upper ? BIT(30) : 0) |
					((u32)rn << 5) | rd;

				orlix_tcti_advsimd_narrow_widen_expect_source_encoding(
					test, 3795U, instruction);
				KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
					orlix_tcti_decode_aarch64(instruction).decode_class);
				orlix_tcti_test_expect_rejected_instruction(test, instruction);
			}
		}
	}
}

static void orlix_tcti_test_execute_shift_long_case(struct kunit *test, bool shll,
					      bool sign, bool upper,
					      u8 size, u8 shift,
					      u8 rn, u8 rd)
{
	u8 source_size = BIT(size);
	u8 source_bits = source_size * 8;
	u8 immediate = source_bits + shift;
	u32 pattern = shll ? ORLIX_TCTI_TEST_SHLL_PATTERN :
		      sign ? ORLIX_TCTI_TEST_SSHLL_PATTERN :
			     ORLIX_TCTI_TEST_USHLL_PATTERN;
	u32 instruction = pattern | (upper ? BIT(30) : 0) |
			  (shll ? (u32)size << 22 :
				  (u32)immediate << 16) |
			  ((u32)rn << 5) | rd;
	struct orlix_tcti_decoded_instruction decoded =
		orlix_tcti_decode_aarch64(instruction);
	struct pt_regs regs = {};
	struct pt_regs before_regs;
	u64 before_simd[ARRAY_SIZE(current->thread.user_simd)];
	u64 result_words[2] = {};
	u64 expected_simd[ARRAY_SIZE(current->thread.user_simd)];
	u8 lane_count = sizeof(u64) / source_size;
	u8 lane;
	struct orlix_tcti_result exec_result;
	u32 source_ordinal = shll ? 3828U : sign ? 4005U : 4020U;

	orlix_tcti_advsimd_narrow_widen_expect_source_encoding(test, source_ordinal,
							 instruction);
	KUNIT_ASSERT_EQ(test, pattern, instruction &
			(shll ? ORLIX_TCTI_TEST_SHLL_MASK :
				ORLIX_TCTI_TEST_SHIFT_LONG_MASK));
	KUNIT_ASSERT_EQ(test, ORLIX_TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, !shll && sign ?
				     ORLIX_TCTI_SIMD_ELEMENT_MOVE_SSHLL :
				     ORLIX_TCTI_SIMD_ELEMENT_MOVE_USHLL,
			decoded.simd_element_move_op);
	KUNIT_EXPECT_EQ(test, source_size, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 2 * sizeof(u64), decoded.result_size);
	KUNIT_EXPECT_EQ(test, shll ? source_bits : shift,
			decoded.shift_amount);
	KUNIT_EXPECT_EQ(test, upper ? lane_count : 0,
			decoded.simd_source_index);

	orlix_tcti_test_initialize_registers(&regs, instruction);
	memcpy(before_simd, current->thread.user_simd, sizeof(before_simd));
	memcpy(expected_simd, before_simd, sizeof(expected_simd));
	regs.pc = ((struct orlix_tcti_advsimd_narrow_widen_context *)test->priv)->instructions;
	regs.pstate |= PSR_MODE_EL0t;
	regs.syscallno = NO_SYSCALL;
	before_regs = regs;

	for (lane = 0; lane < lane_count; lane++) {
		u64 source[2] = {
			before_simd[rn * 2],
			before_simd[rn * 2 + 1],
		};
		u64 value = orlix_tcti_test_read_lane(
			source, source_size, (upper ? lane_count : 0) + lane);

		if (!shll && sign)
			value = sign_extend64(value, source_bits - 1);
		orlix_tcti_test_write_lane(result_words, 2 * source_size, lane,
				     value << (shll ? source_bits : shift));
	}
	expected_simd[rd * 2] = result_words[0];
	expected_simd[rd * 2 + 1] = result_words[1];
	current->thread.user_simd_valid = 0;

	exec_result = orlix_tcti_test_resume_instruction(test, &regs, instruction);

	orlix_tcti_test_expect_resume_success(test, &exec_result, &before_regs, &regs);
	KUNIT_EXPECT_MEMEQ(test, expected_simd, current->thread.user_simd,
			   sizeof(expected_simd));
	KUNIT_EXPECT_EQ(test, 1UL, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, BIT(22) | BIT(24), current->thread.user_fpcr);
	KUNIT_EXPECT_EQ(test, BIT(4), current->thread.user_fpsr);
}

static void orlix_tcti_shift_long_source_leaves_execute_all_legal_variants(
	struct kunit *test)
{
	u8 sign;
	u8 upper;
	u8 size;

	for (sign = 0; sign < 2; sign++) {
		for (upper = 0; upper < 2; upper++) {
			for (size = 0; size < 3; size++) {
				u8 source_bits = BIT(size) * 8;
				u8 shift;

				for (shift = 0; shift < source_bits; shift++) {
					u8 reg = shift & 0x1fU;

					orlix_tcti_test_execute_shift_long_case(
						test, false, sign, upper, size, shift,
						(reg * 11 + size) & 0x1fU, reg);
				}
				orlix_tcti_test_execute_shift_long_case(
					test, false, sign, upper, size,
					source_bits - 1,
					23, 23);
			}
		}
	}
}

static void orlix_tcti_shift_long_source_leaves_reject_reserved_immediates(
	struct kunit *test)
{
	static const u8 immediates[] = {
		0x00, /* immh == 0000 */
		0x40, /* immh<3> selects an unsupported 64-bit source */
		0x7f,
	};
	u8 sign;
	u8 upper;
	u8 immediate_index;

	for (sign = 0; sign < 2; sign++) {
		for (upper = 0; upper < 2; upper++) {
			for (immediate_index = 0;
			     immediate_index < ARRAY_SIZE(immediates);
			     immediate_index++) {
				u32 pattern = sign ? ORLIX_TCTI_TEST_SSHLL_PATTERN :
						     ORLIX_TCTI_TEST_USHLL_PATTERN;
				u32 instruction = pattern |
					(upper ? BIT(30) : 0) |
					((u32)immediates[immediate_index] << 16) |
					(9U << 5) | 10U;

				orlix_tcti_advsimd_narrow_widen_expect_source_encoding(
					test, sign ? 4005U : 4020U, instruction);
				KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
					orlix_tcti_decode_aarch64(instruction).decode_class);
				orlix_tcti_test_expect_rejected_instruction(test, instruction);
			}
		}
	}
}

static void orlix_tcti_shll_alias_matches_unsigned_shift_long(struct kunit *test)
{
	u8 upper;
	u8 size;
	u8 reg;

	for (upper = 0; upper < 2; upper++) {
		for (size = 0; size < 3; size++) {
			u32 instruction = ORLIX_TCTI_TEST_SHLL_PATTERN |
				((u32)size << 22) | (upper ? BIT(30) : 0) |
				(13U << 5) | 14U;
			struct orlix_tcti_decoded_instruction decoded =
				orlix_tcti_decode_aarch64(instruction);

			KUNIT_ASSERT_EQ(test, ORLIX_TCTI_TEST_SHLL_PATTERN,
				instruction & ORLIX_TCTI_TEST_SHLL_MASK);
			KUNIT_EXPECT_EQ(test,
				ORLIX_TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE,
				decoded.decode_class);
			KUNIT_EXPECT_EQ(test, ORLIX_TCTI_SIMD_ELEMENT_MOVE_USHLL,
				decoded.simd_element_move_op);
			KUNIT_EXPECT_EQ(test, BIT(size), decoded.access_size);
			KUNIT_EXPECT_EQ(test, BIT(size) * 8,
				decoded.shift_amount);

			reg = size * 7 + upper;
			orlix_tcti_test_execute_shift_long_case(
				test, true, false, upper, size, 0,
				(reg * 11 + size) & 0x1fU, reg);
		}
	}
}

static void orlix_tcti_shll_source_leaf_rejects_reserved_size(struct kunit *test)
{
	u8 upper;
	u8 rn;

	for (upper = 0; upper < 2; upper++) {
		for (rn = 0; rn < 32; rn++) {
			u32 instruction = ORLIX_TCTI_TEST_SHLL_PATTERN |
				(3U << 22) | (upper ? BIT(30) : 0) |
				((u32)rn << 5) | 31U;

			orlix_tcti_advsimd_narrow_widen_expect_source_encoding(test, 3828U,
								 instruction);
			KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
				orlix_tcti_decode_aarch64(instruction).decode_class);
			orlix_tcti_test_expect_rejected_instruction(test, instruction);
		}
	}
}

static u64 orlix_tcti_test_saturating_narrow_lane(
	enum orlix_tcti_simd_vector_arithmetic_op operation, u8 source_size, u64 value,
	bool *saturated)
{
	u8 result_bits = source_size * 4;
	u64 result_mask = GENMASK_ULL(result_bits - 1, 0);
	s64 signed_value;
	s64 minimum;
	s64 maximum;

	if (operation == ORLIX_TCTI_SIMD_ARITH_UQXTN) {
		if (value > result_mask) {
			*saturated = true;
			return result_mask;
		}
		return value;
	}

	signed_value = source_size == sizeof(u64) ? (s64)value :
		sign_extend64(value, source_size * 8 - 1);
	minimum = operation == ORLIX_TCTI_SIMD_ARITH_SQXTN ?
		-(s64)BIT_ULL(result_bits - 1) : 0;
	maximum = operation == ORLIX_TCTI_SIMD_ARITH_SQXTN ?
		(s64)BIT_ULL(result_bits - 1) - 1 : (s64)result_mask;
	if (signed_value < minimum) {
		*saturated = true;
		return (u64)minimum & result_mask;
	}
	if (signed_value > maximum) {
		*saturated = true;
		return (u64)maximum;
	}
	return (u64)signed_value & result_mask;
}

static u64 orlix_tcti_test_saturating_narrow_input(
	enum orlix_tcti_simd_vector_arithmetic_op operation, u8 source_size, u8 lane)
{
	u8 result_bits = source_size * 4;
	u64 result_mask = GENMASK_ULL(result_bits - 1, 0);
	u64 signed_min = BIT_ULL(result_bits - 1);
	u64 signed_max = signed_min - 1;

	switch (lane & 3U) {
	case 0:
		return operation == ORLIX_TCTI_SIMD_ARITH_SQXTN ? signed_max :
			operation == ORLIX_TCTI_SIMD_ARITH_SQXTUN ? 0 : result_mask;
	case 1:
		return operation == ORLIX_TCTI_SIMD_ARITH_SQXTN ? signed_max + 1 :
			result_mask + 1;
	case 2:
		return operation == ORLIX_TCTI_SIMD_ARITH_SQXTN ?
			-(s64)signed_min : -1;
	default:
		return operation == ORLIX_TCTI_SIMD_ARITH_SQXTN ?
			-(s64)signed_min - 1 : 1;
	}
}

static u32 orlix_tcti_test_saturating_narrow_source_ordinal(
	enum orlix_tcti_simd_vector_arithmetic_op operation, bool scalar)
{
	if (operation == ORLIX_TCTI_SIMD_ARITH_SQXTN)
		return scalar ? 3559U : 3796U;
	if (operation == ORLIX_TCTI_SIMD_ARITH_UQXTN)
		return scalar ? 3577U : 3829U;
	return scalar ? 3576U : 3827U;
}

static void orlix_tcti_test_execute_saturating_narrow_case(
	struct kunit *test, enum orlix_tcti_simd_vector_arithmetic_op operation,
	bool scalar, bool upper, u8 size, u8 rn, u8 rd)
{
	u32 pattern;
	u32 instruction;
	struct orlix_tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	struct pt_regs before_regs;
	u64 before_simd[ARRAY_SIZE(current->thread.user_simd)];
	u64 expected_simd[ARRAY_SIZE(current->thread.user_simd)];
	u64 source[2] = {};
	u64 narrowed = 0;
	u8 source_size = 2U << size;
	u8 result_size = source_size / 2;
	u8 lane_count = scalar ? 1 : 2 * sizeof(u64) / source_size;
	u8 lane;
	bool saturated = false;
	struct orlix_tcti_result exec_result;

	pattern = scalar ? ORLIX_TCTI_TEST_SCALAR_SQXTN_PATTERN :
		ORLIX_TCTI_TEST_SQXTN_PATTERN;
	if (operation == ORLIX_TCTI_SIMD_ARITH_SQXTUN)
		pattern = scalar ? ORLIX_TCTI_TEST_SCALAR_SQXTUN_PATTERN :
			ORLIX_TCTI_TEST_SQXTUN_PATTERN;
	instruction = pattern | ((u32)size << 22) |
		(operation == ORLIX_TCTI_SIMD_ARITH_UQXTN ? BIT(29) : 0) |
		(!scalar && upper ? BIT(30) : 0) | ((u32)rn << 5) | rd;
	decoded = orlix_tcti_decode_aarch64(instruction);

	orlix_tcti_advsimd_narrow_widen_expect_source_encoding(test,
		orlix_tcti_test_saturating_narrow_source_ordinal(operation, scalar),
		instruction);
	KUNIT_ASSERT_EQ(test, pattern,
		instruction & (scalar ? ORLIX_TCTI_TEST_SCALAR_SATURATING_NARROW_MASK :
			ORLIX_TCTI_TEST_SATURATING_NARROW_MASK));
	KUNIT_ASSERT_EQ(test, ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC,
		decoded.decode_class);
	KUNIT_EXPECT_EQ(test, operation, decoded.simd_arithmetic_op);
	KUNIT_EXPECT_EQ(test, source_size, decoded.access_size);
	KUNIT_EXPECT_EQ(test, scalar ? result_size :
		upper ? 2 * sizeof(u64) : sizeof(u64), decoded.result_size);
	KUNIT_EXPECT_EQ(test, scalar ? 0 : upper,
		decoded.simd_destination_index);
	KUNIT_EXPECT_EQ(test, scalar, decoded.simd_scalar);

	orlix_tcti_test_initialize_registers(&regs, instruction);
	for (lane = 0; lane < 2 * sizeof(u64) / source_size; lane++)
		orlix_tcti_test_write_lane(source, source_size, lane,
			orlix_tcti_test_saturating_narrow_input(operation, source_size, lane));
	current->thread.user_simd[rn * 2] = source[0];
	current->thread.user_simd[rn * 2 + 1] = source[1];
	memcpy(before_simd, current->thread.user_simd, sizeof(before_simd));
	memcpy(expected_simd, before_simd, sizeof(expected_simd));
	regs.pc = ((struct orlix_tcti_advsimd_narrow_widen_context *)test->priv)->instructions;
	regs.pstate |= PSR_MODE_EL0t;
	regs.syscallno = NO_SYSCALL;
	before_regs = regs;
	for (lane = 0; lane < lane_count; lane++) {
		u64 value = orlix_tcti_test_read_lane(source, source_size, lane);
		u64 result = orlix_tcti_test_saturating_narrow_lane(operation,
			source_size, value, &saturated);

		narrowed |= result << (lane * result_size * 8);
	}
	if (scalar) {
		expected_simd[rd * 2] = narrowed;
		expected_simd[rd * 2 + 1] = 0;
	} else if (upper) {
		expected_simd[rd * 2 + 1] = narrowed;
	} else {
		expected_simd[rd * 2] = narrowed;
		expected_simd[rd * 2 + 1] = 0;
	}
	current->thread.user_simd_valid = 0;
	current->thread.user_fpsr = BIT(5);

	exec_result = orlix_tcti_test_resume_instruction(test, &regs, instruction);

	orlix_tcti_test_expect_resume_success(test, &exec_result, &before_regs, &regs);
	KUNIT_EXPECT_MEMEQ(test, expected_simd, current->thread.user_simd,
			   sizeof(expected_simd));
	KUNIT_EXPECT_EQ(test, 1UL, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, BIT(22) | BIT(24), current->thread.user_fpcr);
	KUNIT_EXPECT_EQ(test, BIT(5) | (saturated ? ORLIX_TCTI_TEST_FPSR_QC : 0),
			current->thread.user_fpsr);
}

static void orlix_tcti_saturating_narrow_source_leaves_execute_all_legal_variants(
	struct kunit *test)
{
	static const enum orlix_tcti_simd_vector_arithmetic_op operations[] = {
		ORLIX_TCTI_SIMD_ARITH_SQXTN,
		ORLIX_TCTI_SIMD_ARITH_UQXTN,
		ORLIX_TCTI_SIMD_ARITH_SQXTUN,
	};
	u8 operation;
	u8 size;
	u8 upper;

	for (operation = 0; operation < ARRAY_SIZE(operations); operation++) {
		for (size = 0; size < 3; size++) {
			for (upper = 0; upper < 2; upper++)
				orlix_tcti_test_execute_saturating_narrow_case(test,
					operations[operation], false, upper, size,
					(11 + size) & 0x1fU, (23 + upper) & 0x1fU);
			orlix_tcti_test_execute_saturating_narrow_case(test,
				operations[operation], true, false, size, 17, 17);
		}
	}
}

static void orlix_tcti_saturating_narrow_source_leaves_reject_reserved_encodings(
	struct kunit *test)
{
	static const struct {
		u32 ordinal;
		u32 pattern;
	} leaves[] = {
		{ 3796U, 0x0e214800U },
		{ 3829U, 0x2e214800U },
		{ 3827U, 0x2e212800U },
		{ 3559U, 0x5e214800U },
		{ 3577U, 0x7e214800U },
		{ 3576U, 0x7e212800U },
	};
	u8 index;

	for (index = 0; index < ARRAY_SIZE(leaves); index++) {
		u32 instruction = leaves[index].pattern | (3U << 22) |
			(7U << 5) | 8U;

		orlix_tcti_advsimd_narrow_widen_expect_source_encoding(test,
			leaves[index].ordinal, instruction);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
			orlix_tcti_decode_aarch64(instruction).decode_class);
		orlix_tcti_test_expect_rejected_instruction(test, instruction);
	}
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
		orlix_tcti_decode_aarch64(ORLIX_TCTI_TEST_SQXTUN_PATTERN |
			(7U << 5) | 8U).decode_class);
	orlix_tcti_test_expect_rejected_instruction(test, ORLIX_TCTI_TEST_SQXTUN_PATTERN |
			(7U << 5) | 8U);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
		orlix_tcti_decode_aarch64(ORLIX_TCTI_TEST_SCALAR_SQXTUN_PATTERN |
			(7U << 5) | 8U).decode_class);
	orlix_tcti_test_expect_rejected_instruction(test,
		ORLIX_TCTI_TEST_SCALAR_SQXTUN_PATTERN | (7U << 5) | 8U);
}

static void orlix_tcti_narrow_widen_resume_reports_instruction_fetch_fault(
	struct kunit *test)
{
	struct orlix_tcti_advsimd_narrow_widen_context *context = test->priv;
	struct pt_regs regs = {};
	struct pt_regs before;
	struct orlix_tcti_result result;
	u64 before_simd[ARRAY_SIZE(current->thread.user_simd)];
	unsigned long before_simd_valid;
	unsigned long before_fpcr;
	unsigned long before_fpsr;
	int ret;

	ret = sys_mprotect(context->instructions, PAGE_SIZE, PROT_NONE);
	KUNIT_ASSERT_EQ(test, 0, ret);
	orlix_tcti_test_initialize_registers(&regs, 0xaabbccddU);
	regs.pc = context->instructions;
	regs.pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_C_BIT;
	regs.syscallno = NO_SYSCALL;
	before = regs;
	memcpy(before_simd, current->thread.user_simd, sizeof(before_simd));
	before_simd_valid = current->thread.user_simd_valid;
	before_fpcr = current->thread.user_fpcr;
	before_fpsr = current->thread.user_fpsr;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_USER_FAULT, result.reason);
	KUNIT_EXPECT_EQ(test, -EACCES, result.status);
	KUNIT_EXPECT_EQ(test, context->instructions, result.fault_address);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_ACCESS_FETCH, result.fault_access);
	KUNIT_EXPECT_EQ(test, context->instructions, result.pc);
	KUNIT_EXPECT_EQ(test, 0U, result.instruction);
	KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
	KUNIT_EXPECT_MEMEQ(test, before_simd, current->thread.user_simd,
			   sizeof(before_simd));
	KUNIT_EXPECT_EQ(test, before_simd_valid,
			current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, before_fpcr, current->thread.user_fpcr);
	KUNIT_EXPECT_EQ(test, before_fpsr, current->thread.user_fpsr);
	KUNIT_EXPECT_EQ(test, 0, sys_mprotect(context->instructions, PAGE_SIZE,
						 PROT_READ | PROT_WRITE));
}

static struct kunit_case orlix_tcti_advsimd_narrow_widen_source_bound_cases[] = {
	KUNIT_CASE(orlix_tcti_advsimd_narrow_widen_source_manifest_is_exact),
	KUNIT_CASE(orlix_tcti_xtn_source_leaf_executes_all_legal_variants),
	KUNIT_CASE(orlix_tcti_xtn_source_leaf_rejects_reserved_size),
	KUNIT_CASE(orlix_tcti_shift_long_source_leaves_execute_all_legal_variants),
	KUNIT_CASE(orlix_tcti_shift_long_source_leaves_reject_reserved_immediates),
	KUNIT_CASE(orlix_tcti_shll_alias_matches_unsigned_shift_long),
	KUNIT_CASE(orlix_tcti_shll_source_leaf_rejects_reserved_size),
	KUNIT_CASE(orlix_tcti_saturating_narrow_source_leaves_execute_all_legal_variants),
	KUNIT_CASE(orlix_tcti_saturating_narrow_source_leaves_reject_reserved_encodings),
	KUNIT_CASE(orlix_tcti_narrow_widen_resume_reports_instruction_fetch_fault),
	{}
};

static struct kunit_suite orlix_tcti_advsimd_narrow_widen_source_bound_suite = {
	.name = "orlix-tcti-advsimd-narrow-widen-source-bound",
	.init = orlix_tcti_advsimd_narrow_widen_test_init,
	.exit = orlix_tcti_advsimd_narrow_widen_test_exit,
	.test_cases = orlix_tcti_advsimd_narrow_widen_source_bound_cases,
};

kunit_test_suite(orlix_tcti_advsimd_narrow_widen_source_bound_suite);
