// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/bitops.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/syscalls.h>

#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/tcti.h>
#include <asm/unistd.h>

#include "../decode_aarch64.h"
#include "../fixed_fp.h"
#include "../switch_debug.h"

#define TCTI_FPCR_RMODE_POSINF BIT(22)
#define TCTI_FPCR_RMODE_NEGINF BIT(23)
#define TCTI_FPCR_RMODE_ZERO GENMASK(23, 22)
#define TCTI_FPSR_IOC BIT(0)
#define TCTI_FPSR_DZC BIT(1)
#define TCTI_FPSR_IXC BIT(4)
#define TCTI_FPSR_QC BIT(27)

static void tcti_scalar_fp16_decode_pinned_legal_and_reserved(
	struct kunit *test)
{
	static const struct {
		u32 instruction;
		enum tcti_decode_class decode_class;
		u8 operation;
	} legal[] = {
		{ 0x1ee0c000U, TCTI_DECODE_FP_SCALAR_1SOURCE, TCTI_FP1_FABS },
		{ 0x1ee14000U, TCTI_DECODE_FP_SCALAR_1SOURCE, TCTI_FP1_FNEG },
		{ 0x1ee1c000U, TCTI_DECODE_FP_SCALAR_1SOURCE, TCTI_FP1_FSQRT },
		{ 0x1ee44000U, TCTI_DECODE_FP_SCALAR_1SOURCE, TCTI_FP1_FRINTN },
		{ 0x1ee4c000U, TCTI_DECODE_FP_SCALAR_1SOURCE, TCTI_FP1_FRINTP },
		{ 0x1ee54000U, TCTI_DECODE_FP_SCALAR_1SOURCE, TCTI_FP1_FRINTM },
		{ 0x1ee5c000U, TCTI_DECODE_FP_SCALAR_1SOURCE, TCTI_FP1_FRINTZ },
		{ 0x1ee64000U, TCTI_DECODE_FP_SCALAR_1SOURCE, TCTI_FP1_FRINTA },
		{ 0x1ee74000U, TCTI_DECODE_FP_SCALAR_1SOURCE, TCTI_FP1_FRINTX },
		{ 0x1ee7c000U, TCTI_DECODE_FP_SCALAR_1SOURCE, TCTI_FP1_FRINTI },
		{ 0x1ee00800U, TCTI_DECODE_FP_SCALAR_2SOURCE, TCTI_FP2_FMUL },
		{ 0x1ee01800U, TCTI_DECODE_FP_SCALAR_2SOURCE, TCTI_FP2_FDIV },
		{ 0x1ee02800U, TCTI_DECODE_FP_SCALAR_2SOURCE, TCTI_FP2_FADD },
		{ 0x1ee03800U, TCTI_DECODE_FP_SCALAR_2SOURCE, TCTI_FP2_FSUB },
		{ 0x1ee04800U, TCTI_DECODE_FP_SCALAR_2SOURCE, TCTI_FP2_FMAX },
		{ 0x1ee05800U, TCTI_DECODE_FP_SCALAR_2SOURCE, TCTI_FP2_FMIN },
		{ 0x1ee06800U, TCTI_DECODE_FP_SCALAR_2SOURCE, TCTI_FP2_FMAXNM },
		{ 0x1ee07800U, TCTI_DECODE_FP_SCALAR_2SOURCE, TCTI_FP2_FMINNM },
		{ 0x1ee08800U, TCTI_DECODE_FP_SCALAR_2SOURCE, TCTI_FP2_FNMUL },
		{ 0x1fc00000U, TCTI_DECODE_FP_SCALAR_3SOURCE, TCTI_FP3_FMADD },
		{ 0x1fc08000U, TCTI_DECODE_FP_SCALAR_3SOURCE, TCTI_FP3_FMSUB },
		{ 0x1fe00000U, TCTI_DECODE_FP_SCALAR_3SOURCE, TCTI_FP3_FNMADD },
		{ 0x1fe08000U, TCTI_DECODE_FP_SCALAR_3SOURCE, TCTI_FP3_FNMSUB },
	};
	size_t index;

	for (index = 0; index < ARRAY_SIZE(legal); index++) {
		struct tcti_decoded_instruction decoded =
			tcti_decode_aarch64(legal[index].instruction);
		struct tcti_decoded_instruction reserved = tcti_decode_aarch64(
			legal[index].instruction & ~BIT(22));

		KUNIT_EXPECT_EQ(test, legal[index].decode_class,
				decoded.decode_class);
		KUNIT_EXPECT_EQ(test, sizeof(u16), decoded.access_size);
		KUNIT_EXPECT_EQ(test, sizeof(u16), decoded.result_size);
		KUNIT_EXPECT_TRUE(test, decoded.simd_fp);
		switch (decoded.decode_class) {
		case TCTI_DECODE_FP_SCALAR_1SOURCE:
			KUNIT_EXPECT_EQ(test, legal[index].operation,
					decoded.fp1_op);
			break;
		case TCTI_DECODE_FP_SCALAR_2SOURCE:
			KUNIT_EXPECT_EQ(test, legal[index].operation,
					decoded.fp2_op);
			break;
		case TCTI_DECODE_FP_SCALAR_3SOURCE:
			KUNIT_EXPECT_EQ(test, legal[index].operation,
					decoded.fp3_op);
			break;
		default:
			KUNIT_FAIL(test, "unexpected scalar FP16 decode class %u",
				   decoded.decode_class);
		}
		KUNIT_EXPECT_EQ(test, TCTI_DECODE_UNSUPPORTED,
				reserved.decode_class);
	}

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_UNSUPPORTED,
		tcti_decode_aarch64(0x1ee6c000U).decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_DECODE_UNSUPPORTED,
		tcti_decode_aarch64(0x1ee09800U).decode_class);
}

static unsigned long tcti_scalar_fp16_map_instructions(struct kunit *test,
	const u32 *instructions, size_t instruction_count)
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
		KUNIT_FAIL(test, "could not write scalar FP16 instructions: %d",
			   ret);
		return 0;
	}
	ret = sys_mprotect(mapped, PAGE_SIZE, PROT_READ | PROT_EXEC);
	if (ret) {
		vm_munmap(mapped, PAGE_SIZE);
		KUNIT_FAIL(test, "could not protect scalar FP16 instructions: %d",
			   ret);
		return 0;
	}

	return mapped;
}

static void tcti_fp16_resume_rejects_without_state_change(
	struct kunit *test, u32 instruction)
{
	const u32 instructions[] = { instruction, 0xd4000001U };
	struct tcti_result result;
	struct pt_regs regs = {};
	struct pt_regs before;
	u64 simd_before[ARRAY_SIZE(current->thread.user_simd)];
	unsigned long fpcr_before;
	unsigned long fpsr_before;
	unsigned long mapped;
	int ret;

	mapped = tcti_scalar_fp16_map_instructions(test, instructions,
		ARRAY_SIZE(instructions));
	KUNIT_ASSERT_NE(test, 0UL, mapped);
	current->thread.user_simd[0] = 0x0123456789abcdefULL;
	current->thread.user_simd[1] = 0xfedcba9876543210ULL;
	current->thread.user_simd[2] = 0x3e003c007c007e00ULL;
	current->thread.user_simd[3] = 0x1122334455667788ULL;
	current->thread.user_simd[4] = 0x400038003c000000ULL;
	current->thread.user_simd[5] = 0x8877665544332211ULL;
	current->thread.user_simd_valid = true;
	current->thread.user_fpcr = TCTI_FPCR_RMODE_POSINF;
	current->thread.user_fpsr = TCTI_FPSR_QC;
	regs.pc = mapped;
	regs.sp = STACK_TOP - 16;
	regs.pstate = PSR_MODE_EL0t;
	regs.syscallno = NO_SYSCALL;
	regs.regs[0] = 0xaaaaaaaaaaaaaaaaULL;
	regs.regs[1] = 0xbbbbbbbbbbbbbbbbULL;
	before = regs;
	memcpy(simd_before, current->thread.user_simd, sizeof(simd_before));
	fpcr_before = current->thread.user_fpcr;
	fpsr_before = current->thread.user_fpsr;

	result = tcti_resume_user(current, &regs, current->mm);

	KUNIT_EXPECT_EQ(test, TCTI_EXIT_UNSUPPORTED_INSTRUCTION, result.reason);
	KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, result.status);
	KUNIT_EXPECT_EQ(test, mapped, result.pc);
	KUNIT_EXPECT_EQ(test, instruction, result.instruction);
	KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
	KUNIT_EXPECT_MEMEQ(test, simd_before, current->thread.user_simd,
			  sizeof(simd_before));
	KUNIT_EXPECT_EQ(test, fpcr_before, current->thread.user_fpcr);
	KUNIT_EXPECT_EQ(test, fpsr_before, current->thread.user_fpsr);
	KUNIT_EXPECT_TRUE(test, current->thread.user_simd_valid);

	ret = vm_munmap(mapped, PAGE_SIZE);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void tcti_scalar_fp16_resume_user_is_runtime_gated(
	struct kunit *test)
{
	tcti_fp16_resume_rejects_without_state_change(test, 0x1ee22820U);
}

static void tcti_scalar_fp16_compare_select_is_runtime_gated(
	struct kunit *test)
{
	static const u32 instructions[] = {
		0x1ee22020U, /* fcmp h1, h2 */
		0x1ee20c20U, /* fcsel h0, h1, h2, eq */
		0xd4000001U, /* svc #0 */
	};
	static const struct {
		u32 instruction;
		enum tcti_decode_class decode_class;
	} legal[] = {
		{ 0x1ee02000U, TCTI_DECODE_FP_SCALAR_COMPARE },
		{ 0x1ee02008U, TCTI_DECODE_FP_SCALAR_COMPARE },
		{ 0x1ee02010U, TCTI_DECODE_FP_SCALAR_COMPARE },
		{ 0x1ee00400U, TCTI_DECODE_FP_SCALAR_COMPARE },
		{ 0x1ee00410U, TCTI_DECODE_FP_SCALAR_COMPARE },
		{ 0x1ee00c00U, TCTI_DECODE_FP_CONDITIONAL_SELECT },
	};
	size_t index;

	for (index = 0; index < ARRAY_SIZE(legal); index++) {
		struct tcti_decoded_instruction decoded =
			tcti_decode_aarch64(legal[index].instruction);

		KUNIT_EXPECT_EQ(test, legal[index].decode_class,
				decoded.decode_class);
		KUNIT_EXPECT_EQ(test, sizeof(u16), decoded.access_size);
		KUNIT_EXPECT_EQ(test, sizeof(u16), decoded.result_size);
		KUNIT_EXPECT_TRUE(test, decoded.simd_fp);
		KUNIT_EXPECT_EQ(test, TCTI_DECODE_UNSUPPORTED,
			tcti_decode_aarch64(legal[index].instruction & ~BIT(22)).decode_class);
	}

	tcti_fp16_resume_rejects_without_state_change(test, instructions[0]);
}

static void tcti_scalar_fp16_fccmp_is_runtime_gated(
	struct kunit *test)
{
	static const u32 instructions[] = {
		0x1ee2042aU, /* fccmp h1, h2, #0xa, eq */
		0x1ee24c20U, /* fcsel h0, h1, h2, mi */
		0xd4000001U, /* svc #0 */
	};
	tcti_fp16_resume_rejects_without_state_change(test, instructions[0]);
}

static void tcti_scalar_fp_execute(struct kunit *test, u32 instruction,
				   u64 left, u64 right, unsigned long fpcr,
				   unsigned long initial_fpsr, u64 expected,
				   unsigned long expected_fpsr)
{
	struct tcti_decoded_instruction decoded = tcti_decode_aarch64(instruction);
	struct pt_regs regs = {};
	unsigned long host_fpcr_before;
	unsigned long host_fpsr_before;
	unsigned long host_fpcr_after;
	unsigned long host_fpsr_after;
	int ret;

	KUNIT_ASSERT_EQ(test, TCTI_DECODE_FP_SCALAR_2SOURCE,
			decoded.decode_class);
	KUNIT_ASSERT_EQ(test, decoded.access_size, decoded.result_size);
	asm volatile("mrs %0, fpcr\n\tmrs %1, fpsr"
		: "=r" (host_fpcr_before), "=r" (host_fpsr_before));
	current->thread.user_simd[0] = left;
	current->thread.user_simd[1] = 0x0123456789abcdefULL;
	current->thread.user_simd[2] = right;
	current->thread.user_simd[3] = 0xfedcba9876543210ULL;
	current->thread.user_fpcr = fpcr;
	current->thread.user_fpsr = initial_fpsr;
	regs.pc = 0x9000;

	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, expected, current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, 0ULL, current->thread.user_simd[1]);
	KUNIT_EXPECT_EQ(test, fpcr, current->thread.user_fpcr);
	KUNIT_EXPECT_EQ(test, expected_fpsr, current->thread.user_fpsr);
	KUNIT_EXPECT_EQ(test, 0x9004ULL, regs.pc);
	asm volatile("mrs %0, fpcr\n\tmrs %1, fpsr"
		: "=r" (host_fpcr_after), "=r" (host_fpsr_after));
	KUNIT_EXPECT_EQ(test, host_fpcr_before, host_fpcr_after);
	KUNIT_EXPECT_EQ(test, host_fpsr_before, host_fpsr_after);
}

static void tcti_scalar_fp_fadd_honors_rounding_and_inexact(
	struct kunit *test)
{
	static const struct {
		unsigned long fpcr;
		u32 expected;
	} cases[] = {
		{ 0, 0x4b000002U },
		{ TCTI_FPCR_RMODE_POSINF, 0x4b000002U },
		{ TCTI_FPCR_RMODE_NEGINF, 0x4b000001U },
		{ TCTI_FPCR_RMODE_ZERO, 0x4b000001U },
	};
	size_t index;

	for (index = 0; index < ARRAY_SIZE(cases); index++)
		tcti_scalar_fp_execute(test, 0x1e212800U,
			0x3ffc0000U, 0x4b000000U, cases[index].fpcr,
			TCTI_FPSR_QC, cases[index].expected,
			TCTI_FPSR_QC | TCTI_FPSR_IXC);
}

static void tcti_scalar_fp_fadd_quiets_signaling_nan(struct kunit *test)
{
	tcti_scalar_fp_execute(test, 0x1e212800U, 0x7f800001U,
		0x3f800000U, 0, TCTI_FPSR_QC, 0x7fc00001U,
		TCTI_FPSR_QC | TCTI_FPSR_IOC);
}

static void tcti_scalar_fp_fsub_preserves_zero_sign(struct kunit *test)
{
	tcti_scalar_fp_execute(test, 0x1e213800U, 0x00000000U,
		0x00000000U, 0, TCTI_FPSR_QC, 0x00000000U,
		TCTI_FPSR_QC);
	tcti_scalar_fp_execute(test, 0x1e213800U, 0x80000000U,
		0x00000000U, 0, TCTI_FPSR_QC, 0x80000000U,
		TCTI_FPSR_QC);
}

static void tcti_scalar_fp_zero_compare_respects_s_and_d_widths(
	struct kunit *test)
{
	struct tcti_decoded_instruction scalar_s =
		tcti_decode_aarch64(0x5ea0d800U);
	struct tcti_decoded_instruction scalar_d =
		tcti_decode_aarch64(0x5ee0d800U);
	struct pt_regs regs = {};
	int ret;

	KUNIT_ASSERT_EQ(test, TCTI_DECODE_SIMD_VECTOR_ARITHMETIC,
		scalar_s.decode_class);
	KUNIT_ASSERT_TRUE(test, scalar_s.simd_scalar);
	KUNIT_ASSERT_EQ(test, sizeof(u32), scalar_s.access_size);
	KUNIT_ASSERT_EQ(test, sizeof(u32), scalar_s.result_size);
	current->thread.user_simd[0] = 0x3ff0000000000000ULL;
	current->thread.user_simd[1] = 0x0123456789abcdefULL;
	current->thread.user_fpsr = TCTI_FPSR_QC;
	regs.pc = 0x9100;
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &scalar_s, NULL);
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0xffffffffULL, current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, 0ULL, current->thread.user_simd[1]);
	KUNIT_EXPECT_EQ(test, TCTI_FPSR_QC, current->thread.user_fpsr);
	KUNIT_EXPECT_EQ(test, 0x9104ULL, regs.pc);

	KUNIT_ASSERT_EQ(test, TCTI_DECODE_SIMD_VECTOR_ARITHMETIC,
		scalar_d.decode_class);
	KUNIT_ASSERT_TRUE(test, scalar_d.simd_scalar);
	KUNIT_ASSERT_EQ(test, sizeof(u64), scalar_d.access_size);
	KUNIT_ASSERT_EQ(test, sizeof(u64), scalar_d.result_size);
	current->thread.user_simd[0] = 0x3ff0000000000000ULL;
	current->thread.user_simd[1] = 0x0123456789abcdefULL;
	current->thread.user_fpsr = TCTI_FPSR_QC;
	regs.pc = 0x9200;
	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &scalar_d, NULL);
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0ULL, current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, 0ULL, current->thread.user_simd[1]);
	KUNIT_EXPECT_EQ(test, TCTI_FPSR_QC, current->thread.user_fpsr);
	KUNIT_EXPECT_EQ(test, 0x9204ULL, regs.pc);
}

static void tcti_simd_fp16_three_same_preserves_fp_environment(
	struct kunit *test)
{
	u64 result[2] = {};
	u64 left[2] = { 0x0000000000006400ULL, 0 };
	u64 right[2] = { 0x0000000000003800ULL, 0 };
	u64 accumulator[2] = {};
	unsigned long fpsr = TCTI_FPSR_QC;
	unsigned long host_fpcr_before;
	unsigned long host_fpsr_before;
	unsigned long host_fpcr_after;
	unsigned long host_fpsr_after;
	int ret;

	asm volatile("mrs %0, fpcr\n\tmrs %1, fpsr"
		: "=r" (host_fpcr_before), "=r" (host_fpsr_before));
	ret = tcti_native_simd_fp16_three_same(TCTI_SIMD_ARITH_FADD, true,
		true, result, left, right, accumulator, 0, &fpsr);
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x6400ULL, result[0]);
	KUNIT_EXPECT_EQ(test, 0ULL, result[1]);
	KUNIT_EXPECT_EQ(test, TCTI_FPSR_QC | TCTI_FPSR_IXC, fpsr);

	fpsr = TCTI_FPSR_QC;
	ret = tcti_native_simd_fp16_three_same(TCTI_SIMD_ARITH_FADD, true,
		true, result, left, right, accumulator, TCTI_FPCR_RMODE_POSINF,
		&fpsr);
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x6401ULL, result[0]);
	KUNIT_EXPECT_EQ(test, TCTI_FPSR_QC | TCTI_FPSR_IXC, fpsr);
	asm volatile("mrs %0, fpcr\n\tmrs %1, fpsr"
		: "=r" (host_fpcr_after), "=r" (host_fpsr_after));
	KUNIT_EXPECT_EQ(test, host_fpcr_before, host_fpcr_after);
	KUNIT_EXPECT_EQ(test, host_fpsr_before, host_fpsr_after);
}

static void tcti_simd_fp16_three_same_nan_zero_and_accumulate(
	struct kunit *test)
{
	u64 result[2] = {};
	u64 left[2] = { 0x0000000000007c01ULL, 0 };
	u64 right[2] = { 0x0000000000003c00ULL, 0 };
	u64 accumulator[2] = {};
	unsigned long fpsr = TCTI_FPSR_QC;
	int ret;

	ret = tcti_native_simd_fp16_three_same(TCTI_SIMD_ARITH_FADD, true,
		true, result, left, right, accumulator, 0, &fpsr);
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x7e01ULL, result[0]);
	KUNIT_EXPECT_EQ(test, TCTI_FPSR_QC | TCTI_FPSR_IOC, fpsr);

	left[0] = 0x0000000000008000ULL;
	right[0] = 0;
	fpsr = TCTI_FPSR_QC;
	ret = tcti_native_simd_fp16_three_same(TCTI_SIMD_ARITH_FSUB, true,
		true, result, left, right, accumulator, 0, &fpsr);
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x8000ULL, result[0]);
	KUNIT_EXPECT_EQ(test, TCTI_FPSR_QC, fpsr);

	left[0] = 0x40003c00ULL;
	right[0] = 0x40004000ULL;
	accumulator[0] = 0x42004200ULL;
	fpsr = TCTI_FPSR_QC;
	ret = tcti_native_simd_fp16_three_same(TCTI_SIMD_ARITH_FMLA, false,
		false, result, left, right, accumulator, 0, &fpsr);
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x47004500ULL, result[0]);
	KUNIT_EXPECT_EQ(test, 0ULL, result[1]);
	KUNIT_EXPECT_EQ(test, TCTI_FPSR_QC, fpsr);
}

static void tcti_simd_fp16_three_same_rejects_unknown_operation(
	struct kunit *test)
{
	u64 result[2] = { ~0ULL, ~0ULL };
	u64 source[2] = {};
	unsigned long fpsr = TCTI_FPSR_QC;
	int ret;

	ret = tcti_native_simd_fp16_three_same(TCTI_SIMD_ARITH_PMUL, false,
		true, result, source, source, source, 0, &fpsr);
	KUNIT_EXPECT_EQ(test, -EINVAL, ret);
	KUNIT_EXPECT_EQ(test, ~0ULL, result[0]);
	KUNIT_EXPECT_EQ(test, ~0ULL, result[1]);
	KUNIT_EXPECT_EQ(test, TCTI_FPSR_QC, fpsr);
}

static void tcti_simd_fp16_three_same_pairwise_and_compare_lanes(
	struct kunit *test)
{
	u64 result[2] = {};
	u64 left[2] = { 0x4400420040003c00ULL, 0 };
	u64 right[2] = { 0x4800470046004500ULL, 0 };
	u64 accumulator[2] = {};
	unsigned long fpsr = TCTI_FPSR_QC;
	int ret;

	ret = tcti_native_simd_fp16_three_same(TCTI_SIMD_ARITH_FADDP, false,
		false, result, left, right, accumulator, 0, &fpsr);
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x4b80498047004200ULL, result[0]);
	KUNIT_EXPECT_EQ(test, 0ULL, result[1]);
	KUNIT_EXPECT_EQ(test, TCTI_FPSR_QC, fpsr);

	left[0] = 0x00003c007c013c00ULL;
	right[0] = 0x0000bc003c003c00ULL;
	fpsr = TCTI_FPSR_QC;
	ret = tcti_native_simd_fp16_three_same(TCTI_SIMD_ARITH_FCMEQ, false,
		false, result, left, right, accumulator, 0, &fpsr);
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0xffff00000000ffffULL, result[0]);
	KUNIT_EXPECT_EQ(test, TCTI_FPSR_QC | TCTI_FPSR_IOC, fpsr);
}

/*
 * These source leaves are intentionally still rejected.  They stay visible as
 * blocking AARCHMRS 2026-06 obligations until their decoder and exact ASL
 * semantics are owned.  The ASL paths identify the authoritative operation
 * entry, not a host-side semantic substitute.
 */
static void tcti_advsimd_fp16_unimplemented_source_leaves_are_rejected(
	struct kunit *test)
{
	static const struct {
		const char *source_id;
		const char *asl_entry;
		u32 instruction;
	} cases[] = {
		{ "FSUB_asimdsamefp16_only", "operations/FSUB_advsimd",
		  0x0ec21420U },
		{ "FAMAX_asimdsamefp16_only", "operations/FAMAX_advsimd",
		  0x0ec21c20U },
		{ "FAMIN_asimdsamefp16_only", "operations/FAMIN_advsimd",
		  0x2ec21c20U },
		{ "FSCALE_asimdsamefp16_only", "operations/FSCALE_advsimd",
		  0x2ec23c20U },
	};
	size_t index;

	for (index = 0; index < ARRAY_SIZE(cases); index++) {
		struct tcti_decoded_instruction decoded =
			tcti_decode_aarch64(cases[index].instruction);

		KUNIT_EXPECT_EQ_MSG(test, TCTI_DECODE_UNSUPPORTED,
			decoded.decode_class, "%s (%s)", cases[index].source_id,
			cases[index].asl_entry);
	}
}

static void tcti_advsimd_fp16_resume_is_runtime_gated(
	struct kunit *test)
{
	tcti_fp16_resume_rejects_without_state_change(test, 0x6e423c20U);
}

static void tcti_scalar_fp16_execute(struct kunit *test,
	const struct tcti_decoded_instruction *decoded, u64 left, u64 right,
	u64 addend, unsigned long fpcr, unsigned long initial_fpsr,
	u64 expected, unsigned long expected_fpsr)
{
	struct pt_regs regs = {};
	unsigned long host_fpcr_before;
	unsigned long host_fpsr_before;
	unsigned long host_fpcr_after;
	unsigned long host_fpsr_after;
	int ret;

	asm volatile("mrs %0, fpcr\n\tmrs %1, fpsr"
		: "=r" (host_fpcr_before), "=r" (host_fpsr_before));
	current->thread.user_simd[0] = left;
	current->thread.user_simd[1] = 0x0123456789abcdefULL;
	current->thread.user_simd[2] = right;
	current->thread.user_simd[3] = 0xfedcba9876543210ULL;
	current->thread.user_simd[4] = addend;
	current->thread.user_simd[5] = 0x1122334455667788ULL;
	current->thread.user_fpcr = fpcr;
	current->thread.user_fpsr = initial_fpsr;
	regs.pc = 0xa000;

	ret = tcti_switch_debug_execute_decoded(NULL, &regs, decoded, NULL);

	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, expected, current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, 0ULL, current->thread.user_simd[1]);
	KUNIT_EXPECT_EQ(test, fpcr, current->thread.user_fpcr);
	KUNIT_EXPECT_EQ(test, expected_fpsr, current->thread.user_fpsr);
	KUNIT_EXPECT_EQ(test, 0xa004ULL, regs.pc);
	asm volatile("mrs %0, fpcr\n\tmrs %1, fpsr"
		: "=r" (host_fpcr_after), "=r" (host_fpsr_after));
	KUNIT_EXPECT_EQ(test, host_fpcr_before, host_fpcr_after);
	KUNIT_EXPECT_EQ(test, host_fpsr_before, host_fpsr_after);
}

static void tcti_scalar_fp16_unary_rounding_and_sqrt(struct kunit *test)
{
	struct tcti_decoded_instruction decoded = {
		.decode_class = TCTI_DECODE_FP_SCALAR_1SOURCE,
		.rd = 0,
		.rn = 1,
		.access_size = sizeof(u16),
		.result_size = sizeof(u16),
	};

	decoded.fp1_op = TCTI_FP1_FSQRT;
	tcti_scalar_fp16_execute(test, &decoded, 0x4400U, 0, 0, 0,
		TCTI_FPSR_QC, 0x4000U, TCTI_FPSR_QC);
	decoded.fp1_op = TCTI_FP1_FRINTM;
	tcti_scalar_fp16_execute(test, &decoded, 0xbe00U, 0, 0, 0,
		TCTI_FPSR_QC, 0xc000U, TCTI_FPSR_QC);
}

static void tcti_scalar_fp16_two_source_preserves_fp_state(struct kunit *test)
{
	struct tcti_decoded_instruction decoded = {
		.decode_class = TCTI_DECODE_FP_SCALAR_2SOURCE,
		.rd = 0,
		.rn = 1,
		.rm = 2,
		.access_size = sizeof(u16),
		.result_size = sizeof(u16),
		.fp2_op = TCTI_FP2_FADD,
	};

	tcti_scalar_fp16_execute(test, &decoded, 0x3c00U, 0x1000U, 0,
		TCTI_FPCR_RMODE_POSINF, TCTI_FPSR_QC, 0x3c01U,
		TCTI_FPSR_QC | TCTI_FPSR_IXC);
	tcti_scalar_fp16_execute(test, &decoded, 0x7c01U, 0x3c00U, 0,
		0, TCTI_FPSR_QC, 0x7e01U,
		TCTI_FPSR_QC | TCTI_FPSR_IOC);
	decoded.fp2_op = TCTI_FP2_FSUB;
	tcti_scalar_fp16_execute(test, &decoded, 0x8000U, 0, 0, 0,
		TCTI_FPSR_QC, 0x8000U, TCTI_FPSR_QC);
}

static void tcti_scalar_fp16_fused_three_source_is_single_step(
	struct kunit *test)
{
	struct tcti_decoded_instruction decoded = {
		.decode_class = TCTI_DECODE_FP_SCALAR_3SOURCE,
		.rd = 0,
		.rn = 1,
		.rm = 2,
		.ra = 3,
		.access_size = sizeof(u16),
		.result_size = sizeof(u16),
		.fp3_op = TCTI_FP3_FMADD,
	};

	tcti_scalar_fp16_execute(test, &decoded, 0x3e00U, 0x4000U, 0x3800U,
		0, TCTI_FPSR_QC, 0x4300U, TCTI_FPSR_QC);
}

static void tcti_scalar_fp16_reserved_width_is_rejected(struct kunit *test)
{
	struct tcti_decoded_instruction decoded = {
		.decode_class = TCTI_DECODE_FP_SCALAR_2SOURCE,
		.rd = 0,
		.rn = 1,
		.rm = 2,
		.access_size = sizeof(u8),
		.result_size = sizeof(u8),
		.fp2_op = TCTI_FP2_FADD,
	};
	struct pt_regs regs = { .pc = 0xa100 };
	int ret;

	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);
	KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, ret);
	KUNIT_EXPECT_EQ(test, 0xa100ULL, regs.pc);
}

/*
 * These rows bind the direct scalar conversion encodings to the production
 * resume path.  The pinned source exposes the operation identifier and
 * encoding, while the shared-ASL corpus is still absent, so this is execution
 * evidence only and must not discharge the scalar FP semantic obligation.
 */
struct tcti_scalar_fp_convert_source_row {
	u16 ordinal;
	const char *source_id;
	u32 instruction;
	enum tcti_fp_int_convert_op operation;
	bool gpr_to_fp;
	u64 input;
	u64 expected;
	unsigned long expected_fpsr;
};

#define TCTI_SCALAR_FP_QC TCTI_FPSR_QC
#define TCTI_SCALAR_FP_QC_IXC (TCTI_FPSR_QC | TCTI_FPSR_IXC)

static const struct tcti_scalar_fp_convert_source_row
tcti_scalar_fp_convert_source_rows[] = {
	{ 4108U, "FCVTNS_32S_float2int", 0x1e200020U,
	  TCTI_FP_INT_FCVTNS, false, 0x3fc00000U, 2U,
	  TCTI_SCALAR_FP_QC_IXC },
	{ 4109U, "FCVTNU_32S_float2int", 0x1e210020U,
	  TCTI_FP_INT_FCVTNU, false, 0x3fc00000U, 2U,
	  TCTI_SCALAR_FP_QC_IXC },
	{ 4110U, "SCVTF_S32_float2int", 0x1e220020U,
	  TCTI_FP_INT_SCVTF, true, 0xfffffffeU, 0xc0000000U,
	  TCTI_SCALAR_FP_QC },
	{ 4111U, "UCVTF_S32_float2int", 0x1e230020U,
	  TCTI_FP_INT_UCVTF, true, 3U, 0x40400000U,
	  TCTI_SCALAR_FP_QC },
	{ 4112U, "FCVTAS_32S_float2int", 0x1e240020U,
	  TCTI_FP_INT_FCVTAS, false, 0x3fc00000U, 2U,
	  TCTI_SCALAR_FP_QC_IXC },
	{ 4113U, "FCVTAU_32S_float2int", 0x1e250020U,
	  TCTI_FP_INT_FCVTAU, false, 0x3fc00000U, 2U,
	  TCTI_SCALAR_FP_QC_IXC },
	{ 4116U, "FCVTPS_32S_float2int", 0x1e280020U,
	  TCTI_FP_INT_FCVTPS, false, 0x3fc00000U, 2U,
	  TCTI_SCALAR_FP_QC_IXC },
	{ 4117U, "FCVTPU_32S_float2int", 0x1e290020U,
	  TCTI_FP_INT_FCVTPU, false, 0x3fc00000U, 2U,
	  TCTI_SCALAR_FP_QC_IXC },
	{ 4118U, "FCVTMS_32S_float2int", 0x1e300020U,
	  TCTI_FP_INT_FCVTMS, false, 0x3fc00000U, 1U,
	  TCTI_SCALAR_FP_QC_IXC },
	{ 4119U, "FCVTMU_32S_float2int", 0x1e310020U,
	  TCTI_FP_INT_FCVTMU, false, 0x3fc00000U, 1U,
	  TCTI_SCALAR_FP_QC_IXC },
	{ 4120U, "FCVTZS_32S_float2int", 0x1e380020U,
	  TCTI_FP_INT_FCVTZS, false, 0x3fc00000U, 1U,
	  TCTI_SCALAR_FP_QC_IXC },
	{ 4121U, "FCVTZU_32S_float2int", 0x1e390020U,
	  TCTI_FP_INT_FCVTZU, false, 0x3fc00000U, 1U,
	  TCTI_SCALAR_FP_QC_IXC },
	{ 4084U, "SCVTF_S32_float2fix", 0x1e02fc20U,
	  TCTI_FP_INT_SCVTF_FIXED, true, 1U, 0x3f000000U,
	  TCTI_SCALAR_FP_QC },
	{ 4085U, "UCVTF_S32_float2fix", 0x1e03fc20U,
	  TCTI_FP_INT_UCVTF_FIXED, true, 1U, 0x3f000000U,
	  TCTI_SCALAR_FP_QC },
	{ 4086U, "FCVTZS_32S_float2fix", 0x1e18fc20U,
	  TCTI_FP_INT_FCVTZS_FIXED, false, 0x3f000000U, 1U,
	  TCTI_SCALAR_FP_QC },
	{ 4087U, "FCVTZU_32S_float2fix", 0x1e19fc20U,
	  TCTI_FP_INT_FCVTZU_FIXED, false, 0x3f000000U, 1U,
	  TCTI_SCALAR_FP_QC },
};

static void tcti_scalar_fp_convert_resume_source_rows(struct kunit *test)
{
	static const u32 svc = 0xd4000001U;
	size_t index;

	for (index = 0; index < ARRAY_SIZE(tcti_scalar_fp_convert_source_rows);
	     index++) {
		const struct tcti_scalar_fp_convert_source_row *row =
			&tcti_scalar_fp_convert_source_rows[index];
		const u32 instructions[] = { row->instruction, svc };
		struct tcti_decoded_instruction decoded =
			tcti_decode_aarch64(row->instruction);
		struct tcti_result result;
		struct pt_regs regs = {};
		unsigned long mapped;
		int ret;

		KUNIT_ASSERT_EQ_MSG(test, TCTI_DECODE_FP_INT_CONVERT,
				    decoded.decode_class, "%s source ordinal %u",
				    row->source_id, row->ordinal);
		KUNIT_ASSERT_EQ_MSG(test, row->operation, decoded.fp_int_op,
				    "%s source ordinal %u", row->source_id,
				    row->ordinal);
		KUNIT_ASSERT_EQ(test, 0U, decoded.rd);
		KUNIT_ASSERT_EQ(test, 1U, decoded.rn);

		mapped = tcti_scalar_fp16_map_instructions(test, instructions,
						     ARRAY_SIZE(instructions));
		KUNIT_ASSERT_NE(test, 0UL, mapped);
		regs.pc = mapped;
		regs.sp = STACK_TOP - 16;
		regs.pstate = PSR_MODE_EL0t;
		regs.syscallno = NO_SYSCALL;
		current->thread.user_fpcr = 0;
		current->thread.user_fpsr = TCTI_SCALAR_FP_QC;
		if (row->gpr_to_fp)
			regs.regs[1] = row->input;
		else {
			current->thread.user_simd[2] = row->input;
			current->thread.user_simd[3] = 0x0123456789abcdefULL;
		}

		result = tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ_MSG(test, TCTI_EXIT_SYSCALL, result.reason,
				    "%s source ordinal %u", row->source_id,
				    row->ordinal);
		KUNIT_EXPECT_EQ(test, 0L, result.status);
		KUNIT_EXPECT_EQ(test, mapped + sizeof(u32), result.pc);
		KUNIT_EXPECT_EQ(test, svc, result.instruction);
		KUNIT_EXPECT_EQ(test, mapped + sizeof(u32), regs.pc);
		KUNIT_EXPECT_EQ(test, (unsigned long)PSR_MODE_EL0t,
				regs.pstate & PSR_MODE_MASK);
		KUNIT_EXPECT_EQ(test, row->expected_fpsr,
				current->thread.user_fpsr);
		if (row->gpr_to_fp) {
			KUNIT_EXPECT_EQ(test, row->input, regs.regs[1]);
			KUNIT_EXPECT_EQ(test, row->expected,
					current->thread.user_simd[0]);
			KUNIT_EXPECT_EQ(test, 0ULL,
					current->thread.user_simd[1]);
		} else {
			KUNIT_EXPECT_EQ(test, row->expected, regs.regs[0]);
			KUNIT_EXPECT_EQ(test, row->input,
					current->thread.user_simd[2]);
			KUNIT_EXPECT_EQ(test, 0x0123456789abcdefULL,
					current->thread.user_simd[3]);
		}

		ret = vm_munmap(mapped, PAGE_SIZE);
		KUNIT_EXPECT_EQ(test, 0, ret);
	}
}

static void tcti_scalar_fp_convert_reserved_forms_exit_without_state_change(
	struct kunit *test)
{
	static const u32 invalid[] = {
		0x1e2c0020U, /* invalid FCVTA rounding selector */
		0x1e3c0020U, /* invalid FCVTZ rounding selector */
	};
	static const u32 svc = 0xd4000001U;
	size_t index;

	for (index = 0; index < ARRAY_SIZE(invalid); index++) {
		const u32 instructions[] = { invalid[index], svc };
		struct tcti_result result;
		struct pt_regs regs = {};
		struct pt_regs before;
		unsigned long mapped;
		int ret;

		KUNIT_ASSERT_EQ(test, TCTI_DECODE_UNSUPPORTED,
			tcti_decode_aarch64(invalid[index]).decode_class);
		mapped = tcti_scalar_fp16_map_instructions(test, instructions,
						     ARRAY_SIZE(instructions));
		KUNIT_ASSERT_NE(test, 0UL, mapped);
		regs.pc = mapped;
		regs.sp = STACK_TOP - 16;
		regs.pstate = PSR_MODE_EL0t;
		regs.syscallno = NO_SYSCALL;
		regs.regs[0] = 0xaaaaaaaaaaaaaaaaULL;
		current->thread.user_simd[2] = 0x3fc00000U;
		current->thread.user_fpsr = TCTI_SCALAR_FP_QC;
		before = regs;

		result = tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ(test, TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
				result.reason);
		KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, result.status);
		KUNIT_EXPECT_EQ(test, invalid[index], result.instruction);
		KUNIT_EXPECT_EQ(test, before.pc, regs.pc);
		KUNIT_EXPECT_EQ(test, before.regs[0], regs.regs[0]);
		KUNIT_EXPECT_EQ(test, TCTI_SCALAR_FP_QC,
				current->thread.user_fpsr);

		ret = vm_munmap(mapped, PAGE_SIZE);
		KUNIT_EXPECT_EQ(test, 0, ret);
	}
}

static struct kunit_case tcti_scalar_fp_semantics_test_cases[] = {
	KUNIT_CASE(tcti_scalar_fp16_decode_pinned_legal_and_reserved),
	KUNIT_CASE(tcti_scalar_fp16_resume_user_is_runtime_gated),
	KUNIT_CASE(tcti_scalar_fp16_compare_select_is_runtime_gated),
	KUNIT_CASE(tcti_scalar_fp16_fccmp_is_runtime_gated),
	KUNIT_CASE(tcti_scalar_fp_fadd_honors_rounding_and_inexact),
	KUNIT_CASE(tcti_scalar_fp_fadd_quiets_signaling_nan),
	KUNIT_CASE(tcti_scalar_fp_fsub_preserves_zero_sign),
	KUNIT_CASE(tcti_scalar_fp_zero_compare_respects_s_and_d_widths),
	KUNIT_CASE(tcti_simd_fp16_three_same_preserves_fp_environment),
	KUNIT_CASE(tcti_simd_fp16_three_same_nan_zero_and_accumulate),
	KUNIT_CASE(tcti_simd_fp16_three_same_rejects_unknown_operation),
	KUNIT_CASE(tcti_simd_fp16_three_same_pairwise_and_compare_lanes),
	KUNIT_CASE(tcti_advsimd_fp16_unimplemented_source_leaves_are_rejected),
	KUNIT_CASE(tcti_advsimd_fp16_resume_is_runtime_gated),
	KUNIT_CASE(tcti_scalar_fp16_unary_rounding_and_sqrt),
	KUNIT_CASE(tcti_scalar_fp16_two_source_preserves_fp_state),
	KUNIT_CASE(tcti_scalar_fp16_fused_three_source_is_single_step),
	KUNIT_CASE(tcti_scalar_fp16_reserved_width_is_rejected),
	KUNIT_CASE(tcti_scalar_fp_convert_resume_source_rows),
	KUNIT_CASE(tcti_scalar_fp_convert_reserved_forms_exit_without_state_change),
	{}
};

static struct kunit_suite tcti_scalar_fp_semantics_test_suite = {
	.name = "orlix-tcti-scalar-fp-semantics",
	.test_cases = tcti_scalar_fp_semantics_test_cases,
};

kunit_test_suite(tcti_scalar_fp_semantics_test_suite);

MODULE_LICENSE("GPL");
