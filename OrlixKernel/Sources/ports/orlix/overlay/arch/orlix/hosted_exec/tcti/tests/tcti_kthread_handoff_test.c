// SPDX-License-Identifier: GPL-2.0-only

#include <kunit/test.h>
#include <linux/completion.h>
#include <linux/err.h>
#include <linux/jiffies.h>
#include <linux/kthread.h>
#include <linux/sched.h>

struct tcti_kthread_handoff_state {
	struct completion entered;
	struct completion exited;
};

static int tcti_kthread_handoff_worker(void *data)
{
	struct tcti_kthread_handoff_state *state = data;

	complete(&state->entered);
	while (!kthread_should_stop()) {
		set_current_state(TASK_INTERRUPTIBLE);
		if (!kthread_should_stop())
			schedule();
	}
	__set_current_state(TASK_RUNNING);
	complete(&state->exited);
	return 0;
}

static void tcti_kthread_handoff_runs_and_joins_live_worker(struct kunit *test)
{
	struct tcti_kthread_handoff_state state;
	struct task_struct *task;
	int ret;

	init_completion(&state.entered);
	init_completion(&state.exited);
	task = kthread_run(tcti_kthread_handoff_worker, &state,
			   "tcti-kthread-handoff");
	KUNIT_ASSERT_NOT_NULL(test, task);
	KUNIT_ASSERT_FALSE(test, IS_ERR(task));

	if (!wait_for_completion_timeout(&state.entered,
					 msecs_to_jiffies(5000))) {
		KUNIT_FAIL(test, "kthread worker did not enter");
		kthread_stop(task);
		return;
	}

	ret = kthread_stop(task);
	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_TRUE(test, completion_done(&state.exited));
}

static struct kunit_case tcti_kthread_handoff_test_cases[] = {
	KUNIT_CASE(tcti_kthread_handoff_runs_and_joins_live_worker),
	{}
};

static struct kunit_suite tcti_kthread_handoff_test_suite = {
	.name = "orlix-tcti-kthread-handoff",
	.test_cases = tcti_kthread_handoff_test_cases,
};

kunit_test_suite(tcti_kthread_handoff_test_suite);

MODULE_LICENSE("GPL");
