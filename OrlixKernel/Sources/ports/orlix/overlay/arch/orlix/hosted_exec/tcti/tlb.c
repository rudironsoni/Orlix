// SPDX-License-Identifier: GPL-2.0-only
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/string.h>

#include "tlb.h"

static bool tcti_tlb_access_allowed(const struct tcti_tlb_entry *entry,
				    enum tcti_access access)
{
	switch (access) {
	case TCTI_ACCESS_FETCH:
		return entry->fetch_ok;
	case TCTI_ACCESS_READ:
		return entry->read_ok;
	case TCTI_ACCESS_WRITE:
		return entry->write_ok;
	default:
		return false;
	}
}

static void tcti_tlb_count_hit(struct tcti_tlb *tlb, enum tcti_access access)
{
	switch (access) {
	case TCTI_ACCESS_FETCH:
		tlb->fetch_hits++;
		break;
	case TCTI_ACCESS_READ:
		tlb->read_hits++;
		break;
	case TCTI_ACCESS_WRITE:
		tlb->write_hits++;
		break;
	default:
		break;
	}
}

static void tcti_tlb_count_miss(struct tcti_tlb *tlb, enum tcti_access access)
{
	switch (access) {
	case TCTI_ACCESS_FETCH:
		tlb->fetch_misses++;
		break;
	case TCTI_ACCESS_READ:
		tlb->read_misses++;
		break;
	case TCTI_ACCESS_WRITE:
		tlb->write_misses++;
		break;
	default:
		tlb->faults++;
		break;
	}
}

void tcti_tlb_init(struct tcti_tlb *tlb)
{
	if (tlb)
		memset(tlb, 0, sizeof(*tlb));
}

void tcti_tlb_flush(struct tcti_tlb *tlb)
{
	if (!tlb)
		return;

	memset(tlb->entries, 0, sizeof(tlb->entries));
	tlb->active_mm = NULL;
	tlb->active_generation = 0;
	tlb->generation_flushes++;
}

void tcti_tlb_flush_if_generation_changed(struct tcti_tlb *tlb,
					  struct mm_struct *mm,
					  u32 translation_generation)
{
	if (!tlb)
		return;

	if (tlb->active_mm != mm ||
	    tlb->active_generation != translation_generation) {
		tcti_tlb_flush(tlb);
		tlb->active_mm = mm;
		tlb->active_generation = translation_generation;
	}
}

void *tcti_tlb_lookup(struct tcti_tlb *tlb,
		      struct mm_struct *mm,
		      unsigned long guest_addr,
		      enum tcti_access access,
		      u32 translation_generation)
{
	struct tcti_tlb_entry *entry;

	if (!tlb || !mm) {
		if (tlb)
			tlb->faults++;
		return NULL;
	}

	tcti_tlb_flush_if_generation_changed(tlb, mm, translation_generation);
	entry = &tlb->entries[tcti_tlb_index(guest_addr)];
	if (entry->mm == mm &&
	    entry->guest_page == (guest_addr & PAGE_MASK) &&
	    entry->translation_generation == translation_generation &&
	    tcti_tlb_access_allowed(entry, access)) {
		tcti_tlb_count_hit(tlb, access);
		return (char *)entry->host_page + offset_in_page(guest_addr);
	}

	tcti_tlb_count_miss(tlb, access);
	return NULL;
}

int tcti_tlb_fill(struct tcti_tlb *tlb,
		  struct mm_struct *mm,
		  unsigned long guest_addr,
		  enum tcti_access access,
		  const struct tcti_user_page *page)
{
	struct tcti_tlb_entry *entry;

	if (!tlb || !mm || !page || !page->host_data)
		return -EINVAL;
	if ((guest_addr & PAGE_MASK) != page->user_page)
		return -EINVAL;

	tcti_tlb_flush_if_generation_changed(tlb, mm,
					     page->translation_generation);
	entry = &tlb->entries[tcti_tlb_index(guest_addr)];
	memset(entry, 0, sizeof(*entry));
	entry->mm = mm;
	entry->guest_page = page->user_page;
	entry->host_page = page->host_data;
	entry->page = page->page;
	entry->translation_generation = page->translation_generation;
	entry->cow_sensitive = page->cow_sensitive;
	entry->has_translated_blocks = page->has_translated_blocks;

	switch (access) {
	case TCTI_ACCESS_FETCH:
		entry->fetch_ok = true;
		break;
	case TCTI_ACCESS_READ:
		entry->read_ok = true;
		break;
	case TCTI_ACCESS_WRITE:
		entry->write_ok = true;
		break;
	default:
		memset(entry, 0, sizeof(*entry));
		return -EINVAL;
	}

	return 0;
}
