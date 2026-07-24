/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _ASM_ORLIX_HOSTED_EXEC_H
#define _ASM_ORLIX_HOSTED_EXEC_H

#include <linux/types.h>

struct pt_regs;
struct task_struct;
struct mm_struct;

#if defined(ORLIX_APP_HOSTED_BOOT)
void orlix_hosted_capture_host_context(void);
void orlix_hosted_save_kernel_stack(unsigned long sp);
void orlix_hosted_preserve_user_tls(void);
void orlix_hosted_switch_user_tls(struct task_struct *next);
void orlix_hosted_set_current_user_tls(unsigned long user_tls);
unsigned long orlix_hosted_prepare_user_entry(unsigned long entry_user_tls);
void orlix_hosted_note_user_entry_tls(unsigned long user_tls);
void __noreturn orlix_hosted_enter_user(struct pt_regs *regs);
#ifdef CONFIG_ORLIX_HOSTED_EXEC_NATIVE
int orlix_hosted_sync_syscall_gate(void);
#else
static inline int orlix_hosted_sync_syscall_gate(void)
{
	return 0;
}
#endif
int orlix_try_sync_current_user_mappings(struct pt_regs *regs);
int orlix_try_sync_current_user_minimal_mappings(struct pt_regs *regs);
void orlix_sync_current_user_mappings(struct pt_regs *regs);
void orlix_sync_current_user_minimal_mappings(struct pt_regs *regs);
int orlix_sync_current_user_mapping_page(unsigned long address);
int orlix_refresh_current_user_mapping_page(unsigned long address);
int orlix_refresh_current_user_mapping_page_from_kernel(unsigned long address,
							 const void *source_page);
int orlix_refresh_user_mapping_page_from_kernel(struct mm_struct *mm,
						 unsigned long address,
						 const void *source_page);
int orlix_refresh_current_user_mapping_range_from_kernel(unsigned long address,
							  const void *source_page,
							  size_t length);
int orlix_refresh_user_mapping_range_from_kernel(struct mm_struct *mm,
						  unsigned long address,
						  const void *source_page,
						  size_t length);
void orlix_host_user_unmap_pages_serialized(unsigned long address,
					    unsigned long length);
void orlix_host_user_discard_pages_serialized(unsigned long address,
					      unsigned long length);
int orlix_sync_current_user_fault_window(unsigned long address,
					 unsigned long fault_flags);
int orlix_sync_hosted_kernel_fault(unsigned long address);
int orlix_handle_host_user_fault(struct pt_regs *regs, unsigned long address,
				 unsigned long fault_flags);
void orlix_hosted_dump_recent_user_events(void);
long orlix_hosted_syscall_dispatch(unsigned long scno, unsigned long arg0,
				   unsigned long arg1, unsigned long arg2,
				   unsigned long arg3, unsigned long arg4,
				   unsigned long arg5, unsigned long user_sp,
				   unsigned long user_tls);
void __noreturn orlix_hosted_syscall_enter_user(void);
#endif

long orlix_syscall_dispatch(struct pt_regs *regs);

#endif /* _ASM_ORLIX_HOSTED_EXEC_H */
