// SPDX-License-Identifier: GPL-2.0-only
#include <linux/errno.h>
#include <linux/minmax.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/unaligned.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/processor.h>
#include <asm/tcti.h>

#include "../hosted_exec/tcti/block_cache.h"

static int tcti_access_required_vm_flags(enum tcti_access access,
					 vm_flags_t *required)
{
	switch (access) {
	case TCTI_ACCESS_FETCH:
		*required = VM_EXEC;
		return 0;
	case TCTI_ACCESS_READ:
		*required = VM_READ;
		return 0;
	case TCTI_ACCESS_WRITE:
		*required = VM_WRITE;
		return 0;
	default:
		return -EINVAL;
	}
}

static int tcti_resolve_user_data_locked(struct mm_struct *mm,
					 unsigned long user_va,
					 enum tcti_access access,
					 void **host_data,
					 unsigned long *linux_perms,
					 struct page **page)
{
	struct vm_area_struct *vma;
	vm_flags_t required;
	pgd_t *pgd;
	p4d_t *p4d;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
	pte_t entry;
	int ret;

	ret = tcti_access_required_vm_flags(access, &required);
	if (ret)
		return ret;

	vma = find_vma(mm, user_va);
	if (!vma || user_va < vma->vm_start || user_va >= vma->vm_end)
		return -EFAULT;

	if (!(vma->vm_flags & required))
		return -EACCES;

	pgd = pgd_offset(mm, user_va);
	if (pgd_none(*pgd) || pgd_bad(*pgd))
		return -EFAULT;
	p4d = p4d_offset(pgd, user_va);
	if (p4d_none(*p4d) || p4d_bad(*p4d))
		return -EFAULT;
	pud = pud_offset(p4d, user_va);
	if (pud_none(*pud) || pud_bad(*pud))
		return -EFAULT;
	pmd = pmd_offset(pud, user_va);
	if (pmd_none(*pmd) || pmd_bad(*pmd))
		return -EFAULT;

	pte = pte_offset_kernel(pmd, user_va);
	entry = READ_ONCE(*pte);
	if (!(pte_val(entry) & _PAGE_PRESENT) || !(pte_val(entry) & _PAGE_USER))
		return -EFAULT;
	if (access == TCTI_ACCESS_FETCH && !(pte_val(entry) & _PAGE_EXEC))
		return -EACCES;
	if (access == TCTI_ACCESS_WRITE && !pte_write(entry))
		return -EACCES;

	if (host_data)
		*host_data = (char *)__va(PFN_PHYS(pte_pfn(entry))) +
			     offset_in_page(user_va);
	if (linux_perms)
		*linux_perms = vma->vm_flags;
	if (page)
		*page = pte_page(entry);

	return 0;
}

int tcti_pin_user_page(struct mm_struct *mm, unsigned long user_va,
		       enum tcti_access access, struct tcti_user_page *out)
{
	unsigned long linux_perms = 0;
	void *host_data = NULL;
	struct page *page = NULL;
	int ret;

	if (!mm || !out)
		return -EINVAL;
	if (user_va >= TASK_SIZE)
		return -EFAULT;

	memset(out, 0, sizeof(*out));
	out->user_page = user_va & PAGE_MASK;

	mmap_read_lock(mm);
	ret = tcti_resolve_user_data_locked(mm, user_va, access, &host_data,
					    &linux_perms, &page);
	if (!ret) {
		out->host_data = (void *)((unsigned long)host_data & PAGE_MASK);
		out->page = page;
		out->linux_perms = linux_perms;
		out->translation_generation = tcti_translation_generation(mm);
		out->code_generation = tcti_code_generation(mm);
	}
	mmap_read_unlock(mm);

	return ret;
}

void tcti_unpin_user_page(struct tcti_user_page *page)
{
	(void)page;
}

int tcti_fetch_instruction(struct mm_struct *mm, unsigned long pc,
			   u32 *instruction)
{
	void *host_data = NULL;
	int ret;

	if (!mm || !instruction)
		return -EINVAL;
	if (pc & (sizeof(u32) - 1))
		return -EFAULT;
	if (pc >= TASK_SIZE || pc > TASK_SIZE - sizeof(u32))
		return -EFAULT;

	mmap_read_lock(mm);
	ret = tcti_resolve_user_data_locked(mm, pc, TCTI_ACCESS_FETCH,
					    &host_data, NULL, NULL);
	if (!ret)
		*instruction = get_unaligned_le32(host_data);
	mmap_read_unlock(mm);

	return ret;
}

static int tcti_copy_user_data(struct mm_struct *mm, unsigned long user_va,
			       void *buffer, size_t size,
			       enum tcti_access access)
{
	size_t copied = 0;

	if (!mm || !buffer)
		return -EINVAL;
	if (access != TCTI_ACCESS_READ && access != TCTI_ACCESS_WRITE)
		return -EINVAL;
	if (!size)
		return 0;
	if (user_va >= TASK_SIZE || size > TASK_SIZE - user_va)
		return -EFAULT;

	while (copied < size) {
		unsigned long current_va = user_va + copied;
		size_t chunk = min(size - copied,
				   (size_t)(PAGE_SIZE -
					    offset_in_page(current_va)));
		void *host_data = NULL;
		int ret;

		mmap_read_lock(mm);
		ret = tcti_resolve_user_data_locked(mm, current_va, access,
						    &host_data, NULL, NULL);
		if (!ret) {
			if (access == TCTI_ACCESS_READ)
				memcpy((char *)buffer + copied, host_data, chunk);
			else
				memcpy(host_data, (char *)buffer + copied, chunk);
		}
		mmap_read_unlock(mm);
		if (ret)
			return ret;

		copied += chunk;
	}

	return 0;
}

int tcti_read_user_data(struct mm_struct *mm, unsigned long user_va,
			void *buffer, size_t size)
{
	return tcti_copy_user_data(mm, user_va, buffer, size, TCTI_ACCESS_READ);
}

int tcti_write_user_data(struct mm_struct *mm, unsigned long user_va,
			 const void *buffer, size_t size)
{
	return tcti_copy_user_data(mm, user_va, (void *)buffer, size,
				   TCTI_ACCESS_WRITE);
}
