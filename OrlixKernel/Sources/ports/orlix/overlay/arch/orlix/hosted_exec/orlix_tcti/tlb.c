// SPDX-License-Identifier: GPL-2.0-only
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <asm/processor.h>

#include "block_cache.h"
#include "tlb.h"

static bool orlix_tcti_tlb_access_allowed(const struct orlix_tcti_tlb_entry *entry,
				    enum orlix_tcti_access access)
{
	switch (access) {
	case ORLIX_TCTI_ACCESS_FETCH:
		return entry->fetch_ok;
	case ORLIX_TCTI_ACCESS_READ:
		return entry->read_ok;
	case ORLIX_TCTI_ACCESS_WRITE:
		return entry->write_ok;
	default:
		return false;
	}
}

static void orlix_tcti_tlb_count_hit(struct orlix_tcti_tlb *tlb, enum orlix_tcti_access access)
{
	switch (access) {
	case ORLIX_TCTI_ACCESS_FETCH:
		tlb->fetch_hits++;
		break;
	case ORLIX_TCTI_ACCESS_READ:
		tlb->read_hits++;
		break;
	case ORLIX_TCTI_ACCESS_WRITE:
		tlb->write_hits++;
		break;
	default:
		break;
	}
}

static void orlix_tcti_tlb_count_miss(struct orlix_tcti_tlb *tlb, enum orlix_tcti_access access)
{
	switch (access) {
	case ORLIX_TCTI_ACCESS_FETCH:
		tlb->fetch_misses++;
		break;
	case ORLIX_TCTI_ACCESS_READ:
		tlb->read_misses++;
		break;
	case ORLIX_TCTI_ACCESS_WRITE:
		tlb->write_misses++;
		break;
	default:
		tlb->faults++;
		break;
	}
}

void orlix_tcti_tlb_init(struct orlix_tcti_tlb *tlb)
{
	if (tlb)
		memset(tlb, 0, sizeof(*tlb));
}

void orlix_tcti_tlb_flush(struct orlix_tcti_tlb *tlb)
{
	u32 i;

	if (!tlb)
		return;

	for (i = 0; i < ORLIX_TCTI_TLB_SIZE; i++) {
		if (tlb->entries[i].page)
			put_page(tlb->entries[i].page);
	}
	memset(tlb->entries, 0, sizeof(tlb->entries));
	tlb->active_mm = NULL;
	tlb->active_generation = 0;
	tlb->generation_flushes++;
}

void orlix_tcti_tlb_flush_if_generation_changed(struct orlix_tcti_tlb *tlb,
					  struct mm_struct *mm,
					  u64 translation_generation)
{
	if (!tlb)
		return;

	if (tlb->active_mm != mm ||
	    tlb->active_generation != translation_generation) {
		orlix_tcti_tlb_flush(tlb);
		tlb->active_mm = mm;
		tlb->active_generation = translation_generation;
	}
}

void *orlix_tcti_tlb_lookup(struct orlix_tcti_tlb *tlb,
		      struct mm_struct *mm,
		      unsigned long guest_addr,
		      enum orlix_tcti_access access,
		      u64 translation_generation)
{
	void *host_data = NULL;

	if (orlix_tcti_tlb_lookup_page(tlb, mm, guest_addr, access,
				 translation_generation, &host_data, NULL))
		return NULL;

	return host_data;
}

int orlix_tcti_tlb_lookup_page(struct orlix_tcti_tlb *tlb,
			 struct mm_struct *mm,
			 unsigned long guest_addr,
			 enum orlix_tcti_access access,
			 u64 translation_generation,
			 void **host_data,
			 unsigned long *linux_perms)
{
	struct orlix_tcti_tlb_entry *entry;
	u64 current_generation;

	if (!tlb || !mm || !host_data) {
		if (tlb)
			tlb->faults++;
		return -EINVAL;
	}

	current_generation = orlix_tcti_translation_generation(mm);
	orlix_tcti_tlb_flush_if_generation_changed(tlb, mm, current_generation);
	if (translation_generation != current_generation ||
	    !orlix_tcti_translation_generation_stable(mm, current_generation)) {
		orlix_tcti_tlb_count_miss(tlb, access);
		return -EAGAIN;
	}

	entry = &tlb->entries[orlix_tcti_tlb_index(guest_addr)];
	if (entry->mm == mm &&
	    entry->guest_page == (guest_addr & PAGE_MASK) &&
	    entry->translation_generation == translation_generation &&
	    orlix_tcti_tlb_access_allowed(entry, access)) {
		if (!orlix_tcti_translation_generation_stable(mm,
						       current_generation)) {
			orlix_tcti_tlb_count_miss(tlb, access);
			return -EAGAIN;
		}
		orlix_tcti_tlb_count_hit(tlb, access);
		*host_data = (char *)entry->host_page +
			     offset_in_page(guest_addr);
		if (linux_perms)
			*linux_perms = entry->linux_perms;
		return 0;
	}

	orlix_tcti_tlb_count_miss(tlb, access);
	return -ENOENT;
}

int orlix_tcti_tlb_fill(struct orlix_tcti_tlb *tlb,
		  struct mm_struct *mm,
		  unsigned long guest_addr,
		  enum orlix_tcti_access access,
		  const struct orlix_tcti_user_page *page)
{
	struct orlix_tcti_tlb_entry *entry;
	u64 current_generation;

	if (!tlb || !mm || !page || !page->host_data)
		return -EINVAL;
	if ((guest_addr & PAGE_MASK) != page->user_page)
		return -EINVAL;
	if (access != ORLIX_TCTI_ACCESS_FETCH && access != ORLIX_TCTI_ACCESS_READ &&
	    access != ORLIX_TCTI_ACCESS_WRITE)
		return -EINVAL;

	current_generation = orlix_tcti_translation_generation(mm);
	orlix_tcti_tlb_flush_if_generation_changed(tlb, mm, current_generation);
	if (page->translation_generation != current_generation ||
	    !orlix_tcti_translation_generation_stable(mm, current_generation))
		return -EAGAIN;

	entry = &tlb->entries[orlix_tcti_tlb_index(guest_addr)];
	if (entry->mm == mm &&
	    entry->guest_page == page->user_page &&
	    entry->host_page == page->host_data &&
	    entry->translation_generation == page->translation_generation) {
		entry->linux_perms = page->linux_perms;
		goto authorize_access;
	}

	if (entry->page)
		put_page(entry->page);
	memset(entry, 0, sizeof(*entry));
	entry->mm = mm;
	entry->guest_page = page->user_page;
	entry->host_page = page->host_data;
	entry->page = page->page;
	if (entry->page)
		get_page(entry->page);
	entry->translation_generation = page->translation_generation;
	entry->linux_perms = page->linux_perms;
	entry->cow_sensitive = page->cow_sensitive;
	entry->has_translated_blocks = page->has_translated_blocks;

authorize_access:
	switch (access) {
	case ORLIX_TCTI_ACCESS_FETCH:
		entry->fetch_ok = true;
		break;
	case ORLIX_TCTI_ACCESS_READ:
		entry->read_ok = true;
		break;
	case ORLIX_TCTI_ACCESS_WRITE:
		entry->write_ok = true;
		break;
	default:
		break;
	}

	if (!orlix_tcti_translation_generation_stable(mm, current_generation)) {
		if (entry->page)
			put_page(entry->page);
		memset(entry, 0, sizeof(*entry));
		return -EAGAIN;
	}

	return 0;
}

struct orlix_tcti_tlb *orlix_tcti_tlb_get_current(void)
{
	struct orlix_tcti_tlb *tlb;

	tlb = current->thread.orlix_tcti_tlb;
	if (tlb)
		return tlb;

	tlb = kvzalloc(sizeof(*tlb), GFP_KERNEL);
	if (!tlb)
		return NULL;

	orlix_tcti_tlb_init(tlb);
	current->thread.orlix_tcti_tlb = tlb;
	return tlb;
}

void orlix_tcti_flush_task_state(struct task_struct *task)
{
	if (task && task->thread.orlix_tcti_tlb)
		orlix_tcti_tlb_flush(task->thread.orlix_tcti_tlb);
}

void orlix_tcti_release_task_state(struct task_struct *task)
{
	struct orlix_tcti_tlb *tlb;

	if (!task)
		return;

	tlb = task->thread.orlix_tcti_tlb;
	task->thread.orlix_tcti_tlb = NULL;
	if (!tlb)
		return;

	orlix_tcti_tlb_flush(tlb);
	kvfree(tlb);
}
