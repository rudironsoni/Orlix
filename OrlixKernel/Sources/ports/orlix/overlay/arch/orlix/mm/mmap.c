// SPDX-License-Identifier: GPL-2.0-only

#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/security.h>
#include <asm/processor.h>
#include <asm/boot.h>

static bool orlix_hosted_prot_none_reservation(struct file *file,
					       unsigned long flags,
					       vm_flags_t vm_flags)
{
	return !file && !(flags & MAP_FIXED) &&
	       !(vm_flags & VM_ACCESS_FLAGS);
}

static bool orlix_hosted_anonymous_private_mapping(struct file *file,
						   unsigned long flags)
{
	return !file && !(flags & MAP_FIXED) && (flags & MAP_ANONYMOUS) &&
	       !(flags & MAP_SHARED);
}

static unsigned long orlix_hosted_mmap_length_align_mask(struct file *file,
							 unsigned long flags,
							 unsigned long len)
{
	if (!orlix_hosted_anonymous_private_mapping(file, flags) ||
	    len < PAGE_SIZE || (len & (len - 1)))
		return 0;
	return len - 1;
}

static unsigned long orlix_hosted_mmap_align_mask(struct file *file,
						  unsigned long flags,
						  vm_flags_t vm_flags,
						  unsigned long len)
{
	unsigned long length_mask;
	unsigned long granule = arch_boot_host_page_size();

	length_mask = orlix_hosted_mmap_length_align_mask(file, flags, len);
	if (length_mask)
		return length_mask;

	if (granule <= PAGE_SIZE ||
	    !orlix_hosted_prot_none_reservation(file, flags, vm_flags))
		return 0;
	return granule - 1;
}

static unsigned long orlix_hosted_mmap_align_offset(struct file *file,
						    unsigned long flags,
						    vm_flags_t vm_flags,
						    unsigned long len)
{
	unsigned long length_mask;
	unsigned long mask;

	length_mask = orlix_hosted_mmap_length_align_mask(file, flags, len);
	if (length_mask)
		return 0;

	mask = orlix_hosted_mmap_align_mask(file, flags, vm_flags, len);
	if (!mask)
		return 0;
	return (mask + 1 - PAGE_SIZE) & mask;
}

static bool orlix_hosted_mmap_address_aligned(struct file *file,
					      unsigned long flags,
					      vm_flags_t vm_flags,
					      unsigned long len,
					      unsigned long addr)
{
	unsigned long mask = orlix_hosted_mmap_align_mask(file, flags,
							  vm_flags, len);
	unsigned long offset;

	if (!mask)
		return true;

	offset = orlix_hosted_mmap_align_offset(file, flags, vm_flags, len);
	return !((addr + offset) & mask);
}

static unsigned long orlix_mmap_low_limit(struct mm_struct *mm)
{
#if defined(ORLIX_APP_HOSTED_BOOT)
	return max(mm->mmap_base, TASK_UNMAPPED_BASE);
#else
	return mm->mmap_base;
#endif
}

static unsigned long orlix_mmap_floor(void)
{
#if defined(ORLIX_APP_HOSTED_BOOT)
	return TASK_UNMAPPED_BASE;
#else
	return mmap_min_addr;
#endif
}

unsigned long arch_get_unmapped_area(struct file *file, unsigned long addr,
				     unsigned long len, unsigned long pgoff,
				     unsigned long flags, vm_flags_t vm_flags)
{
	struct mm_struct *mm = current->mm;
	struct vm_area_struct *vma, *prev;
	struct vm_unmapped_area_info info = {};
	const unsigned long mmap_end = arch_get_mmap_end(addr, len, flags);
	const unsigned long mmap_floor = orlix_mmap_floor();

#if !defined(ORLIX_APP_HOSTED_BOOT)
	if (!orlix_hosted_mmap_align_mask(file, flags, vm_flags)) {
		return generic_get_unmapped_area(file, addr, len, pgoff,
						 flags, vm_flags);
	}
#endif

	if (len > mmap_end - mmap_floor)
		return -ENOMEM;

	if (flags & MAP_FIXED)
		return addr;

	if (addr) {
		addr = PAGE_ALIGN(addr);
		vma = find_vma_prev(mm, addr, &prev);
		if (mmap_end - len >= addr && addr >= mmap_floor &&
		    orlix_hosted_mmap_address_aligned(file, flags, vm_flags,
						      len, addr) &&
		    (!vma || addr + len <= vm_start_gap(vma)) &&
		    (!prev || addr >= vm_end_gap(prev)))
			return addr;
	}

	info.length = len;
	info.low_limit = orlix_mmap_low_limit(mm);
	info.high_limit = mmap_end;
	info.align_mask = orlix_hosted_mmap_align_mask(file, flags,
						       vm_flags, len);
	info.align_offset = orlix_hosted_mmap_align_offset(file, flags,
							   vm_flags, len);
	return vm_unmapped_area(&info);
}

unsigned long arch_get_unmapped_area_topdown(struct file *file,
					     unsigned long addr,
					     unsigned long len,
					     unsigned long pgoff,
					     unsigned long flags,
					     vm_flags_t vm_flags)
{
	struct mm_struct *mm = current->mm;
	struct vm_area_struct *vma, *prev;
	struct vm_unmapped_area_info info = {};
	const unsigned long mmap_end = arch_get_mmap_end(addr, len, flags);
	unsigned long result;
	const unsigned long mmap_floor = orlix_mmap_floor();

#if !defined(ORLIX_APP_HOSTED_BOOT)
	if (!orlix_hosted_mmap_align_mask(file, flags, vm_flags)) {
		return generic_get_unmapped_area_topdown(file, addr, len, pgoff,
							 flags, vm_flags);
	}
#endif

	if (len > mmap_end - mmap_floor)
		return -ENOMEM;

	if (flags & MAP_FIXED)
		return addr;

	if (addr) {
		addr = PAGE_ALIGN(addr);
		vma = find_vma_prev(mm, addr, &prev);
		if (mmap_end - len >= addr && addr >= mmap_floor &&
		    orlix_hosted_mmap_address_aligned(file, flags, vm_flags,
						      len, addr) &&
		    (!vma || addr + len <= vm_start_gap(vma)) &&
		    (!prev || addr >= vm_end_gap(prev)))
			return addr;
	}

	info.flags = VM_UNMAPPED_AREA_TOPDOWN;
	info.length = len;
	info.low_limit = TASK_UNMAPPED_BASE;
	info.high_limit = arch_get_mmap_base(addr, mm->mmap_base);
	info.align_mask = orlix_hosted_mmap_align_mask(file, flags,
						       vm_flags, len);
	info.align_offset = orlix_hosted_mmap_align_offset(file, flags,
							   vm_flags, len);
	result = vm_unmapped_area(&info);
	if (offset_in_page(result)) {
		VM_BUG_ON(result != -ENOMEM);
		info.flags = 0;
		info.low_limit = TASK_UNMAPPED_BASE;
		info.high_limit = mmap_end;
		result = vm_unmapped_area(&info);
	}

	return result;
}
