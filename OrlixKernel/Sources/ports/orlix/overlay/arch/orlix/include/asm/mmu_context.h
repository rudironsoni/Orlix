/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _ASM_ORLIX_MMU_CONTEXT_H
#define _ASM_ORLIX_MMU_CONTEXT_H

#include <asm-generic/mm_hooks.h>
#include <asm/tlbflush.h>

struct mm_struct;
struct task_struct;

static inline void switch_mm(struct mm_struct *prev, struct mm_struct *next,
				     struct task_struct *tsk)
{
#if defined(ORLIX_APP_HOSTED_BOOT)
	/*
	 * Hosted user mappings live in the single Darwin process address space,
	 * not in per-Linux-mm hardware page tables.  Drop the host-side user
	 * view whenever Linux switches address spaces so stale mappings from a
	 * previous task cannot satisfy faults for the next task.
	 */
	if (prev != next)
		flush_tlb_mm(next);
#endif
	(void)tsk;
}

#include <asm-generic/mmu_context.h>

#endif /* _ASM_ORLIX_MMU_CONTEXT_H */
