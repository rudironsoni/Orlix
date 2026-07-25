// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/bitops.h>
#include <linux/sched.h>

#include <asm/ptrace.h>

#include "../decode_aarch64.h"
#include "../switch_debug.h"

#define TCTI_FPCR_RMODE_POSINF BIT(22)
#define TCTI_FPCR_RMODE_NEGINF BIT(23)
#define TCTI_FPCR_RMODE_ZERO GENMASK(23, 22)
#define TCTI_FPSR_IOC BIT(0)
#define TCTI_FPSR_IXC BIT(4)
#define TCTI_FPSR_QC BIT(27)

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

static struct kunit_case tcti_scalar_fp_semantics_test_cases[] = {
	KUNIT_CASE(tcti_scalar_fp_fadd_honors_rounding_and_inexact),
	KUNIT_CASE(tcti_scalar_fp_fadd_quiets_signaling_nan),
	KUNIT_CASE(tcti_scalar_fp_fsub_preserves_zero_sign),
	KUNIT_CASE(tcti_scalar_fp_zero_compare_respects_s_and_d_widths),
	{}
};

static struct kunit_suite tcti_scalar_fp_semantics_test_suite = {
	.name = "orlix-tcti-scalar-fp-semantics",
	.test_cases = tcti_scalar_fp_semantics_test_cases,
};

kunit_test_suite(tcti_scalar_fp_semantics_test_suite);

MODULE_LICENSE("GPL");
