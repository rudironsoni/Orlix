// SPDX-License-Identifier: GPL-2.0-only
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <linux/types.h>

#define __NR_getpid 172
#define NO_SYSCALL (-1)
#define PSR_MODE_EL0t 0x00000000UL
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

struct tcti_kernel_syscall_dispatch_smoke_result {
	bool decoded_svc;
	bool svc_boundary_reached;
	bool handoff_prepared;
	unsigned long observed_syscall_nr;
	bool orlix_syscall_dispatch_entered;
	bool linux_return_state_written;
	long return_value;
	unsigned long return_x0;
	s32 syscallno_after_dispatch;
};

void tcti_prepare_syscall_handoff(struct pt_regs *regs)
{
	regs->orig_x0 = regs->regs[0];
	regs->syscallno = regs->regs[8];
	regs->pc += sizeof(u32);
}

static inline void syscall_set_return_value(struct pt_regs *regs, long value)
{
	regs->regs[0] = (unsigned long)value;
}

static inline bool in_syscall(const struct pt_regs *regs)
{
	return regs->syscallno != NO_SYSCALL;
}

static inline void forget_syscall(struct pt_regs *regs)
{
	regs->syscallno = NO_SYSCALL;
}

static long sys_getpid(unsigned long arg0, unsigned long arg1,
		       unsigned long arg2, unsigned long arg3,
		       unsigned long arg4, unsigned long arg5)
{
	(void)arg0;
	(void)arg1;
	(void)arg2;
	(void)arg3;
	(void)arg4;
	(void)arg5;
	return 4242;
}

static long sys_ni_syscall(unsigned long arg0, unsigned long arg1,
			   unsigned long arg2, unsigned long arg3,
			   unsigned long arg4, unsigned long arg5)
{
	(void)arg0;
	(void)arg1;
	(void)arg2;
	(void)arg3;
	(void)arg4;
	(void)arg5;
	return -38;
}

long orlix_syscall_dispatch(struct pt_regs *regs)
{
	unsigned long nr = regs->syscallno;
	long ret = -38;

	regs->orig_x0 = regs->regs[0];
	if (nr == __NR_getpid)
		ret = sys_getpid(regs->orig_x0, regs->regs[1], regs->regs[2],
				 regs->regs[3], regs->regs[4], regs->regs[5]);
	else
		ret = sys_ni_syscall(regs->orig_x0, regs->regs[1],
				     regs->regs[2], regs->regs[3],
				     regs->regs[4], regs->regs[5]);

	syscall_set_return_value(regs, ret);
	if (in_syscall(regs))
		forget_syscall(regs);
	return regs->regs[0];
}

#include "../syscall_dispatch_smoke.h"

static bool smoke_result_passed(
	const struct tcti_kernel_syscall_dispatch_smoke_result *result,
	const struct pt_regs *regs)
{
	return result->decoded_svc &&
	       result->svc_boundary_reached &&
	       result->handoff_prepared &&
	       result->observed_syscall_nr == __NR_getpid &&
	       result->orlix_syscall_dispatch_entered &&
	       result->linux_return_state_written &&
	       result->return_value == (long)result->return_x0 &&
	       result->return_value >= 0 &&
	       result->syscallno_after_dispatch == NO_SYSCALL &&
	       regs->syscallno == NO_SYSCALL;
}

int main(void)
{
	const char *test_name =
		"tcti_kernel_syscall_dispatch_smoke_reaches_linux_dispatch";
	struct tcti_kernel_syscall_dispatch_smoke_result result;
	struct pt_regs regs;
	bool ok;

	memset(&result, 0, sizeof(result));
	memset(&regs, 0, sizeof(regs));
	regs.pc = 0x210128;
	regs.sp = STACK_TOP - 16;
	regs.pstate = PSR_MODE_EL0t;
	regs.syscallno = NO_SYSCALL;

	ok = tcti_kernel_syscall_dispatch_smoke_execute(
		&regs, &result, orlix_syscall_dispatch);
	ok = ok && smoke_result_passed(&result, &regs);

	printf("KTAP version 1\n");
	printf("1..1\n");
	if (ok)
		printf("ok 1 - orlix-tcti-decode.%s\n", test_name);
	else
		printf("not ok 1 - orlix-tcti-decode.%s\n", test_name);
	printf("%s=%s\n", test_name, ok ? "pass" : "fail");
	printf("ORLIX-KUNIT-RUNNER workload_hook_executed=%s\n",
	       ok ? "true" : "false");
	printf("ORLIX-KUNIT-RUNNER svc_boundary_reached=%s\n",
	       result.svc_boundary_reached ? "true" : "false");
	printf("ORLIX-KUNIT-RUNNER syscall_number_observed=%lu\n",
	       result.observed_syscall_nr);
	printf("ORLIX-KUNIT-RUNNER orlix_syscall_dispatch_entered=%s\n",
	       result.orlix_syscall_dispatch_entered ? "true" : "false");
	printf("ORLIX-KUNIT-RUNNER linux_return_state_written=%s\n",
	       result.linux_return_state_written ? "true" : "false");
	printf("ORLIX-KUNIT-RUNNER return_value=%ld\n", result.return_value);
	printf("ORLIX-KUNIT-RUNNER syscallno_after_dispatch=%d\n",
	       result.syscallno_after_dispatch);

	return ok ? 0 : 1;
}
