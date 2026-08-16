/* SPDX-License-Identifier: GPL-2.0-only */
#include <kunit/test.h>

#include "../decode_aarch64.h"
#include "../sme_fp_decode.h"

struct orlix_tcti_sme_fp_source_leaf {
	u16 ordinal;
	const char *id;
	u32 mask;
	u32 pattern;
	const char *condition;
};

#define ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE(...)
#define ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW(ordinal, id, mnemonic, operation, \
					   mask, pattern, condition, offset, length) \
	{ ordinal, id, mask, pattern, condition },
static const struct orlix_tcti_sme_fp_source_leaf orlix_tcti_sme_fp_source[] = {
#include "../isa/source_manifest.def"
};
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE

static void orlix_tcti_sme_fp_source_matrix_decodes_exactly_94_leaves(
	struct kunit *test)
{
	size_t index;
	size_t rows = 0;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_sme_fp_source); index++) {
		const struct orlix_tcti_sme_fp_source_leaf *source =
			&orlix_tcti_sme_fp_source[index];
		struct orlix_tcti_decoded_instruction decoded;

		if (!orlix_tcti_sme_fp_source_ordinal(source->ordinal))
			continue;
		rows++;
		KUNIT_ASSERT_NOT_NULL_MSG(test, source->condition, "%u", source->ordinal);
		KUNIT_ASSERT_NE_MSG(test, source->condition[0], '\0', "%u", source->ordinal);
		decoded = orlix_tcti_decode_aarch64(source->pattern);
		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_DECODE_SME_FP,
			decoded.decode_class, "%u %s", source->ordinal, source->id);
		KUNIT_EXPECT_EQ_MSG(test, source->ordinal,
			decoded.sme_fp_source_ordinal, "%u %s", source->ordinal,
			source->id);
	}

	KUNIT_EXPECT_EQ(test, 94UL, rows);
}

static struct kunit_case orlix_tcti_sme_fp_decode_test_cases[] = {
	KUNIT_CASE(orlix_tcti_sme_fp_source_matrix_decodes_exactly_94_leaves),
	{}
};

static struct kunit_suite orlix_tcti_sme_fp_decode_test_suite = {
	.name = "orlix-tcti-sme-fp-decode",
	.test_cases = orlix_tcti_sme_fp_decode_test_cases,
};

kunit_test_suite(orlix_tcti_sme_fp_decode_test_suite);
