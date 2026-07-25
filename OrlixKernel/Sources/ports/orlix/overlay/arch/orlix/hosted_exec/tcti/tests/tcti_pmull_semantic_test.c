// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/bitops.h>
#include <linux/string.h>
#include <asm/ptrace.h>

#include "../decode_aarch64.h"
#include "../switch_debug.h"

#define TCTI_PMULL_PATTERN	0x0e20e000U

/*
 * This is deliberately an independent bit-at-a-time polynomial product.
 * The executor may use whatever implementation is appropriate, but the
 * expected value here is defined solely by multiplication over GF(2).
 */
static u16 tcti_pmull_test_u8(u8 left, u8 right)
{
	u16 result = 0;
	u8 bit;

	for (bit = 0; bit < 8; bit++)
		if (right & BIT(bit))
			result ^= (u16)left << bit;
	return result;
}

static void tcti_pmull_test_u64(u64 left, u64 right, u64 *low, u64 *high)
{
	u64 result_low = 0;
	u64 result_high = 0;
	u8 bit;

	for (bit = 0; bit < 64; bit++) {
		if (!(right & BIT_ULL(bit)))
			continue;
		result_low ^= left << bit;
		if (bit)
			result_high ^= left >> (64 - bit);
	}
	*low = result_low;
	*high = result_high;
}

static void tcti_pmull_test_expected(u8 size, u64 left, u64 right,
				      u64 *low, u64 *high)
{
	if (!size) {
		u64 result_low = 0;
		u64 result_high = 0;
		u8 lane;

		for (lane = 0; lane < 8; lane++) {
			u16 product = tcti_pmull_test_u8(left >> (lane * 8),
						  right >> (lane * 8));

			if (lane < 4)
				result_low |= (u64)product << (lane * 16);
			else
				result_high |= (u64)product << ((lane - 4) * 16);
		}
		*low = result_low;
		*high = result_high;
		return;
	}
	tcti_pmull_test_u64(left, right, low, high);
}

static u32 tcti_pmull_test_instruction(u8 size, u8 q, u8 rd, u8 rn, u8 rm)
{
	return TCTI_PMULL_PATTERN | ((u32)size << 22) |
		(q ? BIT(30) : 0) | ((u32)rm << 16) | ((u32)rn << 5) | rd;
}

static void tcti_pmull_test_initialize_state(u32 instruction)
{
	u8 word;

	for (word = 0; word < ARRAY_SIZE(current->thread.user_simd); word++)
		current->thread.user_simd[word] =
			0xd1b54a32d192ed03ULL ^ ((u64)instruction << (word & 15)) ^
			rol64(0x9e3779b97f4a7c15ULL, word) ^ word;
}

static void tcti_pmull_test_expect_execution(struct kunit *test, u8 size,
					      u8 q, u8 rd, u8 rn, u8 rm)
{
	u32 instruction = tcti_pmull_test_instruction(size, q, rd, rn, rm);
	struct tcti_decoded_instruction decoded = tcti_decode_aarch64(instruction);
	struct pt_regs regs = {
		.pc = 0x12345678000ULL,
		.sp = 0x1234567ULL,
		.pstate = PSR_N_BIT | PSR_Z_BIT | 0x155UL,
	};
	struct pt_regs before_regs;
	u64 before_simd[ARRAY_SIZE(current->thread.user_simd)];
	u64 expected_simd[ARRAY_SIZE(current->thread.user_simd)];
	u64 expected_low;
	u64 expected_high;
	u64 left;
	u64 right;
	int ret;

	KUNIT_ASSERT_EQ(test, TCTI_DECODE_SIMD_VECTOR_ARITHMETIC,
			decoded.decode_class);
	KUNIT_ASSERT_EQ(test, TCTI_SIMD_ARITH_PMULL,
			decoded.simd_arithmetic_op);
	KUNIT_ASSERT_EQ(test, BIT(size), decoded.access_size);
	KUNIT_ASSERT_EQ(test, 2 * sizeof(u64), decoded.result_size);
	KUNIT_ASSERT_EQ(test, q, decoded.simd_source_index);
	KUNIT_ASSERT_EQ(test, rd, decoded.rd);
	KUNIT_ASSERT_EQ(test, rn, decoded.rn);
	KUNIT_ASSERT_EQ(test, rm, decoded.rm);

	regs.regs[0] = 0x0123456789abcdefULL;
	regs.regs[17] = 0xfedcba9876543210ULL;
	before_regs = regs;
	tcti_pmull_test_initialize_state(instruction);
	memcpy(before_simd, current->thread.user_simd, sizeof(before_simd));
	memcpy(expected_simd, before_simd, sizeof(expected_simd));
	left = before_simd[rn * 2 + q];
	right = before_simd[rm * 2 + q];
	tcti_pmull_test_expected(size, left, right, &expected_low,
					  &expected_high);
	expected_simd[rd * 2] = expected_low;
	expected_simd[rd * 2 + 1] = expected_high;
	current->thread.user_simd_valid = 0;
	current->thread.user_fpsr = BIT(7);

	ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL);
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_MEMEQ(test, expected_simd, current->thread.user_simd,
			   sizeof(expected_simd));
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, BIT(7), current->thread.user_fpsr);
	KUNIT_EXPECT_MEMEQ(test, before_regs.regs, regs.regs,
			   sizeof(regs.regs));
	KUNIT_EXPECT_EQ(test, before_regs.sp, regs.sp);
	KUNIT_EXPECT_EQ(test, before_regs.pstate, regs.pstate);
	KUNIT_EXPECT_EQ(test, before_regs.pc + sizeof(u32), regs.pc);
}

static void tcti_pmull_semantic_all_register_combinations(struct kunit *test)
{
	u8 size;
	u8 q;
	u8 rd;
	u8 rn;
	u8 rm;

	for (size = 0; size <= 3; size += 3)
		for (q = 0; q < 2; q++)
			for (rd = 0; rd < 32; rd++)
				for (rn = 0; rn < 32; rn++)
					for (rm = 0; rm < 32; rm++)
						tcti_pmull_test_expect_execution(test, size,
									q, rd, rn, rm);
}

static void tcti_pmull_semantic_known_mathematical_vectors(struct kunit *test)
{
	static const struct {
		u8 size;
		u64 left;
		u64 right;
		u64 low;
		u64 high;
	} vectors[] = {
		{ 0, 0x5757575757575757ULL, 0x1313131313131313ULL,
		  0x0589058905890589ULL, 0x0589058905890589ULL },
		{ 3, 0x8000000000000001ULL, 3ULL,
		  0x8000000000000003ULL, 1ULL },
		{ 3, U64_MAX, U64_MAX,
		  0x5555555555555555ULL, 0x5555555555555555ULL },
	};
	u8 q;
	size_t index;

	for (index = 0; index < ARRAY_SIZE(vectors); index++) {
		u64 low;
		u64 high;

		tcti_pmull_test_expected(vectors[index].size, vectors[index].left,
					  vectors[index].right, &low, &high);
		KUNIT_EXPECT_EQ(test, vectors[index].low, low);
		KUNIT_EXPECT_EQ(test, vectors[index].high, high);

		for (q = 0; q < 2; q++) {
			u32 instruction = tcti_pmull_test_instruction(
				vectors[index].size, q, 17, 9, 3);
			struct tcti_decoded_instruction decoded =
				tcti_decode_aarch64(instruction);
			struct pt_regs regs = { .pc = 0x4000 };
			int ret;

			memset(current->thread.user_simd, 0,
			       sizeof(current->thread.user_simd));
			current->thread.user_simd[9 * 2 + q] = vectors[index].left;
			current->thread.user_simd[3 * 2 + q] = vectors[index].right;
			current->thread.user_simd_valid = 0;
			ret = tcti_switch_debug_execute_decoded(NULL, &regs, &decoded,
							 NULL);
			KUNIT_ASSERT_EQ(test, 0, ret);
			KUNIT_EXPECT_EQ(test, vectors[index].low,
					current->thread.user_simd[17 * 2]);
			KUNIT_EXPECT_EQ(test, vectors[index].high,
					current->thread.user_simd[17 * 2 + 1]);
			KUNIT_EXPECT_EQ(test, 0x4004ULL, regs.pc);
		}
	}
}

static void tcti_pmull_semantic_rejects_reserved_sizes(struct kunit *test)
{
	u8 size;
	u8 q;

	for (size = 1; size <= 2; size++)
		for (q = 0; q < 2; q++) {
			struct tcti_decoded_instruction decoded = tcti_decode_aarch64(
				tcti_pmull_test_instruction(size, q, 17, 9, 3));

			KUNIT_EXPECT_EQ_MSG(test, TCTI_DECODE_UNSUPPORTED,
				decoded.decode_class,
				"reserved PMULL size=%u q=%u", size, q);
		}
}

static struct kunit_case tcti_pmull_semantic_cases[] = {
	KUNIT_CASE(tcti_pmull_semantic_all_register_combinations),
	KUNIT_CASE(tcti_pmull_semantic_known_mathematical_vectors),
	KUNIT_CASE(tcti_pmull_semantic_rejects_reserved_sizes),
	{}
};

static struct kunit_suite tcti_pmull_semantic_test_suite = {
	.name = "orlix-tcti-pmull-semantic",
	.test_cases = tcti_pmull_semantic_cases,
};

kunit_test_suite(tcti_pmull_semantic_test_suite);
