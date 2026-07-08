// SPDX-License-Identifier: GPL-2.0-only
#include <linux/errno.h>
#include <linux/minmax.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/sched/task_stack.h>
#include <linux/string.h>
#include <linux/unaligned.h>
#include <asm/hosted_exec.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/processor.h>
#include <asm/tcti.h>
#include <internal/asm/host_trap.h>

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

static int tcti_fault_in_user_page(struct mm_struct *mm, unsigned long address,
				   enum tcti_access access)
{
	struct pt_regs *regs = task_pt_regs(current);
	vm_flags_t required;
	bool tried = false;
	int ret;

	ret = tcti_access_required_vm_flags(access, &required);
	if (ret)
		return ret;

	if (faulthandler_disabled())
		return -EFAULT;

retry:
	{
		struct vm_area_struct *vma;
		unsigned int flags = FAULT_FLAG_DEFAULT | FAULT_FLAG_USER;
		vm_fault_t fault;

		if (access == TCTI_ACCESS_WRITE)
			flags |= FAULT_FLAG_WRITE;
		if (access == TCTI_ACCESS_FETCH)
			flags |= FAULT_FLAG_INSTRUCTION;
		if (tried)
			flags |= FAULT_FLAG_TRIED;

		vma = lock_mm_and_find_vma(mm, address, regs);
		if (!vma)
			return -EFAULT;

		if (!(vma->vm_flags & required)) {
			mmap_read_unlock(mm);
			return -EACCES;
		}

		fault = handle_mm_fault(vma, address, flags, regs);
		if (fault_signal_pending(fault, regs))
			return -EINTR;
		if (fault & VM_FAULT_COMPLETED)
			return 0;
		if (fault & VM_FAULT_RETRY) {
			tried = true;
			goto retry;
		}
		if (unlikely(fault & VM_FAULT_ERROR)) {
			int err = vm_fault_to_errno(fault, 0);

			mmap_read_unlock(mm);
			return err ? err : -EFAULT;
		}

		mmap_read_unlock(mm);
		return 0;
	}
}

static bool tcti_vma_is_executable(struct mm_struct *mm, unsigned long address)
{
	struct vm_area_struct *vma;
	bool executable = false;

	if (!mm)
		return false;

	mmap_read_lock(mm);
	vma = find_vma(mm, address);
	if (vma && address >= vma->vm_start && address < vma->vm_end &&
	    (vma->vm_flags & VM_EXEC))
		executable = true;
	mmap_read_unlock(mm);

	return executable;
}

static int tcti_sync_faulted_user_window(struct mm_struct *mm,
					 unsigned long address,
					 enum tcti_access access)
{
#if defined(ORLIX_APP_HOSTED_BOOT)
	unsigned long fault_flags = 0;

	if (access == TCTI_ACCESS_READ && tcti_vma_is_executable(mm, address))
		return 0;

	switch (access) {
	case TCTI_ACCESS_FETCH:
		fault_flags = ORLIX_HOST_USER_FAULT_EXEC;
		break;
	case TCTI_ACCESS_WRITE:
		fault_flags = ORLIX_HOST_USER_FAULT_WRITE;
		break;
	case TCTI_ACCESS_READ:
		break;
	}

	return orlix_sync_current_user_fault_window(address, fault_flags);
#else
	(void)mm;
	(void)address;
	(void)access;
	return 0;
#endif
}

static void tcti_log_fetch_resolution_failure(struct mm_struct *mm,
					      unsigned long pc, int ret)
{
	struct vm_area_struct *vma;
	unsigned long vm_start = 0;
	unsigned long vm_end = 0;
	unsigned long vm_flags = 0;
	unsigned long pgd_bits = 0;
	unsigned long p4d_bits = 0;
	unsigned long pud_bits = 0;
	unsigned long pmd_bits = 0;
	unsigned long pte_bits = 0;
	pgd_t *pgd;
	p4d_t *p4d;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;

	if (!mm)
		return;

	mmap_read_lock(mm);
	vma = find_vma(mm, pc);
	if (vma && pc >= vma->vm_start && pc < vma->vm_end) {
		vm_start = vma->vm_start;
		vm_end = vma->vm_end;
		vm_flags = vma->vm_flags;
	}
	pgd = pgd_offset(mm, pc);
	pgd_bits = pgd_val(*pgd);
	if (!pgd_none(*pgd) && !pgd_bad(*pgd)) {
		p4d = p4d_offset(pgd, pc);
		p4d_bits = p4d_val(*p4d);
		if (!p4d_none(*p4d) && !p4d_bad(*p4d)) {
			pud = pud_offset(p4d, pc);
			pud_bits = pud_val(*pud);
			if (!pud_none(*pud) && !pud_bad(*pud)) {
				pmd = pmd_offset(pud, pc);
				pmd_bits = pmd_val(*pmd);
				if (!pmd_none(*pmd) && !pmd_bad(*pmd)) {
					pte = pte_offset_kernel(pmd, pc);
					pte_bits = pte_val(READ_ONCE(*pte));
				}
			}
		}
	}
	mmap_read_unlock(mm);

	pr_info("Orlix TCTI: fetch fault detail task=%s pid=%d pc=%#lx ret=%d task_size=%#lx vma=%#lx-%#lx flags=%#lx pgd=%#lx p4d=%#lx pud=%#lx pmd=%#lx pte=%#lx\n",
		current->comm, task_pid_nr(current), pc, ret, TASK_SIZE,
		vm_start, vm_end, vm_flags, pgd_bits, p4d_bits, pud_bits,
		pmd_bits, pte_bits);
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
	if (access == TCTI_ACCESS_WRITE &&
	    (!pte_dirty(entry) || !pte_young(entry))) {
		entry = pte_mkdirty(pte_mkyoung(entry));
		set_pte(pte, entry);
	}
	if (access == TCTI_ACCESS_WRITE)
		set_page_dirty(pte_page(entry));

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
	if (ret == -EFAULT || ret == -EACCES) {
		ret = tcti_fault_in_user_page(mm, pc, TCTI_ACCESS_FETCH);
		if (ret) {
			tcti_log_fetch_resolution_failure(mm, pc, ret);
			return ret;
		}

		mmap_read_lock(mm);
		ret = tcti_resolve_user_data_locked(mm, pc, TCTI_ACCESS_FETCH,
						    &host_data, NULL, NULL);
		if (!ret)
			*instruction = get_unaligned_le32(host_data);
		mmap_read_unlock(mm);
		if (ret == -EFAULT) {
			/*
			 * Final authorization still comes from VM_EXEC and the
			 * executable PTE check above. This only gives hosted
			 * file-backed pages a data-fault population pass before
			 * TCTI retries the executable fetch resolution.
			 */
			if (tcti_fault_in_user_page(mm, pc, TCTI_ACCESS_READ))
				return ret;

			mmap_read_lock(mm);
			ret = tcti_resolve_user_data_locked(mm, pc,
							    TCTI_ACCESS_FETCH,
							    &host_data, NULL, NULL);
			if (!ret)
				*instruction = get_unaligned_le32(host_data);
			mmap_read_unlock(mm);
		}
	}

	if (ret)
		tcti_log_fetch_resolution_failure(mm, pc, ret);

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
		void *host_page = NULL;
		void *host_data = NULL;
		int ret;

		mmap_read_lock(mm);
		ret = tcti_resolve_user_data_locked(mm, current_va, access,
						    &host_data, NULL, NULL);
		if (!ret) {
			if (access == TCTI_ACCESS_READ)
				memcpy((char *)buffer + copied, host_data, chunk);
			else {
				memcpy(host_data, (char *)buffer + copied, chunk);
				host_page = (void *)((unsigned long)host_data &
						     PAGE_MASK);
			}
		}
		mmap_read_unlock(mm);
		if (ret) {
			ret = tcti_fault_in_user_page(mm, current_va, access);
			if (!ret) {
				ret = tcti_sync_faulted_user_window(mm, current_va,
								   access);
				if (ret)
					return ret;
				continue;
			}
		}
		if (ret)
			return ret;
#if defined(ORLIX_APP_HOSTED_BOOT)
		if (access == TCTI_ACCESS_WRITE) {
			ret = orlix_refresh_current_user_mapping_page_from_kernel(
				current_va, host_page);
			if (ret)
				return ret;
		}
#endif

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
