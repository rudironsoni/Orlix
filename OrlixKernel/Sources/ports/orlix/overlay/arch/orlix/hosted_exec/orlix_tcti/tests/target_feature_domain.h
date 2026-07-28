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
#include "target_instruction_artifact.h"

enum orlix_tcti_feature_domain_value_kind {
	ORLIX_TCTI_FEATURE_DOMAIN_VALUE_INVALID,
	ORLIX_TCTI_FEATURE_DOMAIN_VALUE_BOOL,
	ORLIX_TCTI_FEATURE_DOMAIN_VALUE_SIGNED,
	ORLIX_TCTI_FEATURE_DOMAIN_VALUE_UNSIGNED,
	ORLIX_TCTI_FEATURE_DOMAIN_VALUE_ATOM,
	ORLIX_TCTI_FEATURE_DOMAIN_VALUE_DOT_ATOM,
	ORLIX_TCTI_FEATURE_DOMAIN_VALUE_SET,
};

/*
 * The Arm feature source permits UInt and SInt over Values.Value bit strings.
 * Keep the source width and both limbs explicitly.  Do not depend on a host
 * compiler's optional __int128 extension, and do not silently widen a source
 * literal beyond the 128-bit form this evaluator currently owns.
 */
struct orlix_tcti_feature_domain_integer {
	orlix_tcti_feature_artifact_u64 low;
	orlix_tcti_feature_artifact_u64 high;
	orlix_tcti_feature_artifact_u8 width;
};

struct orlix_tcti_feature_domain_value {
	enum orlix_tcti_feature_domain_value_kind kind;
	orlix_tcti_feature_artifact_u8 boolean;
	struct orlix_tcti_feature_domain_integer integer;
	const char *text;
	orlix_tcti_feature_artifact_u32 node_index;
};

/*
 * Lookup callbacks return zero only after assigning a value with a valid kind.
 * A callback cannot use VALUE_SET or VALUE_DOT_ATOM because those forms are
 * artifact-owned composite values.  It must return a scalar, Boolean, or atom.
 */
struct orlix_tcti_feature_domain_environment {
	void *context;
	int (*feature)(void *context, const char *name,
		struct orlix_tcti_feature_domain_value *value);
	int (*configuration)(void *context,
		orlix_tcti_feature_artifact_u32 node_index,
		enum orlix_tcti_feature_domain_value_kind numeric_kind,
		orlix_tcti_feature_artifact_u32 width,
		struct orlix_tcti_feature_domain_value *value);
	int (*field)(void *context, const char *state, const char *register_name,
		     const char *selector, struct orlix_tcti_feature_domain_value *value);
};

enum orlix_tcti_feature_domain_error {
	ORLIX_TCTI_FEATURE_DOMAIN_OK,
	ORLIX_TCTI_FEATURE_DOMAIN_INVALID_ARGUMENT,
	ORLIX_TCTI_FEATURE_DOMAIN_REFERENCE,
	ORLIX_TCTI_FEATURE_DOMAIN_CYCLE,
	ORLIX_TCTI_FEATURE_DOMAIN_DEPTH,
	ORLIX_TCTI_FEATURE_DOMAIN_MISSING_FEATURE,
	ORLIX_TCTI_FEATURE_DOMAIN_MISSING_CONFIGURATION,
	ORLIX_TCTI_FEATURE_DOMAIN_MISSING_FIELD,
	ORLIX_TCTI_FEATURE_DOMAIN_CALLBACK_VALUE,
	ORLIX_TCTI_FEATURE_DOMAIN_TYPE,
	ORLIX_TCTI_FEATURE_DOMAIN_OVERFLOW,
	ORLIX_TCTI_FEATURE_DOMAIN_SET,
	ORLIX_TCTI_FEATURE_DOMAIN_NODE,
	ORLIX_TCTI_FEATURE_DOMAIN_SCRATCH,
};

struct orlix_tcti_feature_domain_diagnostic {
	enum orlix_tcti_feature_domain_error error;
	orlix_tcti_feature_artifact_u32 node_index;
};

/*
 * Parse the exact quoted-binary Values.Value representation used by Arm
 * feature expressions, for example "'011'".  The result retains its source
 * width, accepts one through 128 bits, and rejects every other text form.
 */
int orlix_tcti_feature_domain_parse_uint_literal(const char *text,
	struct orlix_tcti_feature_domain_value *value);
int orlix_tcti_feature_domain_parse_sint_literal(const char *text,
	struct orlix_tcti_feature_domain_value *value);

/*
 * Compare same-kind exact integers after normalizing both values to the wider
 * source width. Signed values sign-extend and unsigned values zero-extend.
 * `order` receives -1, 0, or +1. Mixed signedness, absent widths, values
 * wider than 128 bits, and all non-numeric forms fail closed.
 */
int orlix_tcti_feature_domain_compare_numeric(
	const struct orlix_tcti_feature_domain_value *left,
	const struct orlix_tcti_feature_domain_value *right, int *order);

struct orlix_tcti_feature_domain_scratch {
	orlix_tcti_feature_artifact_u8 *active;
	size_t active_count;
	orlix_tcti_feature_artifact_u32 max_depth;
};

/*
 * Validate one TCND v1 instruction-source condition against the checked Arm
 * feature artifact.  Every FEATURE terminal must name an artifact parameter.
 * Operand and value terminals are retained as instruction-semantic variants,
 * rather than being coerced into a feature result.  This validates the source
 * binding only.  A separate exact feature-domain SAT result is required to
 * establish applicability across the full Arm configuration union.
 */
enum orlix_tcti_feature_domain_tcnd_error {
	ORLIX_TCTI_FEATURE_DOMAIN_TCND_OK,
	ORLIX_TCTI_FEATURE_DOMAIN_TCND_INVALID_ARGUMENT,
	ORLIX_TCTI_FEATURE_DOMAIN_TCND_ENCODING,
	ORLIX_TCTI_FEATURE_DOMAIN_TCND_DEPTH,
	ORLIX_TCTI_FEATURE_DOMAIN_TCND_UNKNOWN_FEATURE,
	ORLIX_TCTI_FEATURE_DOMAIN_TCND_MISSING_FEATURE,
	ORLIX_TCTI_FEATURE_DOMAIN_TCND_MISSING_OPERAND,
	ORLIX_TCTI_FEATURE_DOMAIN_TCND_CALLBACK_VALUE,
	ORLIX_TCTI_FEATURE_DOMAIN_TCND_TYPE,
};

struct orlix_tcti_feature_domain_tcnd_diagnostic {
	enum orlix_tcti_feature_domain_tcnd_error error;
	size_t byte_offset;
	size_t feature_terminals;
};

int orlix_tcti_feature_domain_validate_tcnd_features(
	const struct orlix_tcti_feature_artifact *artifact, const char *tcnd_hex,
	struct orlix_tcti_feature_domain_tcnd_diagnostic *diagnostic);

/*
 * Exact TCND v1 evaluation for one explicit target configuration.  The
 * callbacks receive an offset and byte length into the checked TCND hex
 * string, so evaluation never relies on temporary NUL-terminated copies of
 * Arm-owned data. `feature` supplies a Boolean feature state. `operand`
 * supplies the exact source-value spelling of one instruction operand,
 * including its length. Every missing
 * callback, unknown feature, malformed record, unsupported value kind, and
 * type mismatch fails closed with a typed diagnostic.
 */
struct orlix_tcti_feature_domain_tcnd_environment {
	void *context;
	int (*feature)(void *context, const char *tcnd_hex,
		       size_t byte_offset, size_t byte_length,
		       orlix_tcti_feature_artifact_u8 *enabled);
	int (*operand)(void *context, const char *tcnd_hex,
		       size_t byte_offset, size_t byte_length,
		       const char **value, size_t *value_length);
};

/*
 * Generated-instruction operand source for one concrete encoded instruction.
 * The callback below resolves only operand names and bit ranges emitted by the
 * checked instruction artifact. It does not classify, execute, or prove the
 * selected leaf.
 */
struct orlix_tcti_target_instruction_operand_assignment {
	const struct orlix_tcti_target_instruction_artifact *artifact;
	orlix_tcti_feature_artifact_u32 leaf_index;
	orlix_tcti_feature_artifact_u32 instruction;
	char value[35];
};

int orlix_tcti_target_instruction_operand_assignment(void *context,
	const char *tcnd_hex, size_t byte_offset, size_t byte_length,
	const char **value, size_t *value_length);

int orlix_tcti_feature_domain_evaluate_tcnd(
	const struct orlix_tcti_feature_artifact *artifact, const char *tcnd_hex,
	const struct orlix_tcti_feature_domain_tcnd_environment *environment,
	orlix_tcti_feature_artifact_u8 *satisfied,
	struct orlix_tcti_feature_domain_tcnd_diagnostic *diagnostic);

struct orlix_tcti_feature_domain_tcnd_union_candidate {
	const struct orlix_tcti_feature_domain_tcnd_environment *environment;
};

struct orlix_tcti_feature_domain_tcnd_union_result {
	size_t candidate_count;
	size_t evaluated_count;
	size_t satisfied_count;
	size_t unsatisfied_count;
	size_t unsupported_count;
	struct orlix_tcti_feature_domain_tcnd_diagnostic first_unsupported;
};

/* Evaluate one source condition across every supplied target configuration. */
int orlix_tcti_feature_domain_evaluate_tcnd_union(
	const struct orlix_tcti_feature_artifact *artifact, const char *tcnd_hex,
	const struct orlix_tcti_feature_domain_tcnd_union_candidate *candidates,
	size_t candidate_count, struct orlix_tcti_feature_domain_tcnd_union_result *result);

/* Evaluate one feature-constraint root or return a typed hard failure. */
int orlix_tcti_feature_domain_evaluate(
	const struct orlix_tcti_feature_artifact *artifact,
	orlix_tcti_feature_artifact_u32 root,
	const struct orlix_tcti_feature_domain_environment *environment,
	struct orlix_tcti_feature_domain_scratch *scratch,
	struct orlix_tcti_feature_domain_value *value,
	struct orlix_tcti_feature_domain_diagnostic *diagnostic);

/*
 * Evaluate every checked feature constraint.  `satisfied` is assigned only on
 * success and is one when the complete constraint conjunction holds.
 */
int orlix_tcti_feature_domain_evaluate_constraints(
	const struct orlix_tcti_feature_artifact *artifact,
	const struct orlix_tcti_feature_domain_environment *environment,
	struct orlix_tcti_feature_domain_scratch *scratch,
	orlix_tcti_feature_artifact_u8 *satisfied,
	struct orlix_tcti_feature_domain_diagnostic *diagnostic);

/*
 * A target leaf is applicable when at least one explicitly supplied Arm
 * feature configuration satisfies its constraints.  This is a target-domain
 * union, not a runtime HWCAP projection.  Every supplied configuration is
 * evaluated, so an unsupported field relation or missing feature callback is
 * retained in `unsupported_count` even when another configuration satisfies
 * the same constraints.
 *
 * The caller owns one scratch object for each candidate because evaluation is
 * deliberately allocation-free.  A hard evaluation failure returns -1 after
 * recording the first diagnostic and does not make the offending candidate
 * disappear from the union.
 */
struct orlix_tcti_feature_domain_union_candidate {
	const struct orlix_tcti_feature_domain_environment *environment;
};

struct orlix_tcti_feature_domain_union_scratch {
	struct orlix_tcti_feature_domain_scratch *evaluators;
	size_t evaluator_count;
};

struct orlix_tcti_feature_domain_union_result {
	size_t candidate_count;
	size_t evaluated_count;
	size_t satisfied_count;
	size_t unsatisfied_count;
	size_t unsupported_count;
	struct orlix_tcti_feature_domain_diagnostic first_unsupported;
};

int orlix_tcti_feature_domain_evaluate_constraint_union(
	const struct orlix_tcti_feature_artifact *artifact,
	const struct orlix_tcti_feature_domain_union_candidate *candidates,
	size_t candidate_count,
	struct orlix_tcti_feature_domain_union_scratch *scratch,
	struct orlix_tcti_feature_domain_union_result *result);

#endif /* ORLIX_TCTI_TARGET_FEATURE_DOMAIN_H */
