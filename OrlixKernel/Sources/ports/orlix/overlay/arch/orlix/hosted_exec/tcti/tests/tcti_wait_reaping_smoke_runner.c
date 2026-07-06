// SPDX-License-Identifier: GPL-2.0-only
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <linux/types.h>

#define TCTI_WAIT_REAPING_SMOKE_CHILD_PID 31337
#define TCTI_WAIT_REAPING_SMOKE_EXIT_CODE 7

enum tcti_exit_reason {
	TCTI_EXIT_SYSCALL,
	TCTI_EXIT_USER_FAULT,
	TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
	TCTI_EXIT_SIGNAL_POINT,
	TCTI_EXIT_YIELD,
	TCTI_EXIT_TASK_EXIT,
};

struct tcti_result {
	enum tcti_exit_reason reason;
	long status;
	unsigned long fault_address;
	int fault_access;
	unsigned long pc;
	u32 instruction;
};

struct tcti_kernel_wait_reaping_smoke_result {
	bool tcti_task_exit_observed;
	bool child_exit_state_recorded;
	bool linux_wait_entered;
	bool linux_wait_status_recorded;
	bool linux_reaping_completed;
	int child_pid;
	int child_exit_code;
	int wait_result_pid;
	int wait_status;
	int wait_return;
};

#include "../wait_reaping_smoke.h"

static int linux_wait_reaping_stub(int child_pid, int child_exit_code,
				   int *wait_result_pid, int *wait_status,
				   bool *child_reaped)
{
	*wait_result_pid = child_pid;
	*wait_status = child_exit_code << 8;
	*child_reaped = true;
	return 0;
}

static bool smoke_result_passed(
	const struct tcti_kernel_wait_reaping_smoke_result *result)
{
	return result->tcti_task_exit_observed &&
	       result->child_exit_state_recorded &&
	       result->linux_wait_entered &&
	       result->linux_wait_status_recorded &&
	       result->linux_reaping_completed &&
	       result->child_pid == TCTI_WAIT_REAPING_SMOKE_CHILD_PID &&
	       result->child_exit_code == TCTI_WAIT_REAPING_SMOKE_EXIT_CODE &&
	       result->wait_result_pid == TCTI_WAIT_REAPING_SMOKE_CHILD_PID &&
	       result->wait_status ==
		       (TCTI_WAIT_REAPING_SMOKE_EXIT_CODE << 8) &&
	       result->wait_return == 0;
}

int main(void)
{
	const char *test_name =
		"tcti_kernel_wait_reaping_smoke_reports_linux_wait";
	struct tcti_kernel_wait_reaping_smoke_result result;
	struct tcti_result task_exit = {
		.reason = TCTI_EXIT_TASK_EXIT,
		.status = TCTI_WAIT_REAPING_SMOKE_EXIT_CODE,
	};
	bool ok;

	memset(&result, 0, sizeof(result));

	ok = tcti_kernel_wait_reaping_smoke_execute(
		&task_exit, &result, linux_wait_reaping_stub,
		TCTI_WAIT_REAPING_SMOKE_CHILD_PID,
		TCTI_WAIT_REAPING_SMOKE_EXIT_CODE);
	ok = ok && smoke_result_passed(&result);

	printf("KTAP version 1\n");
	printf("1..1\n");
	if (ok)
		printf("ok 1 - orlix-tcti-decode.%s\n", test_name);
	else
		printf("not ok 1 - orlix-tcti-decode.%s\n", test_name);
	printf("%s=%s\n", test_name, ok ? "pass" : "fail");
	printf("ORLIX-WAIT-REAPING-RUNNER-BEGIN\n");
	printf("ORLIX-WAIT-REAPING-RUNNER workload_hook_executed=%s\n",
	       ok ? "true" : "false");
	printf("ORLIX-WAIT-REAPING-RUNNER tcti_task_exit_observed=%s\n",
	       result.tcti_task_exit_observed ? "true" : "false");
	printf("ORLIX-WAIT-REAPING-RUNNER child_exit_state_recorded=%s\n",
	       result.child_exit_state_recorded ? "true" : "false");
	printf("ORLIX-WAIT-REAPING-RUNNER linux_wait_entered=%s\n",
	       result.linux_wait_entered ? "true" : "false");
	printf("ORLIX-WAIT-REAPING-RUNNER linux_wait_status_recorded=%s\n",
	       result.linux_wait_status_recorded ? "true" : "false");
	printf("ORLIX-WAIT-REAPING-RUNNER linux_reaping_completed=%s\n",
	       result.linux_reaping_completed ? "true" : "false");
	printf("ORLIX-WAIT-REAPING-RUNNER child_pid=%d\n",
	       result.child_pid);
	printf("ORLIX-WAIT-REAPING-RUNNER child_exit_code=%d\n",
	       result.child_exit_code);
	printf("ORLIX-WAIT-REAPING-RUNNER wait_result_pid=%d\n",
	       result.wait_result_pid);
	printf("ORLIX-WAIT-REAPING-RUNNER wait_status=%d\n",
	       result.wait_status);
	printf("ORLIX-WAIT-REAPING-RUNNER wait_return=%d\n",
	       result.wait_return);
	printf("ORLIX-WAIT-REAPING-RUNNER-END status=%s\n",
	       ok ? "pass" : "fail");

	return ok ? 0 : 1;
}
