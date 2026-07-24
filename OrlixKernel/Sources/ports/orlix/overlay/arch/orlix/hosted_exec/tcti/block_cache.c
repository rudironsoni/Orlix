// SPDX-License-Identifier: GPL-2.0-only
#include <linux/errno.h>
#include <linux/hash.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <asm/page.h>

#include "block_cache.h"

static DEFINE_SPINLOCK(tcti_global_cache_lock);
static struct hlist_head tcti_global_block_hash[TCTI_BLOCK_CACHE_BUCKETS];
static LIST_HEAD(tcti_global_blocks);
static atomic64_t tcti_global_code_generation = ATOMIC64_INIT(1);
static u32 tcti_global_block_count;

static u32 tcti_block_hash(struct mm_struct *mm, unsigned long guest_pc,
			   u64 code_generation)
{
	unsigned long key = (unsigned long)mm;

	key ^= guest_pc >> 2;
	key ^= (unsigned long)code_generation << 17;
	key ^= (unsigned long)(code_generation >> 32);
	return hash_long(key, TCTI_BLOCK_CACHE_BUCKET_BITS);
}

static struct tcti_block *tcti_block_find_locked(struct mm_struct *mm,
						 unsigned long guest_pc,
						 u64 code_generation)
{
	struct tcti_block *block;
	u32 bucket = tcti_block_hash(mm, guest_pc, code_generation);

	hlist_for_each_entry(block, &tcti_global_block_hash[bucket], hash_node) {
		if (block->mm == mm &&
		    block->guest_start_pc == guest_pc &&
		    block->code_generation == code_generation)
			return block;
	}

	return NULL;
}

static void tcti_block_free(struct tcti_block *block)
{
	kfree(block);
}

static void tcti_block_cache_evict_oldest_locked(void)
{
	struct tcti_block *block;

	if (list_empty(&tcti_global_blocks))
		return;

	block = list_first_entry(&tcti_global_blocks, struct tcti_block,
				 mm_node);
	hlist_del_init(&block->hash_node);
	list_del_init(&block->mm_node);
	tcti_global_block_count--;
	tcti_block_put(block);
}

u64 tcti_translation_generation(struct mm_struct *mm)
{
	if (!mm)
		return 0;

	return (u64)atomic64_read(&mm->context.orlix_tcti_mapping_sequence);
}

/*
 * Accessors may sleep on the backing page before taking this read lock, but
 * must not acquire a page-table or page lock while holding it. PTE writers
 * enter through tcti_mapping_sequence_begin() with their existing PTE lock.
 */
bool tcti_translation_generation_stable(struct mm_struct *mm,
					u64 generation)
{
	return mm && !(generation & 1) &&
	       generation == tcti_translation_generation(mm);
}

bool tcti_mapping_access_lock(struct mm_struct *mm, u64 generation)
{
	if (!mm)
		return false;

	read_lock(&mm->context.orlix_tcti_mapping_lock);
	if (tcti_translation_generation_stable(mm, generation))
		return true;
	read_unlock(&mm->context.orlix_tcti_mapping_lock);
	return false;
}

void tcti_mapping_access_unlock(struct mm_struct *mm)
{
	if (mm)
		read_unlock(&mm->context.orlix_tcti_mapping_lock);
}

void tcti_mapping_sequence_begin(struct mm_struct *mm)
{
	if (!mm)
		return;

	write_lock(&mm->context.orlix_tcti_mapping_lock);
	atomic64_inc(&mm->context.orlix_tcti_mapping_sequence);
	tcti_bump_code_generation(mm);
}

void tcti_mapping_sequence_end(struct mm_struct *mm)
{
	if (!mm)
		return;

	atomic64_inc(&mm->context.orlix_tcti_mapping_sequence);
	write_unlock(&mm->context.orlix_tcti_mapping_lock);
}

u64 tcti_code_generation(struct mm_struct *mm)
{
	(void)mm;
	return (u64)atomic64_read(&tcti_global_code_generation);
}

void tcti_bump_code_generation(struct mm_struct *mm)
{
	(void)mm;
	atomic64_inc(&tcti_global_code_generation);
}

struct tcti_block *tcti_block_cache_lookup(struct mm_struct *mm,
					   unsigned long guest_pc,
					   u64 code_generation)
{
	struct tcti_block *block;
	unsigned long flags;

	if (!mm)
		return NULL;

	spin_lock_irqsave(&tcti_global_cache_lock, flags);
	block = tcti_block_find_locked(mm, guest_pc, code_generation);
	if (block)
		refcount_inc(&block->refs);
	spin_unlock_irqrestore(&tcti_global_cache_lock, flags);

	return block;
}

int tcti_block_cache_insert(struct mm_struct *mm,
			    unsigned long guest_start_pc,
			    unsigned long guest_end_pc,
			    u64 code_generation,
			    u32 instruction_count,
			    const struct tcti_gadget_word *program,
			    u32 program_words,
			    struct tcti_block **out)
{
	struct tcti_block *existing;
	struct tcti_block *block;
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

	spin_lock_irqsave(&tcti_global_cache_lock, flags);
	if (code_generation != tcti_code_generation(mm)) {
		spin_unlock_irqrestore(&tcti_global_cache_lock, flags);
		tcti_block_free(block);
		return -ESTALE;
	}
	existing = tcti_block_find_locked(mm, guest_start_pc, code_generation);
	if (existing) {
		if (out) {
			refcount_inc(&existing->refs);
			*out = existing;
		}
		spin_unlock_irqrestore(&tcti_global_cache_lock, flags);
		tcti_block_free(block);
		return 0;
	}

	if (tcti_global_block_count >= TCTI_BLOCK_CACHE_MAX_BLOCKS)
		tcti_block_cache_evict_oldest_locked();
	if (tcti_global_block_count >= TCTI_BLOCK_CACHE_MAX_BLOCKS) {
		spin_unlock_irqrestore(&tcti_global_cache_lock, flags);
		tcti_block_free(block);
		return -ENOSPC;
	}

	bucket = tcti_block_hash(mm, guest_start_pc, code_generation);
	hlist_add_head(&block->hash_node, &tcti_global_block_hash[bucket]);
	list_add_tail(&block->mm_node, &tcti_global_blocks);
	tcti_global_block_count++;
	if (out) {
		refcount_inc(&block->refs);
		*out = block;
	}
	spin_unlock_irqrestore(&tcti_global_cache_lock, flags);

	return 0;
}

void tcti_block_put(struct tcti_block *block)
{
	if (block && refcount_dec_and_test(&block->refs))
		tcti_block_free(block);
}

void tcti_block_cache_invalidate_mm(struct mm_struct *mm)
{
	struct tcti_block *block;
	struct tcti_block *tmp;
	unsigned long flags;

	tcti_bump_code_generation(mm);

	spin_lock_irqsave(&tcti_global_cache_lock, flags);
	list_for_each_entry_safe(block, tmp, &tcti_global_blocks, mm_node) {
		if (block->mm != mm)
			continue;
		hlist_del_init(&block->hash_node);
		list_del_init(&block->mm_node);
		tcti_global_block_count--;
		tcti_block_put(block);
	}
	spin_unlock_irqrestore(&tcti_global_cache_lock, flags);
}

void tcti_block_cache_invalidate_range(struct mm_struct *mm,
				       unsigned long start, unsigned long end)
{
	struct tcti_block *block;
	struct tcti_block *tmp;
	unsigned long flags;

	if (!mm || end <= start)
		return;
	tcti_bump_code_generation(mm);
	spin_lock_irqsave(&tcti_global_cache_lock, flags);
	list_for_each_entry_safe(block, tmp, &tcti_global_blocks, mm_node) {
		if (block->mm != mm || block->guest_end_pc <= start ||
		    block->guest_start_pc >= end)
			continue;
		hlist_del_init(&block->hash_node);
		list_del_init(&block->mm_node);
		tcti_global_block_count--;
		tcti_block_put(block);
	}
	spin_unlock_irqrestore(&tcti_global_cache_lock, flags);
}

void tcti_block_cache_invalidate_all(void)
{
	struct tcti_block *block;
	struct tcti_block *tmp;
	unsigned long flags;

	tcti_bump_code_generation(NULL);

	spin_lock_irqsave(&tcti_global_cache_lock, flags);
	list_for_each_entry_safe(block, tmp, &tcti_global_blocks, mm_node) {
		hlist_del_init(&block->hash_node);
		list_del_init(&block->mm_node);
		tcti_global_block_count--;
		tcti_block_put(block);
	}
	spin_unlock_irqrestore(&tcti_global_cache_lock, flags);
}

#if IS_ENABLED(CONFIG_ORLIX_TCTI_KUNIT_TEST)
void tcti_block_cache_reset_for_tests(void)
{
	struct tcti_block *block;
	struct tcti_block *tmp;
	unsigned long flags;

	spin_lock_irqsave(&tcti_global_cache_lock, flags);
	list_for_each_entry_safe(block, tmp, &tcti_global_blocks, mm_node) {
		hlist_del_init(&block->hash_node);
		list_del_init(&block->mm_node);
		tcti_global_block_count--;
		tcti_block_put(block);
	}
	atomic64_set(&tcti_global_code_generation, 1);
	spin_unlock_irqrestore(&tcti_global_cache_lock, flags);
}

void tcti_block_cache_set_generation_for_tests(u64 generation)
{
	atomic64_set(&tcti_global_code_generation, generation);
}

u32 tcti_block_cache_count_for_tests(void)
{
	u32 count;
	unsigned long flags;

	spin_lock_irqsave(&tcti_global_cache_lock, flags);
	count = tcti_global_block_count;
	spin_unlock_irqrestore(&tcti_global_cache_lock, flags);

	return count;
}
#endif
