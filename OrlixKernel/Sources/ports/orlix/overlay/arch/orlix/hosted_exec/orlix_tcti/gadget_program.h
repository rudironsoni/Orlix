/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_GADGET_PROGRAM_H
#define ORLIX_TCTI_GADGET_PROGRAM_H

#include <linux/kernel.h>
#include <linux/types.h>

struct mm_struct;
struct pt_regs;

#include "decode_aarch64.h"

struct orlix_tcti_gadget_word {
	unsigned long value;
};

#define ORLIX_TCTI_DECODED_INSTRUCTION_WORDS \
	DIV_ROUND_UP(sizeof(struct orlix_tcti_decoded_instruction), \
		     sizeof(struct orlix_tcti_gadget_word))
#define ORLIX_TCTI_SINGLE_INSTRUCTION_PROGRAM_WORDS \
	(1U + ORLIX_TCTI_DECODED_INSTRUCTION_WORDS + 1U)
#define ORLIX_TCTI_PROGRAM_WORDS_FOR_INSTRUCTIONS(_count) \
	((_count) * (1U + ORLIX_TCTI_DECODED_INSTRUCTION_WORDS) + 1U)

typedef int (*orlix_tcti_gadget_fn)(struct mm_struct *mm, struct pt_regs *regs,
			      const struct orlix_tcti_gadget_word **cursor,
			      unsigned long *fault_address);

int orlix_tcti_lower_decoded_instruction(
	const struct orlix_tcti_decoded_instruction *decoded,
	struct orlix_tcti_gadget_word *program,
	size_t capacity,
	size_t *word_count);
int orlix_tcti_append_decoded_instruction(
	const struct orlix_tcti_decoded_instruction *decoded,
	struct orlix_tcti_gadget_word *program,
	size_t capacity,
	size_t *word_count);
int orlix_tcti_execute_gadget_program(struct mm_struct *mm, struct pt_regs *regs,
				const struct orlix_tcti_gadget_word *program,
				size_t word_count,
				unsigned long *fault_address);
int orlix_tcti_execute_gadget_program_authorized(
	struct mm_struct *mm, struct pt_regs *regs,
	const struct orlix_tcti_gadget_word *program, size_t word_count,
	unsigned long *fault_address, u64 code_generation);
int orlix_tcti_execute_gadget_program_authorized_observed(
	struct mm_struct *mm, struct pt_regs *regs,
	const struct orlix_tcti_gadget_word *program, size_t word_count,
	unsigned long *fault_address, u64 code_generation, bool *entry_valid,
	unsigned long *entry_pc, u32 *entry_instruction);
#ifdef CONFIG_ORLIX_TCTI_KUNIT_TEST
void orlix_tcti_gadget_program_set_pre_authorized_test_hook(
	void (*hook)(void *), void *data);
bool orlix_tcti_gadget_program_uses_variable_shift_gadget(
	const struct orlix_tcti_gadget_word *program, size_t word_count);
#endif

#endif /* ORLIX_TCTI_GADGET_PROGRAM_H */
