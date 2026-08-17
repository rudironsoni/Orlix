// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/err.h>
#include <linux/limits.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched/mm.h>
#include <linux/string.h>
#include <linux/syscalls.h>
#include <asm/orlix_tcti.h>

struct orlix_tcti_atomic_128_value {
	u64 low;
	u64 high;
};

struct orlix_tcti_atomic_128_case {
	enum orlix_tcti_atomic_memory_operation operation;
	struct orlix_tcti_atomic_128_value expected;
};

static unsigned long orlix_tcti_atomic_128_map(struct kunit *test, size_t size,
					 int prot)
{
	unsigned long mapped;

	mapped = ksys_mmap_pgoff(0, size, prot, MAP_PRIVATE | MAP_ANONYMOUS,
				 -1, 0);
	KUNIT_EXPECT_FALSE(test, IS_ERR_VALUE(mapped));
	return mapped;
}

static void orlix_tcti_atomic_128_expect_memory(
	struct kunit *test, unsigned long mapped,
	const struct orlix_tcti_atomic_128_value *expected)
{
	struct orlix_tcti_atomic_128_value observed = {};
	int ret;

	ret = orlix_tcti_read_user_data(current->mm, mapped, &observed,
				  sizeof(observed));
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_MEMEQ(test, expected, &observed, sizeof(observed));
}

static void orlix_tcti_atomic_128_rmw_asymmetric_lanes(struct kunit *test)
{
	const struct orlix_tcti_atomic_128_value initial = {
		.low = 0x0123456789abcdefULL,
		.high = 0xfedcba9876543210ULL,
	};
	const struct orlix_tcti_atomic_128_value operand = {
		.low = 0x0f0f55ffaa5500ffULL,
		.high = 0x33cc0ff0f00f5aa5ULL,
	};
	const struct orlix_tcti_atomic_128_case cases[] = {
		{
			.operation = ORLIX_TCTI_ATOMIC_MEMORY_SWP,
			.expected = operand,
		},
		{
			.operation = ORLIX_TCTI_ATOMIC_MEMORY_CLR,
			.expected = {
				.low = initial.low & ~operand.low,
				.high = initial.high & ~operand.high,
			},
		},
		{
			.operation = ORLIX_TCTI_ATOMIC_MEMORY_SET,
			.expected = {
				.low = initial.low | operand.low,
				.high = initial.high | operand.high,
			},
		},
	};
	unsigned long mapped =
		orlix_tcti_atomic_128_map(test, PAGE_SIZE, PROT_READ | PROT_WRITE);
	size_t i;

	for (i = 0; i < ARRAY_SIZE(cases); i++) {
		struct orlix_tcti_atomic_128_value old = {};
		bool exchanged = false;
		int ret;

		ret = orlix_tcti_write_user_data(current->mm, mapped, &initial,
					   sizeof(initial));
		KUNIT_ASSERT_EQ(test, 0, ret);
		ret = orlix_tcti_atomic_user_data(
			current->mm, mapped, cases[i].operation,
			ORLIX_TCTI_ATOMIC_MEMORY_ACQ_REL, NULL, &operand, &old,
			sizeof(old), &exchanged);
		KUNIT_ASSERT_EQ(test, 0, ret);
		KUNIT_EXPECT_TRUE(test, exchanged);
		KUNIT_EXPECT_MEMEQ(test, &initial, &old, sizeof(old));
		orlix_tcti_atomic_128_expect_memory(test, mapped, &cases[i].expected);
	}

	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void orlix_tcti_atomic_128_same_value_writes_break_reservations(
	struct kunit *test)
{
	const struct orlix_tcti_atomic_128_value initial = {
		.low = 0x1122334455667788ULL,
		.high = 0x8877665544332211ULL,
	};
	const struct orlix_tcti_atomic_128_value zero = {};
	const enum orlix_tcti_atomic_memory_operation operations[] = {
		ORLIX_TCTI_ATOMIC_MEMORY_SWP,
		ORLIX_TCTI_ATOMIC_MEMORY_CLR,
		ORLIX_TCTI_ATOMIC_MEMORY_SET,
	};
	unsigned long mapped =
		orlix_tcti_atomic_128_map(test, PAGE_SIZE, PROT_READ | PROT_WRITE);
	size_t i;

	for (i = 0; i < ARRAY_SIZE(operations); i++) {
		const struct orlix_tcti_atomic_128_value *operand =
			operations[i] == ORLIX_TCTI_ATOMIC_MEMORY_SWP ? &initial : &zero;
		struct orlix_tcti_atomic_128_value observed = {};
		struct orlix_tcti_atomic_128_value desired = {
			.low = ~initial.low,
			.high = ~initial.high,
		};
		unsigned long pfn;
		u64 generation;
		u64 mapping_generation;
		bool exchanged = false;
		bool stored = true;
		int ret;

		ret = orlix_tcti_write_user_data(current->mm, mapped, &initial,
					   sizeof(initial));
		KUNIT_ASSERT_EQ(test, 0, ret);
		ret = orlix_tcti_load_exclusive_user_data(
			current->mm, mapped, &observed, sizeof(observed), &pfn,
			&generation, &mapping_generation);
		KUNIT_ASSERT_EQ(test, 0, ret);
		ret = orlix_tcti_atomic_user_data(
			current->mm, mapped, operations[i],
			ORLIX_TCTI_ATOMIC_MEMORY_RELAXED, NULL, operand, &observed,
			sizeof(observed), &exchanged);
		KUNIT_ASSERT_EQ(test, 0, ret);
		KUNIT_EXPECT_TRUE(test, exchanged);
		ret = orlix_tcti_store_exclusive_user_data(
			current->mm, mapped, &desired, sizeof(desired), pfn,
			generation, mapping_generation, &stored);
		KUNIT_ASSERT_EQ(test, 0, ret);
		KUNIT_EXPECT_FALSE(test, stored);
		orlix_tcti_atomic_128_expect_memory(test, mapped, &initial);
	}

	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void orlix_tcti_atomic_128_unsupported_ops_do_not_mutate(struct kunit *test)
{
	const struct orlix_tcti_atomic_128_value initial = {
		.low = 0xa55aa55a01234567ULL,
		.high = 0x5aa55aa5fedcba98ULL,
	};
	const struct orlix_tcti_atomic_128_value operand = {
		.low = 0x1111111122222222ULL,
		.high = 0x3333333344444444ULL,
	};
	const enum orlix_tcti_atomic_memory_operation operations[] = {
		ORLIX_TCTI_ATOMIC_MEMORY_ADD,
		ORLIX_TCTI_ATOMIC_MEMORY_EOR,
		ORLIX_TCTI_ATOMIC_MEMORY_SMAX,
		ORLIX_TCTI_ATOMIC_MEMORY_SMIN,
		ORLIX_TCTI_ATOMIC_MEMORY_UMAX,
		ORLIX_TCTI_ATOMIC_MEMORY_UMIN,
	};
	unsigned long mapped =
		orlix_tcti_atomic_128_map(test, PAGE_SIZE, PROT_READ | PROT_WRITE);
	size_t i;
	int ret;

	ret = orlix_tcti_write_user_data(current->mm, mapped, &initial,
				   sizeof(initial));
	KUNIT_ASSERT_EQ(test, 0, ret);
	for (i = 0; i < ARRAY_SIZE(operations); i++) {
		const struct orlix_tcti_atomic_128_value old_sentinel = {
			.low = U64_MAX,
			.high = U64_MAX,
		};
		struct orlix_tcti_atomic_128_value old = old_sentinel;
		bool exchanged = true;

		ret = orlix_tcti_atomic_user_data(
			current->mm, mapped, operations[i],
			ORLIX_TCTI_ATOMIC_MEMORY_RELAXED, NULL, &operand, &old,
			sizeof(old), &exchanged);
		KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, ret);
		KUNIT_EXPECT_TRUE(test, exchanged);
		KUNIT_EXPECT_MEMEQ(test, &old_sentinel, &old, sizeof(old));
		orlix_tcti_atomic_128_expect_memory(test, mapped, &initial);
	}

	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void orlix_tcti_atomic_128_faults_do_not_partially_mutate(struct kunit *test)
{
	const struct orlix_tcti_atomic_128_value operand = {
		.low = U64_MAX,
		.high = U64_MAX,
	};
	const struct orlix_tcti_atomic_128_value old_sentinel = {
		.low = 0x13579bdf2468ace0ULL,
		.high = 0x02468ace13579bdfULL,
	};
	struct orlix_tcti_atomic_128_value old = old_sentinel;
	u8 initial[32];
	u8 observed[32];
	unsigned long mapped =
		orlix_tcti_atomic_128_map(test, 2 * PAGE_SIZE,
				    PROT_READ | PROT_WRITE);
	unsigned long readonly =
		orlix_tcti_atomic_128_map(test, PAGE_SIZE, PROT_READ);
	bool exchanged = true;
	int ret;

	memset(initial, 0x5a, sizeof(initial));
	ret = orlix_tcti_write_user_data(current->mm, mapped, initial,
				   sizeof(initial));
	KUNIT_ASSERT_EQ(test, 0, ret);
	ret = orlix_tcti_atomic_user_data(
		current->mm, mapped + 1, ORLIX_TCTI_ATOMIC_MEMORY_SET,
		ORLIX_TCTI_ATOMIC_MEMORY_RELAXED, NULL, &operand, &old, sizeof(old),
		&exchanged);
	KUNIT_EXPECT_EQ(test, -EFAULT, ret);
	KUNIT_EXPECT_TRUE(test, exchanged);
	KUNIT_EXPECT_MEMEQ(test, &old_sentinel, &old, sizeof(old));
	ret = orlix_tcti_read_user_data(current->mm, mapped, observed,
				  sizeof(observed));
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_MEMEQ(test, initial, observed, sizeof(initial));

	/*
	 * PAGE_SIZE is a multiple of 16, so every 16-byte access that crosses a
	 * page boundary is necessarily misaligned. Keep canaries on both pages
	 * to prove that the combined rejection cannot partially mutate either.
	 */
	memset(initial, 0xa5, sizeof(initial));
	ret = orlix_tcti_write_user_data(current->mm, mapped + PAGE_SIZE - 16,
				   initial, sizeof(initial));
	KUNIT_ASSERT_EQ(test, 0, ret);
	old = old_sentinel;
	exchanged = true;
	ret = orlix_tcti_atomic_user_data(
		current->mm, mapped + PAGE_SIZE - 8,
		ORLIX_TCTI_ATOMIC_MEMORY_SET, ORLIX_TCTI_ATOMIC_MEMORY_RELAXED, NULL,
		&operand, &old, sizeof(old), &exchanged);
	KUNIT_EXPECT_EQ(test, -EFAULT, ret);
	KUNIT_EXPECT_TRUE(test, exchanged);
	KUNIT_EXPECT_MEMEQ(test, &old_sentinel, &old, sizeof(old));
	ret = orlix_tcti_read_user_data(current->mm, mapped + PAGE_SIZE - 16,
				  observed, sizeof(observed));
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_MEMEQ(test, initial, observed, sizeof(initial));

	old = old_sentinel;
	exchanged = true;
	ret = orlix_tcti_atomic_user_data(
		current->mm, readonly, ORLIX_TCTI_ATOMIC_MEMORY_SET,
		ORLIX_TCTI_ATOMIC_MEMORY_RELAXED, NULL, &operand, &old, sizeof(old),
		&exchanged);
	KUNIT_EXPECT_EQ(test, -EACCES, ret);
	KUNIT_EXPECT_TRUE(test, exchanged);
	KUNIT_EXPECT_MEMEQ(test, &old_sentinel, &old, sizeof(old));
	orlix_tcti_atomic_128_expect_memory(
		test, readonly, &(const struct orlix_tcti_atomic_128_value) {});

	KUNIT_EXPECT_EQ(test, 0, vm_munmap(readonly, PAGE_SIZE));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, 2 * PAGE_SIZE));
}

static struct kunit_case orlix_tcti_atomic_128_rmw_test_cases[] = {
	KUNIT_CASE(orlix_tcti_atomic_128_rmw_asymmetric_lanes),
	KUNIT_CASE(orlix_tcti_atomic_128_same_value_writes_break_reservations),
	KUNIT_CASE(orlix_tcti_atomic_128_unsupported_ops_do_not_mutate),
	KUNIT_CASE(orlix_tcti_atomic_128_faults_do_not_partially_mutate),
	{}
};

static int orlix_tcti_atomic_128_rmw_test_init(struct kunit *test)
{
	struct mm_struct *mm = mm_alloc();

	if (!mm)
		return -ENOMEM;
	kthread_use_mm(mm);
	test->priv = mm;
	return 0;
}

static void orlix_tcti_atomic_128_rmw_test_exit(struct kunit *test)
{
	struct mm_struct *mm = test->priv;

	if (!mm)
		return;
	kthread_unuse_mm(mm);
	mmput(mm);
}

static struct kunit_suite orlix_tcti_atomic_128_rmw_test_suite = {
	.name = "orlix-tcti-atomic-128-rmw",
	.init = orlix_tcti_atomic_128_rmw_test_init,
	.exit = orlix_tcti_atomic_128_rmw_test_exit,
	.test_cases = orlix_tcti_atomic_128_rmw_test_cases,
};

kunit_test_suite(orlix_tcti_atomic_128_rmw_test_suite);
