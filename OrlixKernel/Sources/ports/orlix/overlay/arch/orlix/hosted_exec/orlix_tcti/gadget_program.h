/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_GADGET_PROGRAM_H
#define ORLIX_TCTI_GADGET_PROGRAM_H

#include <linux/kernel.h>
#include <linux/types.h>

struct mm_struct;
struct pt_regs;
struct orlix_tcti_native_capture;

#include "decode_aarch64.h"

struct orlix_tcti_gadget_word {
	unsigned long value;
};

#define ORLIX_TCTI_MICRO_OP_WORDS 4U
#define ORLIX_TCTI_SINGLE_INSTRUCTION_PROGRAM_WORDS \
	(ORLIX_TCTI_MICRO_OP_WORDS + 1U)
#define ORLIX_TCTI_PROGRAM_WORDS_FOR_INSTRUCTIONS(_count) \
	((_count) * ORLIX_TCTI_MICRO_OP_WORDS + 1U)

struct orlix_tcti_exec;

typedef int (*orlix_tcti_gadget_fn)(struct orlix_tcti_exec *exec);

enum orlix_tcti_gadget_program_kind {
	ORLIX_TCTI_GADGET_PROGRAM_GENERIC,
	ORLIX_TCTI_GADGET_PROGRAM_CRC32,
};

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
int orlix_tcti_execute_gadget_program_authorized_captured(
	struct mm_struct *mm, struct pt_regs *regs,
	const struct orlix_tcti_gadget_word *program, size_t word_count,
	unsigned long *fault_address, u64 code_generation, bool *entry_valid,
	unsigned long *entry_pc, u32 *entry_instruction,
	struct orlix_tcti_native_capture *capture);
enum orlix_tcti_gadget_program_kind orlix_tcti_gadget_program_first_kind(
	const struct orlix_tcti_gadget_word *program, size_t word_count);
u32 orlix_tcti_program_instruction_at(
	const struct orlix_tcti_gadget_word *program, size_t word_count,
	u32 index);
#ifdef CONFIG_ORLIX_TCTI_KUNIT_TEST
void orlix_tcti_gadget_program_set_pre_authorized_test_hook(
	void (*hook)(void *), void *data);
bool orlix_tcti_gadget_program_fuses_subs_b_cond(
	const struct orlix_tcti_gadget_word *program, size_t word_count);
#endif

#endif /* ORLIX_TCTI_GADGET_PROGRAM_H */
