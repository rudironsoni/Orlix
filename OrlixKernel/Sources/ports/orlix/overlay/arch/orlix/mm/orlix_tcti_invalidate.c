// SPDX-License-Identifier: GPL-2.0-only
#include <linux/mm.h>

#include "../hosted_exec/orlix_tcti/block_cache.h"

void orlix_tcti_invalidate_mm(struct mm_struct *mm)
{
	if (!mm)
		return;

	orlix_tcti_mapping_sequence_begin(mm);
	orlix_tcti_block_cache_invalidate_mm(mm);
	orlix_tcti_mapping_sequence_end(mm);
}

void orlix_tcti_invalidate_all(void)
{
	orlix_tcti_block_cache_invalidate_all();
}

void orlix_tcti_invalidate_vma(struct vm_area_struct *vma)
{
	if (vma)
		orlix_tcti_invalidate_range(vma->vm_mm, vma->vm_start, vma->vm_end);
}

void orlix_tcti_invalidate_range(struct mm_struct *mm, unsigned long start,
			   unsigned long end)
{
	if (!mm || end <= start)
		return;

	orlix_tcti_mapping_sequence_begin(mm);
	orlix_tcti_block_cache_invalidate_range(mm, start, end);
	orlix_tcti_mapping_sequence_end(mm);
}

void orlix_tcti_note_pte_update(struct mm_struct *mm)
{
	if (!mm)
		return;

	orlix_tcti_mapping_sequence_begin(mm);
	orlix_tcti_block_cache_invalidate_mm(mm);
	orlix_tcti_mapping_sequence_end(mm);
}

void orlix_tcti_note_pte_update_range(struct mm_struct *mm, unsigned long start,
				unsigned long end)
{
	if (!mm)
		return;

	if (end > start)
		orlix_tcti_block_cache_invalidate_range(mm, start, end);
	else
		orlix_tcti_bump_code_generation(mm);
	orlix_tcti_mapping_sequence_end(mm);
}

void orlix_tcti_note_pte_update_address(struct mm_struct *mm,
				  unsigned long address)
{
	if (!mm)
		return;

	orlix_tcti_mapping_sequence_begin(mm);
	orlix_tcti_note_pte_update_range(mm, address, address + PAGE_SIZE);
}
