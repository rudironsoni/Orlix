// SPDX-License-Identifier: GPL-2.0-only
/* Architectural memory metadata not representable by Darwin VM mappings. */
#include <asm/orlix_tcti.h>
#include <linux/bitfield.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/xarray.h>

#define ORLIX_TCTI_TAG_GRANULE 16UL
#define ORLIX_TCTI_METADATA_TAG_MASK GENMASK(3, 0)
#define ORLIX_TCTI_METADATA_TAG_VALID BIT(4)
#define ORLIX_TCTI_METADATA_GCS BIT(5)
#define ORLIX_TCTI_LOGICAL_TAG_SHIFT 56
#define ORLIX_TCTI_UNTAGGED_ADDRESS_MASK GENMASK_ULL(55, 0)

struct orlix_tcti_memory_metadata {
	struct mutex lock;
	struct xarray entries;
};

struct orlix_tcti_memory_metadata_entry {
	struct page *page;
	unsigned long value;
};

static struct orlix_tcti_memory_metadata *
orlix_tcti_get_memory_metadata(struct mm_struct *mm, bool create)
{
	struct orlix_tcti_memory_metadata *metadata;
	struct orlix_tcti_memory_metadata *existing;

	metadata = READ_ONCE(mm->context.orlix_tcti_memory_metadata);
	if (metadata || !create)
		return metadata;
	metadata = kzalloc(sizeof(*metadata), GFP_KERNEL);
	if (!metadata)
		return NULL;
	mutex_init(&metadata->lock);
	xa_init(&metadata->entries);
	existing = cmpxchg(&mm->context.orlix_tcti_memory_metadata, NULL,
			   metadata);
	if (!existing)
		return metadata;
	xa_destroy(&metadata->entries);
	kfree(metadata);
	return existing;
}

static unsigned long orlix_tcti_metadata_index(struct page *page,
					       unsigned long address)
{
	return (page_to_pfn(page) << (PAGE_SHIFT - 4)) |
		(offset_in_page(address) / ORLIX_TCTI_TAG_GRANULE);
}

static unsigned long orlix_tcti_metadata_value(
			struct orlix_tcti_memory_metadata_entry *entry)
{
	return entry ? entry->value : 0;
}

static int orlix_tcti_metadata_store_locked(
			       struct orlix_tcti_memory_metadata *metadata,
				       unsigned long index, struct page *page,
				       unsigned long value, gfp_t gfp)
{
	struct orlix_tcti_memory_metadata_entry *entry;
	void *stored;

	if (!value) {
		entry = xa_erase(&metadata->entries, index);
		if (entry) {
			put_page(entry->page);
			kfree(entry);
		}
		return 0;
	}
	entry = xa_load(&metadata->entries, index);
	if (xa_is_zero(entry))
		entry = NULL;
	if (entry) {
		entry->value = value;
		return 0;
	}
	entry = kmalloc(sizeof(*entry), gfp);
	if (!entry)
		return -ENOMEM;
	get_page(page);
	entry->page = page;
	entry->value = value;
	stored = xa_store(&metadata->entries, index, entry, gfp);
	if (xa_is_err(stored)) {
		put_page(page);
		kfree(entry);
		return xa_err(stored);
	}
	return 0;
}

int orlix_tcti_set_gcs_memory(struct mm_struct *mm, unsigned long user_va,
			 size_t size, bool enabled)
{
	unsigned long address;
	unsigned long end;
	struct orlix_tcti_memory_metadata *metadata;
	int ret = 0;

	if (!mm || !size || user_va > ULONG_MAX - size)
		return -EINVAL;
	address = user_va & ORLIX_TCTI_UNTAGGED_ADDRESS_MASK;
	end = (user_va + size - 1) & ORLIX_TCTI_UNTAGGED_ADDRESS_MASK;
	address &= ~(ORLIX_TCTI_TAG_GRANULE - 1);
	metadata = orlix_tcti_get_memory_metadata(mm, enabled);
	if (!metadata)
		return enabled ? -ENOMEM : 0;
	while (address <= end) {
		struct orlix_tcti_user_page pinned;
		unsigned long index;
		unsigned long value;

		ret = orlix_tcti_pin_user_page_faulting(mm, address,
						 ORLIX_TCTI_ACCESS_WRITE,
						 &pinned);
		if (ret)
			break;
		index = orlix_tcti_metadata_index(pinned.page, address);
		mutex_lock(&metadata->lock);
		value = orlix_tcti_metadata_value(xa_load(
			&metadata->entries, index));

		if (enabled)
			value |= ORLIX_TCTI_METADATA_GCS;
		else
			value &= ~ORLIX_TCTI_METADATA_GCS;
		ret = orlix_tcti_metadata_store_locked(metadata, index,
						       pinned.page, value,
						       GFP_KERNEL);
		mutex_unlock(&metadata->lock);
		orlix_tcti_unpin_user_page(&pinned);
		if (ret)
			break;
		address += ORLIX_TCTI_TAG_GRANULE;
	}
	return ret;
}

int orlix_tcti_write_gcs_user_data(struct mm_struct *mm,
			      unsigned long user_va, const void *buffer,
			      size_t size)
{
	unsigned long address;
	unsigned long first;
	unsigned long last;
	struct orlix_tcti_user_page first_page;
	struct orlix_tcti_user_page last_page;
	struct orlix_tcti_memory_metadata *metadata;
	int ret;

	if (!mm || !buffer || !size || user_va > ULONG_MAX - size)
		return -EINVAL;
	address = user_va & ORLIX_TCTI_UNTAGGED_ADDRESS_MASK;
	ret = orlix_tcti_pin_user_page_faulting(mm, address,
						 ORLIX_TCTI_ACCESS_WRITE,
						 &first_page);
	if (ret)
		return ret;
	ret = orlix_tcti_pin_user_page_faulting(mm, address + size - 1,
						 ORLIX_TCTI_ACCESS_WRITE,
						 &last_page);
	if (ret) {
		orlix_tcti_unpin_user_page(&first_page);
		return ret;
	}
	first = orlix_tcti_metadata_index(first_page.page, address);
	last = orlix_tcti_metadata_index(last_page.page, address + size - 1);
	metadata = orlix_tcti_get_memory_metadata(mm, false);
	if (!metadata) {
		ret = -EACCES;
		goto out_unpin;
	}
	mutex_lock(&metadata->lock);
	if (!(orlix_tcti_metadata_value(xa_load(
		&metadata->entries, first)) &
	      ORLIX_TCTI_METADATA_GCS) ||
	    !(orlix_tcti_metadata_value(xa_load(
		&metadata->entries, last)) &
	      ORLIX_TCTI_METADATA_GCS)) {
		ret = -EACCES;
	} else {
		ret = 0;
	}
	mutex_unlock(&metadata->lock);
	if (!ret)
		ret = orlix_tcti_write_user_data(mm, address, buffer, size);
out_unpin:
	orlix_tcti_unpin_user_page(&last_page);
	orlix_tcti_unpin_user_page(&first_page);
	return ret;
}

int orlix_tcti_store_tagged_pair(struct mm_struct *mm,
			    unsigned long tagged_user_va,
			    const void *buffer, size_t size)
{
	unsigned long address = tagged_user_va & ORLIX_TCTI_UNTAGGED_ADDRESS_MASK;
	unsigned long index;
	unsigned long old_value;
	unsigned long new_value;
	struct orlix_tcti_user_page pinned;
	struct orlix_tcti_memory_metadata *metadata;
	int ret;

	if (!mm || !buffer || size != ORLIX_TCTI_TAG_GRANULE ||
	    !IS_ALIGNED(address, ORLIX_TCTI_TAG_GRANULE))
		return -EINVAL;
	metadata = orlix_tcti_get_memory_metadata(mm, true);
	if (!metadata)
		return -ENOMEM;
	ret = orlix_tcti_pin_user_page_faulting(mm, address,
						 ORLIX_TCTI_ACCESS_WRITE, &pinned);
	if (ret)
		return ret;
	index = orlix_tcti_metadata_index(pinned.page, address);
	mutex_lock(&metadata->lock);
	old_value = orlix_tcti_metadata_value(xa_load(
		&metadata->entries, index));
	new_value = (old_value & ORLIX_TCTI_METADATA_GCS) |
		ORLIX_TCTI_METADATA_TAG_VALID |
		FIELD_PREP(ORLIX_TCTI_METADATA_TAG_MASK,
			   (tagged_user_va >> ORLIX_TCTI_LOGICAL_TAG_SHIFT) & 0xfU);
	ret = orlix_tcti_metadata_store_locked(metadata, index, pinned.page,
					       new_value, GFP_KERNEL);
	if (ret)
		goto out;
	ret = orlix_tcti_write_user_data(mm, address, buffer, size);
	if (ret)
		WARN_ON_ONCE(orlix_tcti_metadata_store_locked(metadata, index,
							pinned.page, old_value,
							GFP_NOWAIT));
out:
	mutex_unlock(&metadata->lock);
	orlix_tcti_unpin_user_page(&pinned);
	return ret;
}

int orlix_tcti_load_allocation_tag(struct mm_struct *mm,
			      unsigned long user_va, u8 *tag)
{
	unsigned long value;
	unsigned long address;
	unsigned long index;
	struct orlix_tcti_user_page pinned;
	struct orlix_tcti_memory_metadata *metadata;
	int ret;

	if (!mm || !tag)
		return -EINVAL;
	address = user_va & ORLIX_TCTI_UNTAGGED_ADDRESS_MASK;
	ret = orlix_tcti_pin_user_page_faulting(mm, address,
						 ORLIX_TCTI_ACCESS_READ, &pinned);
	if (ret)
		return ret;
	index = orlix_tcti_metadata_index(pinned.page, address);
	metadata = orlix_tcti_get_memory_metadata(mm, false);
	if (!metadata) {
		orlix_tcti_unpin_user_page(&pinned);
		return -ENOENT;
	}
	mutex_lock(&metadata->lock);
	value = orlix_tcti_metadata_value(xa_load(
		&metadata->entries, index));
	mutex_unlock(&metadata->lock);
	orlix_tcti_unpin_user_page(&pinned);
	if (!(value & ORLIX_TCTI_METADATA_TAG_VALID))
		return -ENOENT;
	*tag = FIELD_GET(ORLIX_TCTI_METADATA_TAG_MASK, value);
	return 0;
}

void orlix_tcti_memory_metadata_destroy(struct mm_struct *mm)
{
	struct orlix_tcti_memory_metadata *metadata;
	struct orlix_tcti_memory_metadata_entry *entry;
	unsigned long index;

	if (!mm)
		return;
	metadata = xchg(&mm->context.orlix_tcti_memory_metadata, NULL);
	if (!metadata)
		return;
	xa_for_each(&metadata->entries, index, entry) {
		put_page(entry->page);
		kfree(entry);
	}
	xa_destroy(&metadata->entries);
	kfree(metadata);
}
