// SPDX-License-Identifier: GPL-2.0-only
#include <linux/mm.h>

#include "../hosted_exec/tcti/block_cache.h"

void tcti_invalidate_mm(struct mm_struct *mm)
{
	if (!mm)
		return;

	tcti_mapping_sequence_begin(mm);
	tcti_block_cache_invalidate_mm(mm);
	tcti_mapping_sequence_end(mm);
}

void tcti_invalidate_all(void)
{
	tcti_block_cache_invalidate_all();
}

void tcti_invalidate_vma(struct vm_area_struct *vma)
{
	if (vma)
		tcti_invalidate_range(vma->vm_mm, vma->vm_start, vma->vm_end);
}

void tcti_invalidate_range(struct mm_struct *mm, unsigned long start,
			   unsigned long end)
{
	if (!mm || end <= start)
		return;

	tcti_mapping_sequence_begin(mm);
	tcti_block_cache_invalidate_range(mm, start, end);
	tcti_mapping_sequence_end(mm);
}

void tcti_note_pte_update(struct mm_struct *mm)
{
	if (!mm)
		return;

	tcti_mapping_sequence_begin(mm);
	tcti_block_cache_invalidate_mm(mm);
	tcti_mapping_sequence_end(mm);
}

void tcti_note_pte_update_range(struct mm_struct *mm, unsigned long start,
				unsigned long end)
{
	if (!mm)
		return;

	if (end > start)
		tcti_block_cache_invalidate_range(mm, start, end);
	else
		tcti_bump_code_generation(mm);
	tcti_mapping_sequence_end(mm);
}

void tcti_note_pte_update_address(struct mm_struct *mm,
				  unsigned long address)
{
	if (!mm)
		return;

	tcti_mapping_sequence_begin(mm);
	tcti_note_pte_update_range(mm, address, address + PAGE_SIZE);
}
