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
	ORLIX_TCTI_TARGET_COMPLETION_BLOCKER_ASL = 1U << 2,
	ORLIX_TCTI_TARGET_COMPLETION_BLOCKER_FEATURE_UNION = 1U << 3,
	ORLIX_TCTI_TARGET_COMPLETION_BLOCKER_PROOF = 1U << 4,
	ORLIX_TCTI_TARGET_COMPLETION_BLOCKER_UNPROVED_OBLIGATIONS = 1U << 5,
	ORLIX_TCTI_TARGET_COMPLETION_BLOCKER_RUNTIME_CANDIDATE = 1U << 6,
	ORLIX_TCTI_TARGET_COMPLETION_BLOCKER_FEATURE_UNION_INCOMPLETE = 1U << 7,
	ORLIX_TCTI_TARGET_COMPLETION_BLOCKER_EXECUTION_EVIDENCE = 1U << 8,
	/* The aggregate result carries artifact-wide failures separately. */
	ORLIX_TCTI_TARGET_COMPLETION_BLOCKER_SOURCE_CONDITION = 1U << 9,
};

enum orlix_tcti_target_completion_asl_state {
	ORLIX_TCTI_TARGET_COMPLETION_ASL_INVALID,
	ORLIX_TCTI_TARGET_COMPLETION_ASL_ABSENT_BLOCKING,
	ORLIX_TCTI_TARGET_COMPLETION_ASL_UNAVAILABLE,
};

enum orlix_tcti_target_completion_asl_body_state {
	ORLIX_TCTI_A64_ASL_BODY_ABSENT,
	ORLIX_TCTI_A64_ASL_BODY_PLACEHOLDER,
	ORLIX_TCTI_A64_ASL_BODY_PRESENT,
};

enum orlix_tcti_target_completion_asl_decode_state {
	ORLIX_TCTI_A64_ASL_DECODE_ABSENT,
	ORLIX_TCTI_A64_ASL_DECODE_NULL,
	ORLIX_TCTI_A64_ASL_DECODE_PRESENT,
};

enum orlix_tcti_target_completion_asl_corpus_state {
	ORLIX_TCTI_A64_ASL_CORPUS_ABSENT,
	ORLIX_TCTI_A64_ASL_CORPUS_PRESENT,
};

enum orlix_tcti_target_completion_asl_helper_state {
	ORLIX_TCTI_A64_ASL_HELPERS_UNAVAILABLE,
	ORLIX_TCTI_A64_ASL_HELPERS_AVAILABLE,
};

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
	const char *asl_operation_object;
	enum orlix_tcti_target_completion_asl_state asl_state;
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

/*
 * Provenance for the authoritative ASL relationship of every pinned source
 * leaf.  Availability is deliberately separate from implementation proof:
 * an absent shared ASL corpus blocks semantic-completeness claims.
 */
struct orlix_tcti_target_completion_asl_provenance {
	const char *format;
	const char *source_sha256;
	enum orlix_tcti_target_completion_asl_corpus_state corpus_state;
	enum orlix_tcti_target_completion_asl_helper_state helper_state;
};

struct orlix_tcti_target_completion_asl_row {
	orlix_tcti_completion_u32 ordinal;
	const char *name;
	const char *operation_id;
	const char *semantic_operation_id;
	const char *semantic_member_locator;
	orlix_tcti_completion_u32 semantic_member_source_offset;
	orlix_tcti_completion_u32 semantic_member_source_length;
	orlix_tcti_completion_u32 semantic_body_source_offset;
	orlix_tcti_completion_u32 semantic_body_source_length;
	const char *semantic_body_sha256;
	enum orlix_tcti_target_completion_asl_body_state semantic_body_state;
	const char *decode_member_locator;
	orlix_tcti_completion_u32 decode_member_source_offset;
	orlix_tcti_completion_u32 decode_member_source_length;
	orlix_tcti_completion_u32 decode_source_offset;
	orlix_tcti_completion_u32 decode_source_length;
	const char *decode_sha256;
	enum orlix_tcti_target_completion_asl_decode_state decode_state;
	enum orlix_tcti_target_completion_asl_corpus_state corpus_state;
	enum orlix_tcti_target_completion_asl_helper_state helper_state;
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
	orlix_tcti_completion_u64 reconciliation_identity;
};

struct orlix_tcti_target_completion_system_accessor_row {
	orlix_tcti_completion_u32 accessor_index;
	orlix_tcti_completion_u32 encoding_index;
	const char *name;
	const char *generic_leaf;
	enum orlix_tcti_target_completion_system_accessor_direction direction;
	enum orlix_tcti_target_completion_system_accessor_disposition disposition;
	orlix_tcti_completion_u32 selector_count;
	orlix_tcti_completion_u32 condition_expression;
	orlix_tcti_completion_u64 selector_identity;
	orlix_tcti_completion_u64 condition_identity;
	orlix_tcti_completion_u64 accessor_source_offset;
	orlix_tcti_completion_u64 accessor_source_length;
	orlix_tcti_completion_u64 encoding_source_offset;
	orlix_tcti_completion_u64 encoding_source_length;
	orlix_tcti_completion_u64 condition_source_offset;
	orlix_tcti_completion_u64 condition_source_length;
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
	ORLIX_TCTI_TARGET_COMPLETION_ERROR_ASL_AVAILABILITY = 1U << 14,
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
	size_t invalid_feature_artifact;
	/* ASL provenance must bind every leaf and remain unavailable until present. */
	size_t asl_availability_rows;
	size_t invalid_asl_availability_rows;
	size_t unavailable_asl_rows;
	/* Supplemental Registers.json relationships never alter source_rows. */
	size_t system_accessor_rows;
	size_t mapped_system_accessor_rows;
	size_t nonmapped_system_accessor_rows;
	size_t invalid_system_accessor_rows;
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
	/* Optional host-only checked artifact injection for atomicity tests. */
	const struct orlix_tcti_target_instruction_artifact *instruction_artifact;
	const struct orlix_tcti_target_linux_proof_disposition_row *linux_proof;
	size_t linux_proof_count;
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

int orlix_tcti_target_completion_validate_asl_availability(
	const struct orlix_tcti_target_completion_source_row *source,
	size_t source_count,
	const struct orlix_tcti_target_completion_asl_provenance *provenance,
	const struct orlix_tcti_target_completion_asl_row *availability,
	size_t availability_count,
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

const struct orlix_tcti_target_completion_asl_provenance *
orlix_tcti_target_completion_asl_provenance(void);

const struct orlix_tcti_target_completion_asl_row *
orlix_tcti_target_completion_asl_availability(size_t *count);

const struct orlix_tcti_target_completion_system_accessor_provenance *
orlix_tcti_target_completion_system_accessor_provenance(void);

const struct orlix_tcti_target_completion_system_accessor_row *
orlix_tcti_target_completion_system_accessors(size_t *count);

const struct orlix_tcti_target_completion_source_row *
orlix_tcti_target_completion_source(size_t *count);

const struct orlix_tcti_target_completion_classification_row *
orlix_tcti_target_completion_classification(size_t *count);

#endif /* ORLIX_TCTI_TARGET_COMPLETION_AUDIT_H */
