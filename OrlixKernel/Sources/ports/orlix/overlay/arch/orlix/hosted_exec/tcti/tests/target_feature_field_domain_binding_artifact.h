/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Static consumer contract for the checked feature FIELD-to-register-domain
 * binding artifact.  The maintainer lane emits the companion .def from the
 * pinned Arm source.  Normal kernel and host consumers never read that raw
 * source.
 */
#ifndef ORLIX_TCTI_TARGET_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_H
#define ORLIX_TCTI_TARGET_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_H

#include "target_feature_artifact.h"

#define TCTI_FEATURE_FIELD_DOMAIN_BINDING_OCCURRENCE_COUNT 605U
#define TCTI_FEATURE_FIELD_DOMAIN_BINDING_IDENTITY_GROUP_COUNT 362U
#define TCTI_FEATURE_FIELD_DOMAIN_BINDING_MAPPED_COUNT 604U
#define TCTI_FEATURE_FIELD_DOMAIN_BINDING_AMBIGUOUS_FIELD_COUNT 1U
#define TCTI_FEATURE_FIELD_DOMAIN_BINDING_FNV1A_OFFSET 0x14650fb0739d0383ULL
#define TCTI_FEATURE_FIELD_DOMAIN_BINDING_FNV1A_PRIME 0x100000001b3ULL

#define TCTI_FEATURE_FIELD_DOMAIN_BINDING_REGISTER_SOURCE_SHA256 \
	"5bd76c3c3ce90322eb4fd179675dafe82df2fd1cb789beee516e5b29c471b874"
#define TCTI_FEATURE_FIELD_DOMAIN_BINDING_REGISTER_SOURCE_LENGTH 96016602U

enum tcti_feature_field_domain_disposition {
	TCTI_FEATURE_FIELD_DOMAIN_MAPPED,
	TCTI_FEATURE_FIELD_DOMAIN_MISSING_REGISTER,
	TCTI_FEATURE_FIELD_DOMAIN_AMBIGUOUS_REGISTER,
	TCTI_FEATURE_FIELD_DOMAIN_MISSING_FIELD,
	TCTI_FEATURE_FIELD_DOMAIN_AMBIGUOUS_FIELD,
	TCTI_FEATURE_FIELD_DOMAIN_INVALID_MODEL,
	TCTI_FEATURE_FIELD_DOMAIN_DISPOSITION_COUNT,
};

/*
 * Each generated record binds one feature AST.FIELD occurrence to an Arm
 * register field/value-domain relation.  `identity_group_index` coalesces
 * equal semantic relationships while every occurrence retains independent
 * raw source spans and a direct feature-node index.
 */
struct tcti_feature_field_domain_binding {
	tcti_feature_artifact_u32 order;
	tcti_feature_artifact_u32 feature_node_index;
	tcti_feature_artifact_u32 identity_group_index;
	tcti_feature_artifact_u32 occurrence_count;
	tcti_feature_artifact_u32 register_index;
	tcti_feature_artifact_u32 field_index;
	tcti_feature_artifact_u32 value_relation_index;
	enum tcti_feature_field_domain_disposition disposition;
	const char *field_state;
	const char *field_register_name;
	const char *field_selector;
	struct tcti_feature_artifact_field_qualifier field_instance;
	struct tcti_feature_artifact_field_qualifier field_slices;
	struct tcti_feature_artifact_span feature_source;
	struct tcti_feature_artifact_span register_source;
	struct tcti_feature_artifact_span field_source;
	struct tcti_feature_artifact_span value_relation_source;
};

struct tcti_feature_field_domain_binding_artifact {
	struct tcti_feature_artifact_source source;
	const char *register_source_sha256;
	tcti_feature_artifact_u32 register_source_length;
	tcti_feature_artifact_u32 occurrence_count;
	tcti_feature_artifact_u32 identity_group_count;
	tcti_feature_artifact_u32 mapped_count;
	tcti_feature_artifact_u32 ambiguous_field_count;
	tcti_feature_artifact_u64 identity;
	const struct tcti_feature_field_domain_binding *bindings;
};

struct tcti_feature_field_domain_binding_contract {
	tcti_feature_artifact_u32 occurrence_count;
	tcti_feature_artifact_u32 identity_group_count;
	tcti_feature_artifact_u32 mapped_count;
	tcti_feature_artifact_u32 ambiguous_field_count;
	const char *register_source_sha256;
	tcti_feature_artifact_u32 register_source_length;
};

enum tcti_feature_field_domain_binding_error {
	TCTI_FEATURE_FIELD_DOMAIN_BINDING_VALID,
	TCTI_FEATURE_FIELD_DOMAIN_BINDING_INVALID_ARGUMENT,
	TCTI_FEATURE_FIELD_DOMAIN_BINDING_PIN_MISMATCH,
	TCTI_FEATURE_FIELD_DOMAIN_BINDING_COUNT_MISMATCH,
	TCTI_FEATURE_FIELD_DOMAIN_BINDING_POINTER_MISSING,
	TCTI_FEATURE_FIELD_DOMAIN_BINDING_DISPOSITION_INVALID,
	TCTI_FEATURE_FIELD_DOMAIN_BINDING_INDEX_INVALID,
	TCTI_FEATURE_FIELD_DOMAIN_BINDING_IDENTITY_INVALID,
	TCTI_FEATURE_FIELD_DOMAIN_BINDING_PROVENANCE_INVALID,
	TCTI_FEATURE_FIELD_DOMAIN_BINDING_COVERAGE_INVALID,
	/* Production records must match the checked canonical relation exactly. */
	TCTI_FEATURE_FIELD_DOMAIN_BINDING_CANONICAL_MISMATCH,
	TCTI_FEATURE_FIELD_DOMAIN_BINDING_SCRATCH_TOO_SMALL,
};

struct tcti_feature_field_domain_binding_diagnostic {
	enum tcti_feature_field_domain_binding_error error;
	tcti_feature_artifact_u32 index;
};

struct tcti_feature_field_domain_binding_scratch {
	tcti_feature_artifact_u8 *feature_node_coverage;
	size_t feature_node_coverage_count;
	tcti_feature_artifact_u32 *identity_group_coverage;
	size_t identity_group_coverage_count;
};

/*
 * Checked, versioned binding artifact consumed by normal kernel and host
 * audit code. The definition is generated only in the maintainer lane from
 * the pinned Arm sources.
 */
const struct tcti_feature_field_domain_binding_artifact *
tcti_feature_field_domain_binding_artifact_canonical(void);

/*
 * The generator must emit this shape in the checked .def:
 *
 * TCTI_A64_FEATURE_FIELD_DOMAIN_SOURCE(architecture, build, reference,
 *     schema, features_sha256, features_length, registers_sha256,
 *     registers_length)
 * TCTI_A64_FEATURE_FIELD_DOMAIN_COUNTS(occurrences, identity_groups, mapped,
 *     ambiguous_field)
 * TCTI_A64_FEATURE_FIELD_DOMAIN_IDENTITY(fnv1a)
 * TCTI_A64_FEATURE_FIELD_DOMAIN_OCCURRENCE(order, feature_node_index,
 *     identity_group_index, occurrence_count, register_index, field_index,
 *     value_relation_index, disposition, state, register_name, selector,
 *     instance_kind, instance_offset, instance_length, slices_kind,
 *     slices_offset, slices_length, feature_offset, feature_length,
 *     register_offset, register_length, field_offset, field_length,
 *     value_relation_offset, value_relation_length)
 *
 * The canonical production consumer is intentionally deferred until that
 * generated checked artifact exists.  This validator is independently
 * fixture-testable and requires no raw Arm input.
 */
enum tcti_feature_field_domain_binding_error
tcti_feature_field_domain_binding_validate_with_contract(
	const struct tcti_feature_artifact *feature_artifact,
	const struct tcti_feature_field_domain_binding_artifact *artifact,
	const struct tcti_feature_field_domain_binding_contract *contract,
	struct tcti_feature_field_domain_binding_scratch *scratch,
	struct tcti_feature_field_domain_binding_diagnostic *diagnostic);

/* Validates the pinned 605-occurrence, 362-identity-group production contract. */
enum tcti_feature_field_domain_binding_error
tcti_feature_field_domain_binding_validate(
	const struct tcti_feature_artifact *feature_artifact,
	const struct tcti_feature_field_domain_binding_artifact *artifact,
	struct tcti_feature_field_domain_binding_scratch *scratch,
	struct tcti_feature_field_domain_binding_diagnostic *diagnostic);

/* Canonical FNV-1a serialization for TCTI_A64_FEATURE_FIELD_DOMAIN_IDENTITY. */
tcti_feature_artifact_u64 tcti_feature_field_domain_binding_identity(
	const struct tcti_feature_field_domain_binding *bindings,
	tcti_feature_artifact_u32 occurrence_count);

const char *tcti_feature_field_domain_binding_error_name(
	enum tcti_feature_field_domain_binding_error error);

#endif
