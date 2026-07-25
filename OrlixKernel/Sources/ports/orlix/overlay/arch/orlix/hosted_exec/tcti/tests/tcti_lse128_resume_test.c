// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched/mm.h>
#include <linux/string.h>
#include <linux/syscalls.h>
#include <asm/ptrace.h>
#include <asm/tcti.h>

#include "../decode_aarch64.h"

#define TCTI_LSE128_SVC 0xd4000001U
#define TCTI_LSE128_OPERATION_MASK (0xfU << 12)
#define TCTI_LSE_CASP_BASE 0x08207c00U

struct tcti_lse128_value {
	u64 low;
	u64 high;
};

struct tcti_lse_casp32_value {
	u32 low;
	u32 high;
};

struct tcti_lse128_leaf {
	u32 source_ordinal;
	enum tcti_lse_atomic_op operation;
	u8 order;
	u32 instruction;
};

/* AARCHMRS 2026-06 source ordinals 2345 through 2352. */
struct tcti_lse_casp_leaf {
	u32 source_ordinal;
	bool is_64bit;
	u8 order;
};

static const struct tcti_lse_casp_leaf tcti_lse_casp_leaves[] = {
	{ 2345U, false, 0 },
	{ 2346U, false, 2 },
	{ 2347U, false, 1 },
	{ 2348U, false, 3 },
	{ 2349U, true, 0 },
	{ 2350U, true, 2 },
	{ 2351U, true, 1 },
	{ 2352U, true, 3 },
};

static const struct tcti_lse128_leaf tcti_lse128_leaves[] = {
	{ 2521U, TCTI_LSE_ATOMIC_CLR, 0, 0x19201000U },
	{ 2522U, TCTI_LSE_ATOMIC_SET, 0, 0x19203000U },
	{ 2523U, TCTI_LSE_ATOMIC_SWP, 0, 0x19208000U },
	{ 2527U, TCTI_LSE_ATOMIC_CLR, 2, 0x19601000U },
	{ 2528U, TCTI_LSE_ATOMIC_SET, 2, 0x19603000U },
	{ 2529U, TCTI_LSE_ATOMIC_SWP, 2, 0x19608000U },
	{ 2533U, TCTI_LSE_ATOMIC_CLR, 1, 0x19a01000U },
	{ 2534U, TCTI_LSE_ATOMIC_SET, 1, 0x19a03000U },
	{ 2535U, TCTI_LSE_ATOMIC_SWP, 1, 0x19a08000U },
	{ 2539U, TCTI_LSE_ATOMIC_CLR, 3, 0x19e01000U },
	{ 2540U, TCTI_LSE_ATOMIC_SET, 3, 0x19e03000U },
	{ 2541U, TCTI_LSE_ATOMIC_SWP, 3, 0x19e08000U },
};

static const u8 tcti_lse128_reserved_operations[] = {
	0, 2, 4, 5, 6, 7, 9, 10, 11, 12, 13, 14, 15,
};

static const u32 tcti_lse128_fixed_bit_near_misses[] = {
	BIT(21), BIT(10),
};

static int tcti_lse128_resume_test_init(struct kunit *test)
{
	struct mm_struct *mm = mm_alloc();

	if (!mm)
		return -ENOMEM;
	kthread_use_mm(mm);
	test->priv = mm;
	return 0;
}

static void tcti_lse128_resume_test_exit(struct kunit *test)
{
	struct mm_struct *mm = test->priv;

	if (!mm)
		return;
	kthread_unuse_mm(mm);
	mmput(mm);
}

static u32 tcti_lse128_instruction(const struct tcti_lse128_leaf *leaf,
				   u8 rt2, u8 rn, u8 rt)
{
	return leaf->instruction | ((u32)rt2 << 16) | ((u32)rn << 5) | rt;
}

static u32 tcti_lse_casp_instruction(const struct tcti_lse_casp_leaf *leaf,
				    u8 rs, u8 rn, u8 rt)
{
	return TCTI_LSE_CASP_BASE |
		(leaf->is_64bit ? BIT(30) : 0) |
		(leaf->order & 1U ? BIT(22) : 0) |
		(leaf->order & 2U ? BIT(15) : 0) |
		((u32)rs << 16) | ((u32)rn << 5) | rt;
}

static struct tcti_lse128_value
tcti_lse128_expected(const struct tcti_lse128_leaf *leaf,
			     const struct tcti_lse128_value *initial,
			     const struct tcti_lse128_value *operand)
{
	struct tcti_lse128_value expected;

	switch (leaf->operation) {
	case TCTI_LSE_ATOMIC_CLR:
		expected.low = initial->low & ~operand->low;
		expected.high = initial->high & ~operand->high;
		break;
	case TCTI_LSE_ATOMIC_SET:
		expected.low = initial->low | operand->low;
		expected.high = initial->high | operand->high;
		break;
	default:
		expected = *operand;
		break;
	}
	return expected;
}

static unsigned long tcti_lse128_map(struct kunit *test, int prot)
{
	unsigned long mapped;

	mapped = ksys_mmap_pgoff(0, PAGE_SIZE, prot,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_EXPECT_FALSE(test, IS_ERR_VALUE(mapped));
	return mapped;
}

static unsigned long tcti_lse128_map_two_pages(struct kunit *test, int prot)
{
	unsigned long mapped;

	mapped = ksys_mmap_pgoff(0, 2 * PAGE_SIZE, prot,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_EXPECT_FALSE(test, IS_ERR_VALUE(mapped));
	return mapped;
}

static int tcti_lse128_write_program(unsigned long address,
				    const u32 *program, size_t count)
{
	int ret;

	ret = sys_mprotect(address, PAGE_SIZE, PROT_READ | PROT_WRITE);
	if (ret)
		return ret;
	ret = tcti_write_user_data(current->mm, address, program,
				   count * sizeof(*program));
	if (ret)
		return ret;
	return sys_mprotect(address, PAGE_SIZE, PROT_READ | PROT_EXEC);
}

static void tcti_lse128_resume_expect_fault_for_all_leaves(
	struct kunit *test, unsigned long instructions, unsigned long target,
	enum tcti_exit_reason reason, long status,
	const void *expected_memory, unsigned long observed_address,
	size_t observed_size, const char *kind)
{
	const struct tcti_lse128_value operand = {
		.low = 0x0f0f55ffaa5500ffULL,
		.high = 0x33cc0ff0f00f5aa5ULL,
	};
	u8 observed[sizeof(struct tcti_lse128_value)] = {};
	size_t index;

	for (index = 0; index < ARRAY_SIZE(tcti_lse128_leaves); index++) {
		const struct tcti_lse128_leaf *leaf = &tcti_lse128_leaves[index];
		const u32 program[] = {
			tcti_lse128_instruction(leaf, 6, 10, 8), TCTI_LSE128_SVC,
		};
		struct pt_regs regs = {};
		struct pt_regs before;
		struct tcti_result result;
		int ret;

		ret = tcti_lse128_write_program(instructions, program,
						ARRAY_SIZE(program));
		KUNIT_ASSERT_EQ_MSG(test, 0, ret, "leaf=%zu %s", index, kind);
		regs.regs[8] = operand.low;
		regs.regs[6] = operand.high;
		regs.regs[10] = target;
		regs.pc = instructions;
		regs.pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_Z_BIT |
			PSR_C_BIT | PSR_V_BIT;
		regs.syscallno = NO_SYSCALL;
		before = regs;

		result = tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ_MSG(test, reason, result.reason,
			"leaf=%zu %s", index, kind);
		KUNIT_EXPECT_EQ_MSG(test, status, result.status,
			"leaf=%zu %s", index, kind);
		KUNIT_EXPECT_EQ_MSG(test, target, result.fault_address,
			"leaf=%zu %s", index, kind);
		KUNIT_EXPECT_EQ_MSG(test, TCTI_ACCESS_WRITE, result.fault_access,
			"leaf=%zu %s", index, kind);
		KUNIT_EXPECT_EQ_MSG(test, instructions, result.pc,
			"leaf=%zu %s", index, kind);
		KUNIT_EXPECT_EQ_MSG(test, program[0], result.instruction,
			"leaf=%zu %s", index, kind);
		KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
		if (!observed_size)
			continue;
		KUNIT_ASSERT_LE(test, observed_size, sizeof(observed));
		ret = tcti_read_user_data(current->mm, observed_address, observed,
					  observed_size);
		KUNIT_ASSERT_EQ_MSG(test, 0, ret, "leaf=%zu %s", index, kind);
		KUNIT_EXPECT_MEMEQ(test, expected_memory, observed, observed_size);
	}
}

static void tcti_lse128_resume_rejects_instruction(
	struct kunit *test, unsigned long instructions, unsigned long data,
	u32 instruction, size_t leaf_index, const char *kind, u32 value)
{
	const struct tcti_lse128_value initial = {
		.low = 0x0123456789abcdefULL,
		.high = 0xfedcba9876543210ULL,
	};
	const u32 program[] = { instruction, TCTI_LSE128_SVC };
	struct tcti_lse128_value observed = {};
	struct pt_regs regs = {};
	struct pt_regs before;
	struct tcti_result result;
	int ret;

	ret = tcti_lse128_write_program(instructions, program,
					ARRAY_SIZE(program));
	KUNIT_ASSERT_EQ(test, 0, ret);
	ret = tcti_write_user_data(current->mm, data, &initial, sizeof(initial));
	KUNIT_ASSERT_EQ(test, 0, ret);
	regs.regs[8] = 0x0f0f55ffaa5500ffULL;
	regs.regs[6] = 0x33cc0ff0f00f5aa5ULL;
	regs.regs[10] = data;
	regs.pc = instructions;
	regs.pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_Z_BIT |
		PSR_C_BIT | PSR_V_BIT;
	regs.syscallno = NO_SYSCALL;
	before = regs;

	result = tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ_MSG(test, TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
			result.reason, "leaf=%zu %s=%#x", leaf_index, kind, value);
	KUNIT_EXPECT_EQ_MSG(test, -EOPNOTSUPP, result.status,
			"leaf=%zu %s=%#x", leaf_index, kind, value);
	KUNIT_EXPECT_EQ_MSG(test, instructions, result.pc,
			"leaf=%zu %s=%#x", leaf_index, kind, value);
	KUNIT_EXPECT_EQ_MSG(test, instruction, result.instruction,
			"leaf=%zu %s=%#x", leaf_index, kind, value);
	KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
	ret = tcti_read_user_data(current->mm, data, &observed, sizeof(observed));
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_MEMEQ(test, &initial, &observed, sizeof(observed));
}

static void tcti_lse128_resume_all_source_leaves(struct kunit *test)
{
	const struct tcti_lse128_value initial = {
		.low = 0x0123456789abcdefULL,
		.high = 0xfedcba9876543210ULL,
	};
	const struct tcti_lse128_value operand = {
		.low = 0x0f0f55ffaa5500ffULL,
		.high = 0x33cc0ff0f00f5aa5ULL,
	};
	unsigned long instructions = tcti_lse128_map(test, PROT_READ | PROT_WRITE);
	unsigned long data = tcti_lse128_map(test, PROT_READ | PROT_WRITE);
	size_t index;

	if (IS_ERR_VALUE(instructions) || IS_ERR_VALUE(data))
		return;
	for (index = 0; index < ARRAY_SIZE(tcti_lse128_leaves); index++) {
		const struct tcti_lse128_leaf *leaf = &tcti_lse128_leaves[index];
		const u32 program[] = {
			tcti_lse128_instruction(leaf, 6, 10, 8), TCTI_LSE128_SVC,
		};
		const struct tcti_lse128_value expected =
			tcti_lse128_expected(leaf, &initial, &operand);
		struct tcti_lse128_value observed = {};
		struct tcti_decoded_instruction decoded =
			tcti_decode_aarch64(program[0]);
		struct pt_regs regs = {};
		struct tcti_result result;
		int ret;

		ret = tcti_lse128_write_program(instructions, program,
						ARRAY_SIZE(program));
		KUNIT_ASSERT_EQ_MSG(test, 0, ret, "source=%u", leaf->source_ordinal);
		KUNIT_ASSERT_EQ_MSG(test, TCTI_DECODE_LSE_ATOMIC,
			decoded.decode_class, "source=%u", leaf->source_ordinal);
		KUNIT_EXPECT_EQ_MSG(test, leaf->operation, decoded.lse_atomic_op,
			"source=%u", leaf->source_ordinal);
		KUNIT_EXPECT_TRUE_MSG(test, decoded.lse128,
			"source=%u", leaf->source_ordinal);
		KUNIT_EXPECT_TRUE_MSG(test, decoded.pair,
			"source=%u", leaf->source_ordinal);
		KUNIT_EXPECT_EQ_MSG(test, sizeof(u64), decoded.access_size,
			"source=%u", leaf->source_ordinal);
		KUNIT_EXPECT_EQ_MSG(test, !!(leaf->order & 1U), decoded.acquire,
			"source=%u", leaf->source_ordinal);
		KUNIT_EXPECT_EQ_MSG(test, !!(leaf->order & 2U), decoded.release,
			"source=%u", leaf->source_ordinal);
		ret = tcti_write_user_data(current->mm, data, &initial,
					   sizeof(initial));
		KUNIT_ASSERT_EQ(test, 0, ret);
		regs.regs[8] = operand.low;
		regs.regs[6] = operand.high;
		regs.regs[10] = data;
		regs.pc = instructions;
		regs.pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_Z_BIT |
			PSR_C_BIT | PSR_V_BIT;
		regs.syscallno = NO_SYSCALL;

		result = tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ_MSG(test, TCTI_EXIT_SYSCALL, result.reason,
			"source=%u", leaf->source_ordinal);
		KUNIT_EXPECT_EQ_MSG(test, 0L, result.status,
			"source=%u", leaf->source_ordinal);
		KUNIT_EXPECT_EQ_MSG(test, instructions + sizeof(u32), result.pc,
			"source=%u", leaf->source_ordinal);
		KUNIT_EXPECT_EQ_MSG(test, TCTI_LSE128_SVC, result.instruction,
			"source=%u", leaf->source_ordinal);
		KUNIT_EXPECT_EQ_MSG(test, initial.low, regs.regs[8],
			"source=%u", leaf->source_ordinal);
		KUNIT_EXPECT_EQ_MSG(test, initial.high, regs.regs[6],
			"source=%u", leaf->source_ordinal);
		KUNIT_EXPECT_EQ_MSG(test, instructions + sizeof(u32), regs.pc,
			"source=%u", leaf->source_ordinal);
		KUNIT_EXPECT_EQ_MSG(test, PSR_MODE_EL0t | PSR_N_BIT | PSR_Z_BIT |
			PSR_C_BIT | PSR_V_BIT, regs.pstate, "leaf=%zu", index);
		ret = tcti_read_user_data(current->mm, data, &observed,
					  sizeof(observed));
		KUNIT_ASSERT_EQ_MSG(test, 0, ret, "source=%u", leaf->source_ordinal);
		KUNIT_EXPECT_MEMEQ_MSG(test, &expected, &observed, sizeof(observed),
			"source=%u", leaf->source_ordinal);
	}

	KUNIT_EXPECT_EQ(test, 0, vm_munmap(data, PAGE_SIZE));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(instructions, PAGE_SIZE));
}

static void tcti_lse_casp_resume_all_source_leaves(struct kunit *test)
{
	const struct tcti_lse128_value expected = {
		.low = 0x0123456789abcdefULL,
		.high = 0xfedcba9876543210ULL,
	};
	const struct tcti_lse128_value desired = {
		.low = 0x1122334455667788ULL,
		.high = 0x8877665544332211ULL,
	};
	const struct tcti_lse128_value mismatch = {
		.low = 0xa55aa55a01234567ULL,
		.high = 0x5aa55aa5fedcba98ULL,
	};
	const struct tcti_lse_casp32_value expected32 = {
		.low = expected.low,
		.high = expected.high,
	};
	const struct tcti_lse_casp32_value desired32 = {
		.low = desired.low,
		.high = desired.high,
	};
	const struct tcti_lse_casp32_value mismatch32 = {
		.low = mismatch.low,
		.high = mismatch.high,
	};
	unsigned long instructions = tcti_lse128_map(test, PROT_READ | PROT_WRITE);
	unsigned long data = tcti_lse128_map(test, PROT_READ | PROT_WRITE);
	size_t index;

	if (IS_ERR_VALUE(instructions) || IS_ERR_VALUE(data))
		return;
	for (index = 0; index < ARRAY_SIZE(tcti_lse_casp_leaves); index++) {
		const struct tcti_lse_casp_leaf *leaf = &tcti_lse_casp_leaves[index];
		const u32 program[] = {
			tcti_lse_casp_instruction(leaf, 6, 10, 8), TCTI_LSE128_SVC,
		};
		const size_t lane_size = leaf->is_64bit ? sizeof(u64) : sizeof(u32);
		const size_t total_size = 2 * lane_size;
		const void *expected_value = leaf->is_64bit ? (const void *)&expected :
			(const void *)&expected32;
		const void *desired_value = leaf->is_64bit ? (const void *)&desired :
			(const void *)&desired32;
		const void *mismatch_value = leaf->is_64bit ? (const void *)&mismatch :
			(const void *)&mismatch32;
		u8 observed[sizeof(struct tcti_lse128_value)] = {};
		struct tcti_decoded_instruction decoded =
			tcti_decode_aarch64(program[0]);
		struct pt_regs regs = {};
		struct tcti_result result;
		int ret;

		ret = tcti_lse128_write_program(instructions, program,
						ARRAY_SIZE(program));
		KUNIT_ASSERT_EQ_MSG(test, 0, ret, "source=%u", leaf->source_ordinal);
		KUNIT_ASSERT_EQ_MSG(test, TCTI_DECODE_LSE_ATOMIC,
			decoded.decode_class, "source=%u", leaf->source_ordinal);
		KUNIT_EXPECT_EQ_MSG(test, TCTI_LSE_ATOMIC_CAS,
			decoded.lse_atomic_op, "source=%u", leaf->source_ordinal);
		KUNIT_EXPECT_TRUE(test, decoded.pair);
		KUNIT_EXPECT_EQ(test, lane_size, decoded.access_size);
		KUNIT_EXPECT_EQ(test, !!(leaf->order & 1U), decoded.acquire);
		KUNIT_EXPECT_EQ(test, !!(leaf->order & 2U), decoded.release);
		ret = tcti_write_user_data(current->mm, data, expected_value,
					   total_size);
		KUNIT_ASSERT_EQ_MSG(test, 0, ret, "source=%u", leaf->source_ordinal);
		regs.regs[6] = leaf->is_64bit ? expected.low : expected32.low;
		regs.regs[7] = leaf->is_64bit ? expected.high : expected32.high;
		regs.regs[8] = leaf->is_64bit ? desired.low : desired32.low;
		regs.regs[9] = leaf->is_64bit ? desired.high : desired32.high;
		regs.regs[10] = data;
		regs.pc = instructions;
		regs.pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_Z_BIT |
			PSR_C_BIT | PSR_V_BIT;
		regs.syscallno = NO_SYSCALL;
		result = tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ_MSG(test, TCTI_EXIT_SYSCALL, result.reason,
			"source=%u", leaf->source_ordinal);
		KUNIT_EXPECT_EQ_MSG(test, 0L, result.status, "source=%u",
			leaf->source_ordinal);
		KUNIT_EXPECT_EQ_MSG(test, instructions + sizeof(u32), result.pc,
			"source=%u", leaf->source_ordinal);
		KUNIT_EXPECT_EQ_MSG(test, TCTI_LSE128_SVC, result.instruction,
			"source=%u", leaf->source_ordinal);
		KUNIT_EXPECT_EQ_MSG(test, PSR_MODE_EL0t | PSR_N_BIT | PSR_Z_BIT |
			PSR_C_BIT | PSR_V_BIT, regs.pstate, "source=%u",
			leaf->source_ordinal);
		KUNIT_EXPECT_EQ_MSG(test, leaf->is_64bit ? expected.low : expected32.low,
			regs.regs[6], "source=%u",
			leaf->source_ordinal);
		KUNIT_EXPECT_EQ_MSG(test, leaf->is_64bit ? expected.high : expected32.high,
			regs.regs[7], "source=%u",
			leaf->source_ordinal);
		ret = tcti_read_user_data(current->mm, data, &observed, total_size);
		KUNIT_ASSERT_EQ_MSG(test, 0, ret, "source=%u", leaf->source_ordinal);
		KUNIT_EXPECT_MEMEQ(test, desired_value, observed, total_size);

		ret = tcti_write_user_data(current->mm, data, mismatch_value, total_size);
		KUNIT_ASSERT_EQ_MSG(test, 0, ret, "source=%u", leaf->source_ordinal);
		regs.regs[6] = leaf->is_64bit ? expected.low : expected32.low;
		regs.regs[7] = leaf->is_64bit ? expected.high : expected32.high;
		regs.regs[8] = leaf->is_64bit ? desired.low : desired32.low;
		regs.regs[9] = leaf->is_64bit ? desired.high : desired32.high;
		regs.pc = instructions;
		result = tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ_MSG(test, TCTI_EXIT_SYSCALL, result.reason,
			"source=%u mismatch", leaf->source_ordinal);
		KUNIT_EXPECT_EQ_MSG(test, instructions + sizeof(u32), result.pc,
			"source=%u mismatch", leaf->source_ordinal);
		KUNIT_EXPECT_EQ_MSG(test, PSR_MODE_EL0t | PSR_N_BIT | PSR_Z_BIT |
			PSR_C_BIT | PSR_V_BIT, regs.pstate, "source=%u mismatch",
			leaf->source_ordinal);
		KUNIT_EXPECT_EQ_MSG(test, leaf->is_64bit ? mismatch.low : mismatch32.low,
			regs.regs[6],
			"source=%u mismatch", leaf->source_ordinal);
		KUNIT_EXPECT_EQ_MSG(test, leaf->is_64bit ? mismatch.high : mismatch32.high,
			regs.regs[7],
			"source=%u mismatch", leaf->source_ordinal);
		ret = tcti_read_user_data(current->mm, data, &observed, total_size);
		KUNIT_ASSERT_EQ_MSG(test, 0, ret, "source=%u mismatch",
			leaf->source_ordinal);
		KUNIT_EXPECT_MEMEQ(test, mismatch_value, observed, total_size);
	}

	KUNIT_EXPECT_EQ(test, 0, vm_munmap(data, PAGE_SIZE));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(instructions, PAGE_SIZE));
}

static void tcti_lse_casp_rejects_invalid_register_pairs(struct kunit *test)
{
	const struct tcti_lse_casp_leaf *leaf = &tcti_lse_casp_leaves[4];

	KUNIT_EXPECT_EQ(test, TCTI_DECODE_UNSUPPORTED,
		tcti_decode_aarch64(tcti_lse_casp_instruction(leaf, 7, 10, 8)).decode_class);
	KUNIT_EXPECT_EQ(test, TCTI_DECODE_UNSUPPORTED,
		tcti_decode_aarch64(tcti_lse_casp_instruction(leaf, 6, 10, 9)).decode_class);
}

static void tcti_lse_casp_alignment_faults_preserve_source_state(
	struct kunit *test)
{
	const u8 initial[2 * sizeof(struct tcti_lse128_value)] = {
		[0 ... (2 * sizeof(struct tcti_lse128_value)) - 1] = 0x5a,
	};
	u8 observed[sizeof(initial)] = {};
	unsigned long instructions = tcti_lse128_map(test, PROT_READ | PROT_WRITE);
	unsigned long data = tcti_lse128_map(test, PROT_READ | PROT_WRITE);
	size_t index;

	if (IS_ERR_VALUE(instructions) || IS_ERR_VALUE(data))
		return;
	for (index = 0; index < ARRAY_SIZE(tcti_lse_casp_leaves); index++) {
		const struct tcti_lse_casp_leaf *leaf = &tcti_lse_casp_leaves[index];
		const u32 program[] = {
			tcti_lse_casp_instruction(leaf, 6, 10, 8), TCTI_LSE128_SVC,
		};
		struct pt_regs regs = {};
		struct pt_regs before;
		struct tcti_result result;
		int ret;

		ret = tcti_lse128_write_program(instructions, program,
						ARRAY_SIZE(program));
		KUNIT_ASSERT_EQ_MSG(test, 0, ret, "source=%u", leaf->source_ordinal);
		ret = tcti_write_user_data(current->mm, data, initial, sizeof(initial));
		KUNIT_ASSERT_EQ_MSG(test, 0, ret, "source=%u", leaf->source_ordinal);
		regs.regs[6] = 0x0123456789abcdefULL;
		regs.regs[7] = 0xfedcba9876543210ULL;
		regs.regs[8] = 0x1122334455667788ULL;
		regs.regs[9] = 0x8877665544332211ULL;
		regs.regs[10] = data + 1;
		regs.pc = instructions;
		regs.pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_Z_BIT |
			PSR_C_BIT | PSR_V_BIT;
		regs.syscallno = NO_SYSCALL;
		before = regs;
		result = tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ_MSG(test, TCTI_EXIT_ALIGNMENT_FAULT, result.reason,
			"source=%u", leaf->source_ordinal);
		KUNIT_EXPECT_EQ_MSG(test, -EFAULT, result.status, "source=%u",
			leaf->source_ordinal);
		KUNIT_EXPECT_EQ_MSG(test, data + 1, result.fault_address,
			"source=%u", leaf->source_ordinal);
		KUNIT_EXPECT_EQ_MSG(test, TCTI_ACCESS_WRITE, result.fault_access,
			"source=%u", leaf->source_ordinal);
		KUNIT_EXPECT_EQ_MSG(test, instructions, result.pc, "source=%u",
			leaf->source_ordinal);
		KUNIT_EXPECT_EQ_MSG(test, program[0], result.instruction,
			"source=%u", leaf->source_ordinal);
		KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
		ret = tcti_read_user_data(current->mm, data, observed, sizeof(observed));
		KUNIT_ASSERT_EQ_MSG(test, 0, ret, "source=%u", leaf->source_ordinal);
		KUNIT_EXPECT_MEMEQ(test, initial, observed, sizeof(initial));
	}

	KUNIT_EXPECT_EQ(test, 0, vm_munmap(data, PAGE_SIZE));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(instructions, PAGE_SIZE));
}

static void tcti_lse128_resume_fault_does_not_mutate(struct kunit *test)
{
	const struct tcti_lse128_value initial = {
		.low = 0x1111222233334444ULL,
		.high = 0xaaaabbbbccccddddULL,
	};
	const struct tcti_lse128_leaf leaf = {
		TCTI_LSE_ATOMIC_SWP, 0x19e08000U,
	};
	const u32 program[] = {
		tcti_lse128_instruction(&leaf, 6, 10, 8), TCTI_LSE128_SVC,
	};
	unsigned long instructions = tcti_lse128_map(test, PROT_READ | PROT_WRITE);
	unsigned long data = tcti_lse128_map(test, PROT_READ | PROT_WRITE);
	struct tcti_lse128_value observed = {};
	struct pt_regs regs = {};
	struct pt_regs before;
	struct tcti_result result;
	int ret;

	if (IS_ERR_VALUE(instructions) || IS_ERR_VALUE(data))
		return;
	ret = tcti_lse128_write_program(instructions, program,
					ARRAY_SIZE(program));
	KUNIT_ASSERT_EQ(test, 0, ret);
	ret = tcti_write_user_data(current->mm, data, &initial, sizeof(initial));
	KUNIT_ASSERT_EQ(test, 0, ret);
	regs.regs[8] = U64_MAX;
	regs.regs[6] = U64_MAX - 1;
	regs.regs[10] = data;
	regs.pc = instructions;
	regs.pstate = PSR_MODE_EL0t | PSR_N_BIT;
	regs.syscallno = NO_SYSCALL;
	before = regs;
	KUNIT_ASSERT_EQ(test, 0, sys_mprotect(data, PAGE_SIZE, PROT_READ));

	result = tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, TCTI_EXIT_USER_FAULT, result.reason);
	KUNIT_EXPECT_EQ(test, -EACCES, result.status);
	KUNIT_EXPECT_EQ(test, data, result.fault_address);
	KUNIT_EXPECT_EQ(test, TCTI_ACCESS_WRITE, result.fault_access);
	KUNIT_EXPECT_EQ(test, instructions, result.pc);
	KUNIT_EXPECT_EQ(test, program[0], result.instruction);
	KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
	ret = tcti_read_user_data(current->mm, data, &observed, sizeof(observed));
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_MEMEQ(test, &initial, &observed, sizeof(observed));
	KUNIT_EXPECT_EQ(test, 0, sys_mprotect(data, PAGE_SIZE,
					PROT_READ | PROT_WRITE));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(data, PAGE_SIZE));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(instructions, PAGE_SIZE));
}

static void tcti_lse128_resume_alignment_faults_all_leaves(
	struct kunit *test)
{
	const struct tcti_lse128_value initial = {
		.low = 0x1111222233334444ULL,
		.high = 0xaaaabbbbccccddddULL,
	};
	unsigned long instructions = tcti_lse128_map(test, PROT_READ | PROT_WRITE);
	unsigned long data = tcti_lse128_map(test, PROT_READ | PROT_WRITE);
	unsigned long target;
	int ret;

	if (IS_ERR_VALUE(instructions) || IS_ERR_VALUE(data))
		return;
	target = data + sizeof(u64);
	ret = tcti_write_user_data(current->mm, target, &initial,
				   sizeof(initial));
	KUNIT_ASSERT_EQ(test, 0, ret);
	tcti_lse128_resume_expect_fault_for_all_leaves(
		test, instructions, target, TCTI_EXIT_ALIGNMENT_FAULT, -EFAULT,
		&initial, target, sizeof(initial), "unaligned");
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(data, PAGE_SIZE));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(instructions, PAGE_SIZE));
}

static void tcti_lse128_resume_readonly_faults_all_leaves(
	struct kunit *test)
{
	const struct tcti_lse128_value initial = {
		.low = 0x5555666677778888ULL,
		.high = 0x9999aaaabbbbccccULL,
	};
	unsigned long instructions = tcti_lse128_map(test, PROT_READ | PROT_WRITE);
	unsigned long data = tcti_lse128_map(test, PROT_READ | PROT_WRITE);
	int ret;

	if (IS_ERR_VALUE(instructions) || IS_ERR_VALUE(data))
		return;
	ret = tcti_write_user_data(current->mm, data, &initial, sizeof(initial));
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_ASSERT_EQ(test, 0, sys_mprotect(data, PAGE_SIZE, PROT_READ));
	tcti_lse128_resume_expect_fault_for_all_leaves(
		test, instructions, data, TCTI_EXIT_USER_FAULT, -EACCES,
		&initial, data, sizeof(initial), "readonly");
	KUNIT_EXPECT_EQ(test, 0,
			sys_mprotect(data, PAGE_SIZE, PROT_READ | PROT_WRITE));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(data, PAGE_SIZE));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(instructions, PAGE_SIZE));
}

static void tcti_lse128_resume_unmapped_faults_all_leaves(struct kunit *test)
{
	const struct tcti_lse128_value initial = {
		.low = 0x123456789abcdef0ULL,
		.high = 0x0fedcba987654321ULL,
	};
	unsigned long instructions = tcti_lse128_map(test, PROT_READ | PROT_WRITE);
	unsigned long data = tcti_lse128_map_two_pages(test,
							   PROT_READ | PROT_WRITE);
	unsigned long target;
	int ret;

	if (IS_ERR_VALUE(instructions) || IS_ERR_VALUE(data))
		return;
	ret = tcti_write_user_data(current->mm, data, &initial, sizeof(initial));
	KUNIT_ASSERT_EQ(test, 0, ret);
	target = data + PAGE_SIZE;
	KUNIT_ASSERT_EQ(test, 0, vm_munmap(target, PAGE_SIZE));
	tcti_lse128_resume_expect_fault_for_all_leaves(
		test, instructions, target, TCTI_EXIT_USER_FAULT, -EFAULT,
		&initial, data, sizeof(initial), "unmapped");
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(data, PAGE_SIZE));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(instructions, PAGE_SIZE));
}

static void tcti_lse128_resume_alignment_precedes_second_page_permission(
	struct kunit *test)
{
	const struct tcti_lse128_value initial = {
		.low = 0x0102030405060708ULL,
		.high = 0x8899aabbccddeeffULL,
	};
	unsigned long instructions = tcti_lse128_map(test, PROT_READ | PROT_WRITE);
	unsigned long data = tcti_lse128_map_two_pages(test,
							   PROT_READ | PROT_WRITE);
	unsigned long target;
	struct tcti_lse128_value observed = {};
	int ret;

	if (IS_ERR_VALUE(instructions) || IS_ERR_VALUE(data))
		return;
	/*
	 * PAGE_SIZE is divisible by 16, so a valid aligned 16-byte LSE128 access
	 * cannot straddle pages. This target proves alignment rejection precedes
	 * any second-page permission check. It does not execute a spanning access.
	 */
	target = data + PAGE_SIZE - sizeof(u64);
	ret = tcti_write_user_data(current->mm, target, &initial, sizeof(initial));
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_ASSERT_EQ(test, 0,
			sys_mprotect(data + PAGE_SIZE, PAGE_SIZE, PROT_NONE));
	tcti_lse128_resume_expect_fault_for_all_leaves(
		test, instructions, target, TCTI_EXIT_ALIGNMENT_FAULT, -EFAULT,
		&initial, target, sizeof(u64), "alignment-before-second-page");
	KUNIT_ASSERT_EQ(test, 0, sys_mprotect(data + PAGE_SIZE, PAGE_SIZE,
						 PROT_READ | PROT_WRITE));
	ret = tcti_read_user_data(current->mm, target, &observed,
				  sizeof(observed));
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_MEMEQ(test, &initial, &observed, sizeof(observed));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(data, 2 * PAGE_SIZE));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(instructions, PAGE_SIZE));
}

static void tcti_lse128_resume_rejects_reserved_operations(struct kunit *test)
{
	unsigned long instructions = tcti_lse128_map(test, PROT_READ | PROT_WRITE);
	unsigned long data = tcti_lse128_map(test, PROT_READ | PROT_WRITE);
	size_t leaf_index;

	if (IS_ERR_VALUE(instructions) || IS_ERR_VALUE(data))
		return;
	for (leaf_index = 0; leaf_index < ARRAY_SIZE(tcti_lse128_leaves);
	     leaf_index++) {
		const struct tcti_lse128_leaf *leaf =
			&tcti_lse128_leaves[leaf_index];
		size_t operation_index;

		for (operation_index = 0;
		     operation_index < ARRAY_SIZE(tcti_lse128_reserved_operations);
		     operation_index++) {
			u32 instruction = tcti_lse128_instruction(leaf, 6, 10, 8);
			u8 operation = tcti_lse128_reserved_operations[operation_index];

			instruction &= ~TCTI_LSE128_OPERATION_MASK;
			instruction |= (u32)operation << 12;
			tcti_lse128_resume_rejects_instruction(
				test, instructions, data, instruction, leaf_index,
				"reserved-operation", operation);
		}
	}
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(data, PAGE_SIZE));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(instructions, PAGE_SIZE));
}

static void tcti_lse128_resume_rejects_fixed_bit_near_misses(
	struct kunit *test)
{
	unsigned long instructions = tcti_lse128_map(test, PROT_READ | PROT_WRITE);
	unsigned long data = tcti_lse128_map(test, PROT_READ | PROT_WRITE);
	size_t leaf_index;

	if (IS_ERR_VALUE(instructions) || IS_ERR_VALUE(data))
		return;
	for (leaf_index = 0; leaf_index < ARRAY_SIZE(tcti_lse128_leaves);
	     leaf_index++) {
		const struct tcti_lse128_leaf *leaf =
			&tcti_lse128_leaves[leaf_index];
		size_t mutation_index;

		for (mutation_index = 0;
		     mutation_index < ARRAY_SIZE(tcti_lse128_fixed_bit_near_misses);
		     mutation_index++) {
			u32 mutation = tcti_lse128_fixed_bit_near_misses[mutation_index];
			u32 instruction =
			tcti_lse128_instruction(leaf, 6, 10, 8) ^ mutation;

			tcti_lse128_resume_rejects_instruction(
				test, instructions, data, instruction, leaf_index,
				"fixed-bit-near-miss", mutation);
		}
	}
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(data, PAGE_SIZE));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(instructions, PAGE_SIZE));
}

static struct kunit_case tcti_lse128_resume_test_cases[] = {
	KUNIT_CASE(tcti_lse128_resume_all_source_leaves),
	KUNIT_CASE(tcti_lse_casp_resume_all_source_leaves),
	KUNIT_CASE(tcti_lse_casp_rejects_invalid_register_pairs),
	KUNIT_CASE(tcti_lse_casp_alignment_faults_preserve_source_state),
	KUNIT_CASE(tcti_lse128_resume_fault_does_not_mutate),
	KUNIT_CASE(tcti_lse128_resume_alignment_faults_all_leaves),
	KUNIT_CASE(tcti_lse128_resume_readonly_faults_all_leaves),
	KUNIT_CASE(tcti_lse128_resume_unmapped_faults_all_leaves),
	KUNIT_CASE(tcti_lse128_resume_alignment_precedes_second_page_permission),
	KUNIT_CASE(tcti_lse128_resume_rejects_reserved_operations),
	KUNIT_CASE(tcti_lse128_resume_rejects_fixed_bit_near_misses),
	{}
};

static struct kunit_suite tcti_lse128_resume_test_suite = {
	.name = "orlix-tcti-lse128-resume",
	.init = tcti_lse128_resume_test_init,
	.exit = tcti_lse128_resume_test_exit,
	.test_cases = tcti_lse128_resume_test_cases,
};
kunit_test_suite(tcti_lse128_resume_test_suite);

MODULE_LICENSE("GPL");
