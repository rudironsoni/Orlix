// SPDX-License-Identifier: GPL-2.0-only
#include <linux/atomic.h>
#include <linux/kernel.h>
#include <linux/auxvec.h>
#include <linux/err.h>
#include <linux/elf.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/sched/task_stack.h>
#include <linux/signal.h>
#include <linux/smp.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/utsname.h>
#include <asm/hosted_exec.h>
#include <asm/ioctls.h>
#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/signal.h>
#include <asm/termios.h>
#include <asm/tcti.h>
#include <asm/unistd.h>

#include "block_cache.h"
#include "decode_aarch64.h"
#include "engine.h"
#include "execve_binfmt_smoke.h"
#include "gadget_program.h"
#include "report.h"
#include "syscall_dispatch_smoke.h"

#define TCTI_ELF_IMAGE_SCAN_GRANULE (64UL * 1024UL)
#define TCTI_ELF_IMAGE_SCAN_LIMIT (16UL * 1024UL * 1024UL)
#define TCTI_MAX_DYNAMIC_ENTRIES 256
#define TCTI_MAX_RELA_ENTRIES 4096
#define TCTI_STATIC_PIE_TLS_TCB_OFFSET 0x78UL
#define TCTI_PROGRESS_REPORT_INTERVAL 100000UL
#define TCTI_MAX_BLOCK_INSTRUCTIONS 32U
#define TCTI_LOCAL_HOT_BLOCKS 16U
#define TCTI_BLOCK_PROGRAM_WORDS \
	TCTI_PROGRAM_WORDS_FOR_INSTRUCTIONS(TCTI_MAX_BLOCK_INSTRUCTIONS)

struct tcti_hot_block {
	unsigned long guest_pc;
	u32 code_generation;
	struct tcti_block *block;
};

static atomic_t tcti_block_trace_budget = ATOMIC_INIT(64);
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

static int tcti_read_user_data_faulting_impl(struct pt_regs *regs,
					     struct mm_struct *mm,
					     unsigned long address,
					     void *buffer, size_t size,
					     bool log_missing_vma)
{
	int ret;
	int fault_ret;

	ret = tcti_read_user_data(mm, address, buffer, size);
	if (ret != -EFAULT && ret != -EACCES)
		return ret;
	if (!tcti_address_has_vma(mm, address, TCTI_ACCESS_READ)) {
		if (log_missing_vma)
			pr_info("Orlix TCTI: static PIE read has no readable VMA task=%s pid=%d addr=%#lx size=%zu ret=%d\n",
				current->comm, task_pid_nr(current), address,
				size, ret);
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

static int tcti_read_user_data_faulting(struct pt_regs *regs,
					struct mm_struct *mm,
					unsigned long address,
					void *buffer, size_t size)
{
	return tcti_read_user_data_faulting_impl(regs, mm, address, buffer,
						 size, true);
}

static int tcti_scan_user_data_faulting(struct pt_regs *regs,
					struct mm_struct *mm,
					unsigned long address,
					void *buffer, size_t size)
{
	return tcti_read_user_data_faulting_impl(regs, mm, address, buffer,
						 size, false);
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
		if (!tcti_scan_user_data_faulting(regs, mm, candidate, &ehdr,
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

static unsigned long tcti_saved_auxv_value(struct mm_struct *mm,
					   unsigned long type)
{
	size_t index;

	if (!mm)
		return 0;

	for (index = 0; index + 1 < ARRAY_SIZE(mm->saved_auxv); index += 2) {
		unsigned long tag = mm->saved_auxv[index];

		if (tag == AT_NULL)
			break;
		if (tag == type)
			return mm->saved_auxv[index + 1];
	}

	return 0;
}

static bool tcti_elf_base_is_main_executable(struct mm_struct *mm,
					     unsigned long base,
					     const Elf64_Ehdr *ehdr)
{
	unsigned long at_phdr;

	if (!mm || !ehdr)
		return false;

	at_phdr = tcti_saved_auxv_value(mm, AT_PHDR);
	if (!at_phdr)
		return true;
	if (base > ULONG_MAX - ehdr->e_phoff)
		return false;

	return at_phdr == base + ehdr->e_phoff;
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

bool tcti_static_pie_initial_tls(unsigned long base, const Elf64_Phdr *phdr,
				 unsigned long *initial_tls)
{
	unsigned long tls;

	if (!phdr || phdr->p_type != PT_TLS || !initial_tls)
		return false;
	if (!phdr->p_memsz)
		return false;
	if (phdr->p_vaddr > ULONG_MAX - TCTI_STATIC_PIE_TLS_TCB_OFFSET)
		return false;
	if (base > ULONG_MAX - phdr->p_vaddr - TCTI_STATIC_PIE_TLS_TCB_OFFSET)
		return false;

	tls = base + phdr->p_vaddr + TCTI_STATIC_PIE_TLS_TCB_OFFSET;
	if (tls >= TASK_SIZE)
		return false;

	*initial_tls = tls;
	return true;
}

static int tcti_apply_static_pie_relative_relocations(struct task_struct *task,
						      struct pt_regs *regs,
						      struct mm_struct *mm,
						      unsigned long *applied_base)
{
	Elf64_Phdr dynamic_phdr = { 0 };
	unsigned long initial_tls = 0;
	unsigned long base;
	Elf64_Ehdr ehdr;
	bool found_dynamic = false;
	bool found_initial_tls = false;
	size_t index;
	int ret;

	ret = tcti_find_current_elf_base(mm, regs, &base);
	if (ret == -ENOEXEC)
		return 0;
	if (ret)
		return ret;
	if (base == tcti_saved_auxv_value(mm, AT_BASE)) {
		pr_info_once("Orlix TCTI: leaving PT_INTERP image self-relocation to ld.so task=%s pid=%d base=%#lx\n",
			     task->comm, task_pid_nr(task), base);
		if (applied_base)
			*applied_base = base;
		return 0;
	}
	if (mm->context.orlix_tcti_static_pie_base == base)
		return 0;
	if (applied_base && *applied_base == base)
		return 0;

	ret = tcti_read_user_data_faulting(regs, mm, base, &ehdr, sizeof(ehdr));
	if (ret)
		return ret;
	if (ehdr.e_phentsize != sizeof(Elf64_Phdr) || !ehdr.e_phnum)
		return -ENOEXEC;
	if (!tcti_elf_base_is_main_executable(mm, base, &ehdr)) {
		pr_info_once("Orlix TCTI: leaving shared object relocation to rtld task=%s pid=%d base=%#lx at_phdr=%#lx phoff=%#llx\n",
			     task->comm, task_pid_nr(task), base,
			     tcti_saved_auxv_value(mm, AT_PHDR),
			     (unsigned long long)ehdr.e_phoff);
		if (applied_base)
			*applied_base = base;
		return 0;
	}
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
		if (tcti_static_pie_initial_tls(base, &phdr, &initial_tls))
			found_initial_tls = true;
		if (phdr.p_type == PT_DYNAMIC) {
			dynamic_phdr = phdr;
			found_dynamic = true;
		}
	}

	if (found_initial_tls) {
#if defined(ORLIX_APP_HOSTED_BOOT)
		unsigned long prepared_tls;

		prepared_tls = orlix_hosted_prepare_user_entry(initial_tls);
		pr_info("Orlix TCTI: static PIE initial TLS task=%s pid=%d base=%#lx tls=%#lx prepared=%#lx\n",
			task->comm, task_pid_nr(task), base, initial_tls,
			prepared_tls);
#endif
	}

	if (!found_dynamic)
		return 0;

	pr_info("Orlix TCTI: static PIE dynamic task=%s pid=%d base=%#lx vaddr=%#llx memsz=%#llx filesz=%#llx flags=%#x\n",
		task->comm, task_pid_nr(task), base,
		(unsigned long long)dynamic_phdr.p_vaddr,
		(unsigned long long)dynamic_phdr.p_memsz,
		(unsigned long long)dynamic_phdr.p_filesz, dynamic_phdr.p_flags);
	ret = tcti_apply_relative_relocations(mm, regs, base, &dynamic_phdr);
	if (ret)
		return ret;
	pr_info_once("Orlix TCTI: applied static PIE R_AARCH64_RELATIVE relocations task=%s pid=%d base=%#lx\n",
		     task->comm, task_pid_nr(task), base);
	mm->context.orlix_tcti_static_pie_base = base;
	if (applied_base)
		*applied_base = base;
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

static bool tcti_decode_class_ends_block(enum tcti_decode_class decode_class)
{
	switch (decode_class) {
	case TCTI_DECODE_SVC:
	case TCTI_DECODE_UNCONDITIONAL_BRANCH_IMMEDIATE:
	case TCTI_DECODE_UNCONDITIONAL_BRANCH_REGISTER:
	case TCTI_DECODE_COMPARE_BRANCH_IMMEDIATE:
	case TCTI_DECODE_TEST_BRANCH_IMMEDIATE:
	case TCTI_DECODE_CONDITIONAL_BRANCH_IMMEDIATE:
		return true;
	default:
		return false;
	}
}

static enum tcti_access
tcti_fault_access_for_program_pc(const struct tcti_block *block,
				 unsigned long pc)
{
	unsigned long index;
	size_t offset;
	struct tcti_decoded_instruction decoded;

	if (!block || pc < block->guest_start_pc)
		return TCTI_ACCESS_FETCH;
	index = (pc - block->guest_start_pc) / sizeof(u32);
	if (index >= block->instruction_count)
		return TCTI_ACCESS_FETCH;

	offset = index * (1U + TCTI_DECODED_INSTRUCTION_WORDS);
	if (offset + 1 + TCTI_DECODED_INSTRUCTION_WORDS > block->program_words)
		return TCTI_ACCESS_FETCH;

	memcpy(&decoded, &block->program[offset + 1], sizeof(decoded));
	return tcti_fault_access_for_decoded(&decoded);
}

static int tcti_build_straight_line_block(struct mm_struct *mm,
					  unsigned long start_pc,
					  struct tcti_gadget_word *program,
					  size_t capacity,
					  size_t *word_count,
					  u32 *instruction_count,
					  u32 *first_instruction,
					  bool *first_is_svc)
{
	u32 count;
	int ret;

	if (!mm || !program || !word_count || !instruction_count)
		return -EINVAL;

	*word_count = 0;
	*instruction_count = 0;
	if (first_instruction)
		*first_instruction = 0;
	if (first_is_svc)
		*first_is_svc = false;

	for (count = 0; count < TCTI_MAX_BLOCK_INSTRUCTIONS; count++) {
		struct tcti_decoded_instruction decoded;
		unsigned long pc = start_pc + count * sizeof(u32);
		u32 instruction = 0;

		ret = tcti_fetch_instruction(mm, pc, &instruction);
		if (ret)
			return count ? 0 : ret;

		if (!count && first_instruction)
			*first_instruction = instruction;

		decoded = tcti_decode_aarch64(instruction);
		if (decoded.decode_class == TCTI_DECODE_SVC) {
			if (!count && first_is_svc)
				*first_is_svc = true;
			return count ? 0 : -EINTR;
		}
		if (decoded.decode_class == TCTI_DECODE_UNSUPPORTED)
			return count ? 0 : -EOPNOTSUPP;

		ret = tcti_append_decoded_instruction(&decoded, program,
						      capacity, word_count);
		if (ret)
			return count ? 0 : ret;
		(*instruction_count)++;

		if (tcti_decode_class_ends_block(decoded.decode_class))
			break;
		if (((pc + sizeof(u32)) & PAGE_MASK) != (pc & PAGE_MASK))
			break;
	}

	return *instruction_count ? 0 : -EOPNOTSUPP;
}

static void tcti_hot_blocks_release(struct tcti_hot_block *hot_blocks)
{
	u32 i;

	for (i = 0; i < TCTI_LOCAL_HOT_BLOCKS; i++) {
		if (!hot_blocks[i].block)
			continue;
		tcti_block_put(hot_blocks[i].block);
		hot_blocks[i].block = NULL;
	}
}

static struct tcti_block *
tcti_hot_blocks_lookup(struct tcti_hot_block *hot_blocks,
		       unsigned long guest_pc, u32 code_generation)
{
	u32 i;

	for (i = 0; i < TCTI_LOCAL_HOT_BLOCKS; i++) {
		if (hot_blocks[i].block &&
		    hot_blocks[i].guest_pc == guest_pc &&
		    hot_blocks[i].code_generation == code_generation)
			return hot_blocks[i].block;
	}

	return NULL;
}

static void tcti_hot_blocks_remember(struct tcti_hot_block *hot_blocks,
				     u32 *cursor, struct tcti_block *block)
{
	struct tcti_hot_block *slot;

	if (!hot_blocks || !cursor || !block)
		return;

	slot = &hot_blocks[*cursor % TCTI_LOCAL_HOT_BLOCKS];
	(*cursor)++;

	if (slot->block == block) {
		slot->guest_pc = block->guest_start_pc;
		slot->code_generation = block->code_generation;
		return;
	}

	if (slot->block)
		tcti_block_put(slot->block);

	refcount_inc(&block->refs);
	slot->guest_pc = block->guest_start_pc;
	slot->code_generation = block->code_generation;
	slot->block = block;
}

struct tcti_result tcti_resume_user(struct task_struct *task,
				    struct pt_regs *regs,
				    struct mm_struct *mm)
{
	unsigned long long instruction_count = 0;
	struct tcti_hot_block hot_blocks[TCTI_LOCAL_HOT_BLOCKS] = {};
	u32 hot_block_cursor = 0;
	struct tcti_result result = {
		.reason = TCTI_EXIT_TASK_EXIT,
		.status = -EINVAL,
	};

	if (!task || !regs || !mm)
		return result;

	for (;;) {
		struct tcti_gadget_word program[TCTI_BLOCK_PROGRAM_WORDS];
		struct tcti_block *block;
		struct tcti_decoded_instruction decoded;
		unsigned long fault_address = regs->pc;
		unsigned long block_pc = regs->pc;
		u32 code_generation;
		size_t word_count = 0;
		u32 block_instruction_count = 0;
		u32 instruction;
		bool first_is_svc = false;
		bool global_cache_ref = false;
		int ret;

		if (++instruction_count % TCTI_PROGRESS_REPORT_INTERVAL == 0)
			pr_info_ratelimited("Orlix TCTI: progress task=%s pid=%d pc=%#llx instructions=%llu sp=%#llx x0=%#llx x1=%#llx x2=%#llx x3=%#llx x8=%#llx x9=%#llx x10=%#llx x11=%#llx x19=%#llx x20=%#llx x21=%#llx x22=%#llx x23=%#llx x24=%#llx x30=%#llx pstate=%#llx\n",
					    task->comm, task_pid_nr(task),
					    regs->pc, instruction_count,
					    regs->sp, regs->regs[0],
					    regs->regs[1], regs->regs[2],
					    regs->regs[3], regs->regs[8],
					    regs->regs[9], regs->regs[10],
					    regs->regs[11], regs->regs[19],
					    regs->regs[20], regs->regs[21],
					    regs->regs[22], regs->regs[23],
					    regs->regs[24], regs->regs[30],
					    regs->pstate);

		code_generation = tcti_code_generation(mm);
		block = tcti_hot_blocks_lookup(hot_blocks, block_pc,
					       code_generation);
		if (!block) {
			block = tcti_block_cache_lookup(mm, block_pc,
							code_generation);
			if (block) {
				global_cache_ref = true;
				tcti_hot_blocks_remember(hot_blocks,
							 &hot_block_cursor,
							 block);
			}
		}
		if (block) {
			enum tcti_access block_fault_access;
			u32 block_instruction = 0;
			u32 block_program_words = block->program_words;
			unsigned long long before_pc = regs->pc;
			unsigned long long before_sp = regs->sp;
			unsigned long long before_lr = regs->regs[30];

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
			if (atomic_dec_if_positive(&tcti_block_trace_budget) >= 0)
				pr_info("Orlix TCTI: block exec task=%s pid=%d start_pc=%#llx end_pc=%#llx before_lr=%#llx after_lr=%#llx before_sp=%#llx after_sp=%#llx insn=%#x ret=%d words=%u count=%u cached=1\n",
					task->comm, task_pid_nr(task),
					before_pc, regs->pc, before_lr,
					regs->regs[30], before_sp, regs->sp,
					block_instruction, ret, block_program_words,
					block->instruction_count);
			if (ret == -EFAULT || ret == -EACCES) {
				block_fault_access =
					tcti_fault_access_for_program_pc(block,
									 regs->pc);
				if (block_fault_access == TCTI_ACCESS_FETCH)
					block_fault_access =
						tcti_fault_access_for_program(
							block->program,
							block->program_words);
			}
			if (global_cache_ref)
				tcti_block_put(block);
			if (!ret)
				continue;

			if (ret == -EFAULT || ret == -EACCES) {
				pr_info("Orlix TCTI: cached block fault task=%s pid=%d pc=%#llx ret=%d fault=%#lx insn=%#x code_generation=%u words=%u\n",
					task->comm, task_pid_nr(task), regs->pc,
					ret, fault_address, block_instruction,
					code_generation, block_program_words);
				result.reason = TCTI_EXIT_USER_FAULT;
				result.status = ret;
				result.fault_address = fault_address;
				result.fault_access = block_fault_access;
				result.pc = regs->pc;
				result.instruction = block_instruction;
				tcti_hot_blocks_release(hot_blocks);
				return result;
			}

			result.reason = TCTI_EXIT_UNSUPPORTED_INSTRUCTION;
			result.status = ret;
			result.pc = regs->pc;
			result.instruction = block_instruction;
			tcti_hot_blocks_release(hot_blocks);
			return result;
		}

		ret = tcti_build_straight_line_block(mm, regs->pc, program,
						     ARRAY_SIZE(program),
						     &word_count,
						     &block_instruction_count,
						     &instruction,
						     &first_is_svc);
		if (ret == -EINTR && first_is_svc) {
			result.reason = TCTI_EXIT_SYSCALL;
			result.status = 0;
			result.pc = regs->pc;
			result.instruction = instruction;
			tcti_hot_blocks_release(hot_blocks);
			return result;
		}
		if (ret) {
			result.status = ret;
			result.pc = regs->pc;
			result.instruction = instruction;
			if (ret == -EFAULT || ret == -EACCES) {
				pr_info("Orlix TCTI: fetch failed task=%s pid=%d pc=%#llx ret=%d\n",
					task->comm, task_pid_nr(task), regs->pc,
					ret);
				result.reason = TCTI_EXIT_USER_FAULT;
				result.fault_address = regs->pc;
				result.fault_access = TCTI_ACCESS_FETCH;
			} else {
				result.reason = TCTI_EXIT_UNSUPPORTED_INSTRUCTION;
			}
			tcti_hot_blocks_release(hot_blocks);
			return result;
		}

		decoded = tcti_decode_aarch64(instruction);
		ret = tcti_block_cache_insert(
			mm, block_pc,
			block_pc + block_instruction_count * sizeof(u32),
			code_generation, block_instruction_count, program,
			word_count, &block);
		if (!ret && block) {
			unsigned long long before_pc = regs->pc;
			unsigned long long before_sp = regs->sp;
			unsigned long long before_lr = regs->regs[30];

			tcti_hot_blocks_remember(hot_blocks, &hot_block_cursor,
						 block);
			ret = tcti_execute_gadget_program(mm, regs, block->program,
							  block->program_words,
							  &fault_address);
			if (atomic_dec_if_positive(&tcti_block_trace_budget) >= 0)
				pr_info("Orlix TCTI: block exec task=%s pid=%d start_pc=%#llx end_pc=%#llx before_lr=%#llx after_lr=%#llx before_sp=%#llx after_sp=%#llx insn=%#x ret=%d words=%u count=%u cached=0\n",
					task->comm, task_pid_nr(task),
					before_pc, regs->pc, before_lr,
					regs->regs[30], before_sp, regs->sp,
					instruction, ret, block->program_words,
					block->instruction_count);
			tcti_block_put(block);
		} else {
			unsigned long long before_pc = regs->pc;
			unsigned long long before_sp = regs->sp;
			unsigned long long before_lr = regs->regs[30];

			ret = tcti_execute_gadget_program(mm, regs, program,
							  word_count,
							  &fault_address);
			if (atomic_dec_if_positive(&tcti_block_trace_budget) >= 0)
				pr_info("Orlix TCTI: block exec task=%s pid=%d start_pc=%#llx end_pc=%#llx before_lr=%#llx after_lr=%#llx before_sp=%#llx after_sp=%#llx insn=%#x ret=%d words=%zu count=%u cached=0\n",
					task->comm, task_pid_nr(task),
					before_pc, regs->pc, before_lr,
					regs->regs[30], before_sp, regs->sp,
					instruction, ret, word_count,
					block_instruction_count);
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
			tcti_hot_blocks_release(hot_blocks);
			return result;
		}

		result.reason = TCTI_EXIT_UNSUPPORTED_INSTRUCTION;
		result.status = ret;
		result.pc = regs->pc;
		result.instruction = instruction;
		tcti_hot_blocks_release(hot_blocks);
		return result;
	}
}

void tcti_prepare_syscall_handoff(struct pt_regs *regs)
{
	regs->orig_x0 = regs->regs[0];
	regs->syscallno = regs->regs[8];
	regs->pc += sizeof(u32);
}

#if IS_ENABLED(CONFIG_ORLIX_TCTI_KUNIT_TEST)
bool tcti_kernel_syscall_dispatch_smoke_for_tests(
	struct pt_regs *regs,
	struct tcti_kernel_syscall_dispatch_smoke_result *out)
{
	if (!regs || !out)
		return false;

	memset(out, 0, sizeof(*out));

	return tcti_kernel_syscall_dispatch_smoke_execute(
		regs, out, orlix_syscall_dispatch);
}

bool tcti_kernel_execve_binfmt_elf_smoke_for_tests(
	const struct tcti_kernel_execve_binfmt_elf_smoke_payload *payload,
	struct pt_regs *regs,
	struct tcti_kernel_execve_binfmt_elf_smoke_result *out)
{
	if (!payload || !regs || !out)
		return false;

	return tcti_kernel_execve_binfmt_elf_smoke_execute(
		payload, regs, out, start_thread);
}
#endif

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

static void tcti_refresh_current_user_range(unsigned long start, size_t length)
{
	unsigned long page;
	unsigned long end;

	if (!start || start >= TASK_SIZE || !length)
		return;
	if (length > TASK_SIZE - start)
		length = TASK_SIZE - start;

	end = PAGE_ALIGN(start + length);
	if (!end || end > TASK_SIZE)
		end = TASK_SIZE;

	for (page = start & PAGE_MASK; page < end; page += PAGE_SIZE)
		(void)orlix_refresh_current_user_mapping_page(page);
}

static void tcti_sync_syscall_user_ranges(struct pt_regs *regs,
					  unsigned long nr)
{
	if (!regs || IS_ERR_VALUE(regs->regs[0]))
		return;

	switch (nr) {
	case __NR_read:
		tcti_refresh_current_user_range(regs->regs[1], regs->regs[0]);
		break;
	case __NR_ioctl:
		if (regs->regs[1] == TIOCGWINSZ)
			tcti_refresh_current_user_range(regs->regs[2],
							sizeof(struct winsize));
		break;
	case __NR_rt_sigaction:
		tcti_refresh_current_user_range(regs->regs[2],
						sizeof(struct sigaction));
		break;
	case __NR_rt_sigprocmask:
		tcti_refresh_current_user_range(regs->regs[2],
						sizeof(sigset_t));
		break;
	case __NR_uname:
		tcti_refresh_current_user_range(regs->orig_x0,
						sizeof(struct new_utsname));
		break;
	default:
		break;
	}
}

static bool tcti_syscall_changes_user_mappings(unsigned long nr)
{
	switch (nr) {
	case __NR_brk:
	case __NR_mmap:
	case __NR_mprotect:
	case __NR_munmap:
	case __NR_mremap:
		return true;
	default:
		return false;
	}
}

#if IS_ENABLED(CONFIG_ORLIX_TCTI_KUNIT_TEST)
bool tcti_syscall_changes_user_mappings_for_tests(unsigned long nr)
{
	return tcti_syscall_changes_user_mappings(nr);
}
#endif

static void tcti_trace_execve_user_argv(const struct pt_regs *regs)
{
	static atomic_t execve_trace_budget = ATOMIC_INIT(16);
	const char __user *const __user *argv;
	const char __user *filename;
	char filename_buf[96];
	long copied;
	int i;

	if (atomic_dec_if_positive(&execve_trace_budget) < 0)
		return;

	filename = (const char __user *)regs->regs[0];
	argv = (const char __user *const __user *)regs->regs[1];

	copied = strncpy_from_user(filename_buf, filename,
				   sizeof(filename_buf) - 1);
	if (copied < 0)
		strscpy(filename_buf, "<fault>", sizeof(filename_buf));
	else
		filename_buf[sizeof(filename_buf) - 1] = '\0';

	pr_info("Orlix TCTI: execve argv task=%s pid=%d filename_ptr=%#llx filename=\"%s\" argv_ptr=%#llx envp_ptr=%#llx\n",
		current->comm, task_pid_nr(current), regs->regs[0],
		filename_buf, regs->regs[1], regs->regs[2]);

	for (i = 0; i < 6; i++) {
		const char __user *argp = NULL;
		char arg_buf[160];

		if (copy_from_user(&argp, argv + i, sizeof(argp))) {
			pr_info("Orlix TCTI: execve argv task=%s pid=%d argv%d_ptr=<fault>\n",
				current->comm, task_pid_nr(current), i);
			break;
		}
		if (!argp) {
			pr_info("Orlix TCTI: execve argv task=%s pid=%d argv%d_ptr=NULL\n",
				current->comm, task_pid_nr(current), i);
			break;
		}

		copied = strncpy_from_user(arg_buf, argp,
					   sizeof(arg_buf) - 1);
		if (copied < 0)
			strscpy(arg_buf, "<fault>", sizeof(arg_buf));
		else
			arg_buf[sizeof(arg_buf) - 1] = '\0';
		pr_info("Orlix TCTI: execve argv task=%s pid=%d argv%d_ptr=%px argv%d=\"%s\"\n",
			current->comm, task_pid_nr(current), i, argp, i,
			arg_buf);
	}
}

static void orlix_tcti_handle_syscall(struct pt_regs *regs)
{
	unsigned long nr = regs->regs[8];
	unsigned long pc = regs->pc;
	struct pt_regs *task_regs;

	static atomic_t post_dispatch_report_budget = ATOMIC_INIT(32);
	if (nr == __NR_execve)
		tcti_trace_execve_user_argv(regs);
	tcti_prepare_syscall_handoff(regs);
	orlix_syscall_dispatch(regs);
	task_regs = task_pt_regs(current);
	if (atomic_dec_if_positive(&post_dispatch_report_budget) >= 0)
		pr_info("Orlix TCTI: syscall post-dispatch task=%s pid=%d syscall=%lu entry_pc=%#lx regs_pc=%#llx task_regs_pc=%#llx ret=%#llx syscallno=%d task_regs_syscallno=%d sp=%#llx task_regs_sp=%#llx x30=%#llx task_regs_x30=%#llx\n",
			current->comm, task_pid_nr(current), nr, pc,
			regs->pc, task_regs ? task_regs->pc : 0,
			regs->regs[0], regs->syscallno,
			task_regs ? task_regs->syscallno : NO_SYSCALL,
			regs->sp, task_regs ? task_regs->sp : 0,
			regs->regs[30], task_regs ? task_regs->regs[30] : 0);
	if (current->mm && !IS_ERR_VALUE(regs->regs[0]) &&
	    tcti_syscall_changes_user_mappings(nr))
		tcti_block_cache_invalidate_mm(current->mm);
	tcti_sync_syscall_user_ranges(regs, nr);
	tcti_report_syscall_return(current, regs, nr, pc);
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

			tcti_report_syscall(current, regs, &result);
			orlix_tcti_handle_syscall(regs);
			if (syscall_was_execve && !IS_ERR_VALUE(regs->regs[0])) {
				regs = task_pt_regs(current);
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
