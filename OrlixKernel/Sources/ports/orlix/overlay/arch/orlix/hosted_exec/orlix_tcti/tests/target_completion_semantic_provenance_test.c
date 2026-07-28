/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_completion_audit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EXPECT(condition) do { \
	if (!(condition)) { \
		fprintf(stderr, "semantic provenance expectation failed at line %d\n", \
			__LINE__); \
		return 1; \
	} \
} while (0)

static int validate_canonical_contract(
	const struct orlix_tcti_target_completion_source_row *source,
	size_t source_count,
	const struct orlix_tcti_target_completion_semantic_provenance *provenance,
	const struct orlix_tcti_target_completion_semantic_provenance_row *rows,
	size_t row_count)
{
	struct orlix_tcti_target_completion_result result = { 0 };

	EXPECT(orlix_tcti_target_completion_validate_semantic_provenance(
		       source, source_count, provenance, rows, row_count, &result) == -1);
	if (result.semantic_provenance_rows != 4350U)
		fprintf(stderr,
			"semantic provenance rows=%zu ddi=%zu unspecified=%zu "
			"missing=%zu stale=%zu incompatible=%zu malformed=%zu "
			"dangling=%zu ambiguous=%zu "
			"first_invalid_plus_one=%zu mask=0x%08x\n",
			result.semantic_provenance_rows,
			result.external_ddi0602_semantic_provenance_rows,
			result.official_semantics_not_specified_rows,
			result.missing_semantic_provenance_rows,
			result.stale_semantic_provenance_rows,
			result.incompatible_semantic_provenance_rows,
			result.malformed_semantic_provenance_rows,
			result.dangling_semantic_provenance_rows,
			result.ambiguous_semantic_provenance_rows,
			result.first_invalid_semantic_provenance_ordinal_plus_one,
			result.error_mask);
	EXPECT(result.semantic_provenance_rows == 4350U);
	EXPECT(result.external_ddi0602_semantic_provenance_rows == 4332U);
	EXPECT(result.official_semantics_not_specified_rows == 18U);
	EXPECT(result.missing_semantic_provenance_rows == 0U);
	EXPECT(result.stale_semantic_provenance_rows == 0U);
	EXPECT(result.incompatible_semantic_provenance_rows == 0U);
	EXPECT(result.malformed_semantic_provenance_rows == 0U);
	EXPECT(result.dangling_semantic_provenance_rows == 0U);
	EXPECT(result.ambiguous_semantic_provenance_rows == 0U);
	EXPECT(!(result.error_mask &
		 ORLIX_TCTI_TARGET_COMPLETION_ERROR_SEMANTIC_PROVENANCE));
	EXPECT(result.error_mask &
	       ORLIX_TCTI_TARGET_COMPLETION_ERROR_OFFICIAL_SEMANTICS_NOT_SPECIFIED);
	/* Provenance validation never manufactures implementation or proof credit. */
	EXPECT(result.source_bound_rows == 0U);
	EXPECT(result.linux_proof_executed_rows == 0U);
	EXPECT(result.runtime_capability_cohort_candidate_membership_rows == 0U);
	return 0;
}

int main(void)
{
	const struct orlix_tcti_target_completion_source_row *source;
	const struct orlix_tcti_target_completion_semantic_provenance *provenance;
	const struct orlix_tcti_target_completion_semantic_provenance_row *rows;
	struct orlix_tcti_target_completion_semantic_provenance stale_provenance;
	struct orlix_tcti_target_completion_semantic_provenance_row *mutated;
	struct orlix_tcti_target_completion_result result;
	size_t source_count;
	size_t row_count;
	size_t ddi_ordinal;
	char *stale_identity;
	size_t identity_length;

	source = orlix_tcti_target_completion_source(&source_count);
	provenance = orlix_tcti_target_completion_semantic_provenance();
	rows = orlix_tcti_target_completion_semantic_provenance_rows(&row_count);
	EXPECT(source && provenance && rows);
	EXPECT(source_count == 4350U && row_count == 4350U);
	EXPECT(validate_canonical_contract(source, source_count, provenance, rows,
					   row_count) == 0);

	stale_provenance = *provenance;
	stale_provenance.identity = "stale-external-provenance";
	memset(&result, 0, sizeof(result));
	EXPECT(orlix_tcti_target_completion_validate_semantic_provenance(
		       source, source_count, &stale_provenance, rows, row_count,
		       &result) == -1);
	EXPECT(result.incompatible_semantic_provenance_rows == 1U);
	EXPECT(result.error_mask &
	       ORLIX_TCTI_TARGET_COMPLETION_ERROR_SEMANTIC_PROVENANCE);

	identity_length = strlen(provenance->identity);
	stale_identity = malloc(identity_length + 1U);
	EXPECT(stale_identity);
	memcpy(stale_identity, provenance->identity, identity_length + 1U);
	stale_identity[identity_length - 1U] =
		stale_identity[identity_length - 1U] == '8' ? '9' : '8';
	stale_provenance.identity = stale_identity;
	memset(&result, 0, sizeof(result));
	EXPECT(orlix_tcti_target_completion_validate_semantic_provenance(
		       source, source_count, &stale_provenance, rows, row_count,
		       &result) == -1);
	EXPECT(result.stale_semantic_provenance_rows == 1U);
	free(stale_identity);

	mutated = malloc(row_count * sizeof(*mutated));
	EXPECT(mutated);
	memcpy(mutated, rows, row_count * sizeof(*mutated));
	for (ddi_ordinal = 0; ddi_ordinal < row_count; ddi_ordinal++)
		if (mutated[ddi_ordinal].disposition ==
		    ORLIX_TCTI_A64_SEMANTIC_PROVENANCE_EXTERNAL_DDI0602)
			break;
	EXPECT(ddi_ordinal < row_count);
	mutated[ddi_ordinal].execute_sha256 = NULL;
	memset(&result, 0, sizeof(result));
	EXPECT(orlix_tcti_target_completion_validate_semantic_provenance(
		       source, source_count, provenance, mutated, row_count,
		       &result) == -1);
	EXPECT(result.malformed_semantic_provenance_rows == 1U);

	memcpy(mutated, rows, row_count * sizeof(*mutated));
	mutated[ddi_ordinal].execute_sha256 =
		"0000000000000000000000000000000000000000000000000000000000000000";
	memset(&result, 0, sizeof(result));
	EXPECT(orlix_tcti_target_completion_validate_semantic_provenance(
		       source, source_count, provenance, mutated, row_count,
		       &result) == -1);
	EXPECT(result.stale_semantic_provenance_rows == 1U);

	memcpy(mutated, rows, row_count * sizeof(*mutated));
	mutated[ddi_ordinal].name = "dangling-leaf";
	memset(&result, 0, sizeof(result));
	EXPECT(orlix_tcti_target_completion_validate_semantic_provenance(
		       source, source_count, provenance, mutated, row_count,
		       &result) == -1);
	EXPECT(result.dangling_semantic_provenance_rows == 1U);

	memcpy(mutated, rows, row_count * sizeof(*mutated));
	mutated[1] = mutated[0];
	memset(&result, 0, sizeof(result));
	EXPECT(orlix_tcti_target_completion_validate_semantic_provenance(
		       source, source_count, provenance, mutated, row_count,
		       &result) == -1);
	EXPECT(result.ambiguous_semantic_provenance_rows == 1U);
	EXPECT(result.missing_semantic_provenance_rows == 1U);

	memset(&result, 0, sizeof(result));
	EXPECT(orlix_tcti_target_completion_validate_semantic_provenance(
		       source, source_count, provenance, rows, row_count - 1U,
		       &result) == -1);
	EXPECT(result.missing_semantic_provenance_rows == 1U);

	free(mutated);
	return 0;
}
