// SPDX-License-Identifier: GPL-2.0-only
#include <linux/kernel.h>
#include <linux/pid.h>
#include <linux/sched.h>
#include <asm/ptrace.h>
#include <asm/tcti.h>

#include "report.h"

void tcti_report_unsupported(struct task_struct *task, struct pt_regs *regs,
			     const struct tcti_result *result)
{
	pr_info("Orlix TCTI: unsupported instruction task=%s pid=%d pc=%#lx insn=%#x x0=%#llx x8=%#llx sp=%#llx pstate=%#llx\n",
		task ? task->comm : "<none>",
		task ? task_pid_nr(task) : -1,
		result ? result->pc : 0,
		result ? result->instruction : 0,
		regs ? regs->regs[0] : 0,
		regs ? regs->regs[8] : 0,
		regs ? regs->sp : 0,
		regs ? regs->pstate : 0);
}

void tcti_report_syscall(struct task_struct *task, struct pt_regs *regs,
			 const struct tcti_result *result)
{
	pr_info("Orlix TCTI: svc #0 task=%s pid=%d pc=%#lx syscall=%llu x0=%#llx x1=%#llx x2=%#llx x3=%#llx x4=%#llx x5=%#llx\n",
		task ? task->comm : "<none>",
		task ? task_pid_nr(task) : -1,
		result ? result->pc : 0,
		regs ? (unsigned long long)regs->regs[8] : 0,
		regs ? (unsigned long long)regs->regs[0] : 0,
		regs ? (unsigned long long)regs->regs[1] : 0,
		regs ? (unsigned long long)regs->regs[2] : 0,
		regs ? (unsigned long long)regs->regs[3] : 0,
		regs ? (unsigned long long)regs->regs[4] : 0,
		regs ? (unsigned long long)regs->regs[5] : 0);
}

void tcti_report_exit(struct task_struct *task, struct pt_regs *regs,
		      const struct tcti_result *result)
{
	pr_info("Orlix TCTI: exit task=%s pid=%d reason=%d status=%ld pc=%#lx fault=%#lx insn=%#x\n",
		task ? task->comm : "<none>",
		task ? task_pid_nr(task) : -1,
		result ? result->reason : -1,
		result ? result->status : -1,
		result ? result->pc : 0,
		result ? result->fault_address : 0,
		result ? result->instruction : 0);
}
