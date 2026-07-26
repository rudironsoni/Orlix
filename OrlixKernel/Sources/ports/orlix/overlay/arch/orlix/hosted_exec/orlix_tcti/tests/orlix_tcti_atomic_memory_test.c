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
#include <asm/orlix_tcti.h>
#include <asm/uaccess.h>

#include "../decode_aarch64.h"
#include "../switch_debug.h"
#include "orlix_tcti_test_suites.h"

#define ORLIX_TCTI_ATOMIC_PAIR_ITERATIONS 1024
#define ORLIX_TCTI_EXCLUSIVE_BASE 0x08000000U

static u32 orlix_tcti_exclusive_instruction(u8 size, bool load, bool acquire_release,
				     u8 rs, u8 rn, u8 rt)
{
	return ORLIX_TCTI_EXCLUSIVE_BASE | ((u32)size << 30) |
		((u32)load << 22) | ((u32)rs << 16) |
		((u32)acquire_release << 15) | (31U << 10) |
		((u32)rn << 5) | rt;
}

static int orlix_tcti_atomic_memory_test_init(struct kunit *test)
{
	struct mm_struct *mm;

	test->priv = NULL;
	mm = mm_alloc();
	if (!mm)
		return -ENOMEM;
	kthread_use_mm(mm);
	test->priv = mm;
	return 0;
}

static void orlix_tcti_atomic_memory_test_exit(struct kunit *test)
{
	struct mm_struct *mm = test->priv;

	if (!mm)
		return;
	kthread_unuse_mm(mm);
	mmput(mm);
}

static unsigned long orlix_tcti_atomic_test_map(struct kunit *test, int prot)
{
	unsigned long mapped;

	mapped = ksys_mmap_pgoff(0, PAGE_SIZE, prot,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (IS_ERR_VALUE(mapped)) {
		KUNIT_FAIL(test, "could not map atomic test memory: %ld",
			   (long)mapped);
		return 0;
	}
	return mapped;
}

static u64 orlix_tcti_atomic_mask(size_t size)
{
	return size == sizeof(u64) ? U64_MAX : (1ULL << (size * 8)) - 1;
}

static u64 orlix_tcti_atomic_rmw_expected(enum orlix_tcti_atomic_memory_operation operation,
					    u64 old, u64 operand, size_t size)
{
	u64 mask = orlix_tcti_atomic_mask(size);

	switch (operation) {
	case ORLIX_TCTI_ATOMIC_MEMORY_SWP:
		return operand & mask;
	case ORLIX_TCTI_ATOMIC_MEMORY_ADD:
		return (old + operand) & mask;
	case ORLIX_TCTI_ATOMIC_MEMORY_CLR:
		return old & ~operand & mask;
	case ORLIX_TCTI_ATOMIC_MEMORY_EOR:
		return (old ^ operand) & mask;
	case ORLIX_TCTI_ATOMIC_MEMORY_SET:
		return (old | operand) & mask;
	case ORLIX_TCTI_ATOMIC_MEMORY_SMAX:
		return old > operand ? old : operand;
	case ORLIX_TCTI_ATOMIC_MEMORY_SMIN:
		return old < operand ? old : operand;
	case ORLIX_TCTI_ATOMIC_MEMORY_UMAX:
		return old > operand ? old : operand;
	case ORLIX_TCTI_ATOMIC_MEMORY_UMIN:
		return old < operand ? old : operand;
	case ORLIX_TCTI_ATOMIC_MEMORY_CAS:
		return old;
	}

	return 0;
}

static void orlix_tcti_atomic_memory_cas_all_scalar_widths(struct kunit *test)
{
	unsigned long mapped = orlix_tcti_atomic_test_map(test, PROT_READ | PROT_WRITE);
	const size_t sizes[] = { sizeof(u8), sizeof(u16), sizeof(u32), sizeof(u64) };
	size_t i;

	if (!mapped)
		return;
	for (i = 0; i < ARRAY_SIZE(sizes); i++) {
		u64 initial = 0x1122334455667788ULL & orlix_tcti_atomic_mask(sizes[i]);
		u64 replacement = 0x8877665544332211ULL & orlix_tcti_atomic_mask(sizes[i]);
		u64 expected = initial;
		u64 old = 0;
		u64 observed = 0;
		bool exchanged = false;
		int ret;

		ret = orlix_tcti_write_user_data(current->mm, mapped, &initial, sizes[i]);
		KUNIT_ASSERT_EQ(test, 0, ret);
		ret = orlix_tcti_atomic_user_data(current->mm, mapped,
				ORLIX_TCTI_ATOMIC_MEMORY_CAS, ORLIX_TCTI_ATOMIC_MEMORY_ACQ_REL,
				&expected, &replacement, &old, sizes[i], &exchanged);
		KUNIT_ASSERT_EQ(test, 0, ret);
		KUNIT_EXPECT_TRUE(test, exchanged);
		KUNIT_EXPECT_EQ(test, initial, old);

		expected = initial;
		old = 0;
		exchanged = true;
		ret = orlix_tcti_atomic_user_data(current->mm, mapped,
				ORLIX_TCTI_ATOMIC_MEMORY_CAS, ORLIX_TCTI_ATOMIC_MEMORY_RELAXED,
				&expected, &initial, &old, sizes[i], &exchanged);
		KUNIT_ASSERT_EQ(test, 0, ret);
		KUNIT_EXPECT_FALSE(test, exchanged);
		KUNIT_EXPECT_EQ(test, replacement, old);
		ret = orlix_tcti_read_user_data(current->mm, mapped, &observed, sizes[i]);
		KUNIT_ASSERT_EQ(test, 0, ret);
		KUNIT_EXPECT_EQ(test, replacement, observed);
	}

	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void orlix_tcti_atomic_memory_rmw_all_scalar_ops_and_widths(struct kunit *test)
{
	unsigned long mapped = orlix_tcti_atomic_test_map(test, PROT_READ | PROT_WRITE);
	const enum orlix_tcti_atomic_memory_operation operations[] = {
		ORLIX_TCTI_ATOMIC_MEMORY_SWP, ORLIX_TCTI_ATOMIC_MEMORY_ADD,
		ORLIX_TCTI_ATOMIC_MEMORY_CLR, ORLIX_TCTI_ATOMIC_MEMORY_EOR,
		ORLIX_TCTI_ATOMIC_MEMORY_SET, ORLIX_TCTI_ATOMIC_MEMORY_SMAX,
		ORLIX_TCTI_ATOMIC_MEMORY_SMIN, ORLIX_TCTI_ATOMIC_MEMORY_UMAX,
		ORLIX_TCTI_ATOMIC_MEMORY_UMIN,
	};
	const size_t sizes[] = { sizeof(u8), sizeof(u16), sizeof(u32), sizeof(u64) };
	size_t i, j;

	if (!mapped)
		return;
	for (i = 0; i < ARRAY_SIZE(sizes); i++) {
		for (j = 0; j < ARRAY_SIZE(operations); j++) {
			u64 initial = 0x12 & orlix_tcti_atomic_mask(sizes[i]);
			u64 operand = 0x25 & orlix_tcti_atomic_mask(sizes[i]);
			u64 old = 0;
			u64 observed = 0;
			u64 expected = orlix_tcti_atomic_rmw_expected(operations[j], initial,
								 operand, sizes[i]);
			bool exchanged = false;
			int ret;

			ret = orlix_tcti_write_user_data(current->mm, mapped, &initial,
						   sizes[i]);
			KUNIT_ASSERT_EQ(test, 0, ret);
			ret = orlix_tcti_atomic_user_data(current->mm, mapped, operations[j],
					ORLIX_TCTI_ATOMIC_MEMORY_RELEASE, NULL, &operand, &old,
					sizes[i], &exchanged);
			KUNIT_ASSERT_EQ(test, 0, ret);
			KUNIT_EXPECT_TRUE(test, exchanged);
			KUNIT_EXPECT_EQ(test, initial, old);
			ret = orlix_tcti_read_user_data(current->mm, mapped, &observed,
						  sizes[i]);
			KUNIT_ASSERT_EQ(test, 0, ret);
			KUNIT_EXPECT_EQ(test, expected, observed);
		}
	}

	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void orlix_tcti_atomic_memory_signed_extrema_all_scalar_widths(struct kunit *test)
{
	unsigned long mapped = orlix_tcti_atomic_test_map(test, PROT_READ | PROT_WRITE);
	const size_t sizes[] = { sizeof(u8), sizeof(u16), sizeof(u32), sizeof(u64) };
	size_t i;

	if (!mapped)
		return;
	for (i = 0; i < ARRAY_SIZE(sizes); i++) {
		u64 mask = orlix_tcti_atomic_mask(sizes[i]);
		u64 min = 1ULL << (sizes[i] * 8 - 1);
		u64 max = min - 1;
		u64 one = 1;
		u64 negative_one = mask;
		u64 old = 0;
		u64 observed = 0;
		bool exchanged = false;
		int ret;

		ret = orlix_tcti_write_user_data(current->mm, mapped, &min, sizes[i]);
		KUNIT_ASSERT_EQ(test, 0, ret);
		ret = orlix_tcti_atomic_user_data(current->mm, mapped,
				ORLIX_TCTI_ATOMIC_MEMORY_SMAX, ORLIX_TCTI_ATOMIC_MEMORY_RELAXED,
				NULL, &one, &old, sizes[i], &exchanged);
		KUNIT_ASSERT_EQ(test, 0, ret);
		KUNIT_EXPECT_EQ(test, min, old);
		ret = orlix_tcti_read_user_data(current->mm, mapped, &observed, sizes[i]);
		KUNIT_ASSERT_EQ(test, 0, ret);
		KUNIT_EXPECT_EQ(test, one, observed);

		ret = orlix_tcti_write_user_data(current->mm, mapped, &max, sizes[i]);
		KUNIT_ASSERT_EQ(test, 0, ret);
		ret = orlix_tcti_atomic_user_data(current->mm, mapped,
				ORLIX_TCTI_ATOMIC_MEMORY_SMIN, ORLIX_TCTI_ATOMIC_MEMORY_RELAXED,
				NULL, &negative_one, &old, sizes[i], &exchanged);
		KUNIT_ASSERT_EQ(test, 0, ret);
		KUNIT_EXPECT_EQ(test, max, old);
		ret = orlix_tcti_read_user_data(current->mm, mapped, &observed, sizes[i]);
		KUNIT_ASSERT_EQ(test, 0, ret);
		KUNIT_EXPECT_EQ(test, negative_one, observed);

		ret = orlix_tcti_write_user_data(current->mm, mapped, &min, sizes[i]);
		KUNIT_ASSERT_EQ(test, 0, ret);
		ret = orlix_tcti_atomic_user_data(current->mm, mapped,
				ORLIX_TCTI_ATOMIC_MEMORY_SMIN, ORLIX_TCTI_ATOMIC_MEMORY_RELAXED,
				NULL, &one, &old, sizes[i], &exchanged);
		KUNIT_ASSERT_EQ(test, 0, ret);
		ret = orlix_tcti_read_user_data(current->mm, mapped, &observed, sizes[i]);
		KUNIT_ASSERT_EQ(test, 0, ret);
		KUNIT_EXPECT_EQ(test, min, observed);

		ret = orlix_tcti_write_user_data(current->mm, mapped, &max, sizes[i]);
		KUNIT_ASSERT_EQ(test, 0, ret);
		ret = orlix_tcti_atomic_user_data(current->mm, mapped,
				ORLIX_TCTI_ATOMIC_MEMORY_SMAX, ORLIX_TCTI_ATOMIC_MEMORY_RELAXED,
				NULL, &negative_one, &old, sizes[i], &exchanged);
		KUNIT_ASSERT_EQ(test, 0, ret);
		ret = orlix_tcti_read_user_data(current->mm, mapped, &observed, sizes[i]);
		KUNIT_ASSERT_EQ(test, 0, ret);
		KUNIT_EXPECT_EQ(test, max, observed);
	}

	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

struct orlix_tcti_atomic_pair_worker {
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

struct orlix_tcti_atomic_pair_reader {
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

static void orlix_tcti_atomic_pair_wait_for_stop(void)
{
	for (;;) {
		set_current_state(TASK_INTERRUPTIBLE);
		if (kthread_should_stop())
			break;
		schedule();
	}
	__set_current_state(TASK_RUNNING);
}

static int orlix_tcti_atomic_pair_writer(void *data)
{
	struct orlix_tcti_atomic_pair_worker *worker = data;
	u8 old[16];
	unsigned int i;

	kthread_use_mm(worker->mm);
	wait_for_completion(worker->start);
	for (i = 0; i < ORLIX_TCTI_ATOMIC_PAIR_ITERATIONS; i++) {
		const u8 *expected = i & 1 ? worker->second : worker->first;
		const u8 *replacement = i & 1 ? worker->first : worker->second;
		bool exchanged = false;

		if (kthread_should_stop())
			break;
		worker->ret = orlix_tcti_atomic_user_data(worker->mm, worker->mapped,
				ORLIX_TCTI_ATOMIC_MEMORY_CAS, ORLIX_TCTI_ATOMIC_MEMORY_ACQ_REL,
				expected, replacement, old, worker->size, &exchanged);
		if (worker->ret)
			break;
		cond_resched();
	}
	atomic_dec(worker->writers_running);
	complete(&worker->done);
	orlix_tcti_atomic_pair_wait_for_stop();
	kthread_unuse_mm(worker->mm);
	return 0;
}

static int orlix_tcti_atomic_pair_reader(void *data)
{
	struct orlix_tcti_atomic_pair_reader *reader = data;
	u8 observed[16];

	kthread_use_mm(reader->mm);
	complete(reader->ready);
	wait_for_completion(reader->start);
	while (atomic_read(reader->writers_running)) {
		if (kthread_should_stop())
			break;
		reader->ret = orlix_tcti_read_user_data(reader->mm, reader->mapped,
						  observed, reader->size);
		if (reader->ret ||
		    (memcmp(observed, reader->first, reader->size) &&
		     memcmp(observed, reader->second, reader->size))) {
			reader->torn = true;
			break;
		}
		cpu_relax();
		cond_resched();
	}
	complete(&reader->done);
	orlix_tcti_atomic_pair_wait_for_stop();
	kthread_unuse_mm(reader->mm);
	return 0;
}

static void orlix_tcti_atomic_memory_casp_no_tear_under_coordinated_threads(
	struct kunit *test)
{
	unsigned long mapped = orlix_tcti_atomic_test_map(test, PROT_READ | PROT_WRITE);
	const size_t sizes[] = { sizeof(u64), 2 * sizeof(u64) };
	size_t i;

	if (!mapped)
		return;
	for (i = 0; i < ARRAY_SIZE(sizes); i++) {
		u8 first[16];
		u8 second[16];
		struct completion start;
		struct completion reader_ready;
		struct orlix_tcti_atomic_pair_worker writers[2];
		struct orlix_tcti_atomic_pair_reader reader;
		struct task_struct *writer_tasks[2] = {};
		struct task_struct *reader_task = NULL;
		atomic_t writers_running;
		unsigned int j;
		unsigned int writer_count = 0;
		bool exercise_completed = false;
		int ret;

		memset(first, 0x11, sizeof(first));
		memset(second, 0xaa, sizeof(second));
		init_completion(&start);
		init_completion(&reader_ready);
		atomic_set(&writers_running, ARRAY_SIZE(writers));
		ret = orlix_tcti_write_user_data(current->mm, mapped, first, sizes[i]);
		if (ret) {
			KUNIT_FAIL(test, "could not initialize CASP test memory: %d",
				   ret);
			goto out_unmap;
		}
		for (j = 0; j < ARRAY_SIZE(writers); j++) {
			writers[j] = (struct orlix_tcti_atomic_pair_worker) {
				.mm = current->mm, .mapped = mapped, .size = sizes[i],
				.first = first, .second = second, .start = &start,
				.writers_running = &writers_running,
			};
			init_completion(&writers[j].done);
			writer_tasks[j] = kthread_run(orlix_tcti_atomic_pair_writer,
						      &writers[j], "orlix-tcti-atomic-pair");
			if (IS_ERR_OR_NULL(writer_tasks[j])) {
				KUNIT_FAIL(test, "could not create CASP writer %u", j);
				writer_tasks[j] = NULL;
				goto out_stop_threads;
			}
			writer_count++;
		}
		reader = (struct orlix_tcti_atomic_pair_reader) {
			.mm = current->mm, .mapped = mapped, .size = sizes[i],
			.first = first, .second = second, .start = &start,
			.ready = &reader_ready,
			.writers_running = &writers_running,
		};
		init_completion(&reader.done);
		reader_task = kthread_run(orlix_tcti_atomic_pair_reader, &reader,
					  "orlix-tcti-atomic-read");
		if (IS_ERR_OR_NULL(reader_task)) {
			KUNIT_FAIL(test, "could not create CASP reader");
			reader_task = NULL;
			goto out_stop_threads;
		}
		if (!wait_for_completion_timeout(&reader_ready,
					 msecs_to_jiffies(5000))) {
			KUNIT_FAIL(test, "CASP reader did not become ready");
			goto out_stop_threads;
		}
		complete_all(&start);
		for (j = 0; j < ARRAY_SIZE(writers); j++) {
			if (!wait_for_completion_timeout(&writers[j].done,
						 msecs_to_jiffies(5000))) {
				KUNIT_FAIL(test, "CASP writer %u timed out", j);
				goto out_stop_threads;
			}
		}
		if (!wait_for_completion_timeout(&reader.done,
					 msecs_to_jiffies(5000))) {
			KUNIT_FAIL(test, "CASP reader timed out");
			goto out_stop_threads;
		}
		exercise_completed = true;

out_stop_threads:
		/* Every created task retains this function's stack and current->mm. */
		complete_all(&start);
		if (reader_task)
			kthread_stop(reader_task);
		for (j = 0; j < writer_count; j++)
			kthread_stop(writer_tasks[j]);
		if (!exercise_completed)
			goto out_unmap;

		for (j = 0; j < ARRAY_SIZE(writers); j++)
			KUNIT_EXPECT_EQ(test, 0, writers[j].ret);
		KUNIT_EXPECT_EQ(test, 0, reader.ret);
		KUNIT_EXPECT_FALSE(test, reader.torn);
	}

out_unmap:
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void orlix_tcti_atomic_memory_validates_inputs_and_permissions(
	struct kunit *test)
{
	unsigned long mapped;
	unsigned long readonly;
	unsigned long inaccessible;
	u32 value = 1;
	u32 old = 0;
	bool exchanged = false;
	int ret;

	mapped = orlix_tcti_atomic_test_map(test, PROT_READ | PROT_WRITE);
	if (!mapped)
		return;
	readonly = orlix_tcti_atomic_test_map(test, PROT_READ);
	if (!readonly)
		goto out_mapped;
	inaccessible = orlix_tcti_atomic_test_map(test, PROT_NONE);
	if (!inaccessible)
		goto out_readonly;
	ret = orlix_tcti_atomic_user_data(current->mm, mapped + 1,
			ORLIX_TCTI_ATOMIC_MEMORY_ADD, ORLIX_TCTI_ATOMIC_MEMORY_RELAXED, NULL,
			&value, &old, sizeof(value), &exchanged);
	KUNIT_EXPECT_EQ(test, -EFAULT, ret);
	ret = orlix_tcti_atomic_user_data(current->mm, inaccessible,
			ORLIX_TCTI_ATOMIC_MEMORY_ADD, ORLIX_TCTI_ATOMIC_MEMORY_ACQUIRE, NULL,
			&value, &old, sizeof(value), &exchanged);
	KUNIT_EXPECT_EQ(test, -EACCES, ret);
	ret = orlix_tcti_atomic_user_data(current->mm, readonly,
			ORLIX_TCTI_ATOMIC_MEMORY_ADD, ORLIX_TCTI_ATOMIC_MEMORY_ACQUIRE, NULL,
			&value, &old, sizeof(value), &exchanged);
	KUNIT_EXPECT_EQ(test, -EACCES, ret);
	ret = orlix_tcti_atomic_user_data(current->mm, mapped,
			ORLIX_TCTI_ATOMIC_MEMORY_ADD,
			(enum orlix_tcti_atomic_memory_order)-1, NULL, &value, &old,
			sizeof(value), &exchanged);
	KUNIT_EXPECT_EQ(test, -EINVAL, ret);
	ret = orlix_tcti_atomic_user_data(current->mm, mapped,
			ORLIX_TCTI_ATOMIC_MEMORY_ADD, ORLIX_TCTI_ATOMIC_MEMORY_RELAXED, NULL,
			&value, &old, 3, &exchanged);
	KUNIT_EXPECT_EQ(test, -EINVAL, ret);
	ret = orlix_tcti_atomic_user_data(current->mm, mapped,
			ORLIX_TCTI_ATOMIC_MEMORY_CAS, ORLIX_TCTI_ATOMIC_MEMORY_RELAXED, NULL,
			&value, &old, sizeof(value), &exchanged);
	KUNIT_EXPECT_EQ(test, -EINVAL, ret);
	ret = orlix_tcti_atomic_user_data(NULL, mapped,
			ORLIX_TCTI_ATOMIC_MEMORY_ADD, ORLIX_TCTI_ATOMIC_MEMORY_RELAXED, NULL,
			&value, &old, sizeof(value), &exchanged);
	KUNIT_EXPECT_EQ(test, -EINVAL, ret);
	ret = orlix_tcti_atomic_user_data(current->mm, TASK_SIZE - sizeof(value),
			ORLIX_TCTI_ATOMIC_MEMORY_ADD, ORLIX_TCTI_ATOMIC_MEMORY_RELAXED, NULL,
			&value, &old, 2 * sizeof(value), &exchanged);
	KUNIT_EXPECT_EQ(test, -EFAULT, ret);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(inaccessible, PAGE_SIZE));
out_readonly:
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(readonly, PAGE_SIZE));
out_mapped:
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void orlix_tcti_exclusive_reservation_rejects_same_value_writes(
	struct kunit *test)
{
	unsigned long mapped = orlix_tcti_atomic_test_map(test, PROT_READ | PROT_WRITE);
	unsigned long pfn;
	u64 generation;
	u64 mapping_generation;
	u64 initial = 0x1122334455667788ULL;
	u64 desired = 0x8877665544332211ULL;
	u64 old;
	bool exchanged;
	bool stored;
	int ret;

	if (!mapped)
		return;
	ret = orlix_tcti_write_user_data(current->mm, mapped, &initial, sizeof(initial));
	KUNIT_ASSERT_EQ(test, 0, ret);
	ret = orlix_tcti_load_exclusive_user_data(current->mm, mapped, &old,
					    sizeof(old), &pfn, &generation,
					    &mapping_generation);
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, initial, old);
	ret = orlix_tcti_write_user_data(current->mm, mapped, &initial, sizeof(initial));
	KUNIT_ASSERT_EQ(test, 0, ret);
	ret = orlix_tcti_store_exclusive_user_data(current->mm, mapped, &desired,
					     sizeof(desired), pfn, generation,
					     mapping_generation, &stored);
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_FALSE(test, stored);

	ret = orlix_tcti_load_exclusive_user_data(current->mm, mapped, &old,
					    sizeof(old), &pfn, &generation,
					    &mapping_generation);
	KUNIT_ASSERT_EQ(test, 0, ret);
	exchanged = false;
	ret = orlix_tcti_atomic_user_data(current->mm, mapped,
				    ORLIX_TCTI_ATOMIC_MEMORY_SWP,
				    ORLIX_TCTI_ATOMIC_MEMORY_RELAXED, NULL, &initial,
				    &old, sizeof(initial), &exchanged);
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_ASSERT_TRUE(test, exchanged);
	ret = orlix_tcti_store_exclusive_user_data(current->mm, mapped, &desired,
					     sizeof(desired), pfn, generation,
					     mapping_generation, &stored);
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_FALSE(test, stored);

	ret = orlix_tcti_load_exclusive_user_data(current->mm, mapped, &old,
					    sizeof(old), &pfn, &generation,
					    &mapping_generation);
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_ASSERT_EQ(test, 0UL,
			      raw_copy_to_user((void __user *)mapped, &initial,
					       sizeof(initial)));
	ret = orlix_tcti_store_exclusive_user_data(current->mm, mapped, &desired,
					     sizeof(desired), pfn, generation,
					     mapping_generation, &stored);
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_FALSE(test, stored);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void orlix_tcti_exclusive_reservation_rejects_restored_mapping_generation(
	struct kunit *test)
{
	unsigned long mapped = orlix_tcti_atomic_test_map(test, PROT_READ | PROT_WRITE);
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

	if (!mapped)
		return;
	ret = orlix_tcti_write_user_data(current->mm, mapped, &initial,
				   sizeof(initial));
	KUNIT_ASSERT_EQ(test, 0, ret);
	ret = orlix_tcti_load_exclusive_user_data(
		current->mm, mapped, &observed, sizeof(observed), &reserved_pfn,
		&reserved_generation, &reserved_mapping_generation);
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_ASSERT_EQ(test, initial, observed);

	/*
	 * Model a completed PTE mutation interval whose replacement resolves to
	 * the same PFN. No memory write occurs, so the reservation epoch must
	 * remain unchanged while the per-mm mapping generation advances.
	 */
	orlix_tcti_mapping_sequence_begin(current->mm);
	orlix_tcti_mapping_sequence_end(current->mm);

	ret = orlix_tcti_load_exclusive_user_data(
		current->mm, mapped, &observed, sizeof(observed), &current_pfn,
		&current_generation, &current_mapping_generation);
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_ASSERT_EQ(test, reserved_pfn, current_pfn);
	KUNIT_ASSERT_EQ(test, reserved_generation, current_generation);
	KUNIT_ASSERT_NE(test, reserved_mapping_generation,
			current_mapping_generation);

	ret = orlix_tcti_store_exclusive_user_data(
		current->mm, mapped, &desired, sizeof(desired), reserved_pfn,
		reserved_generation, reserved_mapping_generation, &stored);
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_FALSE(test, stored);
	ret = orlix_tcti_read_user_data(current->mm, mapped, &observed,
				  sizeof(observed));
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, initial, observed);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

/*
 * Exercise the production decoded path, rather than only the backing-memory
 * helpers. LDAXR must establish a reservation, a later ordinary write must
 * break it even when it restores the original bytes, and STLXR must report
 * failure without changing memory. A fresh LDAXR/STLXR pair must then store
 * exactly once and clear the monitor on both terminal paths.
 */
static void orlix_tcti_exclusive_acqrel_status_and_monitor_transitions(
	struct kunit *test)
{
	const u64 initial = 0x1122334455667788ULL;
	const u64 desired = 0x8877665544332211ULL;
	const u32 ldaxr_instruction = orlix_tcti_exclusive_instruction(
		3, true, true, 31, 10, 0);
	const u32 stlxr_instruction = orlix_tcti_exclusive_instruction(
		3, false, true, 2, 10, 4);
	const struct orlix_tcti_decoded_instruction ldaxr =
		orlix_tcti_decode_aarch64(ldaxr_instruction);
	const struct orlix_tcti_decoded_instruction stlxr =
		orlix_tcti_decode_aarch64(stlxr_instruction);
	unsigned long mapped = orlix_tcti_atomic_test_map(test, PROT_READ | PROT_WRITE);
	struct pt_regs regs = {};
	u64 observed = 0;
	int ret;

	if (!mapped)
		return;
	KUNIT_ASSERT_EQ(test, ORLIX_TCTI_DECODE_LOAD_STORE_EXCLUSIVE,
			ldaxr.decode_class);
	KUNIT_ASSERT_EQ(test, ORLIX_TCTI_DECODE_LOAD_STORE_EXCLUSIVE,
			stlxr.decode_class);
	KUNIT_EXPECT_TRUE(test, ldaxr.load);
	KUNIT_EXPECT_TRUE(test, ldaxr.exclusive);
	KUNIT_EXPECT_TRUE(test, ldaxr.acquire);
	KUNIT_EXPECT_FALSE(test, ldaxr.release);
	KUNIT_EXPECT_FALSE(test, stlxr.load);
	KUNIT_EXPECT_TRUE(test, stlxr.exclusive);
	KUNIT_EXPECT_FALSE(test, stlxr.acquire);
	KUNIT_EXPECT_TRUE(test, stlxr.release);

	ret = orlix_tcti_write_user_data(current->mm, mapped, &initial,
				   sizeof(initial));
	KUNIT_ASSERT_EQ(test, 0, ret);
	regs.regs[10] = mapped;
	regs.regs[4] = desired;
	regs.pc = 0x1000;
	ret = orlix_tcti_switch_debug_execute_decoded(current->mm, &regs, &ldaxr,
						       NULL);
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, initial, regs.regs[0]);
	KUNIT_EXPECT_EQ(test, 0x1004ULL, regs.pc);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_exclusive_valid);

	ret = orlix_tcti_write_user_data(current->mm, mapped, &initial,
				   sizeof(initial));
	KUNIT_ASSERT_EQ(test, 0, ret);
	ret = orlix_tcti_switch_debug_execute_decoded(current->mm, &regs, &stlxr,
						       NULL);
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 1ULL, regs.regs[2]);
	KUNIT_EXPECT_EQ(test, 0x1008ULL, regs.pc);
	KUNIT_EXPECT_EQ(test, 0, current->thread.user_exclusive_valid);
	ret = orlix_tcti_read_user_data(current->mm, mapped, &observed,
				  sizeof(observed));
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, initial, observed);

	ret = orlix_tcti_switch_debug_execute_decoded(current->mm, &regs, &ldaxr,
						       NULL);
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, initial, regs.regs[0]);
	KUNIT_EXPECT_EQ(test, 0x100cULL, regs.pc);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_exclusive_valid);
	ret = orlix_tcti_switch_debug_execute_decoded(current->mm, &regs, &stlxr,
						       NULL);
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0ULL, regs.regs[2]);
	KUNIT_EXPECT_EQ(test, 0x1010ULL, regs.pc);
	KUNIT_EXPECT_EQ(test, 0, current->thread.user_exclusive_valid);
	ret = orlix_tcti_read_user_data(current->mm, mapped, &observed,
				  sizeof(observed));
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, desired, observed);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void orlix_tcti_start_thread_clears_exclusive_reservation_state(
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

static void orlix_tcti_signal_delivery_clears_exclusive_reservation_state(
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

	orlix_tcti_prepare_signal_delivery();

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

static struct kunit_case orlix_tcti_atomic_memory_test_cases[] = {
	KUNIT_CASE(orlix_tcti_atomic_memory_cas_all_scalar_widths),
	KUNIT_CASE(orlix_tcti_atomic_memory_rmw_all_scalar_ops_and_widths),
	KUNIT_CASE(orlix_tcti_atomic_memory_signed_extrema_all_scalar_widths),
	KUNIT_CASE(orlix_tcti_atomic_memory_casp_no_tear_under_coordinated_threads),
	KUNIT_CASE(orlix_tcti_atomic_memory_validates_inputs_and_permissions),
	KUNIT_CASE(orlix_tcti_exclusive_reservation_rejects_same_value_writes),
	KUNIT_CASE(orlix_tcti_start_thread_clears_exclusive_reservation_state),
	KUNIT_CASE(orlix_tcti_signal_delivery_clears_exclusive_reservation_state),
	KUNIT_CASE(
		orlix_tcti_exclusive_reservation_rejects_restored_mapping_generation),
	KUNIT_CASE(orlix_tcti_exclusive_acqrel_status_and_monitor_transitions),
	{}
};

struct kunit_suite orlix_tcti_atomic_memory_test_suite = {
	.name = "orlix-tcti-atomic-memory",
	.init = orlix_tcti_atomic_memory_test_init,
	.exit = orlix_tcti_atomic_memory_test_exit,
	.test_cases = orlix_tcti_atomic_memory_test_cases,
};
kunit_test_suite(orlix_tcti_atomic_memory_test_suite);
