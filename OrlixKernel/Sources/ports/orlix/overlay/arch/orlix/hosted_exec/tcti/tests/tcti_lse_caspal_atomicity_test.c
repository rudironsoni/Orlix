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
#include <asm/ptrace.h>
#include <asm/tcti.h>

#include "../decode_aarch64.h"
#include "../switch_debug.h"

#define TCTI_LSE_CASPAL_ITERATIONS 1024U
#define TCTI_LSE_CASPAL_BASE 0x08207c00U

static int tcti_lse_caspal_atomicity_test_init(struct kunit *test)
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

static void tcti_lse_caspal_atomicity_test_exit(struct kunit *test)
{
	struct mm_struct *mm = test->priv;

	if (!mm)
		return;
	kthread_unuse_mm(mm);
	mmput(mm);
}

struct tcti_lse_caspal_value {
	u64 low;
	u64 high;
};

struct tcti_lse_caspal_writer {
	struct mm_struct *mm;
	unsigned long address;
	const struct tcti_decoded_instruction *decoded;
	const struct tcti_lse_caspal_value *first;
	const struct tcti_lse_caspal_value *second;
	struct completion *start;
	struct completion done;
	atomic_t *writers_running;
	int ret;
	unsigned int successes;
};

struct tcti_lse_caspal_reader {
	struct mm_struct *mm;
	unsigned long address;
	const struct tcti_lse_caspal_value *first;
	const struct tcti_lse_caspal_value *second;
	struct completion *start;
	struct completion ready;
	struct completion done;
	atomic_t *writers_running;
	int ret;
	bool torn;
};

static bool tcti_lse_caspal_value_is(const struct tcti_lse_caspal_value *value,
				      const struct tcti_lse_caspal_value *expected)
{
	return !memcmp(value, expected, sizeof(*value));
}

static const struct tcti_lse_caspal_value *
tcti_lse_caspal_other(const struct tcti_lse_caspal_value *observed,
			      const struct tcti_lse_caspal_value *first,
			      const struct tcti_lse_caspal_value *second)
{
	if (tcti_lse_caspal_value_is(observed, first))
		return second;
	if (tcti_lse_caspal_value_is(observed, second))
		return first;
	return NULL;
}

static int tcti_lse_caspal_writer(void *data)
{
	struct tcti_lse_caspal_writer *writer = data;
	struct pt_regs regs = {};
	struct tcti_lse_caspal_value observed = *writer->first;
	unsigned int index;

	regs.regs[10] = writer->address;
	kthread_use_mm(writer->mm);
	wait_for_completion(writer->start);
	for (index = 0; index < TCTI_LSE_CASPAL_ITERATIONS; index++) {
		const struct tcti_lse_caspal_value expected = observed;
		const struct tcti_lse_caspal_value *desired =
			tcti_lse_caspal_other(&observed, writer->first,
					      writer->second);

		if (kthread_should_stop())
			break;
		if (!desired) {
			writer->ret = -EINVAL;
			break;
		}
		regs.regs[6] = observed.low;
		regs.regs[7] = observed.high;
		regs.regs[8] = desired->low;
		regs.regs[9] = desired->high;
		writer->ret = tcti_switch_debug_execute_decoded(
			writer->mm, &regs, writer->decoded, NULL);
		if (writer->ret)
			break;
		observed.low = regs.regs[6];
		observed.high = regs.regs[7];
		if (tcti_lse_caspal_value_is(&observed, &expected))
			writer->successes++;
		cond_resched();
	}
	atomic_dec(writer->writers_running);
	kthread_unuse_mm(writer->mm);
	complete(&writer->done);
	return 0;
}

static int tcti_lse_caspal_reader(void *data)
{
	struct tcti_lse_caspal_reader *reader = data;
	struct tcti_lse_caspal_value observed;

	kthread_use_mm(reader->mm);
	complete(&reader->ready);
	wait_for_completion(reader->start);
	while (atomic_read(reader->writers_running)) {
		if (kthread_should_stop())
			break;
		reader->ret = tcti_read_user_data(reader->mm, reader->address,
					  &observed, sizeof(observed));
		if (reader->ret ||
		    (!tcti_lse_caspal_value_is(&observed, reader->first) &&
		     !tcti_lse_caspal_value_is(&observed, reader->second))) {
			reader->torn = true;
			break;
		}
		cpu_relax();
		cond_resched();
	}
	kthread_unuse_mm(reader->mm);
	complete(&reader->done);
	return 0;
}

static void tcti_lse_caspal_acqrel_does_not_expose_torn_pair(struct kunit *test)
{
	const struct tcti_lse_caspal_value first = {
		.low = 0x0123456789abcdefULL,
		.high = 0xfedcba9876543210ULL,
	};
	const struct tcti_lse_caspal_value second = {
		.low = 0x1122334455667788ULL,
		.high = 0x8877665544332211ULL,
	};
	const u32 instruction = TCTI_LSE_CASPAL_BASE | BIT(30) | BIT(22) |
		BIT(15) | (6U << 16) | (10U << 5) | 8U;
	struct tcti_decoded_instruction decoded =
		tcti_decode_aarch64(instruction);
	unsigned long mapped;
	struct completion start;
	struct tcti_lse_caspal_writer writers[2];
	struct tcti_lse_caspal_reader reader;
	struct tcti_lse_caspal_value final_value;
	struct task_struct *writer_tasks[ARRAY_SIZE(writers)] = {};
	struct task_struct *reader_task = NULL;
	atomic_t writers_running;
	unsigned int writer_count = 0;
	unsigned int index;
	bool exercise_completed = false;
	int ret;

	mapped = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));
	KUNIT_ASSERT_EQ(test, TCTI_DECODE_LSE_ATOMIC, decoded.decode_class);
	KUNIT_ASSERT_EQ(test, TCTI_LSE_ATOMIC_CAS, decoded.lse_atomic_op);
	KUNIT_ASSERT_TRUE(test, decoded.pair);
	KUNIT_ASSERT_TRUE(test, decoded.acquire);
	KUNIT_ASSERT_TRUE(test, decoded.release);
	KUNIT_ASSERT_EQ(test, sizeof(u64), decoded.access_size);
	KUNIT_ASSERT_EQ(test, 0,
		tcti_write_user_data(current->mm, mapped, &first, sizeof(first)));

	init_completion(&start);
	atomic_set(&writers_running, ARRAY_SIZE(writers));
	for (index = 0; index < ARRAY_SIZE(writers); index++) {
		writers[index] = (struct tcti_lse_caspal_writer) {
			.mm = current->mm,
			.address = mapped,
			.decoded = &decoded,
			.first = &first,
			.second = &second,
			.start = &start,
			.writers_running = &writers_running,
		};
		init_completion(&writers[index].done);
		writer_tasks[index] = kthread_run(tcti_lse_caspal_writer,
			&writers[index], "tcti-caspal-writer");
		if (IS_ERR(writer_tasks[index])) {
			KUNIT_FAIL(test, "failed to create CASPAL writer %u",
				   index);
			writer_tasks[index] = NULL;
			goto out_stop_threads;
		}
		writer_count++;
	}
	reader = (struct tcti_lse_caspal_reader) {
		.mm = current->mm,
		.address = mapped,
		.first = &first,
		.second = &second,
		.start = &start,
		.writers_running = &writers_running,
	};
	init_completion(&reader.ready);
	init_completion(&reader.done);
	reader_task = kthread_run(tcti_lse_caspal_reader, &reader,
				  "tcti-caspal-reader");
	if (IS_ERR(reader_task)) {
		KUNIT_FAIL(test, "failed to create CASPAL reader");
		reader_task = NULL;
		goto out_stop_threads;
	}
	if (!wait_for_completion_timeout(&reader.ready,
					 msecs_to_jiffies(5000))) {
		KUNIT_FAIL(test, "CASPAL reader did not become ready");
		goto out_stop_threads;
	}
	complete_all(&start);
	for (index = 0; index < ARRAY_SIZE(writers); index++) {
		if (!wait_for_completion_timeout(&writers[index].done,
						 msecs_to_jiffies(5000))) {
			KUNIT_FAIL(test, "CASPAL writer %u timed out", index);
			goto out_stop_threads;
		}
	}
	if (!wait_for_completion_timeout(&reader.done,
					 msecs_to_jiffies(5000))) {
		KUNIT_FAIL(test, "CASPAL reader timed out");
		goto out_stop_threads;
	}
	exercise_completed = true;

out_stop_threads:
	/*
	 * Every created task references this function's stack and current->mm.
	 * Open the gate on every failure path, then synchronously join the tasks
	 * before inspecting results or allowing either context to go out of
	 * scope.
	 */
	complete_all(&start);
	if (reader_task)
		kthread_stop(reader_task);
	for (index = 0; index < writer_count; index++)
		kthread_stop(writer_tasks[index]);
	if (!exercise_completed)
		goto out_unmap;

	for (index = 0; index < ARRAY_SIZE(writers); index++) {
		KUNIT_EXPECT_EQ(test, 0, writers[index].ret);
		KUNIT_EXPECT_GT(test, writers[index].successes, 0U);
	}
	KUNIT_EXPECT_EQ(test, 0, reader.ret);
	KUNIT_EXPECT_FALSE(test, reader.torn);
	ret = tcti_read_user_data(current->mm, mapped, &final_value,
				  sizeof(final_value));
	KUNIT_EXPECT_EQ(test, 0, ret);
	if (!ret)
		KUNIT_EXPECT_TRUE(test,
			tcti_lse_caspal_value_is(&final_value, &first) ||
			tcti_lse_caspal_value_is(&final_value, &second));

out_unmap:
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static struct kunit_case tcti_lse_caspal_atomicity_test_cases[] = {
	KUNIT_CASE(tcti_lse_caspal_acqrel_does_not_expose_torn_pair),
	{}
};

struct kunit_suite tcti_lse_caspal_atomicity_test_suite = {
	.name = "orlix-tcti-lse-caspal-atomicity",
	.init = tcti_lse_caspal_atomicity_test_init,
	.exit = tcti_lse_caspal_atomicity_test_exit,
	.test_cases = tcti_lse_caspal_atomicity_test_cases,
};
kunit_test_suite(tcti_lse_caspal_atomicity_test_suite);

MODULE_LICENSE("GPL");
