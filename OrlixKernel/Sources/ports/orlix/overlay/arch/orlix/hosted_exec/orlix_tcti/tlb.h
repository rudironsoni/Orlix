/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TLB_H
#define ORLIX_TCTI_TLB_H

#include <linux/types.h>
#include <asm/page.h>
#include <asm/orlix_tcti.h>

#define ORLIX_TCTI_TLB_BITS 13U
#define ORLIX_TCTI_TLB_SIZE (1U << ORLIX_TCTI_TLB_BITS)

struct mm_struct;
struct page;

struct orlix_tcti_tlb_entry {
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

struct orlix_tcti_tlb {
	struct mm_struct *active_mm;
	u64 active_generation;
	struct orlix_tcti_tlb_entry entries[ORLIX_TCTI_TLB_SIZE];
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

static inline unsigned int orlix_tcti_tlb_index(unsigned long addr)
{
	return ((addr >> PAGE_SHIFT) ^
		(addr >> (PAGE_SHIFT + ORLIX_TCTI_TLB_BITS))) &
	       (ORLIX_TCTI_TLB_SIZE - 1);
}

void orlix_tcti_tlb_init(struct orlix_tcti_tlb *tlb);
void orlix_tcti_tlb_flush(struct orlix_tcti_tlb *tlb);
void orlix_tcti_tlb_flush_if_generation_changed(struct orlix_tcti_tlb *tlb,
					  struct mm_struct *mm,
					  u64 translation_generation);
void *orlix_tcti_tlb_lookup(struct orlix_tcti_tlb *tlb,
		      struct mm_struct *mm,
		      unsigned long guest_addr,
		      enum orlix_tcti_access access,
		      u64 translation_generation);
int orlix_tcti_tlb_lookup_page(struct orlix_tcti_tlb *tlb,
			 struct mm_struct *mm,
			 unsigned long guest_addr,
			 enum orlix_tcti_access access,
			 u64 translation_generation,
			 void **host_data,
			 unsigned long *linux_perms);
int orlix_tcti_tlb_fill(struct orlix_tcti_tlb *tlb,
		  struct mm_struct *mm,
		  unsigned long guest_addr,
		  enum orlix_tcti_access access,
		  const struct orlix_tcti_user_page *page);
struct orlix_tcti_tlb *orlix_tcti_tlb_get_current(void);

#endif /* ORLIX_TCTI_TLB_H */
