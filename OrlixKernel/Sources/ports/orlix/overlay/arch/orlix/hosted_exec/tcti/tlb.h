/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TLB_H
#define ORLIX_TCTI_TLB_H

#include <linux/types.h>
#include <asm/page.h>
#include <asm/tcti.h>

#define TCTI_TLB_BITS 13U
#define TCTI_TLB_SIZE (1U << TCTI_TLB_BITS)

struct mm_struct;
struct page;

struct tcti_tlb_entry {
	struct mm_struct *mm;
	unsigned long guest_page;
	void *host_page;
	struct page *page;
	u64 translation_generation;
	unsigned long linux_perms;
	bool fetch_ok;
	bool read_ok;
	bool write_ok;
	bool cow_sensitive;
	bool has_translated_blocks;
};

struct tcti_tlb {
	struct mm_struct *active_mm;
	u64 active_generation;
	struct tcti_tlb_entry entries[TCTI_TLB_SIZE];
	u64 fetch_hits;
	u64 fetch_misses;
	u64 read_hits;
	u64 read_misses;
	u64 write_hits;
	u64 write_misses;
	u64 cross_page_reads;
	u64 cross_page_writes;
	u64 faults;
	u64 generation_flushes;
};

static inline unsigned int tcti_tlb_index(unsigned long addr)
{
	return ((addr >> PAGE_SHIFT) ^
		(addr >> (PAGE_SHIFT + TCTI_TLB_BITS))) &
	       (TCTI_TLB_SIZE - 1);
}

void tcti_tlb_init(struct tcti_tlb *tlb);
void tcti_tlb_flush(struct tcti_tlb *tlb);
void tcti_tlb_flush_if_generation_changed(struct tcti_tlb *tlb,
					  struct mm_struct *mm,
					  u64 translation_generation);
void *tcti_tlb_lookup(struct tcti_tlb *tlb,
		      struct mm_struct *mm,
		      unsigned long guest_addr,
		      enum tcti_access access,
		      u64 translation_generation);
int tcti_tlb_lookup_page(struct tcti_tlb *tlb,
			 struct mm_struct *mm,
			 unsigned long guest_addr,
			 enum tcti_access access,
			 u64 translation_generation,
			 void **host_data,
			 unsigned long *linux_perms);
int tcti_tlb_fill(struct tcti_tlb *tlb,
		  struct mm_struct *mm,
		  unsigned long guest_addr,
		  enum tcti_access access,
		  const struct tcti_user_page *page);
struct tcti_tlb *tcti_tlb_get_current(void);

#endif /* ORLIX_TCTI_TLB_H */
