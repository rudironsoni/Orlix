/* SPDX-License-Identifier: GPL-2.0-only */
#include <kunit/test.h>

#include "../mops_provenance.h"

static void orlix_tcti_mops_provenance_covers_every_pinned_leaf(struct kunit *test)
{
	struct orlix_tcti_mops_leaf_provenance leaf;
	u32 ordinal;
	u32 count = 0;

	for (ordinal = 2675U; ordinal <= 2823U; ++ordinal) {
		if (ordinal >= 2687U && ordinal <= 2703U) {
			KUNIT_EXPECT_FALSE(test,
					   orlix_tcti_mops_leaf_provenance(ordinal, &leaf));
			continue;
		}
		KUNIT_ASSERT_TRUE(test, orlix_tcti_mops_leaf_provenance(ordinal, &leaf));
		KUNIT_EXPECT_EQ(test, leaf.source_ordinal, ordinal);
		KUNIT_EXPECT_STREQ(test, leaf.feature,
				  ordinal <= 2686U ? "FEAT_MOPS_GO" : "FEAT_MOPS");
		KUNIT_EXPECT_EQ(test, leaf.semantic_provenance,
				ordinal <= 2686U ?
				ORLIX_TCTI_MOPS_PROVENANCE_OFFICIAL_NOT_SPECIFIED :
				ORLIX_TCTI_MOPS_PROVENANCE_EXTERNAL_DDI0602);
		if ((ordinal >= 2704U && ordinal <= 2751U) ||
		    (ordinal >= 2764U && ordinal <= 2811U)) {
			KUNIT_EXPECT_EQ(test, leaf.implementation_status,
				ORLIX_TCTI_MOPS_IMPLEMENTATION_PRODUCTION);
			KUNIT_EXPECT_EQ(test, leaf.proof_status,
				ORLIX_TCTI_MOPS_PROOF_KUNIT_OWNER);
			KUNIT_EXPECT_NOT_NULL(test, leaf.ddi0602_locator);
			KUNIT_EXPECT_NOT_NULL(test, leaf.ddi0602_archive_sha256);
			KUNIT_EXPECT_STREQ(test, leaf.production_owner,
				"orlix_tcti_execute_mops_copy");
			KUNIT_EXPECT_STREQ(test, leaf.kunit_suite,
				"orlix-tcti-mops-copy");
			KUNIT_EXPECT_STREQ(test, leaf.linux_proof_disposition,
				"not_applicable_no_linux_visible_abi");
		} else {
			KUNIT_EXPECT_EQ(test, leaf.implementation_status,
				ORLIX_TCTI_MOPS_IMPLEMENTATION_REQUIRED_UNIMPLEMENTED);
			KUNIT_EXPECT_EQ(test, leaf.proof_status,
				ORLIX_TCTI_MOPS_PROOF_REQUIRED_UNPROVEN);
		}
		KUNIT_ASSERT_NOT_NULL(test, leaf.operation);
		KUNIT_ASSERT_NOT_NULL(test, leaf.asl_operation);
		count++;
	}

	KUNIT_EXPECT_EQ(test, count, ORLIX_TCTI_MOPS_TOTAL_LEAF_COUNT);
}

static void orlix_tcti_mops_go_semantics_remain_officially_unspecified(
	struct kunit *test)
{
	struct orlix_tcti_mops_leaf_provenance leaf;
	u32 ordinal;

	for (ordinal = 2675U; ordinal <= 2686U; ordinal++) {
		KUNIT_ASSERT_TRUE(test,
			orlix_tcti_mops_leaf_provenance(ordinal, &leaf));
		KUNIT_EXPECT_EQ(test, leaf.semantic_provenance,
			ORLIX_TCTI_MOPS_PROVENANCE_OFFICIAL_NOT_SPECIFIED);
		KUNIT_EXPECT_EQ(test, leaf.implementation_status,
			ORLIX_TCTI_MOPS_IMPLEMENTATION_REQUIRED_UNIMPLEMENTED);
		KUNIT_EXPECT_EQ(test, leaf.proof_status,
			ORLIX_TCTI_MOPS_PROOF_REQUIRED_UNPROVEN);
	}
}

static void orlix_tcti_mops_provenance_preserves_phase_partitions(struct kunit *test)
{
	struct orlix_tcti_mops_leaf_provenance leaf;

	KUNIT_ASSERT_TRUE(test, orlix_tcti_mops_leaf_provenance(2704U, &leaf));
	KUNIT_EXPECT_EQ(test, leaf.phase,
			ORLIX_TCTI_MOPS_PHASE_COPY_FORWARD_PROLOGUE);
	KUNIT_EXPECT_STREQ(test, leaf.operation, "CPYFP");

	KUNIT_ASSERT_TRUE(test, orlix_tcti_mops_leaf_provenance(2720U, &leaf));
	KUNIT_EXPECT_EQ(test, leaf.phase, ORLIX_TCTI_MOPS_PHASE_COPY_FORWARD_MAIN);
	KUNIT_EXPECT_STREQ(test, leaf.operation, "CPYFM");

	KUNIT_ASSERT_TRUE(test, orlix_tcti_mops_leaf_provenance(2736U, &leaf));
	KUNIT_EXPECT_EQ(test, leaf.phase,
			ORLIX_TCTI_MOPS_PHASE_COPY_FORWARD_EPILOGUE);
	KUNIT_EXPECT_STREQ(test, leaf.operation, "CPYFE");

	KUNIT_ASSERT_TRUE(test, orlix_tcti_mops_leaf_provenance(2764U, &leaf));
	KUNIT_EXPECT_EQ(test, leaf.phase,
			ORLIX_TCTI_MOPS_PHASE_COPY_BACKWARD_PROLOGUE);
	KUNIT_EXPECT_STREQ(test, leaf.operation, "CPYP");

	KUNIT_ASSERT_TRUE(test, orlix_tcti_mops_leaf_provenance(2796U, &leaf));
	KUNIT_EXPECT_EQ(test, leaf.phase,
			ORLIX_TCTI_MOPS_PHASE_COPY_BACKWARD_EPILOGUE);
	KUNIT_EXPECT_STREQ(test, leaf.operation, "CPYE");

	KUNIT_ASSERT_TRUE(test, orlix_tcti_mops_leaf_provenance(2820U, &leaf));
	KUNIT_EXPECT_EQ(test, leaf.phase,
			ORLIX_TCTI_MOPS_PHASE_SET_TAGGED_EPILOGUE);
	KUNIT_EXPECT_STREQ(test, leaf.operation, "SETGE");
}

static void orlix_tcti_mops_copy_provenance_uses_exact_ddi0602_locators(
	struct kunit *test)
{
	struct orlix_tcti_mops_leaf_provenance leaf;

	KUNIT_ASSERT_TRUE(test, orlix_tcti_mops_leaf_provenance(2704U, &leaf));
	KUNIT_EXPECT_STREQ(test, leaf.ddi0602_locator,
		"cpyfp.xml#A64.ldst.memcms.CPYFP_CPY_memcms/execute");
	KUNIT_EXPECT_STREQ(test, leaf.ddi0602_archive_sha256,
		"63a01a1696483bbe2edfef9e0f0cd053d6c1c619ec0587876cb7a60bb344f354");
	KUNIT_ASSERT_TRUE(test, orlix_tcti_mops_leaf_provenance(2764U, &leaf));
	KUNIT_EXPECT_STREQ(test, leaf.ddi0602_locator,
		"cpyp.xml#A64.ldst.memcms.CPYP_CPY_memcms/execute");
}

static struct kunit_case orlix_tcti_mops_provenance_cases[] = {
	KUNIT_CASE(orlix_tcti_mops_provenance_covers_every_pinned_leaf),
	KUNIT_CASE(orlix_tcti_mops_go_semantics_remain_officially_unspecified),
	KUNIT_CASE(orlix_tcti_mops_provenance_preserves_phase_partitions),
	KUNIT_CASE(orlix_tcti_mops_copy_provenance_uses_exact_ddi0602_locators),
	{}
};

static struct kunit_suite orlix_tcti_mops_provenance_suite = {
	.name = "orlix-tcti-mops-provenance",
	.test_cases = orlix_tcti_mops_provenance_cases,
};

kunit_test_suite(orlix_tcti_mops_provenance_suite);

MODULE_LICENSE("GPL");
