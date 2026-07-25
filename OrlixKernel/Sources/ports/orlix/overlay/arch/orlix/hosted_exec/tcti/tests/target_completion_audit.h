/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TARGET_COMPLETION_AUDIT_H
#define ORLIX_TCTI_TARGET_COMPLETION_AUDIT_H

#ifdef __KERNEL__
#include <linux/stddef.h>
#include <linux/types.h>
typedef u32 tcti_completion_u32;
typedef u64 tcti_completion_u64;
#define TCTI_COMPLETION_U64_C(value) value##ULL
#else
#include <stddef.h>
#include <stdint.h>
typedef uint32_t tcti_completion_u32;
typedef uint64_t tcti_completion_u64;
#define TCTI_COMPLETION_U64_C(value) UINT64_C(value)
#endif

#include "target_proof_registry.h"
#include "target_feature_domain.h"
#include "target_feature_field_domain_binding_artifact.h"
#include "target_runtime_capability_cohort_artifact.h"

#define TCTI_TARGET_COMPLETION_SOURCE_ROWS 4350U
#define TCTI_TARGET_COMPLETION_SOURCE_BYTE_LENGTH TCTI_COMPLETION_U64_C(115441429)
#define TCTI_TARGET_COMPLETION_SYSTEM_ACCESSOR_ROWS 2014U
#define TCTI_TARGET_COMPLETION_REGISTERS_BYTE_LENGTH TCTI_COMPLETION_U64_C(96016602)

enum tcti_target_completion_class {
	TCTI_TARGET_COMPLETION_UNCLASSIFIED,
	TCTI_TARGET_COMPLETION_REQUIRED_EL0,
	TCTI_TARGET_COMPLETION_NON_EL0,
	TCTI_TARGET_COMPLETION_ARCH_UNDEFINED_OR_UNALLOCATED,
	TCTI_TARGET_COMPLETION_ALIAS_OR_DUPLICATE,
};

enum tcti_target_completion_relation {
	TCTI_TARGET_COMPLETION_RELATION_NONE,
	TCTI_TARGET_COMPLETION_RELATION_ALIAS,
	TCTI_TARGET_COMPLETION_RELATION_DUPLICATE,
};

/*
 * A derived per-source-leaf status.  These bits report only a missing or
 * unresolved relationship in a checked canonical artifact.  They never
 * imply an implementation owner, semantic behavior, or executed proof.
 */
enum tcti_target_completion_obligation_blocker {
	TCTI_TARGET_COMPLETION_BLOCKER_NONE = 0,
	TCTI_TARGET_COMPLETION_BLOCKER_UNCLASSIFIED = 1U << 0,
	TCTI_TARGET_COMPLETION_BLOCKER_RELATIONSHIP = 1U << 1,
	TCTI_TARGET_COMPLETION_BLOCKER_ASL = 1U << 2,
	TCTI_TARGET_COMPLETION_BLOCKER_FEATURE_UNION = 1U << 3,
	TCTI_TARGET_COMPLETION_BLOCKER_PROOF = 1U << 4,
	TCTI_TARGET_COMPLETION_BLOCKER_UNPROVED_OBLIGATIONS = 1U << 5,
	TCTI_TARGET_COMPLETION_BLOCKER_RUNTIME_CANDIDATE = 1U << 6,
	TCTI_TARGET_COMPLETION_BLOCKER_FEATURE_UNION_INCOMPLETE = 1U << 7,
	TCTI_TARGET_COMPLETION_BLOCKER_EXECUTION_EVIDENCE = 1U << 8,
	/* The aggregate result carries artifact-wide failures separately. */
	TCTI_TARGET_COMPLETION_BLOCKER_SOURCE_CONDITION = 1U << 9,
};

enum tcti_target_completion_asl_state {
	TCTI_TARGET_COMPLETION_ASL_INVALID,
	TCTI_TARGET_COMPLETION_ASL_ABSENT_BLOCKING,
	TCTI_TARGET_COMPLETION_ASL_UNAVAILABLE,
};

enum tcti_target_completion_asl_body_state {
	TCTI_A64_ASL_BODY_ABSENT,
	TCTI_A64_ASL_BODY_PLACEHOLDER,
	TCTI_A64_ASL_BODY_PRESENT,
};

enum tcti_target_completion_asl_decode_state {
	TCTI_A64_ASL_DECODE_ABSENT,
	TCTI_A64_ASL_DECODE_NULL,
	TCTI_A64_ASL_DECODE_PRESENT,
};

enum tcti_target_completion_asl_corpus_state {
	TCTI_A64_ASL_CORPUS_ABSENT,
	TCTI_A64_ASL_CORPUS_PRESENT,
};

enum tcti_target_completion_asl_helper_state {
	TCTI_A64_ASL_HELPERS_UNAVAILABLE,
	TCTI_A64_ASL_HELPERS_AVAILABLE,
};

enum tcti_target_completion_feature_union_state {
	TCTI_TARGET_COMPLETION_FEATURE_UNION_INVALID,
	TCTI_TARGET_COMPLETION_FEATURE_UNION_MISSING_CONFIGURATION,
	TCTI_TARGET_COMPLETION_FEATURE_UNION_MISSING_OPERAND,
	TCTI_TARGET_COMPLETION_FEATURE_UNION_UNRESOLVED,
	TCTI_TARGET_COMPLETION_FEATURE_UNION_EVALUATED,
};

/* More than one source condition may be incomplete for one leaf. */
enum tcti_target_completion_feature_union_reason {
	TCTI_TARGET_COMPLETION_FEATURE_UNION_REASON_NONE = 0,
	TCTI_TARGET_COMPLETION_FEATURE_UNION_REASON_MISSING_FEATURE = 1U << 0,
	TCTI_TARGET_COMPLETION_FEATURE_UNION_REASON_MISSING_OPERAND = 1U << 1,
	TCTI_TARGET_COMPLETION_FEATURE_UNION_REASON_INCOMPLETE = 1U << 2,
	TCTI_TARGET_COMPLETION_FEATURE_UNION_REASON_INVALID = 1U << 3,
};

enum tcti_target_completion_proof_state {
	TCTI_TARGET_COMPLETION_PROOF_NONE,
	TCTI_TARGET_COMPLETION_PROOF_STALE,
	TCTI_TARGET_COMPLETION_PROOF_SOURCE_BOUND_UNPROVED,
	/* Static registry metadata has no outstanding bits, never execution credit. */
	TCTI_TARGET_COMPLETION_PROOF_SOURCE_BOUND_NO_UNPROVED_METADATA,
};

enum tcti_target_completion_runtime_candidate_state {
	TCTI_TARGET_COMPLETION_RUNTIME_CANDIDATE_NONE,
	TCTI_TARGET_COMPLETION_RUNTIME_CANDIDATE_UNRESOLVED,
};

struct tcti_target_completion_source_row {
	tcti_completion_u32 ordinal;
	const char *name;
	const char *mnemonic;
	const char *operation_id;
	tcti_completion_u32 mask;
	tcti_completion_u32 pattern;
	const char *condition_tcnd_hex;
	tcti_completion_u64 source_offset;
	tcti_completion_u64 source_length;
};

/* Pinned Arm-source identity carried by the canonical C manifest. */
struct tcti_target_completion_source_provenance {
	const char *architecture;
	const char *build;
	const char *release;
	const char *schema;
	const char *timestamp;
	const char *source_sha256;
	tcti_completion_u64 source_byte_length;
	size_t leaf_count;
};

struct tcti_target_completion_classification_row {
	const char *name;
	enum tcti_target_completion_class classification;
	enum tcti_target_completion_relation relation;
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
struct tcti_target_completion_obligation {
	tcti_completion_u32 ordinal;
	tcti_completion_u32 blocker_mask;
	const char *name;
	const char *mnemonic;
	const char *operation_id;
	enum tcti_target_completion_class classification;
	enum tcti_target_completion_relation relation;
	const char *canonical_name;
	const char *asl_operation_object;
	enum tcti_target_completion_asl_state asl_state;
	enum tcti_target_completion_feature_union_state feature_union_state;
	/* Diagnostic order is retained only as a first-failure locator. */
	enum tcti_feature_domain_tcnd_error first_unsupported_error;
	/* First failure plus incompleteness only. This is not an exhaustive cause set. */
	tcti_completion_u32 known_feature_union_reason_mask;
	const char *proof_id;
	enum tcti_target_completion_proof_state proof_state;
	tcti_completion_u32 required_obligations;
	tcti_completion_u32 unproved_obligations;
	const char *kunit_source;
	const char *kunit_suite;
	const struct tcti_target_kselftest_provenance *kselftest;
	enum tcti_target_completion_runtime_candidate_state runtime_candidate_state;
	tcti_completion_u32 runtime_candidate_first;
	tcti_completion_u32 runtime_candidate_count;
};

/*
 * Provenance for the authoritative ASL relationship of every pinned source
 * leaf.  Availability is deliberately separate from implementation proof:
 * an absent shared ASL corpus blocks semantic-completeness claims.
 */
struct tcti_target_completion_asl_provenance {
	const char *format;
	const char *source_sha256;
	enum tcti_target_completion_asl_corpus_state corpus_state;
	enum tcti_target_completion_asl_helper_state helper_state;
};

struct tcti_target_completion_asl_row {
	tcti_completion_u32 ordinal;
	const char *name;
	const char *operation_id;
	const char *semantic_operation_id;
	const char *semantic_member_locator;
	tcti_completion_u32 semantic_member_source_offset;
	tcti_completion_u32 semantic_member_source_length;
	tcti_completion_u32 semantic_body_source_offset;
	tcti_completion_u32 semantic_body_source_length;
	const char *semantic_body_sha256;
	enum tcti_target_completion_asl_body_state semantic_body_state;
	const char *decode_member_locator;
	tcti_completion_u32 decode_member_source_offset;
	tcti_completion_u32 decode_member_source_length;
	tcti_completion_u32 decode_source_offset;
	tcti_completion_u32 decode_source_length;
	const char *decode_sha256;
	enum tcti_target_completion_asl_decode_state decode_state;
	enum tcti_target_completion_asl_corpus_state corpus_state;
	enum tcti_target_completion_asl_helper_state helper_state;
};

enum tcti_target_completion_system_accessor_direction {
	TCTI_TARGET_COMPLETION_ACCESSOR_DIRECTION_NONE,
	TCTI_TARGET_COMPLETION_ACCESSOR_DIRECTION_READ,
	TCTI_TARGET_COMPLETION_ACCESSOR_DIRECTION_WRITE,
	TCTI_TARGET_COMPLETION_ACCESSOR_DIRECTION_EXECUTE,
};

enum tcti_target_completion_system_accessor_disposition {
	TCTI_TARGET_COMPLETION_ACCESSOR_MAPPED,
	TCTI_TARGET_COMPLETION_ACCESSOR_RESERVED,
	TCTI_TARGET_COMPLETION_ACCESSOR_PRIVILEGED,
	TCTI_TARGET_COMPLETION_ACCESSOR_UNSUPPORTED,
	TCTI_TARGET_COMPLETION_ACCESSOR_AMBIGUOUS,
	TCTI_TARGET_COMPLETION_ACCESSOR_CONTRADICTORY,
	TCTI_TARGET_COMPLETION_ACCESSOR_INVALID,
};

struct tcti_target_completion_system_accessor_provenance {
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
	tcti_completion_u64 reconciliation_identity;
};

struct tcti_target_completion_system_accessor_row {
	tcti_completion_u32 accessor_index;
	tcti_completion_u32 encoding_index;
	const char *name;
	const char *generic_leaf;
	enum tcti_target_completion_system_accessor_direction direction;
	enum tcti_target_completion_system_accessor_disposition disposition;
	tcti_completion_u32 selector_count;
	tcti_completion_u32 condition_expression;
	tcti_completion_u64 selector_identity;
	tcti_completion_u64 condition_identity;
	tcti_completion_u64 accessor_source_offset;
	tcti_completion_u64 accessor_source_length;
	tcti_completion_u64 encoding_source_offset;
	tcti_completion_u64 encoding_source_length;
	tcti_completion_u64 condition_source_offset;
	tcti_completion_u64 condition_source_length;
};

enum tcti_target_completion_error {
	TCTI_TARGET_COMPLETION_ERROR_NONE = 0,
	TCTI_TARGET_COMPLETION_ERROR_SOURCE_COUNT = 1U << 0,
	TCTI_TARGET_COMPLETION_ERROR_CLASSIFICATION_COUNT = 1U << 1,
	TCTI_TARGET_COMPLETION_ERROR_SOURCE = 1U << 2,
	TCTI_TARGET_COMPLETION_ERROR_ABSENT = 1U << 3,
	TCTI_TARGET_COMPLETION_ERROR_STALE = 1U << 4,
	TCTI_TARGET_COMPLETION_ERROR_UNCLASSIFIED = 1U << 5,
	TCTI_TARGET_COMPLETION_ERROR_SOURCE_BINDING = 1U << 6,
	TCTI_TARGET_COMPLETION_ERROR_RELATIONSHIP = 1U << 7,
	TCTI_TARGET_COMPLETION_ERROR_PROOF_REGISTRY = 1U << 8,
	TCTI_TARGET_COMPLETION_ERROR_STALE_PROOF_BINDING = 1U << 9,
	TCTI_TARGET_COMPLETION_ERROR_SOURCE_PROVENANCE = 1U << 10,
	TCTI_TARGET_COMPLETION_ERROR_FEATURE_DOMAIN = 1U << 11,
	TCTI_TARGET_COMPLETION_ERROR_FEATURE_APPLICABILITY = 1U << 12,
	TCTI_TARGET_COMPLETION_ERROR_UNPROVED_OBLIGATIONS = 1U << 13,
	TCTI_TARGET_COMPLETION_ERROR_ASL_AVAILABILITY = 1U << 14,
	TCTI_TARGET_COMPLETION_ERROR_SYSTEM_ACCESSOR = 1U << 15,
	/*
	 * Every checked AST.FIELD occurrence must bind to an authoritative
	 * Registers.json domain. Mapped rows remain blocking until their domain
	 * semantics are evaluated, and ambiguity is never silently resolved.
	 */
	TCTI_TARGET_COMPLETION_ERROR_FEATURE_FIELD_DOMAIN = 1U << 16,
	/* Feature-conditioned leaves remain blocking until each cohort is proved. */
	TCTI_TARGET_COMPLETION_ERROR_RUNTIME_CAPABILITY_COHORT = 1U << 17,
};

struct tcti_target_completion_result {
	tcti_completion_u32 error_mask;
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
};

/* Host-test-only dependency injection. It is not a kernel or runtime ABI. */
struct tcti_target_completion_audit_inputs_for_test {
	const struct tcti_target_completion_source_row *source;
	size_t source_count;
	const struct tcti_target_completion_classification_row *classification;
	size_t classification_count;
	const struct tcti_target_proof_registry_entry *registry;
	size_t registry_count;
	/* Optional host-only checked artifact injection for atomicity tests. */
	const struct tcti_target_instruction_artifact *instruction_artifact;
};

int tcti_target_completion_validate(
	const struct tcti_target_completion_source_row *source,
	size_t source_count,
	const struct tcti_target_completion_classification_row *classification,
	size_t classification_count,
	const struct tcti_target_proof_registry_entry *registry,
	size_t registry_count,
	struct tcti_target_completion_result *result);

int tcti_target_completion_audit(struct tcti_target_completion_result *result);

/*
 * Run the canonical aggregate audit and, when requested, derive exactly one
 * immutable obligation row for every source ordinal.  A projection is valid
 * only with a full 4,350-row caller buffer.  NULL with count zero requests no
 * projection.
 */
int tcti_target_completion_audit_with_obligations(
	struct tcti_target_completion_result *result,
	struct tcti_target_completion_obligation *obligations,
	size_t obligation_count);

int tcti_target_completion_audit_with_inputs_for_test(
	const struct tcti_target_completion_audit_inputs_for_test *inputs,
	struct tcti_target_completion_result *result,
	struct tcti_target_completion_obligation *obligations,
	size_t obligation_count);

int tcti_target_completion_validate_source_provenance(
	const struct tcti_target_completion_source_provenance *provenance,
	struct tcti_target_completion_result *result);

int tcti_target_completion_validate_asl_availability(
	const struct tcti_target_completion_source_row *source,
	size_t source_count,
	const struct tcti_target_completion_asl_provenance *provenance,
	const struct tcti_target_completion_asl_row *availability,
	size_t availability_count,
	struct tcti_target_completion_result *result);

int tcti_target_completion_validate_system_accessors(
	const struct tcti_target_completion_source_row *source,
	size_t source_count,
	const struct tcti_target_completion_system_accessor_provenance *provenance,
	const struct tcti_target_completion_system_accessor_row *accessors,
	size_t accessor_count,
	struct tcti_target_completion_result *result);

int tcti_target_completion_validate_feature_field_domains(
	const struct tcti_feature_artifact *feature_artifact,
	const struct tcti_feature_field_domain_binding_artifact *artifact,
	struct tcti_target_completion_result *result);

int tcti_target_completion_validate_runtime_capability_cohorts(
	const struct tcti_runtime_capability_cohort_artifact *artifact,
	struct tcti_target_completion_result *result);

const struct tcti_target_completion_source_provenance *
tcti_target_completion_source_provenance(void);

const struct tcti_target_completion_asl_provenance *
tcti_target_completion_asl_provenance(void);

const struct tcti_target_completion_asl_row *
tcti_target_completion_asl_availability(size_t *count);

const struct tcti_target_completion_system_accessor_provenance *
tcti_target_completion_system_accessor_provenance(void);

const struct tcti_target_completion_system_accessor_row *
tcti_target_completion_system_accessors(size_t *count);

const struct tcti_target_completion_source_row *
tcti_target_completion_source(size_t *count);

const struct tcti_target_completion_classification_row *
tcti_target_completion_classification(size_t *count);

#endif /* ORLIX_TCTI_TARGET_COMPLETION_AUDIT_H */
