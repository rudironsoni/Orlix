// SPDX-License-Identifier: GPL-2.0-only
/*
 * This KUnit is intentionally fail-closed.  It proves that every pinned
 * feature-conditioned PAuth, BTI, and GCS leaf remains explicit and cannot
 * fall through to a baseline decoder class. External DDI0602 provenance does
 * not discharge any implementation, rejection, or proof obligation.
 */
#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/orlix_tcti.h>
#include <kunit/test.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/sched/mm.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/syscalls.h>

#include "../decode_aarch64.h"
#include "../isa/pauth_bti_gcs_obligation_ledger.h"
#include "orlix_tcti_test_suites.h"

#define PAUTH_BTI_GCS_RECORD(ordinal, source_id, operation, feature, asl, mask, pattern, behavior) \
	{ ordinal, source_id, operation, feature, asl, mask, pattern, behavior, \
	  ORLIX_TCTI_PAUTH_BTI_GCS_PROVENANCE_EXTERNAL_DDI0602, \
	  ORLIX_TCTI_PAUTH_BTI_GCS_IMPLEMENTATION_REQUIRED_UNIMPLEMENTED, \
	  ORLIX_TCTI_PAUTH_BTI_GCS_PROOF_REQUIRED_UNPROVEN },

static const struct orlix_tcti_pauth_bti_gcs_obligation_record pauth_bti_gcs_rows[] = {
	ORLIX_TCTI_PAUTH_BTI_GCS_OBLIGATION_ROWS(PAUTH_BTI_GCS_RECORD)
};

static void pauth_bti_gcs_inventory_is_complete_and_explicit(struct kunit *test)
{
	size_t index;

	KUNIT_EXPECT_EQ(test, 65U, ARRAY_SIZE(pauth_bti_gcs_rows));
	for (index = 0; index < ARRAY_SIZE(pauth_bti_gcs_rows); index++) {
		const struct orlix_tcti_pauth_bti_gcs_obligation_record *row =
			&pauth_bti_gcs_rows[index];
		size_t prior;

		KUNIT_EXPECT_NE_MSG(test, 0U, row->mask, "%s", row->source_id);
		KUNIT_EXPECT_EQ_MSG(test, row->pattern,
				    row->pattern & row->mask, "%s", row->source_id);
		KUNIT_EXPECT_TRUE_MSG(test, strlen(row->feature_predicate) > 0,
				      "%s", row->source_id);
		KUNIT_EXPECT_TRUE_MSG(test, !strncmp(row->asl_operation,
				    "operations/", 11), "%s", row->source_id);
		KUNIT_EXPECT_EQ_MSG(test,
			ORLIX_TCTI_PAUTH_BTI_GCS_PROVENANCE_EXTERNAL_DDI0602,
			row->semantic_provenance, "%s", row->source_id);
		KUNIT_EXPECT_EQ_MSG(test,
			ORLIX_TCTI_PAUTH_BTI_GCS_IMPLEMENTATION_REQUIRED_UNIMPLEMENTED,
			row->implementation_status, "%s", row->source_id);
		KUNIT_EXPECT_EQ_MSG(test,
			ORLIX_TCTI_PAUTH_BTI_GCS_PROOF_REQUIRED_UNPROVEN,
			row->proof_status, "%s", row->source_id);
		for (prior = 0; prior < index; prior++)
			KUNIT_EXPECT_NE_MSG(test, row->source_ordinal,
					    pauth_bti_gcs_rows[prior].source_ordinal,
					    "duplicate pinned ordinal %s", row->source_id);
	}
}

static void pauth_bti_gcs_unimplemented_leaves_fail_closed(struct kunit *test)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(pauth_bti_gcs_rows); index++) {
		const struct orlix_tcti_pauth_bti_gcs_obligation_record *row =
			&pauth_bti_gcs_rows[index];
		struct orlix_tcti_decoded_instruction decoded =
			orlix_tcti_decode_aarch64(row->pattern);

		KUNIT_EXPECT_EQ_MSG(test, row->pattern & row->mask,
				    decoded.instruction & row->mask,
				    "%u %s lost its source encoding", row->source_ordinal,
				    row->source_id);
		if (row->source_ordinal == 2253U ||
		    row->source_ordinal == 2264U) {
			KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_DECODE_HINT,
					    decoded.decode_class,
					    "%u %s is a BASE_SYSTEM hint NOP",
					    row->source_ordinal, row->source_id);
			KUNIT_EXPECT_EQ_MSG(test, row->source_ordinal,
					    decoded.source_ordinal,
					    "%u %s ordinal", row->source_ordinal,
					    row->source_id);
			continue;
		}
		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
				    decoded.decode_class,
				    "%u %s must not use a baseline semantic path",
				    row->source_ordinal, row->source_id);
	}
}

static void pauth_bti_gcs_non_el0_exits_remain_distinct(struct kunit *test)
{
	size_t index;
	unsigned int non_el0 = 0;

	for (index = 0; index < ARRAY_SIZE(pauth_bti_gcs_rows); index++)
		if (pauth_bti_gcs_rows[index].required_behavior ==
		    ORLIX_TCTI_PAUTH_BTI_GCS_NON_EL0_REJECTION)
			non_el0++;

	KUNIT_EXPECT_EQ(test, 2U, non_el0);
}

static unsigned long pauth_bti_gcs_map_instruction(struct kunit *test,
						    u32 instruction)
{
	u32 program[] = { instruction, 0xd4000001U };
	unsigned long mapped;
	int ret;

	mapped = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));
	ret = orlix_tcti_write_user_data(current->mm, mapped, program,
				   sizeof(program));
	KUNIT_ASSERT_EQ(test, 0, ret);
	ret = sys_mprotect(mapped, PAGE_SIZE, PROT_READ | PROT_EXEC);
	KUNIT_ASSERT_EQ(test, 0, ret);
	return mapped;
}

static void pauth_bti_gcs_non_el0_rejections_preserve_architectural_state(
	struct kunit *test)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(pauth_bti_gcs_rows); index++) {
		const struct orlix_tcti_pauth_bti_gcs_obligation_record *row =
			&pauth_bti_gcs_rows[index];
		struct pt_regs regs = { };
		struct pt_regs before;
		struct orlix_tcti_result result;
		unsigned long mapped;

		if (row->required_behavior !=
		    ORLIX_TCTI_PAUTH_BTI_GCS_NON_EL0_REJECTION)
			continue;

		mapped = pauth_bti_gcs_map_instruction(test, row->pattern);
		regs.pc = mapped;
		regs.sp = STACK_TOP - 16;
		regs.pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_C_BIT;
		regs.syscallno = NO_SYSCALL;
		regs.regs[0] = 0x123456789abcdef0ULL;
		regs.regs[30] = 0x0fedcba987654321ULL;
		before = regs;
		result = orlix_tcti_resume_user(current, &regs, current->mm);

		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
				    result.reason, "%u %s", row->source_ordinal,
				    row->source_id);
		KUNIT_EXPECT_EQ_MSG(test, -EOPNOTSUPP, result.status,
				    "%u %s", row->source_ordinal, row->source_id);
		KUNIT_EXPECT_EQ_MSG(test, row->pattern, result.instruction,
				    "%u %s", row->source_ordinal, row->source_id);
		KUNIT_EXPECT_EQ(test, before.pc, result.pc);
		KUNIT_EXPECT_MEMEQ(test, before.regs, regs.regs,
				   sizeof(regs.regs));
		KUNIT_EXPECT_EQ(test, before.pc, regs.pc);
		KUNIT_EXPECT_EQ(test, before.sp, regs.sp);
		KUNIT_EXPECT_EQ(test, before.pstate, regs.pstate);
		KUNIT_EXPECT_EQ(test, before.syscallno, regs.syscallno);
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
	}
}

static struct kunit_case pauth_bti_gcs_cases[] = {
	KUNIT_CASE(pauth_bti_gcs_inventory_is_complete_and_explicit),
	KUNIT_CASE(pauth_bti_gcs_unimplemented_leaves_fail_closed),
	KUNIT_CASE(pauth_bti_gcs_non_el0_exits_remain_distinct),
	KUNIT_CASE(pauth_bti_gcs_non_el0_rejections_preserve_architectural_state),
	{}
};

static int orlix_tcti_pauth_bti_gcs_obligation_test_init(struct kunit *test)
{
	struct mm_struct *mm = mm_alloc();

	if (!mm)
		return -ENOMEM;
	kthread_use_mm(mm);
	test->priv = mm;
	return 0;
}

static void orlix_tcti_pauth_bti_gcs_obligation_test_exit(struct kunit *test)
{
	struct mm_struct *mm = test->priv;

	if (!mm)
		return;
	kthread_unuse_mm(mm);
	mmput(mm);
}

struct kunit_suite orlix_tcti_pauth_bti_gcs_obligation_test_suite = {
	.name = "orlix-tcti-pauth-bti-gcs-obligations",
	.init = orlix_tcti_pauth_bti_gcs_obligation_test_init,
	.exit = orlix_tcti_pauth_bti_gcs_obligation_test_exit,
	.test_cases = pauth_bti_gcs_cases,
};

kunit_test_suite(orlix_tcti_pauth_bti_gcs_obligation_test_suite);

MODULE_LICENSE("GPL");
