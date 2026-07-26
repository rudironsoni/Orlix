// SPDX-License-Identifier: GPL-2.0-only
#include <linux/errno.h>
#include <linux/hash.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <asm/page.h>

#include "block_cache.h"

static DEFINE_SPINLOCK(orlix_tcti_global_cache_lock);
static struct hlist_head orlix_tcti_global_block_hash[ORLIX_TCTI_BLOCK_CACHE_BUCKETS];
static LIST_HEAD(orlix_tcti_global_blocks);
static atomic64_t orlix_tcti_global_code_generation = ATOMIC64_INIT(1);
static u32 orlix_tcti_global_block_count;

static u32 orlix_tcti_block_hash(struct mm_struct *mm, unsigned long guest_pc,
			   u64 code_generation)
{
	unsigned long key = (unsigned long)mm;

	key ^= guest_pc >> 2;
	key ^= (unsigned long)code_generation << 17;
	key ^= (unsigned long)(code_generation >> 32);
	return hash_long(key, ORLIX_TCTI_BLOCK_CACHE_BUCKET_BITS);
}

static struct orlix_tcti_block *orlix_tcti_block_find_locked(struct mm_struct *mm,
						 unsigned long guest_pc,
						 u64 code_generation)
{
	struct orlix_tcti_block *block;
	u32 bucket = orlix_tcti_block_hash(mm, guest_pc, code_generation);

	hlist_for_each_entry(block, &orlix_tcti_global_block_hash[bucket], hash_node) {
		if (block->mm == mm &&
		    block->guest_start_pc == guest_pc &&
		    block->code_generation == code_generation)
			return block;
	}

	return NULL;
}

static void orlix_tcti_block_free(struct orlix_tcti_block *block)
{
	kfree(block);
}

static void orlix_tcti_block_cache_evict_oldest_locked(void)
{
	struct orlix_tcti_block *block;

	if (list_empty(&orlix_tcti_global_blocks))
		return;

	block = list_first_entry(&orlix_tcti_global_blocks, struct orlix_tcti_block,
				 mm_node);
	hlist_del_init(&block->hash_node);
	list_del_init(&block->mm_node);
	orlix_tcti_global_block_count--;
	orlix_tcti_block_put(block);
}

u64 orlix_tcti_translation_generation(struct mm_struct *mm)
{
	if (!mm)
		return 0;

	return (u64)atomic64_read(&mm->context.orlix_tcti_mapping_sequence);
}

/*
 * Accessors may sleep on the backing page before taking this read lock, but
 * must not acquire a page-table or page lock while holding it. PTE writers
 * enter through orlix_tcti_mapping_sequence_begin() with their existing PTE lock.
 */
bool orlix_tcti_translation_generation_stable(struct mm_struct *mm,
					u64 generation)
{
	return mm && !(generation & 1) &&
	       generation == orlix_tcti_translation_generation(mm);
}

bool orlix_tcti_mapping_access_lock(struct mm_struct *mm, u64 generation)
{
	if (!mm)
		return false;

	read_lock(&mm->context.orlix_tcti_mapping_lock);
	if (orlix_tcti_translation_generation_stable(mm, generation))
		return true;
	read_unlock(&mm->context.orlix_tcti_mapping_lock);
	return false;
}

void orlix_tcti_mapping_access_unlock(struct mm_struct *mm)
{
	if (mm)
		read_unlock(&mm->context.orlix_tcti_mapping_lock);
}

void orlix_tcti_mapping_sequence_begin(struct mm_struct *mm)
{
	if (!mm)
		return;

	write_lock(&mm->context.orlix_tcti_mapping_lock);
	atomic64_inc(&mm->context.orlix_tcti_mapping_sequence);
	orlix_tcti_bump_code_generation(mm);
}

void orlix_tcti_mapping_sequence_end(struct mm_struct *mm)
{
	if (!mm)
		return;

	atomic64_inc(&mm->context.orlix_tcti_mapping_sequence);
	write_unlock(&mm->context.orlix_tcti_mapping_lock);
}

u64 orlix_tcti_code_generation(struct mm_struct *mm)
{
	(void)mm;
	return (u64)atomic64_read(&orlix_tcti_global_code_generation);
}

void orlix_tcti_bump_code_generation(struct mm_struct *mm)
{
	(void)mm;
	atomic64_inc(&orlix_tcti_global_code_generation);
}

struct orlix_tcti_block *orlix_tcti_block_cache_lookup(struct mm_struct *mm,
					   unsigned long guest_pc,
					   u64 code_generation)
{
	struct orlix_tcti_block *block;
	unsigned long flags;

	if (!mm)
		return NULL;

	spin_lock_irqsave(&orlix_tcti_global_cache_lock, flags);
	block = orlix_tcti_block_find_locked(mm, guest_pc, code_generation);
	if (block)
		refcount_inc(&block->refs);
	spin_unlock_irqrestore(&orlix_tcti_global_cache_lock, flags);

	return block;
}

int orlix_tcti_block_cache_insert(struct mm_struct *mm,
			    unsigned long guest_start_pc,
			    unsigned long guest_end_pc,
			    u64 code_generation,
			    u32 instruction_count,
			    const struct orlix_tcti_gadget_word *program,
			    u32 program_words,
			    struct orlix_tcti_block **out)
{
	struct orlix_tcti_block *existing;
	struct orlix_tcti_block *block;
	size_t size;
	unsigned long flags;
	u32 bucket;

	if (out)
		*out = NULL;
	if (!mm || !program || !program_words || !instruction_count)
		return -EINVAL;
	if (guest_end_pc < guest_start_pc)
		return -EINVAL;

	size = struct_size(block, program, program_words);
	block = kzalloc(size, GFP_KERNEL);
	if (!block)
		return -ENOMEM;

	refcount_set(&block->refs, 1);
	block->mm = mm;
	block->guest_start_pc = guest_start_pc;
	block->guest_end_pc = guest_end_pc;
	block->code_generation = code_generation;
	block->instruction_count = instruction_count;
	block->program_words = program_words;
	memcpy(block->program, program, program_words * sizeof(*program));

	spin_lock_irqsave(&orlix_tcti_global_cache_lock, flags);
	if (code_generation != orlix_tcti_code_generation(mm)) {
		spin_unlock_irqrestore(&orlix_tcti_global_cache_lock, flags);
		orlix_tcti_block_free(block);
		return -ESTALE;
	}
	existing = orlix_tcti_block_find_locked(mm, guest_start_pc, code_generation);
	if (existing) {
		if (out) {
			refcount_inc(&existing->refs);
			*out = existing;
		}
		spin_unlock_irqrestore(&orlix_tcti_global_cache_lock, flags);
		orlix_tcti_block_free(block);
		return 0;
	}

	if (orlix_tcti_global_block_count >= ORLIX_TCTI_BLOCK_CACHE_MAX_BLOCKS)
		orlix_tcti_block_cache_evict_oldest_locked();
	if (orlix_tcti_global_block_count >= ORLIX_TCTI_BLOCK_CACHE_MAX_BLOCKS) {
		spin_unlock_irqrestore(&orlix_tcti_global_cache_lock, flags);
		orlix_tcti_block_free(block);
		return -ENOSPC;
	}

	bucket = orlix_tcti_block_hash(mm, guest_start_pc, code_generation);
	hlist_add_head(&block->hash_node, &orlix_tcti_global_block_hash[bucket]);
	list_add_tail(&block->mm_node, &orlix_tcti_global_blocks);
	orlix_tcti_global_block_count++;
	if (out) {
		refcount_inc(&block->refs);
		*out = block;
	}
	spin_unlock_irqrestore(&orlix_tcti_global_cache_lock, flags);

	return 0;
}

void orlix_tcti_block_put(struct orlix_tcti_block *block)
{
	if (block && refcount_dec_and_test(&block->refs))
		orlix_tcti_block_free(block);
}

void orlix_tcti_block_cache_invalidate_mm(struct mm_struct *mm)
{
	struct orlix_tcti_block *block;
	struct orlix_tcti_block *tmp;
	unsigned long flags;

	orlix_tcti_bump_code_generation(mm);

	spin_lock_irqsave(&orlix_tcti_global_cache_lock, flags);
	list_for_each_entry_safe(block, tmp, &orlix_tcti_global_blocks, mm_node) {
		if (block->mm != mm)
			continue;
		hlist_del_init(&block->hash_node);
		list_del_init(&block->mm_node);
		orlix_tcti_global_block_count--;
		orlix_tcti_block_put(block);
	}
	spin_unlock_irqrestore(&orlix_tcti_global_cache_lock, flags);
}

void orlix_tcti_block_cache_invalidate_range(struct mm_struct *mm,
				       unsigned long start, unsigned long end)
{
	struct orlix_tcti_block *block;
	struct orlix_tcti_block *tmp;
	unsigned long flags;

	if (!mm || end <= start)
		return;
	orlix_tcti_bump_code_generation(mm);
	spin_lock_irqsave(&orlix_tcti_global_cache_lock, flags);
	list_for_each_entry_safe(block, tmp, &orlix_tcti_global_blocks, mm_node) {
		if (block->mm != mm || block->guest_end_pc <= start ||
		    block->guest_start_pc >= end)
			continue;
		hlist_del_init(&block->hash_node);
		list_del_init(&block->mm_node);
		orlix_tcti_global_block_count--;
		orlix_tcti_block_put(block);
	}
	spin_unlock_irqrestore(&orlix_tcti_global_cache_lock, flags);
}

void orlix_tcti_block_cache_invalidate_all(void)
{
	struct orlix_tcti_block *block;
	struct orlix_tcti_block *tmp;
	unsigned long flags;

	orlix_tcti_bump_code_generation(NULL);

	spin_lock_irqsave(&orlix_tcti_global_cache_lock, flags);
	list_for_each_entry_safe(block, tmp, &orlix_tcti_global_blocks, mm_node) {
		hlist_del_init(&block->hash_node);
		list_del_init(&block->mm_node);
		orlix_tcti_global_block_count--;
		orlix_tcti_block_put(block);
	}
	spin_unlock_irqrestore(&orlix_tcti_global_cache_lock, flags);
}

#if IS_ENABLED(CONFIG_ORLIX_TCTI_KUNIT_TEST)
void orlix_tcti_block_cache_reset_for_tests(void)
{
	struct orlix_tcti_block *block;
	struct orlix_tcti_block *tmp;
	unsigned long flags;

	spin_lock_irqsave(&orlix_tcti_global_cache_lock, flags);
	list_for_each_entry_safe(block, tmp, &orlix_tcti_global_blocks, mm_node) {
		hlist_del_init(&block->hash_node);
		list_del_init(&block->mm_node);
		orlix_tcti_global_block_count--;
		orlix_tcti_block_put(block);
	}
	atomic64_set(&orlix_tcti_global_code_generation, 1);
	spin_unlock_irqrestore(&orlix_tcti_global_cache_lock, flags);
}

void orlix_tcti_block_cache_set_generation_for_tests(u64 generation)
{
	atomic64_set(&orlix_tcti_global_code_generation, generation);
}

u32 orlix_tcti_block_cache_count_for_tests(void)
{
	u32 count;
	unsigned long flags;

	spin_lock_irqsave(&orlix_tcti_global_cache_lock, flags);
	count = orlix_tcti_global_block_count;
	spin_unlock_irqrestore(&orlix_tcti_global_cache_lock, flags);

	return count;
}
#endif
