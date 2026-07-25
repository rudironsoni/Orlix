// SPDX-License-Identifier: GPL-2.0-only
#ifdef __KERNEL__
#include <linux/errno.h>
#else
#include <errno.h>
#endif

#include "target_ordinal_ledger.h"

struct tcti_a64_ordinal_source_row {
	u32 ordinal;
	const char *id;
	const char *mnemonic;
	const char *operation;
	u32 mask;
	u32 pattern;
	const char *feature_predicate_tcnd;
	u32 source_offset;
	u32 source_length;
};

struct tcti_a64_ordinal_source_provenance {
	const char *architecture;
	const char *build;
	const char *release;
	const char *schema;
	const char *sha256;
	u32 leaf_count;
};

#define TCTI_A64_SOURCE_MANIFEST_SOURCE(format, build, release, schema, sha256, leaf_count, timestamp, size) \
	format, build, release, schema, sha256, leaf_count
#define TCTI_A64_SOURCE_MANIFEST_ROW(...)
static const struct tcti_a64_ordinal_source_provenance source_provenance = {
#include "source_manifest.def"
};
#undef TCTI_A64_SOURCE_MANIFEST_ROW
#undef TCTI_A64_SOURCE_MANIFEST_SOURCE

#define TCTI_A64_SOURCE_MANIFEST_SOURCE(...)
#define TCTI_A64_SOURCE_MANIFEST_ROW(ordinal, id, mnemonic, operation, mask, pattern, predicate, offset, length) \
	{ ordinal, id, mnemonic, operation, mask, pattern, predicate, offset, length },
static const struct tcti_a64_ordinal_source_row source_rows[] = {
#include "source_manifest.def"
};
#undef TCTI_A64_SOURCE_MANIFEST_ROW
#undef TCTI_A64_SOURCE_MANIFEST_SOURCE

static int tcti_a64_ordinal_streq(const char *left, const char *right)
{
	while (*left && *right && *left == *right) {
		left++;
		right++;
	}
	return *left == *right;
}

int tcti_a64_ordinal_ledger_validate(
	const struct tcti_a64_ordinal_range *range,
	struct tcti_a64_ordinal_ledger_result *result)
{
	u32 ordinal;

	if (!range || !result)
		return -EINVAL;
	if (range->first > range->last ||
	    range->last >= TCTI_A64_TARGET_ORDINAL_LEAF_COUNT)
		return -ERANGE;
	if (sizeof(source_rows) / sizeof(source_rows[0]) !=
		TCTI_A64_TARGET_ORDINAL_LEAF_COUNT ||
	    source_provenance.leaf_count != TCTI_A64_TARGET_ORDINAL_LEAF_COUNT ||
	    !tcti_a64_ordinal_streq(source_provenance.architecture, "vFATAp1-A") ||
	    !tcti_a64_ordinal_streq(source_provenance.release, "2026-06_rel") ||
	    !tcti_a64_ordinal_streq(source_provenance.sha256,
		"a1ad2c6538a47cd97d8762791ac5af88bce1d5f6aff096c9b77aef853e76acfe"))
		return -ESTALE;

	result->rows = 0;
	result->first_ordinal = range->first;
	result->last_ordinal = range->last;
	for (ordinal = range->first; ordinal <= range->last; ordinal++) {
		const struct tcti_a64_ordinal_source_row *row =
			&source_rows[ordinal];

		if (row->ordinal != ordinal || !row->id[0] || !row->mnemonic[0] ||
		    !row->operation[0] || !row->feature_predicate_tcnd[0] ||
		    !row->source_length)
			return -EINVAL;
		result->rows++;
	}
	return 0;
}
