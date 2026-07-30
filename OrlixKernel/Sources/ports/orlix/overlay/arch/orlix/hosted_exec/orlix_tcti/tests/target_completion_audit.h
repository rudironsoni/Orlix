/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TARGET_COMPLETION_AUDIT_H
#define ORLIX_TCTI_TARGET_COMPLETION_AUDIT_H

#ifdef __KERNEL__
#include <linux/stddef.h>
#include <linux/types.h>
typedef u32 orlix_tcti_completion_u32;
typedef u64 orlix_tcti_completion_u64;
#define ORLIX_TCTI_COMPLETION_U64_C(value) value##ULL
#else
#include <stddef.h>
#include <stdint.h>
typedef uint32_t orlix_tcti_completion_u32;
typedef uint64_t orlix_tcti_completion_u64;
#define ORLIX_TCTI_COMPLETION_U64_C(value) UINT64_C(value)
#endif

#include "target_proof_registry.h"
#include "target_feature_domain.h"
#include "target_feature_applicability_artifact.h"
#include "target_feature_field_domain_binding_artifact.h"
#include "target_runtime_capability_cohort_artifact.h"

#define ORLIX_TCTI_TARGET_COMPLETION_SOURCE_ROWS 4350U
#define ORLIX_TCTI_TARGET_COMPLETION_SOURCE_BYTE_LENGTH ORLIX_TCTI_COMPLETION_U64_C(115441429)
#define ORLIX_TCTI_TARGET_COMPLETION_SYSTEM_ACCESSOR_ROWS 2014U
#define ORLIX_TCTI_TARGET_COMPLETION_REGISTERS_BYTE_LENGTH ORLIX_TCTI_COMPLETION_U64_C(96016602)

enum orlix_tcti_target_completion_class {
	ORLIX_TCTI_TARGET_COMPLETION_UNCLASSIFIED,
	ORLIX_TCTI_TARGET_COMPLETION_REQUIRED_EL0,
	ORLIX_TCTI_TARGET_COMPLETION_NON_EL0,
	ORLIX_TCTI_TARGET_COMPLETION_ARCH_UNDEFINED_OR_UNALLOCATED,
	ORLIX_TCTI_TARGET_COMPLETION_ALIAS_OR_DUPLICATE,
};

enum orlix_tcti_target_completion_relation {
	ORLIX_TCTI_TARGET_COMPLETION_RELATION_NONE,
	ORLIX_TCTI_TARGET_COMPLETION_RELATION_ALIAS,
	ORLIX_TCTI_TARGET_COMPLETION_RELATION_DUPLICATE,
};

/*
 * A derived per-source-leaf status.  These bits report only a missing or
 * unresolved relationship in a checked canonical artifact.  They never
 * imply an implementation owner, semantic behavior, or executed proof.
 */
enum orlix_tcti_target_completion_obligation_blocker {
	ORLIX_TCTI_TARGET_COMPLETION_BLOCKER_NONE = 0,
	ORLIX_TCTI_TARGET_COMPLETION_BLOCKER_UNCLASSIFIED = 1U << 0,
	ORLIX_TCTI_TARGET_COMPLETION_BLOCKER_RELATIONSHIP = 1U << 1,
	ORLIX_TCTI_TARGET_COMPLETION_BLOCKER_SEMANTIC_PROVENANCE = 1U << 2,
	ORLIX_TCTI_TARGET_COMPLETION_BLOCKER_FEATURE_UNION = 1U << 3,
	ORLIX_TCTI_TARGET_COMPLETION_BLOCKER_PROOF = 1U << 4,
	ORLIX_TCTI_TARGET_COMPLETION_BLOCKER_UNPROVED_OBLIGATIONS = 1U << 5,
	ORLIX_TCTI_TARGET_COMPLETION_BLOCKER_RUNTIME_CANDIDATE = 1U << 6,
	ORLIX_TCTI_TARGET_COMPLETION_BLOCKER_FEATURE_UNION_INCOMPLETE = 1U << 7,
	ORLIX_TCTI_TARGET_COMPLETION_BLOCKER_EXECUTION_EVIDENCE = 1U << 8,
	/* The aggregate result carries artifact-wide failures separately. */
	ORLIX_TCTI_TARGET_COMPLETION_BLOCKER_SOURCE_CONDITION = 1U << 9,
};

enum orlix_tcti_a64_semantic_provenance_disposition {
	ORLIX_TCTI_A64_SEMANTIC_PROVENANCE_EXTERNAL_DDI0602,
	ORLIX_TCTI_A64_SEMANTIC_PROVENANCE_OFFICIAL_SEMANTICS_NOT_SPECIFIED,
};

struct orlix_tcti_target_completion_semantic_provenance_row;

enum orlix_tcti_target_completion_feature_union_state {
	ORLIX_TCTI_TARGET_COMPLETION_FEATURE_UNION_INVALID,
	ORLIX_TCTI_TARGET_COMPLETION_FEATURE_UNION_MISSING_CONFIGURATION,
	ORLIX_TCTI_TARGET_COMPLETION_FEATURE_UNION_MISSING_OPERAND,
	ORLIX_TCTI_TARGET_COMPLETION_FEATURE_UNION_UNRESOLVED,
	ORLIX_TCTI_TARGET_COMPLETION_FEATURE_UNION_EVALUATED,
};

/* More than one source condition may be incomplete for one leaf. */
enum orlix_tcti_target_completion_feature_union_reason {
	ORLIX_TCTI_TARGET_COMPLETION_FEATURE_UNION_REASON_NONE = 0,
	ORLIX_TCTI_TARGET_COMPLETION_FEATURE_UNION_REASON_MISSING_FEATURE = 1U << 0,
	ORLIX_TCTI_TARGET_COMPLETION_FEATURE_UNION_REASON_MISSING_OPERAND = 1U << 1,
	ORLIX_TCTI_TARGET_COMPLETION_FEATURE_UNION_REASON_INCOMPLETE = 1U << 2,
	ORLIX_TCTI_TARGET_COMPLETION_FEATURE_UNION_REASON_INVALID = 1U << 3,
};

enum orlix_tcti_target_completion_proof_state {
	ORLIX_TCTI_TARGET_COMPLETION_PROOF_NONE,
	ORLIX_TCTI_TARGET_COMPLETION_PROOF_STALE,
	ORLIX_TCTI_TARGET_COMPLETION_PROOF_SOURCE_BOUND_UNPROVED,
	/* Static registry metadata has no outstanding bits, never execution credit. */
	ORLIX_TCTI_TARGET_COMPLETION_PROOF_SOURCE_BOUND_NO_UNPROVED_METADATA,
};

enum orlix_tcti_target_completion_runtime_candidate_state {
	ORLIX_TCTI_TARGET_COMPLETION_RUNTIME_CANDIDATE_NONE,
	ORLIX_TCTI_TARGET_COMPLETION_RUNTIME_CANDIDATE_UNRESOLVED,
};

struct orlix_tcti_target_completion_source_row {
	orlix_tcti_completion_u32 ordinal;
	const char *name;
	const char *mnemonic;
	const char *operation_id;
	orlix_tcti_completion_u32 mask;
	orlix_tcti_completion_u32 pattern;
	const char *condition_tcnd_hex;
	orlix_tcti_completion_u64 source_offset;
	orlix_tcti_completion_u64 source_length;
};

/* Pinned Arm-source identity carried by the canonical C manifest. */
struct orlix_tcti_target_completion_source_provenance {
	const char *architecture;
	const char *build;
	const char *release;
	const char *schema;
	const char *timestamp;
	const char *source_sha256;
	orlix_tcti_completion_u64 source_byte_length;
	size_t leaf_count;
};

struct orlix_tcti_target_completion_classification_row {
	const char *name;
	enum orlix_tcti_target_completion_class classification;
	enum orlix_tcti_target_completion_relation relation;
	const char *canonical_name;
	const char *evidence;
	const char *proof_id;
};

/*
 * Optional ordinal-indexed projection of the canonical completion audit.
 * All pointers borrow immutable canonical artifact storage.  A NULL owner is
 * intentionally not represented by a placeholder: this audit does not own
 * per-leaf implementation assignment.
 */
struct orlix_tcti_target_completion_obligation {
	orlix_tcti_completion_u32 ordinal;
	orlix_tcti_completion_u32 blocker_mask;
	const char *name;
	const char *mnemonic;
	const char *operation_id;
	enum orlix_tcti_target_completion_class classification;
	enum orlix_tcti_target_completion_relation relation;
	const char *canonical_name;
	enum orlix_tcti_a64_semantic_provenance_disposition
		semantic_provenance_disposition;
	const struct orlix_tcti_target_completion_semantic_provenance_row
		*semantic_provenance_row;
	const char *semantic_relative_file;
	const char *semantic_decode_locator;
	const char *semantic_decode_sha256;
	const char *semantic_execute_locator;
	const char *semantic_execute_sha256;
	const char *aarchmrs_operation_locator;
	const char *aarchmrs_operation_sha256;
	enum orlix_tcti_target_completion_feature_union_state feature_union_state;
	/* Diagnostic order is retained only as a first-failure locator. */
	enum orlix_tcti_feature_domain_tcnd_error first_unsupported_error;
	/* First failure plus incompleteness only. This is not an exhaustive cause set. */
	orlix_tcti_completion_u32 known_feature_union_reason_mask;
	const char *proof_id;
	enum orlix_tcti_target_completion_proof_state proof_state;
	orlix_tcti_completion_u32 required_obligations;
	orlix_tcti_completion_u32 unproved_obligations;
	const char *kunit_source;
	const char *kunit_suite;
	const struct orlix_tcti_target_kselftest_provenance *kselftest;
	enum orlix_tcti_target_completion_runtime_candidate_state runtime_candidate_state;
	orlix_tcti_completion_u32 runtime_candidate_first;
	orlix_tcti_completion_u32 runtime_candidate_count;
};

/* External Arm source identity. It is provenance only and grants no credit. */
struct orlix_tcti_target_completion_semantic_provenance {
	const char *identity;
};

struct orlix_tcti_target_completion_semantic_provenance_row {
	orlix_tcti_completion_u32 ordinal;
	const char *name;
	enum orlix_tcti_a64_semantic_provenance_disposition disposition;
	const char *relative_file;
	const char *decode_locator;
	const char *decode_sha256;
	orlix_tcti_completion_u32 decode_section_count;
	orlix_tcti_completion_u32 decode_helper_count;
	const char *decode_helper_closure_sha256;
	const char *execute_locator;
	const char *execute_sha256;
	orlix_tcti_completion_u32 execute_section_count;
	orlix_tcti_completion_u32 execute_helper_count;
	const char *execute_helper_closure_sha256;
	const char *aarchmrs_operation_locator;
	orlix_tcti_completion_u64 aarchmrs_operation_source_offset;
	orlix_tcti_completion_u64 aarchmrs_operation_source_length;
	const char *aarchmrs_operation_sha256;
};

enum orlix_tcti_target_completion_system_accessor_direction {
	ORLIX_TCTI_TARGET_COMPLETION_ACCESSOR_DIRECTION_NONE,
	ORLIX_TCTI_TARGET_COMPLETION_ACCESSOR_DIRECTION_READ,
	ORLIX_TCTI_TARGET_COMPLETION_ACCESSOR_DIRECTION_WRITE,
	ORLIX_TCTI_TARGET_COMPLETION_ACCESSOR_DIRECTION_EXECUTE,
};

enum orlix_tcti_target_completion_system_accessor_disposition {
	ORLIX_TCTI_TARGET_COMPLETION_ACCESSOR_MAPPED,
	ORLIX_TCTI_TARGET_COMPLETION_ACCESSOR_RESERVED,
	ORLIX_TCTI_TARGET_COMPLETION_ACCESSOR_PRIVILEGED,
	ORLIX_TCTI_TARGET_COMPLETION_ACCESSOR_UNSUPPORTED,
	ORLIX_TCTI_TARGET_COMPLETION_ACCESSOR_AMBIGUOUS,
	ORLIX_TCTI_TARGET_COMPLETION_ACCESSOR_CONTRADICTORY,
	ORLIX_TCTI_TARGET_COMPLETION_ACCESSOR_INVALID,
};

enum orlix_tcti_target_completion_system_accessor_applicability {
	ORLIX_TCTI_TARGET_COMPLETION_ACCESSOR_EL0_BEHAVIOR_REQUIRED = 1,
};

enum orlix_tcti_target_completion_system_accessor_semantics {
	ORLIX_TCTI_TARGET_COMPLETION_ACCESSOR_SOURCE_ACCESS_SEMANTICS = 1,
	ORLIX_TCTI_TARGET_COMPLETION_ACCESSOR_GENERIC_LEAF_SEMANTICS,
};

enum orlix_tcti_target_completion_system_accessor_implementation {
	ORLIX_TCTI_TARGET_COMPLETION_ACCESSOR_IMPLEMENTED = 1,
	ORLIX_TCTI_TARGET_COMPLETION_ACCESSOR_ARCHITECTURAL_REJECTION,
	ORLIX_TCTI_TARGET_COMPLETION_ACCESSOR_UNIMPLEMENTED_REJECTION,
};

enum orlix_tcti_target_completion_system_accessor_proof_state {
	ORLIX_TCTI_TARGET_COMPLETION_ACCESSOR_PROOF_NOT_OBSERVED = 1,
};

struct orlix_tcti_target_completion_system_accessor_provenance {
	const char *architecture;
	const char *build;
	const char *release;
	const char *schema;
	const char *timestamp;
	const char *source_sha256;
	size_t accessor_count;
	size_t mapped_count;
	size_t reserved_count;
	size_t privileged_count;
	size_t unsupported_count;
	size_t ambiguous_count;
	size_t contradictory_count;
	size_t invalid_count;
	size_t semantic_count;
	size_t source_access_semantics_count;
	size_t generic_leaf_semantics_count;
	size_t implemented_count;
	size_t architectural_rejection_count;
	size_t unimplemented_rejection_count;
	size_t concrete_selector_count;
	size_t symbolic_selector_count;
	size_t proof_not_observed_count;
	orlix_tcti_completion_u64 reconciliation_identity;
};

struct orlix_tcti_target_completion_system_accessor_row {
	orlix_tcti_completion_u32 accessor_index;
	orlix_tcti_completion_u32 encoding_index;
	const char *name;
	const char *variant_name;
	const char *generic_leaf;
	enum orlix_tcti_target_completion_system_accessor_direction direction;
	enum orlix_tcti_target_completion_system_accessor_disposition disposition;
	orlix_tcti_completion_u32 selector_count;
	orlix_tcti_completion_u32 condition_expression;
	orlix_tcti_completion_u32 access_expression;
	orlix_tcti_completion_u32 concrete_selector;
	enum orlix_tcti_target_completion_system_accessor_applicability applicability;
	enum orlix_tcti_target_completion_system_accessor_semantics semantics;
	enum orlix_tcti_target_completion_system_accessor_implementation implementation;
	enum orlix_tcti_target_completion_system_accessor_proof_state proof_state;
	orlix_tcti_completion_u64 selector_identity;
	orlix_tcti_completion_u64 condition_identity;
	orlix_tcti_completion_u64 access_identity;
	orlix_tcti_completion_u64 decode_key;
	orlix_tcti_completion_u32 execution_operation;
	const char *decoder_owner;
	const char *execution_owner;
	const char *kunit_suite;
	const char *kunit_case;
	orlix_tcti_completion_u64 accessor_source_offset;
	orlix_tcti_completion_u64 accessor_source_length;
	orlix_tcti_completion_u64 encoding_source_offset;
	orlix_tcti_completion_u64 encoding_source_length;
	orlix_tcti_completion_u64 condition_source_offset;
	orlix_tcti_completion_u64 condition_source_length;
	orlix_tcti_completion_u64 access_source_offset;
	orlix_tcti_completion_u64 access_source_length;
};

enum orlix_tcti_target_completion_error {
	ORLIX_TCTI_TARGET_COMPLETION_ERROR_NONE = 0,
	ORLIX_TCTI_TARGET_COMPLETION_ERROR_SOURCE_COUNT = 1U << 0,
	ORLIX_TCTI_TARGET_COMPLETION_ERROR_CLASSIFICATION_COUNT = 1U << 1,
	ORLIX_TCTI_TARGET_COMPLETION_ERROR_SOURCE = 1U << 2,
	ORLIX_TCTI_TARGET_COMPLETION_ERROR_ABSENT = 1U << 3,
	ORLIX_TCTI_TARGET_COMPLETION_ERROR_STALE = 1U << 4,
	ORLIX_TCTI_TARGET_COMPLETION_ERROR_UNCLASSIFIED = 1U << 5,
	ORLIX_TCTI_TARGET_COMPLETION_ERROR_SOURCE_BINDING = 1U << 6,
	ORLIX_TCTI_TARGET_COMPLETION_ERROR_RELATIONSHIP = 1U << 7,
	ORLIX_TCTI_TARGET_COMPLETION_ERROR_PROOF_REGISTRY = 1U << 8,
	ORLIX_TCTI_TARGET_COMPLETION_ERROR_STALE_PROOF_BINDING = 1U << 9,
	ORLIX_TCTI_TARGET_COMPLETION_ERROR_SOURCE_PROVENANCE = 1U << 10,
	ORLIX_TCTI_TARGET_COMPLETION_ERROR_FEATURE_DOMAIN = 1U << 11,
	ORLIX_TCTI_TARGET_COMPLETION_ERROR_FEATURE_APPLICABILITY = 1U << 12,
	ORLIX_TCTI_TARGET_COMPLETION_ERROR_UNPROVED_OBLIGATIONS = 1U << 13,
	ORLIX_TCTI_TARGET_COMPLETION_ERROR_SEMANTIC_PROVENANCE = 1U << 14,
	ORLIX_TCTI_TARGET_COMPLETION_ERROR_SYSTEM_ACCESSOR = 1U << 15,
	/*
	 * Every checked AST.FIELD occurrence must bind to an authoritative
	 * Registers.json domain. Mapped rows remain blocking until their domain
	 * semantics are evaluated, and ambiguity is never silently resolved.
	 */
	ORLIX_TCTI_TARGET_COMPLETION_ERROR_FEATURE_FIELD_DOMAIN = 1U << 16,
	/* Feature-conditioned leaves remain blocking until each cohort is proved. */
	ORLIX_TCTI_TARGET_COMPLETION_ERROR_RUNTIME_CAPABILITY_COHORT = 1U << 17,
	ORLIX_TCTI_TARGET_COMPLETION_ERROR_LINUX_PROOF_MATRIX = 1U << 18,
	ORLIX_TCTI_TARGET_COMPLETION_ERROR_OFFICIAL_SEMANTICS_NOT_SPECIFIED = 1U << 19,
	ORLIX_TCTI_TARGET_COMPLETION_ERROR_OPERATIONAL_NOTE = 1U << 20,
};

struct orlix_tcti_target_completion_result {
	orlix_tcti_completion_u32 error_mask;
	size_t errors;
	size_t source_rows;
	size_t classification_rows;
	size_t classified_rows;
	size_t unclassified_rows;
	size_t required_el0_rows;
	size_t non_el0_rows;
	size_t undefined_or_unallocated_rows;
	size_t alias_or_duplicate_rows;
	size_t absent_rows;
	size_t stale_rows;
	/* Static registry bindings only. Native test execution owns proof status. */
	size_t source_bound_rows;
	size_t source_unbound_rows;
	size_t invalid_relationship_rows;
	size_t invalid_source_rows;
	size_t invalid_source_provenance;
	size_t invalid_registry_entries;
	size_t stale_proof_bindings;
	/* Required duties still lacking native execution evidence. */
	size_t unproved_obligation_bindings;
	/* Authored operational notes are additional, never substitute, obligations. */
	size_t operational_note_rows;
	size_t mapped_operational_note_rows;
	size_t invalid_operational_note_mappings;
	size_t unproved_operational_note_rows;
	/* Source conditions that bind to the checked Arm feature-domain artifact. */
	size_t source_condition_domain_bound_rows;
	size_t invalid_source_condition_rows;
	/* No leaf may be treated as applicable without an exact union result. */
	size_t unresolved_feature_applicability_rows;
	size_t evaluated_feature_applicability_rows;
	size_t satisfied_feature_applicability_rows;
	size_t unsatisfied_feature_applicability_rows;
	size_t unsupported_feature_applicability_rows;
	/* The TCND evaluator records why an otherwise valid source row is blocked. */
	size_t unresolved_feature_configuration_rows;
	size_t unresolved_instruction_operand_rows;
	size_t invalid_feature_applicability_rows;
	size_t invalid_feature_applicability_artifact;
	size_t invalid_feature_artifact;
	/* External semantic provenance is source identity only and grants no credit. */
	size_t semantic_provenance_rows;
	size_t external_ddi0602_semantic_provenance_rows;
	size_t official_semantics_not_specified_rows;
	size_t missing_semantic_provenance_rows;
	size_t stale_semantic_provenance_rows;
	size_t incompatible_semantic_provenance_rows;
	size_t malformed_semantic_provenance_rows;
	size_t dangling_semantic_provenance_rows;
	size_t ambiguous_semantic_provenance_rows;
	/* Zero means none; otherwise this is the first invalid ordinal plus one. */
	size_t first_invalid_semantic_provenance_ordinal_plus_one;
	/* Supplemental Registers.json relationships never alter source_rows. */
	size_t system_accessor_rows;
	size_t mapped_system_accessor_rows;
	size_t nonmapped_system_accessor_rows;
	size_t invalid_system_accessor_rows;
	size_t implemented_system_accessor_rows;
	size_t rejected_system_accessor_rows;
	size_t unobserved_system_accessor_proof_rows;
	/* Checked feature AST.FIELD-to-register-domain relationships. */
	size_t feature_field_domain_rows;
	size_t mapped_feature_field_domain_rows;
	/* Structurally mapped rows whose value-domain semantics are not proved. */
	size_t unresolved_feature_field_domain_rows;
	/* Authoritative ambiguity remains distinct from unresolved mapping. */
	size_t ambiguous_feature_field_domain_rows;
	size_t invalid_feature_field_domain_rows;
	/* Checked feature-conditioned runtime capability candidates. */
	size_t runtime_capability_cohort_leaf_rows;
	size_t runtime_capability_cohort_candidate_membership_rows;
	size_t unresolved_runtime_capability_cohort_membership_rows;
	size_t invalid_runtime_capability_cohort_rows;
	/* Static ownership dispositions only. Executed kselftest proof stays zero. */
	size_t linux_proof_rows;
	size_t linux_proof_source_leaf_rows;
	size_t linux_proof_semantic_variant_rows;
	size_t linux_proof_kselftest_owned_rows;
	size_t linux_proof_not_applicable_rows;
	size_t linux_proof_executed_rows;
	size_t missing_linux_proof_rows;
	size_t duplicate_linux_proof_rows;
	size_t stale_linux_proof_rows;
	size_t malformed_linux_proof_rows;
	size_t ambiguous_linux_proof_rows;
	size_t invalid_linux_proof_provenance_rows;
	size_t linux_proof_substitution_rows;
};

/* Host-test-only dependency injection. It is not a kernel or runtime ABI. */
struct orlix_tcti_target_completion_audit_inputs_for_test {
	const struct orlix_tcti_target_completion_source_row *source;
	size_t source_count;
	const struct orlix_tcti_target_completion_classification_row *classification;
	size_t classification_count;
	const struct orlix_tcti_target_proof_registry_entry *registry;
	size_t registry_count;
	const struct orlix_tcti_target_operational_note_proof_mapping
		*operational_note_mappings;
	size_t operational_note_mapping_count;
	/* Optional host-only checked artifact injection for atomicity tests. */
	const struct orlix_tcti_target_instruction_artifact *instruction_artifact;
	const struct orlix_tcti_target_feature_applicability_artifact *feature_applicability;
	const struct orlix_tcti_target_linux_proof_disposition_row *linux_proof;
	size_t linux_proof_count;
	const struct orlix_tcti_target_completion_semantic_provenance
		*semantic_provenance;
	const struct orlix_tcti_target_completion_semantic_provenance_row
		*semantic_provenance_rows;
	size_t semantic_provenance_count;
};

int orlix_tcti_target_completion_validate(
	const struct orlix_tcti_target_completion_source_row *source,
	size_t source_count,
	const struct orlix_tcti_target_completion_classification_row *classification,
	size_t classification_count,
	const struct orlix_tcti_target_proof_registry_entry *registry,
	size_t registry_count,
	struct orlix_tcti_target_completion_result *result);

int orlix_tcti_target_completion_audit(struct orlix_tcti_target_completion_result *result);

/*
 * Run the canonical aggregate audit and, when requested, derive exactly one
 * immutable obligation row for every source ordinal.  A projection is valid
 * only with a full 4,350-row caller buffer.  NULL with count zero requests no
 * projection.
 */
int orlix_tcti_target_completion_audit_with_obligations(
	struct orlix_tcti_target_completion_result *result,
	struct orlix_tcti_target_completion_obligation *obligations,
	size_t obligation_count);

int orlix_tcti_target_completion_audit_with_inputs_for_test(
	const struct orlix_tcti_target_completion_audit_inputs_for_test *inputs,
	struct orlix_tcti_target_completion_result *result,
	struct orlix_tcti_target_completion_obligation *obligations,
	size_t obligation_count);

int orlix_tcti_target_completion_validate_source_provenance(
	const struct orlix_tcti_target_completion_source_provenance *provenance,
	struct orlix_tcti_target_completion_result *result);

int orlix_tcti_target_completion_validate_semantic_provenance(
	const struct orlix_tcti_target_completion_source_row *source,
	size_t source_count,
	const struct orlix_tcti_target_completion_semantic_provenance *provenance,
	const struct orlix_tcti_target_completion_semantic_provenance_row *rows,
	size_t row_count,
	struct orlix_tcti_target_completion_result *result);

int orlix_tcti_target_completion_validate_system_accessors(
	const struct orlix_tcti_target_completion_source_row *source,
	size_t source_count,
	const struct orlix_tcti_target_completion_system_accessor_provenance *provenance,
	const struct orlix_tcti_target_completion_system_accessor_row *accessors,
	size_t accessor_count,
	struct orlix_tcti_target_completion_result *result);

int orlix_tcti_target_completion_validate_feature_field_domains(
	const struct orlix_tcti_feature_artifact *feature_artifact,
	const struct orlix_tcti_feature_field_domain_binding_artifact *artifact,
	struct orlix_tcti_target_completion_result *result);

int orlix_tcti_target_completion_validate_runtime_capability_cohorts(
	const struct orlix_tcti_runtime_capability_cohort_artifact *artifact,
	struct orlix_tcti_target_completion_result *result);

const struct orlix_tcti_target_completion_source_provenance *
orlix_tcti_target_completion_source_provenance(void);

const struct orlix_tcti_target_completion_semantic_provenance *
orlix_tcti_target_completion_semantic_provenance(void);

const struct orlix_tcti_target_completion_semantic_provenance_row *
orlix_tcti_target_completion_semantic_provenance_rows(size_t *count);

const struct orlix_tcti_target_completion_system_accessor_provenance *
orlix_tcti_target_completion_system_accessor_provenance(void);

const struct orlix_tcti_target_completion_system_accessor_row *
orlix_tcti_target_completion_system_accessors(size_t *count);

const struct orlix_tcti_target_completion_source_row *
orlix_tcti_target_completion_source(size_t *count);

const struct orlix_tcti_target_completion_classification_row *
orlix_tcti_target_completion_classification(size_t *count);

#endif /* ORLIX_TCTI_TARGET_COMPLETION_AUDIT_H */
