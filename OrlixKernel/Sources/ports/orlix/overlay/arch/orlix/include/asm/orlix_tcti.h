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

enum orlix_tcti_exit_reason {
	ORLIX_TCTI_EXIT_SYSCALL,
	ORLIX_TCTI_EXIT_BREAKPOINT,
	ORLIX_TCTI_EXIT_USER_FAULT,
	ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
	ORLIX_TCTI_EXIT_SIGNAL_POINT,
	ORLIX_TCTI_EXIT_YIELD,
	ORLIX_TCTI_EXIT_TASK_EXIT,
	ORLIX_TCTI_EXIT_ALIGNMENT_FAULT,
	ORLIX_TCTI_EXIT_UNDEFINED_INSTRUCTION,
};

enum orlix_tcti_access {
	ORLIX_TCTI_ACCESS_FETCH,
	ORLIX_TCTI_ACCESS_READ,
	ORLIX_TCTI_ACCESS_WRITE,
};

/* SVE state is task-local and always interpreted by OrlixTCTI C semantics. */
#define ORLIX_TCTI_SVE_MIN_VL_BYTES	16U
#define ORLIX_TCTI_SVE_MAX_VL_BYTES	256U
#define ORLIX_TCTI_SVE_DEFAULT_VL_BYTES	ORLIX_TCTI_SVE_MAX_VL_BYTES
#define ORLIX_TCTI_SVE_ZREG_COUNT	32U
#define ORLIX_TCTI_SVE_PREG_COUNT	16U
#define ORLIX_TCTI_SVE_PREG_MAX_BYTES	(ORLIX_TCTI_SVE_MAX_VL_BYTES / 8U)

enum orlix_tcti_sve_crypto_feature_state {
	ORLIX_TCTI_SVE_CRYPTO_FEATURES_UNAVAILABLE,
	ORLIX_TCTI_SVE_CRYPTO_FEATURES_ACTIVE_PROFILE,
};

struct orlix_tcti_sve_state {
	u16 vl_bytes;
	/* #155 derives this from the immutable active execution profile. */
	enum orlix_tcti_sve_crypto_feature_state crypto_feature_state;
	u32 crypto_features;
	u8 z[ORLIX_TCTI_SVE_ZREG_COUNT][ORLIX_TCTI_SVE_MAX_VL_BYTES];
	u8 p[ORLIX_TCTI_SVE_PREG_COUNT][ORLIX_TCTI_SVE_PREG_MAX_BYTES];
	u8 ffr[ORLIX_TCTI_SVE_PREG_MAX_BYTES];
	bool valid;
};

/* SME allocation is task-owned and sized from SVL. It has no wire maximum. */
struct orlix_tcti_sme_state {
	u16 svl_bytes;
	u8 *za;
	u8 *zt0;
	size_t za_bytes;
	size_t zt0_bytes;
	bool streaming_mode;
	bool za_enabled;
	bool zt0_valid;
	bool valid;
};

int orlix_tcti_sve_state_reset(struct orlix_tcti_sve_state *state,
			 unsigned long *user_simd, u16 vl_bytes);
int orlix_tcti_sve_state_copy(struct orlix_tcti_sve_state *destination,
			unsigned long *destination_simd,
			const struct orlix_tcti_sve_state *source,
			const unsigned long *source_simd);
int orlix_tcti_sme_state_reset(struct orlix_tcti_sme_state *state,
				u16 svl_bytes, bool streaming_mode, bool za_enabled,
				bool zt0_valid);
int orlix_tcti_sme_state_copy(struct orlix_tcti_sme_state *destination,
			       const struct orlix_tcti_sme_state *source);
void orlix_tcti_sme_state_release(struct orlix_tcti_sme_state *state);

/*
 * These are the acquire/release variants encoded by AArch64 atomics.  There
 * is deliberately no implicit seq_cst variant: the decoder must select the
 * ordering specified by the guest instruction.
 */
enum orlix_tcti_atomic_memory_order {
	ORLIX_TCTI_ATOMIC_MEMORY_RELAXED,
	ORLIX_TCTI_ATOMIC_MEMORY_ACQUIRE,
	ORLIX_TCTI_ATOMIC_MEMORY_RELEASE,
	ORLIX_TCTI_ATOMIC_MEMORY_ACQ_REL,
};

enum orlix_tcti_atomic_memory_operation {
	ORLIX_TCTI_ATOMIC_MEMORY_CAS,
	ORLIX_TCTI_ATOMIC_MEMORY_SWP,
	ORLIX_TCTI_ATOMIC_MEMORY_ADD,
	ORLIX_TCTI_ATOMIC_MEMORY_CLR,
	ORLIX_TCTI_ATOMIC_MEMORY_EOR,
	ORLIX_TCTI_ATOMIC_MEMORY_SET,
	ORLIX_TCTI_ATOMIC_MEMORY_SMAX,
	ORLIX_TCTI_ATOMIC_MEMORY_SMIN,
	ORLIX_TCTI_ATOMIC_MEMORY_UMAX,
	ORLIX_TCTI_ATOMIC_MEMORY_UMIN,
};

struct orlix_tcti_result {
	enum orlix_tcti_exit_reason reason;
	long status;
	unsigned long fault_address;
	enum orlix_tcti_access fault_access;
	unsigned long pc;
	u32 instruction;
	bool entry_valid;
	unsigned long entry_pc;
	u32 entry_instruction;
};

struct orlix_tcti_user_page {
	unsigned long user_page;
	void *host_data;
	struct page *page;
	unsigned long linux_perms;
	u64 translation_generation;
	u64 code_generation;
	bool cow_sensitive;
	bool has_translated_blocks;
};

struct orlix_tcti_result orlix_tcti_resume_user(struct task_struct *task,
				    struct pt_regs *regs,
				    struct mm_struct *mm);
void __noreturn orlix_tcti_enter_user(struct pt_regs *regs);

int orlix_tcti_pin_user_page(struct mm_struct *mm, unsigned long user_va,
		       enum orlix_tcti_access access,
		       struct orlix_tcti_user_page *out);
void orlix_tcti_unpin_user_page(struct orlix_tcti_user_page *page);
int orlix_tcti_fetch_instruction(struct mm_struct *mm, unsigned long pc,
			   u32 *instruction);
int orlix_tcti_read_user_data(struct mm_struct *mm, unsigned long user_va,
			void *buffer, size_t size);
int orlix_tcti_write_user_data(struct mm_struct *mm, unsigned long user_va,
			  const void *buffer, size_t size);
int orlix_tcti_load_exclusive_user_data(struct mm_struct *mm,
				  unsigned long user_va, void *buffer, size_t size,
				  unsigned long *pfn, u64 *generation,
				  u64 *mapping_generation);
int orlix_tcti_store_exclusive_user_data(struct mm_struct *mm,
				   unsigned long user_va, const void *buffer,
				   size_t size, unsigned long reserved_pfn,
				   u64 reserved_generation,
				   u64 reserved_mapping_generation, bool *stored);
int orlix_tcti_atomic_user_data(struct mm_struct *mm, unsigned long user_va,
			  enum orlix_tcti_atomic_memory_operation operation,
			  enum orlix_tcti_atomic_memory_order order,
			  const void *expected, const void *operand,
			  void *old_value, size_t size, bool *exchanged);
int orlix_tcti_compare_exchange_user_data(struct mm_struct *mm,
				     unsigned long user_va,
				     const void *expected,
				     const void *desired,
				     size_t size, bool *exchanged);
int orlix_tcti_handle_user_fault(struct pt_regs *regs, unsigned long address,
			   enum orlix_tcti_access access);
void orlix_tcti_invalidate_mm(struct mm_struct *mm);
void orlix_tcti_invalidate_all(void);
void orlix_tcti_invalidate_vma(struct vm_area_struct *vma);
void orlix_tcti_invalidate_range(struct mm_struct *mm, unsigned long start,
			   unsigned long end);
bool orlix_tcti_mapping_access_lock(struct mm_struct *mm, u64 generation);
void orlix_tcti_mapping_access_unlock(struct mm_struct *mm);
void orlix_tcti_mapping_sequence_begin(struct mm_struct *mm);
void orlix_tcti_mapping_sequence_end(struct mm_struct *mm);
void orlix_tcti_note_pte_update(struct mm_struct *mm);
void orlix_tcti_note_pte_update_range(struct mm_struct *mm, unsigned long start,
				unsigned long end);
void orlix_tcti_note_pte_update_address(struct mm_struct *mm,
				  unsigned long address);
void orlix_tcti_flush_task_state(struct task_struct *task);
void orlix_tcti_release_task_state(struct task_struct *task);
void orlix_tcti_prepare_signal_delivery(void);

#endif /* _ASM_ORLIX_TCTI_H */
