/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_completion_audit.h"

#include <stdio.h>

int main(void)
{
	struct orlix_tcti_target_completion_result result;
	int status = orlix_tcti_target_completion_audit(&result);

	printf("target_completion source=%zu classification=%zu "
	       "classified=%zu unclassified=%zu absent=%zu stale=%zu "
	       "required_el0=%zu non_el0=%zu undefined_or_unallocated=%zu "
	       "alias_or_duplicate=%zu "
	       "source_bound=%zu source_binding_failures=%zu "
	       "invalid_relationships=%zu "
	       "invalid_source=%zu invalid_source_provenance=%zu "
	       "invalid_registry=%zu "
	       "stale_proof_bindings=%zu unproved_obligation_bindings=%zu "
	       "source_condition_domain_bound=%zu "
	       "invalid_source_conditions=%zu "
		"unresolved_feature_applicability=%zu "
		"evaluated_feature_applicability=%zu "
		"satisfied_feature_applicability=%zu "
		"unsatisfied_feature_applicability=%zu "
		"unsupported_feature_applicability=%zu "
		"missing_feature_configuration=%zu "
		"missing_instruction_operand=%zu "
		"invalid_feature_applicability=%zu "
	       "invalid_feature_artifact=%zu "
	       "asl_availability=%zu invalid_asl_availability=%zu "
	       "unavailable_asl=%zu "
	       "system_accessors=%zu mapped_system_accessors=%zu "
	       "nonmapped_system_accessors=%zu "
	       "invalid_system_accessors=%zu "
	       "feature_field_domains=%zu mapped_feature_field_domains=%zu "
	       "unresolved_feature_field_domains=%zu "
	       "ambiguous_feature_field_domains=%zu "
	       "invalid_feature_field_domains=%zu "
	       "runtime_capability_cohort_leaves=%zu "
	       "runtime_capability_cohort_candidates=%zu "
	       "unresolved_runtime_capability_cohorts=%zu "
	       "invalid_runtime_capability_cohorts=%zu "
	       "errors=%zu error_mask=0x%08x\n",
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
	       result.unproved_obligation_bindings,
	       result.source_condition_domain_bound_rows,
	       result.invalid_source_condition_rows,
		result.unresolved_feature_applicability_rows,
		result.evaluated_feature_applicability_rows,
		result.satisfied_feature_applicability_rows,
		result.unsatisfied_feature_applicability_rows,
		result.unsupported_feature_applicability_rows,
		result.unresolved_feature_configuration_rows,
		result.unresolved_instruction_operand_rows,
		result.invalid_feature_applicability_rows,
	       result.invalid_feature_artifact,
	       result.asl_availability_rows,
	       result.invalid_asl_availability_rows,
	       result.unavailable_asl_rows,
	       result.system_accessor_rows,
	       result.mapped_system_accessor_rows,
	       result.nonmapped_system_accessor_rows,
	       result.invalid_system_accessor_rows,
	       result.feature_field_domain_rows,
	       result.mapped_feature_field_domain_rows,
	       result.unresolved_feature_field_domain_rows,
	       result.ambiguous_feature_field_domain_rows,
	       result.invalid_feature_field_domain_rows,
	       result.runtime_capability_cohort_leaf_rows,
	       result.runtime_capability_cohort_candidate_membership_rows,
	       result.unresolved_runtime_capability_cohort_membership_rows,
	       result.invalid_runtime_capability_cohort_rows,
	       result.errors,
	       result.error_mask);
	return status ? 1 : 0;
}
