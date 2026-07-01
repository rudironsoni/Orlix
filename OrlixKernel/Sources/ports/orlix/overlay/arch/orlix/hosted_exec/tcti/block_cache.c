// SPDX-License-Identifier: GPL-2.0-only
#include <linux/errno.h>
#include <linux/hash.h>
#include <linux/slab.h>
#include <linux/string.h>

#include "block_cache.h"

static DEFINE_SPINLOCK(tcti_global_cache_lock);
static struct hlist_head tcti_global_block_hash[TCTI_BLOCK_CACHE_BUCKETS];
static LIST_HEAD(tcti_global_blocks);
static atomic_t tcti_global_translation_generation = ATOMIC_INIT(1);
static atomic_t tcti_global_code_generation = ATOMIC_INIT(1);
static u32 tcti_global_block_count;

static u32 tcti_block_hash(struct mm_struct *mm, unsigned long guest_pc,
			   u32 code_generation)
{
	unsigned long key = (unsigned long)mm;

	key ^= guest_pc >> 2;
	key ^= (unsigned long)code_generation << 17;
	return hash_long(key, TCTI_BLOCK_CACHE_BUCKET_BITS);
}

static struct tcti_block *tcti_block_find_locked(struct mm_struct *mm,
						 unsigned long guest_pc,
						 u32 code_generation)
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
	kfree(block->pages);
	kfree(block);
}

u32 tcti_translation_generation(struct mm_struct *mm)
{
	(void)mm;
	return (u32)atomic_read(&tcti_global_translation_generation);
}

u32 tcti_code_generation(struct mm_struct *mm)
{
	(void)mm;
	return (u32)atomic_read(&tcti_global_code_generation);
}

void tcti_bump_translation_generation(struct mm_struct *mm)
{
	(void)mm;
	atomic_inc(&tcti_global_translation_generation);
}

void tcti_bump_code_generation(struct mm_struct *mm)
{
	(void)mm;
	atomic_inc(&tcti_global_code_generation);
}

struct tcti_block *tcti_block_cache_lookup(struct mm_struct *mm,
					   unsigned long guest_pc,
					   u32 code_generation)
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
			    u32 code_generation,
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
	INIT_HLIST_HEAD(&block->incoming_patch_slots);
	memcpy(block->program, program, program_words * sizeof(*program));

	spin_lock_irqsave(&tcti_global_cache_lock, flags);
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

	tcti_bump_translation_generation(mm);
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
	atomic_set(&tcti_global_translation_generation, 1);
	atomic_set(&tcti_global_code_generation, 1);
	spin_unlock_irqrestore(&tcti_global_cache_lock, flags);
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
