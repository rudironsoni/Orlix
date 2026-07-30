// SPDX-License-Identifier: GPL-2.0-only
#include <asm/ptrace.h>
#include <linux/errno.h>
#include <linux/string.h>

#include "block_cache.h"
#include "decode_aarch64.h"
#include "gadget_program.h"
#include "native_capture.h"
#include "semantics.h"

#define ORLIX_TCTI_GADGET_DONE 1

#ifdef CONFIG_ORLIX_TCTI_KUNIT_TEST
static void (*orlix_tcti_pre_authorized_test_hook)(void *);
static void *orlix_tcti_pre_authorized_test_hook_data;

void orlix_tcti_gadget_program_set_pre_authorized_test_hook(
		void (*hook)(void *), void *data)
{
	orlix_tcti_pre_authorized_test_hook = hook;
	orlix_tcti_pre_authorized_test_hook_data = data;
}

static void orlix_tcti_gadget_program_run_pre_authorized_test_hook(void)
{
	void (*hook)(void *) = orlix_tcti_pre_authorized_test_hook;
	void *data = orlix_tcti_pre_authorized_test_hook_data;

	orlix_tcti_pre_authorized_test_hook = NULL;
	orlix_tcti_pre_authorized_test_hook_data = NULL;
	if (hook)
		hook(data);
}
#else
static void orlix_tcti_gadget_program_run_pre_authorized_test_hook(void)
{
}
#endif

static int orlix_tcti_gadget_execute_decoded(struct mm_struct *mm,
				       struct pt_regs *regs,
				       const struct orlix_tcti_gadget_word **cursor,
				       unsigned long *fault_address,
				       struct orlix_tcti_native_capture *capture)
{
	struct orlix_tcti_decoded_instruction decoded;
	int ret;

	memcpy(&decoded, *cursor, sizeof(decoded));
	*cursor += ORLIX_TCTI_DECODED_INSTRUCTION_WORDS;

	orlix_tcti_native_capture_before_decoded(capture, mm, regs, &decoded);
	ret = orlix_tcti_execute_decoded_semantics(mm, regs, &decoded, fault_address);
	if (ret)
		orlix_tcti_native_capture_fault(capture, &decoded, *fault_address, ret);
	else {
		orlix_tcti_native_capture_after_decoded(capture, mm, regs, &decoded);
	}
	return ret;
}

static int orlix_tcti_gadget_halt(struct mm_struct *mm, struct pt_regs *regs,
			    const struct orlix_tcti_gadget_word **cursor,
			    unsigned long *fault_address,
			    struct orlix_tcti_native_capture *capture)
{
	(void)mm;
	(void)regs;
	(void)cursor;
	(void)fault_address;
	(void)capture;
	return ORLIX_TCTI_GADGET_DONE;
}

int orlix_tcti_lower_decoded_instruction(
	const struct orlix_tcti_decoded_instruction *decoded,
	struct orlix_tcti_gadget_word *program,
	size_t capacity,
	size_t *word_count)
{
	int ret;

	if (!decoded || !program || !word_count)
		return -EINVAL;
	if (decoded->decode_class == ORLIX_TCTI_DECODE_UNSUPPORTED ||
	    decoded->decode_class == ORLIX_TCTI_DECODE_SVC ||
	    decoded->decode_class == ORLIX_TCTI_DECODE_BRK ||
	    decoded->decode_class == ORLIX_TCTI_DECODE_HLT)
		return -EOPNOTSUPP;

	*word_count = 0;
	ret = orlix_tcti_append_decoded_instruction(decoded, program, capacity,
					      word_count);
	if (ret)
		return ret;

	return 0;
}

int orlix_tcti_append_decoded_instruction(
	const struct orlix_tcti_decoded_instruction *decoded,
	struct orlix_tcti_gadget_word *program,
	size_t capacity,
	size_t *word_count)
{
	size_t start;
	size_t words;

	if (!decoded || !program || !word_count)
		return -EINVAL;
	if (decoded->decode_class == ORLIX_TCTI_DECODE_UNSUPPORTED ||
	    decoded->decode_class == ORLIX_TCTI_DECODE_SVC ||
	    decoded->decode_class == ORLIX_TCTI_DECODE_BRK)
		return -EOPNOTSUPP;

	start = *word_count ? *word_count - 1 : 0;
	words = start + 1 + ORLIX_TCTI_DECODED_INSTRUCTION_WORDS + 1;
	if (capacity < words)
		return -ENOSPC;

	program[start].value = (unsigned long)orlix_tcti_gadget_execute_decoded;
	memcpy(&program[start + 1], decoded, sizeof(*decoded));
	program[start + 1 + ORLIX_TCTI_DECODED_INSTRUCTION_WORDS].value =
		(unsigned long)orlix_tcti_gadget_halt;
	*word_count = words;

	return 0;
}

static int orlix_tcti_execute_gadget_program_checked(
	struct mm_struct *mm, struct pt_regs *regs,
	const struct orlix_tcti_gadget_word *program, size_t word_count,
	unsigned long *fault_address, bool authorize, u64 code_generation,
	bool *entry_valid, unsigned long *entry_pc, u32 *entry_instruction,
	struct orlix_tcti_native_capture *capture)
{
	const struct orlix_tcti_gadget_word *cursor = program;
	const struct orlix_tcti_gadget_word *end = program + word_count;

	if (!regs || !program || !word_count)
		return -EINVAL;
	if (authorize && !mm)
		return -EINVAL;

	while (cursor < end) {
		struct orlix_tcti_decoded_instruction entry_decoded;
		unsigned long candidate_pc = regs->pc;
		orlix_tcti_gadget_fn gadget;
		bool candidate = false;
		int ret;

		if (authorize) {
			orlix_tcti_gadget_program_run_pre_authorized_test_hook();
			if (code_generation != orlix_tcti_code_generation(mm))
				return -ESTALE;
		}
		gadget = (orlix_tcti_gadget_fn)cursor->value;
		cursor++;
		if (!gadget)
			return -EINVAL;
		if (gadget == orlix_tcti_gadget_execute_decoded && entry_valid &&
		    !*entry_valid) {
			memcpy(&entry_decoded, cursor, sizeof(entry_decoded));
			candidate = true;
		}

		ret = gadget(mm, regs, &cursor, fault_address, capture);
		if (candidate && ret != -ESTALE) {
			*entry_valid = true;
			*entry_pc = candidate_pc;
			*entry_instruction = entry_decoded.instruction;
		}
		if (ret == ORLIX_TCTI_GADGET_DONE)
			return 0;
		if (ret)
			return ret;
	}

	return -EINVAL;
}

int orlix_tcti_execute_gadget_program(struct mm_struct *mm, struct pt_regs *regs,
				const struct orlix_tcti_gadget_word *program,
				size_t word_count,
				unsigned long *fault_address)
{
	return orlix_tcti_execute_gadget_program_checked(mm, regs, program, word_count,
						  fault_address, false, 0,
					  NULL, NULL, NULL, NULL);
}

int orlix_tcti_execute_gadget_program_authorized(
	struct mm_struct *mm, struct pt_regs *regs,
	const struct orlix_tcti_gadget_word *program, size_t word_count,
	unsigned long *fault_address, u64 code_generation)
{
	return orlix_tcti_execute_gadget_program_checked(
		mm, regs, program, word_count, fault_address, true,
		code_generation, NULL, NULL, NULL, NULL);
}

int orlix_tcti_execute_gadget_program_authorized_observed(
	struct mm_struct *mm, struct pt_regs *regs,
	const struct orlix_tcti_gadget_word *program, size_t word_count,
	unsigned long *fault_address, u64 code_generation, bool *entry_valid,
	unsigned long *entry_pc, u32 *entry_instruction)
{
	if (!entry_valid || !entry_pc || !entry_instruction)
		return -EINVAL;
	return orlix_tcti_execute_gadget_program_checked(
		mm, regs, program, word_count, fault_address, true,
		code_generation, entry_valid, entry_pc, entry_instruction, NULL);
}

int orlix_tcti_execute_gadget_program_authorized_captured(
	struct mm_struct *mm, struct pt_regs *regs,
	const struct orlix_tcti_gadget_word *program, size_t word_count,
	unsigned long *fault_address, u64 code_generation, bool *entry_valid,
	unsigned long *entry_pc, u32 *entry_instruction,
	struct orlix_tcti_native_capture *capture)
{
	if (!entry_valid || !entry_pc || !entry_instruction || !capture)
		return -EINVAL;
	return orlix_tcti_execute_gadget_program_checked(
		mm, regs, program, word_count, fault_address, true, code_generation,
		entry_valid, entry_pc, entry_instruction, capture);
}
