// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/limits.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched/mm.h>
#include <linux/string.h>
#include <linux/syscalls.h>
#include <asm/ptrace.h>
#include <asm/orlix_tcti.h>

#include "../decode_aarch64.h"
#include "../switch_debug.h"

#define LSE128_RMW_BASE 0x19200000U

struct orlix_tcti_lse128_leaf {
	enum orlix_tcti_lse_atomic_op op;
	u8 order;
	u32 encoding;
};

struct orlix_tcti_lse128_value {
	u64 low;
	u64 high;
};

static const struct orlix_tcti_lse128_leaf orlix_tcti_lse128_leaves[] = {
	{ ORLIX_TCTI_LSE_ATOMIC_CLR, 0, 0x19201000U },
	{ ORLIX_TCTI_LSE_ATOMIC_SET, 0, 0x19203000U },
	{ ORLIX_TCTI_LSE_ATOMIC_SWP, 0, 0x19208000U },
	{ ORLIX_TCTI_LSE_ATOMIC_CLR, 2, 0x19601000U },
	{ ORLIX_TCTI_LSE_ATOMIC_SET, 2, 0x19603000U },
	{ ORLIX_TCTI_LSE_ATOMIC_SWP, 2, 0x19608000U },
	{ ORLIX_TCTI_LSE_ATOMIC_CLR, 1, 0x19a01000U },
	{ ORLIX_TCTI_LSE_ATOMIC_SET, 1, 0x19a03000U },
	{ ORLIX_TCTI_LSE_ATOMIC_SWP, 1, 0x19a08000U },
	{ ORLIX_TCTI_LSE_ATOMIC_CLR, 3, 0x19e01000U },
	{ ORLIX_TCTI_LSE_ATOMIC_SET, 3, 0x19e03000U },
	{ ORLIX_TCTI_LSE_ATOMIC_SWP, 3, 0x19e08000U },
};

static u32 orlix_tcti_lse128_instruction(enum orlix_tcti_lse_atomic_op op, u8 order,
				   u8 rs, u8 rn, u8 rt)
{
	u8 encoding;

	switch (op) {
	case ORLIX_TCTI_LSE_ATOMIC_CLR:
		encoding = 1;
		break;
	case ORLIX_TCTI_LSE_ATOMIC_SET:
		encoding = 3;
		break;
	case ORLIX_TCTI_LSE_ATOMIC_SWP:
		encoding = 8;
		break;
	default:
		return 0;
	}

	return LSE128_RMW_BASE | ((u32)encoding << 12) |
		(order & 1U ? BIT(23) : 0) |
		(order & 2U ? BIT(22) : 0) |
		((u32)rs << 16) | ((u32)rn << 5) | rt;
}

static unsigned long orlix_tcti_lse128_map(struct kunit *test)
{
	unsigned long mapped;

	mapped = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_EXPECT_FALSE(test, IS_ERR_VALUE(mapped));
	return mapped;
}

static struct orlix_tcti_lse128_value
orlix_tcti_lse128_result(enum orlix_tcti_lse_atomic_op op,
			   const struct orlix_tcti_lse128_value *initial,
			   const struct orlix_tcti_lse128_value *operand)
{
	struct orlix_tcti_lse128_value result;

	switch (op) {
	case ORLIX_TCTI_LSE_ATOMIC_CLR:
		result.low = initial->low & ~operand->low;
		result.high = initial->high & ~operand->high;
		break;
	case ORLIX_TCTI_LSE_ATOMIC_SET:
		result.low = initial->low | operand->low;
		result.high = initial->high | operand->high;
		break;
	case ORLIX_TCTI_LSE_ATOMIC_SWP:
		result = *operand;
		break;
	default:
		result = (struct orlix_tcti_lse128_value) {};
		break;
	}

	return result;
}

static void orlix_tcti_lse128_decode_all_leaves(struct kunit *test)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(orlix_tcti_lse128_leaves); i++) {
		const struct orlix_tcti_lse128_leaf *leaf = &orlix_tcti_lse128_leaves[i];
		u32 instruction = leaf->encoding | (6U << 16) | (7U << 5) | 8U;
		struct orlix_tcti_decoded_instruction decoded =
			orlix_tcti_decode_aarch64(instruction);

		KUNIT_ASSERT_EQ(test, ORLIX_TCTI_DECODE_LSE_ATOMIC,
				decoded.decode_class);
		KUNIT_EXPECT_EQ(test, leaf->op, decoded.lse_atomic_op);
		KUNIT_EXPECT_EQ(test, sizeof(u64), decoded.access_size);
		KUNIT_EXPECT_EQ(test, sizeof(u64), decoded.result_size);
		KUNIT_EXPECT_TRUE(test, decoded.pair);
		KUNIT_EXPECT_EQ(test, 7, decoded.rn);
		KUNIT_EXPECT_EQ(test, 8, decoded.rt);
		KUNIT_EXPECT_EQ(test, 6, decoded.rt2);
		KUNIT_EXPECT_TRUE(test, decoded.lse128);
		KUNIT_EXPECT_EQ(test, !!(leaf->order & 1U), decoded.acquire);
		KUNIT_EXPECT_EQ(test, !!(leaf->order & 2U), decoded.release);
	}
}

static void orlix_tcti_lse128_rejects_reserved_operations(
	struct kunit *test)
{
	const u32 invalid_ops[] = { 0, 2, 4, 5, 6, 7, 12, 13, 14, 15 };
	size_t i;

	for (i = 0; i < ARRAY_SIZE(invalid_ops); i++)
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
			orlix_tcti_decode_aarch64(LSE128_RMW_BASE |
				((u32)invalid_ops[i] << 12) | (6U << 16) |
				(7U << 5) | 8U).decode_class);
}

static void orlix_tcti_lse128_accepts_independent_odd_registers(
	struct kunit *test)
{
	struct orlix_tcti_decoded_instruction decoded;

	decoded = orlix_tcti_decode_aarch64(orlix_tcti_lse128_instruction(
		ORLIX_TCTI_LSE_ATOMIC_CLR, 0, 5, 8, 11));
	KUNIT_ASSERT_EQ(test, ORLIX_TCTI_DECODE_LSE_ATOMIC, decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 11, decoded.rt);
	KUNIT_EXPECT_EQ(test, 5, decoded.rt2);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
		orlix_tcti_decode_aarch64(orlix_tcti_lse128_instruction(
			ORLIX_TCTI_LSE_ATOMIC_CLR, 0, 5, 8, 31)).decode_class);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
		orlix_tcti_decode_aarch64(orlix_tcti_lse128_instruction(
			ORLIX_TCTI_LSE_ATOMIC_CLR, 0, 31, 8, 5)).decode_class);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
		orlix_tcti_decode_aarch64(orlix_tcti_lse128_instruction(
			ORLIX_TCTI_LSE_ATOMIC_CLR, 0, 5, 8, 5)).decode_class);
}

static void orlix_tcti_lse128_execute_all_leaves(struct kunit *test)
{
	const struct orlix_tcti_lse128_value initial = {
		.low = 0x0123456789abcdefULL,
		.high = 0xfedcba9876543210ULL,
	};
	const struct orlix_tcti_lse128_value operand = {
		.low = 0x0f0f55ffaa5500ffULL,
		.high = 0x33cc0ff0f00f5aa5ULL,
	};
	unsigned long mapped = orlix_tcti_lse128_map(test);
	size_t i;

	for (i = 0; i < ARRAY_SIZE(orlix_tcti_lse128_leaves); i++) {
		const struct orlix_tcti_lse128_leaf *leaf = &orlix_tcti_lse128_leaves[i];
		struct orlix_tcti_decoded_instruction decoded = orlix_tcti_decode_aarch64(
			orlix_tcti_lse128_instruction(leaf->op, leaf->order, 6, 10, 8));
		struct orlix_tcti_lse128_value expected =
			orlix_tcti_lse128_result(leaf->op, &initial, &operand);
		struct orlix_tcti_lse128_value observed = {};
		struct pt_regs regs = {};
		int ret;

		regs.regs[8] = operand.low;
		regs.regs[6] = operand.high;
		regs.regs[10] = mapped;
		regs.pc = 0x8000;
		regs.pstate = 0xa0000000ULL | leaf->order;
		ret = orlix_tcti_write_user_data(current->mm, mapped, &initial,
					   sizeof(initial));
		KUNIT_ASSERT_EQ(test, 0, ret);
		ret = orlix_tcti_switch_debug_execute_decoded(current->mm, &regs,
							       &decoded, NULL);
		KUNIT_ASSERT_EQ(test, 0, ret);
		ret = orlix_tcti_read_user_data(current->mm, mapped, &observed,
					  sizeof(observed));
		KUNIT_ASSERT_EQ(test, 0, ret);
		KUNIT_EXPECT_MEMEQ(test, &expected, &observed, sizeof(expected));
		KUNIT_EXPECT_EQ(test, initial.low, regs.regs[8]);
		KUNIT_EXPECT_EQ(test, initial.high, regs.regs[6]);
		KUNIT_EXPECT_EQ(test, 0x8004ULL, regs.pc);
		KUNIT_EXPECT_EQ(test, 0xa0000000ULL | leaf->order, regs.pstate);
	}

	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void orlix_tcti_lse128_rejects_overlapping_result_lanes(struct kunit *test)
{
	struct orlix_tcti_decoded_instruction decoded = orlix_tcti_decode_aarch64(
		orlix_tcti_lse128_instruction(ORLIX_TCTI_LSE_ATOMIC_SET, 3, 6, 10, 6));

	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED, decoded.decode_class);
}

static void orlix_tcti_lse128_fixed_bit_near_misses_are_not_decoded(struct kunit *test)
{
	u32 instruction = orlix_tcti_lse128_instruction(ORLIX_TCTI_LSE_ATOMIC_SWP,
						   0, 6, 10, 8);
	struct orlix_tcti_decoded_instruction unpriv;

	KUNIT_EXPECT_NE(test, ORLIX_TCTI_DECODE_LSE_ATOMIC,
		orlix_tcti_decode_aarch64(instruction ^ BIT(21)).decode_class);
	unpriv = orlix_tcti_decode_aarch64(instruction ^ BIT(10));
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_LSE_ATOMIC, unpriv.decode_class);
	KUNIT_EXPECT_TRUE(test, unpriv.unprivileged);
	KUNIT_EXPECT_FALSE(test, unpriv.lse128);
}

static void orlix_tcti_lse128_unaligned_fault_preserves_state(struct kunit *test)
{
	const struct orlix_tcti_lse128_value initial = {
		.low = 0x1111222233334444ULL,
		.high = 0xaaaabbbbccccddddULL,
	};
	unsigned long mapped = orlix_tcti_lse128_map(test);
	struct orlix_tcti_decoded_instruction decoded = orlix_tcti_decode_aarch64(
		orlix_tcti_lse128_instruction(ORLIX_TCTI_LSE_ATOMIC_SWP, 0, 6, 10, 8));
	struct orlix_tcti_lse128_value observed = {};
	struct pt_regs regs = {};
	struct pt_regs before;
	unsigned long fault_address = 0;
	int ret;

	regs.regs[6] = U64_MAX;
	regs.regs[7] = U64_MAX;
	regs.regs[10] = mapped + sizeof(u64);
	regs.pc = 0x9000;
	before = regs;
	ret = orlix_tcti_write_user_data(current->mm, mapped, &initial, sizeof(initial));
	KUNIT_ASSERT_EQ(test, 0, ret);
	ret = orlix_tcti_switch_debug_execute_decoded(current->mm, &regs, &decoded,
						       &fault_address);
	KUNIT_EXPECT_EQ(test, -EFAULT, ret);
	KUNIT_EXPECT_EQ(test, mapped + sizeof(u64), fault_address);
	KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
	ret = orlix_tcti_read_user_data(current->mm, mapped, &observed, sizeof(observed));
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_MEMEQ(test, &initial, &observed, sizeof(initial));
	regs.regs[10] = mapped;
	before = regs;
	KUNIT_ASSERT_EQ(test, 0, sys_mprotect(mapped, PAGE_SIZE, PROT_READ));
	ret = orlix_tcti_switch_debug_execute_decoded(current->mm, &regs, &decoded,
						       &fault_address);
	KUNIT_EXPECT_EQ(test, -EACCES, ret);
	KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
	ret = orlix_tcti_read_user_data(current->mm, mapped, &observed, sizeof(observed));
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_MEMEQ(test, &initial, &observed, sizeof(initial));
	KUNIT_ASSERT_EQ(test, 0, sys_mprotect(mapped, PAGE_SIZE,
						      PROT_READ | PROT_WRITE));

	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static struct kunit_case orlix_tcti_lse128_noncas_cases[] = {
	KUNIT_CASE(orlix_tcti_lse128_decode_all_leaves),
	KUNIT_CASE(orlix_tcti_lse128_rejects_reserved_operations),
	KUNIT_CASE(orlix_tcti_lse128_accepts_independent_odd_registers),
	KUNIT_CASE(orlix_tcti_lse128_execute_all_leaves),
	KUNIT_CASE(orlix_tcti_lse128_rejects_overlapping_result_lanes),
	KUNIT_CASE(orlix_tcti_lse128_fixed_bit_near_misses_are_not_decoded),
	KUNIT_CASE(orlix_tcti_lse128_unaligned_fault_preserves_state),
	{}
};

static int orlix_tcti_lse128_noncas_test_init(struct kunit *test)
{
	struct mm_struct *mm = mm_alloc();

	if (!mm)
		return -ENOMEM;
	kthread_use_mm(mm);
	test->priv = mm;
	return 0;
}

static void orlix_tcti_lse128_noncas_test_exit(struct kunit *test)
{
	struct mm_struct *mm = test->priv;

	if (!mm)
		return;
	kthread_unuse_mm(mm);
	mmput(mm);
}

struct kunit_suite orlix_tcti_lse128_noncas_test_suite = {
	.name = "orlix_tcti_lse128_noncas",
	.init = orlix_tcti_lse128_noncas_test_init,
	.exit = orlix_tcti_lse128_noncas_test_exit,
	.test_cases = orlix_tcti_lse128_noncas_cases,
};

kunit_test_suite(orlix_tcti_lse128_noncas_test_suite);

MODULE_LICENSE("GPL");
