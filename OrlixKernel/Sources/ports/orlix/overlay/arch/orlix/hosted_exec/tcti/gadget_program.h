/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_GADGET_PROGRAM_H
#define ORLIX_TCTI_GADGET_PROGRAM_H

#include <linux/kernel.h>
#include <linux/types.h>

struct mm_struct;
struct pt_regs;

#include "decode_aarch64.h"

struct tcti_gadget_word {
	unsigned long value;
};

#define TCTI_DECODED_INSTRUCTION_WORDS \
	DIV_ROUND_UP(sizeof(struct tcti_decoded_instruction), \
		     sizeof(struct tcti_gadget_word))
#define TCTI_SINGLE_INSTRUCTION_PROGRAM_WORDS \
	(1U + TCTI_DECODED_INSTRUCTION_WORDS + 1U)
#define TCTI_PROGRAM_WORDS_FOR_INSTRUCTIONS(_count) \
	((_count) * (1U + TCTI_DECODED_INSTRUCTION_WORDS) + 1U)

typedef int (*tcti_gadget_fn)(struct mm_struct *mm, struct pt_regs *regs,
			      const struct tcti_gadget_word **cursor,
			      unsigned long *fault_address);

int tcti_lower_decoded_instruction(
	const struct tcti_decoded_instruction *decoded,
	struct tcti_gadget_word *program,
	size_t capacity,
	size_t *word_count);
int tcti_append_decoded_instruction(
	const struct tcti_decoded_instruction *decoded,
	struct tcti_gadget_word *program,
	size_t capacity,
	size_t *word_count);
int tcti_execute_gadget_program(struct mm_struct *mm, struct pt_regs *regs,
				const struct tcti_gadget_word *program,
				size_t word_count,
				unsigned long *fault_address);
int tcti_execute_gadget_program_authorized(
	struct mm_struct *mm, struct pt_regs *regs,
	const struct tcti_gadget_word *program, size_t word_count,
	unsigned long *fault_address, u64 code_generation);

#endif /* ORLIX_TCTI_GADGET_PROGRAM_H */
