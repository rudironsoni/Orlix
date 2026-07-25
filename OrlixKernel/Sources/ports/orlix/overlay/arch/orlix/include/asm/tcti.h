/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _ASM_ORLIX_TCTI_H
#define _ASM_ORLIX_TCTI_H

#include <linux/compiler.h>
#include <linux/types.h>

struct mm_struct;
struct page;
struct pt_regs;
struct task_struct;
struct vm_area_struct;

enum tcti_exit_reason {
	TCTI_EXIT_SYSCALL,
	TCTI_EXIT_BREAKPOINT,
	TCTI_EXIT_USER_FAULT,
	TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
	TCTI_EXIT_SIGNAL_POINT,
	TCTI_EXIT_YIELD,
	TCTI_EXIT_TASK_EXIT,
	TCTI_EXIT_ALIGNMENT_FAULT,
};

enum tcti_access {
	TCTI_ACCESS_FETCH,
	TCTI_ACCESS_READ,
	TCTI_ACCESS_WRITE,
};

/* SVE state is task-local and always interpreted by TCTI C semantics. */
#define TCTI_SVE_MIN_VL_BYTES	16U
#define TCTI_SVE_MAX_VL_BYTES	256U
#define TCTI_SVE_DEFAULT_VL_BYTES	TCTI_SVE_MAX_VL_BYTES
#define TCTI_SVE_ZREG_COUNT	32U
#define TCTI_SVE_PREG_COUNT	16U
#define TCTI_SVE_PREG_MAX_BYTES	(TCTI_SVE_MAX_VL_BYTES / 8U)

struct tcti_sve_state {
	u16 vl_bytes;
	u8 z[TCTI_SVE_ZREG_COUNT][TCTI_SVE_MAX_VL_BYTES];
	u8 p[TCTI_SVE_PREG_COUNT][TCTI_SVE_PREG_MAX_BYTES];
	u8 ffr[TCTI_SVE_PREG_MAX_BYTES];
	bool valid;
};

int tcti_sve_state_reset(struct tcti_sve_state *state,
			 unsigned long *user_simd, u16 vl_bytes);
int tcti_sve_state_copy(struct tcti_sve_state *destination,
			unsigned long *destination_simd,
			const struct tcti_sve_state *source,
			const unsigned long *source_simd);

/*
 * These are the acquire/release variants encoded by AArch64 atomics.  There
 * is deliberately no implicit seq_cst variant: the decoder must select the
 * ordering specified by the guest instruction.
 */
enum tcti_atomic_memory_order {
	TCTI_ATOMIC_MEMORY_RELAXED,
	TCTI_ATOMIC_MEMORY_ACQUIRE,
	TCTI_ATOMIC_MEMORY_RELEASE,
	TCTI_ATOMIC_MEMORY_ACQ_REL,
};

enum tcti_atomic_memory_operation {
	TCTI_ATOMIC_MEMORY_CAS,
	TCTI_ATOMIC_MEMORY_SWP,
	TCTI_ATOMIC_MEMORY_ADD,
	TCTI_ATOMIC_MEMORY_CLR,
	TCTI_ATOMIC_MEMORY_EOR,
	TCTI_ATOMIC_MEMORY_SET,
	TCTI_ATOMIC_MEMORY_SMAX,
	TCTI_ATOMIC_MEMORY_SMIN,
	TCTI_ATOMIC_MEMORY_UMAX,
	TCTI_ATOMIC_MEMORY_UMIN,
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
	u64 translation_generation;
	u64 code_generation;
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
int tcti_load_exclusive_user_data(struct mm_struct *mm,
				  unsigned long user_va, void *buffer, size_t size,
				  unsigned long *pfn, u64 *generation,
				  u64 *mapping_generation);
int tcti_store_exclusive_user_data(struct mm_struct *mm,
				   unsigned long user_va, const void *buffer,
				   size_t size, unsigned long reserved_pfn,
				   u64 reserved_generation,
				   u64 reserved_mapping_generation, bool *stored);
int tcti_atomic_user_data(struct mm_struct *mm, unsigned long user_va,
			  enum tcti_atomic_memory_operation operation,
			  enum tcti_atomic_memory_order order,
			  const void *expected, const void *operand,
			  void *old_value, size_t size, bool *exchanged);
int tcti_compare_exchange_user_data(struct mm_struct *mm,
				     unsigned long user_va,
				     const void *expected,
				     const void *desired,
				     size_t size, bool *exchanged);
int tcti_handle_user_fault(struct pt_regs *regs, unsigned long address,
			   enum tcti_access access);
void tcti_invalidate_mm(struct mm_struct *mm);
void tcti_invalidate_all(void);
void tcti_invalidate_vma(struct vm_area_struct *vma);
void tcti_invalidate_range(struct mm_struct *mm, unsigned long start,
			   unsigned long end);
bool tcti_mapping_access_lock(struct mm_struct *mm, u64 generation);
void tcti_mapping_access_unlock(struct mm_struct *mm);
void tcti_mapping_sequence_begin(struct mm_struct *mm);
void tcti_mapping_sequence_end(struct mm_struct *mm);
void tcti_note_pte_update(struct mm_struct *mm);
void tcti_note_pte_update_range(struct mm_struct *mm, unsigned long start,
				unsigned long end);
void tcti_note_pte_update_address(struct mm_struct *mm,
				  unsigned long address);
void tcti_flush_task_state(struct task_struct *task);
void tcti_release_task_state(struct task_struct *task);
void tcti_prepare_signal_delivery(void);

#endif /* _ASM_ORLIX_TCTI_H */
