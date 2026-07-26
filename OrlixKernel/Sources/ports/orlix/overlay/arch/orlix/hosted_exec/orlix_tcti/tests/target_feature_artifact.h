/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Static consumer for the checked C artifact emitted from the pinned Arm
 * feature source.  This layer consumes no raw maintainer input and has no
 * import-model dependency.  Normal kernel and host consumers validate the
 * fixed-width artifact only.
 */
#ifndef ORLIX_TCTI_TARGET_FEATURE_ARTIFACT_H
#define ORLIX_TCTI_TARGET_FEATURE_ARTIFACT_H

#ifdef __KERNEL__
#include <linux/stddef.h>
#include <linux/types.h>
typedef u8 orlix_tcti_feature_artifact_u8;
typedef u32 orlix_tcti_feature_artifact_u32;
typedef u64 orlix_tcti_feature_artifact_u64;
typedef s64 orlix_tcti_feature_artifact_s64;
#else
#include <stddef.h>
#include <stdint.h>
typedef uint8_t orlix_tcti_feature_artifact_u8;
typedef uint32_t orlix_tcti_feature_artifact_u32;
typedef uint64_t orlix_tcti_feature_artifact_u64;
typedef int64_t orlix_tcti_feature_artifact_s64;
#endif

#define ORLIX_TCTI_FEATURE_ARTIFACT_PARAMETER_COUNT 409U
#define ORLIX_TCTI_FEATURE_ARTIFACT_CONSTRAINT_COUNT 1626U
#define ORLIX_TCTI_FEATURE_ARTIFACT_PARAMETER_CONSTRAINT_COUNT 1615U
#define ORLIX_TCTI_FEATURE_ARTIFACT_GLOBAL_CONSTRAINT_COUNT 11U
#define ORLIX_TCTI_FEATURE_ARTIFACT_NODE_COUNT 8955U
#define ORLIX_TCTI_FEATURE_ARTIFACT_CHILD_COUNT 654U
#define ORLIX_TCTI_FEATURE_ARTIFACT_FIELD_NODE_COUNT 605U
#define ORLIX_TCTI_FEATURE_ARTIFACT_NODE_NONE ((orlix_tcti_feature_artifact_u32)-1)

#define ORLIX_TCTI_FEATURE_ARTIFACT_ARCHITECTURE "vFATAp1-A"
#define ORLIX_TCTI_FEATURE_ARTIFACT_BUILD "818"
#define ORLIX_TCTI_FEATURE_ARTIFACT_REFERENCE "2026-06_rel"
#define ORLIX_TCTI_FEATURE_ARTIFACT_SCHEMA "2.9.5"
#define ORLIX_TCTI_FEATURE_ARTIFACT_PINNED_SOURCE_LENGTH 1243621U
#define ORLIX_TCTI_FEATURE_ARTIFACT_SOURCE_SHA256 \
	"633259000ffd3da32900bd0c0c1beae4a9eea7095c278f74d62a00c846b41187"

enum orlix_tcti_feature_artifact_node_kind {
	ORLIX_TCTI_FEATURE_ARTIFACT_BOOL,
	ORLIX_TCTI_FEATURE_ARTIFACT_IDENTIFIER,
	ORLIX_TCTI_FEATURE_ARTIFACT_INTEGER,
	ORLIX_TCTI_FEATURE_ARTIFACT_DOT_ATOM,
	ORLIX_TCTI_FEATURE_ARTIFACT_SET,
	ORLIX_TCTI_FEATURE_ARTIFACT_NOT,
	ORLIX_TCTI_FEATURE_ARTIFACT_AND,
	ORLIX_TCTI_FEATURE_ARTIFACT_OR,
	ORLIX_TCTI_FEATURE_ARTIFACT_EQ,
	ORLIX_TCTI_FEATURE_ARTIFACT_NE,
	ORLIX_TCTI_FEATURE_ARTIFACT_LT,
	ORLIX_TCTI_FEATURE_ARTIFACT_GT,
	ORLIX_TCTI_FEATURE_ARTIFACT_GE,
	ORLIX_TCTI_FEATURE_ARTIFACT_IN,
	ORLIX_TCTI_FEATURE_ARTIFACT_IMPLIES,
	ORLIX_TCTI_FEATURE_ARTIFACT_IFF,
	ORLIX_TCTI_FEATURE_ARTIFACT_UINT,
	ORLIX_TCTI_FEATURE_ARTIFACT_SINT,
	ORLIX_TCTI_FEATURE_ARTIFACT_FIELD,
	ORLIX_TCTI_FEATURE_ARTIFACT_VALUE,
	ORLIX_TCTI_FEATURE_ARTIFACT_NODE_KIND_COUNT,
};

enum orlix_tcti_feature_artifact_field_qualifier_kind {
	ORLIX_TCTI_FEATURE_ARTIFACT_FIELD_QUALIFIER_NONE,
	ORLIX_TCTI_FEATURE_ARTIFACT_FIELD_QUALIFIER_NULL,
	ORLIX_TCTI_FEATURE_ARTIFACT_FIELD_QUALIFIER_KIND_COUNT,
};

struct orlix_tcti_feature_artifact_span {
	orlix_tcti_feature_artifact_u32 offset;
	orlix_tcti_feature_artifact_u32 length;
};

struct orlix_tcti_feature_artifact_source {
	const char *architecture;
	const char *build;
	const char *reference;
	const char *schema;
	const char *sha256;
	orlix_tcti_feature_artifact_u32 length;
};

struct orlix_tcti_feature_artifact_counts {
	orlix_tcti_feature_artifact_u32 parameter_count;
	orlix_tcti_feature_artifact_u32 constraint_count;
	orlix_tcti_feature_artifact_u32 parameter_constraint_count;
	orlix_tcti_feature_artifact_u32 node_count;
	orlix_tcti_feature_artifact_u32 child_count;
	orlix_tcti_feature_artifact_u32 global_constraint_count;
};

struct orlix_tcti_feature_artifact_parameter {
	const char *name;
	orlix_tcti_feature_artifact_u32 first_constraint;
	orlix_tcti_feature_artifact_u32 constraint_count;
	struct orlix_tcti_feature_artifact_span source;
};

struct orlix_tcti_feature_artifact_constraint {
	orlix_tcti_feature_artifact_u32 node_index;
	struct orlix_tcti_feature_artifact_span source;
};

struct orlix_tcti_feature_artifact_field_qualifier {
	orlix_tcti_feature_artifact_u32 kind;
	struct orlix_tcti_feature_artifact_span source;
};

struct orlix_tcti_feature_artifact_node {
	orlix_tcti_feature_artifact_u32 kind;
	orlix_tcti_feature_artifact_u32 left;
	orlix_tcti_feature_artifact_u32 right;
	orlix_tcti_feature_artifact_u32 first_child;
	orlix_tcti_feature_artifact_u32 child_count;
	orlix_tcti_feature_artifact_s64 integer;
	const char *text;
	const char *field_state;
	const char *field_register_name;
	const char *field_selector;
	struct orlix_tcti_feature_artifact_span field_source;
	struct orlix_tcti_feature_artifact_field_qualifier field_instance;
	struct orlix_tcti_feature_artifact_field_qualifier field_slices;
	struct orlix_tcti_feature_artifact_span source;
};

struct orlix_tcti_feature_artifact {
	struct orlix_tcti_feature_artifact_source source;
	struct orlix_tcti_feature_artifact_counts counts;
	const struct orlix_tcti_feature_artifact_parameter *parameters;
	const struct orlix_tcti_feature_artifact_constraint *constraints;
	const struct orlix_tcti_feature_artifact_node *nodes;
	const orlix_tcti_feature_artifact_u32 *children;
};

enum orlix_tcti_feature_artifact_error {
	ORLIX_TCTI_FEATURE_ARTIFACT_VALID,
	ORLIX_TCTI_FEATURE_ARTIFACT_INVALID_ARGUMENT,
	ORLIX_TCTI_FEATURE_ARTIFACT_PIN_MISMATCH,
	ORLIX_TCTI_FEATURE_ARTIFACT_COUNT_MISMATCH,
	ORLIX_TCTI_FEATURE_ARTIFACT_POINTER_MISSING,
	ORLIX_TCTI_FEATURE_ARTIFACT_STRING_INVALID,
	ORLIX_TCTI_FEATURE_ARTIFACT_SOURCE_SPAN_INVALID,
	ORLIX_TCTI_FEATURE_ARTIFACT_SCOPE_INVALID,
	ORLIX_TCTI_FEATURE_ARTIFACT_REFERENCE_INVALID,
	ORLIX_TCTI_FEATURE_ARTIFACT_NODE_INVALID,
	ORLIX_TCTI_FEATURE_ARTIFACT_CYCLE,
	ORLIX_TCTI_FEATURE_ARTIFACT_GRAPH_COVERAGE_INVALID,
	ORLIX_TCTI_FEATURE_ARTIFACT_CHILD_COVERAGE_INVALID,
	ORLIX_TCTI_FEATURE_ARTIFACT_PROVENANCE_INVALID,
	ORLIX_TCTI_FEATURE_ARTIFACT_SCRATCH_TOO_SMALL,
};

struct orlix_tcti_feature_artifact_diagnostic {
	enum orlix_tcti_feature_artifact_error error;
	orlix_tcti_feature_artifact_u32 index;
};

struct orlix_tcti_feature_artifact_frame {
	orlix_tcti_feature_artifact_u32 node_index;
	orlix_tcti_feature_artifact_u64 next_edge;
};

struct orlix_tcti_feature_artifact_scratch {
	orlix_tcti_feature_artifact_u8 *state;
	size_t state_count;
	struct orlix_tcti_feature_artifact_frame *frames;
	size_t frame_count;
	orlix_tcti_feature_artifact_u8 *child_coverage;
	size_t child_coverage_count;
};

/*
 * `state` and `frames` must each provide at least counts.node_count entries.
 * They are scratch only, and are cleared before use.  Validation is iterative,
 * performs no allocation, and does not inspect raw maintainer input.  A zero-valued
 * runtime feature still has to occupy its parameter, constraint, and node
 * representation in this artifact.
 */
enum orlix_tcti_feature_artifact_error orlix_tcti_feature_artifact_validate(
	const struct orlix_tcti_feature_artifact *artifact,
	struct orlix_tcti_feature_artifact_scratch *scratch,
	struct orlix_tcti_feature_artifact_diagnostic *diagnostic);

/* The checked arch/orlix artifact is the sole production data source. */
const struct orlix_tcti_feature_artifact *orlix_tcti_feature_artifact_canonical(void);

const char *orlix_tcti_feature_artifact_error_name(
	enum orlix_tcti_feature_artifact_error error);

#endif
