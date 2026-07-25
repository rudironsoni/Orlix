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
	       "source_bound=%zu source_binding_failures=%zu "
	       "invalid_relationships=%zu "
	       "invalid_source=%zu invalid_source_provenance=%zu "
	       "invalid_registry=%zu "
	       "stale_proof_bindings=%zu source_condition_domain_bound=%zu "
	       "invalid_source_conditions=%zu "
	       "unresolved_feature_applicability=%zu "
	       "invalid_feature_artifact=%zu errors=%zu error_mask=0x%08x\n",
	       result.source_rows, result.classification_rows,
	       result.classified_rows, result.unclassified_rows,
	       result.absent_rows, result.stale_rows,
	       result.required_el0_rows, result.non_el0_rows,
	       result.undefined_or_unallocated_rows,
	       result.alias_or_duplicate_rows, result.source_bound_rows,
	       result.source_unbound_rows, result.invalid_relationship_rows,
	       result.invalid_source_rows, result.invalid_source_provenance,
	       result.invalid_registry_entries,
	       result.stale_proof_bindings,
	       result.source_condition_domain_bound_rows,
	       result.invalid_source_condition_rows,
	       result.unresolved_feature_applicability_rows,
	       result.invalid_feature_artifact, result.errors,
	       result.error_mask);
	return status ? 1 : 0;
}
