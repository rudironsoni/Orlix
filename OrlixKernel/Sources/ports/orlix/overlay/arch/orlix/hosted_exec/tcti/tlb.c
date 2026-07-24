// SPDX-License-Identifier: GPL-2.0-only
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <asm/processor.h>

#include "block_cache.h"
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
	u32 i;

	if (!tlb)
		return;

	for (i = 0; i < TCTI_TLB_SIZE; i++) {
		if (tlb->entries[i].page)
			put_page(tlb->entries[i].page);
	}
	memset(tlb->entries, 0, sizeof(tlb->entries));
	tlb->active_mm = NULL;
	tlb->active_generation = 0;
	tlb->generation_flushes++;
}

void tcti_tlb_flush_if_generation_changed(struct tcti_tlb *tlb,
					  struct mm_struct *mm,
					  u64 translation_generation)
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
		      u64 translation_generation)
{
	void *host_data = NULL;

	if (tcti_tlb_lookup_page(tlb, mm, guest_addr, access,
				 translation_generation, &host_data, NULL))
		return NULL;

	return host_data;
}

int tcti_tlb_lookup_page(struct tcti_tlb *tlb,
			 struct mm_struct *mm,
			 unsigned long guest_addr,
			 enum tcti_access access,
			 u64 translation_generation,
			 void **host_data,
			 unsigned long *linux_perms)
{
	struct tcti_tlb_entry *entry;
	u64 current_generation;

	if (!tlb || !mm || !host_data) {
		if (tlb)
			tlb->faults++;
		return -EINVAL;
	}

	current_generation = tcti_translation_generation(mm);
	tcti_tlb_flush_if_generation_changed(tlb, mm, current_generation);
	if (translation_generation != current_generation ||
	    !tcti_translation_generation_stable(mm, current_generation)) {
		tcti_tlb_count_miss(tlb, access);
		return -EAGAIN;
	}

	entry = &tlb->entries[tcti_tlb_index(guest_addr)];
	if (entry->mm == mm &&
	    entry->guest_page == (guest_addr & PAGE_MASK) &&
	    entry->translation_generation == translation_generation &&
	    tcti_tlb_access_allowed(entry, access)) {
		if (!tcti_translation_generation_stable(mm,
						       current_generation)) {
			tcti_tlb_count_miss(tlb, access);
			return -EAGAIN;
		}
		tcti_tlb_count_hit(tlb, access);
		*host_data = (char *)entry->host_page +
			     offset_in_page(guest_addr);
		if (linux_perms)
			*linux_perms = entry->linux_perms;
		return 0;
	}

	tcti_tlb_count_miss(tlb, access);
	return -ENOENT;
}

int tcti_tlb_fill(struct tcti_tlb *tlb,
		  struct mm_struct *mm,
		  unsigned long guest_addr,
		  enum tcti_access access,
		  const struct tcti_user_page *page)
{
	struct tcti_tlb_entry *entry;
	u64 current_generation;

	if (!tlb || !mm || !page || !page->host_data)
		return -EINVAL;
	if ((guest_addr & PAGE_MASK) != page->user_page)
		return -EINVAL;
	if (access != TCTI_ACCESS_FETCH && access != TCTI_ACCESS_READ &&
	    access != TCTI_ACCESS_WRITE)
		return -EINVAL;

	current_generation = tcti_translation_generation(mm);
	tcti_tlb_flush_if_generation_changed(tlb, mm, current_generation);
	if (page->translation_generation != current_generation ||
	    !tcti_translation_generation_stable(mm, current_generation))
		return -EAGAIN;

	entry = &tlb->entries[tcti_tlb_index(guest_addr)];
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
		break;
	}

	if (!tcti_translation_generation_stable(mm, current_generation)) {
		if (entry->page)
			put_page(entry->page);
		memset(entry, 0, sizeof(*entry));
		return -EAGAIN;
	}

	return 0;
}

struct tcti_tlb *tcti_tlb_get_current(void)
{
	struct tcti_tlb *tlb;

	tlb = current->thread.orlix_tcti_tlb;
	if (tlb)
		return tlb;

	tlb = kvzalloc(sizeof(*tlb), GFP_KERNEL);
	if (!tlb)
		return NULL;

	tcti_tlb_init(tlb);
	current->thread.orlix_tcti_tlb = tlb;
	return tlb;
}

void tcti_flush_task_state(struct task_struct *task)
{
	if (task && task->thread.orlix_tcti_tlb)
		tcti_tlb_flush(task->thread.orlix_tcti_tlb);
}

void tcti_release_task_state(struct task_struct *task)
{
	struct tcti_tlb *tlb;

	if (!task)
		return;

	tlb = task->thread.orlix_tcti_tlb;
	task->thread.orlix_tcti_tlb = NULL;
	if (!tlb)
		return;

	tcti_tlb_flush(tlb);
	kvfree(tlb);
}
