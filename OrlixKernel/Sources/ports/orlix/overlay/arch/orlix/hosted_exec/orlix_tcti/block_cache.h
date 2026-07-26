/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_BLOCK_CACHE_H
#define ORLIX_TCTI_BLOCK_CACHE_H

#include <linux/list.h>
#include <linux/rcupdate.h>
#include <linux/refcount.h>
#include <linux/spinlock.h>
#include <linux/types.h>

#include "gadget_program.h"

#define ORLIX_TCTI_BLOCK_CACHE_BUCKET_BITS 12U
#define ORLIX_TCTI_BLOCK_CACHE_BUCKETS (1U << ORLIX_TCTI_BLOCK_CACHE_BUCKET_BITS)
#define ORLIX_TCTI_BLOCK_CACHE_MAX_BLOCKS 4096U
struct mm_struct;
struct orlix_tcti_block;

struct orlix_tcti_block {
	struct hlist_node hash_node;
	struct list_head mm_node;
	struct rcu_head rcu;
	refcount_t refs;
	struct mm_struct *mm;
	unsigned long guest_start_pc;
	unsigned long guest_end_pc;
	u64 code_generation;
	u32 instruction_count;
	u32 program_words;
	struct orlix_tcti_gadget_word program[];
};

u64 orlix_tcti_translation_generation(struct mm_struct *mm);
bool orlix_tcti_translation_generation_stable(struct mm_struct *mm,
					u64 generation);
bool orlix_tcti_mapping_access_lock(struct mm_struct *mm, u64 generation);
void orlix_tcti_mapping_access_unlock(struct mm_struct *mm);
void orlix_tcti_mapping_sequence_begin(struct mm_struct *mm);
void orlix_tcti_mapping_sequence_end(struct mm_struct *mm);
u64 orlix_tcti_code_generation(struct mm_struct *mm);
void orlix_tcti_bump_code_generation(struct mm_struct *mm);

struct orlix_tcti_block *orlix_tcti_block_cache_lookup(struct mm_struct *mm,
					   unsigned long guest_pc,
					   u64 code_generation);
int orlix_tcti_block_cache_insert(struct mm_struct *mm,
			    unsigned long guest_start_pc,
			    unsigned long guest_end_pc,
			    u64 code_generation,
			    u32 instruction_count,
			    const struct orlix_tcti_gadget_word *program,
			    u32 program_words,
			    struct orlix_tcti_block **out);
void orlix_tcti_block_put(struct orlix_tcti_block *block);
void orlix_tcti_block_cache_invalidate_mm(struct mm_struct *mm);
void orlix_tcti_block_cache_invalidate_range(struct mm_struct *mm,
				       unsigned long start, unsigned long end);
void orlix_tcti_block_cache_invalidate_all(void);

#if IS_ENABLED(CONFIG_ORLIX_TCTI_KUNIT_TEST)
void orlix_tcti_block_cache_reset_for_tests(void);
void orlix_tcti_block_cache_set_generation_for_tests(u64 generation);
u32 orlix_tcti_block_cache_count_for_tests(void);
#endif

#endif /* ORLIX_TCTI_BLOCK_CACHE_H */
