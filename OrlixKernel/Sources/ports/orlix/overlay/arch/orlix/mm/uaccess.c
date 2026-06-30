// SPDX-License-Identifier: GPL-2.0-only

#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/sched/task_stack.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <asm/hosted_exec.h>
#include <asm/page.h>
#include <asm/pgtable.h>

static int orlix_uaccess_resolve(struct mm_struct *mm, unsigned long address,
				 bool write, void **kaddr)
{
	pgd_t *pgd;
	p4d_t *p4d;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
	pte_t entry;

	pgd = pgd_offset(mm, address);
	if (pgd_none(*pgd) || pgd_bad(*pgd))
		return -EFAULT;

	p4d = p4d_offset(pgd, address);
	if (p4d_none(*p4d) || p4d_bad(*p4d))
		return -EFAULT;

	pud = pud_offset(p4d, address);
	if (pud_none(*pud) || pud_bad(*pud))
		return -EFAULT;

	pmd = pmd_offset(pud, address);
	if (pmd_none(*pmd) || pmd_bad(*pmd))
		return -EFAULT;

	pte = pte_offset_kernel(pmd, address);
	entry = READ_ONCE(*pte);
	if (!(pte_val(entry) & _PAGE_PRESENT) || !(pte_val(entry) & _PAGE_USER))
		return -EFAULT;
	if (write && !pte_write(entry))
		return -EFAULT;

	*kaddr = (char *)__va(PFN_PHYS(pte_pfn(entry))) + offset_in_page(address);
	return 0;
}

static int orlix_uaccess_fault_in(struct mm_struct *mm, unsigned long address,
				  bool write)
{
	struct pt_regs *regs = task_pt_regs(current);
	bool tried = false;
	vm_flags_t required = write ? VM_WRITE : VM_READ;

	if (faulthandler_disabled())
		return -EFAULT;

retry:
	{
		struct vm_area_struct *vma;
		unsigned int flags = FAULT_FLAG_DEFAULT | FAULT_FLAG_USER;
		vm_fault_t fault;

		if (write)
			flags |= FAULT_FLAG_WRITE;
		if (tried)
			flags |= FAULT_FLAG_TRIED;

		vma = lock_mm_and_find_vma(mm, address, regs);
		if (!vma)
			return -EFAULT;

		if (!(vma->vm_flags & required)) {
			mmap_read_unlock(mm);
			return -EFAULT;
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

static int orlix_uaccess_sync_to_user(unsigned long address, const void *kaddr)
{
#if defined(ORLIX_APP_HOSTED_BOOT)
	const void *source_page = (const void *)((unsigned long)kaddr & PAGE_MASK);

	return orlix_refresh_current_user_mapping_page_from_kernel(address,
								  source_page);
#else
	(void)address;
	(void)kaddr;
	return 0;
#endif
}

static unsigned long orlix_uaccess_copy_from_user(void *to,
						  unsigned long from,
						  unsigned long n)
{
	struct mm_struct *mm = current->mm;
	unsigned long remaining = n;
	unsigned char *dst = to;

	if (!mm)
		return n;

	while (remaining) {
		unsigned long chunk = min(remaining, PAGE_SIZE - offset_in_page(from));
		void *src;

		mmap_read_lock(mm);
		if (orlix_uaccess_resolve(mm, from, false, &src)) {
			mmap_read_unlock(mm);
			if (orlix_uaccess_fault_in(mm, from, false))
				return remaining;
			mmap_read_lock(mm);
			if (orlix_uaccess_resolve(mm, from, false, &src)) {
				mmap_read_unlock(mm);
				return remaining;
			}
		}
		memcpy(dst, src, chunk);
		mmap_read_unlock(mm);

		dst += chunk;
		from += chunk;
		remaining -= chunk;
	}

	return 0;
}

static unsigned long orlix_uaccess_copy_to_user(unsigned long to,
						const void *from,
						unsigned long n)
{
	struct mm_struct *mm = current->mm;
	unsigned long remaining = n;
	const unsigned char *src = from;

	if (!mm)
		return n;

	while (remaining) {
		unsigned long chunk = min(remaining, PAGE_SIZE - offset_in_page(to));
		void *dst;

		mmap_read_lock(mm);
		if (orlix_uaccess_resolve(mm, to, true, &dst)) {
			mmap_read_unlock(mm);
			if (orlix_uaccess_fault_in(mm, to, true))
				return remaining;
			mmap_read_lock(mm);
			if (orlix_uaccess_resolve(mm, to, true, &dst)) {
				mmap_read_unlock(mm);
				return remaining;
			}
		}
		memcpy(dst, src, chunk);
		mmap_read_unlock(mm);
		if (orlix_uaccess_sync_to_user(to, dst))
			return remaining;

		src += chunk;
		to += chunk;
		remaining -= chunk;
	}

	return 0;
}

unsigned long raw_copy_from_user(void *to, const void __user *from,
				 unsigned long n)
{
	return orlix_uaccess_copy_from_user(to, (unsigned long)from, n);
}

unsigned long raw_copy_to_user(void __user *to, const void *from,
			       unsigned long n)
{
	return orlix_uaccess_copy_to_user((unsigned long)to, from, n);
}

unsigned long __clear_user(void __user *to, unsigned long n)
{
	static const unsigned long zero_page[PAGE_SIZE / sizeof(unsigned long)];
	unsigned long remaining = n;
	unsigned long address = (unsigned long)to;

	while (remaining) {
		unsigned long chunk = min(remaining,
					  PAGE_SIZE - offset_in_page(address));
		unsigned long left;

		left = orlix_uaccess_copy_to_user(address, zero_page, chunk);
		if (left)
			return remaining - (chunk - left);

		address += chunk;
		remaining -= chunk;
	}

	return 0;
}
