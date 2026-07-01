// SPDX-License-Identifier: GPL-2.0-only
#include <linux/errno.h>
#include <linux/string.h>

#include "decode_aarch64.h"
#include "gadget_program.h"
#include "semantics.h"

#define TCTI_GADGET_DONE 1

static int tcti_gadget_execute_decoded(struct mm_struct *mm,
				       struct pt_regs *regs,
				       const struct tcti_gadget_word **cursor,
				       unsigned long *fault_address)
{
	struct tcti_decoded_instruction decoded;

	memcpy(&decoded, *cursor, sizeof(decoded));
	*cursor += TCTI_DECODED_INSTRUCTION_WORDS;

	return tcti_execute_decoded_semantics(mm, regs, &decoded,
					      fault_address);
}

static int tcti_gadget_halt(struct mm_struct *mm, struct pt_regs *regs,
			    const struct tcti_gadget_word **cursor,
			    unsigned long *fault_address)
{
	(void)mm;
	(void)regs;
	(void)cursor;
	(void)fault_address;
	return TCTI_GADGET_DONE;
}

int tcti_lower_decoded_instruction(
	const struct tcti_decoded_instruction *decoded,
	struct tcti_gadget_word *program,
	size_t capacity,
	size_t *word_count)
{
	size_t words = TCTI_SINGLE_INSTRUCTION_PROGRAM_WORDS;

	if (!decoded || !program || !word_count)
		return -EINVAL;
	if (decoded->decode_class == TCTI_DECODE_UNSUPPORTED ||
	    decoded->decode_class == TCTI_DECODE_SVC)
		return -EOPNOTSUPP;
	if (capacity < words)
		return -ENOSPC;

	memset(program, 0, words * sizeof(*program));
	program[0].value = (unsigned long)tcti_gadget_execute_decoded;
	memcpy(&program[1], decoded, sizeof(*decoded));
	program[1 + TCTI_DECODED_INSTRUCTION_WORDS].value =
		(unsigned long)tcti_gadget_halt;
	*word_count = words;
	return 0;
}

int tcti_execute_gadget_program(struct mm_struct *mm, struct pt_regs *regs,
				const struct tcti_gadget_word *program,
				size_t word_count,
				unsigned long *fault_address)
{
	const struct tcti_gadget_word *cursor = program;
	const struct tcti_gadget_word *end = program + word_count;

	if (!regs || !program || !word_count)
		return -EINVAL;

	while (cursor < end) {
		tcti_gadget_fn gadget = (tcti_gadget_fn)cursor->value;
		int ret;

		cursor++;
		if (!gadget)
			return -EINVAL;

		ret = gadget(mm, regs, &cursor, fault_address);
		if (ret == TCTI_GADGET_DONE)
			return 0;
		if (ret)
			return ret;
	}

	return -EINVAL;
}
