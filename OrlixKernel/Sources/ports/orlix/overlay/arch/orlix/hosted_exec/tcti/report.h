/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_REPORT_H
#define ORLIX_TCTI_REPORT_H

#include <asm/tcti.h>

void tcti_report_unsupported(struct task_struct *task, struct pt_regs *regs,
			      const struct tcti_result *result);
void tcti_report_syscall(struct task_struct *task, struct pt_regs *regs,
			 const struct tcti_result *result);
void tcti_report_syscall_return(struct task_struct *task, struct pt_regs *regs,
				unsigned long nr, unsigned long pc);
void tcti_report_exit(struct task_struct *task, struct pt_regs *regs,
		      const struct tcti_result *result);

#endif /* ORLIX_TCTI_REPORT_H */
