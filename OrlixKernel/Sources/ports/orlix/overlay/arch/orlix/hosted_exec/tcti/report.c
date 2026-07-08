// SPDX-License-Identifier: GPL-2.0-only
#include <linux/atomic.h>
#include <linux/kernel.h>
#include <linux/pid.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <asm/ptrace.h>
#include <asm/tcti.h>

#include "report.h"

#define TCTI_DEFAULT_SYSCALL_REPORT_BUDGET 32
#define TCTI_NON_INIT_SYSCALL_REPORT_BUDGET 64

static bool tcti_is_init_task(struct task_struct *task)
{
	return task && strcmp(task->comm, "init") == 0;
}

static bool tcti_should_report_syscall_entry(struct task_struct *task)
{
	static atomic_t budget =
		ATOMIC_INIT(TCTI_DEFAULT_SYSCALL_REPORT_BUDGET);
	static atomic_t non_init_budget =
		ATOMIC_INIT(TCTI_NON_INIT_SYSCALL_REPORT_BUDGET);

	if (IS_ENABLED(CONFIG_ORLIX_TCTI_SYSCALL_TRACE))
		return true;
	if (!tcti_is_init_task(task))
		return atomic_dec_if_positive(&non_init_budget) >= 0;

	return atomic_dec_if_positive(&budget) >= 0;
}

static bool tcti_should_report_syscall_return(struct task_struct *task)
{
	static atomic_t non_init_return_budget =
		ATOMIC_INIT(TCTI_NON_INIT_SYSCALL_REPORT_BUDGET);

	if (IS_ENABLED(CONFIG_ORLIX_TCTI_SYSCALL_TRACE))
		return true;

	return !tcti_is_init_task(task) &&
	       atomic_dec_if_positive(&non_init_return_budget) >= 0;
}

void tcti_report_unsupported(struct task_struct *task, struct pt_regs *regs,
			     const struct tcti_result *result)
{
	pr_info("Orlix TCTI: unsupported instruction task=%s pid=%d pc=%#lx insn=%#x x0=%#llx x8=%#llx x9=%#llx x10=%#llx x11=%#llx x12=%#llx x13=%#llx x19=%#llx sp=%#llx pstate=%#llx\n",
		task ? task->comm : "<none>",
		task ? task_pid_nr(task) : -1,
		result ? result->pc : 0,
		result ? result->instruction : 0,
		regs ? regs->regs[0] : 0,
		regs ? regs->regs[8] : 0,
		regs ? regs->regs[9] : 0,
		regs ? regs->regs[10] : 0,
		regs ? regs->regs[11] : 0,
		regs ? regs->regs[12] : 0,
		regs ? regs->regs[13] : 0,
		regs ? regs->regs[19] : 0,
		regs ? regs->sp : 0,
		regs ? regs->pstate : 0);
}

void tcti_report_syscall(struct task_struct *task, struct pt_regs *regs,
			 const struct tcti_result *result)
{
	if (!tcti_should_report_syscall_entry(task))
		return;

	pr_info("Orlix TCTI: svc #0 task=%s pid=%d pc=%#lx syscall=%llu x0=%#llx x1=%#llx x2=%#llx x3=%#llx x4=%#llx x5=%#llx x30=%#llx sp=%#llx\n",
		task ? task->comm : "<none>",
		task ? task_pid_nr(task) : -1,
		result ? result->pc : 0,
		regs ? (unsigned long long)regs->regs[8] : 0,
		regs ? (unsigned long long)regs->regs[0] : 0,
		regs ? (unsigned long long)regs->regs[1] : 0,
		regs ? (unsigned long long)regs->regs[2] : 0,
		regs ? (unsigned long long)regs->regs[3] : 0,
		regs ? (unsigned long long)regs->regs[4] : 0,
		regs ? (unsigned long long)regs->regs[5] : 0,
		regs ? (unsigned long long)regs->regs[30] : 0,
		regs ? (unsigned long long)regs->sp : 0);
}

void tcti_report_syscall_return(struct task_struct *task, struct pt_regs *regs,
				unsigned long nr, unsigned long pc)
{
	if (!tcti_should_report_syscall_return(task))
		return;

	pr_info("Orlix TCTI: syscall return task=%s pid=%d pc=%#lx syscall=%lu ret=%#llx signed_ret=%lld sp=%#llx\n",
		task ? task->comm : "<none>",
		task ? task_pid_nr(task) : -1,
		pc,
		nr,
		regs ? (unsigned long long)regs->regs[0] : 0,
		regs ? (long long)regs->regs[0] : 0,
		regs ? (unsigned long long)regs->sp : 0);
}

void tcti_report_exit(struct task_struct *task, struct pt_regs *regs,
			      const struct tcti_result *result)
{
	pr_info("Orlix TCTI: exit task=%s pid=%d reason=%d status=%ld pc=%#lx fault=%#lx insn=%#x x0=%#llx x1=%#llx x8=%#llx x9=%#llx x10=%#llx x11=%#llx x12=%#llx x13=%#llx x19=%#llx x20=%#llx x21=%#llx x22=%#llx x29=%#llx x30=%#llx sp=%#llx pstate=%#llx\n",
		task ? task->comm : "<none>",
		task ? task_pid_nr(task) : -1,
		result ? result->reason : -1,
		result ? result->status : -1,
		result ? result->pc : 0,
		result ? result->fault_address : 0,
		result ? result->instruction : 0,
		regs ? regs->regs[0] : 0,
		regs ? regs->regs[1] : 0,
		regs ? regs->regs[8] : 0,
		regs ? regs->regs[9] : 0,
		regs ? regs->regs[10] : 0,
		regs ? regs->regs[11] : 0,
		regs ? regs->regs[12] : 0,
		regs ? regs->regs[13] : 0,
		regs ? regs->regs[19] : 0,
		regs ? regs->regs[20] : 0,
		regs ? regs->regs[21] : 0,
		regs ? regs->regs[22] : 0,
		regs ? regs->regs[29] : 0,
		regs ? regs->regs[30] : 0,
		regs ? regs->sp : 0,
		regs ? regs->pstate : 0);
}
