/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _ASM_ORLIX_MTE_H
#define _ASM_ORLIX_MTE_H

#include <linux/bits.h>
#include <linux/prctl.h>
#include <linux/types.h>

struct mm_struct;
struct page;
struct pt_regs;

#define ORLIX_MTE_GRANULE_SIZE 16U
#define ORLIX_MTE_TAG_SHIFT 56U
#define ORLIX_MTE_TAG_MASK GENMASK_ULL(59, 56)
#define ORLIX_MTE_ADDRESS_MASK GENMASK_ULL(55, 0)

/* Async/asymmetric checking is rejected until a Linux return-to-user hook
 * owns deferred SEGV_MTEAERR delivery.  No HWCAP MTE variant is exposed. */
#define ORLIX_MTE_SUPPORTED_TCF PR_MTE_TCF_SYNC

/* Pinned Linux v6.12 include/uapi/linux/prctl.h exposes SYNC and ASYNC only.
 * DDI0602's asynchronous mode needs return-to-user delivery, which this port
 * does not implement.  ASYMM has no v6.12 PR_SET_TAGGED_ADDR_CTRL encoding. */
enum orlix_mte_prctl_tcf_disposition {
	ORLIX_MTE_PRCTL_TCF_SYNC_SUPPORTED,
	ORLIX_MTE_PRCTL_TCF_ASYNC_REJECTED_NO_RETURN_TO_USER,
	ORLIX_MTE_PRCTL_TCF_ASYMM_UNAVAILABLE_V612_UAPI,
};

static inline enum orlix_mte_prctl_tcf_disposition
orlix_mte_prctl_tcf_disposition(unsigned long tcf)
{
	if (tcf & PR_MTE_TCF_ASYNC)
		return ORLIX_MTE_PRCTL_TCF_ASYNC_REJECTED_NO_RETURN_TO_USER;
	return ORLIX_MTE_PRCTL_TCF_SYNC_SUPPORTED;
}

static inline unsigned long orlix_mte_untagged_address(unsigned long address)
{
	return address & ORLIX_MTE_ADDRESS_MASK;
}

int orlix_mte_init_mm(struct mm_struct *mm);
void orlix_mte_destroy_mm(struct mm_struct *mm);
int orlix_mte_load_allocation_tag(struct mm_struct *mm, unsigned long address,
				  u8 *tag);
int orlix_mte_load_allocation_tags(struct mm_struct *mm, unsigned long address,
				   u8 *tags, unsigned int granules);
int orlix_mte_store_allocation_tag(struct mm_struct *mm, unsigned long address,
				   u8 tag);
int orlix_mte_store_allocation_tags(struct mm_struct *mm,
				    unsigned long address, u8 tag,
				    unsigned int granules);
int orlix_mte_check_access(struct mm_struct *mm, unsigned long address,
				   size_t size, bool write);
void orlix_mte_copy_page_tags(struct page *to, struct page *from);
void orlix_mte_clear_page_tags(struct page *page);
void orlix_mte_sync_page_tags(struct page *page);
void arch_alloc_page(struct page *page, int order);
void arch_free_page(struct page *page, int order);
void orlix_mte_signal_sync_fault(struct pt_regs *regs, unsigned long address);

#endif /* _ASM_ORLIX_MTE_H */
