/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _ASM_ORLIX_ISA_H
#define _ASM_ORLIX_ISA_H

#include <uapi/asm/hwcap.h>

/*
 * The Orlix userspace toolchain targets generic AArch64:
 *   -target-feature +v8a
 *   -target-feature +fp-armv8
 *   -target-feature +neon
 *
 * Linux auxiliary-vector bits are advertised only after every pinned source
 * leaf in the corresponding feature cohort resolves through the production
 * proof registry. The complete 4,350-leaf TCTI target remains independent of
 * this runtime-advertisement projection.
 */
#define ORLIX_EL0_ARCH_MAJOR	8
#define ORLIX_EL0_ARCH_MINOR	0
#define ORLIX_EL0_HWCAP		0
#define ORLIX_EL0_HWCAP2	0

#endif /* _ASM_ORLIX_ISA_H */
