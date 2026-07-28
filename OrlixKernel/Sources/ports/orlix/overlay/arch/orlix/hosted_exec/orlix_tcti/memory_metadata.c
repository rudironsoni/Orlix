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

static unsigned long orlix_tcti_metadata_index(unsigned long address)
{
	return (address & ORLIX_TCTI_UNTAGGED_ADDRESS_MASK) /
		ORLIX_TCTI_TAG_GRANULE;
}

static unsigned long orlix_tcti_metadata_value(void *entry)
{
	return entry ? xa_to_value(entry) : 0;
}

static int orlix_tcti_metadata_store_locked(
			       struct orlix_tcti_memory_metadata *metadata,
				       unsigned long index,
				       unsigned long value, gfp_t gfp)
{
	void *entry;

	if (!value) {
		xa_erase(&metadata->entries, index);
		return 0;
	}
	entry = xa_store(&metadata->entries, index,
			 xa_mk_value(value), gfp);
	return xa_err(entry);
}

int orlix_tcti_set_gcs_memory(struct mm_struct *mm, unsigned long user_va,
			 size_t size, bool enabled)
{
	unsigned long first;
	unsigned long last;
	unsigned long index;
	struct orlix_tcti_memory_metadata *metadata;
	int ret = 0;

	if (!mm || !size || user_va > ULONG_MAX - size)
		return -EINVAL;
	first = orlix_tcti_metadata_index(user_va);
	last = orlix_tcti_metadata_index(user_va + size - 1);
	metadata = orlix_tcti_get_memory_metadata(mm, enabled);
	if (!metadata)
		return enabled ? -ENOMEM : 0;
	mutex_lock(&metadata->lock);
	for (index = first; index <= last; index++) {
		unsigned long value = orlix_tcti_metadata_value(
			xa_load(&metadata->entries, index));

		if (enabled)
			value |= ORLIX_TCTI_METADATA_GCS;
		else
			value &= ~ORLIX_TCTI_METADATA_GCS;
		ret = orlix_tcti_metadata_store_locked(metadata, index, value,
						       GFP_KERNEL);
		if (ret)
			break;
	}
	mutex_unlock(&metadata->lock);
	return ret;
}

int orlix_tcti_write_gcs_user_data(struct mm_struct *mm,
			      unsigned long user_va, const void *buffer,
			      size_t size)
{
	unsigned long first;
	unsigned long last;
	struct orlix_tcti_memory_metadata *metadata;
	int ret;

	if (!mm || !buffer || !size || user_va > ULONG_MAX - size)
		return -EINVAL;
	first = orlix_tcti_metadata_index(user_va);
	last = orlix_tcti_metadata_index(user_va + size - 1);
	metadata = orlix_tcti_get_memory_metadata(mm, false);
	if (!metadata)
		return -EACCES;
	mutex_lock(&metadata->lock);
	if (!(orlix_tcti_metadata_value(xa_load(
		&metadata->entries, first)) &
	      ORLIX_TCTI_METADATA_GCS) ||
	    !(orlix_tcti_metadata_value(xa_load(
		&metadata->entries, last)) &
	      ORLIX_TCTI_METADATA_GCS)) {
		ret = -EACCES;
		goto out;
	}
	ret = orlix_tcti_write_user_data(mm,
		user_va & ORLIX_TCTI_UNTAGGED_ADDRESS_MASK, buffer, size);
out:
	mutex_unlock(&metadata->lock);
	return ret;
}

int orlix_tcti_store_tagged_pair(struct mm_struct *mm,
			    unsigned long tagged_user_va,
			    const void *buffer, size_t size)
{
	unsigned long address = tagged_user_va & ORLIX_TCTI_UNTAGGED_ADDRESS_MASK;
	unsigned long index = orlix_tcti_metadata_index(address);
	unsigned long old_value;
	unsigned long new_value;
	struct orlix_tcti_memory_metadata *metadata;
	int ret;

	if (!mm || !buffer || size != ORLIX_TCTI_TAG_GRANULE ||
	    !IS_ALIGNED(address, ORLIX_TCTI_TAG_GRANULE))
		return -EINVAL;
	metadata = orlix_tcti_get_memory_metadata(mm, true);
	if (!metadata)
		return -ENOMEM;
	mutex_lock(&metadata->lock);
	old_value = orlix_tcti_metadata_value(xa_load(
		&metadata->entries, index));
	ret = xa_reserve(&metadata->entries,
			 index, GFP_KERNEL);
	if (ret)
		goto out;
	ret = orlix_tcti_write_user_data(mm, address, buffer, size);
	if (ret) {
		if (!old_value)
			xa_erase(&metadata->entries, index);
		goto out;
	}
	new_value = (old_value & ORLIX_TCTI_METADATA_GCS) |
		ORLIX_TCTI_METADATA_TAG_VALID |
		FIELD_PREP(ORLIX_TCTI_METADATA_TAG_MASK,
			   (tagged_user_va >> ORLIX_TCTI_LOGICAL_TAG_SHIFT) & 0xfU);
	ret = orlix_tcti_metadata_store_locked(metadata, index, new_value,
					       GFP_NOWAIT);
out:
	mutex_unlock(&metadata->lock);
	return ret;
}

int orlix_tcti_load_allocation_tag(struct mm_struct *mm,
			      unsigned long user_va, u8 *tag)
{
	unsigned long value;
	struct orlix_tcti_memory_metadata *metadata;

	if (!mm || !tag)
		return -EINVAL;
	metadata = orlix_tcti_get_memory_metadata(mm, false);
	if (!metadata)
		return -ENOENT;
	mutex_lock(&metadata->lock);
	value = orlix_tcti_metadata_value(xa_load(
		&metadata->entries,
		orlix_tcti_metadata_index(user_va)));
	mutex_unlock(&metadata->lock);
	if (!(value & ORLIX_TCTI_METADATA_TAG_VALID))
		return -ENOENT;
	*tag = FIELD_GET(ORLIX_TCTI_METADATA_TAG_MASK, value);
	return 0;
}

void orlix_tcti_memory_metadata_destroy(struct mm_struct *mm)
{
	struct orlix_tcti_memory_metadata *metadata;

	if (!mm)
		return;
	metadata = xchg(&mm->context.orlix_tcti_memory_metadata, NULL);
	if (!metadata)
		return;
	xa_destroy(&metadata->entries);
	kfree(metadata);
}
