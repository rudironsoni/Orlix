// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/completion.h>
#include <linux/err.h>
#include <linux/jiffies.h>
#include <linux/kthread.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched/mm.h>
#include <linux/string.h>
#include <linux/syscalls.h>
#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/tcti.h>
#include <asm/uaccess.h>

#include "tcti_test_suites.h"

#define TCTI_ATOMIC_PAIR_ITERATIONS 1024

static unsigned long tcti_atomic_test_map(struct kunit *test, int prot)
{
	unsigned long mapped;

	mapped = ksys_mmap_pgoff(0, PAGE_SIZE, prot,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_EXPECT_FALSE(test, IS_ERR_VALUE(mapped));
	return mapped;
}

static u64 tcti_atomic_mask(size_t size)
{
	return size == sizeof(u64) ? U64_MAX : (1ULL << (size * 8)) - 1;
}

static u64 tcti_atomic_rmw_expected(enum tcti_atomic_memory_operation operation,
					    u64 old, u64 operand, size_t size)
{
	u64 mask = tcti_atomic_mask(size);

	switch (operation) {
	case TCTI_ATOMIC_MEMORY_SWP:
		return operand & mask;
	case TCTI_ATOMIC_MEMORY_ADD:
		return (old + operand) & mask;
	case TCTI_ATOMIC_MEMORY_CLR:
		return old & ~operand & mask;
	case TCTI_ATOMIC_MEMORY_EOR:
		return (old ^ operand) & mask;
	case TCTI_ATOMIC_MEMORY_SET:
		return (old | operand) & mask;
	case TCTI_ATOMIC_MEMORY_SMAX:
		return old > operand ? old : operand;
	case TCTI_ATOMIC_MEMORY_SMIN:
		return old < operand ? old : operand;
	case TCTI_ATOMIC_MEMORY_UMAX:
		return old > operand ? old : operand;
	case TCTI_ATOMIC_MEMORY_UMIN:
		return old < operand ? old : operand;
	case TCTI_ATOMIC_MEMORY_CAS:
		return old;
	}

	return 0;
}

static void tcti_atomic_memory_cas_all_scalar_widths(struct kunit *test)
{
	unsigned long mapped = tcti_atomic_test_map(test, PROT_READ | PROT_WRITE);
	const size_t sizes[] = { sizeof(u8), sizeof(u16), sizeof(u32), sizeof(u64) };
	size_t i;

	for (i = 0; i < ARRAY_SIZE(sizes); i++) {
		u64 initial = 0x1122334455667788ULL & tcti_atomic_mask(sizes[i]);
		u64 replacement = 0x8877665544332211ULL & tcti_atomic_mask(sizes[i]);
		u64 expected = initial;
		u64 old = 0;
		u64 observed = 0;
		bool exchanged = false;
		int ret;

		ret = tcti_write_user_data(current->mm, mapped, &initial, sizes[i]);
		KUNIT_ASSERT_EQ(test, 0, ret);
		ret = tcti_atomic_user_data(current->mm, mapped,
				TCTI_ATOMIC_MEMORY_CAS, TCTI_ATOMIC_MEMORY_ACQ_REL,
				&expected, &replacement, &old, sizes[i], &exchanged);
		KUNIT_ASSERT_EQ(test, 0, ret);
		KUNIT_EXPECT_TRUE(test, exchanged);
		KUNIT_EXPECT_EQ(test, initial, old);

		expected = initial;
		old = 0;
		exchanged = true;
		ret = tcti_atomic_user_data(current->mm, mapped,
				TCTI_ATOMIC_MEMORY_CAS, TCTI_ATOMIC_MEMORY_RELAXED,
				&expected, &initial, &old, sizes[i], &exchanged);
		KUNIT_ASSERT_EQ(test, 0, ret);
		KUNIT_EXPECT_FALSE(test, exchanged);
		KUNIT_EXPECT_EQ(test, replacement, old);
		ret = tcti_read_user_data(current->mm, mapped, &observed, sizes[i]);
		KUNIT_ASSERT_EQ(test, 0, ret);
		KUNIT_EXPECT_EQ(test, replacement, observed);
	}

	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void tcti_atomic_memory_rmw_all_scalar_ops_and_widths(struct kunit *test)
{
	unsigned long mapped = tcti_atomic_test_map(test, PROT_READ | PROT_WRITE);
	const enum tcti_atomic_memory_operation operations[] = {
		TCTI_ATOMIC_MEMORY_SWP, TCTI_ATOMIC_MEMORY_ADD,
		TCTI_ATOMIC_MEMORY_CLR, TCTI_ATOMIC_MEMORY_EOR,
		TCTI_ATOMIC_MEMORY_SET, TCTI_ATOMIC_MEMORY_SMAX,
		TCTI_ATOMIC_MEMORY_SMIN, TCTI_ATOMIC_MEMORY_UMAX,
		TCTI_ATOMIC_MEMORY_UMIN,
	};
	const size_t sizes[] = { sizeof(u8), sizeof(u16), sizeof(u32), sizeof(u64) };
	size_t i, j;

	for (i = 0; i < ARRAY_SIZE(sizes); i++) {
		for (j = 0; j < ARRAY_SIZE(operations); j++) {
			u64 initial = 0x12 & tcti_atomic_mask(sizes[i]);
			u64 operand = 0x25 & tcti_atomic_mask(sizes[i]);
			u64 old = 0;
			u64 observed = 0;
			u64 expected = tcti_atomic_rmw_expected(operations[j], initial,
								 operand, sizes[i]);
			bool exchanged = false;
			int ret;

			ret = tcti_write_user_data(current->mm, mapped, &initial,
						   sizes[i]);
			KUNIT_ASSERT_EQ(test, 0, ret);
			ret = tcti_atomic_user_data(current->mm, mapped, operations[j],
					TCTI_ATOMIC_MEMORY_RELEASE, NULL, &operand, &old,
					sizes[i], &exchanged);
			KUNIT_ASSERT_EQ(test, 0, ret);
			KUNIT_EXPECT_TRUE(test, exchanged);
			KUNIT_EXPECT_EQ(test, initial, old);
			ret = tcti_read_user_data(current->mm, mapped, &observed,
						  sizes[i]);
			KUNIT_ASSERT_EQ(test, 0, ret);
			KUNIT_EXPECT_EQ(test, expected, observed);
		}
	}

	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void tcti_atomic_memory_signed_extrema_all_scalar_widths(struct kunit *test)
{
	unsigned long mapped = tcti_atomic_test_map(test, PROT_READ | PROT_WRITE);
	const size_t sizes[] = { sizeof(u8), sizeof(u16), sizeof(u32), sizeof(u64) };
	size_t i;

	for (i = 0; i < ARRAY_SIZE(sizes); i++) {
		u64 mask = tcti_atomic_mask(sizes[i]);
		u64 min = 1ULL << (sizes[i] * 8 - 1);
		u64 max = min - 1;
		u64 one = 1;
		u64 negative_one = mask;
		u64 old = 0;
		u64 observed = 0;
		bool exchanged = false;
		int ret;

		ret = tcti_write_user_data(current->mm, mapped, &min, sizes[i]);
		KUNIT_ASSERT_EQ(test, 0, ret);
		ret = tcti_atomic_user_data(current->mm, mapped,
				TCTI_ATOMIC_MEMORY_SMAX, TCTI_ATOMIC_MEMORY_RELAXED,
				NULL, &one, &old, sizes[i], &exchanged);
		KUNIT_ASSERT_EQ(test, 0, ret);
		KUNIT_EXPECT_EQ(test, min, old);
		ret = tcti_read_user_data(current->mm, mapped, &observed, sizes[i]);
		KUNIT_ASSERT_EQ(test, 0, ret);
		KUNIT_EXPECT_EQ(test, one, observed);

		ret = tcti_write_user_data(current->mm, mapped, &max, sizes[i]);
		KUNIT_ASSERT_EQ(test, 0, ret);
		ret = tcti_atomic_user_data(current->mm, mapped,
				TCTI_ATOMIC_MEMORY_SMIN, TCTI_ATOMIC_MEMORY_RELAXED,
				NULL, &negative_one, &old, sizes[i], &exchanged);
		KUNIT_ASSERT_EQ(test, 0, ret);
		KUNIT_EXPECT_EQ(test, max, old);
		ret = tcti_read_user_data(current->mm, mapped, &observed, sizes[i]);
		KUNIT_ASSERT_EQ(test, 0, ret);
		KUNIT_EXPECT_EQ(test, negative_one, observed);

		ret = tcti_write_user_data(current->mm, mapped, &min, sizes[i]);
		KUNIT_ASSERT_EQ(test, 0, ret);
		ret = tcti_atomic_user_data(current->mm, mapped,
				TCTI_ATOMIC_MEMORY_SMIN, TCTI_ATOMIC_MEMORY_RELAXED,
				NULL, &one, &old, sizes[i], &exchanged);
		KUNIT_ASSERT_EQ(test, 0, ret);
		ret = tcti_read_user_data(current->mm, mapped, &observed, sizes[i]);
		KUNIT_ASSERT_EQ(test, 0, ret);
		KUNIT_EXPECT_EQ(test, min, observed);

		ret = tcti_write_user_data(current->mm, mapped, &max, sizes[i]);
		KUNIT_ASSERT_EQ(test, 0, ret);
		ret = tcti_atomic_user_data(current->mm, mapped,
				TCTI_ATOMIC_MEMORY_SMAX, TCTI_ATOMIC_MEMORY_RELAXED,
				NULL, &negative_one, &old, sizes[i], &exchanged);
		KUNIT_ASSERT_EQ(test, 0, ret);
		ret = tcti_read_user_data(current->mm, mapped, &observed, sizes[i]);
		KUNIT_ASSERT_EQ(test, 0, ret);
		KUNIT_EXPECT_EQ(test, max, observed);
	}

	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

struct tcti_atomic_pair_worker {
	struct mm_struct *mm;
	unsigned long mapped;
	size_t size;
	const u8 *first;
	const u8 *second;
	struct completion *start;
	struct completion done;
	atomic_t *writers_running;
	int ret;
};

struct tcti_atomic_pair_reader {
	struct mm_struct *mm;
	unsigned long mapped;
	size_t size;
	const u8 *first;
	const u8 *second;
	struct completion *start;
	struct completion *ready;
	struct completion done;
	atomic_t *writers_running;
	int ret;
	bool torn;
};

static int tcti_atomic_pair_writer(void *data)
{
	struct tcti_atomic_pair_worker *worker = data;
	u8 old[16];
	unsigned int i;

	kthread_use_mm(worker->mm);
	wait_for_completion(worker->start);
	for (i = 0; i < TCTI_ATOMIC_PAIR_ITERATIONS; i++) {
		const u8 *expected = i & 1 ? worker->second : worker->first;
		const u8 *replacement = i & 1 ? worker->first : worker->second;
		bool exchanged = false;

		worker->ret = tcti_atomic_user_data(worker->mm, worker->mapped,
				TCTI_ATOMIC_MEMORY_CAS, TCTI_ATOMIC_MEMORY_ACQ_REL,
				expected, replacement, old, worker->size, &exchanged);
		if (worker->ret)
			break;
	}
	atomic_dec(worker->writers_running);
	kthread_unuse_mm(worker->mm);
	complete(&worker->done);
	return 0;
}

static int tcti_atomic_pair_reader(void *data)
{
	struct tcti_atomic_pair_reader *reader = data;
	u8 observed[16];

	kthread_use_mm(reader->mm);
	complete(reader->ready);
	wait_for_completion(reader->start);
	while (atomic_read(reader->writers_running)) {
		reader->ret = tcti_read_user_data(reader->mm, reader->mapped,
						  observed, reader->size);
		if (reader->ret ||
		    (memcmp(observed, reader->first, reader->size) &&
		     memcmp(observed, reader->second, reader->size))) {
			reader->torn = true;
			break;
		}
		cpu_relax();
	}
	kthread_unuse_mm(reader->mm);
	complete(&reader->done);
	return 0;
}

static void tcti_atomic_memory_casp_no_tear_under_coordinated_threads(
	struct kunit *test)
{
	unsigned long mapped = tcti_atomic_test_map(test, PROT_READ | PROT_WRITE);
	const size_t sizes[] = { sizeof(u64), 2 * sizeof(u64) };
	size_t i;

	for (i = 0; i < ARRAY_SIZE(sizes); i++) {
		u8 first[16];
		u8 second[16];
		struct completion start;
		struct completion reader_ready;
		struct tcti_atomic_pair_worker writers[2];
		struct tcti_atomic_pair_reader reader;
		struct task_struct *writer_tasks[2];
		struct task_struct *reader_task;
		atomic_t writers_running;
		unsigned int j;
		int ret;

		memset(first, 0x11, sizeof(first));
		memset(second, 0xaa, sizeof(second));
		ret = tcti_write_user_data(current->mm, mapped, first, sizes[i]);
		KUNIT_ASSERT_EQ(test, 0, ret);
		init_completion(&start);
		init_completion(&reader_ready);
		atomic_set(&writers_running, ARRAY_SIZE(writers));
		for (j = 0; j < ARRAY_SIZE(writers); j++) {
			writers[j] = (struct tcti_atomic_pair_worker) {
				.mm = current->mm, .mapped = mapped, .size = sizes[i],
				.first = first, .second = second, .start = &start,
				.writers_running = &writers_running,
			};
			init_completion(&writers[j].done);
			writer_tasks[j] = kthread_run(tcti_atomic_pair_writer,
						      &writers[j], "tcti-atomic-pair");
			KUNIT_ASSERT_FALSE(test, IS_ERR(writer_tasks[j]));
		}
		reader = (struct tcti_atomic_pair_reader) {
			.mm = current->mm, .mapped = mapped, .size = sizes[i],
			.first = first, .second = second, .start = &start,
			.ready = &reader_ready,
			.writers_running = &writers_running,
		};
		init_completion(&reader.done);
		reader_task = kthread_run(tcti_atomic_pair_reader, &reader,
						  "tcti-atomic-read");
		KUNIT_ASSERT_FALSE(test, IS_ERR(reader_task));
		KUNIT_ASSERT_NE(test, 0UL,
			wait_for_completion_timeout(&reader_ready, msecs_to_jiffies(5000)));
		complete_all(&start);
		for (j = 0; j < ARRAY_SIZE(writers); j++) {
			KUNIT_ASSERT_NE(test, 0UL,
				wait_for_completion_timeout(&writers[j].done,
					msecs_to_jiffies(5000)));
			KUNIT_EXPECT_EQ(test, 0, writers[j].ret);
		}
		KUNIT_ASSERT_NE(test, 0UL,
			wait_for_completion_timeout(&reader.done, msecs_to_jiffies(5000)));
		KUNIT_EXPECT_EQ(test, 0, reader.ret);
		KUNIT_EXPECT_FALSE(test, reader.torn);
	}

	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void tcti_atomic_memory_validates_inputs_and_permissions(
	struct kunit *test)
{
	unsigned long mapped = tcti_atomic_test_map(test, PROT_READ | PROT_WRITE);
	unsigned long readonly = tcti_atomic_test_map(test, PROT_READ);
	unsigned long inaccessible = tcti_atomic_test_map(test, PROT_NONE);
	u32 value = 1;
	u32 old = 0;
	bool exchanged = false;
	int ret;

	ret = tcti_atomic_user_data(current->mm, mapped + 1,
			TCTI_ATOMIC_MEMORY_ADD, TCTI_ATOMIC_MEMORY_RELAXED, NULL,
			&value, &old, sizeof(value), &exchanged);
	KUNIT_EXPECT_EQ(test, -EFAULT, ret);
	ret = tcti_atomic_user_data(current->mm, inaccessible,
			TCTI_ATOMIC_MEMORY_ADD, TCTI_ATOMIC_MEMORY_ACQUIRE, NULL,
			&value, &old, sizeof(value), &exchanged);
	KUNIT_EXPECT_EQ(test, -EACCES, ret);
	ret = tcti_atomic_user_data(current->mm, readonly,
			TCTI_ATOMIC_MEMORY_ADD, TCTI_ATOMIC_MEMORY_ACQUIRE, NULL,
			&value, &old, sizeof(value), &exchanged);
	KUNIT_EXPECT_EQ(test, -EACCES, ret);
	ret = tcti_atomic_user_data(current->mm, mapped,
			TCTI_ATOMIC_MEMORY_ADD,
			(enum tcti_atomic_memory_order)-1, NULL, &value, &old,
			sizeof(value), &exchanged);
	KUNIT_EXPECT_EQ(test, -EINVAL, ret);
	ret = tcti_atomic_user_data(current->mm, mapped,
			TCTI_ATOMIC_MEMORY_ADD, TCTI_ATOMIC_MEMORY_RELAXED, NULL,
			&value, &old, 3, &exchanged);
	KUNIT_EXPECT_EQ(test, -EINVAL, ret);
	ret = tcti_atomic_user_data(current->mm, mapped,
			TCTI_ATOMIC_MEMORY_CAS, TCTI_ATOMIC_MEMORY_RELAXED, NULL,
			&value, &old, sizeof(value), &exchanged);
	KUNIT_EXPECT_EQ(test, -EINVAL, ret);
	ret = tcti_atomic_user_data(NULL, mapped,
			TCTI_ATOMIC_MEMORY_ADD, TCTI_ATOMIC_MEMORY_RELAXED, NULL,
			&value, &old, sizeof(value), &exchanged);
	KUNIT_EXPECT_EQ(test, -EINVAL, ret);
	ret = tcti_atomic_user_data(current->mm, TASK_SIZE - sizeof(value),
			TCTI_ATOMIC_MEMORY_ADD, TCTI_ATOMIC_MEMORY_RELAXED, NULL,
			&value, &old, 2 * sizeof(value), &exchanged);
	KUNIT_EXPECT_EQ(test, -EFAULT, ret);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(inaccessible, PAGE_SIZE));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(readonly, PAGE_SIZE));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void tcti_exclusive_reservation_rejects_same_value_writes(
	struct kunit *test)
{
	unsigned long mapped = tcti_atomic_test_map(test, PROT_READ | PROT_WRITE);
	unsigned long pfn;
	u64 generation;
	u64 mapping_generation;
	u64 initial = 0x1122334455667788ULL;
	u64 desired = 0x8877665544332211ULL;
	u64 old;
	bool exchanged;
	bool stored;
	int ret;

	ret = tcti_write_user_data(current->mm, mapped, &initial, sizeof(initial));
	KUNIT_ASSERT_EQ(test, 0, ret);
	ret = tcti_load_exclusive_user_data(current->mm, mapped, &old,
					    sizeof(old), &pfn, &generation,
					    &mapping_generation);
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, initial, old);
	ret = tcti_write_user_data(current->mm, mapped, &initial, sizeof(initial));
	KUNIT_ASSERT_EQ(test, 0, ret);
	ret = tcti_store_exclusive_user_data(current->mm, mapped, &desired,
					     sizeof(desired), pfn, generation,
					     mapping_generation, &stored);
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_FALSE(test, stored);

	ret = tcti_load_exclusive_user_data(current->mm, mapped, &old,
					    sizeof(old), &pfn, &generation,
					    &mapping_generation);
	KUNIT_ASSERT_EQ(test, 0, ret);
	exchanged = false;
	ret = tcti_atomic_user_data(current->mm, mapped,
				    TCTI_ATOMIC_MEMORY_SWP,
				    TCTI_ATOMIC_MEMORY_RELAXED, NULL, &initial,
				    &old, sizeof(initial), &exchanged);
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_ASSERT_TRUE(test, exchanged);
	ret = tcti_store_exclusive_user_data(current->mm, mapped, &desired,
					     sizeof(desired), pfn, generation,
					     mapping_generation, &stored);
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_FALSE(test, stored);

	ret = tcti_load_exclusive_user_data(current->mm, mapped, &old,
					    sizeof(old), &pfn, &generation,
					    &mapping_generation);
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_ASSERT_EQ(test, 0UL,
			      raw_copy_to_user((void __user *)mapped, &initial,
					       sizeof(initial)));
	ret = tcti_store_exclusive_user_data(current->mm, mapped, &desired,
					     sizeof(desired), pfn, generation,
					     mapping_generation, &stored);
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_FALSE(test, stored);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void tcti_exclusive_reservation_rejects_restored_mapping_generation(
	struct kunit *test)
{
	unsigned long mapped = tcti_atomic_test_map(test, PROT_READ | PROT_WRITE);
	unsigned long reserved_pfn;
	unsigned long current_pfn;
	u64 reserved_generation;
	u64 current_generation;
	u64 reserved_mapping_generation;
	u64 current_mapping_generation;
	u64 initial = 0x1122334455667788ULL;
	u64 desired = 0x8877665544332211ULL;
	u64 observed = 0;
	bool stored;
	int ret;

	ret = tcti_write_user_data(current->mm, mapped, &initial,
				   sizeof(initial));
	KUNIT_ASSERT_EQ(test, 0, ret);
	ret = tcti_load_exclusive_user_data(
		current->mm, mapped, &observed, sizeof(observed), &reserved_pfn,
		&reserved_generation, &reserved_mapping_generation);
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_ASSERT_EQ(test, initial, observed);

	/*
	 * Model a completed PTE mutation interval whose replacement resolves to
	 * the same PFN. No memory write occurs, so the reservation epoch must
	 * remain unchanged while the per-mm mapping generation advances.
	 */
	tcti_mapping_sequence_begin(current->mm);
	tcti_mapping_sequence_end(current->mm);

	ret = tcti_load_exclusive_user_data(
		current->mm, mapped, &observed, sizeof(observed), &current_pfn,
		&current_generation, &current_mapping_generation);
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_ASSERT_EQ(test, reserved_pfn, current_pfn);
	KUNIT_ASSERT_EQ(test, reserved_generation, current_generation);
	KUNIT_ASSERT_NE(test, reserved_mapping_generation,
			current_mapping_generation);

	ret = tcti_store_exclusive_user_data(
		current->mm, mapped, &desired, sizeof(desired), reserved_pfn,
		reserved_generation, reserved_mapping_generation, &stored);
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_FALSE(test, stored);
	ret = tcti_read_user_data(current->mm, mapped, &observed,
				  sizeof(observed));
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, initial, observed);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void tcti_start_thread_clears_exclusive_reservation_state(
	struct kunit *test)
{
	struct pt_regs regs = {};

	current->thread.user_exclusive_address = 0x1000;
	current->thread.user_exclusive_value = 0x101;
	current->thread.user_exclusive_value2 = 0x202;
	current->thread.user_exclusive_pfn = 0x303;
	current->thread.user_exclusive_generation = 0x404;
	current->thread.user_exclusive_mapping_generation = 0x505;
	current->thread.user_exclusive_size = sizeof(u64);
	current->thread.user_exclusive_valid = 1;

	start_thread(&regs, 0x1000, 0x2000);

	KUNIT_EXPECT_EQ(test, 0UL, current->thread.user_exclusive_address);
	KUNIT_EXPECT_EQ(test, 0UL, current->thread.user_exclusive_value);
	KUNIT_EXPECT_EQ(test, 0UL, current->thread.user_exclusive_value2);
	KUNIT_EXPECT_EQ(test, 0UL, current->thread.user_exclusive_pfn);
	KUNIT_EXPECT_EQ(test, 0ULL,
			current->thread.user_exclusive_generation);
	KUNIT_EXPECT_EQ(test, 0ULL,
			current->thread.user_exclusive_mapping_generation);
	KUNIT_EXPECT_EQ(test, 0, current->thread.user_exclusive_size);
	KUNIT_EXPECT_EQ(test, 0, current->thread.user_exclusive_valid);
}

static void tcti_signal_delivery_clears_exclusive_reservation_state(
	struct kunit *test)
{
	current->thread.user_exclusive_address = 0x1000;
	current->thread.user_exclusive_value = 0x101;
	current->thread.user_exclusive_value2 = 0x202;
	current->thread.user_exclusive_pfn = 0x303;
	current->thread.user_exclusive_generation = 0x404;
	current->thread.user_exclusive_mapping_generation = 0x505;
	current->thread.user_exclusive_size = sizeof(u64);
	current->thread.user_exclusive_valid = 1;

	tcti_prepare_signal_delivery();

	KUNIT_EXPECT_EQ(test, 0UL, current->thread.user_exclusive_address);
	KUNIT_EXPECT_EQ(test, 0UL, current->thread.user_exclusive_value);
	KUNIT_EXPECT_EQ(test, 0UL, current->thread.user_exclusive_value2);
	KUNIT_EXPECT_EQ(test, 0UL, current->thread.user_exclusive_pfn);
	KUNIT_EXPECT_EQ(test, 0ULL,
			current->thread.user_exclusive_generation);
	KUNIT_EXPECT_EQ(test, 0ULL,
			current->thread.user_exclusive_mapping_generation);
	KUNIT_EXPECT_EQ(test, 0, current->thread.user_exclusive_size);
	KUNIT_EXPECT_EQ(test, 0, current->thread.user_exclusive_valid);
}

static struct kunit_case tcti_atomic_memory_test_cases[] = {
	KUNIT_CASE(tcti_atomic_memory_cas_all_scalar_widths),
	KUNIT_CASE(tcti_atomic_memory_rmw_all_scalar_ops_and_widths),
	KUNIT_CASE(tcti_atomic_memory_signed_extrema_all_scalar_widths),
	KUNIT_CASE(tcti_atomic_memory_casp_no_tear_under_coordinated_threads),
	KUNIT_CASE(tcti_atomic_memory_validates_inputs_and_permissions),
	KUNIT_CASE(tcti_exclusive_reservation_rejects_same_value_writes),
	KUNIT_CASE(tcti_start_thread_clears_exclusive_reservation_state),
	KUNIT_CASE(tcti_signal_delivery_clears_exclusive_reservation_state),
	KUNIT_CASE(
		tcti_exclusive_reservation_rejects_restored_mapping_generation),
	{}
};

struct kunit_suite tcti_atomic_memory_test_suite = {
	.name = "orlix-tcti-atomic-memory",
	.test_cases = tcti_atomic_memory_test_cases,
};
kunit_test_suite(tcti_atomic_memory_test_suite);
