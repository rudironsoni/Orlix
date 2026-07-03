// SPDX-License-Identifier: GPL-2.0-only
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/elf.h>
#include <linux/mm.h>
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
#include <asm/unistd.h>

#include "block_cache.h"
#include "decode_aarch64.h"
#include "engine.h"
#include "gadget_program.h"
#include "report.h"

#define TCTI_ELF_IMAGE_SCAN_GRANULE (64UL * 1024UL)
#define TCTI_ELF_IMAGE_SCAN_LIMIT (16UL * 1024UL * 1024UL)
#define TCTI_MAX_DYNAMIC_ENTRIES 256
#define TCTI_MAX_RELA_ENTRIES 4096
#ifndef R_AARCH64_RELATIVE
#define R_AARCH64_RELATIVE 1027
#endif

static bool tcti_address_has_vma(struct mm_struct *mm, unsigned long address,
				 enum tcti_access access)
{
	struct vm_area_struct *vma;
	vm_flags_t required;
	bool valid = false;

	switch (access) {
	case TCTI_ACCESS_READ:
		required = VM_READ;
		break;
	case TCTI_ACCESS_WRITE:
		required = VM_WRITE;
		break;
	case TCTI_ACCESS_FETCH:
		required = VM_EXEC;
		break;
	default:
		return false;
	}

	mmap_read_lock(mm);
	vma = find_vma(mm, address);
	if (vma && address >= vma->vm_start && address < vma->vm_end &&
	    (vma->vm_flags & required))
		valid = true;
	mmap_read_unlock(mm);

	return valid;
}

static int tcti_read_user_data_faulting(struct pt_regs *regs,
					struct mm_struct *mm,
					unsigned long address,
					void *buffer, size_t size)
{
	int ret;
	int fault_ret;

	ret = tcti_read_user_data(mm, address, buffer, size);
	if (ret != -EFAULT && ret != -EACCES)
		return ret;
	if (!tcti_address_has_vma(mm, address, TCTI_ACCESS_READ)) {
		pr_info("Orlix TCTI: static PIE read has no readable VMA task=%s pid=%d addr=%#lx size=%zu ret=%d\n",
			current->comm, task_pid_nr(current), address, size, ret);
		return ret;
	}

	fault_ret = tcti_handle_user_fault(regs, address, TCTI_ACCESS_READ);
	if (fault_ret) {
		pr_info("Orlix TCTI: static PIE read fault-in failed task=%s pid=%d addr=%#lx size=%zu ret=%d fault_ret=%d\n",
			current->comm, task_pid_nr(current), address, size, ret,
			fault_ret);
		return fault_ret;
	}

	ret = tcti_read_user_data(mm, address, buffer, size);
	if (ret)
		pr_info("Orlix TCTI: static PIE read retry failed task=%s pid=%d addr=%#lx size=%zu ret=%d\n",
			current->comm, task_pid_nr(current), address, size, ret);

	return ret;
}

static int tcti_write_user_data_faulting(struct pt_regs *regs,
					 struct mm_struct *mm,
					 unsigned long address,
					 const void *buffer, size_t size)
{
	int ret;
	int fault_ret;

	ret = tcti_write_user_data(mm, address, buffer, size);
	if (ret != -EFAULT && ret != -EACCES)
		return ret;
	if (!tcti_address_has_vma(mm, address, TCTI_ACCESS_WRITE)) {
		pr_info("Orlix TCTI: static PIE write has no writable VMA task=%s pid=%d addr=%#lx size=%zu ret=%d\n",
			current->comm, task_pid_nr(current), address, size, ret);
		return ret;
	}

	fault_ret = tcti_handle_user_fault(regs, address, TCTI_ACCESS_WRITE);
	if (fault_ret) {
		pr_info("Orlix TCTI: static PIE write fault-in failed task=%s pid=%d addr=%#lx size=%zu ret=%d fault_ret=%d\n",
			current->comm, task_pid_nr(current), address, size, ret,
			fault_ret);
		return fault_ret;
	}

	ret = tcti_write_user_data(mm, address, buffer, size);
	if (ret)
		pr_info("Orlix TCTI: static PIE write retry failed task=%s pid=%d addr=%#lx size=%zu ret=%d\n",
			current->comm, task_pid_nr(current), address, size, ret);

	return ret;
}

static int tcti_find_current_elf_base(struct mm_struct *mm,
				      struct pt_regs *regs,
				      unsigned long *base)
{
	unsigned long candidate;
	unsigned long limit;
	Elf64_Ehdr ehdr;

	if (!mm || !regs || !base)
		return -EINVAL;

	candidate = regs->pc & ~(TCTI_ELF_IMAGE_SCAN_GRANULE - 1);
	limit = candidate > TCTI_ELF_IMAGE_SCAN_LIMIT ?
		candidate - TCTI_ELF_IMAGE_SCAN_LIMIT : 0;

	for (;;) {
		if (!tcti_read_user_data_faulting(regs, mm, candidate, &ehdr,
						  sizeof(ehdr)) &&
		    memcmp(ehdr.e_ident, ELFMAG, SELFMAG) == 0 &&
		    ehdr.e_ident[EI_CLASS] == ELFCLASS64 &&
		    ehdr.e_ident[EI_DATA] == ELFDATA2LSB &&
		    ehdr.e_machine == EM_AARCH64 &&
		    ehdr.e_type == ET_DYN) {
			*base = candidate;
			return 0;
		}
		if (candidate <= limit || candidate < TCTI_ELF_IMAGE_SCAN_GRANULE)
			break;
		candidate -= TCTI_ELF_IMAGE_SCAN_GRANULE;
	}

	return -ENOEXEC;
}

static int tcti_read_dynamic_value(struct mm_struct *mm, unsigned long base,
				   struct pt_regs *regs,
				   const Elf64_Phdr *phdr, Elf64_Sxword tag,
				   Elf64_Xword *value)
{
	unsigned long dynamic = base + phdr->p_vaddr;
	size_t entries = phdr->p_memsz / sizeof(Elf64_Dyn);
	size_t index;

	if (!value || !entries || entries > TCTI_MAX_DYNAMIC_ENTRIES)
		return -ENOEXEC;

	for (index = 0; index < entries; index++) {
		Elf64_Dyn dyn;
		int ret;

		ret = tcti_read_user_data_faulting(regs, mm,
						   dynamic + index * sizeof(dyn),
						   &dyn, sizeof(dyn));
		if (ret)
			return ret;
		if (dyn.d_tag == DT_NULL)
			return -ENOENT;
		if (dyn.d_tag == tag) {
			*value = dyn.d_un.d_val;
			return 0;
		}
	}

	return -ENOENT;
}

static int tcti_apply_relative_relocations(struct mm_struct *mm,
					   struct pt_regs *regs,
					   unsigned long base,
					   const Elf64_Phdr *dynamic_phdr)
{
	Elf64_Xword rela_vaddr = 0;
	Elf64_Xword rela_size = 0;
	Elf64_Xword rela_entry_size = sizeof(Elf64_Rela);
	size_t count;
	size_t index;
	int ret;

	ret = tcti_read_dynamic_value(mm, base, regs, dynamic_phdr, DT_RELA,
				      &rela_vaddr);
	if (ret == -ENOENT)
		return 0;
	if (ret)
		return ret;
	ret = tcti_read_dynamic_value(mm, base, regs, dynamic_phdr, DT_RELASZ,
				      &rela_size);
	if (ret)
		return ret;
	ret = tcti_read_dynamic_value(mm, base, regs, dynamic_phdr, DT_RELAENT,
				      &rela_entry_size);
	if (ret == -ENOENT)
		rela_entry_size = sizeof(Elf64_Rela);
	else if (ret)
		return ret;
	if (rela_entry_size != sizeof(Elf64_Rela) ||
	    rela_size % sizeof(Elf64_Rela))
		return -ENOEXEC;

	count = rela_size / sizeof(Elf64_Rela);
	if (count > TCTI_MAX_RELA_ENTRIES)
		return -E2BIG;

	for (index = 0; index < count; index++) {
		Elf64_Rela rela;
		unsigned long target;
		unsigned long value;
		unsigned long current_value = 0;

		ret = tcti_read_user_data_faulting(regs, mm,
						   base + rela_vaddr +
						   index * sizeof(rela),
						   &rela, sizeof(rela));
		if (ret)
			return ret;
		if (ELF64_R_TYPE(rela.r_info) != R_AARCH64_RELATIVE)
			return -EOPNOTSUPP;

		target = base + rela.r_offset;
		value = base + rela.r_addend;
		ret = tcti_read_user_data_faulting(regs, mm, target,
						   &current_value,
						   sizeof(current_value));
		if (ret)
			return ret;
		if (current_value == value)
			continue;
		if (current_value != 0 && current_value != rela.r_addend)
			return -EEXIST;
		ret = tcti_write_user_data_faulting(regs, mm, target, &value,
						    sizeof(value));
		if (ret)
			return ret;
	}

	return 0;
}

static int tcti_apply_static_pie_relative_relocations(struct task_struct *task,
						      struct pt_regs *regs,
						      struct mm_struct *mm,
						      unsigned long *applied_base)
{
	unsigned long base;
	Elf64_Ehdr ehdr;
	size_t index;
	int ret;

	ret = tcti_find_current_elf_base(mm, regs, &base);
	if (ret == -ENOEXEC)
		return 0;
	if (ret)
		return ret;
	if (mm->context.orlix_tcti_static_pie_base == base)
		return 0;
	if (applied_base && *applied_base == base)
		return 0;

	ret = tcti_read_user_data_faulting(regs, mm, base, &ehdr, sizeof(ehdr));
	if (ret)
		return ret;
	if (ehdr.e_phentsize != sizeof(Elf64_Phdr) || !ehdr.e_phnum)
		return -ENOEXEC;
	pr_info("Orlix TCTI: static PIE image task=%s pid=%d pc=%#llx base=%#lx entry=%#llx phoff=%#llx phnum=%u phentsize=%u\n",
		task->comm, task_pid_nr(task), regs->pc, base,
		(unsigned long long)ehdr.e_entry,
		(unsigned long long)ehdr.e_phoff, ehdr.e_phnum,
		ehdr.e_phentsize);

	for (index = 0; index < ehdr.e_phnum; index++) {
		Elf64_Phdr phdr;

		ret = tcti_read_user_data_faulting(regs, mm,
						   base + ehdr.e_phoff +
						   index * sizeof(phdr),
						   &phdr, sizeof(phdr));
		if (ret)
			return ret;
		if (phdr.p_type != PT_DYNAMIC)
			continue;

		pr_info("Orlix TCTI: static PIE dynamic task=%s pid=%d base=%#lx index=%zu vaddr=%#llx memsz=%#llx filesz=%#llx flags=%#x\n",
			task->comm, task_pid_nr(task), base, index,
			(unsigned long long)phdr.p_vaddr,
			(unsigned long long)phdr.p_memsz,
			(unsigned long long)phdr.p_filesz, phdr.p_flags);
		ret = tcti_apply_relative_relocations(mm, regs, base, &phdr);
		if (ret)
			return ret;
		pr_info_once("Orlix TCTI: applied static PIE R_AARCH64_RELATIVE relocations task=%s pid=%d base=%#lx\n",
			     task->comm, task_pid_nr(task), base);
		mm->context.orlix_tcti_static_pie_base = base;
		if (applied_base)
			*applied_base = base;
		return 0;
	}

	return 0;
}

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
			u32 block_instruction = 0;

			block_fault_access = tcti_fault_access_for_program(
				block->program, block->program_words);
			if (block->program_words > TCTI_DECODED_INSTRUCTION_WORDS) {
				struct tcti_decoded_instruction block_decoded;

				memcpy(&block_decoded, &block->program[1],
				       sizeof(block_decoded));
				block_instruction = block_decoded.instruction;
			}
			ret = tcti_execute_gadget_program(mm, regs, block->program,
							  block->program_words,
							  &fault_address);
			tcti_block_put(block);
			if (!ret)
				continue;

			if (ret == -EFAULT || ret == -EACCES) {
				pr_info("Orlix TCTI: cached block fault task=%s pid=%d pc=%#llx ret=%d fault=%#lx insn=%#x code_generation=%u words=%u\n",
					task->comm, task_pid_nr(task), regs->pc,
					ret, fault_address, block_instruction,
					code_generation, block->program_words);
				result.reason = TCTI_EXIT_USER_FAULT;
				result.status = ret;
				result.fault_address = fault_address;
				result.fault_access = block_fault_access;
				result.pc = regs->pc;
				result.instruction = block_instruction;
				return result;
			}

			result.reason = TCTI_EXIT_UNSUPPORTED_INSTRUCTION;
			result.status = ret;
			result.pc = regs->pc;
			result.instruction = block_instruction;
			return result;
		}

		ret = tcti_fetch_instruction(mm, regs->pc, &instruction);
		if (ret) {
			pr_info("Orlix TCTI: fetch failed task=%s pid=%d pc=%#llx ret=%d\n",
				task->comm, task_pid_nr(task), regs->pc, ret);
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

bool tcti_prepare_successful_execve_return(struct pt_regs *regs)
{
	if (!regs)
		return false;
	if (user_mode(regs)) {
		regs->syscallno = NO_SYSCALL;
		return false;
	}

	regs->pstate &= ~PSR_MODE_MASK;
	regs->pstate |= PSR_MODE_EL0t;
	regs->syscallno = NO_SYSCALL;
	return true;
}

static void orlix_tcti_handle_syscall(struct pt_regs *regs)
{
	tcti_prepare_syscall_handoff(regs);
	orlix_syscall_dispatch(regs);
}

static void
orlix_tcti_apply_static_pie_relocations_or_exit(struct pt_regs *regs,
						unsigned long *applied_base)
{
	int relocation_ret;

	relocation_ret = tcti_apply_static_pie_relative_relocations(
		current, regs, current->mm, applied_base);
	if (relocation_ret) {
		struct tcti_result result = {
			.reason = TCTI_EXIT_USER_FAULT,
			.status = relocation_ret,
			.fault_address = regs->pc,
			.fault_access = TCTI_ACCESS_READ,
			.pc = regs->pc,
		};

		tcti_report_exit(current, regs, &result);
		do_group_exit(SIGSEGV);
	}
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
	unsigned long applied_static_pie_base = 0;

	orlix_tcti_apply_static_pie_relocations_or_exit(
		regs, &applied_static_pie_base);

	for (;;) {
		struct tcti_result result;

		result = tcti_resume_user(current, regs, current->mm);
		switch (result.reason) {
		case TCTI_EXIT_SYSCALL:
		{
			bool syscall_was_execve = regs->regs[8] == __NR_execve;
			unsigned long syscall_return_pc = result.pc + sizeof(u32);

			tcti_report_syscall(current, regs, &result);
			orlix_tcti_handle_syscall(regs);
			regs = task_pt_regs(current);
			if (syscall_was_execve &&
			    (regs->pc != syscall_return_pc ||
			     !IS_ERR_VALUE(regs->regs[0]))) {
				applied_static_pie_base = 0;
				if (current->mm)
					current->mm->context.orlix_tcti_static_pie_base = 0;
					if (tcti_prepare_successful_execve_return(regs))
						pr_info("Orlix TCTI: restored EL0 pstate after execve task=%s pid=%d pc=%#llx sp=%#llx pstate=%#llx\n",
							current->comm, task_pid_nr(current),
							regs->pc, regs->sp, regs->pstate);
				}
				orlix_tcti_apply_static_pie_relocations_or_exit(
					regs, &applied_static_pie_base);
			break;
		}
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
