/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Typed evaluator for the checked Arm feature-domain artifact.  This is not a
 * runtime HWCAP projection.  Callers provide an explicit candidate feature and
 * register-field assignment and receive either its exact result or a typed
 * failure.  A missing value or unsupported relation is never coerced to false.
 */
#ifndef ORLIX_TCTI_TARGET_FEATURE_DOMAIN_H
#define ORLIX_TCTI_TARGET_FEATURE_DOMAIN_H

#include "target_feature_artifact.h"

enum tcti_feature_domain_value_kind {
	TCTI_FEATURE_DOMAIN_VALUE_INVALID,
	TCTI_FEATURE_DOMAIN_VALUE_BOOL,
	TCTI_FEATURE_DOMAIN_VALUE_SIGNED,
	TCTI_FEATURE_DOMAIN_VALUE_UNSIGNED,
	TCTI_FEATURE_DOMAIN_VALUE_ATOM,
	TCTI_FEATURE_DOMAIN_VALUE_DOT_ATOM,
	TCTI_FEATURE_DOMAIN_VALUE_SET,
};

/*
 * The Arm feature source permits UInt and SInt over Values.Value bit strings.
 * Keep the source width and both limbs explicitly.  Do not depend on a host
 * compiler's optional __int128 extension, and do not silently widen a source
 * literal beyond the 128-bit form this evaluator currently owns.
 */
struct tcti_feature_domain_integer {
	tcti_feature_artifact_u64 low;
	tcti_feature_artifact_u64 high;
	tcti_feature_artifact_u8 width;
};

struct tcti_feature_domain_value {
	enum tcti_feature_domain_value_kind kind;
	tcti_feature_artifact_u8 boolean;
	struct tcti_feature_domain_integer integer;
	const char *text;
	tcti_feature_artifact_u32 node_index;
};

/*
 * Lookup callbacks return zero only after assigning a value with a valid kind.
 * A callback cannot use VALUE_SET or VALUE_DOT_ATOM because those forms are
 * artifact-owned composite values.  It must return a scalar, Boolean, or atom.
 */
struct tcti_feature_domain_environment {
	void *context;
	int (*feature)(void *context, const char *name,
		       struct tcti_feature_domain_value *value);
	int (*field)(void *context, const char *state, const char *register_name,
		     const char *selector, struct tcti_feature_domain_value *value);
};

enum tcti_feature_domain_error {
	TCTI_FEATURE_DOMAIN_OK,
	TCTI_FEATURE_DOMAIN_INVALID_ARGUMENT,
	TCTI_FEATURE_DOMAIN_REFERENCE,
	TCTI_FEATURE_DOMAIN_CYCLE,
	TCTI_FEATURE_DOMAIN_DEPTH,
	TCTI_FEATURE_DOMAIN_MISSING_FEATURE,
	TCTI_FEATURE_DOMAIN_MISSING_FIELD,
	TCTI_FEATURE_DOMAIN_CALLBACK_VALUE,
	TCTI_FEATURE_DOMAIN_TYPE,
	TCTI_FEATURE_DOMAIN_OVERFLOW,
	TCTI_FEATURE_DOMAIN_SET,
	TCTI_FEATURE_DOMAIN_NODE,
	TCTI_FEATURE_DOMAIN_SCRATCH,
};

struct tcti_feature_domain_diagnostic {
	enum tcti_feature_domain_error error;
	tcti_feature_artifact_u32 node_index;
};

/*
 * Parse the exact quoted-binary Values.Value representation used by Arm
 * feature expressions, for example "'011'".  The result retains its source
 * width, accepts one through 128 bits, and rejects every other text form.
 */
int tcti_feature_domain_parse_uint_literal(const char *text,
	struct tcti_feature_domain_value *value);
int tcti_feature_domain_parse_sint_literal(const char *text,
	struct tcti_feature_domain_value *value);

/*
 * Compare same-kind exact integers after normalizing both values to the wider
 * source width. Signed values sign-extend and unsigned values zero-extend.
 * `order` receives -1, 0, or +1. Mixed signedness, absent widths, values
 * wider than 128 bits, and all non-numeric forms fail closed.
 */
int tcti_feature_domain_compare_numeric(
	const struct tcti_feature_domain_value *left,
	const struct tcti_feature_domain_value *right, int *order);

struct tcti_feature_domain_scratch {
	tcti_feature_artifact_u8 *active;
	size_t active_count;
	tcti_feature_artifact_u32 max_depth;
};

/*
 * Validate one TCND v1 instruction-source condition against the checked Arm
 * feature artifact.  Every FEATURE terminal must name an artifact parameter.
 * Operand and value terminals are retained as instruction-semantic variants,
 * rather than being coerced into a feature result.  This validates the source
 * binding only.  A separate exact feature-domain SAT result is required to
 * establish applicability across the full Arm configuration union.
 */
enum tcti_feature_domain_tcnd_error {
	TCTI_FEATURE_DOMAIN_TCND_OK,
	TCTI_FEATURE_DOMAIN_TCND_INVALID_ARGUMENT,
	TCTI_FEATURE_DOMAIN_TCND_ENCODING,
	TCTI_FEATURE_DOMAIN_TCND_DEPTH,
	TCTI_FEATURE_DOMAIN_TCND_UNKNOWN_FEATURE,
};

struct tcti_feature_domain_tcnd_diagnostic {
	enum tcti_feature_domain_tcnd_error error;
	size_t byte_offset;
	size_t feature_terminals;
};

int tcti_feature_domain_validate_tcnd_features(
	const struct tcti_feature_artifact *artifact, const char *tcnd_hex,
	struct tcti_feature_domain_tcnd_diagnostic *diagnostic);

/* Evaluate one feature-constraint root or return a typed hard failure. */
int tcti_feature_domain_evaluate(
	const struct tcti_feature_artifact *artifact,
	tcti_feature_artifact_u32 root,
	const struct tcti_feature_domain_environment *environment,
	struct tcti_feature_domain_scratch *scratch,
	struct tcti_feature_domain_value *value,
	struct tcti_feature_domain_diagnostic *diagnostic);

/*
 * Evaluate every checked feature constraint.  `satisfied` is assigned only on
 * success and is one when the complete constraint conjunction holds.
 */
int tcti_feature_domain_evaluate_constraints(
	const struct tcti_feature_artifact *artifact,
	const struct tcti_feature_domain_environment *environment,
	struct tcti_feature_domain_scratch *scratch,
	tcti_feature_artifact_u8 *satisfied,
	struct tcti_feature_domain_diagnostic *diagnostic);

#endif /* ORLIX_TCTI_TARGET_FEATURE_DOMAIN_H */
