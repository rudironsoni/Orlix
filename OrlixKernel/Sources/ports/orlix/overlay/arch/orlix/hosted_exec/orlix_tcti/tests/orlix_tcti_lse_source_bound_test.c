/* SPDX-License-Identifier: GPL-2.0-only */
#include <kunit/test.h>
#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched/mm.h>
#include <linux/syscalls.h>
#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/orlix_tcti.h>

#include "../decode_aarch64.h"
#include "../switch_debug.h"
#include "target_lse_operation_catalog.h"
#include "orlix_tcti_test_suites.h"

static bool lse_source_bound_casp(
	const struct orlix_tcti_lse_operation_catalog_entry *entry)
{
	return !strcmp(entry->operation_id, "CASP");
}

static u32 lse_source_bound_instruction(
	const struct orlix_tcti_lse_operation_catalog_entry *entry)
{
	u32 instruction = entry->encoding_pattern;

	/* Rs, Rn, and Rt are variable in every base-LSE source leaf. */
	instruction &= ~((0x1fU << 16) | (0x1fU << 5) | 0x1fU);
	if (lse_source_bound_casp(entry))
		return instruction | (2U << 16) | (10U << 5) | 8U;
	return instruction | (3U << 16) | (10U << 5) | 8U;
}

static u64 lse_source_bound_mask(u8 width)
{
	return width == sizeof(u64) ? U64_MAX : (1ULL << (width * 8)) - 1;
}

static u64 lse_source_bound_rmw_result(enum orlix_tcti_lse_atomic_op operation,
					u64 old, u64 operand, u8 width)
{
	u64 mask = lse_source_bound_mask(width);
	s64 signed_old = sign_extend64(old, width * 8 - 1);
	s64 signed_operand = sign_extend64(operand, width * 8 - 1);

	switch (operation) {
	case ORLIX_TCTI_LSE_ATOMIC_SWP:
		return operand & mask;
	case ORLIX_TCTI_LSE_ATOMIC_ADD:
		return (old + operand) & mask;
	case ORLIX_TCTI_LSE_ATOMIC_CLR:
		return old & ~operand & mask;
	case ORLIX_TCTI_LSE_ATOMIC_EOR:
		return (old ^ operand) & mask;
	case ORLIX_TCTI_LSE_ATOMIC_SET:
		return (old | operand) & mask;
	case ORLIX_TCTI_LSE_ATOMIC_SMAX:
		return (signed_old > signed_operand ? old : operand) & mask;
	case ORLIX_TCTI_LSE_ATOMIC_SMIN:
		return (signed_old < signed_operand ? old : operand) & mask;
	case ORLIX_TCTI_LSE_ATOMIC_UMAX:
		return (old > operand ? old : operand) & mask;
	case ORLIX_TCTI_LSE_ATOMIC_UMIN:
		return (old < operand ? old : operand) & mask;
	case ORLIX_TCTI_LSE_ATOMIC_CAS:
		break;
	}
	return 0;
}

static unsigned long lse_source_bound_map(struct kunit *test)
{
	unsigned long mapped;

	mapped = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));
	return mapped;
}

static void lse_source_bound_catalog_is_exact_base_lse(struct kunit *test)
{
	const struct orlix_tcti_lse_operation_catalog_entry *entries;
	enum orlix_tcti_lse_operation_catalog_error error;
	size_t count;
	size_t index;

	entries = orlix_tcti_lse_operation_catalog(&count);
	KUNIT_ASSERT_NOT_NULL(test, entries);
	KUNIT_ASSERT_EQ(test, ORLIX_TCTI_LSE_OPERATION_CATALOG_DIRECT_LEAF_COUNT,
			count);
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_lse_operation_catalog_validate(
			entries, count, orlix_tcti_lse_operation_catalog_lse2_blocker(),
			&error));
	for (index = 0; index < count; index++) {
		const struct orlix_tcti_lse_operation_catalog_entry *entry =
			&entries[index];

		if (entry->cohort != ORLIX_TCTI_LSE_OPERATION_COHORT_BASE_LSE)
			continue;
		KUNIT_EXPECT_TRUE(test, entry->direct_source_leaf);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_LSE_OPERATION_STRUCTURAL_DECODER_ONLY,
				entry->implementation_status);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_LSE_OPERATION_COHORT_BASE_LSE,
				entry->implementation_cohort);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_LSE_OPERATION_PROOF_NONE,
				entry->proof_status);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_LSE_OPERATION_COHORT_NONE,
				entry->proof_cohort);
		KUNIT_EXPECT_EQ(test,
			ORLIX_TCTI_LSE_OPERATION_OBLIGATION_ATOMICITY |
			ORLIX_TCTI_LSE_OPERATION_OBLIGATION_ORDERING,
			entry->obligations &
			(ORLIX_TCTI_LSE_OPERATION_OBLIGATION_ATOMICITY |
			 ORLIX_TCTI_LSE_OPERATION_OBLIGATION_ORDERING));
	}
}

static void lse_source_bound_decodes_every_base_leaf(struct kunit *test)
{
	const struct orlix_tcti_lse_operation_catalog_entry *entries;
	size_t count;
	size_t index;
	size_t base_count = 0;

	entries = orlix_tcti_lse_operation_catalog(&count);
	KUNIT_ASSERT_NOT_NULL(test, entries);
	for (index = 0; index < count; index++) {
		const struct orlix_tcti_lse_operation_catalog_entry *entry =
			&entries[index];
		struct orlix_tcti_decoded_instruction decoded;
		u32 instruction;
		enum orlix_tcti_lse_atomic_op expected;

		if (entry->cohort != ORLIX_TCTI_LSE_OPERATION_COHORT_BASE_LSE)
			continue;
		instruction = lse_source_bound_instruction(entry);
		KUNIT_ASSERT_EQ_MSG(test, entry->encoding_pattern,
				instruction & entry->encoding_mask, "%s",
				entry->source_leaf);
		decoded = orlix_tcti_decode_aarch64(instruction);
		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_DECODE_LSE_ATOMIC,
				decoded.decode_class, "%s %#x", entry->source_leaf,
				instruction);
		expected = entry->semantic_class ==
			ORLIX_TCTI_LSE_OPERATION_SEMANTIC_COMPARE_AND_SWAP ?
			ORLIX_TCTI_LSE_ATOMIC_CAS :
			(((instruction >> 12) & 0xfU) == 8U ?
			 ORLIX_TCTI_LSE_ATOMIC_SWP :
			 ORLIX_TCTI_LSE_ATOMIC_ADD + ((instruction >> 12) & 0xfU));
		KUNIT_EXPECT_EQ_MSG(test, expected, decoded.lse_atomic_op,
				"%s %#x", entry->source_leaf, instruction);
		KUNIT_EXPECT_EQ_MSG(test, lse_source_bound_casp(entry), decoded.pair,
				"%s %#x", entry->source_leaf, instruction);
		if (entry->semantic_class ==
		    ORLIX_TCTI_LSE_OPERATION_SEMANTIC_COMPARE_AND_SWAP) {
			KUNIT_EXPECT_EQ(test, instruction & BIT(22) ? 1 : 0,
					decoded.acquire);
			KUNIT_EXPECT_EQ(test, instruction & BIT(15) ? 1 : 0,
					decoded.release);
		} else {
			KUNIT_EXPECT_EQ(test, instruction & BIT(23) ? 1 : 0,
					decoded.acquire);
			KUNIT_EXPECT_EQ(test, instruction & BIT(22) ? 1 : 0,
					decoded.release);
		}
		base_count++;
	}
	KUNIT_EXPECT_EQ(test, 168U, base_count);
}

static void lse_source_bound_executes_every_base_leaf(struct kunit *test)
{
	const struct orlix_tcti_lse_operation_catalog_entry *entries;
	unsigned long mapped = lse_source_bound_map(test);
	size_t count;
	size_t index;

	entries = orlix_tcti_lse_operation_catalog(&count);
	KUNIT_ASSERT_NOT_NULL(test, entries);
	for (index = 0; index < count; index++) {
		const struct orlix_tcti_lse_operation_catalog_entry *entry =
			&entries[index];
		struct orlix_tcti_decoded_instruction decoded;
		struct pt_regs regs = {};
		u64 old[2] = { 0x1122334455667788ULL, 0x8877665544332211ULL };
		u64 operand[2] = { 0x0123456789abcdefULL, 0xfedcba9876543210ULL };
		u64 observed[2] = {};
		u64 mask;
		u32 instruction;
		u8 width;
		unsigned long fault_address = 0;
		int ret;

		if (entry->cohort != ORLIX_TCTI_LSE_OPERATION_COHORT_BASE_LSE)
			continue;
		instruction = lse_source_bound_instruction(entry);
		decoded = orlix_tcti_decode_aarch64(instruction);
		KUNIT_ASSERT_EQ_MSG(test, ORLIX_TCTI_DECODE_LSE_ATOMIC,
				decoded.decode_class, "%s", entry->source_leaf);
		width = decoded.access_size;
		mask = lse_source_bound_mask(width);
		old[0] &= mask;
		old[1] &= mask;
		operand[0] &= mask;
		operand[1] &= mask;
		regs.regs[decoded.rn] = mapped;
		if (decoded.lse_atomic_op == ORLIX_TCTI_LSE_ATOMIC_CAS) {
			regs.regs[decoded.rs] = old[0];
			regs.regs[decoded.rt] = operand[0];
		} else {
			regs.regs[decoded.rs] = operand[0];
			regs.regs[decoded.rt] = U64_MAX;
		}
		if (decoded.pair) {
			u64 memory_old[2] = { old[0], old[1] };

			if (decoded.lse_atomic_op == ORLIX_TCTI_LSE_ATOMIC_CAS) {
				regs.regs[decoded.rs + 1] = old[1];
				regs.regs[decoded.rt + 1] = operand[1];
			} else {
				regs.regs[decoded.rs + 1] = operand[1];
				regs.regs[decoded.rt + 1] = U64_MAX;
			}
			if (width < sizeof(u64)) {
				memory_old[0] = old[0] | (old[1] << (width * 8));
				memory_old[1] = 0;
			}
			ret = orlix_tcti_write_user_data(current->mm, mapped, memory_old,
					  2 * width);
		} else {
			ret = orlix_tcti_write_user_data(current->mm, mapped, old, width);
		}
		KUNIT_ASSERT_EQ_MSG(test, 0, ret, "%s", entry->source_leaf);
		regs.pc = 0x1000 + index * sizeof(u32);
		ret = orlix_tcti_switch_debug_execute_decoded(current->mm, &regs, &decoded,
						      &fault_address);
		KUNIT_ASSERT_EQ_MSG(test, 0, ret, "%s", entry->source_leaf);
		if (decoded.pair)
			ret = orlix_tcti_read_user_data(current->mm, mapped, observed,
					  2 * width);
		else
			ret = orlix_tcti_read_user_data(current->mm, mapped, observed, width);
		KUNIT_ASSERT_EQ_MSG(test, 0, ret, "%s", entry->source_leaf);
		if (decoded.lse_atomic_op == ORLIX_TCTI_LSE_ATOMIC_CAS) {
			if (decoded.pair && width < sizeof(u64)) {
				KUNIT_EXPECT_EQ(test,
					operand[0] | (operand[1] << (width * 8)),
					observed[0]);
			} else {
				KUNIT_EXPECT_EQ(test, operand[0], observed[0]);
				if (decoded.pair)
					KUNIT_EXPECT_EQ(test, operand[1], observed[1]);
			}
			KUNIT_EXPECT_EQ(test, old[0], regs.regs[decoded.rs]);
			if (decoded.pair)
				KUNIT_EXPECT_EQ(test, old[1], regs.regs[decoded.rs + 1]);
		} else {
			KUNIT_EXPECT_EQ(test,
				lse_source_bound_rmw_result(decoded.lse_atomic_op,
					old[0], operand[0], width), observed[0]);
			KUNIT_EXPECT_EQ(test, old[0], regs.regs[decoded.rt]);
		}
		KUNIT_EXPECT_EQ(test, mapped, fault_address);
		KUNIT_EXPECT_EQ(test, 0x1004ULL + index * sizeof(u32), regs.pc);
	}
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void lse_source_bound_rejects_reserved_encodings(struct kunit *test)
{
	const struct orlix_tcti_lse_operation_catalog_entry *entries;
	size_t count;
	size_t index;
	size_t cas = 0;
	size_t casp = 0;
	size_t rmw = 0;

	entries = orlix_tcti_lse_operation_catalog(&count);
	KUNIT_ASSERT_NOT_NULL(test, entries);
	for (index = 0; index < count; index++) {
		const struct orlix_tcti_lse_operation_catalog_entry *entry =
			&entries[index];
		struct orlix_tcti_decoded_instruction decoded;
		u32 instruction;

		if (entry->cohort != ORLIX_TCTI_LSE_OPERATION_COHORT_BASE_LSE)
			continue;
		instruction = lse_source_bound_instruction(entry);
		if (lse_source_bound_casp(entry)) {
			instruction |= 1U << 16; /* Rs must be even. */
			casp++;
		} else if (entry->semantic_class ==
			   ORLIX_TCTI_LSE_OPERATION_SEMANTIC_COMPARE_AND_SWAP) {
			instruction ^= BIT(10); /* Reserved CAS neighbour. */
			cas++;
		} else {
			instruction &= ~(0xfU << 12);
			instruction |= 9U << 12; /* RMW op values 9..15 are reserved. */
			rmw++;
		}
		decoded = orlix_tcti_decode_aarch64(instruction);
		if (decoded.decode_class == ORLIX_TCTI_DECODE_LS64 ||
		    decoded.decode_class == ORLIX_TCTI_DECODE_LOAD_STORE_EXCLUSIVE ||
		    (decoded.decode_class == ORLIX_TCTI_DECODE_LSE_ATOMIC &&
		     decoded.atomic_rcw))
			continue;
		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
				decoded.decode_class, "%s %#x", entry->source_leaf,
				instruction);
	}
	KUNIT_EXPECT_EQ(test, 16U, cas);
	KUNIT_EXPECT_EQ(test, 8U, casp);
	KUNIT_EXPECT_EQ(test, 144U, rmw);
}

static void lse_source_bound_alignment_and_faults_use_production_path(
	struct kunit *test)
{
	const struct orlix_tcti_lse_operation_catalog_entry *entries;
	unsigned long mapped = lse_source_bound_map(test);
	size_t count;
	size_t index;
	bool width_seen[17] = {};

	entries = orlix_tcti_lse_operation_catalog(&count);
	KUNIT_ASSERT_NOT_NULL(test, entries);
	for (index = 0; index < count; index++) {
		const struct orlix_tcti_lse_operation_catalog_entry *entry =
			&entries[index];
		struct orlix_tcti_decoded_instruction decoded;
		struct pt_regs regs = {};
		u32 instruction;
		u8 width;
		unsigned long fault_address = 0;
		int ret;

		if (entry->cohort != ORLIX_TCTI_LSE_OPERATION_COHORT_BASE_LSE)
			continue;
		instruction = lse_source_bound_instruction(entry);
		decoded = orlix_tcti_decode_aarch64(instruction);
		width = decoded.access_size * (decoded.pair ? 2 : 1);
		if (width_seen[width])
			continue;
		width_seen[width] = true;
		regs.regs[decoded.rn] = width == 1 ? TASK_SIZE : mapped + 1;
		regs.regs[decoded.rs] = 1;
		regs.regs[decoded.rt] = 2;
		regs.pc = 0x9000;
		ret = orlix_tcti_switch_debug_execute_decoded(current->mm, &regs, &decoded,
						      &fault_address);
		KUNIT_EXPECT_EQ_MSG(test, -EFAULT, ret, "%s", entry->source_leaf);
		KUNIT_EXPECT_EQ(test, regs.regs[decoded.rn], fault_address);
		KUNIT_EXPECT_EQ(test, 0x9000ULL, regs.pc);
	}
	KUNIT_EXPECT_TRUE(test, width_seen[1]);
	KUNIT_EXPECT_TRUE(test, width_seen[2]);
	KUNIT_EXPECT_TRUE(test, width_seen[4]);
	KUNIT_EXPECT_TRUE(test, width_seen[8]);
	KUNIT_EXPECT_TRUE(test, width_seen[16]);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void lse_source_bound_cannot_prove_extension_cohorts(struct kunit *test)
{
	const struct orlix_tcti_lse_operation_catalog_entry *entries;
	const struct orlix_tcti_lse_operation_catalog_entry *lse2;
	size_t count;
	size_t index;
	size_t base_count = 0;

	entries = orlix_tcti_lse_operation_catalog(&count);
	lse2 = orlix_tcti_lse_operation_catalog_lse2_blocker();
	KUNIT_ASSERT_NOT_NULL(test, entries);
	KUNIT_ASSERT_NOT_NULL(test, lse2);
	for (index = 0; index < count; index++)
		if (entries[index].cohort == ORLIX_TCTI_LSE_OPERATION_COHORT_BASE_LSE)
			base_count++;
	KUNIT_EXPECT_EQ(test, 168U, base_count);
	KUNIT_EXPECT_FALSE(test, lse2->direct_source_leaf);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_LSE_OPERATION_COHORT_LSE2_SEMANTIC_VARIANT,
			lse2->cohort);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_LSE_OPERATION_PROOF_NONE,
			lse2->proof_status);
}

static struct kunit_case orlix_tcti_lse_source_bound_test_cases[] = {
	KUNIT_CASE(lse_source_bound_catalog_is_exact_base_lse),
	KUNIT_CASE(lse_source_bound_decodes_every_base_leaf),
	KUNIT_CASE(lse_source_bound_executes_every_base_leaf),
	KUNIT_CASE(lse_source_bound_rejects_reserved_encodings),
	KUNIT_CASE(lse_source_bound_alignment_and_faults_use_production_path),
	KUNIT_CASE(lse_source_bound_cannot_prove_extension_cohorts),
	{}
};

static int orlix_tcti_lse_source_bound_test_init(struct kunit *test)
{
	struct mm_struct *mm = mm_alloc();

	if (!mm)
		return -ENOMEM;
	kthread_use_mm(mm);
	test->priv = mm;
	return 0;
}

static void orlix_tcti_lse_source_bound_test_exit(struct kunit *test)
{
	struct mm_struct *mm = test->priv;

	if (!mm)
		return;
	kthread_unuse_mm(mm);
	mmput(mm);
}

struct kunit_suite orlix_tcti_lse_source_bound_test_suite = {
	.name = "orlix-tcti-lse-source-bound",
	.init = orlix_tcti_lse_source_bound_test_init,
	.exit = orlix_tcti_lse_source_bound_test_exit,
	.test_cases = orlix_tcti_lse_source_bound_test_cases,
};

kunit_test_suite(orlix_tcti_lse_source_bound_test_suite);
