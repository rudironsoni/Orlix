/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_BLOCK_CACHE_H
#define ORLIX_TCTI_BLOCK_CACHE_H

#include <linux/list.h>
#include <linux/rcupdate.h>
#include <linux/refcount.h>
#include <linux/spinlock.h>
#include <linux/types.h>

#include "gadget_program.h"

#define TCTI_BLOCK_CACHE_BUCKET_BITS 12U
#define TCTI_BLOCK_CACHE_BUCKETS (1U << TCTI_BLOCK_CACHE_BUCKET_BITS)
#define TCTI_PAGE_INDEX_BUCKETS 4096U
#define TCTI_DIRECT_PATCH_SLOTS 2U
#define TCTI_BLOCK_CACHE_MAX_BLOCKS 4096U

struct mm_struct;
struct tcti_block;

struct tcti_patch_slot {
	struct tcti_block *source;
	unsigned long *slot;
	struct hlist_node target_link;
};

struct tcti_page_ref {
	unsigned long guest_page;
	struct hlist_node block_page_link;
	struct hlist_node page_index_link;
};

struct tcti_block {
	struct hlist_node hash_node;
	struct list_head mm_node;
	struct rcu_head rcu;
	refcount_t refs;
	struct mm_struct *mm;
	unsigned long guest_start_pc;
	unsigned long guest_end_pc;
	u32 code_generation;
	u32 instruction_count;
	u32 program_words;
	u32 page_count;
	unsigned long direct_target_pc[TCTI_DIRECT_PATCH_SLOTS];
	unsigned long *direct_jump_patch_slots[TCTI_DIRECT_PATCH_SLOTS];
	struct hlist_head incoming_patch_slots;
	struct tcti_page_ref *pages;
	struct tcti_gadget_word program[];
};

struct tcti_mm_cache {
	spinlock_t lock;
	atomic_t translation_generation;
	atomic_t code_generation;
	struct hlist_head block_hash[TCTI_BLOCK_CACHE_BUCKETS];
	struct hlist_head page_index[TCTI_PAGE_INDEX_BUCKETS];
	struct list_head blocks;
};

u32 tcti_translation_generation(struct mm_struct *mm);
u32 tcti_code_generation(struct mm_struct *mm);
void tcti_bump_translation_generation(struct mm_struct *mm);
void tcti_bump_code_generation(struct mm_struct *mm);

struct tcti_block *tcti_block_cache_lookup(struct mm_struct *mm,
					   unsigned long guest_pc,
					   u32 code_generation);
int tcti_block_cache_insert(struct mm_struct *mm,
			    unsigned long guest_start_pc,
			    unsigned long guest_end_pc,
			    u32 code_generation,
			    u32 instruction_count,
			    const struct tcti_gadget_word *program,
			    u32 program_words,
			    struct tcti_block **out);
void tcti_block_put(struct tcti_block *block);
void tcti_block_cache_invalidate_mm(struct mm_struct *mm);

#if IS_ENABLED(CONFIG_ORLIX_TCTI_KUNIT_TEST)
void tcti_block_cache_reset_for_tests(void);
u32 tcti_block_cache_count_for_tests(void);
#endif

#endif /* ORLIX_TCTI_BLOCK_CACHE_H */
