// SPDX-License-Identifier: GPL-2.0-only
#include <linux/errno.h>
#include <linux/bits.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/xarray.h>
#include <asm/orlix_tcti.h>

#define ORLIX_TCTI_MTE_GRANULE_SHIFT 4U
#define ORLIX_TCTI_MTE_TAG_MASK 0xfU

struct orlix_tcti_mte_tag_entry {
	struct page *page;
	u8 tag;
};

struct orlix_tcti_mte_state {
	struct mutex lock;
	struct xarray tags;
};

int orlix_tcti_mte_init_mm(struct mm_struct *mm)
{
	struct orlix_tcti_mte_state *state;

	if (!mm)
		return -EINVAL;
	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (!state)
		return -ENOMEM;
	mutex_init(&state->lock);
	xa_init(&state->tags);
	mm->context.orlix_tcti_mte = state;
	return 0;
}

static unsigned long orlix_tcti_mte_key(const struct page *page,
				       unsigned long address)
{
	return (page_to_pfn(page) << (PAGE_SHIFT - ORLIX_TCTI_MTE_GRANULE_SHIFT)) |
		(offset_in_page(address) >> ORLIX_TCTI_MTE_GRANULE_SHIFT);
}

int orlix_tcti_mte_load_allocation_tag(struct mm_struct *mm,
				 unsigned long address, u8 *tag)
{
	struct orlix_tcti_mte_tag_entry *entry;
	struct orlix_tcti_user_page page;
	struct orlix_tcti_mte_state *state;
	unsigned long key;
	int ret = 0;

	if (!mm || !tag || !mm->context.orlix_tcti_mte)
		return -EINVAL;
	state = mm->context.orlix_tcti_mte;
	address &= GENMASK_ULL(55, 0);
	address &= ~GENMASK_ULL(ORLIX_TCTI_MTE_GRANULE_SHIFT - 1, 0);
	ret = orlix_tcti_pin_user_page_faulting(mm, address,
					    ORLIX_TCTI_ACCESS_READ, &page);
	if (ret)
		return ret;
	key = orlix_tcti_mte_key(page.page, address);
	mutex_lock(&state->lock);
	entry = xa_load(&state->tags, key);
	*tag = entry ? entry->tag : 0;
	mutex_unlock(&state->lock);
	orlix_tcti_unpin_user_page(&page);
	return 0;
}

int orlix_tcti_mte_store_allocation_tag(struct mm_struct *mm,
				  unsigned long address, u8 tag)
{
	struct orlix_tcti_mte_tag_entry *entry;
	struct orlix_tcti_mte_tag_entry *new_entry = NULL;
	struct orlix_tcti_user_page page;
	struct orlix_tcti_mte_state *state;
	unsigned long key;
	int ret = 0;

	if (!mm || !mm->context.orlix_tcti_mte)
		return -EINVAL;
	state = mm->context.orlix_tcti_mte;
	tag &= ORLIX_TCTI_MTE_TAG_MASK;
	address &= GENMASK_ULL(55, 0);
	address &= ~GENMASK_ULL(ORLIX_TCTI_MTE_GRANULE_SHIFT - 1, 0);
	ret = orlix_tcti_pin_user_page_faulting(mm, address,
					    ORLIX_TCTI_ACCESS_WRITE, &page);
	if (ret)
		return ret;
	if (tag) {
		new_entry = kmalloc(sizeof(*new_entry), GFP_KERNEL);
		if (!new_entry) {
			orlix_tcti_unpin_user_page(&page);
			return -ENOMEM;
		}
		new_entry->page = page.page;
		new_entry->tag = tag;
	}
	key = orlix_tcti_mte_key(page.page, address);
	mutex_lock(&state->lock);
	entry = xa_load(&state->tags, key);
	if (entry) {
		if (tag) {
			entry->tag = tag;
		} else {
			xa_erase(&state->tags, key);
			put_page(entry->page);
			kfree(entry);
		}
	} else if (tag) {
		get_page(page.page);
		ret = xa_err(xa_store(&state->tags, key,
				      new_entry, GFP_KERNEL));
		if (!ret)
			new_entry = NULL;
		else
			put_page(page.page);
	}
	mutex_unlock(&state->lock);
	kfree(new_entry);
	orlix_tcti_unpin_user_page(&page);
	return ret;
}

void orlix_tcti_mte_destroy_mm(struct mm_struct *mm)
{
	struct orlix_tcti_mte_tag_entry *entry;
	struct orlix_tcti_mte_state *state;
	unsigned long index;

	if (!mm || !mm->context.orlix_tcti_mte)
		return;
	state = mm->context.orlix_tcti_mte;
	mm->context.orlix_tcti_mte = NULL;
	mutex_lock(&state->lock);
	xa_for_each(&state->tags, index, entry) {
		xa_erase(&state->tags, index);
		put_page(entry->page);
		kfree(entry);
	}
	mutex_unlock(&state->lock);
	xa_destroy(&state->tags);
	kfree(state);
}
