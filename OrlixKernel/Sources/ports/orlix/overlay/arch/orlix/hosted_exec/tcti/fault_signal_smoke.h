/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_FAULT_SIGNAL_SMOKE_H
#define ORLIX_TCTI_FAULT_SIGNAL_SMOKE_H

typedef int (*tcti_fault_signal_smoke_fn)(
	struct pt_regs *regs,
	unsigned long address,
	enum tcti_access access,
	int *signal_number,
	int *signal_code,
	unsigned long *signal_address);

static inline bool tcti_kernel_fault_signal_smoke_execute(
	struct pt_regs *regs,
	const struct tcti_result *fault,
	struct tcti_kernel_fault_signal_smoke_result *out,
	tcti_fault_signal_smoke_fn handle_fault,
	int expected_signal,
	int expected_code)
{
	int signal_number = 0;
	int signal_code = 0;
	unsigned long signal_address = 0;

	if (!regs || !fault || !out || !handle_fault)
		return false;

	memset(out, 0, sizeof(*out));
	out->tcti_user_fault_exit = fault->reason == TCTI_EXIT_USER_FAULT &&
		(fault->status == -EFAULT || fault->status == -EACCES);
	out->fault_address = fault->fault_address;
	out->fault_access = fault->fault_access;
	out->fault_pc = fault->pc;
	out->fault_instruction = fault->instruction;
	out->fault_address_recorded = fault->fault_address != 0;
	out->fault_access_recorded = fault->fault_access == TCTI_ACCESS_FETCH ||
		fault->fault_access == TCTI_ACCESS_READ ||
		fault->fault_access == TCTI_ACCESS_WRITE;
	out->fault_pc_recorded = fault->pc == regs->pc;
	out->fault_instruction_recorded = fault->instruction != 0;

	if (!out->tcti_user_fault_exit)
		return false;

	out->linux_fault_handler_entered = true;
	out->handler_return = handle_fault(regs, fault->fault_address,
					   fault->fault_access,
					   &signal_number, &signal_code,
					   &signal_address);
	out->signal_number = signal_number;
	out->signal_code = signal_code;
	out->signaled_address = signal_address;
	out->linux_signal_result_recorded = out->handler_return == 0 &&
		signal_number == expected_signal &&
		signal_code == expected_code &&
		signal_address == fault->fault_address;

	return out->fault_address_recorded &&
		out->fault_access_recorded &&
		out->fault_pc_recorded &&
		out->fault_instruction_recorded &&
		out->linux_fault_handler_entered &&
		out->linux_signal_result_recorded;
}

#endif /* ORLIX_TCTI_FAULT_SIGNAL_SMOKE_H */
