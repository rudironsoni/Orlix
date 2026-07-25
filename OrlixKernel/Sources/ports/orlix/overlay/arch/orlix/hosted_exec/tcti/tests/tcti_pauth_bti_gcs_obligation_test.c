// SPDX-License-Identifier: GPL-2.0-only
/*
 * This KUnit is intentionally fail-closed.  It proves that every pinned
 * feature-conditioned PAuth, BTI, and GCS leaf remains explicit and cannot
 * fall through to a baseline decoder class while shared ASL is absent.
 */
#include <kunit/test.h>
#include <linux/string.h>

#include "../decode_aarch64.h"
#include "../isa/pauth_bti_gcs_obligation_ledger.h"
#include "tcti_test_suites.h"

#define PAUTH_BTI_GCS_RECORD(ordinal, source_id, operation, feature, asl, mask, pattern, behavior) \
	{ ordinal, source_id, operation, feature, asl, mask, pattern, behavior },

static const struct tcti_pauth_bti_gcs_obligation_record pauth_bti_gcs_rows[] = {
	TCTI_PAUTH_BTI_GCS_OBLIGATION_ROWS(PAUTH_BTI_GCS_RECORD)
};

static void pauth_bti_gcs_inventory_is_complete_and_explicit(struct kunit *test)
{
	size_t index;

	KUNIT_EXPECT_EQ(test, 65U, ARRAY_SIZE(pauth_bti_gcs_rows));
	for (index = 0; index < ARRAY_SIZE(pauth_bti_gcs_rows); index++) {
		const struct tcti_pauth_bti_gcs_obligation_record *row =
			&pauth_bti_gcs_rows[index];
		size_t prior;

		KUNIT_EXPECT_NE_MSG(test, 0U, row->mask, "%s", row->source_id);
		KUNIT_EXPECT_EQ_MSG(test, row->pattern,
				    row->pattern & row->mask, "%s", row->source_id);
		KUNIT_EXPECT_TRUE_MSG(test, strlen(row->feature_predicate) > 0,
				      "%s", row->source_id);
		KUNIT_EXPECT_TRUE_MSG(test, !strncmp(row->asl_operation,
				    "operations/", 11), "%s", row->source_id);
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
		const struct tcti_pauth_bti_gcs_obligation_record *row =
			&pauth_bti_gcs_rows[index];
		struct tcti_decoded_instruction decoded =
			tcti_decode_aarch64(row->pattern);

		KUNIT_EXPECT_EQ_MSG(test, row->pattern & row->mask,
				    decoded.instruction & row->mask,
				    "%u %s lost its source encoding", row->source_ordinal,
				    row->source_id);
		KUNIT_EXPECT_EQ_MSG(test, TCTI_DECODE_UNSUPPORTED,
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
		    TCTI_PAUTH_BTI_GCS_NON_EL0_REJECTION)
			non_el0++;

	KUNIT_EXPECT_EQ(test, 2U, non_el0);
}

static struct kunit_case pauth_bti_gcs_cases[] = {
	KUNIT_CASE(pauth_bti_gcs_inventory_is_complete_and_explicit),
	KUNIT_CASE(pauth_bti_gcs_unimplemented_leaves_fail_closed),
	KUNIT_CASE(pauth_bti_gcs_non_el0_exits_remain_distinct),
	{}
};

struct kunit_suite tcti_pauth_bti_gcs_obligation_test_suite = {
	.name = "orlix-tcti-pauth-bti-gcs-obligations",
	.test_cases = pauth_bti_gcs_cases,
};

kunit_test_suite(tcti_pauth_bti_gcs_obligation_test_suite);

MODULE_LICENSE("GPL");
