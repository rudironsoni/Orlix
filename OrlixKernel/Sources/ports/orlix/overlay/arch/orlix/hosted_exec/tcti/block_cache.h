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
#define TCTI_BLOCK_CACHE_MAX_BLOCKS 4096U

struct mm_struct;
struct tcti_block;

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
	struct tcti_gadget_word program[];
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
