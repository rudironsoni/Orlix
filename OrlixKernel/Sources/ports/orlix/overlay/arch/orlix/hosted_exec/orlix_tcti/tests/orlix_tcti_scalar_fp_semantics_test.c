// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/bitops.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/sched/mm.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/syscalls.h>

#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/orlix_tcti.h>
#include <asm/unistd.h>

#include "target_instruction_artifact.h"

#include "../decode_aarch64.h"
#include "../fixed_fp.h"
#include "../switch_debug.h"

#define ORLIX_TCTI_FPCR_RMODE_POSINF BIT(22)
#define ORLIX_TCTI_FPCR_RMODE_NEGINF BIT(23)
#define ORLIX_TCTI_FPCR_RMODE_ZERO GENMASK(23, 22)
#define ORLIX_TCTI_FPSR_IOC BIT(0)
#define ORLIX_TCTI_FPSR_DZC BIT(1)
#define ORLIX_TCTI_FPSR_IXC BIT(4)
#define ORLIX_TCTI_FPSR_QC BIT(27)

static void orlix_tcti_scalar_fp16_decode_pinned_legal_and_reserved(
	struct kunit *test)
{
	static const struct {
		u32 instruction;
		enum orlix_tcti_decode_class decode_class;
		u8 operation;
	} legal[] = {
		{ 0x1ee0c000U, ORLIX_TCTI_DECODE_FP_SCALAR_1SOURCE, ORLIX_TCTI_FP1_FABS },
		{ 0x1ee14000U, ORLIX_TCTI_DECODE_FP_SCALAR_1SOURCE, ORLIX_TCTI_FP1_FNEG },
		{ 0x1ee1c000U, ORLIX_TCTI_DECODE_FP_SCALAR_1SOURCE, ORLIX_TCTI_FP1_FSQRT },
		{ 0x1ee44000U, ORLIX_TCTI_DECODE_FP_SCALAR_1SOURCE, ORLIX_TCTI_FP1_FRINTN },
		{ 0x1ee4c000U, ORLIX_TCTI_DECODE_FP_SCALAR_1SOURCE, ORLIX_TCTI_FP1_FRINTP },
		{ 0x1ee54000U, ORLIX_TCTI_DECODE_FP_SCALAR_1SOURCE, ORLIX_TCTI_FP1_FRINTM },
		{ 0x1ee5c000U, ORLIX_TCTI_DECODE_FP_SCALAR_1SOURCE, ORLIX_TCTI_FP1_FRINTZ },
		{ 0x1ee64000U, ORLIX_TCTI_DECODE_FP_SCALAR_1SOURCE, ORLIX_TCTI_FP1_FRINTA },
		{ 0x1ee74000U, ORLIX_TCTI_DECODE_FP_SCALAR_1SOURCE, ORLIX_TCTI_FP1_FRINTX },
		{ 0x1ee7c000U, ORLIX_TCTI_DECODE_FP_SCALAR_1SOURCE, ORLIX_TCTI_FP1_FRINTI },
		{ 0x1ee00800U, ORLIX_TCTI_DECODE_FP_SCALAR_2SOURCE, ORLIX_TCTI_FP2_FMUL },
		{ 0x1ee01800U, ORLIX_TCTI_DECODE_FP_SCALAR_2SOURCE, ORLIX_TCTI_FP2_FDIV },
		{ 0x1ee02800U, ORLIX_TCTI_DECODE_FP_SCALAR_2SOURCE, ORLIX_TCTI_FP2_FADD },
		{ 0x1ee03800U, ORLIX_TCTI_DECODE_FP_SCALAR_2SOURCE, ORLIX_TCTI_FP2_FSUB },
		{ 0x1ee04800U, ORLIX_TCTI_DECODE_FP_SCALAR_2SOURCE, ORLIX_TCTI_FP2_FMAX },
		{ 0x1ee05800U, ORLIX_TCTI_DECODE_FP_SCALAR_2SOURCE, ORLIX_TCTI_FP2_FMIN },
		{ 0x1ee06800U, ORLIX_TCTI_DECODE_FP_SCALAR_2SOURCE, ORLIX_TCTI_FP2_FMAXNM },
		{ 0x1ee07800U, ORLIX_TCTI_DECODE_FP_SCALAR_2SOURCE, ORLIX_TCTI_FP2_FMINNM },
		{ 0x1ee08800U, ORLIX_TCTI_DECODE_FP_SCALAR_2SOURCE, ORLIX_TCTI_FP2_FNMUL },
		{ 0x1fc00000U, ORLIX_TCTI_DECODE_FP_SCALAR_3SOURCE, ORLIX_TCTI_FP3_FMADD },
		{ 0x1fc08000U, ORLIX_TCTI_DECODE_FP_SCALAR_3SOURCE, ORLIX_TCTI_FP3_FMSUB },
		{ 0x1fe00000U, ORLIX_TCTI_DECODE_FP_SCALAR_3SOURCE, ORLIX_TCTI_FP3_FNMADD },
		{ 0x1fe08000U, ORLIX_TCTI_DECODE_FP_SCALAR_3SOURCE, ORLIX_TCTI_FP3_FNMSUB },
	};
	size_t index;

	for (index = 0; index < ARRAY_SIZE(legal); index++) {
		struct orlix_tcti_decoded_instruction decoded =
			orlix_tcti_decode_aarch64(legal[index].instruction);
		struct orlix_tcti_decoded_instruction reserved = orlix_tcti_decode_aarch64(
			legal[index].instruction & ~BIT(22));

		KUNIT_EXPECT_EQ(test, legal[index].decode_class,
				decoded.decode_class);
		KUNIT_EXPECT_EQ(test, sizeof(u16), decoded.access_size);
		KUNIT_EXPECT_EQ(test, sizeof(u16), decoded.result_size);
		KUNIT_EXPECT_TRUE(test, decoded.simd_fp);
		switch (decoded.decode_class) {
		case ORLIX_TCTI_DECODE_FP_SCALAR_1SOURCE:
			KUNIT_EXPECT_EQ(test, legal[index].operation,
					decoded.fp1_op);
			break;
		case ORLIX_TCTI_DECODE_FP_SCALAR_2SOURCE:
			KUNIT_EXPECT_EQ(test, legal[index].operation,
					decoded.fp2_op);
			break;
		case ORLIX_TCTI_DECODE_FP_SCALAR_3SOURCE:
			KUNIT_EXPECT_EQ(test, legal[index].operation,
					decoded.fp3_op);
			break;
		default:
			KUNIT_FAIL(test, "unexpected scalar FP16 decode class %u",
				   decoded.decode_class);
		}
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
				reserved.decode_class);
	}

	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
		orlix_tcti_decode_aarch64(0x1ee6c000U).decode_class);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
		orlix_tcti_decode_aarch64(0x1ee09800U).decode_class);
}

static unsigned long orlix_tcti_scalar_fp16_map_instructions(struct kunit *test,
	const u32 *instructions, size_t instruction_count)
{
	unsigned long mapped;
	int ret;

	KUNIT_ASSERT_NOT_NULL(test, current->mm);
	mapped = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));
	ret = orlix_tcti_write_user_data(current->mm, mapped, instructions,
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

static void orlix_tcti_fp16_resume_rejects_without_state_change(
	struct kunit *test, u32 instruction)
{
	const u32 instructions[] = { instruction, 0xd4000001U };
	struct orlix_tcti_result result;
	struct pt_regs regs = {};
	struct pt_regs before;
	u64 simd_before[ARRAY_SIZE(current->thread.user_simd)];
	unsigned long fpcr_before;
	unsigned long fpsr_before;
	unsigned long mapped;
	int ret;

	mapped = orlix_tcti_scalar_fp16_map_instructions(test, instructions,
		ARRAY_SIZE(instructions));
	KUNIT_ASSERT_NE(test, 0UL, mapped);
	current->thread.user_simd[0] = 0x0123456789abcdefULL;
	current->thread.user_simd[1] = 0xfedcba9876543210ULL;
	current->thread.user_simd[2] = 0x3e003c007c007e00ULL;
	current->thread.user_simd[3] = 0x1122334455667788ULL;
	current->thread.user_simd[4] = 0x400038003c000000ULL;
	current->thread.user_simd[5] = 0x8877665544332211ULL;
	current->thread.user_simd_valid = true;
	current->thread.user_fpcr = ORLIX_TCTI_FPCR_RMODE_POSINF;
	current->thread.user_fpsr = ORLIX_TCTI_FPSR_QC;
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

	result = orlix_tcti_resume_user(current, &regs, current->mm);

	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION, result.reason);
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

static void orlix_tcti_scalar_fp16_resume_user_is_runtime_gated(
	struct kunit *test)
{
	orlix_tcti_fp16_resume_rejects_without_state_change(test, 0x1ee22820U);
}

static void orlix_tcti_scalar_fp16_compare_select_is_runtime_gated(
	struct kunit *test)
{
	static const u32 instructions[] = {
		0x1ee22020U, /* fcmp h1, h2 */
		0x1ee20c20U, /* fcsel h0, h1, h2, eq */
		0xd4000001U, /* svc #0 */
	};
	static const struct {
		u32 instruction;
		enum orlix_tcti_decode_class decode_class;
	} legal[] = {
		{ 0x1ee02000U, ORLIX_TCTI_DECODE_FP_SCALAR_COMPARE },
		{ 0x1ee02008U, ORLIX_TCTI_DECODE_FP_SCALAR_COMPARE },
		{ 0x1ee02010U, ORLIX_TCTI_DECODE_FP_SCALAR_COMPARE },
		{ 0x1ee00400U, ORLIX_TCTI_DECODE_FP_SCALAR_COMPARE },
		{ 0x1ee00410U, ORLIX_TCTI_DECODE_FP_SCALAR_COMPARE },
		{ 0x1ee00c00U, ORLIX_TCTI_DECODE_FP_CONDITIONAL_SELECT },
	};
	size_t index;

	for (index = 0; index < ARRAY_SIZE(legal); index++) {
		struct orlix_tcti_decoded_instruction decoded =
			orlix_tcti_decode_aarch64(legal[index].instruction);

		KUNIT_EXPECT_EQ(test, legal[index].decode_class,
				decoded.decode_class);
		KUNIT_EXPECT_EQ(test, sizeof(u16), decoded.access_size);
		KUNIT_EXPECT_EQ(test, sizeof(u16), decoded.result_size);
		KUNIT_EXPECT_TRUE(test, decoded.simd_fp);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
			orlix_tcti_decode_aarch64(legal[index].instruction & ~BIT(22)).decode_class);
	}

	orlix_tcti_fp16_resume_rejects_without_state_change(test, instructions[0]);
}

static void orlix_tcti_scalar_fp16_fccmp_is_runtime_gated(
	struct kunit *test)
{
	static const u32 instructions[] = {
		0x1ee2042aU, /* fccmp h1, h2, #0xa, eq */
		0x1ee24c20U, /* fcsel h0, h1, h2, mi */
		0xd4000001U, /* svc #0 */
	};
	orlix_tcti_fp16_resume_rejects_without_state_change(test, instructions[0]);
}

static void orlix_tcti_scalar_fp_execute(struct kunit *test, u32 instruction,
				   u64 left, u64 right, unsigned long fpcr,
				   unsigned long initial_fpsr, u64 expected,
				   unsigned long expected_fpsr)
{
	struct orlix_tcti_decoded_instruction decoded = orlix_tcti_decode_aarch64(instruction);
	struct pt_regs regs = {};
	unsigned long host_fpcr_before;
	unsigned long host_fpsr_before;
	unsigned long host_fpcr_after;
	unsigned long host_fpsr_after;
	int ret;

	KUNIT_ASSERT_EQ(test, ORLIX_TCTI_DECODE_FP_SCALAR_2SOURCE,
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

	ret = orlix_tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);

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

static void orlix_tcti_scalar_fp_fadd_honors_rounding_and_inexact(
	struct kunit *test)
{
	static const struct {
		unsigned long fpcr;
		u32 expected;
	} cases[] = {
		{ 0, 0x4b000002U },
		{ ORLIX_TCTI_FPCR_RMODE_POSINF, 0x4b000002U },
		{ ORLIX_TCTI_FPCR_RMODE_NEGINF, 0x4b000001U },
		{ ORLIX_TCTI_FPCR_RMODE_ZERO, 0x4b000001U },
	};
	size_t index;

	for (index = 0; index < ARRAY_SIZE(cases); index++)
		orlix_tcti_scalar_fp_execute(test, 0x1e212800U,
			0x3ffc0000U, 0x4b000000U, cases[index].fpcr,
			ORLIX_TCTI_FPSR_QC, cases[index].expected,
			ORLIX_TCTI_FPSR_QC | ORLIX_TCTI_FPSR_IXC);
}

static void orlix_tcti_scalar_fp_fadd_quiets_signaling_nan(struct kunit *test)
{
	orlix_tcti_scalar_fp_execute(test, 0x1e212800U, 0x7f800001U,
		0x3f800000U, 0, ORLIX_TCTI_FPSR_QC, 0x7fc00001U,
		ORLIX_TCTI_FPSR_QC | ORLIX_TCTI_FPSR_IOC);
}

static void orlix_tcti_scalar_fp_fsub_preserves_zero_sign(struct kunit *test)
{
	orlix_tcti_scalar_fp_execute(test, 0x1e213800U, 0x00000000U,
		0x00000000U, 0, ORLIX_TCTI_FPSR_QC, 0x00000000U,
		ORLIX_TCTI_FPSR_QC);
	orlix_tcti_scalar_fp_execute(test, 0x1e213800U, 0x80000000U,
		0x00000000U, 0, ORLIX_TCTI_FPSR_QC, 0x80000000U,
		ORLIX_TCTI_FPSR_QC);
}

static void orlix_tcti_scalar_fp_zero_compare_respects_s_and_d_widths(
	struct kunit *test)
{
	struct orlix_tcti_decoded_instruction scalar_s =
		orlix_tcti_decode_aarch64(0x5ea0d800U);
	struct orlix_tcti_decoded_instruction scalar_d =
		orlix_tcti_decode_aarch64(0x5ee0d800U);
	struct pt_regs regs = {};
	int ret;

	KUNIT_ASSERT_EQ(test, ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC,
		scalar_s.decode_class);
	KUNIT_ASSERT_TRUE(test, scalar_s.simd_scalar);
	KUNIT_ASSERT_EQ(test, sizeof(u32), scalar_s.access_size);
	KUNIT_ASSERT_EQ(test, sizeof(u32), scalar_s.result_size);
	current->thread.user_simd[0] = 0x3ff0000000000000ULL;
	current->thread.user_simd[1] = 0x0123456789abcdefULL;
	current->thread.user_fpsr = ORLIX_TCTI_FPSR_QC;
	regs.pc = 0x9100;
	ret = orlix_tcti_switch_debug_execute_decoded(NULL, &regs, &scalar_s, NULL);
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0xffffffffULL, current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, 0ULL, current->thread.user_simd[1]);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_FPSR_QC, current->thread.user_fpsr);
	KUNIT_EXPECT_EQ(test, 0x9104ULL, regs.pc);

	KUNIT_ASSERT_EQ(test, ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC,
		scalar_d.decode_class);
	KUNIT_ASSERT_TRUE(test, scalar_d.simd_scalar);
	KUNIT_ASSERT_EQ(test, sizeof(u64), scalar_d.access_size);
	KUNIT_ASSERT_EQ(test, sizeof(u64), scalar_d.result_size);
	current->thread.user_simd[0] = 0x3ff0000000000000ULL;
	current->thread.user_simd[1] = 0x0123456789abcdefULL;
	current->thread.user_fpsr = ORLIX_TCTI_FPSR_QC;
	regs.pc = 0x9200;
	ret = orlix_tcti_switch_debug_execute_decoded(NULL, &regs, &scalar_d, NULL);
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0ULL, current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, 0ULL, current->thread.user_simd[1]);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_FPSR_QC, current->thread.user_fpsr);
	KUNIT_EXPECT_EQ(test, 0x9204ULL, regs.pc);
}

static void orlix_tcti_simd_fp16_three_same_preserves_fp_environment(
	struct kunit *test)
{
	u64 result[2] = {};
	u64 left[2] = { 0x0000000000006400ULL, 0 };
	u64 right[2] = { 0x0000000000003800ULL, 0 };
	u64 accumulator[2] = {};
	unsigned long fpsr = ORLIX_TCTI_FPSR_QC;
	unsigned long host_fpcr_before;
	unsigned long host_fpsr_before;
	unsigned long host_fpcr_after;
	unsigned long host_fpsr_after;
	int ret;

	asm volatile("mrs %0, fpcr\n\tmrs %1, fpsr"
		: "=r" (host_fpcr_before), "=r" (host_fpsr_before));
	ret = orlix_tcti_native_simd_fp16_three_same(ORLIX_TCTI_SIMD_ARITH_FADD, true,
		true, result, left, right, accumulator, 0, &fpsr);
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x6400ULL, result[0]);
	KUNIT_EXPECT_EQ(test, 0ULL, result[1]);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_FPSR_QC | ORLIX_TCTI_FPSR_IXC, fpsr);

	fpsr = ORLIX_TCTI_FPSR_QC;
	ret = orlix_tcti_native_simd_fp16_three_same(ORLIX_TCTI_SIMD_ARITH_FADD, true,
		true, result, left, right, accumulator, ORLIX_TCTI_FPCR_RMODE_POSINF,
		&fpsr);
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x6401ULL, result[0]);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_FPSR_QC | ORLIX_TCTI_FPSR_IXC, fpsr);
	asm volatile("mrs %0, fpcr\n\tmrs %1, fpsr"
		: "=r" (host_fpcr_after), "=r" (host_fpsr_after));
	KUNIT_EXPECT_EQ(test, host_fpcr_before, host_fpcr_after);
	KUNIT_EXPECT_EQ(test, host_fpsr_before, host_fpsr_after);
}

static void orlix_tcti_simd_fp16_three_same_nan_zero_and_accumulate(
	struct kunit *test)
{
	u64 result[2] = {};
	u64 left[2] = { 0x0000000000007c01ULL, 0 };
	u64 right[2] = { 0x0000000000003c00ULL, 0 };
	u64 accumulator[2] = {};
	unsigned long fpsr = ORLIX_TCTI_FPSR_QC;
	int ret;

	ret = orlix_tcti_native_simd_fp16_three_same(ORLIX_TCTI_SIMD_ARITH_FADD, true,
		true, result, left, right, accumulator, 0, &fpsr);
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x7e01ULL, result[0]);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_FPSR_QC | ORLIX_TCTI_FPSR_IOC, fpsr);

	left[0] = 0x0000000000008000ULL;
	right[0] = 0;
	fpsr = ORLIX_TCTI_FPSR_QC;
	ret = orlix_tcti_native_simd_fp16_three_same(ORLIX_TCTI_SIMD_ARITH_FSUB, true,
		true, result, left, right, accumulator, 0, &fpsr);
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x8000ULL, result[0]);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_FPSR_QC, fpsr);

	left[0] = 0x40003c00ULL;
	right[0] = 0x40004000ULL;
	accumulator[0] = 0x42004200ULL;
	fpsr = ORLIX_TCTI_FPSR_QC;
	ret = orlix_tcti_native_simd_fp16_three_same(ORLIX_TCTI_SIMD_ARITH_FMLA, false,
		false, result, left, right, accumulator, 0, &fpsr);
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x47004500ULL, result[0]);
	KUNIT_EXPECT_EQ(test, 0ULL, result[1]);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_FPSR_QC, fpsr);
}

static void orlix_tcti_simd_fp16_three_same_rejects_unknown_operation(
	struct kunit *test)
{
	u64 result[2] = { ~0ULL, ~0ULL };
	u64 source[2] = {};
	unsigned long fpsr = ORLIX_TCTI_FPSR_QC;
	int ret;

	ret = orlix_tcti_native_simd_fp16_three_same(ORLIX_TCTI_SIMD_ARITH_PMUL, false,
		true, result, source, source, source, 0, &fpsr);
	KUNIT_EXPECT_EQ(test, -EINVAL, ret);
	KUNIT_EXPECT_EQ(test, ~0ULL, result[0]);
	KUNIT_EXPECT_EQ(test, ~0ULL, result[1]);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_FPSR_QC, fpsr);
}

static void orlix_tcti_simd_fp16_three_same_pairwise_and_compare_lanes(
	struct kunit *test)
{
	u64 result[2] = {};
	u64 left[2] = { 0x4400420040003c00ULL, 0 };
	u64 right[2] = { 0x4800470046004500ULL, 0 };
	u64 accumulator[2] = {};
	unsigned long fpsr = ORLIX_TCTI_FPSR_QC;
	int ret;

	ret = orlix_tcti_native_simd_fp16_three_same(ORLIX_TCTI_SIMD_ARITH_FADDP, false,
		false, result, left, right, accumulator, 0, &fpsr);
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0x4b80498047004200ULL, result[0]);
	KUNIT_EXPECT_EQ(test, 0ULL, result[1]);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_FPSR_QC, fpsr);

	left[0] = 0x00003c007c013c00ULL;
	right[0] = 0x0000bc003c003c00ULL;
	fpsr = ORLIX_TCTI_FPSR_QC;
	ret = orlix_tcti_native_simd_fp16_three_same(ORLIX_TCTI_SIMD_ARITH_FCMEQ, false,
		false, result, left, right, accumulator, 0, &fpsr);
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0xffff00000000ffffULL, result[0]);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_FPSR_QC | ORLIX_TCTI_FPSR_IOC, fpsr);
}

/*
 * These source leaves are intentionally still rejected.  They stay visible as
 * blocking AARCHMRS 2026-06 obligations until their decoder and exact
 * architecture semantics are owned. The external DDI0602 locators identify
 * provenance, not a host-side semantic substitute or proof credit.
 */
static void orlix_tcti_advsimd_fp16_unimplemented_source_leaves_are_rejected(
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
		struct orlix_tcti_decoded_instruction decoded =
			orlix_tcti_decode_aarch64(cases[index].instruction);

		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
			decoded.decode_class, "%s (%s)", cases[index].source_id,
			cases[index].asl_entry);
	}
}

static void orlix_tcti_advsimd_fp16_resume_is_runtime_gated(
	struct kunit *test)
{
	orlix_tcti_fp16_resume_rejects_without_state_change(test, 0x6e423c20U);
}

static void orlix_tcti_scalar_fp16_execute(struct kunit *test,
	const struct orlix_tcti_decoded_instruction *decoded, u64 left, u64 right,
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

	ret = orlix_tcti_switch_debug_execute_decoded(NULL, &regs, decoded, NULL);

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

static void orlix_tcti_scalar_fp16_unary_rounding_and_sqrt(struct kunit *test)
{
	struct orlix_tcti_decoded_instruction decoded = {
		.decode_class = ORLIX_TCTI_DECODE_FP_SCALAR_1SOURCE,
		.rd = 0,
		.rn = 1,
		.access_size = sizeof(u16),
		.result_size = sizeof(u16),
	};

	decoded.fp1_op = ORLIX_TCTI_FP1_FSQRT;
	orlix_tcti_scalar_fp16_execute(test, &decoded, 0x4400U, 0, 0, 0,
		ORLIX_TCTI_FPSR_QC, 0x4000U, ORLIX_TCTI_FPSR_QC);
	decoded.fp1_op = ORLIX_TCTI_FP1_FRINTM;
	orlix_tcti_scalar_fp16_execute(test, &decoded, 0xbe00U, 0, 0, 0,
		ORLIX_TCTI_FPSR_QC, 0xc000U, ORLIX_TCTI_FPSR_QC);
}

static void orlix_tcti_scalar_fp16_two_source_preserves_fp_state(struct kunit *test)
{
	struct orlix_tcti_decoded_instruction decoded = {
		.decode_class = ORLIX_TCTI_DECODE_FP_SCALAR_2SOURCE,
		.rd = 0,
		.rn = 1,
		.rm = 2,
		.access_size = sizeof(u16),
		.result_size = sizeof(u16),
		.fp2_op = ORLIX_TCTI_FP2_FADD,
	};

	orlix_tcti_scalar_fp16_execute(test, &decoded, 0x3c00U, 0x1000U, 0,
		ORLIX_TCTI_FPCR_RMODE_POSINF, ORLIX_TCTI_FPSR_QC, 0x3c01U,
		ORLIX_TCTI_FPSR_QC | ORLIX_TCTI_FPSR_IXC);
	orlix_tcti_scalar_fp16_execute(test, &decoded, 0x7c01U, 0x3c00U, 0,
		0, ORLIX_TCTI_FPSR_QC, 0x7e01U,
		ORLIX_TCTI_FPSR_QC | ORLIX_TCTI_FPSR_IOC);
	decoded.fp2_op = ORLIX_TCTI_FP2_FSUB;
	orlix_tcti_scalar_fp16_execute(test, &decoded, 0x8000U, 0, 0, 0,
		ORLIX_TCTI_FPSR_QC, 0x8000U, ORLIX_TCTI_FPSR_QC);
}

static void orlix_tcti_scalar_fp16_fused_three_source_is_single_step(
	struct kunit *test)
{
	struct orlix_tcti_decoded_instruction decoded = {
		.decode_class = ORLIX_TCTI_DECODE_FP_SCALAR_3SOURCE,
		.rd = 0,
		.rn = 1,
		.rm = 2,
		.ra = 3,
		.access_size = sizeof(u16),
		.result_size = sizeof(u16),
		.fp3_op = ORLIX_TCTI_FP3_FMADD,
	};

	orlix_tcti_scalar_fp16_execute(test, &decoded, 0x3e00U, 0x4000U, 0x3800U,
		0, ORLIX_TCTI_FPSR_QC, 0x4300U, ORLIX_TCTI_FPSR_QC);
}

static void orlix_tcti_scalar_fp16_reserved_width_is_rejected(struct kunit *test)
{
	struct orlix_tcti_decoded_instruction decoded = {
		.decode_class = ORLIX_TCTI_DECODE_FP_SCALAR_2SOURCE,
		.rd = 0,
		.rn = 1,
		.rm = 2,
		.access_size = sizeof(u8),
		.result_size = sizeof(u8),
		.fp2_op = ORLIX_TCTI_FP2_FADD,
	};
	struct pt_regs regs = { .pc = 0xa100 };
	int ret;

	ret = orlix_tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);
	KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, ret);
	KUNIT_EXPECT_EQ(test, 0xa100ULL, regs.pc);
}

/*
 * These rows bind the direct scalar conversion encodings to the production
 * resume path.  The pinned source exposes the operation identifier and
 * encoding. External DDI0602 provenance remains orthogonal, so this execution
 * evidence must not discharge the scalar FP semantic obligation.
 */
struct orlix_tcti_scalar_fp_convert_source_row {
	u16 ordinal;
	const char *source_id;
	u32 instruction;
	enum orlix_tcti_fp_int_convert_op operation;
	bool gpr_to_fp;
	u64 input;
	u64 expected;
	unsigned long expected_fpsr;
};

#define ORLIX_TCTI_SCALAR_FP_QC ORLIX_TCTI_FPSR_QC
#define ORLIX_TCTI_SCALAR_FP_QC_IXC (ORLIX_TCTI_FPSR_QC | ORLIX_TCTI_FPSR_IXC)

static const struct orlix_tcti_scalar_fp_convert_source_row
orlix_tcti_scalar_fp_convert_source_rows[] = {
	{ 4108U, "FCVTNS_32S_float2int", 0x1e200020U,
	  ORLIX_TCTI_FP_INT_FCVTNS, false, 0x3fc00000U, 2U,
	  ORLIX_TCTI_SCALAR_FP_QC_IXC },
	{ 4109U, "FCVTNU_32S_float2int", 0x1e210020U,
	  ORLIX_TCTI_FP_INT_FCVTNU, false, 0x3fc00000U, 2U,
	  ORLIX_TCTI_SCALAR_FP_QC_IXC },
	{ 4110U, "SCVTF_S32_float2int", 0x1e220020U,
	  ORLIX_TCTI_FP_INT_SCVTF, true, 0xfffffffeU, 0xc0000000U,
	  ORLIX_TCTI_SCALAR_FP_QC },
	{ 4111U, "UCVTF_S32_float2int", 0x1e230020U,
	  ORLIX_TCTI_FP_INT_UCVTF, true, 3U, 0x40400000U,
	  ORLIX_TCTI_SCALAR_FP_QC },
	{ 4112U, "FCVTAS_32S_float2int", 0x1e240020U,
	  ORLIX_TCTI_FP_INT_FCVTAS, false, 0x3fc00000U, 2U,
	  ORLIX_TCTI_SCALAR_FP_QC_IXC },
	{ 4113U, "FCVTAU_32S_float2int", 0x1e250020U,
	  ORLIX_TCTI_FP_INT_FCVTAU, false, 0x3fc00000U, 2U,
	  ORLIX_TCTI_SCALAR_FP_QC_IXC },
	{ 4116U, "FCVTPS_32S_float2int", 0x1e280020U,
	  ORLIX_TCTI_FP_INT_FCVTPS, false, 0x3fc00000U, 2U,
	  ORLIX_TCTI_SCALAR_FP_QC_IXC },
	{ 4117U, "FCVTPU_32S_float2int", 0x1e290020U,
	  ORLIX_TCTI_FP_INT_FCVTPU, false, 0x3fc00000U, 2U,
	  ORLIX_TCTI_SCALAR_FP_QC_IXC },
	{ 4118U, "FCVTMS_32S_float2int", 0x1e300020U,
	  ORLIX_TCTI_FP_INT_FCVTMS, false, 0x3fc00000U, 1U,
	  ORLIX_TCTI_SCALAR_FP_QC_IXC },
	{ 4119U, "FCVTMU_32S_float2int", 0x1e310020U,
	  ORLIX_TCTI_FP_INT_FCVTMU, false, 0x3fc00000U, 1U,
	  ORLIX_TCTI_SCALAR_FP_QC_IXC },
	{ 4120U, "FCVTZS_32S_float2int", 0x1e380020U,
	  ORLIX_TCTI_FP_INT_FCVTZS, false, 0x3fc00000U, 1U,
	  ORLIX_TCTI_SCALAR_FP_QC_IXC },
	{ 4121U, "FCVTZU_32S_float2int", 0x1e390020U,
	  ORLIX_TCTI_FP_INT_FCVTZU, false, 0x3fc00000U, 1U,
	  ORLIX_TCTI_SCALAR_FP_QC_IXC },
	{ 4084U, "SCVTF_S32_float2fix", 0x1e02fc20U,
	  ORLIX_TCTI_FP_INT_SCVTF_FIXED, true, 1U, 0x3f000000U,
	  ORLIX_TCTI_SCALAR_FP_QC },
	{ 4085U, "UCVTF_S32_float2fix", 0x1e03fc20U,
	  ORLIX_TCTI_FP_INT_UCVTF_FIXED, true, 1U, 0x3f000000U,
	  ORLIX_TCTI_SCALAR_FP_QC },
	{ 4086U, "FCVTZS_32S_float2fix", 0x1e18fc20U,
	  ORLIX_TCTI_FP_INT_FCVTZS_FIXED, false, 0x3f000000U, 1U,
	  ORLIX_TCTI_SCALAR_FP_QC },
	{ 4087U, "FCVTZU_32S_float2fix", 0x1e19fc20U,
	  ORLIX_TCTI_FP_INT_FCVTZU_FIXED, false, 0x3f000000U, 1U,
	  ORLIX_TCTI_SCALAR_FP_QC },
};

static void orlix_tcti_scalar_fp_convert_resume_source_rows(struct kunit *test)
{
	static const u32 svc = 0xd4000001U;
	size_t index;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_scalar_fp_convert_source_rows);
	     index++) {
		const struct orlix_tcti_scalar_fp_convert_source_row *row =
			&orlix_tcti_scalar_fp_convert_source_rows[index];
		const u32 instructions[] = { row->instruction, svc };
		struct orlix_tcti_decoded_instruction decoded =
			orlix_tcti_decode_aarch64(row->instruction);
		struct orlix_tcti_result result;
		struct pt_regs regs = {};
		unsigned long mapped;
		int ret;

		KUNIT_ASSERT_EQ_MSG(test, ORLIX_TCTI_DECODE_FP_INT_CONVERT,
				    decoded.decode_class, "%s source ordinal %u",
				    row->source_id, row->ordinal);
		KUNIT_ASSERT_EQ_MSG(test, row->operation, decoded.fp_int_op,
				    "%s source ordinal %u", row->source_id,
				    row->ordinal);
		KUNIT_ASSERT_EQ(test, 0U, decoded.rd);
		KUNIT_ASSERT_EQ(test, 1U, decoded.rn);

		mapped = orlix_tcti_scalar_fp16_map_instructions(test, instructions,
						     ARRAY_SIZE(instructions));
		KUNIT_ASSERT_NE(test, 0UL, mapped);
		regs.pc = mapped;
		regs.sp = STACK_TOP - 16;
		regs.pstate = PSR_MODE_EL0t;
		regs.syscallno = NO_SYSCALL;
		current->thread.user_fpcr = 0;
		current->thread.user_fpsr = ORLIX_TCTI_SCALAR_FP_QC;
		if (row->gpr_to_fp)
			regs.regs[1] = row->input;
		else {
			current->thread.user_simd[2] = row->input;
			current->thread.user_simd[3] = 0x0123456789abcdefULL;
		}

		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason,
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

static void orlix_tcti_scalar_fp_convert_reserved_forms_exit_without_state_change(
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
		struct orlix_tcti_result result;
		struct pt_regs regs = {};
		struct pt_regs before;
		unsigned long mapped;
		int ret;

		KUNIT_ASSERT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
			orlix_tcti_decode_aarch64(invalid[index]).decode_class);
		mapped = orlix_tcti_scalar_fp16_map_instructions(test, instructions,
						     ARRAY_SIZE(instructions));
		KUNIT_ASSERT_NE(test, 0UL, mapped);
		regs.pc = mapped;
		regs.sp = STACK_TOP - 16;
		regs.pstate = PSR_MODE_EL0t;
		regs.syscallno = NO_SYSCALL;
		regs.regs[0] = 0xaaaaaaaaaaaaaaaaULL;
		current->thread.user_simd[2] = 0x3fc00000U;
		current->thread.user_fpsr = ORLIX_TCTI_SCALAR_FP_QC;
		before = regs;

		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
				result.reason);
		KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, result.status);
		KUNIT_EXPECT_EQ(test, invalid[index], result.instruction);
		KUNIT_EXPECT_EQ(test, before.pc, regs.pc);
		KUNIT_EXPECT_EQ(test, before.regs[0], regs.regs[0]);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_SCALAR_FP_QC,
				current->thread.user_fpsr);

		ret = vm_munmap(mapped, PAGE_SIZE);
		KUNIT_EXPECT_EQ(test, 0, ret);
	}
}

/*
 * These rows bind every currently implemented scalar two-source S and D
 * encoding to the same mapped-RX resume path used by guest EL0 text.  The
 * direct executor tests above retain their focused rounding and NaN cases;
 * this table proves that the decoder, register-bank selection, FP context,
 * and resume loop preserve those semantics for the pinned source leaves.
 */
struct orlix_tcti_scalar_fp_binary_source_row {
	u16 ordinal;
	const char *source_id;
	const char *operation_id;
	u32 source_mask;
	u32 source_pattern;
	u32 instruction;
	enum orlix_tcti_fp_scalar_2source_op operation;
	u8 width;
	u64 left;
	u64 right;
	unsigned long fpcr;
	unsigned long initial_fpsr;
	u64 expected;
	unsigned long expected_fpsr;
};

#define ORLIX_TCTI_SCALAR_FP_BINARY_S(base) ((base) + 0x00020020U)
#define ORLIX_TCTI_SCALAR_FP_BINARY_SOURCE_MASK 0xffe0fc00U
#define ORLIX_TCTI_SCALAR_FP_NZCV \
	(PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT | PSR_V_BIT)

static const char *orlix_tcti_scalar_fp_binary_artifact_string(
	const struct orlix_tcti_target_instruction_artifact *artifact, u32 offset)
{
	if (offset >= artifact->string_pool_size)
		return NULL;

	return (const char *)artifact->string_pool + offset;
}

static const struct orlix_tcti_scalar_fp_binary_source_row
orlix_tcti_scalar_fp_binary_source_rows[] = {
	{ 4308U, "FMUL_S_floatdp2", "FMUL_float",
	  ORLIX_TCTI_SCALAR_FP_BINARY_SOURCE_MASK, 0x1e200800U,
	  ORLIX_TCTI_SCALAR_FP_BINARY_S(0x1e200800U),
	  ORLIX_TCTI_FP2_FMUL, sizeof(u32), 0x3fc00000U, 0x40000000U, 0,
	  ORLIX_TCTI_SCALAR_FP_QC, 0x40400000U, ORLIX_TCTI_SCALAR_FP_QC },
	{ 4309U, "FDIV_S_floatdp2", "FDIV_float",
	  ORLIX_TCTI_SCALAR_FP_BINARY_SOURCE_MASK, 0x1e201800U,
	  ORLIX_TCTI_SCALAR_FP_BINARY_S(0x1e201800U),
	  ORLIX_TCTI_FP2_FDIV, sizeof(u32), 0x3f800000U, 0, 0,
	  ORLIX_TCTI_SCALAR_FP_QC, 0x7f800000U, ORLIX_TCTI_SCALAR_FP_QC | ORLIX_TCTI_FPSR_DZC },
	{ 4310U, "FADD_S_floatdp2", "FADD_float",
	  ORLIX_TCTI_SCALAR_FP_BINARY_SOURCE_MASK, 0x1e202800U,
	  ORLIX_TCTI_SCALAR_FP_BINARY_S(0x1e202800U),
	  ORLIX_TCTI_FP2_FADD, sizeof(u32), 0x3ffc0000U, 0x4b000000U,
	  ORLIX_TCTI_FPCR_RMODE_NEGINF, ORLIX_TCTI_SCALAR_FP_QC, 0x4b000001U,
	  ORLIX_TCTI_SCALAR_FP_QC | ORLIX_TCTI_FPSR_IXC },
	{ 4311U, "FSUB_S_floatdp2", "FSUB_float",
	  ORLIX_TCTI_SCALAR_FP_BINARY_SOURCE_MASK, 0x1e203800U,
	  ORLIX_TCTI_SCALAR_FP_BINARY_S(0x1e203800U),
	  ORLIX_TCTI_FP2_FSUB, sizeof(u32), 0x80000000U, 0, 0,
	  ORLIX_TCTI_SCALAR_FP_QC, 0x80000000U, ORLIX_TCTI_SCALAR_FP_QC },
	{ 4312U, "FMAX_S_floatdp2", "FMAX_float",
	  ORLIX_TCTI_SCALAR_FP_BINARY_SOURCE_MASK, 0x1e204800U,
	  ORLIX_TCTI_SCALAR_FP_BINARY_S(0x1e204800U),
	  ORLIX_TCTI_FP2_FMAX, sizeof(u32), 0, 0x80000000U, 0,
	  ORLIX_TCTI_SCALAR_FP_QC, 0, ORLIX_TCTI_SCALAR_FP_QC },
	{ 4313U, "FMIN_S_floatdp2", "FMIN_float",
	  ORLIX_TCTI_SCALAR_FP_BINARY_SOURCE_MASK, 0x1e205800U,
	  ORLIX_TCTI_SCALAR_FP_BINARY_S(0x1e205800U),
	  ORLIX_TCTI_FP2_FMIN, sizeof(u32), 0, 0x80000000U, 0,
	  ORLIX_TCTI_SCALAR_FP_QC, 0x80000000U, ORLIX_TCTI_SCALAR_FP_QC },
	{ 4314U, "FMAXNM_S_floatdp2", "FMAXNM_float",
	  ORLIX_TCTI_SCALAR_FP_BINARY_SOURCE_MASK, 0x1e206800U,
	  ORLIX_TCTI_SCALAR_FP_BINARY_S(0x1e206800U),
	  ORLIX_TCTI_FP2_FMAXNM, sizeof(u32), 0x7fc00001U, 0x40000000U, 0,
	  ORLIX_TCTI_SCALAR_FP_QC, 0x40000000U, ORLIX_TCTI_SCALAR_FP_QC },
	{ 4315U, "FMINNM_S_floatdp2", "FMINNM_float",
	  ORLIX_TCTI_SCALAR_FP_BINARY_SOURCE_MASK, 0x1e207800U,
	  ORLIX_TCTI_SCALAR_FP_BINARY_S(0x1e207800U),
	  ORLIX_TCTI_FP2_FMINNM, sizeof(u32), 0x7fc00001U, 0x40000000U, 0,
	  ORLIX_TCTI_SCALAR_FP_QC, 0x40000000U, ORLIX_TCTI_SCALAR_FP_QC },
	{ 4316U, "FNMUL_S_floatdp2", "FNMUL_float",
	  ORLIX_TCTI_SCALAR_FP_BINARY_SOURCE_MASK, 0x1e208800U,
	  ORLIX_TCTI_SCALAR_FP_BINARY_S(0x1e208800U),
	  ORLIX_TCTI_FP2_FNMUL, sizeof(u32), 0x3fc00000U, 0x40000000U, 0,
	  ORLIX_TCTI_SCALAR_FP_QC, 0xc0400000U, ORLIX_TCTI_SCALAR_FP_QC },
	{ 4317U, "FMUL_D_floatdp2", "FMUL_float",
	  ORLIX_TCTI_SCALAR_FP_BINARY_SOURCE_MASK, 0x1e600800U,
	  ORLIX_TCTI_SCALAR_FP_BINARY_S(0x1e600800U),
	  ORLIX_TCTI_FP2_FMUL, sizeof(u64), 0x3ff8000000000000ULL,
	  0x4000000000000000ULL, 0, ORLIX_TCTI_SCALAR_FP_QC,
	  0x4008000000000000ULL, ORLIX_TCTI_SCALAR_FP_QC },
	{ 4318U, "FDIV_D_floatdp2", "FDIV_float",
	  ORLIX_TCTI_SCALAR_FP_BINARY_SOURCE_MASK, 0x1e601800U,
	  ORLIX_TCTI_SCALAR_FP_BINARY_S(0x1e601800U),
	  ORLIX_TCTI_FP2_FDIV, sizeof(u64), 0x3ff0000000000000ULL, 0, 0,
	  ORLIX_TCTI_SCALAR_FP_QC, 0x7ff0000000000000ULL,
	  ORLIX_TCTI_SCALAR_FP_QC | ORLIX_TCTI_FPSR_DZC },
	{ 4319U, "FADD_D_floatdp2", "FADD_float",
	  ORLIX_TCTI_SCALAR_FP_BINARY_SOURCE_MASK, 0x1e602800U,
	  ORLIX_TCTI_SCALAR_FP_BINARY_S(0x1e602800U),
	  ORLIX_TCTI_FP2_FADD, sizeof(u64), 0x3ff8000000000000ULL,
	  0x4000000000000000ULL, ORLIX_TCTI_FPCR_RMODE_NEGINF,
	  ORLIX_TCTI_SCALAR_FP_QC, 0x400c000000000000ULL, ORLIX_TCTI_SCALAR_FP_QC },
	{ 4320U, "FSUB_D_floatdp2", "FSUB_float",
	  ORLIX_TCTI_SCALAR_FP_BINARY_SOURCE_MASK, 0x1e603800U,
	  ORLIX_TCTI_SCALAR_FP_BINARY_S(0x1e603800U),
	  ORLIX_TCTI_FP2_FSUB, sizeof(u64), 0x8000000000000000ULL, 0, 0,
	  ORLIX_TCTI_SCALAR_FP_QC, 0x8000000000000000ULL, ORLIX_TCTI_SCALAR_FP_QC },
	{ 4321U, "FMAX_D_floatdp2", "FMAX_float",
	  ORLIX_TCTI_SCALAR_FP_BINARY_SOURCE_MASK, 0x1e604800U,
	  ORLIX_TCTI_SCALAR_FP_BINARY_S(0x1e604800U),
	  ORLIX_TCTI_FP2_FMAX, sizeof(u64), 0, 0x8000000000000000ULL, 0,
	  ORLIX_TCTI_SCALAR_FP_QC, 0, ORLIX_TCTI_SCALAR_FP_QC },
	{ 4322U, "FMIN_D_floatdp2", "FMIN_float",
	  ORLIX_TCTI_SCALAR_FP_BINARY_SOURCE_MASK, 0x1e605800U,
	  ORLIX_TCTI_SCALAR_FP_BINARY_S(0x1e605800U),
	  ORLIX_TCTI_FP2_FMIN, sizeof(u64), 0, 0x8000000000000000ULL, 0,
	  ORLIX_TCTI_SCALAR_FP_QC, 0x8000000000000000ULL, ORLIX_TCTI_SCALAR_FP_QC },
	{ 4323U, "FMAXNM_D_floatdp2", "FMAXNM_float",
	  ORLIX_TCTI_SCALAR_FP_BINARY_SOURCE_MASK, 0x1e606800U,
	  ORLIX_TCTI_SCALAR_FP_BINARY_S(0x1e606800U),
	  ORLIX_TCTI_FP2_FMAXNM, sizeof(u64), 0x7ff8000000000001ULL,
	  0x4000000000000000ULL, 0, ORLIX_TCTI_SCALAR_FP_QC,
	  0x4000000000000000ULL, ORLIX_TCTI_SCALAR_FP_QC },
	{ 4324U, "FMINNM_D_floatdp2", "FMINNM_float",
	  ORLIX_TCTI_SCALAR_FP_BINARY_SOURCE_MASK, 0x1e607800U,
	  ORLIX_TCTI_SCALAR_FP_BINARY_S(0x1e607800U),
	  ORLIX_TCTI_FP2_FMINNM, sizeof(u64), 0x7ff8000000000001ULL,
	  0x4000000000000000ULL, 0, ORLIX_TCTI_SCALAR_FP_QC,
	  0x4000000000000000ULL, ORLIX_TCTI_SCALAR_FP_QC },
	{ 4325U, "FNMUL_D_floatdp2", "FNMUL_float",
	  ORLIX_TCTI_SCALAR_FP_BINARY_SOURCE_MASK, 0x1e608800U,
	  ORLIX_TCTI_SCALAR_FP_BINARY_S(0x1e608800U),
	  ORLIX_TCTI_FP2_FNMUL, sizeof(u64), 0x3ff8000000000000ULL,
	  0x4000000000000000ULL, 0, ORLIX_TCTI_SCALAR_FP_QC,
	  0xc008000000000000ULL, ORLIX_TCTI_SCALAR_FP_QC },
};

static void orlix_tcti_scalar_fp_binary_resume_source_rows(struct kunit *test)
{
	static const u32 svc = 0xd4000001U;
	const struct orlix_tcti_target_instruction_artifact *artifact =
		orlix_tcti_target_instruction_artifact_canonical();
	struct orlix_tcti_target_instruction_artifact_validation_result validation;
	size_t index;

	KUNIT_ASSERT_NOT_NULL(test, artifact);
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_target_instruction_artifact_validate(artifact, &validation));
	KUNIT_ASSERT_GT(test, artifact->leaf_count, 4325U);
	for (index = 0; index < ARRAY_SIZE(orlix_tcti_scalar_fp_binary_source_rows);
	     index++) {
		const struct orlix_tcti_scalar_fp_binary_source_row *row =
			&orlix_tcti_scalar_fp_binary_source_rows[index];
		const struct orlix_tcti_target_instruction_artifact_leaf *source =
			&artifact->leaves[row->ordinal];
		const char *source_id = orlix_tcti_scalar_fp_binary_artifact_string(
			artifact, source->name_offset);
		const char *operation_id = orlix_tcti_scalar_fp_binary_artifact_string(
			artifact, source->operation_offset);
		const u32 instructions[] = { row->instruction, svc };
		struct orlix_tcti_decoded_instruction decoded =
			orlix_tcti_decode_aarch64(row->instruction);
		struct orlix_tcti_result result;
		struct pt_regs regs = {};
		struct pt_regs before;
		unsigned long mapped;
		int ret;

		KUNIT_ASSERT_EQ_MSG(test, ORLIX_TCTI_DECODE_FP_SCALAR_2SOURCE,
				    decoded.decode_class, "%s source ordinal %u",
				    row->source_id, row->ordinal);
		KUNIT_ASSERT_EQ_MSG(test, row->operation, decoded.fp2_op,
				    "%s source ordinal %u", row->source_id,
				    row->ordinal);
		KUNIT_ASSERT_EQ(test, row->width, decoded.access_size);
		KUNIT_ASSERT_EQ(test, row->width, decoded.result_size);
		KUNIT_ASSERT_EQ(test, 0U, decoded.rd);
		KUNIT_ASSERT_EQ(test, 1U, decoded.rn);
		KUNIT_ASSERT_EQ(test, 2U, decoded.rm);
		KUNIT_ASSERT_NOT_NULL_MSG(test, source_id, "ordinal=%u",
				      row->ordinal);
		KUNIT_ASSERT_NOT_NULL_MSG(test, operation_id, "ordinal=%u",
				      row->ordinal);
		KUNIT_EXPECT_STREQ(test, row->source_id, source_id);
		KUNIT_EXPECT_STREQ(test, row->operation_id, operation_id);
		KUNIT_EXPECT_EQ(test, row->source_mask, source->encoding_mask);
		KUNIT_EXPECT_EQ(test, row->source_pattern,
				source->encoding_pattern);
		KUNIT_EXPECT_EQ(test, row->source_pattern,
				row->instruction & row->source_mask);
		KUNIT_EXPECT_EQ(test, source->encoding_pattern,
				row->instruction & source->encoding_mask);

		mapped = orlix_tcti_scalar_fp16_map_instructions(test, instructions,
						     ARRAY_SIZE(instructions));
		KUNIT_ASSERT_NE(test, 0UL, mapped);
		regs.pc = mapped;
		regs.sp = STACK_TOP - 16;
		regs.pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_C_BIT;
		regs.syscallno = NO_SYSCALL;
		regs.regs[6] = 0x0123456789abcdefULL;
		regs.regs[19] = 0xfedcba9876543210ULL;
		current->thread.user_simd[0] = 0xaaaaaaaaaaaaaaaaULL;
		current->thread.user_simd[1] = 0xbbbbbbbbbbbbbbbbULL;
		current->thread.user_simd[2] = row->left;
		current->thread.user_simd[3] = 0x0123456789abcdefULL;
		current->thread.user_simd[4] = row->right;
		current->thread.user_simd[5] = 0xfedcba9876543210ULL;
		current->thread.user_fpcr = row->fpcr;
		current->thread.user_fpsr = row->initial_fpsr;
		before = regs;

		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason,
				    "%s source ordinal %u", row->source_id,
				    row->ordinal);
		KUNIT_EXPECT_EQ(test, 0L, result.status);
		KUNIT_EXPECT_EQ(test, mapped + sizeof(u32), result.pc);
		KUNIT_EXPECT_EQ(test, svc, result.instruction);
		KUNIT_EXPECT_EQ(test, mapped + sizeof(u32), regs.pc);
		before.pc += sizeof(u32);
		KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
		KUNIT_EXPECT_EQ(test, row->expected, current->thread.user_simd[0]);
		KUNIT_EXPECT_EQ(test, 0ULL, current->thread.user_simd[1]);
		KUNIT_EXPECT_EQ(test, row->left, current->thread.user_simd[2]);
		KUNIT_EXPECT_EQ(test, 0x0123456789abcdefULL,
				current->thread.user_simd[3]);
		KUNIT_EXPECT_EQ(test, row->right, current->thread.user_simd[4]);
		KUNIT_EXPECT_EQ(test, 0xfedcba9876543210ULL,
				current->thread.user_simd[5]);
		KUNIT_EXPECT_EQ(test, row->fpcr, current->thread.user_fpcr);
		KUNIT_EXPECT_EQ(test, row->expected_fpsr,
				current->thread.user_fpsr);

		ret = vm_munmap(mapped, PAGE_SIZE);
		KUNIT_EXPECT_EQ(test, 0, ret);
	}
}

/*
 * This is production-path execution evidence for the pinned compare leaves.
 * External DDI0602 provenance grants no semantic-proof credit.
 */
struct orlix_tcti_scalar_fp_compare_source_row {
	u16 ordinal;
	const char *source_id;
	const char *operation_id;
	u32 source_mask;
	u32 source_pattern;
	u32 instruction;
	u8 width;
	bool immediate;
	bool signal_all_nans;
	u64 left;
	u64 right;
	unsigned long expected_nzcv;
	unsigned long expected_fpsr;
};

static const struct orlix_tcti_scalar_fp_compare_source_row
orlix_tcti_scalar_fp_compare_source_rows[] = {
	{ 4287U, "FCMP_S_floatcmp", "FCMP_float", 0xffe0fc1fU,
	  0x1e202000U, 0x1e222020U, sizeof(u32), false, false,
	  0x3f800000U, 0x40000000U, PSR_N_BIT, ORLIX_TCTI_FPSR_QC },
	{ 4288U, "FCMP_SZ_floatcmp", "FCMP_float", 0xfffffc1fU,
	  0x1e202008U, 0x1e202028U, sizeof(u32), true, false,
	  0x80000000U, 0, PSR_Z_BIT | PSR_C_BIT, ORLIX_TCTI_FPSR_QC },
	{ 4289U, "FCMPE_S_floatcmp", "FCMPE_float", 0xffe0fc1fU,
	  0x1e202010U, 0x1e222030U, sizeof(u32), false, true,
	  0x7fc00001U, 0x3f800000U, PSR_C_BIT | PSR_V_BIT,
	  ORLIX_TCTI_FPSR_QC | ORLIX_TCTI_FPSR_IOC },
	{ 4290U, "FCMPE_SZ_floatcmp", "FCMPE_float", 0xfffffc1fU,
	  0x1e202018U, 0x1e202038U, sizeof(u32), true, true,
	  0x7fc00001U, 0, PSR_C_BIT | PSR_V_BIT,
	  ORLIX_TCTI_FPSR_QC | ORLIX_TCTI_FPSR_IOC },
	{ 4291U, "FCMP_D_floatcmp", "FCMP_float", 0xffe0fc1fU,
	  0x1e602000U, 0x1e622020U, sizeof(u64), false, false,
	  0x4000000000000000ULL, 0x3ff0000000000000ULL, PSR_C_BIT,
	  ORLIX_TCTI_FPSR_QC },
	{ 4292U, "FCMP_DZ_floatcmp", "FCMP_float", 0xfffffc1fU,
	  0x1e602008U, 0x1e602028U, sizeof(u64), true, false,
	  0x8000000000000000ULL, 0, PSR_Z_BIT | PSR_C_BIT,
	  ORLIX_TCTI_FPSR_QC },
	{ 4293U, "FCMPE_D_floatcmp", "FCMPE_float", 0xffe0fc1fU,
	  0x1e602010U, 0x1e622030U, sizeof(u64), false, true,
	  0x7ff8000000000001ULL, 0x3ff0000000000000ULL,
	  PSR_C_BIT | PSR_V_BIT, ORLIX_TCTI_FPSR_QC | ORLIX_TCTI_FPSR_IOC },
	{ 4294U, "FCMPE_DZ_floatcmp", "FCMPE_float", 0xfffffc1fU,
	  0x1e602018U, 0x1e602038U, sizeof(u64), true, true,
	  0x7ff8000000000001ULL, 0, PSR_C_BIT | PSR_V_BIT,
	  ORLIX_TCTI_FPSR_QC | ORLIX_TCTI_FPSR_IOC },
	/* FCMP signals only signaling NaNs, unlike FCMPE above. */
	{ 4287U, "FCMP_S_floatcmp", "FCMP_float", 0xffe0fc1fU,
	  0x1e202000U, 0x1e222020U, sizeof(u32), false, false,
	  0x7fc00001U, 0x3f800000U, PSR_C_BIT | PSR_V_BIT,
	  ORLIX_TCTI_FPSR_QC },
	{ 4288U, "FCMP_SZ_floatcmp", "FCMP_float", 0xfffffc1fU,
	  0x1e202008U, 0x1e202028U, sizeof(u32), true, false,
	  0x7f800001U, 0, PSR_C_BIT | PSR_V_BIT,
	  ORLIX_TCTI_FPSR_QC | ORLIX_TCTI_FPSR_IOC },
	{ 4291U, "FCMP_D_floatcmp", "FCMP_float", 0xffe0fc1fU,
	  0x1e602000U, 0x1e622020U, sizeof(u64), false, false,
	  0x7ff8000000000001ULL, 0x3ff0000000000000ULL,
	  PSR_C_BIT | PSR_V_BIT, ORLIX_TCTI_FPSR_QC },
	{ 4292U, "FCMP_DZ_floatcmp", "FCMP_float", 0xfffffc1fU,
	  0x1e602008U, 0x1e602028U, sizeof(u64), true, false,
	  0x7ff0000000000001ULL, 0, PSR_C_BIT | PSR_V_BIT,
	  ORLIX_TCTI_FPSR_QC | ORLIX_TCTI_FPSR_IOC },
};

static void orlix_tcti_scalar_fp_compare_resume_source_rows(struct kunit *test)
{
	static const u32 svc = 0xd4000001U;
	const struct orlix_tcti_target_instruction_artifact *artifact =
		orlix_tcti_target_instruction_artifact_canonical();
	struct orlix_tcti_target_instruction_artifact_validation_result validation;
	size_t index;

	KUNIT_ASSERT_NOT_NULL(test, artifact);
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_target_instruction_artifact_validate(artifact, &validation));
	KUNIT_ASSERT_GT(test, artifact->leaf_count, 4294U);
	for (index = 0; index < ARRAY_SIZE(orlix_tcti_scalar_fp_compare_source_rows);
	     index++) {
		const struct orlix_tcti_scalar_fp_compare_source_row *row =
			&orlix_tcti_scalar_fp_compare_source_rows[index];
		const struct orlix_tcti_target_instruction_artifact_leaf *source =
			&artifact->leaves[row->ordinal];
		const char *source_id = orlix_tcti_scalar_fp_binary_artifact_string(
			artifact, source->name_offset);
		const char *operation_id = orlix_tcti_scalar_fp_binary_artifact_string(
			artifact, source->operation_offset);
		const u32 instructions[] = { row->instruction, svc };
		struct orlix_tcti_decoded_instruction decoded =
			orlix_tcti_decode_aarch64(row->instruction);
		struct orlix_tcti_result result;
		struct pt_regs regs = {};
		struct pt_regs expected;
		u64 simd_before[ARRAY_SIZE(current->thread.user_simd)];
		unsigned long mapped;
		size_t register_index;
		int ret;

		KUNIT_ASSERT_EQ_MSG(test, ORLIX_TCTI_DECODE_FP_SCALAR_COMPARE,
				    decoded.decode_class, "%s source ordinal %u",
				    row->source_id, row->ordinal);
		KUNIT_ASSERT_EQ(test, row->width, decoded.access_size);
		KUNIT_ASSERT_EQ(test, row->width, decoded.result_size);
		KUNIT_ASSERT_EQ(test, row->immediate, decoded.immediate);
		KUNIT_ASSERT_EQ(test, row->signal_all_nans,
				decoded.fp_signal_all_nans);
		KUNIT_ASSERT_EQ(test, 1U, decoded.rn);
		KUNIT_ASSERT_EQ(test, row->immediate ? 0U : 2U, decoded.rm);
		KUNIT_ASSERT_NOT_NULL_MSG(test, source_id, "ordinal=%u",
				      row->ordinal);
		KUNIT_ASSERT_NOT_NULL_MSG(test, operation_id, "ordinal=%u",
				      row->ordinal);
		KUNIT_EXPECT_STREQ(test, row->source_id, source_id);
		KUNIT_EXPECT_STREQ(test, row->operation_id, operation_id);
		KUNIT_EXPECT_EQ(test, row->source_mask, source->encoding_mask);
		KUNIT_EXPECT_EQ(test, row->source_pattern,
				source->encoding_pattern);
		KUNIT_EXPECT_EQ(test, row->source_pattern,
				row->instruction & row->source_mask);
		KUNIT_EXPECT_EQ(test, source->encoding_pattern,
				row->instruction & source->encoding_mask);

		mapped = orlix_tcti_scalar_fp16_map_instructions(test, instructions,
						     ARRAY_SIZE(instructions));
		KUNIT_ASSERT_NE(test, 0UL, mapped);
		regs.pc = mapped;
		regs.sp = STACK_TOP - 16;
		regs.pstate = PSR_MODE_EL0t | ORLIX_TCTI_SCALAR_FP_NZCV;
		for (register_index = 0; register_index < ARRAY_SIZE(regs.regs);
		     register_index++)
			regs.regs[register_index] = 0x9e3779b97f4a7c15ULL ^
				((u64)register_index << 32) ^ register_index;
		regs.orig_x0 = 0xd1b54a32d192ed03ULL;
		regs.syscallno = NO_SYSCALL;
		regs.unused = 0x6d5a56c3U;
		memset(current->thread.user_simd, 0x5a,
		       sizeof(current->thread.user_simd));
		current->thread.user_simd[2] = row->left;
		current->thread.user_simd[3] = 0x0123456789abcdefULL;
		current->thread.user_simd[4] = row->right;
		current->thread.user_simd[5] = 0xfedcba9876543210ULL;
		current->thread.user_simd_valid = true;
		current->thread.user_fpcr = ORLIX_TCTI_FPCR_RMODE_ZERO;
		current->thread.user_fpsr = ORLIX_TCTI_FPSR_QC;
		memcpy(simd_before, current->thread.user_simd,
		       sizeof(simd_before));
		expected = regs;
		expected.pc += sizeof(u32);
		expected.pstate &= ~ORLIX_TCTI_SCALAR_FP_NZCV;
		expected.pstate |= row->expected_nzcv;

		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason,
				    "%s source ordinal %u", row->source_id,
				    row->ordinal);
		KUNIT_EXPECT_EQ(test, 0L, result.status);
		KUNIT_EXPECT_EQ(test, mapped + sizeof(u32), result.pc);
		KUNIT_EXPECT_EQ(test, svc, result.instruction);
		KUNIT_EXPECT_MEMEQ(test, &expected, &regs, sizeof(regs));
		KUNIT_EXPECT_MEMEQ(test, simd_before, current->thread.user_simd,
				  sizeof(simd_before));
		KUNIT_EXPECT_TRUE(test, current->thread.user_simd_valid);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_FPCR_RMODE_ZERO,
				current->thread.user_fpcr);
		KUNIT_EXPECT_EQ(test, row->expected_fpsr,
				current->thread.user_fpsr);

		ret = vm_munmap(mapped, PAGE_SIZE);
		KUNIT_EXPECT_EQ(test, 0, ret);
	}
}

#undef ORLIX_TCTI_SCALAR_FP_BINARY_S
#undef ORLIX_TCTI_SCALAR_FP_NZCV

static struct kunit_case orlix_tcti_scalar_fp_semantics_test_cases[] = {
	KUNIT_CASE(orlix_tcti_scalar_fp16_decode_pinned_legal_and_reserved),
	KUNIT_CASE(orlix_tcti_scalar_fp16_resume_user_is_runtime_gated),
	KUNIT_CASE(orlix_tcti_scalar_fp16_compare_select_is_runtime_gated),
	KUNIT_CASE(orlix_tcti_scalar_fp16_fccmp_is_runtime_gated),
	KUNIT_CASE(orlix_tcti_scalar_fp_fadd_honors_rounding_and_inexact),
	KUNIT_CASE(orlix_tcti_scalar_fp_fadd_quiets_signaling_nan),
	KUNIT_CASE(orlix_tcti_scalar_fp_fsub_preserves_zero_sign),
	KUNIT_CASE(orlix_tcti_scalar_fp_zero_compare_respects_s_and_d_widths),
	KUNIT_CASE(orlix_tcti_simd_fp16_three_same_preserves_fp_environment),
	KUNIT_CASE(orlix_tcti_simd_fp16_three_same_nan_zero_and_accumulate),
	KUNIT_CASE(orlix_tcti_simd_fp16_three_same_rejects_unknown_operation),
	KUNIT_CASE(orlix_tcti_simd_fp16_three_same_pairwise_and_compare_lanes),
	KUNIT_CASE(orlix_tcti_advsimd_fp16_unimplemented_source_leaves_are_rejected),
	KUNIT_CASE(orlix_tcti_advsimd_fp16_resume_is_runtime_gated),
	KUNIT_CASE(orlix_tcti_scalar_fp16_unary_rounding_and_sqrt),
	KUNIT_CASE(orlix_tcti_scalar_fp16_two_source_preserves_fp_state),
	KUNIT_CASE(orlix_tcti_scalar_fp16_fused_three_source_is_single_step),
	KUNIT_CASE(orlix_tcti_scalar_fp16_reserved_width_is_rejected),
	KUNIT_CASE(orlix_tcti_scalar_fp_convert_resume_source_rows),
	KUNIT_CASE(orlix_tcti_scalar_fp_convert_reserved_forms_exit_without_state_change),
	KUNIT_CASE(orlix_tcti_scalar_fp_binary_resume_source_rows),
	KUNIT_CASE(orlix_tcti_scalar_fp_compare_resume_source_rows),
	{}
};

static int orlix_tcti_scalar_fp_semantics_test_init(struct kunit *test)
{
	struct mm_struct *mm = mm_alloc();

	if (!mm)
		return -ENOMEM;
	kthread_use_mm(mm);
	test->priv = mm;
	return 0;
}

static void orlix_tcti_scalar_fp_semantics_test_exit(struct kunit *test)
{
	struct mm_struct *mm = test->priv;

	if (!mm)
		return;
	kthread_unuse_mm(mm);
	mmput(mm);
}

static struct kunit_suite orlix_tcti_scalar_fp_semantics_test_suite = {
	.name = "orlix-tcti-scalar-fp-semantics",
	.init = orlix_tcti_scalar_fp_semantics_test_init,
	.exit = orlix_tcti_scalar_fp_semantics_test_exit,
	.test_cases = orlix_tcti_scalar_fp_semantics_test_cases,
};

kunit_test_suite(orlix_tcti_scalar_fp_semantics_test_suite);

MODULE_LICENSE("GPL");
