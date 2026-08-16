/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * The direct official source has no SETGO execution semantics. This verifies
 * only the generated AARCHMRS inventory and fail-closed disposition; it does
 * not assert data, tag, flag, fault-progress, T, or N behavior.
 */
#include <kunit/test.h>

#include "../decode_aarch64.h"
#include "../mops_provenance.h"

struct setgo_manifest_leaf {
	u32 ordinal;
	const char *name;
	const char *feature;
	u32 mask;
	u32 pattern;
};

#define ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE(...)
#define ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW(ordinal, name, mnemonic, operation, \
	mask, pattern, feature, offset, length) \
	{ ordinal, name, feature, mask, pattern },
static const struct setgo_manifest_leaf setgo_manifest[] = {
#include "../isa/source_manifest.def"
};
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE

static void orlix_tcti_setgo_generated_manifest_matrix(struct kunit *test)
{
	size_t index;
	u32 count = 0;

	for (index = 0; index < ARRAY_SIZE(setgo_manifest); index++) {
		const struct setgo_manifest_leaf *leaf = &setgo_manifest[index];
		struct orlix_tcti_decoded_instruction decoded;
		struct orlix_tcti_mops_leaf_provenance provenance;
		u32 instruction;

		if (leaf->ordinal < 2675U || leaf->ordinal > 2686U)
			continue;

		instruction = leaf->pattern | (3U << 16) | (2U << 5) | 1U;
		decoded = orlix_tcti_decode_aarch64(instruction);
		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_DECODE_SET_GO,
			decoded.decode_class, "%u %s", leaf->ordinal, leaf->name);
		KUNIT_EXPECT_EQ(test, 0x3ffffc00U, leaf->mask);
		KUNIT_EXPECT_STREQ(test, "FEAT_MOPS_GO", leaf->feature);
		KUNIT_EXPECT_EQ(test, leaf->ordinal - 2675U, (u32)decoded.setgo_op);
		KUNIT_EXPECT_EQ(test, 3, decoded.rs);
		KUNIT_EXPECT_EQ(test, 2, decoded.rn);
		KUNIT_EXPECT_EQ(test, 1, decoded.rd);
		KUNIT_ASSERT_TRUE(test,
			orlix_tcti_mops_leaf_provenance(leaf->ordinal, &provenance));
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_MOPS_PROVENANCE_OFFICIAL_NOT_SPECIFIED,
			provenance.semantic_provenance);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_MOPS_IMPLEMENTATION_REQUIRED_UNIMPLEMENTED,
			provenance.implementation_status);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_MOPS_PROOF_REQUIRED_UNPROVEN,
			provenance.proof_status);
		count++;
	}

	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_MOPS_GO_LEAF_COUNT, count);
}

static struct kunit_case orlix_tcti_setgo_source_bound_cases[] = {
	KUNIT_CASE(orlix_tcti_setgo_generated_manifest_matrix),
	{}
};

static struct kunit_suite orlix_tcti_setgo_source_bound_suite = {
	.name = "orlix-tcti-setgo-source-bound",
	.test_cases = orlix_tcti_setgo_source_bound_cases,
};

kunit_test_suite(orlix_tcti_setgo_source_bound_suite);

MODULE_LICENSE("GPL");
