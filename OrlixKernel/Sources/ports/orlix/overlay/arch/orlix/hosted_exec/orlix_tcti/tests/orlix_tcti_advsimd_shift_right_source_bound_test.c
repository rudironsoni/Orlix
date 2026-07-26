// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
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
 * Pinned AARCHMRS 2026-06 vector source leaves.  The existing decoder test
 * exhausts their legal immediate space through the switch executor.  This
 * suite binds each source leaf to the manifest and runs representative legal
 * boundary encodings through mapped RX guest text and orlix_tcti_resume_user().
 */
#define ORLIX_TCTI_ADVSIMD_SHIFT_RIGHT_SVC	0xd4000001U
#define ORLIX_TCTI_ADVSIMD_SHIFT_RIGHT_RD	4U
#define ORLIX_TCTI_ADVSIMD_SHIFT_RIGHT_RN	5U

struct orlix_tcti_advsimd_shift_right_leaf {
	u16 source_ordinal;
	const char *source_name;
	const char *source_mnemonic;
	const char *source_operation;
	u32 source_mask;
	u32 source_pattern;
	enum orlix_tcti_simd_vector_arithmetic_op operation;
};

struct orlix_tcti_advsimd_source_manifest_leaf {
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
static const struct orlix_tcti_advsimd_source_manifest_leaf
	orlix_tcti_advsimd_source_manifest[] = {
#include "../isa/source_manifest.def"
};
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE

static const struct orlix_tcti_advsimd_shift_right_leaf
	orlix_tcti_advsimd_shift_right_leaves[] = {
	{ 3995U, "SSHR_asimdshf_R", "SSHR", "SSHR_advsimd",
	  0xbf80fc00U, 0x0f000400U,
	  ORLIX_TCTI_SIMD_ARITH_SSHR },
	{ 3996U, "SSRA_asimdshf_R", "SSRA", "SSRA_advsimd",
	  0xbf80fc00U, 0x0f001400U,
	  ORLIX_TCTI_SIMD_ARITH_SSRA },
	{ 3997U, "SRSHR_asimdshf_R", "SRSHR", "SRSHR_advsimd",
	  0xbf80fc00U, 0x0f002400U,
	  ORLIX_TCTI_SIMD_ARITH_SRSHR },
	{ 3998U, "SRSRA_asimdshf_R", "SRSRA", "SRSRA_advsimd",
	  0xbf80fc00U, 0x0f003400U,
	  ORLIX_TCTI_SIMD_ARITH_SRSRA },
	{ 4008U, "USHR_asimdshf_R", "USHR", "USHR_advsimd",
	  0xbf80fc00U, 0x2f000400U,
	  ORLIX_TCTI_SIMD_ARITH_USHR },
	{ 4009U, "USRA_asimdshf_R", "USRA", "USRA_advsimd",
	  0xbf80fc00U, 0x2f001400U,
	  ORLIX_TCTI_SIMD_ARITH_USRA },
	{ 4010U, "URSHR_asimdshf_R", "URSHR", "URSHR_advsimd",
	  0xbf80fc00U, 0x2f002400U,
	  ORLIX_TCTI_SIMD_ARITH_URSHR },
	{ 4011U, "URSRA_asimdshf_R", "URSRA", "URSRA_advsimd",
	  0xbf80fc00U, 0x2f003400U,
	  ORLIX_TCTI_SIMD_ARITH_URSRA },
};

struct orlix_tcti_advsimd_shift_right_context {
	struct mm_struct *mm;
	u64 simd[ARRAY_SIZE(current->thread.user_simd)];
	unsigned long simd_valid;
	unsigned long fpcr;
	unsigned long fpsr;
};

static const struct orlix_tcti_advsimd_source_manifest_leaf *
orlix_tcti_advsimd_shift_right_source_leaf(u32 ordinal)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_advsimd_source_manifest); index++)
		if (orlix_tcti_advsimd_source_manifest[index].ordinal == ordinal)
			return &orlix_tcti_advsimd_source_manifest[index];
	return NULL;
}

static int orlix_tcti_advsimd_shift_right_test_init(struct kunit *test)
{
	struct orlix_tcti_advsimd_shift_right_context *context;

	context = kunit_kzalloc(test, sizeof(*context), GFP_KERNEL);
	if (!context)
		return -ENOMEM;
	context->mm = mm_alloc();
	if (!context->mm)
		return -ENOMEM;
	kthread_use_mm(context->mm);
	memcpy(context->simd, current->thread.user_simd, sizeof(context->simd));
	context->simd_valid = current->thread.user_simd_valid;
	context->fpcr = current->thread.user_fpcr;
	context->fpsr = current->thread.user_fpsr;
	test->priv = context;
	return 0;
}

static void orlix_tcti_advsimd_shift_right_test_exit(struct kunit *test)
{
	struct orlix_tcti_advsimd_shift_right_context *context = test->priv;

	memcpy(current->thread.user_simd, context->simd, sizeof(context->simd));
	current->thread.user_simd_valid = context->simd_valid;
	current->thread.user_fpcr = context->fpcr;
	current->thread.user_fpsr = context->fpsr;
	kthread_unuse_mm(context->mm);
	mmput(context->mm);
}

static u32 orlix_tcti_advsimd_shift_right_instruction(
	const struct orlix_tcti_advsimd_shift_right_leaf *leaf, u8 q, u8 size,
	u8 shift, u8 rd, u8 rn)
{
	u8 lane_bits = BIT(size) * 8;

	return leaf->source_pattern | ((u32)q << 30) |
		((u32)(2 * lane_bits - shift) << 16) | ((u32)rn << 5) | rd;
}

static unsigned long orlix_tcti_advsimd_shift_right_map_instruction(
	struct kunit *test, u32 instruction)
{
	const u32 program[] = { instruction, ORLIX_TCTI_ADVSIMD_SHIFT_RIGHT_SVC };
	unsigned long mapped;
	int ret;

	mapped = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));
	ret = orlix_tcti_write_user_data(current->mm, mapped, program, sizeof(program));
	if (ret) {
		vm_munmap(mapped, PAGE_SIZE);
		KUNIT_FAIL(test, "could not write AdvSIMD shift-right program: %d", ret);
		return 0;
	}
	ret = sys_mprotect(mapped, PAGE_SIZE, PROT_READ | PROT_EXEC);
	if (ret) {
		vm_munmap(mapped, PAGE_SIZE);
		KUNIT_FAIL(test, "could not protect AdvSIMD shift-right program: %d", ret);
		return 0;
	}
	return mapped;
}

static void orlix_tcti_advsimd_shift_right_seed_regs(struct pt_regs *regs,
						unsigned long pc)
{
	unsigned int index;

	for (index = 0; index < ARRAY_SIZE(regs->regs); index++)
		regs->regs[index] = 0x9e3779b97f4a7c15ULL ^
			((u64)(index + 1) * 0x0101010101010101ULL);
	regs->sp = 0x00000001fffffff0ULL;
	regs->pc = pc;
	regs->pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_C_BIT;
	regs->orig_x0 = 0xd1b54a32d192ed03ULL;
	regs->syscallno = NO_SYSCALL;
	regs->unused = 0x6d5a56c3U;
}

static u64 orlix_tcti_advsimd_shift_right_mask(u8 bits)
{
	return bits == 64 ? U64_MAX : BIT_ULL(bits) - 1;
}

static bool orlix_tcti_advsimd_shift_right_is_signed(
	enum orlix_tcti_simd_vector_arithmetic_op operation)
{
	return operation == ORLIX_TCTI_SIMD_ARITH_SSHR ||
		operation == ORLIX_TCTI_SIMD_ARITH_SSRA ||
		operation == ORLIX_TCTI_SIMD_ARITH_SRSHR ||
		operation == ORLIX_TCTI_SIMD_ARITH_SRSRA;
}

static bool orlix_tcti_advsimd_shift_right_rounds(
	enum orlix_tcti_simd_vector_arithmetic_op operation)
{
	return operation == ORLIX_TCTI_SIMD_ARITH_SRSHR ||
		operation == ORLIX_TCTI_SIMD_ARITH_URSHR ||
		operation == ORLIX_TCTI_SIMD_ARITH_SRSRA ||
		operation == ORLIX_TCTI_SIMD_ARITH_URSRA;
}

static bool orlix_tcti_advsimd_shift_right_accumulates(
	enum orlix_tcti_simd_vector_arithmetic_op operation)
{
	return operation == ORLIX_TCTI_SIMD_ARITH_SSRA ||
		operation == ORLIX_TCTI_SIMD_ARITH_USRA ||
		operation == ORLIX_TCTI_SIMD_ARITH_SRSRA ||
		operation == ORLIX_TCTI_SIMD_ARITH_URSRA;
}

static u64 orlix_tcti_advsimd_shift_right_lane_input(u8 bits, u8 lane)
{
	u64 mask = orlix_tcti_advsimd_shift_right_mask(bits);
	u64 value = (0x81ULL + 0x13ULL * lane) & mask;

	if (bits > 8 && (lane & 1U))
		value |= BIT_ULL(bits - 1);
	return value;
}

static u64 orlix_tcti_advsimd_shift_right_lane_accumulator(u8 bits, u8 lane)
{
	return (0x23ULL + 0x19ULL * lane) &
		orlix_tcti_advsimd_shift_right_mask(bits);
}

static u64 orlix_tcti_advsimd_shift_right_lane_expected(
	const struct orlix_tcti_advsimd_shift_right_leaf *leaf, u64 value,
	u64 accumulator, u8 bits, u8 shift)
{
	u64 mask = orlix_tcti_advsimd_shift_right_mask(bits);
	u64 shifted;

	if (orlix_tcti_advsimd_shift_right_is_signed(leaf->operation)) {
		s64 signed_value = bits == 64 ? (s64)value :
			sign_extend64(value, bits - 1);
		s64 signed_result = shift == bits ?
			(signed_value < 0 ? -1 : 0) : signed_value >> shift;

		if (orlix_tcti_advsimd_shift_right_rounds(leaf->operation))
			signed_result += (value >> (shift - 1)) & 1U;
		shifted = (u64)signed_result & mask;
	} else {
		shifted = shift == bits ? 0 : value >> shift;
		if (orlix_tcti_advsimd_shift_right_rounds(leaf->operation))
			shifted += (value >> (shift - 1)) & 1U;
	}
	if (orlix_tcti_advsimd_shift_right_accumulates(leaf->operation))
		shifted += accumulator;
	return shifted & mask;
}

static void orlix_tcti_advsimd_shift_right_source_leaves_decode(struct kunit *test)
{
	size_t index;
	u8 q;
	u8 size;

	KUNIT_ASSERT_EQ(test, 8U, ARRAY_SIZE(orlix_tcti_advsimd_shift_right_leaves));
	for (index = 0; index < ARRAY_SIZE(orlix_tcti_advsimd_shift_right_leaves); index++) {
		const struct orlix_tcti_advsimd_shift_right_leaf *leaf =
			&orlix_tcti_advsimd_shift_right_leaves[index];
		const struct orlix_tcti_advsimd_source_manifest_leaf *source =
			orlix_tcti_advsimd_shift_right_source_leaf(leaf->source_ordinal);

		KUNIT_ASSERT_NOT_NULL(test, source);
		KUNIT_EXPECT_STREQ(test, leaf->source_name, source->id);
		KUNIT_EXPECT_STREQ(test, leaf->source_mnemonic, source->mnemonic);
		KUNIT_EXPECT_STREQ(test, leaf->source_operation, source->operation);
		KUNIT_EXPECT_EQ(test, leaf->source_mask, source->mask);
		KUNIT_EXPECT_EQ(test, leaf->source_pattern, source->pattern);
		for (q = 0; q < 2; q++) {
			for (size = 0; size < 4; size++) {
				u8 lane_bits = BIT(size) * 8;
				u32 instruction = orlix_tcti_advsimd_shift_right_instruction(
					leaf, q, size, lane_bits,
					ORLIX_TCTI_ADVSIMD_SHIFT_RIGHT_RD,
					ORLIX_TCTI_ADVSIMD_SHIFT_RIGHT_RN);
				struct orlix_tcti_decoded_instruction decoded =
					orlix_tcti_decode_aarch64(instruction);

				KUNIT_EXPECT_EQ(test, leaf->source_pattern,
						instruction & leaf->source_mask);

				if (!q && size == 3) {
					KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
							decoded.decode_class);
					continue;
				}
				KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC,
						    decoded.decode_class, "%s q=%u size=%u",
						    leaf->source_name, q, size);
				KUNIT_EXPECT_EQ(test, leaf->operation,
						decoded.simd_arithmetic_op);
				KUNIT_EXPECT_EQ(test, BIT(size), decoded.access_size);
				KUNIT_EXPECT_EQ(test, q ? 16U : 8U, decoded.result_size);
				KUNIT_EXPECT_EQ(test, lane_bits, decoded.shift_amount);
			}
		}
	}
}

static void orlix_tcti_advsimd_shift_right_source_leaves_reject_invalid_immediates(
	struct kunit *test)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_advsimd_shift_right_leaves); index++) {
		const struct orlix_tcti_advsimd_shift_right_leaf *leaf =
			&orlix_tcti_advsimd_shift_right_leaves[index];
		u8 q;
		u8 immb;

		for (q = 0; q < 2; q++) {
			for (immb = 0; immb < 8; immb++) {
				u32 instruction = leaf->source_pattern | ((u32)q << 30) |
					((u32)immb << 16) |
					((u32)ORLIX_TCTI_ADVSIMD_SHIFT_RIGHT_RN << 5) |
					ORLIX_TCTI_ADVSIMD_SHIFT_RIGHT_RD;

				KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
					orlix_tcti_decode_aarch64(instruction).decode_class,
					"%s q=%u immb=%u", leaf->source_name, q, immb);
			}
		}
	}
}

static void orlix_tcti_advsimd_shift_right_expect_rejected_resume(
	struct kunit *test, const struct orlix_tcti_advsimd_shift_right_leaf *leaf,
	u32 instruction)
{
	unsigned long mapped =
		orlix_tcti_advsimd_shift_right_map_instruction(test, instruction);
	struct pt_regs regs = {};
	struct pt_regs before;
	struct orlix_tcti_result result;
	u64 before_simd[ARRAY_SIZE(current->thread.user_simd)];
	unsigned int simd_index;

	KUNIT_ASSERT_NE(test, 0UL, mapped);
	for (simd_index = 0; simd_index < ARRAY_SIZE(current->thread.user_simd);
	     simd_index++)
		current->thread.user_simd[simd_index] =
			0x6a09e667f3bcc909ULL ^ ((u64)instruction << 11) ^ simd_index;
	memcpy(before_simd, current->thread.user_simd, sizeof(before_simd));
	current->thread.user_simd_valid = 0;
	current->thread.user_fpcr = BIT(22) | BIT(24);
	current->thread.user_fpsr = BIT(27) | BIT(4);
	orlix_tcti_advsimd_shift_right_seed_regs(&regs, mapped);
	before = regs;
	result = orlix_tcti_resume_user(current, &regs, current->mm);

	KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION, result.reason,
			    "%s instruction=%#x", leaf->source_name, instruction);
	KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, result.status);
	KUNIT_EXPECT_EQ(test, mapped, result.pc);
	KUNIT_EXPECT_EQ(test, instruction, result.instruction);
	KUNIT_EXPECT_MEMEQ(test, before_simd, current->thread.user_simd,
			   sizeof(before_simd));
	KUNIT_EXPECT_EQ(test, 0UL, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, BIT(22) | BIT(24), current->thread.user_fpcr);
	KUNIT_EXPECT_EQ(test, BIT(27) | BIT(4), current->thread.user_fpsr);
	KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void orlix_tcti_advsimd_shift_right_source_leaves_reject_production_invalid_forms(
	struct kunit *test)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_advsimd_shift_right_leaves); index++) {
		const struct orlix_tcti_advsimd_shift_right_leaf *leaf =
			&orlix_tcti_advsimd_shift_right_leaves[index];
		u32 invalid_64bit_vector = orlix_tcti_advsimd_shift_right_instruction(
			leaf, 0, 3, 64, ORLIX_TCTI_ADVSIMD_SHIFT_RIGHT_RD,
			ORLIX_TCTI_ADVSIMD_SHIFT_RIGHT_RN);
		u8 q;
		u8 immb;

		for (q = 0; q < 2; q++)
			for (immb = 0; immb < 8; immb++)
				orlix_tcti_advsimd_shift_right_expect_rejected_resume(
					test, leaf, leaf->source_pattern | ((u32)q << 30) |
					((u32)immb << 16) |
					((u32)ORLIX_TCTI_ADVSIMD_SHIFT_RIGHT_RN << 5) |
					ORLIX_TCTI_ADVSIMD_SHIFT_RIGHT_RD);
		orlix_tcti_advsimd_shift_right_expect_rejected_resume(test, leaf,
						      invalid_64bit_vector);
	}
}

static void orlix_tcti_advsimd_shift_right_source_leaves_resume(struct kunit *test)
{
	size_t index;
	u8 q;
	u8 size;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_advsimd_shift_right_leaves); index++) {
		const struct orlix_tcti_advsimd_shift_right_leaf *leaf =
			&orlix_tcti_advsimd_shift_right_leaves[index];

		for (q = 0; q < 2; q++) {
			for (size = 0; size < 4; size++) {
				u8 lane_bytes = BIT(size);
				u8 lane_bits = lane_bytes * 8;
				u8 result_bytes;
				u8 shift_index;
				u8 shifts[] = { 1, lane_bits / 2, lane_bits };

				if (!q && size == 3)
					continue;
				result_bytes = q ? 16 : 8;
				for (shift_index = 0; shift_index < ARRAY_SIZE(shifts);
				     shift_index++) {
					u8 shift = shifts[shift_index];
					u32 instruction = orlix_tcti_advsimd_shift_right_instruction(
						leaf, q, size, shift,
						ORLIX_TCTI_ADVSIMD_SHIFT_RIGHT_RD,
						ORLIX_TCTI_ADVSIMD_SHIFT_RIGHT_RN);
					unsigned long mapped =
						orlix_tcti_advsimd_shift_right_map_instruction(test, instruction);
					struct pt_regs regs = {};
					struct pt_regs before;
					struct pt_regs expected_regs;
					struct orlix_tcti_result result;
					u64 expected_simd[ARRAY_SIZE(current->thread.user_simd)];
					u8 lane;
					unsigned int simd_index;

					KUNIT_ASSERT_NE(test, 0UL, mapped);
					for (simd_index = 0;
					     simd_index < ARRAY_SIZE(current->thread.user_simd);
					     simd_index++)
						current->thread.user_simd[simd_index] =
							0x9e3779b97f4a7c15ULL ^
							((u64)instruction << 7) ^ simd_index;
					for (lane = 0; lane < result_bytes / lane_bytes; lane++) {
						u8 byte = lane * lane_bytes;
						u8 word = byte / sizeof(u64);
						u8 bit = (byte % sizeof(u64)) * 8;

						current->thread.user_simd[ORLIX_TCTI_ADVSIMD_SHIFT_RIGHT_RN * 2 + word] &=
							~(orlix_tcti_advsimd_shift_right_mask(lane_bits) << bit);
						current->thread.user_simd[ORLIX_TCTI_ADVSIMD_SHIFT_RIGHT_RN * 2 + word] |=
							orlix_tcti_advsimd_shift_right_lane_input(lane_bits, lane) << bit;
						current->thread.user_simd[ORLIX_TCTI_ADVSIMD_SHIFT_RIGHT_RD * 2 + word] &=
							~(orlix_tcti_advsimd_shift_right_mask(lane_bits) << bit);
						current->thread.user_simd[ORLIX_TCTI_ADVSIMD_SHIFT_RIGHT_RD * 2 + word] |=
							orlix_tcti_advsimd_shift_right_lane_accumulator(lane_bits, lane) << bit;
					}
					memcpy(expected_simd, current->thread.user_simd,
					       sizeof(expected_simd));
					for (lane = 0; lane < result_bytes / lane_bytes; lane++) {
						u8 byte = lane * lane_bytes;
						u8 word = byte / sizeof(u64);
						u8 bit = (byte % sizeof(u64)) * 8;
						u64 mask = orlix_tcti_advsimd_shift_right_mask(lane_bits);
						u64 value = orlix_tcti_advsimd_shift_right_lane_input(lane_bits, lane);
						u64 accumulator =
							orlix_tcti_advsimd_shift_right_lane_accumulator(lane_bits, lane);
						u64 expected = orlix_tcti_advsimd_shift_right_lane_expected(
							leaf, value, accumulator, lane_bits, shift);

						expected_simd[ORLIX_TCTI_ADVSIMD_SHIFT_RIGHT_RD * 2 + word] &=
							~(mask << bit);
						expected_simd[ORLIX_TCTI_ADVSIMD_SHIFT_RIGHT_RD * 2 + word] |=
							expected << bit;
					}
					if (!q)
						expected_simd[ORLIX_TCTI_ADVSIMD_SHIFT_RIGHT_RD * 2 + 1] = 0;
					current->thread.user_simd_valid = 0;
					current->thread.user_fpcr = BIT(22) | BIT(24);
					current->thread.user_fpsr = BIT(27) | BIT(4);
					orlix_tcti_advsimd_shift_right_seed_regs(&regs, mapped);
					before = regs;
					expected_regs = before;
					expected_regs.pc += sizeof(u32);
					result = orlix_tcti_resume_user(current, &regs, current->mm);

					KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason,
							    "%s q=%u size=%u shift=%u",
							    leaf->source_name, q, size, shift);
					KUNIT_EXPECT_EQ(test, 0L, result.status);
					KUNIT_EXPECT_EQ(test, mapped + sizeof(u32), result.pc);
					KUNIT_EXPECT_EQ(test, ORLIX_TCTI_ADVSIMD_SHIFT_RIGHT_SVC,
							result.instruction);
					KUNIT_EXPECT_MEMEQ(test, expected_simd,
							   current->thread.user_simd,
							   sizeof(expected_simd));
					KUNIT_EXPECT_EQ(test, 1UL, current->thread.user_simd_valid);
					KUNIT_EXPECT_EQ(test, BIT(22) | BIT(24),
							current->thread.user_fpcr);
					KUNIT_EXPECT_EQ(test, BIT(27) | BIT(4),
							current->thread.user_fpsr);
					KUNIT_EXPECT_MEMEQ(test, &expected_regs, &regs,
							   sizeof(regs));
					KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
				}
			}
		}
	}
}

static struct kunit_case orlix_tcti_advsimd_shift_right_source_bound_cases[] = {
	KUNIT_CASE(orlix_tcti_advsimd_shift_right_source_leaves_decode),
	KUNIT_CASE(orlix_tcti_advsimd_shift_right_source_leaves_reject_invalid_immediates),
	KUNIT_CASE(orlix_tcti_advsimd_shift_right_source_leaves_reject_production_invalid_forms),
	KUNIT_CASE(orlix_tcti_advsimd_shift_right_source_leaves_resume),
	{}
};

static struct kunit_suite orlix_tcti_advsimd_shift_right_source_bound_suite = {
	.name = "orlix-tcti-advsimd-shift-right-source-bound",
	.init = orlix_tcti_advsimd_shift_right_test_init,
	.exit = orlix_tcti_advsimd_shift_right_test_exit,
	.test_cases = orlix_tcti_advsimd_shift_right_source_bound_cases,
};

kunit_test_suite(orlix_tcti_advsimd_shift_right_source_bound_suite);

MODULE_LICENSE("GPL");
