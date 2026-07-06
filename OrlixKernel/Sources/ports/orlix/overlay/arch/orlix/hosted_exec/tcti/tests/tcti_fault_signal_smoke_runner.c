// SPDX-License-Identifier: GPL-2.0-only
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <errno.h>
#include <linux/types.h>

#define NO_SYSCALL (-1)
#define PSR_MODE_EL0t 0x00000000UL
#define SEGV_MAPERR 1
#define SIGSEGV 11
#define STACK_TOP 0x0000800000000000UL

struct pt_regs {
	u64 regs[31];
	u64 sp;
	u64 pc;
	u64 pstate;
	u64 orig_x0;
	s32 syscallno;
	u32 unused;
};

enum tcti_exit_reason {
	TCTI_EXIT_SYSCALL,
	TCTI_EXIT_USER_FAULT,
	TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
	TCTI_EXIT_SIGNAL_POINT,
	TCTI_EXIT_YIELD,
	TCTI_EXIT_TASK_EXIT,
};

enum tcti_access {
	TCTI_ACCESS_FETCH,
	TCTI_ACCESS_READ,
	TCTI_ACCESS_WRITE,
};

struct tcti_result {
	enum tcti_exit_reason reason;
	long status;
	unsigned long fault_address;
	enum tcti_access fault_access;
	unsigned long pc;
	u32 instruction;
};

struct tcti_kernel_fault_signal_smoke_result {
	bool tcti_user_fault_exit;
	bool fault_address_recorded;
	bool fault_access_recorded;
	bool fault_pc_recorded;
	bool fault_instruction_recorded;
	bool linux_fault_handler_entered;
	bool linux_signal_result_recorded;
	unsigned long fault_address;
	enum tcti_access fault_access;
	unsigned long fault_pc;
	u32 fault_instruction;
	int handler_return;
	int signal_number;
	int signal_code;
	unsigned long signaled_address;
};

#include "../fault_signal_smoke.h"

static int linux_fault_signal_stub(struct pt_regs *regs, unsigned long address,
				   enum tcti_access access,
				   int *signal_number, int *signal_code,
				   unsigned long *signal_address)
{
	(void)regs;
	(void)access;
	*signal_number = SIGSEGV;
	*signal_code = SEGV_MAPERR;
	*signal_address = address;
	return 0;
}

static bool smoke_result_passed(
	const struct tcti_kernel_fault_signal_smoke_result *result)
{
	return result->tcti_user_fault_exit &&
	       result->fault_address_recorded &&
	       result->fault_access_recorded &&
	       result->fault_pc_recorded &&
	       result->fault_instruction_recorded &&
	       result->linux_fault_handler_entered &&
	       result->linux_signal_result_recorded &&
	       result->handler_return == 0 &&
	       result->signal_number == SIGSEGV &&
	       result->signal_code == SEGV_MAPERR &&
	       result->signaled_address == result->fault_address;
}

int main(void)
{
	const char *test_name =
		"tcti_kernel_fault_signal_smoke_reports_linux_signal";
	struct tcti_kernel_fault_signal_smoke_result result;
	struct tcti_result fault = {
		.reason = TCTI_EXIT_USER_FAULT,
		.status = -EFAULT,
		.fault_address = 0x4000,
		.fault_access = TCTI_ACCESS_READ,
		.pc = 0x210128,
		.instruction = 0xf9400000,
	};
	struct pt_regs regs;
	bool ok;

	memset(&result, 0, sizeof(result));
	memset(&regs, 0, sizeof(regs));
	regs.pc = fault.pc;
	regs.sp = STACK_TOP - 16;
	regs.pstate = PSR_MODE_EL0t;
	regs.syscallno = NO_SYSCALL;

	ok = tcti_kernel_fault_signal_smoke_execute(
		&regs, &fault, &result, linux_fault_signal_stub,
		SIGSEGV, SEGV_MAPERR);
	ok = ok && smoke_result_passed(&result);

	printf("KTAP version 1\n");
	printf("1..1\n");
	if (ok)
		printf("ok 1 - orlix-tcti-decode.%s\n", test_name);
	else
		printf("not ok 1 - orlix-tcti-decode.%s\n", test_name);
	printf("%s=%s\n", test_name, ok ? "pass" : "fail");
	printf("ORLIX-FAULT-SIGNAL-RUNNER-BEGIN\n");
	printf("ORLIX-FAULT-SIGNAL-RUNNER workload_hook_executed=%s\n",
	       ok ? "true" : "false");
	printf("ORLIX-FAULT-SIGNAL-RUNNER tcti_user_fault_exit=%s\n",
	       result.tcti_user_fault_exit ? "true" : "false");
	printf("ORLIX-FAULT-SIGNAL-RUNNER fault_address=0x%lx\n",
	       result.fault_address);
	printf("ORLIX-FAULT-SIGNAL-RUNNER fault_access=%d\n",
	       result.fault_access);
	printf("ORLIX-FAULT-SIGNAL-RUNNER fault_pc=0x%lx\n",
	       result.fault_pc);
	printf("ORLIX-FAULT-SIGNAL-RUNNER fault_instruction=0x%x\n",
	       result.fault_instruction);
	printf("ORLIX-FAULT-SIGNAL-RUNNER linux_fault_handler_entered=%s\n",
	       result.linux_fault_handler_entered ? "true" : "false");
	printf("ORLIX-FAULT-SIGNAL-RUNNER linux_signal_result_recorded=%s\n",
	       result.linux_signal_result_recorded ? "true" : "false");
	printf("ORLIX-FAULT-SIGNAL-RUNNER signal_number=%d\n",
	       result.signal_number);
	printf("ORLIX-FAULT-SIGNAL-RUNNER signal_code=%d\n",
	       result.signal_code);
	printf("ORLIX-FAULT-SIGNAL-RUNNER signaled_address=0x%lx\n",
	       result.signaled_address);
	printf("ORLIX-FAULT-SIGNAL-RUNNER handler_return=%d\n",
	       result.handler_return);
	printf("ORLIX-FAULT-SIGNAL-RUNNER-END status=%s\n",
	       ok ? "pass" : "fail");

	return ok ? 0 : 1;
}
