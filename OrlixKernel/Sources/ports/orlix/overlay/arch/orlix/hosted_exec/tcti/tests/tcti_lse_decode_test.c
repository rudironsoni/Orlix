// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched/mm.h>
#include <linux/string.h>
#include <linux/syscalls.h>
#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/tcti.h>

#include "../decode_aarch64.h"
#include "../switch_debug.h"
#include "tcti_test_suites.h"

#define LSE_CAS_BASE 0x08a07c00U
#define LSE_CASP_BASE 0x08207c00U
#define LSE_RMW_BASE 0x38200000U

struct tcti_lse_rmw_case {
	enum tcti_lse_atomic_op op;
	u8 encoding;
	u64 initial;
	u64 operand;
};

struct tcti_lse_exclusive_state {
	unsigned long address;
	unsigned long value;
	unsigned long value2;
	u8 size;
	u8 valid;
};

static const struct tcti_lse_rmw_case tcti_lse_rmw_cases[] = {
	{ TCTI_LSE_ATOMIC_ADD, 0, 0x12, 0x25 },
	{ TCTI_LSE_ATOMIC_CLR, 1, 0x3f, 0x15 },
	{ TCTI_LSE_ATOMIC_EOR, 2, 0x3c, 0x25 },
	{ TCTI_LSE_ATOMIC_SET, 3, 0x12, 0x25 },
	{ TCTI_LSE_ATOMIC_SMAX, 4, U64_MAX, 1 },
	{ TCTI_LSE_ATOMIC_SMIN, 5, 0x7fffffffffffffffULL, U64_MAX },
	{ TCTI_LSE_ATOMIC_UMAX, 6, 1, U64_MAX },
	{ TCTI_LSE_ATOMIC_UMIN, 7, U64_MAX, 1 },
	{ TCTI_LSE_ATOMIC_SWP, 8, 0x12, 0x25 },
};

static u32 tcti_lse_order(u32 instruction, bool cas, u8 order)
{
	if (cas) {
		if (order & 1U)
			instruction |= BIT(22);
		if (order & 2U)
			instruction |= BIT(15);
	} else {
		if (order & 1U)
			instruction |= BIT(23);
		if (order & 2U)
			instruction |= BIT(22);
	}
	return instruction;
}

static unsigned long tcti_lse_map(struct kunit *test)
{
	unsigned long mapped;

	mapped = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_EXPECT_FALSE(test, IS_ERR_VALUE(mapped));
	return mapped;
}

static void tcti_lse_set_exclusive(
	const struct tcti_lse_exclusive_state *state)
{
	current->thread.user_exclusive_address = state->address;
	current->thread.user_exclusive_value = state->value;
	current->thread.user_exclusive_value2 = state->value2;
	current->thread.user_exclusive_size = state->size;
	current->thread.user_exclusive_valid = state->valid;
}

static void tcti_lse_expect_exclusive(
	struct kunit *test, const struct tcti_lse_exclusive_state *expected)
{
	KUNIT_EXPECT_EQ(test, expected->address,
			current->thread.user_exclusive_address);
	KUNIT_EXPECT_EQ(test, expected->value,
			current->thread.user_exclusive_value);
	KUNIT_EXPECT_EQ(test, expected->value2,
			current->thread.user_exclusive_value2);
	KUNIT_EXPECT_EQ(test, expected->size,
			current->thread.user_exclusive_size);
	KUNIT_EXPECT_EQ(test, expected->valid,
			current->thread.user_exclusive_valid);
}

static u64 tcti_lse_mask(u8 width)
{
	return width == sizeof(u64) ? U64_MAX :
		(1ULL << (width * 8)) - 1;
}

static u64 tcti_lse_rmw_result(const struct tcti_lse_rmw_case *rmw,
			       u64 initial, u64 operand, u8 width)
{
	u64 mask = tcti_lse_mask(width);
	s64 signed_initial = sign_extend64(initial, width * 8 - 1);
	s64 signed_operand = sign_extend64(operand, width * 8 - 1);

	switch (rmw->op) {
	case TCTI_LSE_ATOMIC_SWP:
		return operand & mask;
	case TCTI_LSE_ATOMIC_ADD:
		return (initial + operand) & mask;
	case TCTI_LSE_ATOMIC_CLR:
		return initial & ~operand & mask;
	case TCTI_LSE_ATOMIC_EOR:
		return (initial ^ operand) & mask;
	case TCTI_LSE_ATOMIC_SET:
		return (initial | operand) & mask;
	case TCTI_LSE_ATOMIC_SMAX:
		return (signed_initial > signed_operand ?
			initial : operand) & mask;
	case TCTI_LSE_ATOMIC_SMIN:
		return (signed_initial < signed_operand ?
			initial : operand) & mask;
	case TCTI_LSE_ATOMIC_UMAX:
		return (initial > operand ? initial : operand) & mask;
	case TCTI_LSE_ATOMIC_UMIN:
		return (initial < operand ? initial : operand) & mask;
	case TCTI_LSE_ATOMIC_CAS:
		break;
	}

	return 0;
}

static void tcti_lse_write_pair(struct kunit *test, unsigned long address,
				const u64 values[2], u8 width)
{
	u8 encoded[2 * sizeof(u64)] = {};
	int ret;

	memcpy(encoded, &values[0], width);
	memcpy(encoded + width, &values[1], width);
	ret = tcti_write_user_data(current->mm, address, encoded, 2 * width);
	KUNIT_ASSERT_EQ(test, 0, ret);
}

static void tcti_lse_expect_pair(struct kunit *test, unsigned long address,
				 const u64 expected[2], u8 width)
{
	u8 encoded[2 * sizeof(u64)] = {};
	u64 observed[2] = {};
	int ret;

	ret = tcti_read_user_data(current->mm, address, encoded, 2 * width);
	KUNIT_ASSERT_EQ(test, 0, ret);
	memcpy(&observed[0], encoded, width);
	memcpy(&observed[1], encoded + width, width);
	KUNIT_EXPECT_EQ(test, expected[0] & tcti_lse_mask(width),
			observed[0]);
	KUNIT_EXPECT_EQ(test, expected[1] & tcti_lse_mask(width),
			observed[1]);
}

static void tcti_expect_lse(struct kunit *test, u32 instruction,
			    enum tcti_lse_atomic_op op, u8 width, bool acquire,
			    bool release, bool pair)
{
	struct tcti_decoded_instruction decoded = tcti_decode_aarch64(instruction);

	KUNIT_ASSERT_EQ(test, TCTI_DECODE_LSE_ATOMIC, decoded.decode_class);
	KUNIT_EXPECT_EQ(test, op, decoded.lse_atomic_op);
	KUNIT_EXPECT_EQ(test, width, decoded.access_size);
	KUNIT_EXPECT_EQ(test, width, decoded.result_size);
	KUNIT_EXPECT_EQ(test, acquire, decoded.acquire);
	KUNIT_EXPECT_EQ(test, release, decoded.release);
	KUNIT_EXPECT_EQ(test, pair, decoded.pair);
	KUNIT_EXPECT_EQ(test, 6, decoded.rs);
	KUNIT_EXPECT_EQ(test, 7, decoded.rn);
	KUNIT_EXPECT_EQ(test, 8, decoded.rt);
}

static void tcti_lse_decode_cas_leaves(struct kunit *test)
{
	u8 size;
	u8 order;

	for (size = 0; size < 4; size++) {
		for (order = 0; order < 4; order++) {
			u32 instruction = LSE_CAS_BASE | ((u32)size << 30) |
				(6U << 16) | (7U << 5) | 8U;

			instruction = tcti_lse_order(instruction, true, order);
			tcti_expect_lse(test, instruction, TCTI_LSE_ATOMIC_CAS,
					1U << size, order & 1U, order & 2U, false);
		}
	}
}

static void tcti_lse_decode_casp_leaves(struct kunit *test)
{
	u8 size;
	u8 order;

	for (size = 0; size < 2; size++) {
		for (order = 0; order < 4; order++) {
			u32 instruction = LSE_CASP_BASE | ((u32)size << 30) |
				(6U << 16) | (7U << 5) | 8U;
			struct tcti_decoded_instruction decoded;

			instruction = tcti_lse_order(instruction, true, order);
			tcti_expect_lse(test, instruction, TCTI_LSE_ATOMIC_CAS,
					4U << size, order & 1U, order & 2U, true);
			decoded = tcti_decode_aarch64(instruction);
			KUNIT_EXPECT_EQ(test, 9, decoded.rt2);
		}
	}
}

static void tcti_lse_decode_rmw_and_swp_leaves(struct kunit *test)
{
	static const enum tcti_lse_atomic_op ops[] = {
		TCTI_LSE_ATOMIC_ADD,
		TCTI_LSE_ATOMIC_CLR,
		TCTI_LSE_ATOMIC_EOR,
		TCTI_LSE_ATOMIC_SET,
		TCTI_LSE_ATOMIC_SMAX,
		TCTI_LSE_ATOMIC_SMIN,
		TCTI_LSE_ATOMIC_UMAX,
		TCTI_LSE_ATOMIC_UMIN,
		TCTI_LSE_ATOMIC_SWP,
	};
	u8 op;
	u8 size;
	u8 order;

	for (op = 0; op < ARRAY_SIZE(ops); op++) {
		for (size = 0; size < 4; size++) {
			for (order = 0; order < 4; order++) {
				u32 instruction = LSE_RMW_BASE | ((u32)size << 30) |
					((u32)op << 12) | (6U << 16) | (7U << 5) | 8U;

				instruction = tcti_lse_order(instruction, false, order);
				tcti_expect_lse(test, instruction, ops[op], 1U << size,
						order & 1U, order & 2U, false);
			}
		}
	}
}

static void tcti_lse_execute_cas_scalar_matrix(struct kunit *test)
{
	unsigned long mapped = tcti_lse_map(test);
	u8 size;
	u8 order;

	for (size = 0; size < 4; size++) {
		u8 width = 1U << size;
		u64 mask = tcti_lse_mask(width);

		for (order = 0; order < 4; order++) {
			u64 initial = 0x1122334455667788ULL & mask;
			u64 replacement = 0x8877665544332211ULL & mask;
			u32 instruction = LSE_CAS_BASE | ((u32)size << 30) |
				(6U << 16) | (7U << 5) | 8U;
			struct tcti_decoded_instruction decoded;
			struct pt_regs regs = {};
			u64 observed = 0;
			unsigned long fault_address = 0;
			int ret;

			instruction = tcti_lse_order(instruction, true, order);
			decoded = tcti_decode_aarch64(instruction);
			regs.regs[6] = initial;
			regs.regs[7] = mapped;
			regs.regs[8] = replacement;
			regs.pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_Z_BIT |
				PSR_C_BIT | PSR_V_BIT;
			regs.pc = 0x1000;
			ret = tcti_write_user_data(current->mm, mapped, &initial,
						   width);
			KUNIT_ASSERT_EQ(test, 0, ret);
			ret = tcti_switch_debug_execute_decoded(
				current->mm, &regs, &decoded, &fault_address);
			KUNIT_ASSERT_EQ(test, 0, ret);
			ret = tcti_read_user_data(current->mm, mapped, &observed,
						  width);
			KUNIT_ASSERT_EQ(test, 0, ret);
			KUNIT_EXPECT_EQ(test, replacement, observed);
			KUNIT_EXPECT_EQ(test, initial, regs.regs[6]);
			KUNIT_EXPECT_EQ(test,
					PSR_MODE_EL0t | PSR_N_BIT | PSR_Z_BIT |
						PSR_C_BIT | PSR_V_BIT,
					regs.pstate);
			KUNIT_EXPECT_EQ(test, mapped, fault_address);
			KUNIT_EXPECT_EQ(test, 0x1004ULL, regs.pc);

			observed = 0;
			regs.regs[6] = (initial ^ 1) & mask;
			regs.regs[8] = replacement;
			regs.pc = 0x2000;
			ret = tcti_write_user_data(current->mm, mapped, &initial,
						   width);
			KUNIT_ASSERT_EQ(test, 0, ret);
			ret = tcti_switch_debug_execute_decoded(
				current->mm, &regs, &decoded, &fault_address);
			KUNIT_ASSERT_EQ(test, 0, ret);
			ret = tcti_read_user_data(current->mm, mapped, &observed,
						  width);
			KUNIT_ASSERT_EQ(test, 0, ret);
			KUNIT_EXPECT_EQ(test, initial, observed);
			KUNIT_EXPECT_EQ(test, initial, regs.regs[6]);
			KUNIT_EXPECT_EQ(test, replacement, regs.regs[8]);
			KUNIT_EXPECT_EQ(test,
					PSR_MODE_EL0t | PSR_N_BIT | PSR_Z_BIT |
						PSR_C_BIT | PSR_V_BIT,
					regs.pstate);
			KUNIT_EXPECT_EQ(test, mapped, fault_address);
			KUNIT_EXPECT_EQ(test, 0x2004ULL, regs.pc);
		}
	}

	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void tcti_lse_execute_casp_matrix(struct kunit *test)
{
	unsigned long mapped = tcti_lse_map(test);
	u8 size;
	u8 order;

	for (size = 0; size < 2; size++) {
		u8 width = 4U << size;
		u64 mask = tcti_lse_mask(width);

		for (order = 0; order < 4; order++) {
			u64 initial[2] = {
				0x1122334455667788ULL & mask,
				0x8877665544332211ULL & mask,
			};
			u64 replacement[2] = {
				0x0123456789abcdefULL & mask,
				0xfedcba9876543210ULL & mask,
			};
			u32 instruction = LSE_CASP_BASE | ((u32)size << 30) |
				(6U << 16) | (10U << 5) | 8U;
			struct tcti_decoded_instruction decoded;
			struct pt_regs regs = {};
			unsigned long fault_address = 0;
			int ret;

			instruction = tcti_lse_order(instruction, true, order);
			decoded = tcti_decode_aarch64(instruction);
			regs.regs[6] = initial[0];
			regs.regs[7] = initial[1];
			regs.regs[8] = replacement[0];
			regs.regs[9] = replacement[1];
			regs.regs[10] = mapped;
			regs.pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_Z_BIT |
				PSR_C_BIT | PSR_V_BIT;
			regs.pc = 0x3000;
			tcti_lse_write_pair(test, mapped, initial, width);
			ret = tcti_switch_debug_execute_decoded(
				current->mm, &regs, &decoded, &fault_address);
			KUNIT_ASSERT_EQ(test, 0, ret);
			tcti_lse_expect_pair(test, mapped, replacement, width);
			KUNIT_EXPECT_EQ(test, initial[0], regs.regs[6]);
			KUNIT_EXPECT_EQ(test, initial[1], regs.regs[7]);
			KUNIT_EXPECT_EQ(test,
					PSR_MODE_EL0t | PSR_N_BIT | PSR_Z_BIT |
						PSR_C_BIT | PSR_V_BIT,
					regs.pstate);
			KUNIT_EXPECT_EQ(test, mapped, fault_address);
			KUNIT_EXPECT_EQ(test, 0x3004ULL, regs.pc);

			regs.regs[6] = initial[0];
			regs.regs[7] = (initial[1] ^ 1) & mask;
			regs.regs[8] = replacement[0];
			regs.regs[9] = replacement[1];
			regs.pc = 0x4000;
			tcti_lse_write_pair(test, mapped, initial, width);
			ret = tcti_switch_debug_execute_decoded(
				current->mm, &regs, &decoded, &fault_address);
			KUNIT_ASSERT_EQ(test, 0, ret);
			tcti_lse_expect_pair(test, mapped, initial, width);
			KUNIT_EXPECT_EQ(test, initial[0], regs.regs[6]);
			KUNIT_EXPECT_EQ(test, initial[1], regs.regs[7]);
			KUNIT_EXPECT_EQ(test, replacement[0], regs.regs[8]);
			KUNIT_EXPECT_EQ(test, replacement[1], regs.regs[9]);
			KUNIT_EXPECT_EQ(test,
					PSR_MODE_EL0t | PSR_N_BIT | PSR_Z_BIT |
						PSR_C_BIT | PSR_V_BIT,
					regs.pstate);
			KUNIT_EXPECT_EQ(test, mapped, fault_address);
			KUNIT_EXPECT_EQ(test, 0x4004ULL, regs.pc);
		}
	}

	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void tcti_lse_execute_rmw_matrix(struct kunit *test)
{
	unsigned long mapped = tcti_lse_map(test);
	size_t op;
	u8 size;
	u8 order;

	for (op = 0; op < ARRAY_SIZE(tcti_lse_rmw_cases); op++) {
		const struct tcti_lse_rmw_case *rmw =
			&tcti_lse_rmw_cases[op];

		for (size = 0; size < 4; size++) {
			u8 width = 1U << size;
			u64 mask = tcti_lse_mask(width);
			u64 sign = 1ULL << (width * 8 - 1);
			u64 initial = rmw->initial & mask;
			u64 operand = rmw->operand & mask;
			u64 expected;

			if (rmw->op == TCTI_LSE_ATOMIC_SMAX) {
				initial = sign;
				operand = 1;
			} else if (rmw->op == TCTI_LSE_ATOMIC_SMIN) {
				initial = sign - 1;
				operand = mask;
			} else if (rmw->op == TCTI_LSE_ATOMIC_UMAX) {
				initial = 0;
				operand = mask;
			} else if (rmw->op == TCTI_LSE_ATOMIC_UMIN) {
				initial = mask;
				operand = 0;
			}
			expected = tcti_lse_rmw_result(
				rmw, initial, operand, width);

			for (order = 0; order < 4; order++) {
				u32 instruction = LSE_RMW_BASE |
					((u32)size << 30) |
					((u32)rmw->encoding << 12) |
					(6U << 16) | (7U << 5) | 8U;
				struct tcti_decoded_instruction decoded;
				struct pt_regs regs = {};
				u64 observed = 0;
				unsigned long fault_address = 0;
				int ret;

				instruction = tcti_lse_order(
					instruction, false, order);
				decoded = tcti_decode_aarch64(instruction);
				regs.regs[6] = operand;
				regs.regs[7] = mapped;
				regs.regs[8] = U64_MAX;
				regs.pstate = PSR_MODE_EL0t | PSR_N_BIT |
					PSR_Z_BIT | PSR_C_BIT | PSR_V_BIT;
				regs.pc = 0x5000;
				ret = tcti_write_user_data(
					current->mm, mapped, &initial, width);
				KUNIT_ASSERT_EQ(test, 0, ret);
				ret = tcti_switch_debug_execute_decoded(
					current->mm, &regs, &decoded,
					&fault_address);
				KUNIT_ASSERT_EQ(test, 0, ret);
				ret = tcti_read_user_data(
					current->mm, mapped, &observed, width);
				KUNIT_ASSERT_EQ(test, 0, ret);
				KUNIT_EXPECT_EQ(test, expected, observed);
				KUNIT_EXPECT_EQ(test, initial, regs.regs[8]);
				KUNIT_EXPECT_EQ(test, operand, regs.regs[6]);
				KUNIT_EXPECT_EQ(test,
						PSR_MODE_EL0t | PSR_N_BIT |
							PSR_Z_BIT | PSR_C_BIT |
							PSR_V_BIT,
						regs.pstate);
				KUNIT_EXPECT_EQ(test, mapped, fault_address);
				KUNIT_EXPECT_EQ(test, 0x5004ULL, regs.pc);
			}
		}
	}

	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void tcti_lse_execute_alias_zero_and_sp(struct kunit *test)
{
	unsigned long mapped = tcti_lse_map(test);
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	u64 initial;
	u64 observed;
	u32 instruction;
	int ret;

	instruction = LSE_RMW_BASE | (3U << 30) | (6U << 16) |
		(31U << 5) | 6U;
	decoded = tcti_decode_aarch64(instruction);
	initial = 3;
	regs.regs[6] = 5;
	regs.sp = mapped;
	regs.pc = 0x6000;
	ret = tcti_write_user_data(current->mm, mapped, &initial,
				   sizeof(initial));
	KUNIT_ASSERT_EQ(test, 0, ret);
	ret = tcti_switch_debug_execute_decoded(
		current->mm, &regs, &decoded, NULL);
	KUNIT_ASSERT_EQ(test, 0, ret);
	ret = tcti_read_user_data(current->mm, mapped, &observed,
				  sizeof(observed));
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 8ULL, observed);
	KUNIT_EXPECT_EQ(test, 3ULL, regs.regs[6]);
	KUNIT_EXPECT_EQ(test, 0x6004ULL, regs.pc);

	instruction = LSE_RMW_BASE | (3U << 30) | (31U << 16) |
		(31U << 5) | 8U;
	decoded = tcti_decode_aarch64(instruction);
	initial = 9;
	observed = 0;
	regs.regs[8] = U64_MAX;
	regs.pc = 0x7000;
	ret = tcti_write_user_data(current->mm, mapped, &initial,
				   sizeof(initial));
	KUNIT_ASSERT_EQ(test, 0, ret);
	ret = tcti_switch_debug_execute_decoded(
		current->mm, &regs, &decoded, NULL);
	KUNIT_ASSERT_EQ(test, 0, ret);
	ret = tcti_read_user_data(current->mm, mapped, &observed,
				  sizeof(observed));
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, initial, observed);
	KUNIT_EXPECT_EQ(test, initial, regs.regs[8]);
	KUNIT_EXPECT_EQ(test, 0x7004ULL, regs.pc);

	instruction = LSE_RMW_BASE | (3U << 30) | (6U << 16) |
		(8U << 12) | (31U << 5) | 31U;
	decoded = tcti_decode_aarch64(instruction);
	initial = 11;
	regs.regs[6] = 13;
	regs.pc = 0x8000;
	ret = tcti_write_user_data(current->mm, mapped, &initial,
				   sizeof(initial));
	KUNIT_ASSERT_EQ(test, 0, ret);
	ret = tcti_switch_debug_execute_decoded(
		current->mm, &regs, &decoded, NULL);
	KUNIT_ASSERT_EQ(test, 0, ret);
	ret = tcti_read_user_data(current->mm, mapped, &observed,
				  sizeof(observed));
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 13ULL, observed);
	KUNIT_EXPECT_EQ(test, 13ULL, regs.regs[6]);
	KUNIT_EXPECT_EQ(test, 0x8004ULL, regs.pc);

	instruction = LSE_CAS_BASE | (3U << 30) | (6U << 16) |
		(31U << 5) | 6U;
	decoded = tcti_decode_aarch64(instruction);
	initial = 17;
	regs.regs[6] = 19;
	regs.pc = 0x9000;
	ret = tcti_write_user_data(current->mm, mapped, &initial,
				   sizeof(initial));
	KUNIT_ASSERT_EQ(test, 0, ret);
	ret = tcti_switch_debug_execute_decoded(
		current->mm, &regs, &decoded, NULL);
	KUNIT_ASSERT_EQ(test, 0, ret);
	ret = tcti_read_user_data(current->mm, mapped, &observed,
				  sizeof(observed));
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, initial, observed);
	KUNIT_EXPECT_EQ(test, initial, regs.regs[6]);
	KUNIT_EXPECT_EQ(test, 0x9004ULL, regs.pc);

	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void tcti_lse_execute_fault_preserves_state(struct kunit *test)
{
	unsigned long mapped = tcti_lse_map(test);
	u32 instruction = LSE_RMW_BASE | (3U << 30) | (6U << 16) |
		(7U << 5) | 8U;
	struct tcti_decoded_instruction decoded =
		tcti_decode_aarch64(instruction);
	struct pt_regs regs = {};
	struct pt_regs before;
	unsigned long fault_address = 0;
	u64 initial = 0x1122334455667788ULL;
	u64 observed = 0;
	int ret;

	regs.regs[6] = 5;
	regs.regs[7] = mapped + 1;
	regs.regs[8] = 7;
	regs.pc = 0xa000;
	before = regs;
	ret = tcti_write_user_data(current->mm, mapped, &initial,
				   sizeof(initial));
	KUNIT_ASSERT_EQ(test, 0, ret);
	ret = tcti_switch_debug_execute_decoded(
		current->mm, &regs, &decoded, &fault_address);
	KUNIT_EXPECT_EQ(test, -EFAULT, ret);
	KUNIT_EXPECT_EQ(test, mapped + 1, fault_address);
	KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
	ret = tcti_read_user_data(current->mm, mapped, &observed,
				  sizeof(observed));
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, initial, observed);

	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void tcti_lse_execute_updates_exclusive_state(struct kunit *test)
{
	static const struct tcti_lse_exclusive_state active = {
		.address = 0x12345000,
		.value = 0x1122334455667788,
		.value2 = 0x8877665544332211,
		.size = sizeof(u64),
		.valid = 1,
	};
	static const struct tcti_lse_exclusive_state cleared;
	unsigned long mapped = tcti_lse_map(test);
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	u64 initial;
	u64 observed;
	u64 pair_initial[2];
	u64 pair_replacement[2];
	u32 instruction;
	int ret;

	instruction = LSE_RMW_BASE | (3U << 30) | (6U << 16) |
		(10U << 5) | 8U;
	decoded = tcti_decode_aarch64(instruction);
	initial = 0x35;
	observed = 0;
	regs.regs[6] = 0;
	regs.regs[10] = mapped;
	regs.pc = 0xb000;
	ret = tcti_write_user_data(current->mm, mapped, &initial,
				   sizeof(initial));
	KUNIT_ASSERT_EQ(test, 0, ret);
	tcti_lse_set_exclusive(&active);
	ret = tcti_switch_debug_execute_decoded(
		current->mm, &regs, &decoded, NULL);
	KUNIT_ASSERT_EQ(test, 0, ret);
	ret = tcti_read_user_data(current->mm, mapped, &observed,
				  sizeof(observed));
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, initial, observed);
	tcti_lse_expect_exclusive(test, &cleared);

	instruction = LSE_CAS_BASE | (3U << 30) | (6U << 16) |
		(10U << 5) | 8U;
	decoded = tcti_decode_aarch64(instruction);
	initial = 0x45;
	regs.regs[6] = initial;
	regs.regs[8] = 0x55;
	regs.pc = 0xc000;
	ret = tcti_write_user_data(current->mm, mapped, &initial,
				   sizeof(initial));
	KUNIT_ASSERT_EQ(test, 0, ret);
	tcti_lse_set_exclusive(&active);
	ret = tcti_switch_debug_execute_decoded(
		current->mm, &regs, &decoded, NULL);
	KUNIT_ASSERT_EQ(test, 0, ret);
	tcti_lse_expect_exclusive(test, &cleared);

	regs.regs[6] = initial + 1;
	regs.regs[8] = 0x65;
	regs.pc = 0xd000;
	ret = tcti_write_user_data(current->mm, mapped, &initial,
				   sizeof(initial));
	KUNIT_ASSERT_EQ(test, 0, ret);
	tcti_lse_set_exclusive(&active);
	ret = tcti_switch_debug_execute_decoded(
		current->mm, &regs, &decoded, NULL);
	KUNIT_ASSERT_EQ(test, 0, ret);
	tcti_lse_expect_exclusive(test, &active);

	instruction = LSE_CASP_BASE | (1U << 30) | (6U << 16) |
		(10U << 5) | 8U;
	decoded = tcti_decode_aarch64(instruction);
	pair_initial[0] = 0x0123456789abcdefULL;
	pair_initial[1] = 0xfedcba9876543210ULL;
	pair_replacement[0] = 0x1122334455667788ULL;
	pair_replacement[1] = 0x8877665544332211ULL;
	regs.regs[6] = pair_initial[0];
	regs.regs[7] = pair_initial[1];
	regs.regs[8] = pair_replacement[0];
	regs.regs[9] = pair_replacement[1];
	regs.pc = 0xe000;
	tcti_lse_write_pair(test, mapped, pair_initial, sizeof(u64));
	tcti_lse_set_exclusive(&active);
	ret = tcti_switch_debug_execute_decoded(
		current->mm, &regs, &decoded, NULL);
	KUNIT_ASSERT_EQ(test, 0, ret);
	tcti_lse_expect_exclusive(test, &cleared);

	regs.regs[6] = pair_initial[0];
	regs.regs[7] = pair_initial[1] ^ 1;
	regs.regs[8] = pair_replacement[0];
	regs.regs[9] = pair_replacement[1];
	regs.pc = 0xf000;
	tcti_lse_write_pair(test, mapped, pair_initial, sizeof(u64));
	tcti_lse_set_exclusive(&active);
	ret = tcti_switch_debug_execute_decoded(
		current->mm, &regs, &decoded, NULL);
	KUNIT_ASSERT_EQ(test, 0, ret);
	tcti_lse_expect_exclusive(test, &active);

	tcti_lse_set_exclusive(&cleared);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void tcti_lse_same_value_cas_invalidates_reservations(
	struct kunit *test)
{
	static const struct tcti_lse_exclusive_state active = {
		.address = 0x34567000,
		.value = 0x1122334455667788,
		.value2 = 0x8877665544332211,
		.size = sizeof(u64),
		.valid = 1,
	};
	static const struct tcti_lse_exclusive_state cleared;
	unsigned long mapped = tcti_lse_map(test);
	struct tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	unsigned long pfn;
	u64 generation;
	u64 mapping_generation;
	u64 scalar_initial = 0x0123456789abcdefULL;
	u64 scalar_observed = 0;
	u64 scalar_replacement = 0xfedcba9876543210ULL;
	u64 pair_initial[2] = {
		0x1122334455667788ULL,
		0x8877665544332211ULL,
	};
	u64 pair_observed[2] = {};
	u64 pair_replacement[2] = {
		0xa5a5a5a5a5a5a5a5ULL,
		0x5a5a5a5a5a5a5a5aULL,
	};
	u32 instruction;
	bool stored;
	int ret;

	ret = tcti_write_user_data(current->mm, mapped, &scalar_initial,
				   sizeof(scalar_initial));
	KUNIT_ASSERT_EQ(test, 0, ret);
	ret = tcti_load_exclusive_user_data(
		current->mm, mapped, &scalar_observed, sizeof(scalar_observed),
		&pfn, &generation, &mapping_generation);
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_ASSERT_EQ(test, scalar_initial, scalar_observed);

	instruction = LSE_CAS_BASE | (3U << 30) | (6U << 16) |
		(10U << 5) | 8U;
	decoded = tcti_decode_aarch64(instruction);
	regs.regs[6] = scalar_initial;
	regs.regs[8] = scalar_initial;
	regs.regs[10] = mapped;
	regs.pc = 0x10000;
	tcti_lse_set_exclusive(&active);
	ret = tcti_switch_debug_execute_decoded(
		current->mm, &regs, &decoded, NULL);
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, scalar_initial, regs.regs[6]);
	tcti_lse_expect_exclusive(test, &cleared);
	ret = tcti_read_user_data(current->mm, mapped, &scalar_observed,
				  sizeof(scalar_observed));
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, scalar_initial, scalar_observed);
	stored = true;
	ret = tcti_store_exclusive_user_data(
		current->mm, mapped, &scalar_replacement,
		sizeof(scalar_replacement), pfn, generation,
		mapping_generation, &stored);
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_FALSE(test, stored);

	tcti_lse_write_pair(test, mapped, pair_initial, sizeof(u64));
	ret = tcti_load_exclusive_user_data(
		current->mm, mapped, pair_observed, sizeof(pair_observed),
		&pfn, &generation, &mapping_generation);
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_MEMEQ(test, pair_initial, pair_observed,
			  sizeof(pair_initial));

	instruction = LSE_CASP_BASE | (1U << 30) | (6U << 16) |
		(10U << 5) | 8U;
	decoded = tcti_decode_aarch64(instruction);
	memset(&regs, 0, sizeof(regs));
	regs.regs[6] = pair_initial[0];
	regs.regs[7] = pair_initial[1];
	regs.regs[8] = pair_initial[0];
	regs.regs[9] = pair_initial[1];
	regs.regs[10] = mapped;
	regs.pc = 0x11000;
	tcti_lse_set_exclusive(&active);
	ret = tcti_switch_debug_execute_decoded(
		current->mm, &regs, &decoded, NULL);
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, pair_initial[0], regs.regs[6]);
	KUNIT_EXPECT_EQ(test, pair_initial[1], regs.regs[7]);
	tcti_lse_expect_exclusive(test, &cleared);
	tcti_lse_expect_pair(test, mapped, pair_initial, sizeof(u64));
	stored = true;
	ret = tcti_store_exclusive_user_data(
		current->mm, mapped, pair_replacement, sizeof(pair_replacement),
		pfn, generation, mapping_generation, &stored);
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_FALSE(test, stored);

	tcti_lse_set_exclusive(&cleared);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void tcti_lse_resume_reports_alignment_faults(struct kunit *test)
{
	static const struct {
		u32 instruction;
		unsigned long offset;
		bool sp_base;
	} cases[] = {
		{
			LSE_RMW_BASE | (3U << 30) | (6U << 16) |
				(10U << 5) | 8U,
			1,
		},
		{
			LSE_CASP_BASE | (6U << 16) | (10U << 5) | 8U,
			4,
		},
		{
			LSE_CASP_BASE | (1U << 30) | (6U << 16) |
				(10U << 5) | 8U,
			8,
		},
		{
			LSE_RMW_BASE | (6U << 16) | (31U << 5) | 8U,
			8,
			true,
		},
	};
	static const struct tcti_lse_exclusive_state active = {
		.address = 0x23456000,
		.value = 0x12345678,
		.value2 = 0x87654321,
		.size = sizeof(u32),
		.valid = 1,
	};
	static const struct tcti_lse_exclusive_state cleared;
	unsigned long instructions = tcti_lse_map(test);
	unsigned long data = tcti_lse_map(test);
	u8 initial[2 * sizeof(u64)];
	u8 observed[2 * sizeof(u64)];
	size_t i;
	int ret;

	memset(initial, 0x5a, sizeof(initial));
	for (i = 0; i < ARRAY_SIZE(cases); i++) {
		struct pt_regs regs = {};
		struct pt_regs before;
		struct tcti_result result;

		ret = tcti_write_user_data(current->mm, instructions,
					   &cases[i].instruction,
					   sizeof(cases[i].instruction));
		KUNIT_ASSERT_EQ(test, 0, ret);
		ret = tcti_write_user_data(current->mm, data, initial,
					   sizeof(initial));
		KUNIT_ASSERT_EQ(test, 0, ret);
		regs.regs[6] = 0x1122334455667788ULL;
		regs.regs[7] = 0x8877665544332211ULL;
		regs.regs[8] = 0x0123456789abcdefULL;
		regs.regs[9] = 0xfedcba9876543210ULL;
		if (cases[i].sp_base)
			regs.sp = data + cases[i].offset;
		else
			regs.regs[10] = data + cases[i].offset;
		regs.pc = instructions;
		before = regs;
		tcti_lse_set_exclusive(&active);

		result = tcti_switch_debug_resume_user(
			current, &regs, current->mm);

		KUNIT_EXPECT_EQ(test, TCTI_EXIT_ALIGNMENT_FAULT,
				result.reason);
		KUNIT_EXPECT_EQ(test, -EFAULT, result.status);
		KUNIT_EXPECT_EQ(test, data + cases[i].offset,
				result.fault_address);
		KUNIT_EXPECT_EQ(test, TCTI_ACCESS_WRITE,
				result.fault_access);
		KUNIT_EXPECT_EQ(test, instructions, result.pc);
		KUNIT_EXPECT_EQ(test, cases[i].instruction,
				result.instruction);
		KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
		tcti_lse_expect_exclusive(test, &active);
		ret = tcti_read_user_data(current->mm, data, observed,
					  sizeof(observed));
		KUNIT_ASSERT_EQ(test, 0, ret);
		KUNIT_EXPECT_MEMEQ(test, initial, observed, sizeof(initial));
	}

	tcti_lse_set_exclusive(&cleared);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(data, PAGE_SIZE));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(instructions, PAGE_SIZE));
}

static void tcti_lse_decode_rejects_reserved_neighbors(struct kunit *test)
{
	u8 size;
	u8 order;
	u8 op;

	for (size = 0; size < 4; size++) {
		for (order = 0; order < 4; order++) {
			u32 cas = LSE_CAS_BASE | ((u32)size << 30);
			u32 rmw = LSE_RMW_BASE | ((u32)size << 30);
			struct tcti_decoded_instruction decoded;

			cas = tcti_lse_order(cas, true, order);
			rmw = tcti_lse_order(rmw, false, order);
			decoded = tcti_decode_aarch64(cas ^ BIT(10));
			KUNIT_EXPECT_EQ_MSG(test, TCTI_DECODE_UNSUPPORTED,
					    decoded.decode_class,
					    "instruction %#lx", cas ^ BIT(10));
			decoded = tcti_decode_aarch64(rmw ^ BIT(10));
			KUNIT_EXPECT_EQ_MSG(test, TCTI_DECODE_UNSUPPORTED,
					    decoded.decode_class,
					    "instruction %#lx", rmw ^ BIT(10));
			for (op = 9; op < 16; op++) {
				u32 instruction = rmw | ((u32)op << 12);

				decoded = tcti_decode_aarch64(instruction);
				KUNIT_EXPECT_EQ_MSG(
					test, TCTI_DECODE_UNSUPPORTED,
					decoded.decode_class,
					"instruction %#x", instruction);
			}
		}
	}

	for (size = 0; size < 2; size++) {
		for (order = 0; order < 4; order++) {
			u32 casp = LSE_CASP_BASE | ((u32)size << 30);
			struct tcti_decoded_instruction decoded;

			casp = tcti_lse_order(casp, true, order);
			decoded = tcti_decode_aarch64(casp ^ BIT(10));
			KUNIT_EXPECT_EQ_MSG(test, TCTI_DECODE_UNSUPPORTED,
					    decoded.decode_class,
					    "instruction %#lx", casp ^ BIT(10));
		}
	}
}

static void tcti_lse_decode_preserves_allocated_exclusive_neighbors(
	struct kunit *test)
{
	static const u32 instructions[] = {
		LSE_CASP_BASE | BIT(31),
		LSE_CASP_BASE | BIT(31) | BIT(22) | (31U << 16),
		LSE_CASP_BASE | BIT(31) | BIT(30),
		LSE_CASP_BASE | BIT(31) | BIT(30) | BIT(22) | (31U << 16),
	};
	size_t i;

	for (i = 0; i < ARRAY_SIZE(instructions); i++) {
		struct tcti_decoded_instruction decoded =
			tcti_decode_aarch64(instructions[i]);

		KUNIT_EXPECT_EQ_MSG(test, TCTI_DECODE_LOAD_STORE_EXCLUSIVE,
				    decoded.decode_class,
				    "instruction %#x", instructions[i]);
		KUNIT_EXPECT_TRUE(test, decoded.pair);
		KUNIT_EXPECT_EQ(test, i & 1U, decoded.load);
		KUNIT_EXPECT_EQ(test, 31, decoded.rt2);
		KUNIT_EXPECT_EQ(test, i & 1U ? 31 : 0, decoded.rs);
		KUNIT_EXPECT_EQ(test, i < 2 ? sizeof(u32) : sizeof(u64),
				decoded.access_size);
	}
}

static void tcti_lse_decode_rejects_invalid_casp_pairs(struct kunit *test)
{
	u8 size;
	u8 order;
	u8 rs;
	u8 rt;

	for (size = 0; size < 2; size++) {
		for (order = 0; order < 4; order++) {
			for (rs = 0; rs < 32; rs++) {
				for (rt = 0; rt < 32; rt++) {
					u32 instruction = LSE_CASP_BASE |
						((u32)size << 30) |
						((u32)rs << 16) |
						(7U << 5) | rt;
					struct tcti_decoded_instruction decoded;

					instruction = tcti_lse_order(
						instruction, true, order);
					decoded =
						tcti_decode_aarch64(instruction);
					if (!((rs | rt) & 1U)) {
						KUNIT_EXPECT_EQ_MSG(
							test,
							TCTI_DECODE_LSE_ATOMIC,
							decoded.decode_class,
							"instruction %#x",
							instruction);
						KUNIT_EXPECT_TRUE(test,
								  decoded.pair);
						KUNIT_EXPECT_EQ(test, rs,
								decoded.rs);
						KUNIT_EXPECT_EQ(test, rt,
								decoded.rt);
						KUNIT_EXPECT_EQ(test, rt + 1,
								decoded.rt2);
						continue;
					}
					KUNIT_EXPECT_EQ_MSG(
						test, TCTI_DECODE_UNSUPPORTED,
						decoded.decode_class,
						"instruction %#x",
						instruction);
				}
			}
		}
	}
}

static struct kunit_case tcti_lse_decode_test_cases[] = {
	KUNIT_CASE(tcti_lse_decode_cas_leaves),
	KUNIT_CASE(tcti_lse_decode_casp_leaves),
	KUNIT_CASE(tcti_lse_decode_rmw_and_swp_leaves),
	KUNIT_CASE(tcti_lse_execute_cas_scalar_matrix),
	KUNIT_CASE(tcti_lse_execute_casp_matrix),
	KUNIT_CASE(tcti_lse_execute_rmw_matrix),
	KUNIT_CASE(tcti_lse_execute_alias_zero_and_sp),
	KUNIT_CASE(tcti_lse_execute_fault_preserves_state),
	KUNIT_CASE(tcti_lse_execute_updates_exclusive_state),
	KUNIT_CASE(tcti_lse_same_value_cas_invalidates_reservations),
	KUNIT_CASE(tcti_lse_resume_reports_alignment_faults),
	KUNIT_CASE(tcti_lse_decode_rejects_reserved_neighbors),
	KUNIT_CASE(tcti_lse_decode_preserves_allocated_exclusive_neighbors),
	KUNIT_CASE(tcti_lse_decode_rejects_invalid_casp_pairs),
	{}
};

struct kunit_suite tcti_lse_decode_test_suite = {
	.name = "orlix-tcti-lse-decode",
	.test_cases = tcti_lse_decode_test_cases,
};

kunit_test_suite(tcti_lse_decode_test_suite);
