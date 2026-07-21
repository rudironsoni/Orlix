/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _ASM_ORLIX_TCTI_H
#define _ASM_ORLIX_TCTI_H

#include <linux/compiler.h>
#include <linux/types.h>

struct mm_struct;
struct page;
struct pt_regs;
struct task_struct;

enum tcti_exit_reason {
	TCTI_EXIT_SYSCALL,
	TCTI_EXIT_BREAKPOINT,
	TCTI_EXIT_USER_FAULT,
	TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
	TCTI_EXIT_SIGNAL_POINT,
	TCTI_EXIT_YIELD,
	TCTI_EXIT_TASK_EXIT,
};

enum tcti_access {
	TCTI_ACCESS_FETCH,
	TCTI_ACCESS_READ,
	TCTI_ACCESS_WRITE,
};

struct tcti_result {
	enum tcti_exit_reason reason;
	long status;
	unsigned long fault_address;
	enum tcti_access fault_access;
	unsigned long pc;
	u32 instruction;
};

struct tcti_user_page {
	unsigned long user_page;
	void *host_data;
	struct page *page;
	unsigned long linux_perms;
	u32 translation_generation;
	u32 code_generation;
	bool cow_sensitive;
	bool has_translated_blocks;
};

struct tcti_result tcti_resume_user(struct task_struct *task,
				    struct pt_regs *regs,
				    struct mm_struct *mm);
void __noreturn orlix_tcti_enter_user(struct pt_regs *regs);

int tcti_pin_user_page(struct mm_struct *mm, unsigned long user_va,
		       enum tcti_access access,
		       struct tcti_user_page *out);
void tcti_unpin_user_page(struct tcti_user_page *page);
int tcti_fetch_instruction(struct mm_struct *mm, unsigned long pc,
			   u32 *instruction);
int tcti_read_user_data(struct mm_struct *mm, unsigned long user_va,
			void *buffer, size_t size);
int tcti_write_user_data(struct mm_struct *mm, unsigned long user_va,
			  const void *buffer, size_t size);
int tcti_compare_exchange_user_data(struct mm_struct *mm,
				     unsigned long user_va,
				     const void *expected,
				     const void *desired,
				     size_t size, bool *exchanged);
int tcti_handle_user_fault(struct pt_regs *regs, unsigned long address,
			   enum tcti_access access);
void tcti_invalidate_mm(struct mm_struct *mm);

#endif /* _ASM_ORLIX_TCTI_H */
