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
		"invalid_feature_applicability_artifact=%zu "
	       "invalid_feature_artifact=%zu "
		"semantic_provenance=%zu ddi0602_provenance=%zu "
		"official_semantics_not_specified=%zu "
		"missing_semantic_provenance=%zu stale_semantic_provenance=%zu "
		"incompatible_semantic_provenance=%zu "
		"malformed_semantic_provenance=%zu dangling_semantic_provenance=%zu "
		"ambiguous_semantic_provenance=%zu "
		"first_invalid_semantic_provenance_ordinal_plus_one=%zu "
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
	       "linux_proof_rows=%zu linux_proof_source_leaves=%zu "
	       "linux_proof_semantic_variants=%zu "
	       "linux_proof_kselftest_owned=%zu "
	       "linux_proof_not_applicable=%zu linux_proof_executed=%zu "
	       "missing_linux_proof=%zu duplicate_linux_proof=%zu "
	       "stale_linux_proof=%zu malformed_linux_proof=%zu "
	       "ambiguous_linux_proof=%zu "
	       "invalid_linux_proof_provenance=%zu "
	       "linux_proof_substitutions=%zu "
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
		result.invalid_feature_applicability_artifact,
	       result.invalid_feature_artifact,
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
	       result.linux_proof_rows,
	       result.linux_proof_source_leaf_rows,
	       result.linux_proof_semantic_variant_rows,
	       result.linux_proof_kselftest_owned_rows,
	       result.linux_proof_not_applicable_rows,
	       result.linux_proof_executed_rows,
	       result.missing_linux_proof_rows,
	       result.duplicate_linux_proof_rows,
	       result.stale_linux_proof_rows,
	       result.malformed_linux_proof_rows,
	       result.ambiguous_linux_proof_rows,
	       result.invalid_linux_proof_provenance_rows,
	       result.linux_proof_substitution_rows,
	       result.errors,
	       result.error_mask);
	return status ? 1 : 0;
}
