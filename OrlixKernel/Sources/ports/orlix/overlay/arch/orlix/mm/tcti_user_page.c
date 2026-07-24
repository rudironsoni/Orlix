// SPDX-License-Identifier: GPL-2.0-only
#include <linux/errno.h>
#include <linux/atomic.h>
#include <linux/hash.h>
#include <linux/minmax.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
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

/* Collisions may cause a permitted spurious STXR failure, never success. */
#define TCTI_RESERVATION_GENERATION_BITS 10U
static atomic64_t tcti_reservation_generations[
	1U << TCTI_RESERVATION_GENERATION_BITS];

static atomic64_t *tcti_reservation_generation_slot(unsigned long pfn)
{
	return &tcti_reservation_generations[
		hash_long(pfn, TCTI_RESERVATION_GENERATION_BITS)];
}

static u64 tcti_reservation_generation_read(struct page *page)
{
	return atomic64_read(tcti_reservation_generation_slot(page_to_pfn(page)));
}

static void tcti_reservation_generation_bump(struct page *page)
{
	atomic64_inc(tcti_reservation_generation_slot(page_to_pfn(page)));
}

#if defined(ORLIX_APP_HOSTED_BOOT)
static atomic_t tcti_committed_refresh_failures = ATOMIC_INIT(0);

static void tcti_note_committed_refresh_failure(struct mm_struct *mm,
						 unsigned long address,
						 size_t size, int ret)
{
	int count = atomic_inc_return(&tcti_committed_refresh_failures);

	orlix_host_user_discard_pages_serialized(address & PAGE_MASK, PAGE_SIZE);
	pr_warn_ratelimited("Orlix TCTI: committed guest memory write host refresh failed mm=%px address=%#lx size=%zu ret=%d count=%d\n",
			    mm, address, size, ret, count);
}
#endif

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

static int tcti_resolve_refcounted_pte_page(pte_t entry,
					    struct page **page)
{
	unsigned long pfn = pte_pfn(entry);
	struct page *resolved;

	if (pfn < ARCH_PFN_OFFSET ||
	    pfn - ARCH_PFN_OFFSET >= max_mapnr ||
	    !pfn_valid(pfn))
		return -EFAULT;

	resolved = pfn_to_page(pfn);
	if (page_count(resolved) <= 0)
		return -EFAULT;

	*page = resolved;
	return 0;
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
					 struct page **page,
					 u64 *translation_generation)
{
	struct vm_area_struct *vma;
	vm_flags_t required;
	pgd_t *pgd;
	p4d_t *p4d;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
	pte_t entry;
	spinlock_t *ptl;
	struct page *resolved_page;
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

	pte = pte_offset_map_lock(mm, pmd, user_va, &ptl);
	entry = READ_ONCE(*pte);
	if (!(pte_val(entry) & _PAGE_PRESENT) || !(pte_val(entry) & _PAGE_USER))
		goto out_fault;
	if (access == TCTI_ACCESS_FETCH && !(pte_val(entry) & _PAGE_EXEC))
		goto out_access;
	if (access == TCTI_ACCESS_WRITE && !pte_write(entry))
		goto out_access;

	ret = tcti_resolve_refcounted_pte_page(entry, &resolved_page);
	if (ret)
		goto out;
	if (page && !get_page_unless_zero(resolved_page)) {
		ret = -EFAULT;
		goto out;
	}

	if (access == TCTI_ACCESS_WRITE &&
	    (!pte_dirty(entry) || !pte_young(entry))) {
		entry = pte_mkdirty(pte_mkyoung(entry));
		set_pte(pte, entry);
	}
	if (access == TCTI_ACCESS_WRITE)
		set_page_dirty(resolved_page);

	if (host_data)
		*host_data = (char *)__va(PFN_PHYS(pte_pfn(entry))) +
			     offset_in_page(user_va);
	if (linux_perms)
		*linux_perms = vma->vm_flags;
	if (page)
		*page = resolved_page;
	if (translation_generation)
		*translation_generation = tcti_translation_generation(mm);
	ret = 0;
out:
	pte_unmap_unlock(pte, ptl);
	return ret;
out_access:
	ret = -EACCES;
	goto out;
out_fault:
	ret = -EFAULT;
	goto out;
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
					    &linux_perms, &page,
					    &out->translation_generation);
	if (!ret) {
		out->host_data = (void *)((unsigned long)host_data & PAGE_MASK);
		out->page = page;
		out->linux_perms = linux_perms;
		out->code_generation = tcti_code_generation(mm);
	}
	mmap_read_unlock(mm);

	return ret;
}

void tcti_unpin_user_page(struct tcti_user_page *page)
{
	if (!page || !page->page)
		return;

	put_page(page->page);
	page->page = NULL;
}

static int tcti_fetch_instruction_pinned(struct mm_struct *mm,
					 unsigned long pc, u32 *instruction)
{
	struct tcti_user_page page;
	void *host_data;
	int ret = -EAGAIN;

	while (ret == -EAGAIN) {
		ret = tcti_pin_user_page(mm, pc, TCTI_ACCESS_FETCH, &page);
		if (ret)
			return ret;

		host_data = (char *)page.host_data + offset_in_page(pc);
		lock_page(page.page);
		if (!tcti_mapping_access_lock(mm,
					      page.translation_generation)) {
			unlock_page(page.page);
			tcti_unpin_user_page(&page);
			ret = -EAGAIN;
			continue;
		}
		*instruction = get_unaligned_le32(host_data);
		tcti_mapping_access_unlock(mm);
		unlock_page(page.page);
		tcti_unpin_user_page(&page);
		ret = 0;
	}

	return ret;
}

int tcti_fetch_instruction(struct mm_struct *mm, unsigned long pc,
			   u32 *instruction)
{
	int ret;

	if (!mm || !instruction)
		return -EINVAL;
	if (pc & (sizeof(u32) - 1))
		return -EFAULT;
	if (pc >= TASK_SIZE || pc > TASK_SIZE - sizeof(u32))
		return -EFAULT;

	ret = tcti_fetch_instruction_pinned(mm, pc, instruction);
	if (ret == -EFAULT || ret == -EACCES) {
		ret = tcti_fault_in_user_page(mm, pc, TCTI_ACCESS_FETCH);
		if (ret) {
			tcti_log_fetch_resolution_failure(mm, pc, ret);
			return ret;
		}

		ret = tcti_fetch_instruction_pinned(mm, pc, instruction);
		if (ret == -EFAULT) {
			/*
			 * Final authorization still comes from VM_EXEC and the
			 * executable PTE check above. This only gives hosted
			 * file-backed pages a data-fault population pass before
			 * TCTI retries the executable fetch resolution.
			 */
			if (tcti_fault_in_user_page(mm, pc, TCTI_ACCESS_READ))
				return ret;

			ret = tcti_fetch_instruction_pinned(mm, pc,
							    instruction);
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
		unsigned long linux_perms = 0;
		struct tcti_user_page page;
		void *host_page = NULL;
		void *host_data = NULL;
		int ret;

		ret = tcti_pin_user_page(mm, current_va, access, &page);
		if (!ret) {
			host_data = (char *)page.host_data + offset_in_page(current_va);
			linux_perms = page.linux_perms;
		}
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

		lock_page(page.page);
		if (!tcti_mapping_access_lock(mm,
					      page.translation_generation)) {
			unlock_page(page.page);
			tcti_unpin_user_page(&page);
			continue;
		}
		if (access == TCTI_ACCESS_READ) {
			memcpy((char *)buffer + copied, host_data, chunk);
		} else {
			memcpy(host_data, (char *)buffer + copied, chunk);
			host_page = (void *)((unsigned long)host_data & PAGE_MASK);
			tcti_reservation_generation_bump(page.page);
		}
		tcti_mapping_access_unlock(mm);

		if (access == TCTI_ACCESS_WRITE && (linux_perms & VM_EXEC))
			tcti_block_cache_invalidate_range(mm, current_va,
							  current_va + chunk);
#if defined(ORLIX_APP_HOSTED_BOOT)
		if (access == TCTI_ACCESS_WRITE) {
			ret = orlix_refresh_user_mapping_range_from_kernel(mm,
				current_va, host_page, chunk);
			if (ret)
				tcti_note_committed_refresh_failure(mm, current_va,
								    chunk, ret);
			ret = 0;
		}
#endif
		unlock_page(page.page);
		tcti_unpin_user_page(&page);

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

static int tcti_pin_user_page_faulting(struct mm_struct *mm,
					unsigned long user_va,
					enum tcti_access access,
					struct tcti_user_page *page)
{
	int ret;

	for (;;) {
		ret = tcti_pin_user_page(mm, user_va, access, page);
		if (!ret)
			return 0;
		if (ret != -EFAULT && ret != -EACCES)
			return ret;
		ret = tcti_fault_in_user_page(mm, user_va, access);
		if (ret)
			return ret;
		ret = tcti_sync_faulted_user_window(mm, user_va, access);
		if (ret)
			return ret;
	}
}

int tcti_load_exclusive_user_data(struct mm_struct *mm,
				  unsigned long user_va, void *buffer, size_t size,
				  unsigned long *pfn, u64 *generation,
				  u64 *mapping_generation)
{
	struct tcti_user_page page;
	void *host_data;
	int ret;

	if (!mm || !buffer || !pfn || !generation || !mapping_generation || !size ||
	    size > PAGE_SIZE - offset_in_page(user_va) ||
	    user_va >= TASK_SIZE || size > TASK_SIZE - user_va)
		return -EINVAL;

retry:
	ret = tcti_pin_user_page_faulting(mm, user_va, TCTI_ACCESS_READ, &page);
	if (ret)
		return ret;
	host_data = (char *)page.host_data + offset_in_page(user_va);
	lock_page(page.page);
	if (!tcti_mapping_access_lock(mm, page.translation_generation)) {
		unlock_page(page.page);
		tcti_unpin_user_page(&page);
		goto retry;
	}
	memcpy(buffer, host_data, size);
	*pfn = page_to_pfn(page.page);
	*generation = tcti_reservation_generation_read(page.page);
	*mapping_generation = page.translation_generation;
	tcti_mapping_access_unlock(mm);
	unlock_page(page.page);
	tcti_unpin_user_page(&page);
	return 0;
}

int tcti_store_exclusive_user_data(struct mm_struct *mm,
				   unsigned long user_va, const void *buffer,
				   size_t size, unsigned long reserved_pfn,
				   u64 reserved_generation,
				   u64 reserved_mapping_generation, bool *stored)
{
	struct tcti_user_page page;
	void *host_page;
	void *host_data;
	int ret;

	if (!mm || !buffer || !stored || !size ||
	    size > PAGE_SIZE - offset_in_page(user_va) ||
	    user_va >= TASK_SIZE || size > TASK_SIZE - user_va)
		return -EINVAL;
	*stored = false;
	ret = tcti_pin_user_page_faulting(mm, user_va, TCTI_ACCESS_WRITE, &page);
	if (ret)
		return ret;
	host_data = (char *)page.host_data + offset_in_page(user_va);
	host_page = (void *)((unsigned long)host_data & PAGE_MASK);
	lock_page(page.page);
	if (page.translation_generation == reserved_mapping_generation &&
	    tcti_mapping_access_lock(mm, page.translation_generation)) {
		if (page_to_pfn(page.page) == reserved_pfn &&
		    tcti_reservation_generation_read(page.page) ==
			    reserved_generation) {
			memcpy(host_data, buffer, size);
			tcti_reservation_generation_bump(page.page);
			*stored = true;
		}
		tcti_mapping_access_unlock(mm);
	}
	if (*stored && (page.linux_perms & VM_EXEC))
		tcti_block_cache_invalidate_range(mm, user_va, user_va + size);
#if defined(ORLIX_APP_HOSTED_BOOT)
	if (*stored) {
		ret = orlix_refresh_user_mapping_range_from_kernel(mm, user_va,
							   host_page, size);
		if (ret) {
			tcti_note_committed_refresh_failure(mm, user_va, size, ret);
			ret = 0;
		}
	}
#endif
	unlock_page(page.page);
	tcti_unpin_user_page(&page);
	return ret;
}


static int tcti_atomic_memory_order_before(enum tcti_atomic_memory_order order)
{
	switch (order) {
	case TCTI_ATOMIC_MEMORY_RELAXED:
	case TCTI_ATOMIC_MEMORY_ACQUIRE:
		return 0;
	case TCTI_ATOMIC_MEMORY_RELEASE:
	case TCTI_ATOMIC_MEMORY_ACQ_REL:
		smp_wmb();
		return 0;
	default:
		return -EINVAL;
	}
}


static void tcti_atomic_memory_order_after(enum tcti_atomic_memory_order order)
{
	if (order == TCTI_ATOMIC_MEMORY_ACQUIRE ||
	    order == TCTI_ATOMIC_MEMORY_ACQ_REL)
		smp_rmb();
}


#define TCTI_ATOMIC_APPLY(type, signed_type)                                  \
	case sizeof(type): {                                                     \
		type old;                                                           \
		type value;                                                         \
		type result;                                                         \
		memcpy(&old, host_data, sizeof(old));                               \
		memcpy(&value, operand, sizeof(value));                             \
		memcpy(old_value, &old, sizeof(old));                               \
		switch (operation) {                                                 \
		case TCTI_ATOMIC_MEMORY_CAS:                                        \
			*exchanged = !memcmp(&old, expected, sizeof(old));             \
			if (*exchanged)                                                  \
				memcpy(host_data, &value, sizeof(value));                    \
			break;                                                          \
		case TCTI_ATOMIC_MEMORY_SWP: result = value; break;                \
		case TCTI_ATOMIC_MEMORY_ADD: result = old + value; break;          \
		case TCTI_ATOMIC_MEMORY_CLR: result = old & ~value; break;         \
		case TCTI_ATOMIC_MEMORY_EOR: result = old ^ value; break;          \
		case TCTI_ATOMIC_MEMORY_SET: result = old | value; break;          \
		case TCTI_ATOMIC_MEMORY_SMAX:                                      \
			result = (type)((signed_type)old > (signed_type)value ? old : value); \
			break;                                                          \
		case TCTI_ATOMIC_MEMORY_SMIN:                                      \
			result = (type)((signed_type)old < (signed_type)value ? old : value); \
			break;                                                          \
		case TCTI_ATOMIC_MEMORY_UMAX: result = old > value ? old : value; break; \
		case TCTI_ATOMIC_MEMORY_UMIN: result = old < value ? old : value; break; \
		default: return -EINVAL;                                            \
		}                                                                   \
		if (operation != TCTI_ATOMIC_MEMORY_CAS) {                          \
			memcpy(host_data, &result, sizeof(result));                       \
			*exchanged = true;                                                \
		}                                                                   \
		return 0;                                                          \
	}

/*
 * The backing page lock serializes aliases of the same physical page. The
 * mapping read lock proves that the pinned page still belongs to this guest
 * address. Together they provide the 128-bit fallback without libatomic or
 * a pair of 64-bit compare-exchanges exposing a torn CASP result.
 */
static noinline int tcti_atomic_apply_locked(void *host_data,
				     enum tcti_atomic_memory_operation operation,
				     const void *expected, const void *operand,
				     void *old_value, size_t size, bool *exchanged)
{
	if (operation == TCTI_ATOMIC_MEMORY_CAS && !expected)
		return -EINVAL;
	if (!operand || !old_value || !exchanged)
		return -EINVAL;

	switch (size) {
	TCTI_ATOMIC_APPLY(u8, s8)
	TCTI_ATOMIC_APPLY(u16, s16)
	TCTI_ATOMIC_APPLY(u32, s32)
	TCTI_ATOMIC_APPLY(u64, s64)
	case 16: {
		u8 old[16];
		u8 value[16];
		u8 result[16];
		size_t i;

		memcpy(old, host_data, sizeof(old));
		memcpy(value, operand, sizeof(value));
		switch (operation) {
		case TCTI_ATOMIC_MEMORY_CAS:
			memcpy(old_value, old, sizeof(old));
			*exchanged = !memcmp(old, expected, sizeof(old));
			if (*exchanged)
				memcpy(host_data, value, sizeof(value));
			return 0;
		case TCTI_ATOMIC_MEMORY_SWP:
			memcpy(result, value, sizeof(result));
			break;
		case TCTI_ATOMIC_MEMORY_CLR:
			for (i = 0; i < sizeof(result); i++)
				result[i] = old[i] & ~value[i];
			break;
		case TCTI_ATOMIC_MEMORY_SET:
			for (i = 0; i < sizeof(result); i++)
				result[i] = old[i] | value[i];
			break;
		default:
			return -EOPNOTSUPP;
		}
		memcpy(old_value, old, sizeof(old));
		memcpy(host_data, result, sizeof(result));
		*exchanged = true;
		return 0;
	}
	default:
		return -EINVAL;
	}
}

#undef TCTI_ATOMIC_APPLY


int tcti_atomic_user_data(struct mm_struct *mm, unsigned long user_va,
			  enum tcti_atomic_memory_operation operation,
			  enum tcti_atomic_memory_order order,
			  const void *expected, const void *operand,
			  void *old_value, size_t size, bool *exchanged)
{
	unsigned long linux_perms = 0;
	void *host_page;
	void *host_data;
	struct tcti_user_page page;
	bool wrote;
	int ret;

	if (!mm || !operand || !old_value || !exchanged)
		return -EINVAL;
	if (size != sizeof(u8) && size != sizeof(u16) &&
	    size != sizeof(u32) && size != sizeof(u64) && size != 16)
		return -EINVAL;
	if (!IS_ALIGNED(user_va, size) ||
	    size > PAGE_SIZE - offset_in_page(user_va) ||
	    user_va >= TASK_SIZE || size > TASK_SIZE - user_va)
		return -EFAULT;

	ret = tcti_atomic_memory_order_before(order);
	if (ret)
		return ret;

	for (;;) {
		ret = tcti_fault_in_user_page(mm, user_va, TCTI_ACCESS_WRITE);
		if (ret)
			return ret;
		ret = tcti_sync_faulted_user_window(mm, user_va,
						    TCTI_ACCESS_WRITE);
		if (ret)
			return ret;

		ret = tcti_pin_user_page(mm, user_va, TCTI_ACCESS_WRITE, &page);
		if (!ret) {
			host_data = (char *)page.host_data + offset_in_page(user_va);
			linux_perms = page.linux_perms;
			host_page = (void *)((unsigned long)host_data & PAGE_MASK);
			lock_page(page.page);
			if (!tcti_mapping_access_lock(
				    mm, page.translation_generation)) {
				unlock_page(page.page);
				tcti_unpin_user_page(&page);
				continue;
			}
			ret = tcti_atomic_apply_locked(host_data, operation, expected,
						       operand, old_value, size, exchanged);
			if (!ret && (operation != TCTI_ATOMIC_MEMORY_CAS || *exchanged))
				tcti_reservation_generation_bump(page.page);
			tcti_mapping_access_unlock(mm);
			if (ret) {
				unlock_page(page.page);
				tcti_unpin_user_page(&page);
			}
		}
		if (!ret)
			break;
		if (ret != -EFAULT && ret != -EACCES)
			return ret;
	}

	wrote = operation != TCTI_ATOMIC_MEMORY_CAS || *exchanged;
	if (wrote && (linux_perms & VM_EXEC))
		tcti_block_cache_invalidate_range(mm, user_va, user_va + size);
#if defined(ORLIX_APP_HOSTED_BOOT)
	if (wrote) {
		ret = orlix_refresh_user_mapping_range_from_kernel(mm, user_va,
							   host_page, size);
		if (ret) {
			tcti_note_committed_refresh_failure(mm, user_va, size, ret);
			ret = 0;
		}
	}
#endif
	tcti_atomic_memory_order_after(order);
	unlock_page(page.page);
	tcti_unpin_user_page(&page);
	if (ret)
		return ret;
	return 0;
}


int tcti_compare_exchange_user_data(struct mm_struct *mm,
				    unsigned long user_va,
				    const void *expected,
				    const void *desired,
				    size_t size, bool *exchanged)
{
	u8 old_value[16];

	return tcti_atomic_user_data(mm, user_va, TCTI_ATOMIC_MEMORY_CAS,
				     TCTI_ATOMIC_MEMORY_ACQ_REL, expected, desired,
				     old_value, size, exchanged);
}
