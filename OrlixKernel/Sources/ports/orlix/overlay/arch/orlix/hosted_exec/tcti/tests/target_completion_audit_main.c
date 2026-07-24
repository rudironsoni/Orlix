/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_completion_audit.h"

#include <stdio.h>

int main(void)
{
	struct tcti_target_completion_result result;
	int status = tcti_target_completion_audit(&result);

	printf("target_completion source=%zu classification=%zu "
	       "classified=%zu unclassified=%zu absent=%zu stale=%zu "
	       "required_el0=%zu non_el0=%zu undefined_or_unallocated=%zu "
	       "alias_or_duplicate=%zu "
	       "proved=%zu unproved=%zu invalid_relationships=%zu "
	       "invalid_source=%zu invalid_source_provenance=%zu "
	       "invalid_registry=%zu "
	       "stale_proof_bindings=%zu errors=%zu error_mask=0x%08x\n",
	       result.source_rows, result.classification_rows,
	       result.classified_rows, result.unclassified_rows,
	       result.absent_rows, result.stale_rows,
	       result.required_el0_rows, result.non_el0_rows,
	       result.undefined_or_unallocated_rows,
	       result.alias_or_duplicate_rows, result.proved_rows,
	       result.unproved_rows, result.invalid_relationship_rows,
	       result.invalid_source_rows, result.invalid_source_provenance,
	       result.invalid_registry_entries,
	       result.stale_proof_bindings, result.errors,
	       result.error_mask);
	return status ? 1 : 0;
}
