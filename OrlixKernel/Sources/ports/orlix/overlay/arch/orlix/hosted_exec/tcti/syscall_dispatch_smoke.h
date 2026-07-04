/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_SYSCALL_DISPATCH_SMOKE_H
#define ORLIX_TCTI_SYSCALL_DISPATCH_SMOKE_H

#include "decode_aarch64.h"

#define TCTI_SYSCALL_DISPATCH_SMOKE_INSTRUCTION 0xd4000001U

typedef long (*tcti_syscall_dispatch_smoke_fn)(struct pt_regs *regs);

static inline bool tcti_kernel_syscall_dispatch_smoke_execute(
	struct pt_regs *regs,
	struct tcti_kernel_syscall_dispatch_smoke_result *out,
	tcti_syscall_dispatch_smoke_fn dispatch)
{
	struct tcti_decoded_instruction decoded;
	long ret;

	if (!regs || !out || !dispatch)
		return false;

	decoded = tcti_decode_aarch64(TCTI_SYSCALL_DISPATCH_SMOKE_INSTRUCTION);
	out->decoded_svc = decoded.decode_class == TCTI_DECODE_SVC;
	if (!out->decoded_svc)
		return false;

	out->svc_boundary_reached = true;
	regs->regs[8] = __NR_getpid;
	tcti_prepare_syscall_handoff(regs);
	out->handoff_prepared = regs->syscallno == __NR_getpid;
	out->observed_syscall_nr = regs->syscallno;
	if (!out->handoff_prepared)
		return false;

	out->orlix_syscall_dispatch_entered = true;
	ret = dispatch(regs);
	out->return_value = ret;
	out->return_x0 = regs->regs[0];
	out->syscallno_after_dispatch = regs->syscallno;
	out->linux_return_state_written =
		regs->regs[0] == (unsigned long)ret &&
		regs->syscallno == NO_SYSCALL;

	return out->linux_return_state_written;
}

#endif /* ORLIX_TCTI_SYSCALL_DISPATCH_SMOKE_H */
