/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_ENGINE_H
#define ORLIX_TCTI_ENGINE_H

#include <asm/tcti.h>

void tcti_prepare_syscall_handoff(struct pt_regs *regs);

#endif /* ORLIX_TCTI_ENGINE_H */
