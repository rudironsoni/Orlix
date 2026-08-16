// SPDX-License-Identifier: GPL-2.0-only
#include <asm/ptrace.h>
#include <linux/errno.h>
#include <linux/string.h>

#include "block_cache.h"
#include "crc32.h"
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

static int orlix_tcti_gadget_execute_crc32(struct mm_struct *mm,
				     struct pt_regs *regs,
				     const struct orlix_tcti_gadget_word **cursor,
				     unsigned long *fault_address,
				     struct orlix_tcti_native_capture *capture)
{
	struct orlix_tcti_decoded_instruction decoded;

	(void)mm;
	(void)fault_address;
	(void)capture;
	memcpy(&decoded, *cursor, sizeof(decoded));
	*cursor += ORLIX_TCTI_DECODED_INSTRUCTION_WORDS;
	return orlix_tcti_execute_crc32(regs, &decoded);
}

static int orlix_tcti_gadget_execute_flag_manipulation(
	struct mm_struct *mm, struct pt_regs *regs,
	const struct orlix_tcti_gadget_word **cursor,
	unsigned long *fault_address,
	struct orlix_tcti_native_capture *capture)
{
	struct orlix_tcti_decoded_instruction decoded;

	(void)mm;
	(void)fault_address;
	(void)capture;
	memcpy(&decoded, *cursor, sizeof(decoded));
	*cursor += ORLIX_TCTI_DECODED_INSTRUCTION_WORDS;
	return orlix_tcti_execute_flag_manipulation_semantics(regs, &decoded);
}

static int orlix_tcti_gadget_execute_memory_tagging(
	struct mm_struct *mm, struct pt_regs *regs,
	const struct orlix_tcti_gadget_word **cursor,
	unsigned long *fault_address,
	struct orlix_tcti_native_capture *capture);
static int orlix_tcti_gadget_execute_setgo(
	struct mm_struct *mm, struct pt_regs *regs,
	const struct orlix_tcti_gadget_word **cursor,
	unsigned long *fault_address,
	struct orlix_tcti_native_capture *capture);

static orlix_tcti_gadget_fn orlix_tcti_gadget_for_decoded(
	const struct orlix_tcti_decoded_instruction *decoded)
{
	if (decoded->decode_class == ORLIX_TCTI_DECODE_MEMORY_TAGGING)
		return orlix_tcti_gadget_execute_memory_tagging;
	if (decoded->decode_class == ORLIX_TCTI_DECODE_SET_GO)
		return orlix_tcti_gadget_execute_setgo;
	if (decoded->decode_class == ORLIX_TCTI_DECODE_FLAG_MANIPULATION)
		return orlix_tcti_gadget_execute_flag_manipulation;
	if (decoded->decode_class == ORLIX_TCTI_DECODE_DATA_PROCESSING_2SOURCE &&
	    (decoded->dp2_op == ORLIX_TCTI_DP2_CRC32 ||
	     decoded->dp2_op == ORLIX_TCTI_DP2_CRC32C))
		return orlix_tcti_gadget_execute_crc32;
	return orlix_tcti_gadget_execute_decoded;
}

static bool orlix_tcti_gadget_has_decoded_payload(orlix_tcti_gadget_fn gadget)
{
	return gadget == orlix_tcti_gadget_execute_decoded ||
		gadget == orlix_tcti_gadget_execute_crc32 ||
		gadget == orlix_tcti_gadget_execute_flag_manipulation ||
		gadget == orlix_tcti_gadget_execute_memory_tagging ||
		gadget == orlix_tcti_gadget_execute_setgo;
}

enum orlix_tcti_gadget_program_kind orlix_tcti_gadget_program_first_kind(
	const struct orlix_tcti_gadget_word *program, size_t word_count)
{
	if (!program || word_count < ORLIX_TCTI_SINGLE_INSTRUCTION_PROGRAM_WORDS)
		return ORLIX_TCTI_GADGET_PROGRAM_GENERIC;
	return program[0].value == (unsigned long)orlix_tcti_gadget_execute_crc32 ?
		ORLIX_TCTI_GADGET_PROGRAM_CRC32 : ORLIX_TCTI_GADGET_PROGRAM_GENERIC;
}

/*
 * MTE is deliberately not lowered through the generic decoded gadget.  Its
 * allocation-tag and fault contract crosses the Linux MM boundary, so this
 * fixed family gadget is the production dispatch point that keeps the TCTI
 * side limited to instruction execution.
 */
static int orlix_tcti_gadget_execute_memory_tagging(
	struct mm_struct *mm, struct pt_regs *regs,
	const struct orlix_tcti_gadget_word **cursor,
	unsigned long *fault_address,
	struct orlix_tcti_native_capture *capture)
{
	struct orlix_tcti_decoded_instruction decoded;
	int ret;

	memcpy(&decoded, *cursor, sizeof(decoded));
	*cursor += ORLIX_TCTI_DECODED_INSTRUCTION_WORDS;
	if (decoded.decode_class != ORLIX_TCTI_DECODE_MEMORY_TAGGING)
		return -EINVAL;
	orlix_tcti_native_capture_before_decoded(capture, mm, regs, &decoded);
	ret = orlix_tcti_execute_decoded_semantics(mm, regs, &decoded,
					      fault_address);
	if (ret)
		orlix_tcti_native_capture_fault(capture, &decoded,
						*fault_address, ret);
	else
		orlix_tcti_native_capture_after_decoded(capture, mm, regs,
							&decoded);
	return ret;
}

static int orlix_tcti_gadget_execute_setgo(
	struct mm_struct *mm, struct pt_regs *regs,
	const struct orlix_tcti_gadget_word **cursor,
	unsigned long *fault_address,
	struct orlix_tcti_native_capture *capture)
{
	*cursor += ORLIX_TCTI_DECODED_INSTRUCTION_WORDS;
	/* SETGO has no compatible official execution contract in the pinned source. */
	(void)mm;
	(void)regs;
	(void)fault_address;
	(void)capture;
	return -EOPNOTSUPP;
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

	program[start].value = (unsigned long)orlix_tcti_gadget_for_decoded(decoded);
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
		if (orlix_tcti_gadget_has_decoded_payload(gadget) && entry_valid &&
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
