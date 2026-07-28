/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_CRC32_H
#define ORLIX_TCTI_CRC32_H

struct orlix_tcti_decoded_instruction;
struct pt_regs;

int orlix_tcti_execute_crc32(struct pt_regs *regs,
			       const struct orlix_tcti_decoded_instruction *decoded);

#endif /* ORLIX_TCTI_CRC32_H */
