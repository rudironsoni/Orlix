/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TARGET_COMPLETION_AUDIT_H
#define ORLIX_TCTI_TARGET_COMPLETION_AUDIT_H

#include <stddef.h>
#include <stdint.h>

#include "target_proof_registry.h"

#define TCTI_TARGET_COMPLETION_SOURCE_ROWS 4350U
#define TCTI_TARGET_COMPLETION_SOURCE_BYTE_LENGTH UINT64_C(115441429)
#define TCTI_TARGET_COMPLETION_SYSTEM_ACCESSOR_ROWS 2014U
#define TCTI_TARGET_COMPLETION_REGISTERS_BYTE_LENGTH UINT64_C(96016602)

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

struct tcti_target_completion_source_row {
	uint32_t ordinal;
	const char *name;
	const char *mnemonic;
	const char *operation_id;
	uint32_t mask;
	uint32_t pattern;
	const char *condition_tcnd_hex;
	uint64_t source_offset;
	uint64_t source_length;
};

/* Pinned Arm-source identity carried by the canonical C manifest. */
struct tcti_target_completion_source_provenance {
	const char *architecture;
	const char *build;
	const char *release;
	const char *schema;
	const char *timestamp;
	const char *source_sha256;
	uint64_t source_byte_length;
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
 * Provenance for the authoritative ASL relationship of every pinned source
 * leaf.  Availability is deliberately separate from implementation proof:
 * an absent shared ASL corpus blocks semantic-completeness claims.
 */
struct tcti_target_completion_asl_provenance {
	const char *format;
	const char *source_sha256;
	const char *availability;
};

struct tcti_target_completion_asl_row {
	uint32_t ordinal;
	const char *name;
	const char *operation_id;
	const char *operation_object;
	uint32_t source_offset;
	uint32_t source_length;
	/* Raw AARCHMRS operational_note provenance, never an ASL semantic claim. */
	const char *operational_note_presence;
	uint32_t operational_note_source_offset;
	uint32_t operational_note_source_length;
	const char *operational_note_sha256;
	const char *availability;
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
	uint64_t reconciliation_identity;
};

struct tcti_target_completion_system_accessor_row {
	uint32_t accessor_index;
	uint32_t encoding_index;
	const char *name;
	const char *generic_leaf;
	enum tcti_target_completion_system_accessor_direction direction;
	enum tcti_target_completion_system_accessor_disposition disposition;
	uint32_t selector_count;
	uint32_t condition_expression;
	uint64_t selector_identity;
	uint64_t condition_identity;
	uint64_t accessor_source_offset;
	uint64_t accessor_source_length;
	uint64_t encoding_source_offset;
	uint64_t encoding_source_length;
	uint64_t condition_source_offset;
	uint64_t condition_source_length;
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
};

struct tcti_target_completion_result {
	uint32_t error_mask;
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
