// SPDX-License-Identifier: GPL-2.0-only
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/sched/task_stack.h>
#include <linux/signal.h>
#include <linux/smp.h>
#include <linux/string.h>
#include <asm/hosted_exec.h>
#include <asm/ptrace.h>
#include <asm/signal.h>
#include <asm/tcti.h>

#include "block_cache.h"
#include "decode_aarch64.h"
#include "engine.h"
#include "gadget_program.h"
#include "report.h"

static enum tcti_access
tcti_fault_access_for_decoded(const struct tcti_decoded_instruction *decoded)
{
	if (!decoded)
		return TCTI_ACCESS_FETCH;

	switch (decoded->decode_class) {
	case TCTI_DECODE_LOAD_STORE_PAIR:
	case TCTI_DECODE_LOAD_STORE_UNSIGNED_IMMEDIATE:
	case TCTI_DECODE_LOAD_STORE_SIGNED_IMMEDIATE:
	case TCTI_DECODE_LOAD_STORE_REGISTER_OFFSET:
	case TCTI_DECODE_LOAD_STORE_EXCLUSIVE:
		return decoded->load ? TCTI_ACCESS_READ : TCTI_ACCESS_WRITE;
	default:
		return TCTI_ACCESS_FETCH;
	}
}

static enum tcti_access
tcti_fault_access_for_program(const struct tcti_gadget_word *program,
			      size_t word_count)
{
	struct tcti_decoded_instruction decoded;

	if (!program || word_count <= TCTI_DECODED_INSTRUCTION_WORDS)
		return TCTI_ACCESS_FETCH;

	memcpy(&decoded, &program[1], sizeof(decoded));
	return tcti_fault_access_for_decoded(&decoded);
}

struct tcti_result tcti_resume_user(struct task_struct *task,
				    struct pt_regs *regs,
				    struct mm_struct *mm)
{
	struct tcti_result result = {
		.reason = TCTI_EXIT_TASK_EXIT,
		.status = -EINVAL,
	};

	if (!task || !regs || !mm)
		return result;

	for (;;) {
		struct tcti_gadget_word program[TCTI_SINGLE_INSTRUCTION_PROGRAM_WORDS];
		struct tcti_block *block;
		struct tcti_decoded_instruction decoded;
		unsigned long fault_address = regs->pc;
		unsigned long block_pc = regs->pc;
		u32 code_generation;
		size_t word_count = 0;
		u32 instruction;
		int ret;

		code_generation = tcti_code_generation(mm);
		block = tcti_block_cache_lookup(mm, block_pc, code_generation);
		if (block) {
			enum tcti_access block_fault_access;

			block_fault_access = tcti_fault_access_for_program(
				block->program, block->program_words);
			ret = tcti_execute_gadget_program(mm, regs, block->program,
							  block->program_words,
							  &fault_address);
			tcti_block_put(block);
			if (!ret)
				continue;

			if (ret == -EFAULT || ret == -EACCES) {
				result.reason = TCTI_EXIT_USER_FAULT;
				result.status = ret;
				result.fault_address = fault_address;
				result.fault_access = block_fault_access;
				result.pc = regs->pc;
				return result;
			}

			result.reason = TCTI_EXIT_UNSUPPORTED_INSTRUCTION;
			result.status = ret;
			result.pc = regs->pc;
			return result;
		}

		ret = tcti_fetch_instruction(mm, regs->pc, &instruction);
		if (ret) {
			result.reason = TCTI_EXIT_USER_FAULT;
			result.status = ret;
			result.fault_address = regs->pc;
			result.fault_access = TCTI_ACCESS_FETCH;
			result.pc = regs->pc;
			return result;
		}

		decoded = tcti_decode_aarch64(instruction);
		if (decoded.decode_class == TCTI_DECODE_SVC) {
			result.reason = TCTI_EXIT_SYSCALL;
			result.status = 0;
			result.pc = regs->pc;
			result.instruction = instruction;
			return result;
		}

		ret = tcti_lower_decoded_instruction(&decoded, program,
						     ARRAY_SIZE(program),
						     &word_count);
		if (ret) {
			result.reason = TCTI_EXIT_UNSUPPORTED_INSTRUCTION;
			result.status = ret;
			result.pc = regs->pc;
			result.instruction = instruction;
			return result;
		}

		ret = tcti_block_cache_insert(mm, block_pc, block_pc + sizeof(u32),
					      code_generation, 1, program,
					      word_count, &block);
		if (!ret && block) {
			ret = tcti_execute_gadget_program(mm, regs, block->program,
							  block->program_words,
							  &fault_address);
			tcti_block_put(block);
		} else {
			ret = tcti_execute_gadget_program(mm, regs, program,
							  word_count,
							  &fault_address);
		}
		if (!ret)
			continue;

		if (ret == -EFAULT || ret == -EACCES) {
			result.reason = TCTI_EXIT_USER_FAULT;
			result.status = ret;
			result.fault_address = fault_address;
			result.fault_access = tcti_fault_access_for_decoded(&decoded);
			result.pc = regs->pc;
			result.instruction = instruction;
			return result;
		}

		result.reason = TCTI_EXIT_UNSUPPORTED_INSTRUCTION;
		result.status = ret;
		result.pc = regs->pc;
		result.instruction = instruction;
		return result;
	}
}

void tcti_prepare_syscall_handoff(struct pt_regs *regs)
{
	regs->orig_x0 = regs->regs[0];
	regs->syscallno = regs->regs[8];
	regs->pc += sizeof(u32);
}

static void orlix_tcti_handle_syscall(struct pt_regs *regs)
{
	tcti_prepare_syscall_handoff(regs);
	orlix_syscall_dispatch(regs);
}

static bool orlix_tcti_handle_user_fault(struct pt_regs *regs,
					 const struct tcti_result *result)
{
	if (!result)
		return false;

	if (!tcti_handle_user_fault(regs, result->fault_address,
				    result->fault_access)) {
		orlix_exit_to_user_mode_work(regs);
		return true;
	}

	return false;
}

void __noreturn orlix_tcti_enter_user(struct pt_regs *regs)
{
	for (;;) {
		struct tcti_result result;

		result = tcti_resume_user(current, regs, current->mm);
		switch (result.reason) {
		case TCTI_EXIT_SYSCALL:
			tcti_report_syscall(current, regs, &result);
			orlix_tcti_handle_syscall(regs);
			regs = task_pt_regs(current);
			break;
		case TCTI_EXIT_USER_FAULT:
			if (orlix_tcti_handle_user_fault(regs, &result)) {
				regs = task_pt_regs(current);
				break;
			}
			tcti_report_exit(current, regs, &result);
			do_group_exit(SIGSEGV);
			break;
		case TCTI_EXIT_UNSUPPORTED_INSTRUCTION:
			tcti_report_unsupported(current, regs, &result);
			do_group_exit(SIGILL);
			break;
		case TCTI_EXIT_TASK_EXIT:
			do_group_exit(SIGKILL);
			break;
		default:
			tcti_report_exit(current, regs, &result);
			do_group_exit(SIGKILL);
			break;
		}
		cpu_relax();
	}
}
