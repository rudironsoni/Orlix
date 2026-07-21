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
 * Keep the Linux auxiliary-vector contract tied to that compile-time profile.
 * Optional extensions are added only after their complete EL0 instruction
 * families and architectural exception boundaries are covered by TCTI KUnit.
 */
#define ORLIX_EL0_ARCH_MAJOR	8
#define ORLIX_EL0_ARCH_MINOR	0
#define ORLIX_EL0_HWCAP		(HWCAP_FP | HWCAP_ASIMD | HWCAP_AES | \
				 HWCAP_PMULL | HWCAP_SHA1 | HWCAP_SHA2 | \
				 HWCAP_CRC32 | HWCAP_SHA3 | HWCAP_SM3 | \
				 HWCAP_SM4 | HWCAP_SHA512)
#define ORLIX_EL0_HWCAP2	0

#endif /* _ASM_ORLIX_ISA_H */
