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
typedef u8 tcti_feature_artifact_u8;
typedef u32 tcti_feature_artifact_u32;
typedef u64 tcti_feature_artifact_u64;
typedef s64 tcti_feature_artifact_s64;
#else
#include <stddef.h>
#include <stdint.h>
typedef uint8_t tcti_feature_artifact_u8;
typedef uint32_t tcti_feature_artifact_u32;
typedef uint64_t tcti_feature_artifact_u64;
typedef int64_t tcti_feature_artifact_s64;
#endif

#define TCTI_FEATURE_ARTIFACT_PARAMETER_COUNT 409U
#define TCTI_FEATURE_ARTIFACT_CONSTRAINT_COUNT 1626U
#define TCTI_FEATURE_ARTIFACT_PARAMETER_CONSTRAINT_COUNT 1615U
#define TCTI_FEATURE_ARTIFACT_GLOBAL_CONSTRAINT_COUNT 11U
#define TCTI_FEATURE_ARTIFACT_NODE_COUNT 8955U
#define TCTI_FEATURE_ARTIFACT_CHILD_COUNT 654U
#define TCTI_FEATURE_ARTIFACT_NODE_NONE ((tcti_feature_artifact_u32)-1)

#define TCTI_FEATURE_ARTIFACT_ARCHITECTURE "vFATAp1-A"
#define TCTI_FEATURE_ARTIFACT_BUILD "818"
#define TCTI_FEATURE_ARTIFACT_REFERENCE "2026-06_rel"
#define TCTI_FEATURE_ARTIFACT_SCHEMA "2.9.5"
#define TCTI_FEATURE_ARTIFACT_PINNED_SOURCE_LENGTH 1243621U
#define TCTI_FEATURE_ARTIFACT_SOURCE_SHA256 \
	"633259000ffd3da32900bd0c0c1beae4a9eea7095c278f74d62a00c846b41187"

enum tcti_feature_artifact_node_kind {
	TCTI_FEATURE_ARTIFACT_BOOL,
	TCTI_FEATURE_ARTIFACT_IDENTIFIER,
	TCTI_FEATURE_ARTIFACT_INTEGER,
	TCTI_FEATURE_ARTIFACT_DOT_ATOM,
	TCTI_FEATURE_ARTIFACT_SET,
	TCTI_FEATURE_ARTIFACT_NOT,
	TCTI_FEATURE_ARTIFACT_AND,
	TCTI_FEATURE_ARTIFACT_OR,
	TCTI_FEATURE_ARTIFACT_EQ,
	TCTI_FEATURE_ARTIFACT_NE,
	TCTI_FEATURE_ARTIFACT_LT,
	TCTI_FEATURE_ARTIFACT_GT,
	TCTI_FEATURE_ARTIFACT_GE,
	TCTI_FEATURE_ARTIFACT_IN,
	TCTI_FEATURE_ARTIFACT_IMPLIES,
	TCTI_FEATURE_ARTIFACT_IFF,
	TCTI_FEATURE_ARTIFACT_UINT,
	TCTI_FEATURE_ARTIFACT_SINT,
	TCTI_FEATURE_ARTIFACT_FIELD,
	TCTI_FEATURE_ARTIFACT_VALUE,
	TCTI_FEATURE_ARTIFACT_NODE_KIND_COUNT,
};

struct tcti_feature_artifact_span {
	tcti_feature_artifact_u32 offset;
	tcti_feature_artifact_u32 length;
};

struct tcti_feature_artifact_source {
	const char *architecture;
	const char *build;
	const char *reference;
	const char *schema;
	const char *sha256;
	tcti_feature_artifact_u32 length;
};

struct tcti_feature_artifact_counts {
	tcti_feature_artifact_u32 parameter_count;
	tcti_feature_artifact_u32 constraint_count;
	tcti_feature_artifact_u32 parameter_constraint_count;
	tcti_feature_artifact_u32 node_count;
	tcti_feature_artifact_u32 child_count;
	tcti_feature_artifact_u32 global_constraint_count;
};

struct tcti_feature_artifact_parameter {
	const char *name;
	tcti_feature_artifact_u32 first_constraint;
	tcti_feature_artifact_u32 constraint_count;
	struct tcti_feature_artifact_span source;
};

struct tcti_feature_artifact_constraint {
	tcti_feature_artifact_u32 node_index;
	struct tcti_feature_artifact_span source;
};

struct tcti_feature_artifact_node {
	tcti_feature_artifact_u32 kind;
	tcti_feature_artifact_u32 left;
	tcti_feature_artifact_u32 right;
	tcti_feature_artifact_u32 first_child;
	tcti_feature_artifact_u32 child_count;
	tcti_feature_artifact_s64 integer;
	const char *text;
	const char *field_state;
	const char *field_register_name;
	const char *field_selector;
	struct tcti_feature_artifact_span field_source;
	struct tcti_feature_artifact_span source;
};

struct tcti_feature_artifact {
	struct tcti_feature_artifact_source source;
	struct tcti_feature_artifact_counts counts;
	const struct tcti_feature_artifact_parameter *parameters;
	const struct tcti_feature_artifact_constraint *constraints;
	const struct tcti_feature_artifact_node *nodes;
	const tcti_feature_artifact_u32 *children;
};

enum tcti_feature_artifact_error {
	TCTI_FEATURE_ARTIFACT_VALID,
	TCTI_FEATURE_ARTIFACT_INVALID_ARGUMENT,
	TCTI_FEATURE_ARTIFACT_PIN_MISMATCH,
	TCTI_FEATURE_ARTIFACT_COUNT_MISMATCH,
	TCTI_FEATURE_ARTIFACT_POINTER_MISSING,
	TCTI_FEATURE_ARTIFACT_STRING_INVALID,
	TCTI_FEATURE_ARTIFACT_SOURCE_SPAN_INVALID,
	TCTI_FEATURE_ARTIFACT_SCOPE_INVALID,
	TCTI_FEATURE_ARTIFACT_REFERENCE_INVALID,
	TCTI_FEATURE_ARTIFACT_NODE_INVALID,
	TCTI_FEATURE_ARTIFACT_CYCLE,
	TCTI_FEATURE_ARTIFACT_GRAPH_COVERAGE_INVALID,
	TCTI_FEATURE_ARTIFACT_CHILD_COVERAGE_INVALID,
	TCTI_FEATURE_ARTIFACT_PROVENANCE_INVALID,
	TCTI_FEATURE_ARTIFACT_SCRATCH_TOO_SMALL,
};

struct tcti_feature_artifact_diagnostic {
	enum tcti_feature_artifact_error error;
	tcti_feature_artifact_u32 index;
};

struct tcti_feature_artifact_frame {
	tcti_feature_artifact_u32 node_index;
	tcti_feature_artifact_u64 next_edge;
};

struct tcti_feature_artifact_scratch {
	tcti_feature_artifact_u8 *state;
	size_t state_count;
	struct tcti_feature_artifact_frame *frames;
	size_t frame_count;
	tcti_feature_artifact_u8 *child_coverage;
	size_t child_coverage_count;
};

/*
 * `state` and `frames` must each provide at least counts.node_count entries.
 * They are scratch only, and are cleared before use.  Validation is iterative,
 * performs no allocation, and does not inspect raw maintainer input.  A zero-valued
 * runtime feature still has to occupy its parameter, constraint, and node
 * representation in this artifact.
 */
enum tcti_feature_artifact_error tcti_feature_artifact_validate(
	const struct tcti_feature_artifact *artifact,
	struct tcti_feature_artifact_scratch *scratch,
	struct tcti_feature_artifact_diagnostic *diagnostic);

/* The checked arch/orlix artifact is the sole production data source. */
const struct tcti_feature_artifact *tcti_feature_artifact_canonical(void);

const char *tcti_feature_artifact_error_name(
	enum tcti_feature_artifact_error error);

#endif
