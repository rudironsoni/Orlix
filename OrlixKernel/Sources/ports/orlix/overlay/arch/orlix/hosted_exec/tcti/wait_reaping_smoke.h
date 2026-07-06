/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_WAIT_REAPING_SMOKE_H
#define ORLIX_TCTI_WAIT_REAPING_SMOKE_H

typedef int (*tcti_wait_reaping_smoke_fn)(
	int child_pid,
	int child_exit_code,
	int *wait_result_pid,
	int *wait_status,
	bool *child_reaped);

static inline bool tcti_kernel_wait_reaping_smoke_execute(
	const struct tcti_result *task_exit,
	struct tcti_kernel_wait_reaping_smoke_result *out,
	tcti_wait_reaping_smoke_fn wait_child,
	int child_pid,
	int expected_exit_code)
{
	int wait_result_pid = 0;
	int wait_status = 0;
	bool child_reaped = false;

	if (!task_exit || !out || !wait_child)
		return false;

	out->tcti_task_exit_observed =
		task_exit->reason == TCTI_EXIT_TASK_EXIT;
	out->child_pid = child_pid;
	out->child_exit_code = task_exit->status;
	out->child_exit_state_recorded =
		out->tcti_task_exit_observed &&
		child_pid > 0 &&
		task_exit->status == expected_exit_code;
	out->linux_wait_entered = false;
	out->linux_wait_status_recorded = false;
	out->linux_reaping_completed = false;
	out->wait_result_pid = 0;
	out->wait_status = 0;
	out->wait_return = 0;

	if (!out->child_exit_state_recorded)
		return false;

	out->linux_wait_entered = true;
	out->wait_return = wait_child(child_pid, task_exit->status,
				      &wait_result_pid, &wait_status,
				      &child_reaped);
	out->wait_result_pid = wait_result_pid;
	out->wait_status = wait_status;
	out->linux_wait_status_recorded =
		out->wait_return == 0 &&
		wait_result_pid == child_pid &&
		wait_status == (expected_exit_code << 8);
	out->linux_reaping_completed = child_reaped;

	return out->linux_wait_entered &&
		out->linux_wait_status_recorded &&
		out->linux_reaping_completed;
}

#endif /* ORLIX_TCTI_WAIT_REAPING_SMOKE_H */
