// SPDX-License-Identifier: GPL-2.0-only
/*
 * Allocation tags are physical-page metadata.  The xarray key is only a live
 * page lookup key: entries never retain a struct page reference and allocator
 * hooks erase them before a PFN may be reused.
 */
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/prctl.h>
#include <linux/rcupdate.h>
#include <linux/refcount.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/xarray.h>

#include <asm/mman.h>
#include <asm/mte.h>
#include <asm/orlix_tcti.h>

#define ORLIX_MTE_TAGS_BYTES (PAGE_SIZE / ORLIX_MTE_GRANULE_SIZE / 2)

struct orlix_mte_page_tags {
	refcount_t refs;
	spinlock_t lock;
	struct rcu_head rcu;
	u8 tags[ORLIX_MTE_TAGS_BYTES];
};

static DEFINE_XARRAY(orlix_mte_pages);
/*
 * A tag instruction observes one architectural transaction.  In particular,
 * ST2G/STZ2G must not expose the first granule while another CPU can still
 * observe the second old granule.  This is deliberately separate from the
 * page-index lock: the latter protects object lifetime, this one serialises
 * tag readers, writers, copies, and multi-page transactions.
 */
static DEFINE_MUTEX(orlix_mte_transaction_lock);

static unsigned long orlix_mte_page_key(const struct page *page)
{
	return page_to_pfn(page);
}

static unsigned int orlix_mte_tag_index(unsigned long address)
{
	return offset_in_page(address) / ORLIX_MTE_GRANULE_SIZE;
}

static bool orlix_mte_vma_enabled(struct mm_struct *mm, unsigned long address,
				  bool write)
{
	struct vm_area_struct *vma;
	bool enabled = false;

	address = orlix_mte_untagged_address(address);
	mmap_read_lock(mm);
	vma = find_vma(mm, address);
	if (vma && address >= vma->vm_start && address < vma->vm_end &&
	    (vma->vm_flags & VM_MTE) && (!write || (vma->vm_flags & VM_WRITE)))
		enabled = true;
	mmap_read_unlock(mm);
	return enabled;
}

static void orlix_mte_tags_rcu_free(struct rcu_head *rcu)
{
	struct orlix_mte_page_tags *tags =
		container_of(rcu, struct orlix_mte_page_tags, rcu);

	kfree(tags);
}

static void orlix_mte_tags_put(struct orlix_mte_page_tags *tags)
{
	if (tags && refcount_dec_and_test(&tags->refs))
		call_rcu(&tags->rcu, orlix_mte_tags_rcu_free);
}

/*
 * The xarray owns one ref. Readers acquire another while RCU prevents an
 * erased entry from being freed between xa_load() and refcount_inc_not_zero().
 * arch_free_page() only erases and drops the xarray ref, so it is safe from
 * allocator atomic context and never frees an object still in use.
 */
static struct orlix_mte_page_tags *orlix_mte_tags_lookup(struct page *page)
{
	struct orlix_mte_page_tags *tags;

	rcu_read_lock();
	tags = xa_load(&orlix_mte_pages, orlix_mte_page_key(page));
	if (tags && !refcount_inc_not_zero(&tags->refs))
		tags = NULL;
	rcu_read_unlock();
	return tags;
}

static struct orlix_mte_page_tags *
orlix_mte_tags_get(struct page *page, bool create)
{
	struct orlix_mte_page_tags *tags, *new_tags;
	void *old;

	tags = orlix_mte_tags_lookup(page);
	if (tags || !create)
		return tags;

	for (;;) {
		new_tags = kzalloc(sizeof(*new_tags), GFP_KERNEL);
		if (!new_tags)
			return ERR_PTR(-ENOMEM);
		refcount_set(&new_tags->refs, 1);
		spin_lock_init(&new_tags->lock);
		old = xa_cmpxchg(&orlix_mte_pages, orlix_mte_page_key(page), NULL,
					 new_tags, GFP_KERNEL);
		if (xa_is_err(old)) {
			kfree(new_tags);
			return ERR_PTR(xa_err(old));
		}
		if (!old) {
			/* Keep the caller ref distinct from the xarray ownership ref. */
			refcount_inc(&new_tags->refs);
			smp_wmb();
			return new_tags;
		}
		kfree(new_tags);
		tags = orlix_mte_tags_lookup(page);
		if (tags)
			return tags;
	}
}

static u8 orlix_mte_tag_get(const struct orlix_mte_page_tags *tags,
			    unsigned int index)
{
	u8 value = READ_ONCE(tags->tags[index >> 1]);

	return index & 1 ? value >> 4 : value & 0xf;
}

static void orlix_mte_tag_set(struct orlix_mte_page_tags *tags,
			      unsigned int index, u8 tag)
{
	u8 *value = &tags->tags[index >> 1];

	if (index & 1)
		*value = (*value & 0x0f) | (tag << 4);
	else
		*value = (*value & 0xf0) | tag;
}

int orlix_mte_init_mm(struct mm_struct *mm)
{
	return mm ? 0 : -EINVAL;
}

void orlix_mte_destroy_mm(struct mm_struct *mm)
{
	/* Tags are page-owned, so fork, exit, and unmap must not reclaim them. */
	(void)mm;
}

void orlix_mte_clear_page_tags(struct page *page)
{
	struct orlix_mte_page_tags *tags = orlix_mte_tags_get(page, true);

	if (IS_ERR(tags))
		return;
	mutex_lock(&orlix_mte_transaction_lock);
	spin_lock(&tags->lock);
	memset(tags->tags, 0, sizeof(tags->tags));
	/* Readers observe a complete zeroed page, never a partially reset page. */
	smp_wmb();
	spin_unlock(&tags->lock);
	mutex_unlock(&orlix_mte_transaction_lock);
	orlix_mte_tags_put(tags);
}

void orlix_mte_sync_page_tags(struct page *page)
{
	struct orlix_mte_page_tags *tags;

	/* First tagged PTE exposure creates an all-zero allocation-tag page. */
	tags = orlix_mte_tags_get(page, true);
	if (!IS_ERR(tags))
		orlix_mte_tags_put(tags);
}

void orlix_mte_copy_page_tags(struct page *to, struct page *from)
{
	struct orlix_mte_page_tags *source, *destination;

	mutex_lock(&orlix_mte_transaction_lock);
	source = orlix_mte_tags_get(from, false);
	if (!source) {
		destination = orlix_mte_tags_get(to, true);
		if (!IS_ERR(destination)) {
			spin_lock(&destination->lock);
			memset(destination->tags, 0, sizeof(destination->tags));
			smp_wmb();
			spin_unlock(&destination->lock);
			orlix_mte_tags_put(destination);
		}
		mutex_unlock(&orlix_mte_transaction_lock);
		if (source)
			orlix_mte_tags_put(source);
		return;
	}
	if (IS_ERR(source)) {
		mutex_unlock(&orlix_mte_transaction_lock);
		return;
	}
	destination = orlix_mte_tags_get(to, true);
	if (IS_ERR(destination)) {
		mutex_unlock(&orlix_mte_transaction_lock);
		orlix_mte_tags_put(source);
		return;
	}
	spin_lock(&source->lock);
	spin_lock(&destination->lock);
	memcpy(destination->tags, source->tags, sizeof(destination->tags));
	smp_wmb();
	spin_unlock(&destination->lock);
	spin_unlock(&source->lock);
	mutex_unlock(&orlix_mte_transaction_lock);
	orlix_mte_tags_put(destination);
	orlix_mte_tags_put(source);
}

void arch_alloc_page(struct page *page, int order)
{
	unsigned int i;

	/* A stale entry is an allocator-integrity failure; erase it fail closed. */
	for (i = 0; i < (1U << order); i++)
		arch_free_page(page + i, 0);
}

void arch_free_page(struct page *page, int order)
{
	unsigned int i;

	for (i = 0; i < (1U << order); i++) {
		struct orlix_mte_page_tags *tags;

		tags = xa_erase(&orlix_mte_pages, orlix_mte_page_key(page + i));
		orlix_mte_tags_put(tags);
	}
}

int orlix_mte_load_allocation_tag(struct mm_struct *mm, unsigned long address,
				  u8 *tag)
{
	return orlix_mte_load_allocation_tags(mm, address, tag, 1);
}

/*
 * Read one architectural tag transaction.  Pair consumers use this instead
 * of two independent loads, so a concurrent ST2G/STZ2G can never be observed
 * as one old and one new allocation tag.
 */
int orlix_mte_load_allocation_tags(struct mm_struct *mm, unsigned long address,
				   u8 *values, unsigned int granules)
{
	struct orlix_tcti_user_page page;
	struct orlix_tcti_user_page next_page;
	struct orlix_mte_page_tags *tags;
	struct orlix_mte_page_tags *next_tags = NULL;
	unsigned long next_address;
	bool next_locked = false;
	int ret;

	address = orlix_mte_untagged_address(address);
	if (!mm || !values || !granules || granules > 2 ||
	    !orlix_mte_vma_enabled(mm, address, false) ||
	    !orlix_mte_vma_enabled(mm, address +
			(granules - 1) * ORLIX_MTE_GRANULE_SIZE, false))
		return -EACCES;
	ret = orlix_tcti_pin_user_page_faulting(mm, address,
					ORLIX_TCTI_ACCESS_READ, &page);
	if (ret)
		return ret;
	memset(&next_page, 0, sizeof(next_page));
	next_address = address + ORLIX_MTE_GRANULE_SIZE;
	if (granules == 2) {
		ret = orlix_tcti_pin_user_page_faulting(mm, next_address,
						ORLIX_TCTI_ACCESS_READ, &next_page);
		if (ret)
			goto out_page;
	}

	mutex_lock(&orlix_mte_transaction_lock);
	tags = orlix_mte_tags_get(page.page, false);
	if (IS_ERR(tags))
		ret = PTR_ERR(tags);
	else {
		if (next_page.page && next_page.page != page.page) {
			next_tags = orlix_mte_tags_get(next_page.page, false);
			if (IS_ERR(next_tags)) {
				ret = PTR_ERR(next_tags);
				goto out_unlock;
			}
			if (tags && next_tags) {
				if (page_to_pfn(next_page.page) < page_to_pfn(page.page)) {
					spin_lock(&next_tags->lock);
					spin_lock(&tags->lock);
				} else {
					spin_lock(&tags->lock);
					spin_lock(&next_tags->lock);
				}
				next_locked = true;
			} else if (tags) {
				spin_lock(&tags->lock);
			} else if (next_tags) {
				spin_lock(&next_tags->lock);
			}
		} else if (tags) {
			spin_lock(&tags->lock);
		}

		values[0] = tags ? orlix_mte_tag_get(tags,
			orlix_mte_tag_index(address)) : 0;
		if (granules == 2)
			values[1] = next_page.page && next_page.page != page.page ?
				(next_tags ? orlix_mte_tag_get(next_tags,
					orlix_mte_tag_index(next_address)) : 0) :
				(tags ? orlix_mte_tag_get(tags,
					orlix_mte_tag_index(next_address)) : 0);
		if (next_locked)
			spin_unlock(&next_tags->lock);
		if (tags)
			spin_unlock(&tags->lock);
		else if (next_tags)
			spin_unlock(&next_tags->lock);
		ret = 0;
	}

out_unlock:
	mutex_unlock(&orlix_mte_transaction_lock);
	if (!IS_ERR_OR_NULL(next_tags))
		orlix_mte_tags_put(next_tags);
	if (!IS_ERR_OR_NULL(tags))
		orlix_mte_tags_put(tags);
	if (next_page.page)
		orlix_tcti_unpin_user_page(&next_page);
out_page:
	orlix_tcti_unpin_user_page(&page);
	return ret;
}

int orlix_mte_store_allocation_tag(struct mm_struct *mm, unsigned long address,
				   u8 tag)
{
	return orlix_mte_store_allocation_tags(mm, address, tag, 1);
}

/*
 * ST2G/STZ2G publish both allocation tags as one ordered transaction.  The
 * caller supplies only aligned architectural granules; pinning retains both
 * physical pages while the page-tag locks are taken in PFN order, preventing
 * both partial observations and cross-page lock inversion.
 */
int orlix_mte_store_allocation_tags(struct mm_struct *mm, unsigned long address,
				    u8 tag, unsigned int granules)
{
	struct orlix_tcti_user_page page;
	struct orlix_tcti_user_page next_page;
	struct orlix_mte_page_tags *tags;
	struct orlix_mte_page_tags *next_tags = NULL;
	unsigned long next_address;
	bool next_locked = false;
	int ret;

	address = orlix_mte_untagged_address(address);
	if (!mm || !granules || granules > 2 ||
	    !orlix_mte_vma_enabled(mm, address, true) ||
	    !orlix_mte_vma_enabled(mm, address +
			(granules - 1) * ORLIX_MTE_GRANULE_SIZE, true))
		return -EACCES;
	ret = orlix_tcti_pin_user_page_faulting(mm, address,
					ORLIX_TCTI_ACCESS_WRITE, &page);
	if (ret)
		return ret;
	memset(&next_page, 0, sizeof(next_page));
	next_address = address + ORLIX_MTE_GRANULE_SIZE;
	if (granules == 2) {
		/* Pin the second granule even when it crosses a physical page. */
		ret = orlix_tcti_pin_user_page_faulting(mm, next_address,
						ORLIX_TCTI_ACCESS_WRITE, &next_page);
		if (ret)
			goto out_page;
	}
	mutex_lock(&orlix_mte_transaction_lock);
	tags = orlix_mte_tags_get(page.page, true);
	if (IS_ERR(tags))
		ret = PTR_ERR(tags);
	else {
		if (next_page.page && next_page.page != page.page) {
			next_tags = orlix_mte_tags_get(next_page.page, true);
			if (IS_ERR(next_tags)) {
				ret = PTR_ERR(next_tags);
				goto out_page;
			}
			if (page_to_pfn(next_page.page) < page_to_pfn(page.page)) {
				spin_lock(&next_tags->lock);
				spin_lock(&tags->lock);
			} else {
				spin_lock(&tags->lock);
				spin_lock(&next_tags->lock);
			}
			next_locked = true;
		} else {
			spin_lock(&tags->lock);
		}
		/* Tag writes publish after every preceding STZ* data write. */
		smp_wmb();
		orlix_mte_tag_set(tags, orlix_mte_tag_index(address), tag & 0xf);
		if (granules == 2) {
			if (next_page.page && next_page.page != page.page)
				orlix_mte_tag_set(next_tags,
					orlix_mte_tag_index(next_address), tag & 0xf);
			else
				orlix_mte_tag_set(tags,
					orlix_mte_tag_index(next_address), tag & 0xf);
		}
		smp_wmb();
		if (next_locked)
			spin_unlock(&next_tags->lock);
		spin_unlock(&tags->lock);
		ret = 0;
	}
out_page:
	mutex_unlock(&orlix_mte_transaction_lock);
	if (!IS_ERR_OR_NULL(next_tags))
		orlix_mte_tags_put(next_tags);
	if (!IS_ERR_OR_NULL(tags))
		orlix_mte_tags_put(tags);
	if (next_page.page)
		orlix_tcti_unpin_user_page(&next_page);
	orlix_tcti_unpin_user_page(&page);
	return ret;
}

int orlix_mte_check_access(struct mm_struct *mm, unsigned long address,
			   size_t size, bool write)
{
	u8 allocation_tags[2];
	unsigned long untagged, first_granule, last_granule;
	unsigned int granules, i;
	u8 logical_tag;
	int ret;

	if (!size)
		return 0;
	untagged = orlix_mte_untagged_address(address);
	if (untagged > ULONG_MAX - (size - 1))
		return -EACCES;
	first_granule = untagged & ~(ORLIX_MTE_GRANULE_SIZE - 1);
	last_granule = (untagged + size - 1) &
		~(ORLIX_MTE_GRANULE_SIZE - 1);
	granules = (last_granule - first_granule) / ORLIX_MTE_GRANULE_SIZE + 1;
	/* Current ordinary, exclusive, and LSE accesses are at most 16 bytes. */
	if (granules > ARRAY_SIZE(allocation_tags))
		return -EACCES;
	/* Untagged mappings retain normal Linux access semantics.  Once an access
	 * starts in VM_MTE, however, every byte must remain in an eligible VMA. */
	if (!mm || !orlix_mte_vma_enabled(mm, untagged, write))
		return 0;
	if (!orlix_mte_vma_enabled(mm, untagged + size - 1, write))
		return -EACCES;
	/* PR_SET_TAGGED_ADDR_CTRL selects checking per task, never per-mm. */
	if (!(current->thread.user_mte_ctrl & PR_TAGGED_ADDR_ENABLE) ||
	    !(current->thread.user_mte_ctrl & PR_MTE_TCF_SYNC))
		return 0;
	ret = orlix_mte_load_allocation_tags(mm, first_granule,
					     allocation_tags, granules);
	if (ret)
		return ret;
	logical_tag = (address & ORLIX_MTE_TAG_MASK) >> ORLIX_MTE_TAG_SHIFT;
	for (i = 0; i < granules; i++)
		if (allocation_tags[i] != logical_tag)
			return -EHWPOISON;
	return 0;
}
