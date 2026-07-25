/* SPDX-License-Identifier: GPL-2.0-only */
#include <kunit/test.h>

#include "../mops_provenance.h"

static void tcti_mops_provenance_covers_every_pinned_leaf(struct kunit *test)
{
	struct tcti_mops_leaf_provenance leaf;
	u32 ordinal;
	u32 count = 0;

	for (ordinal = 2675U; ordinal <= 2823U; ++ordinal) {
		if (ordinal >= 2687U && ordinal <= 2703U) {
			KUNIT_EXPECT_FALSE(test,
					   tcti_mops_leaf_provenance(ordinal, &leaf));
			continue;
		}
		KUNIT_ASSERT_TRUE(test, tcti_mops_leaf_provenance(ordinal, &leaf));
		KUNIT_EXPECT_EQ(test, leaf.source_ordinal, ordinal);
		KUNIT_EXPECT_STREQ(test, leaf.feature,
				  ordinal <= 2686U ? "FEAT_MOPS_GO" : "FEAT_MOPS");
		KUNIT_EXPECT_EQ(test, leaf.semantics_status,
				TCTI_MOPS_SEMANTICS_SHARED_ASL_ABSENT_BLOCKING);
		KUNIT_ASSERT_NOT_NULL(test, leaf.operation);
		KUNIT_ASSERT_NOT_NULL(test, leaf.asl_operation);
		count++;
	}

	KUNIT_EXPECT_EQ(test, count, TCTI_MOPS_TOTAL_LEAF_COUNT);
}

static void tcti_mops_provenance_preserves_phase_partitions(struct kunit *test)
{
	struct tcti_mops_leaf_provenance leaf;

	KUNIT_ASSERT_TRUE(test, tcti_mops_leaf_provenance(2704U, &leaf));
	KUNIT_EXPECT_EQ(test, leaf.phase,
			TCTI_MOPS_PHASE_COPY_FORWARD_PROLOGUE);
	KUNIT_EXPECT_STREQ(test, leaf.operation, "CPYFP");

	KUNIT_ASSERT_TRUE(test, tcti_mops_leaf_provenance(2720U, &leaf));
	KUNIT_EXPECT_EQ(test, leaf.phase, TCTI_MOPS_PHASE_COPY_FORWARD_MAIN);
	KUNIT_EXPECT_STREQ(test, leaf.operation, "CPYFM");

	KUNIT_ASSERT_TRUE(test, tcti_mops_leaf_provenance(2736U, &leaf));
	KUNIT_EXPECT_EQ(test, leaf.phase,
			TCTI_MOPS_PHASE_COPY_FORWARD_EPILOGUE);
	KUNIT_EXPECT_STREQ(test, leaf.operation, "CPYFE");

	KUNIT_ASSERT_TRUE(test, tcti_mops_leaf_provenance(2764U, &leaf));
	KUNIT_EXPECT_EQ(test, leaf.phase,
			TCTI_MOPS_PHASE_COPY_BACKWARD_PROLOGUE);
	KUNIT_EXPECT_STREQ(test, leaf.operation, "CPYP");

	KUNIT_ASSERT_TRUE(test, tcti_mops_leaf_provenance(2796U, &leaf));
	KUNIT_EXPECT_EQ(test, leaf.phase,
			TCTI_MOPS_PHASE_COPY_BACKWARD_EPILOGUE);
	KUNIT_EXPECT_STREQ(test, leaf.operation, "CPYE");

	KUNIT_ASSERT_TRUE(test, tcti_mops_leaf_provenance(2820U, &leaf));
	KUNIT_EXPECT_EQ(test, leaf.phase,
			TCTI_MOPS_PHASE_SET_TAGGED_EPILOGUE);
	KUNIT_EXPECT_STREQ(test, leaf.operation, "SETGE");
}

static struct kunit_case tcti_mops_provenance_cases[] = {
	KUNIT_CASE(tcti_mops_provenance_covers_every_pinned_leaf),
	KUNIT_CASE(tcti_mops_provenance_preserves_phase_partitions),
	{}
};

static struct kunit_suite tcti_mops_provenance_suite = {
	.name = "orlix-tcti-mops-provenance",
	.test_cases = tcti_mops_provenance_cases,
};

kunit_test_suite(tcti_mops_provenance_suite);

MODULE_LICENSE("GPL");
