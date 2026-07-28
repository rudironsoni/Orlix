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
#include <asm/elf.h>
#include <asm/ioctls.h>
#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/signal.h>
#include <asm/termios.h>
#include <asm/orlix_tcti.h>
#include <asm/unistd.h>

#include "block_cache.h"
#include "decode_aarch64.h"
#include "engine.h"
#include "gadget_program.h"
#include "native_capture.h"
#include "report.h"

#define ORLIX_TCTI_ELF_IMAGE_SCAN_GRANULE (64UL * 1024UL)
#define ORLIX_TCTI_ELF_IMAGE_SCAN_LIMIT (16UL * 1024UL * 1024UL)
#define ORLIX_TCTI_MAX_DYNAMIC_ENTRIES 256
#define ORLIX_TCTI_MAX_RELA_ENTRIES 16384
#define ORLIX_TCTI_STATIC_PIE_TLS_TCB_OFFSET 0x78UL
#define ORLIX_TCTI_PROGRESS_REPORT_INTERVAL 100000UL
#define ORLIX_TCTI_MAX_BLOCK_INSTRUCTIONS 32U
#define ORLIX_TCTI_LOCAL_HOT_BLOCKS 16U
#define ORLIX_TCTI_BLOCK_PROGRAM_WORDS \
	ORLIX_TCTI_PROGRAM_WORDS_FOR_INSTRUCTIONS(ORLIX_TCTI_MAX_BLOCK_INSTRUCTIONS)

struct orlix_tcti_hot_block {
	unsigned long guest_pc;
	u64 code_generation;
	struct orlix_tcti_block *block;
};

static atomic_t orlix_tcti_block_trace_budget = ATOMIC_INIT(64);
#ifndef R_AARCH64_RELATIVE
#define R_AARCH64_RELATIVE 1027
#endif

/*
 * The decoder retains feature-conditioned leaves so the complete target
 * inventory can audit them.  Execution is a separate contract: a decoded
 * FEAT_CSSC instruction may enter a guest only after Linux advertises CSSC.
 * ORLIX_EL0_HWCAP2 is currently zero, so all CSSC forms take the normal
 * unsupported-instruction exit without changing guest architectural state.
 * Scalar FP16 uses HWCAP_FPHP and AdvSIMD FP16 uses HWCAP_ASIMDHP. Both are
 * likewise zero until their owning complete-target proof authorizes them.
 */
static bool orlix_tcti_decoded_requires_cssc(
	const struct orlix_tcti_decoded_instruction *decoded)
{
	if (!decoded)
		return false;

	switch (decoded->decode_class) {
	case ORLIX_TCTI_DECODE_MIN_MAX_IMMEDIATE:
		return true;
	case ORLIX_TCTI_DECODE_DATA_PROCESSING_1SOURCE:
		return decoded->dp1_op == ORLIX_TCTI_DP1_CTZ ||
			decoded->dp1_op == ORLIX_TCTI_DP1_CNT ||
			decoded->dp1_op == ORLIX_TCTI_DP1_ABS;
	case ORLIX_TCTI_DECODE_DATA_PROCESSING_2SOURCE:
		return decoded->dp2_op == ORLIX_TCTI_DP2_SMAX ||
			decoded->dp2_op == ORLIX_TCTI_DP2_UMAX ||
			decoded->dp2_op == ORLIX_TCTI_DP2_SMIN ||
			decoded->dp2_op == ORLIX_TCTI_DP2_UMIN;
	default:
		return false;
	}
}

static bool orlix_tcti_decoded_requires_fp16(
	const struct orlix_tcti_decoded_instruction *decoded)
{
	return decoded && decoded->simd_fp &&
		(decoded->access_size == sizeof(u16) ||
		 decoded->result_size == sizeof(u16));
}

static bool orlix_tcti_decoded_runtime_available(
	const struct orlix_tcti_decoded_instruction *decoded,
	const struct orlix_tcti_test_feature_profile *test_profile)
{
	unsigned long hwcap = test_profile ? test_profile->hwcap : ELF_HWCAP;
	unsigned long hwcap2 = test_profile ? test_profile->hwcap2 : ELF_HWCAP2;

	/* FEAT_FlagM remains unavailable until its Linux HWCAP contract is owned. */
	if (decoded &&
	    decoded->decode_class == ORLIX_TCTI_DECODE_FLAG_MANIPULATION)
		return false;
	if (orlix_tcti_decoded_requires_fp16(decoded))
		return decoded->decode_class == ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC ?
			(hwcap & HWCAP_ASIMDHP) : (hwcap & HWCAP_FPHP);
	return !orlix_tcti_decoded_requires_cssc(decoded) ||
		(hwcap2 & HWCAP2_CSSC);
}

static bool orlix_tcti_address_has_vma(struct mm_struct *mm, unsigned long address,
				 enum orlix_tcti_access access)
{
	struct vm_area_struct *vma;
	vm_flags_t required;
	bool valid = false;

	switch (access) {
	case ORLIX_TCTI_ACCESS_READ:
		required = VM_READ;
		break;
	case ORLIX_TCTI_ACCESS_WRITE:
		required = VM_WRITE;
		break;
	case ORLIX_TCTI_ACCESS_FETCH:
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

static int orlix_tcti_read_user_data_faulting_impl(struct pt_regs *regs,
					     struct mm_struct *mm,
					     unsigned long address,
					     void *buffer, size_t size,
					     bool log_missing_vma)
{
	int ret;
	int fault_ret;

	ret = orlix_tcti_read_user_data(mm, address, buffer, size);
	if (ret != -EFAULT && ret != -EACCES)
		return ret;
	if (!orlix_tcti_address_has_vma(mm, address, ORLIX_TCTI_ACCESS_READ)) {
		if (log_missing_vma)
			pr_info("OrlixTCTI: static PIE read has no readable VMA task=%s pid=%d addr=%#lx size=%zu ret=%d\n",
				current->comm, task_pid_nr(current), address,
				size, ret);
		return ret;
	}

	fault_ret = orlix_tcti_handle_user_fault(regs, address, ORLIX_TCTI_ACCESS_READ);
	if (fault_ret) {
		pr_info("OrlixTCTI: static PIE read fault-in failed task=%s pid=%d addr=%#lx size=%zu ret=%d fault_ret=%d\n",
			current->comm, task_pid_nr(current), address, size, ret,
			fault_ret);
		return fault_ret;
	}

	ret = orlix_tcti_read_user_data(mm, address, buffer, size);
	if (ret)
		pr_info("OrlixTCTI: static PIE read retry failed task=%s pid=%d addr=%#lx size=%zu ret=%d\n",
			current->comm, task_pid_nr(current), address, size, ret);

	return ret;
}

static int orlix_tcti_read_user_data_faulting(struct pt_regs *regs,
					struct mm_struct *mm,
					unsigned long address,
					void *buffer, size_t size)
{
	return orlix_tcti_read_user_data_faulting_impl(regs, mm, address, buffer,
						 size, true);
}

static int orlix_tcti_scan_user_data_faulting(struct pt_regs *regs,
					struct mm_struct *mm,
					unsigned long address,
					void *buffer, size_t size)
{
	return orlix_tcti_read_user_data_faulting_impl(regs, mm, address, buffer,
						 size, false);
}

static int orlix_tcti_write_user_data_faulting(struct pt_regs *regs,
					 struct mm_struct *mm,
					 unsigned long address,
					 const void *buffer, size_t size)
{
	int ret;
	int fault_ret;

	ret = orlix_tcti_write_user_data(mm, address, buffer, size);
	if (ret != -EFAULT && ret != -EACCES)
		return ret;
	if (!orlix_tcti_address_has_vma(mm, address, ORLIX_TCTI_ACCESS_WRITE)) {
		pr_info("OrlixTCTI: static PIE write has no writable VMA task=%s pid=%d addr=%#lx size=%zu ret=%d\n",
			current->comm, task_pid_nr(current), address, size, ret);
		return ret;
	}

	fault_ret = orlix_tcti_handle_user_fault(regs, address, ORLIX_TCTI_ACCESS_WRITE);
	if (fault_ret) {
		pr_info("OrlixTCTI: static PIE write fault-in failed task=%s pid=%d addr=%#lx size=%zu ret=%d fault_ret=%d\n",
			current->comm, task_pid_nr(current), address, size, ret,
			fault_ret);
		return fault_ret;
	}

	ret = orlix_tcti_write_user_data(mm, address, buffer, size);
	if (ret)
		pr_info("OrlixTCTI: static PIE write retry failed task=%s pid=%d addr=%#lx size=%zu ret=%d\n",
			current->comm, task_pid_nr(current), address, size, ret);

	return ret;
}

static int orlix_tcti_find_current_elf_base(struct mm_struct *mm,
				      struct pt_regs *regs,
				      unsigned long *base)
{
	unsigned long candidate;
	unsigned long limit;
	Elf64_Ehdr ehdr;

	if (!mm || !regs || !base)
		return -EINVAL;

	candidate = regs->pc & ~(ORLIX_TCTI_ELF_IMAGE_SCAN_GRANULE - 1);
	limit = candidate > ORLIX_TCTI_ELF_IMAGE_SCAN_LIMIT ?
		candidate - ORLIX_TCTI_ELF_IMAGE_SCAN_LIMIT : 0;

	for (;;) {
		if (!orlix_tcti_scan_user_data_faulting(regs, mm, candidate, &ehdr,
						  sizeof(ehdr)) &&
		    memcmp(ehdr.e_ident, ELFMAG, SELFMAG) == 0 &&
		    ehdr.e_ident[EI_CLASS] == ELFCLASS64 &&
		    ehdr.e_ident[EI_DATA] == ELFDATA2LSB &&
		    ehdr.e_machine == EM_AARCH64 &&
		    ehdr.e_type == ET_DYN) {
			*base = candidate;
			return 0;
		}
		if (candidate <= limit || candidate < ORLIX_TCTI_ELF_IMAGE_SCAN_GRANULE)
			break;
		candidate -= ORLIX_TCTI_ELF_IMAGE_SCAN_GRANULE;
	}

	return -ENOEXEC;
}

static int orlix_tcti_read_dynamic_value(struct mm_struct *mm, unsigned long base,
				   struct pt_regs *regs,
				   const Elf64_Phdr *phdr, Elf64_Sxword tag,
				   Elf64_Xword *value)
{
	unsigned long dynamic = base + phdr->p_vaddr;
	size_t entries = phdr->p_memsz / sizeof(Elf64_Dyn);
	size_t index;

	if (!value || !entries || entries > ORLIX_TCTI_MAX_DYNAMIC_ENTRIES)
		return -ENOEXEC;

	for (index = 0; index < entries; index++) {
		Elf64_Dyn dyn;
		int ret;

		ret = orlix_tcti_read_user_data_faulting(regs, mm,
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

static unsigned long orlix_tcti_saved_auxv_value(struct mm_struct *mm,
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

static bool orlix_tcti_elf_base_is_main_executable(struct mm_struct *mm,
					     unsigned long base,
					     const Elf64_Ehdr *ehdr)
{
	unsigned long at_phdr;

	if (!mm || !ehdr)
		return false;

	at_phdr = orlix_tcti_saved_auxv_value(mm, AT_PHDR);
	if (!at_phdr)
		return true;
	if (base > ULONG_MAX - ehdr->e_phoff)
		return false;

	return at_phdr == base + ehdr->e_phoff;
}

bool orlix_tcti_static_pie_relocation_count_valid(size_t count)
{
	return count <= ORLIX_TCTI_MAX_RELA_ENTRIES;
}

static int orlix_tcti_apply_relative_relocations(struct mm_struct *mm,
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

	ret = orlix_tcti_read_dynamic_value(mm, base, regs, dynamic_phdr, DT_RELA,
				      &rela_vaddr);
	if (ret == -ENOENT)
		return 0;
	if (ret)
		return ret;
	ret = orlix_tcti_read_dynamic_value(mm, base, regs, dynamic_phdr, DT_RELASZ,
				      &rela_size);
	if (ret)
		return ret;
	ret = orlix_tcti_read_dynamic_value(mm, base, regs, dynamic_phdr, DT_RELAENT,
				      &rela_entry_size);
	if (ret == -ENOENT)
		rela_entry_size = sizeof(Elf64_Rela);
	else if (ret)
		return ret;
	if (rela_entry_size != sizeof(Elf64_Rela) ||
	    rela_size % sizeof(Elf64_Rela))
		return -ENOEXEC;

	if (rela_size % sizeof(Elf64_Rela))
		return -ENOEXEC;
	count = rela_size / sizeof(Elf64_Rela);
	if (!orlix_tcti_static_pie_relocation_count_valid(count))
		return -E2BIG;

	for (index = 0; index < count; index++) {
		Elf64_Rela rela;
		unsigned long target;
		unsigned long value;
		unsigned long current_value = 0;

		ret = orlix_tcti_read_user_data_faulting(regs, mm,
						   base + rela_vaddr +
						   index * sizeof(rela),
						   &rela, sizeof(rela));
		if (ret)
			return ret;
		if (ELF64_R_TYPE(rela.r_info) != R_AARCH64_RELATIVE)
			return -EOPNOTSUPP;

		target = base + rela.r_offset;
		value = base + rela.r_addend;
		ret = orlix_tcti_read_user_data_faulting(regs, mm, target,
						   &current_value,
						   sizeof(current_value));
		if (ret)
			return ret;
		if (current_value == value)
			continue;
		if (current_value != 0 && current_value != rela.r_addend)
			return -EEXIST;
		ret = orlix_tcti_write_user_data_faulting(regs, mm, target, &value,
						    sizeof(value));
		if (ret)
			return ret;
	}

	return 0;
}

bool orlix_tcti_static_pie_initial_tls(unsigned long base, const Elf64_Phdr *phdr,
				 unsigned long *initial_tls)
{
	unsigned long tls;

	if (!phdr || phdr->p_type != PT_TLS || !initial_tls)
		return false;
	if (!phdr->p_memsz)
		return false;
	if (phdr->p_vaddr > ULONG_MAX - ORLIX_TCTI_STATIC_PIE_TLS_TCB_OFFSET)
		return false;
	if (base > ULONG_MAX - phdr->p_vaddr - ORLIX_TCTI_STATIC_PIE_TLS_TCB_OFFSET)
		return false;

	tls = base + phdr->p_vaddr + ORLIX_TCTI_STATIC_PIE_TLS_TCB_OFFSET;
	if (tls >= TASK_SIZE)
		return false;

	*initial_tls = tls;
	return true;
}

static int orlix_tcti_apply_static_pie_relative_relocations(struct task_struct *task,
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

	ret = orlix_tcti_find_current_elf_base(mm, regs, &base);
	if (ret == -ENOEXEC)
		return 0;
	if (ret)
		return ret;
	if (base == orlix_tcti_saved_auxv_value(mm, AT_BASE)) {
		pr_info_once("OrlixTCTI: leaving PT_INTERP image self-relocation to ld.so task=%s pid=%d base=%#lx\n",
			     task->comm, task_pid_nr(task), base);
		if (applied_base)
			*applied_base = base;
		return 0;
	}
	if (mm->context.orlix_tcti_static_pie_base == base)
		return 0;
	if (applied_base && *applied_base == base)
		return 0;

	ret = orlix_tcti_read_user_data_faulting(regs, mm, base, &ehdr, sizeof(ehdr));
	if (ret)
		return ret;
	if (ehdr.e_phentsize != sizeof(Elf64_Phdr) || !ehdr.e_phnum)
		return -ENOEXEC;
	if (!orlix_tcti_elf_base_is_main_executable(mm, base, &ehdr)) {
		pr_info_once("OrlixTCTI: leaving shared object relocation to rtld task=%s pid=%d base=%#lx at_phdr=%#lx phoff=%#llx\n",
			     task->comm, task_pid_nr(task), base,
			     orlix_tcti_saved_auxv_value(mm, AT_PHDR),
			     (unsigned long long)ehdr.e_phoff);
		if (applied_base)
			*applied_base = base;
		return 0;
	}
	pr_debug("OrlixTCTI: static PIE image task=%s pid=%d pc=%#llx base=%#lx entry=%#llx phoff=%#llx phnum=%u phentsize=%u\n",
		task->comm, task_pid_nr(task), regs->pc, base,
		(unsigned long long)ehdr.e_entry,
		(unsigned long long)ehdr.e_phoff, ehdr.e_phnum,
		ehdr.e_phentsize);

	for (index = 0; index < ehdr.e_phnum; index++) {
		Elf64_Phdr phdr;

		ret = orlix_tcti_read_user_data_faulting(regs, mm,
						   base + ehdr.e_phoff +
						   index * sizeof(phdr),
						   &phdr, sizeof(phdr));
		if (ret)
			return ret;
		if (orlix_tcti_static_pie_initial_tls(base, &phdr, &initial_tls))
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
	pr_debug("OrlixTCTI: static PIE initial TLS task=%s pid=%d base=%#lx tls=%#lx prepared=%#lx\n",
			task->comm, task_pid_nr(task), base, initial_tls,
			prepared_tls);
#endif
	}

	if (!found_dynamic)
		return 0;

	pr_debug("OrlixTCTI: static PIE dynamic task=%s pid=%d base=%#lx vaddr=%#llx memsz=%#llx filesz=%#llx flags=%#x\n",
		task->comm, task_pid_nr(task), base,
		(unsigned long long)dynamic_phdr.p_vaddr,
		(unsigned long long)dynamic_phdr.p_memsz,
		(unsigned long long)dynamic_phdr.p_filesz, dynamic_phdr.p_flags);
	ret = orlix_tcti_apply_relative_relocations(mm, regs, base, &dynamic_phdr);
	if (ret)
		return ret;
	pr_info_once("OrlixTCTI: applied static PIE R_AARCH64_RELATIVE relocations task=%s pid=%d base=%#lx\n",
		     task->comm, task_pid_nr(task), base);
	mm->context.orlix_tcti_static_pie_base = base;
	if (applied_base)
		*applied_base = base;
	return 0;
}

static enum orlix_tcti_access
orlix_tcti_fault_access_for_decoded(const struct orlix_tcti_decoded_instruction *decoded)
{
	if (!decoded)
		return ORLIX_TCTI_ACCESS_FETCH;

	switch (decoded->decode_class) {
	case ORLIX_TCTI_DECODE_LOAD_STORE_PAIR:
	case ORLIX_TCTI_DECODE_LOAD_STORE_UNSIGNED_IMMEDIATE:
	case ORLIX_TCTI_DECODE_LOAD_STORE_SIGNED_IMMEDIATE:
	case ORLIX_TCTI_DECODE_LOAD_STORE_REGISTER_OFFSET:
	case ORLIX_TCTI_DECODE_LOAD_STORE_EXCLUSIVE:
	case ORLIX_TCTI_DECODE_LSE_ATOMIC:
	case ORLIX_TCTI_DECODE_SIMD_LOAD_STORE_SINGLE_STRUCTURE:
	case ORLIX_TCTI_DECODE_SIMD_LOAD_REPLICATE:
	case ORLIX_TCTI_DECODE_SIMD_LOAD_STORE_MULTIPLE_STRUCTURE:
		return decoded->load ? ORLIX_TCTI_ACCESS_READ : ORLIX_TCTI_ACCESS_WRITE;
	default:
		return ORLIX_TCTI_ACCESS_FETCH;
	}
}

static bool
orlix_tcti_decoded_ends_block(const struct orlix_tcti_decoded_instruction *decoded)
{
	if (!decoded)
		return true;
	if (decoded->decode_class == ORLIX_TCTI_DECODE_HINT)
		return decoded->hint_imm >= 1 && decoded->hint_imm <= 3;

	switch (decoded->decode_class) {
	case ORLIX_TCTI_DECODE_SVC:
	case ORLIX_TCTI_DECODE_BRK:
	case ORLIX_TCTI_DECODE_HLT:
	case ORLIX_TCTI_DECODE_UNDEFINED:
	case ORLIX_TCTI_DECODE_BARRIER:
	case ORLIX_TCTI_DECODE_CACHE_MAINTENANCE:
	case ORLIX_TCTI_DECODE_UNCONDITIONAL_BRANCH_IMMEDIATE:
	case ORLIX_TCTI_DECODE_UNCONDITIONAL_BRANCH_REGISTER:
	case ORLIX_TCTI_DECODE_COMPARE_BRANCH_IMMEDIATE:
	case ORLIX_TCTI_DECODE_COMPARE_BRANCH_EXTENSION:
	case ORLIX_TCTI_DECODE_TEST_BRANCH_IMMEDIATE:
	case ORLIX_TCTI_DECODE_CONDITIONAL_BRANCH_IMMEDIATE:
		return true;
	default:
		return false;
	}
}

static void orlix_tcti_set_yield_result(struct mm_struct *mm,
				  const struct pt_regs *regs,
				  struct orlix_tcti_result *result)
{
	unsigned long instruction_pc = regs->pc - sizeof(u32);
	u32 instruction = 0;

	(void)orlix_tcti_fetch_instruction(mm, instruction_pc, &instruction);
	result->reason = ORLIX_TCTI_EXIT_YIELD;
	result->status = (instruction >> 5) & 0x7fU;
	result->pc = regs->pc;
	result->instruction = instruction;
}

static bool orlix_tcti_decoded_for_program_pc(
	const struct orlix_tcti_gadget_word *program, size_t program_words,
	unsigned long start_pc, u32 instruction_count, unsigned long pc,
	struct orlix_tcti_decoded_instruction *decoded)
{
	unsigned long index;
	size_t offset;

	if (!program || !decoded || pc < start_pc ||
	    !IS_ALIGNED(pc - start_pc, sizeof(u32)))
		return false;
	index = (pc - start_pc) / sizeof(u32);
	if (index >= instruction_count)
		return false;

	offset = index * (1U + ORLIX_TCTI_DECODED_INSTRUCTION_WORDS);
	if (offset + 1 + ORLIX_TCTI_DECODED_INSTRUCTION_WORDS > program_words)
		return false;

	memcpy(decoded, &program[offset + 1], sizeof(*decoded));
	return true;
}

static bool
orlix_tcti_lse_alignment_fault(const struct orlix_tcti_decoded_instruction *decoded,
			 unsigned long address)
{
	u8 size;

	if (!decoded || decoded->decode_class != ORLIX_TCTI_DECODE_LSE_ATOMIC)
		return false;
	if (decoded->rn == 31 && !IS_ALIGNED(address, 16))
		return true;
	size = decoded->access_size * (decoded->pair ? 2 : 1);
	return size && !IS_ALIGNED(address, size);
}

static int orlix_tcti_build_straight_line_block(struct mm_struct *mm,
					  unsigned long start_pc,
					  const struct orlix_tcti_test_feature_profile *test_profile,
					  struct orlix_tcti_gadget_word *program,
					  size_t capacity,
					  size_t *word_count,
					  u32 *instruction_count,
					  u32 *first_instruction,
					  enum orlix_tcti_decode_class *first_exit_class)
{
	u32 count;
	int ret;

	if (!mm || !program || !word_count || !instruction_count)
		return -EINVAL;

	*word_count = 0;
	*instruction_count = 0;
	if (first_instruction)
		*first_instruction = 0;
	if (first_exit_class)
		*first_exit_class = ORLIX_TCTI_DECODE_UNSUPPORTED;

	for (count = 0; count < ORLIX_TCTI_MAX_BLOCK_INSTRUCTIONS; count++) {
		struct orlix_tcti_decoded_instruction decoded;
		unsigned long pc = start_pc + count * sizeof(u32);
		u32 instruction = 0;

		ret = orlix_tcti_fetch_instruction(mm, pc, &instruction);
		if (ret)
			return count ? 0 : ret;

		if (!count && first_instruction)
			*first_instruction = instruction;

		decoded = orlix_tcti_decode_aarch64(instruction);
		if (decoded.decode_class == ORLIX_TCTI_DECODE_SVC ||
		    decoded.decode_class == ORLIX_TCTI_DECODE_BRK ||
		    decoded.decode_class == ORLIX_TCTI_DECODE_HLT ||
		    decoded.decode_class == ORLIX_TCTI_DECODE_UNDEFINED) {
			if (!count && first_exit_class)
				*first_exit_class = decoded.decode_class;
			return count ? 0 : -EINTR;
		}
		if (decoded.decode_class == ORLIX_TCTI_DECODE_UNSUPPORTED ||
		    !orlix_tcti_decoded_runtime_available(&decoded, test_profile))
			return count ? 0 : -EOPNOTSUPP;

		ret = orlix_tcti_append_decoded_instruction(&decoded, program,
						      capacity, word_count);
		if (ret)
			return count ? 0 : ret;
		(*instruction_count)++;

		if (orlix_tcti_decoded_ends_block(&decoded))
			break;
		if (((pc + sizeof(u32)) & PAGE_MASK) != (pc & PAGE_MASK))
			break;
	}

	return *instruction_count ? 0 : -EOPNOTSUPP;
}

static void orlix_tcti_hot_blocks_release(struct orlix_tcti_hot_block *hot_blocks)
{
	u32 i;

	for (i = 0; i < ORLIX_TCTI_LOCAL_HOT_BLOCKS; i++) {
		if (!hot_blocks[i].block)
			continue;
		orlix_tcti_block_put(hot_blocks[i].block);
		hot_blocks[i].block = NULL;
	}
}

static struct orlix_tcti_block *
orlix_tcti_hot_blocks_lookup(struct orlix_tcti_hot_block *hot_blocks,
		       unsigned long guest_pc, u64 code_generation)
{
	u32 i;

	for (i = 0; i < ORLIX_TCTI_LOCAL_HOT_BLOCKS; i++) {
		if (hot_blocks[i].block &&
		    hot_blocks[i].guest_pc == guest_pc &&
		    hot_blocks[i].code_generation == code_generation)
			return hot_blocks[i].block;
	}

	return NULL;
}

static void orlix_tcti_hot_blocks_remember(struct orlix_tcti_hot_block *hot_blocks,
				     u32 *cursor, struct orlix_tcti_block *block)
{
	struct orlix_tcti_hot_block *slot;

	if (!hot_blocks || !cursor || !block)
		return;

	slot = &hot_blocks[*cursor % ORLIX_TCTI_LOCAL_HOT_BLOCKS];
	(*cursor)++;

	if (slot->block == block) {
		slot->guest_pc = block->guest_start_pc;
		slot->code_generation = block->code_generation;
		return;
	}

	if (slot->block)
		orlix_tcti_block_put(slot->block);

	refcount_inc(&block->refs);
	slot->guest_pc = block->guest_start_pc;
	slot->code_generation = block->code_generation;
	slot->block = block;
}

static int orlix_tcti_execute_authorized_capture(
	struct mm_struct *mm, struct pt_regs *regs,
	const struct orlix_tcti_gadget_word *program, size_t word_count,
	unsigned long *fault_address, u64 code_generation, bool *entry_valid,
	unsigned long *entry_pc, u32 *entry_instruction,
	struct orlix_tcti_native_capture *capture,
	bool *successful_gadget_execution)
{
	int ret;

	if (capture)
		ret = orlix_tcti_execute_gadget_program_authorized_captured(
			mm, regs, program, word_count, fault_address, code_generation,
			entry_valid, entry_pc, entry_instruction, capture);
	else
		ret = orlix_tcti_execute_gadget_program_authorized_observed(
		mm, regs, program, word_count, fault_address, code_generation,
		entry_valid, entry_pc, entry_instruction);
	if (!ret && successful_gadget_execution)
		*successful_gadget_execution = true;
	return ret;
}

/* This private capability is wrapped in a stack evidence object created only
 * at the ordinary successful-gadget boundary below. */
struct orlix_tcti_successful_gadget_evidence {
	const void *engine_capability;
};

static const u8 orlix_tcti_engine_execution_capability;

bool orlix_tcti_native_capture_engine_evidence_valid(const void *evidence)
{
	const struct orlix_tcti_successful_gadget_evidence *successful = evidence;

	return successful &&
		successful->engine_capability == &orlix_tcti_engine_execution_capability;
}

#define ORLIX_TCTI_CAPTURE_RELEASE_AND_RETURN(_capture, _result, _regs, _hot, \
							_normal_resume, _successful_gadget_execution) \
	do { \
		orlix_tcti_native_capture_exit((_capture), &(_result), (_regs)); \
		if ((_normal_resume) && (_successful_gadget_execution) && \
		    (_result).reason == ORLIX_TCTI_EXIT_SYSCALL && \
		    (_result).status == 0) { \
			const struct orlix_tcti_successful_gadget_evidence evidence = { \
				.engine_capability = &orlix_tcti_engine_execution_capability, \
			}; \
			orlix_tcti_native_capture_complete_successful_gadget((_capture), \
				&evidence); \
		} \
		orlix_tcti_native_capture_finalize((_capture), &(_result), (_regs)); \
		orlix_tcti_hot_blocks_release((_hot)); \
		return (_result); \
	} while (0)

static struct orlix_tcti_result orlix_tcti_resume_user_internal(struct task_struct *task,
				    struct pt_regs *regs,
				    struct mm_struct *mm,
				    struct orlix_tcti_native_capture *capture,
				    bool normal_resume,
				    const struct orlix_tcti_test_feature_profile *test_profile)
{
	unsigned long long instruction_count = 0;
	struct orlix_tcti_hot_block hot_blocks[ORLIX_TCTI_LOCAL_HOT_BLOCKS] = {};
	u32 hot_block_cursor = 0;
	bool successful_gadget_execution = false;
	bool cache_allowed = !test_profile;
	struct orlix_tcti_result result = {
		.reason = ORLIX_TCTI_EXIT_TASK_EXIT,
		.status = -EINVAL,
	};

	if (!task || !regs || !mm) {
		orlix_tcti_native_capture_finalize(capture, &result, regs);
		return result;
	}

	for (;;) {
		struct orlix_tcti_gadget_word program[ORLIX_TCTI_BLOCK_PROGRAM_WORDS];
		struct orlix_tcti_block *block;
		struct orlix_tcti_decoded_instruction decoded;
		unsigned long fault_address = regs->pc;
		unsigned long block_pc = regs->pc;
		u64 code_generation;
		size_t word_count = 0;
		u32 block_instruction_count = 0;
		u32 instruction = 0;
		enum orlix_tcti_decode_class first_exit_class =
			ORLIX_TCTI_DECODE_UNSUPPORTED;
		bool global_cache_ref = false;
		bool fault_decoded;
		int ret;

		if (++instruction_count % ORLIX_TCTI_PROGRESS_REPORT_INTERVAL == 0)
			pr_info_ratelimited("OrlixTCTI: progress task=%s pid=%d pc=%#llx instructions=%llu sp=%#llx x0=%#llx x1=%#llx x2=%#llx x3=%#llx x8=%#llx x9=%#llx x10=%#llx x11=%#llx x19=%#llx x20=%#llx x21=%#llx x22=%#llx x23=%#llx x24=%#llx x30=%#llx pstate=%#llx\n",
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

		code_generation = orlix_tcti_code_generation(mm);
		block = cache_allowed ? orlix_tcti_hot_blocks_lookup(
			hot_blocks, block_pc, code_generation) : NULL;
		if (cache_allowed && !block) {
			block = orlix_tcti_block_cache_lookup(mm, block_pc,
							code_generation);
			if (block) {
				global_cache_ref = true;
				orlix_tcti_hot_blocks_remember(hot_blocks,
							 &hot_block_cursor,
							 block);
			}
		}
		if (block) {
			struct orlix_tcti_decoded_instruction block_decoded;
			enum orlix_tcti_access block_fault_access = ORLIX_TCTI_ACCESS_FETCH;
			u32 block_instruction = 0;
			u32 block_program_words = block->program_words;
			unsigned long long before_pc = regs->pc;
			unsigned long long before_sp = regs->sp;
			unsigned long long before_lr = regs->regs[30];
			bool block_decoded_valid;

			block_decoded_valid = orlix_tcti_decoded_for_program_pc(
				block->program, block->program_words,
				block->guest_start_pc, block->instruction_count,
				regs->pc, &block_decoded);
			if (block_decoded_valid) {
				block_fault_access =
					orlix_tcti_fault_access_for_decoded(&block_decoded);
				block_instruction = block_decoded.instruction;
			}
			ret = orlix_tcti_execute_authorized_capture(
				mm, regs, block->program, block->program_words,
				&fault_address, block->code_generation,
				&result.entry_valid, &result.entry_pc,
				&result.entry_instruction, capture,
				&successful_gadget_execution);
			if (atomic_dec_if_positive(&orlix_tcti_block_trace_budget) >= 0)
		pr_debug("OrlixTCTI: block exec task=%s pid=%d start_pc=%#llx end_pc=%#llx before_lr=%#llx after_lr=%#llx before_sp=%#llx after_sp=%#llx insn=%#x ret=%d words=%u count=%u cached=1\n",
					task->comm, task_pid_nr(task),
					before_pc, regs->pc, before_lr,
					regs->regs[30], before_sp, regs->sp,
					block_instruction, ret, block_program_words,
					block->instruction_count);
			if (ret == -EFAULT || ret == -EACCES) {
				block_decoded_valid = orlix_tcti_decoded_for_program_pc(
					block->program, block->program_words,
					block->guest_start_pc,
					block->instruction_count, regs->pc,
					&block_decoded);
				if (block_decoded_valid) {
					block_fault_access =
						orlix_tcti_fault_access_for_decoded(
							&block_decoded);
					block_instruction =
						block_decoded.instruction;
				}
			}
			if (global_cache_ref)
				orlix_tcti_block_put(block);
			if (!ret)
				continue;
			if (ret == -ESTALE)
				continue;
			if (ret == -EAGAIN) {
				orlix_tcti_set_yield_result(mm, regs, &result);
				ORLIX_TCTI_CAPTURE_RELEASE_AND_RETURN(capture, result, regs,
							       hot_blocks, normal_resume,
							       successful_gadget_execution);
			}

			if (ret == -EFAULT || ret == -EACCES) {
				pr_info("OrlixTCTI: cached block fault task=%s pid=%d pc=%#llx ret=%d fault=%#lx insn=%#x code_generation=%llu words=%u\n",
					task->comm, task_pid_nr(task), regs->pc,
					ret, fault_address, block_instruction,
					(unsigned long long)code_generation,
					block_program_words);
				result.reason =
					ret == -EFAULT && block_decoded_valid &&
					orlix_tcti_lse_alignment_fault(
						&block_decoded, fault_address) ?
					ORLIX_TCTI_EXIT_ALIGNMENT_FAULT :
					ORLIX_TCTI_EXIT_USER_FAULT;
				result.status = ret;
				result.fault_address = fault_address;
				result.fault_access = block_fault_access;
				result.pc = regs->pc;
				result.instruction = block_instruction;
				ORLIX_TCTI_CAPTURE_RELEASE_AND_RETURN(capture, result, regs,
							       hot_blocks, normal_resume,
							       successful_gadget_execution);
			}

			result.reason = ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION;
			result.status = ret;
			result.pc = regs->pc;
			result.instruction = block_instruction;
			ORLIX_TCTI_CAPTURE_RELEASE_AND_RETURN(capture, result, regs,
						       hot_blocks, normal_resume,
						       successful_gadget_execution);
		}

		ret = orlix_tcti_build_straight_line_block(mm, regs->pc,
						     test_profile, program,
						     ARRAY_SIZE(program),
						     &word_count,
						     &block_instruction_count,
						     &instruction,
						     &first_exit_class);
		if (code_generation != orlix_tcti_code_generation(mm))
			continue;
		if (ret == -EINTR) {
			/* Exception exits bypass a gadget, but remain a real decoder path. */
			decoded = orlix_tcti_decode_aarch64(instruction);
			orlix_tcti_native_capture_before_decoded(capture, mm, regs,
							 &decoded);
			if (!result.entry_valid) {
				result.entry_valid = true;
				result.entry_pc = regs->pc;
				result.entry_instruction = instruction;
			}
			if (first_exit_class == ORLIX_TCTI_DECODE_SVC) {
				result.reason = ORLIX_TCTI_EXIT_SYSCALL;
				result.status = 0;
			} else if (first_exit_class == ORLIX_TCTI_DECODE_BRK) {
				result.reason = ORLIX_TCTI_EXIT_BREAKPOINT;
				result.status = (instruction >> 5) & 0xffffU;
			} else {
				result.reason = ORLIX_TCTI_EXIT_UNDEFINED_INSTRUCTION;
				result.status = first_exit_class == ORLIX_TCTI_DECODE_HLT ?
					(instruction >> 5) & 0xffffU : 0;
			}
			result.pc = regs->pc;
			result.instruction = instruction;
			ORLIX_TCTI_CAPTURE_RELEASE_AND_RETURN(capture, result, regs,
						       hot_blocks, normal_resume,
						       successful_gadget_execution);
		}
		if (ret) {
			result.status = ret;
			result.pc = regs->pc;
			result.instruction = instruction;
			if (ret == -EFAULT || ret == -EACCES) {
				pr_info("OrlixTCTI: fetch failed task=%s pid=%d pc=%#llx ret=%d\n",
					task->comm, task_pid_nr(task), regs->pc,
					ret);
				result.reason = ret == -EFAULT &&
					!IS_ALIGNED(regs->pc, sizeof(u32)) ?
					ORLIX_TCTI_EXIT_ALIGNMENT_FAULT :
					ORLIX_TCTI_EXIT_USER_FAULT;
				result.fault_address = regs->pc;
				result.fault_access = ORLIX_TCTI_ACCESS_FETCH;
			} else {
				result.reason = ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION;
			}
			ORLIX_TCTI_CAPTURE_RELEASE_AND_RETURN(capture, result, regs,
						       hot_blocks, normal_resume,
						       successful_gadget_execution);
		}

		decoded = orlix_tcti_decode_aarch64(instruction);
		ret = cache_allowed ? orlix_tcti_block_cache_insert(
			mm, block_pc,
			block_pc + block_instruction_count * sizeof(u32),
			code_generation, block_instruction_count, program,
			word_count, &block) : -EOPNOTSUPP;
		if (ret == -ESTALE)
			continue;
		if (!ret && block) {
			unsigned long long before_pc = regs->pc;
			unsigned long long before_sp = regs->sp;
			unsigned long long before_lr = regs->regs[30];

			orlix_tcti_hot_blocks_remember(hot_blocks, &hot_block_cursor,
						 block);
			ret = orlix_tcti_execute_authorized_capture(
				mm, regs, block->program, block->program_words,
				&fault_address, block->code_generation,
				&result.entry_valid, &result.entry_pc,
				&result.entry_instruction, capture,
				&successful_gadget_execution);
			if (atomic_dec_if_positive(&orlix_tcti_block_trace_budget) >= 0)
		pr_debug("OrlixTCTI: block exec task=%s pid=%d start_pc=%#llx end_pc=%#llx before_lr=%#llx after_lr=%#llx before_sp=%#llx after_sp=%#llx insn=%#x ret=%d words=%u count=%u cached=0\n",
					task->comm, task_pid_nr(task),
					before_pc, regs->pc, before_lr,
					regs->regs[30], before_sp, regs->sp,
					instruction, ret, block->program_words,
					block->instruction_count);
			orlix_tcti_block_put(block);
		} else {
			unsigned long long before_pc = regs->pc;
			unsigned long long before_sp = regs->sp;
			unsigned long long before_lr = regs->regs[30];

			ret = orlix_tcti_execute_authorized_capture(
				mm, regs, program, word_count, &fault_address,
				code_generation, &result.entry_valid,
				&result.entry_pc, &result.entry_instruction, capture,
				&successful_gadget_execution);
			if (atomic_dec_if_positive(&orlix_tcti_block_trace_budget) >= 0)
		pr_debug("OrlixTCTI: block exec task=%s pid=%d start_pc=%#llx end_pc=%#llx before_lr=%#llx after_lr=%#llx before_sp=%#llx after_sp=%#llx insn=%#x ret=%d words=%zu count=%u cached=0\n",
					task->comm, task_pid_nr(task),
					before_pc, regs->pc, before_lr,
					regs->regs[30], before_sp, regs->sp,
					instruction, ret, word_count,
					block_instruction_count);
		}
		if (!ret)
			continue;
		if (ret == -ESTALE)
			continue;
		if (ret == -EAGAIN) {
			orlix_tcti_set_yield_result(mm, regs, &result);
			ORLIX_TCTI_CAPTURE_RELEASE_AND_RETURN(capture, result, regs,
						       hot_blocks, normal_resume,
						       successful_gadget_execution);
		}

		if (ret == -EFAULT || ret == -EACCES) {
			fault_decoded = orlix_tcti_decoded_for_program_pc(
				program, word_count, block_pc,
				block_instruction_count, regs->pc, &decoded);

			if (fault_decoded)
				instruction = decoded.instruction;
			result.reason =
				ret == -EFAULT && fault_decoded &&
				orlix_tcti_lse_alignment_fault(&decoded,
							 fault_address) ?
				ORLIX_TCTI_EXIT_ALIGNMENT_FAULT :
				ORLIX_TCTI_EXIT_USER_FAULT;
			result.status = ret;
			result.fault_address = fault_address;
			result.fault_access = orlix_tcti_fault_access_for_decoded(&decoded);
			result.pc = regs->pc;
			result.instruction = instruction;
			ORLIX_TCTI_CAPTURE_RELEASE_AND_RETURN(capture, result, regs,
						       hot_blocks, normal_resume,
						       successful_gadget_execution);
		}

		result.reason = ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION;
		result.status = ret;
		result.pc = regs->pc;
		result.instruction = instruction;
		ORLIX_TCTI_CAPTURE_RELEASE_AND_RETURN(capture, result, regs,
					       hot_blocks, normal_resume,
					       successful_gadget_execution);
	}
}

struct orlix_tcti_result orlix_tcti_resume_user(struct task_struct *task,
				    struct pt_regs *regs, struct mm_struct *mm)
{
	return orlix_tcti_resume_user_internal(task, regs, mm,
		orlix_tcti_native_capture_claim_resume(task), true, NULL);
}

struct orlix_tcti_result orlix_tcti_resume_user_captured(
	struct task_struct *task, struct pt_regs *regs, struct mm_struct *mm,
	struct orlix_tcti_native_capture *capture)
{
	return orlix_tcti_resume_user_internal(task, regs, mm, capture, false,
		NULL);
}

#if IS_ENABLED(CONFIG_ORLIX_TCTI_KUNIT_TEST)
struct orlix_tcti_result orlix_tcti_resume_user_with_feature_profile_for_tests(
	struct task_struct *task, struct pt_regs *regs, struct mm_struct *mm,
	const struct orlix_tcti_test_feature_profile *profile)
{
	if (!profile)
		return (struct orlix_tcti_result) {
			.reason = ORLIX_TCTI_EXIT_TASK_EXIT,
			.status = -EINVAL,
		};
	return orlix_tcti_resume_user_internal(task, regs, mm, NULL, false,
		profile);
}
#endif

void orlix_tcti_prepare_syscall_handoff(struct pt_regs *regs)
{
	current->thread.user_exclusive_address = 0;
	current->thread.user_exclusive_value = 0;
	current->thread.user_exclusive_value2 = 0;
	current->thread.user_exclusive_pfn = 0;
	current->thread.user_exclusive_generation = 0;
	current->thread.user_exclusive_mapping_generation = 0;
	current->thread.user_exclusive_size = 0;
	current->thread.user_exclusive_valid = 0;
	regs->orig_x0 = regs->regs[0];
	regs->syscallno = regs->regs[8];
	regs->pc += sizeof(u32);
}

bool orlix_tcti_prepare_successful_execve_return(struct pt_regs *regs)
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

static void orlix_tcti_refresh_current_user_range(unsigned long start, size_t length)
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

static void orlix_tcti_sync_syscall_user_ranges(struct pt_regs *regs,
					  unsigned long nr)
{
	if (!regs || IS_ERR_VALUE(regs->regs[0]))
		return;

	switch (nr) {
	case __NR_read:
		orlix_tcti_refresh_current_user_range(regs->regs[1], regs->regs[0]);
		break;
	case __NR_ioctl:
		if (regs->regs[1] == TIOCGWINSZ)
			orlix_tcti_refresh_current_user_range(regs->regs[2],
							sizeof(struct winsize));
		break;
	case __NR_rt_sigaction:
		orlix_tcti_refresh_current_user_range(regs->regs[2],
						sizeof(struct sigaction));
		break;
	case __NR_rt_sigprocmask:
		orlix_tcti_refresh_current_user_range(regs->regs[2],
						sizeof(sigset_t));
		break;
	case __NR_uname:
		orlix_tcti_refresh_current_user_range(regs->orig_x0,
						sizeof(struct new_utsname));
		break;
	default:
		break;
	}
}

static bool orlix_tcti_syscall_changes_user_mappings(unsigned long nr)
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

/*
 * The syscall dispatcher returns after Linux has changed a user mapping.  The
 * block cache alone is not an authorization boundary: the per-mm mapping
 * sequence also invalidates task-local TLB entries that retain host-page
 * references.  Keep that transition in the arch/mm owner.
 */
static void orlix_tcti_invalidate_changed_user_mappings(struct mm_struct *mm,
					  unsigned long nr, long status)
{
	if (mm && !IS_ERR_VALUE(status) && orlix_tcti_syscall_changes_user_mappings(nr))
		orlix_tcti_invalidate_mm(mm);
}

#if IS_ENABLED(CONFIG_ORLIX_TCTI_KUNIT_TEST)
bool orlix_tcti_syscall_changes_user_mappings_for_tests(unsigned long nr)
{
	return orlix_tcti_syscall_changes_user_mappings(nr);
}

void orlix_tcti_invalidate_changed_user_mappings_for_tests(struct mm_struct *mm,
						      unsigned long nr,
						      long status)
{
	orlix_tcti_invalidate_changed_user_mappings(mm, nr, status);
}
#endif

static void orlix_tcti_trace_execve_user_argv(const struct pt_regs *regs)
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

	pr_debug("OrlixTCTI: execve argv task=%s pid=%d filename_ptr=%#llx filename=\"%s\" argv_ptr=%#llx envp_ptr=%#llx\n",
		current->comm, task_pid_nr(current), regs->regs[0],
		filename_buf, regs->regs[1], regs->regs[2]);

	for (i = 0; i < 6; i++) {
		const char __user *argp = NULL;
		char arg_buf[160];

		if (copy_from_user(&argp, argv + i, sizeof(argp))) {
			pr_info("OrlixTCTI: execve argv task=%s pid=%d argv%d_ptr=<fault>\n",
				current->comm, task_pid_nr(current), i);
			break;
		}
		if (!argp) {
			pr_debug("OrlixTCTI: execve argv task=%s pid=%d argv%d_ptr=NULL\n",
				current->comm, task_pid_nr(current), i);
			break;
		}

		copied = strncpy_from_user(arg_buf, argp,
					   sizeof(arg_buf) - 1);
		if (copied < 0)
			strscpy(arg_buf, "<fault>", sizeof(arg_buf));
		else
			arg_buf[sizeof(arg_buf) - 1] = '\0';
		pr_debug("OrlixTCTI: execve argv task=%s pid=%d argv%d_ptr=%px argv%d=\"%s\"\n",
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
		orlix_tcti_trace_execve_user_argv(regs);
	orlix_tcti_prepare_syscall_handoff(regs);
	orlix_syscall_dispatch(regs);
	task_regs = task_pt_regs(current);
	if (atomic_dec_if_positive(&post_dispatch_report_budget) >= 0)
		pr_debug("OrlixTCTI: syscall post-dispatch task=%s pid=%d syscall=%lu entry_pc=%#lx regs_pc=%#llx task_regs_pc=%#llx ret=%#llx syscallno=%d task_regs_syscallno=%d sp=%#llx task_regs_sp=%#llx x30=%#llx task_regs_x30=%#llx\n",
			current->comm, task_pid_nr(current), nr, pc,
			regs->pc, task_regs ? task_regs->pc : 0,
			regs->regs[0], regs->syscallno,
			task_regs ? task_regs->syscallno : NO_SYSCALL,
			regs->sp, task_regs ? task_regs->sp : 0,
			regs->regs[30], task_regs ? task_regs->regs[30] : 0);
	orlix_tcti_invalidate_changed_user_mappings(current->mm, nr, regs->regs[0]);
	orlix_tcti_sync_syscall_user_ranges(regs, nr);
	orlix_tcti_report_syscall_return(current, regs, nr, pc);
}

static void
orlix_tcti_apply_static_pie_relocations_or_exit(struct pt_regs *regs,
						unsigned long *applied_base)
{
	int relocation_ret;

	relocation_ret = orlix_tcti_apply_static_pie_relative_relocations(
		current, regs, current->mm, applied_base);
	if (relocation_ret) {
		struct orlix_tcti_result result = {
			.reason = ORLIX_TCTI_EXIT_USER_FAULT,
			.status = relocation_ret,
			.fault_address = regs->pc,
			.fault_access = ORLIX_TCTI_ACCESS_READ,
			.pc = regs->pc,
		};

		orlix_tcti_report_exit(current, regs, &result);
		do_group_exit(SIGSEGV);
	}
}

static bool orlix_tcti_handle_user_fault_exit(
	struct pt_regs *regs, const struct orlix_tcti_result *result)
{
	if (!result)
		return false;

	if (!orlix_tcti_handle_user_fault(regs, result->fault_address,
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
		struct orlix_tcti_result result;

		result = orlix_tcti_resume_user(current, regs, current->mm);
		switch (result.reason) {
		case ORLIX_TCTI_EXIT_SYSCALL:
		{
			bool syscall_was_execve = regs->regs[8] == __NR_execve;

			orlix_tcti_report_syscall(current, regs, &result);
			orlix_tcti_handle_syscall(regs);
			if (syscall_was_execve && !IS_ERR_VALUE(regs->regs[0])) {
				regs = task_pt_regs(current);
				applied_static_pie_base = 0;
				if (current->mm)
					current->mm->context.orlix_tcti_static_pie_base = 0;
					if (orlix_tcti_prepare_successful_execve_return(regs))
						pr_info("OrlixTCTI: restored EL0 pstate after execve task=%s pid=%d pc=%#llx sp=%#llx pstate=%#llx\n",
							current->comm, task_pid_nr(current),
							regs->pc, regs->sp, regs->pstate);
				}
				orlix_tcti_apply_static_pie_relocations_or_exit(
					regs, &applied_static_pie_base);
			break;
		}
		case ORLIX_TCTI_EXIT_BREAKPOINT:
			force_sig_fault(SIGTRAP, TRAP_BRKPT,
					(void __user *)result.pc);
			orlix_exit_to_user_mode_work(regs);
			break;
		case ORLIX_TCTI_EXIT_YIELD:
			cond_resched();
			orlix_exit_to_user_mode_work(regs);
			break;
		case ORLIX_TCTI_EXIT_USER_FAULT:
			if (orlix_tcti_handle_user_fault_exit(regs, &result)) {
				regs = task_pt_regs(current);
				break;
			}
			orlix_tcti_report_exit(current, regs, &result);
			do_group_exit(SIGSEGV);
			break;
		case ORLIX_TCTI_EXIT_ALIGNMENT_FAULT:
			force_sig_fault(SIGBUS, BUS_ADRALN,
					(void __user *)result.fault_address);
			orlix_exit_to_user_mode_work(regs);
			break;
		case ORLIX_TCTI_EXIT_UNDEFINED_INSTRUCTION:
			force_sig_fault(SIGILL, ILL_ILLOPC,
					(void __user *)result.pc);
			orlix_exit_to_user_mode_work(regs);
			break;
		case ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION:
			orlix_tcti_report_unsupported(current, regs, &result);
			do_group_exit(SIGILL);
			break;
		case ORLIX_TCTI_EXIT_TASK_EXIT:
			do_group_exit(SIGKILL);
			break;
		default:
			orlix_tcti_report_exit(current, regs, &result);
			do_group_exit(SIGKILL);
			break;
		}
		cpu_relax();
	}
}
