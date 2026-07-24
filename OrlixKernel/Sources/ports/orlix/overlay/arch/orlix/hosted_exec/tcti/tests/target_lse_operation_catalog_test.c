/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_lse_operation_catalog.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EXPECT(expression) do { \
	if (!(expression)) { \
		fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression); \
		return -1; \
	} \
} while (0)

static int valid_catalog_has_exact_family_counts(void)
{
	const struct tcti_lse_operation_catalog_entry *entries;
	enum tcti_lse_operation_catalog_error error;
	size_t count;
	size_t index;
	size_t base_lse = 0;
	size_t lor = 0;
	size_t lsui = 0;
	size_t lse128 = 0;
	size_t the = 0;
	size_t rcpc = 0;

	entries = tcti_lse_operation_catalog(&count);
	EXPECT(entries != NULL);
	EXPECT(count == TCTI_LSE_OPERATION_CATALOG_DIRECT_LEAF_COUNT);
	EXPECT(tcti_lse_operation_catalog_validate(
		entries, count, tcti_lse_operation_catalog_lse2_blocker(),
		&error) == 0);
	for (index = 0; index < count; index++) {
		switch (entries[index].cohort) {
		case TCTI_LSE_OPERATION_COHORT_BASE_LSE: base_lse++; break;
		case TCTI_LSE_OPERATION_COHORT_LOR: lor++; break;
		case TCTI_LSE_OPERATION_COHORT_LSUI: lsui++; break;
		case TCTI_LSE_OPERATION_COHORT_LSE128: lse128++; break;
		case TCTI_LSE_OPERATION_COHORT_THE: the++; break;
		case TCTI_LSE_OPERATION_COHORT_RCPC: rcpc++; break;
		default: return -1;
		}
	}
	EXPECT(base_lse == 168U);
	EXPECT(lor == 8U);
	EXPECT(lsui == 64U);
	EXPECT(lse128 == 12U);
	EXPECT(the == 64U);
	EXPECT(rcpc == 41U);
	return 0;
}

static struct tcti_lse_operation_catalog_entry *copy_catalog(size_t *count)
{
	const struct tcti_lse_operation_catalog_entry *entries =
		tcti_lse_operation_catalog(count);
	struct tcti_lse_operation_catalog_entry *copy;

	if (!entries)
		return NULL;
	copy = malloc(*count * sizeof(*copy));
	if (copy)
		memcpy(copy, entries, *count * sizeof(*copy));
	return copy;
}

static int drift_and_missing_entries_fail_closed(void)
{
	struct tcti_lse_operation_catalog_entry *copy;
	enum tcti_lse_operation_catalog_error error;
	size_t count;

	copy = copy_catalog(&count);
	EXPECT(copy != NULL);
	copy[0].encoding_pattern ^= 1U;
	EXPECT(tcti_lse_operation_catalog_validate(
		copy, count, tcti_lse_operation_catalog_lse2_blocker(),
		&error) == -1);
	EXPECT(error == TCTI_LSE_OPERATION_CATALOG_STALE_FINGERPRINT);
	copy[0].encoding_pattern ^= 1U;
	copy[0].source_leaf = "not-a-pinned-source-leaf";
	EXPECT(tcti_lse_operation_catalog_validate(
		copy, count, tcti_lse_operation_catalog_lse2_blocker(),
		&error) == -1);
	EXPECT(error == TCTI_LSE_OPERATION_CATALOG_MISSING_LEAF);
	free(copy);
	return 0;
}

static int duplicate_and_count_drift_fail_closed(void)
{
	struct tcti_lse_operation_catalog_entry *copy;
	enum tcti_lse_operation_catalog_error error;
	size_t count;

	copy = copy_catalog(&count);
	EXPECT(copy != NULL);
	copy[1] = copy[0];
	EXPECT(tcti_lse_operation_catalog_validate(
		copy, count, tcti_lse_operation_catalog_lse2_blocker(),
		&error) == -1);
	EXPECT(error == TCTI_LSE_OPERATION_CATALOG_DUPLICATE_LEAF);
	EXPECT(tcti_lse_operation_catalog_validate(
		copy, count - 1, tcti_lse_operation_catalog_lse2_blocker(),
		&error) == -1);
	EXPECT(error == TCTI_LSE_OPERATION_CATALOG_BAD_COUNT);
	free(copy);
	return 0;
}

static int broad_base_lse_decoder_cannot_prove_extensions(void)
{
	struct tcti_lse_operation_catalog_entry *copy;
	enum tcti_lse_operation_catalog_error error;
	size_t count;
	size_t index;

	copy = copy_catalog(&count);
	EXPECT(copy != NULL);
	for (index = 0; index < count; index++)
		if (copy[index].cohort == TCTI_LSE_OPERATION_COHORT_LSE128)
			break;
	EXPECT(index != count);
	copy[index].implementation_status =
		TCTI_LSE_OPERATION_STRUCTURAL_DECODER_ONLY;
	copy[index].implementation_cohort =
		TCTI_LSE_OPERATION_COHORT_BASE_LSE;
	copy[index].proof_status = TCTI_LSE_OPERATION_PROOF_KUNIT;
	copy[index].proof_cohort = TCTI_LSE_OPERATION_COHORT_BASE_LSE;
	EXPECT(tcti_lse_operation_catalog_validate(
		copy, count, tcti_lse_operation_catalog_lse2_blocker(),
		&error) == -1);
	EXPECT(error == TCTI_LSE_OPERATION_CATALOG_CROSS_COHORT_IMPLEMENTATION);
	free(copy);
	return 0;
}

static int lse2_stays_a_supplemental_blocker(void)
{
	struct tcti_lse_operation_catalog_entry blocker =
		*tcti_lse_operation_catalog_lse2_blocker();
	const struct tcti_lse_operation_catalog_entry *entries;
	enum tcti_lse_operation_catalog_error error;
	size_t count;

	entries = tcti_lse_operation_catalog(&count);
	EXPECT(entries != NULL);
	blocker.direct_source_leaf = true;
	EXPECT(tcti_lse_operation_catalog_validate(entries, count, &blocker,
		&error) == -1);
	EXPECT(error == TCTI_LSE_OPERATION_CATALOG_INVALID_SUPPLEMENTAL_BLOCKER);
	return 0;
}

int main(void)
{
	static const struct {
		const char *name;
		int (*run)(void);
	} tests[] = {
		{ "valid_catalog_has_exact_family_counts", valid_catalog_has_exact_family_counts },
		{ "drift_and_missing_entries_fail_closed", drift_and_missing_entries_fail_closed },
		{ "duplicate_and_count_drift_fail_closed", duplicate_and_count_drift_fail_closed },
		{ "broad_base_lse_decoder_cannot_prove_extensions", broad_base_lse_decoder_cannot_prove_extensions },
		{ "lse2_stays_a_supplemental_blocker", lse2_stays_a_supplemental_blocker },
	};
	size_t index;

	for (index = 0; index < sizeof(tests) / sizeof(tests[0]); index++) {
		if (tests[index].run())
			return 1;
		printf("PASS %s\n", tests[index].name);
	}
	return 0;
}
