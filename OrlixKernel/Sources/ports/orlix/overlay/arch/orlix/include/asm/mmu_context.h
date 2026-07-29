/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _ASM_ORLIX_MMU_CONTEXT_H
#define _ASM_ORLIX_MMU_CONTEXT_H

#include <asm-generic/mm_hooks.h>
#include <asm/mte.h>
#include <asm/orlix_tcti.h>
#include <asm/tlbflush.h>

struct mm_struct;
struct task_struct;

#define init_new_context init_new_context
static inline int init_new_context(struct task_struct *tsk,
				   struct mm_struct *mm)
{
	(void)tsk;
#if defined(ORLIX_APP_HOSTED_BOOT) && defined(CONFIG_ORLIX_TCTI_HOSTED_EXEC)
	if (mm) {
		atomic64_set(&mm->context.orlix_tcti_mapping_sequence, 0);
		rwlock_init(&mm->context.orlix_tcti_mapping_lock);
		mm->context.orlix_tcti_memory_metadata = NULL;
		if (orlix_mte_init_mm(mm))
			return -ENOMEM;
		orlix_tcti_invalidate_mm(mm);
	}
#endif
	return 0;
}

#define destroy_context destroy_context
static inline void destroy_context(struct mm_struct *mm)
{
#if defined(ORLIX_APP_HOSTED_BOOT) && defined(CONFIG_ORLIX_TCTI_HOSTED_EXEC)
	orlix_mte_destroy_mm(mm);
	orlix_tcti_invalidate_mm(mm);
	orlix_tcti_memory_metadata_destroy(mm);
#else
	(void)mm;
#endif
}

static inline void switch_mm(struct mm_struct *prev, struct mm_struct *next,
			     struct task_struct *tsk)
{
#if defined(ORLIX_APP_HOSTED_BOOT)
	/*
	 * Hosted user mappings live in single Darwin process address space,
	 * not in per-Linux-mm hardware page tables. Drop host-side user
	 * view whenever Linux switches address spaces so stale mappings
	 * from the previous task cannot satisfy faults in the next task.
	 */
	if (prev != next)
		flush_tlb_mm(next);
#endif
	(void)tsk;
}

#include <asm-generic/mmu_context.h>

#endif /* _ASM_ORLIX_MMU_CONTEXT_H */
