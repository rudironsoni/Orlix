/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_H
#define ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_H

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stddef.h>
#include <stdint.h>
typedef uint32_t u32;
typedef uint8_t u8;
#endif

/*
 * Fixed-width consumer view of the C artifact emitted by
 * target_instruction_artifact_generator.  The checked canonical header owns
 * the storage.  This view consumes no raw maintainer input and has no
 * import-model dependency, so an ordinary host or kernel-side consumer
 * validates the C data alone.  It accepts only compiler-emitted static artifact
 * storage.  Callers must not pass user-controlled or otherwise
 * untrusted C pointers, because C cannot validate pointer provenance before
 * dereferencing the fixed-width view.
 */
#define ORLIX_TCTI_A64_INSTRUCTION_ARTIFACT_VERSION 4U
#define ORLIX_TCTI_A64_INSTRUCTION_ARTIFACT_LEAF_COUNT 4350U
#define ORLIX_TCTI_A64_INSTRUCTION_ARTIFACT_INSTRUCTION_ALIAS_COUNT 292U
#define ORLIX_TCTI_A64_INSTRUCTION_ARTIFACT_OPERATION_ALIAS_COUNT 171U
#define ORLIX_TCTI_A64_INSTRUCTION_ARTIFACT_EXPECTED_FIXED_OPERAND_COUNT 16675U

enum orlix_tcti_target_alias_relation_kind {
	/* Reserved for a future source-declared direct-leaf duplicate edge. */
	ORLIX_TCTI_TARGET_ALIAS_RELATION_ENCODING = 1,
	/* Reserved for a future source-declared direct-leaf duplicate edge. */
	ORLIX_TCTI_TARGET_ALIAS_RELATION_DECODE = 2,
	ORLIX_TCTI_TARGET_ALIAS_RELATION_SEMANTIC = 3,
	ORLIX_TCTI_TARGET_ALIAS_RELATION_ASSEMBLER_ONLY = 4,
};

enum orlix_tcti_target_alias_predicate_kind {
	ORLIX_TCTI_TARGET_ALIAS_PREDICATE_SOURCE_CONDITION = 1,
	ORLIX_TCTI_TARGET_ALIAS_PREDICATE_SCHEMA_UNCONDITIONAL = 2,
};

struct orlix_tcti_target_instruction_artifact_leaf {
	u32 name_offset;
	u32 mnemonic_offset;
	u32 operation_offset;
	u32 encoding_mask;
	u32 encoding_pattern;
	u32 condition_offset;
	u32 condition_length;
	u32 operand_first;
	u32 operand_count;
	u32 fixed_operand_first;
	u32 fixed_operand_count;
};

struct orlix_tcti_target_instruction_artifact_operand {
	u32 name_offset;
	u32 leaf_index;
	u32 condition_offset;
	u32 condition_length;
	u32 variable_mask;
	u8 start;
	u8 width;
};

/*
 * A source-declared, complete fixed encoding field.  This is provenance for
 * source conditions, distinct from the runtime-variable operand plane.
 */
struct orlix_tcti_target_instruction_artifact_fixed_operand {
	u32 name_offset;
	u32 leaf_index;
	u32 condition_offset;
	u32 condition_length;
	u32 fixed_mask;
	u32 fixed_value;
	u32 source_offset;
	u32 source_length;
	u32 source_identity_offset;
	u8 start;
	u8 width;
};

/*
 * Source-declared InstructionAlias provenance.  These aliases remain
 * supplemental source relationships.  They never add an execution leaf or
 * close an owning proof obligation.  Direct-leaf containment is derived from
 * the source spans during maintainer validation, rather than duplicated in
 * the checked runtime artifact.
 */
struct orlix_tcti_target_instruction_artifact_instruction_alias {
	u32 ordinal;
	u32 name_offset;
	u32 declared_operation_offset;
	u32 resolved_operation_offset;
	u32 condition_offset;
	u32 condition_length;
	u32 source_offset;
	u32 source_length;
	u32 source_identity_offset;
	u32 condition_source_offset;
	u32 condition_source_length;
	u32 condition_identity_offset;
	u32 preferred_source_offset;
	u32 preferred_source_length;
	u32 preferred_identity_offset;
	u32 predicate_sha256_offset;
	u8 relation_kind;
	u8 predicate_kind;
	u8 preferred_present;
};

/*
 * A reachable OperationAlias and its bounded source-chain relationship.
 * declared_operation identifies this alias object, target_operation is the
 * immediate source declaration, and resolved_operation is its concrete
 * canonical target.  Its direct-leaf relationships are derived through the
 * InstructionAlias source edges during maintainer validation.
 */
struct orlix_tcti_target_instruction_artifact_operation_alias {
	u32 declared_operation_offset;
	u32 target_operation_offset;
	u32 resolved_operation_offset;
	u32 source_offset;
	u32 source_length;
	u32 source_identity_offset;
	u32 predicate_offset;
	u32 predicate_length;
	u32 predicate_sha256_offset;
	u8 relation_kind;
	u8 predicate_kind;
};

struct orlix_tcti_target_instruction_artifact {
	u32 version;
	const char *architecture;
	const char *build;
	const char *reference;
	const char *schema;
	const char *source_sha256;
	const struct orlix_tcti_target_instruction_artifact_leaf *leaves;
	size_t leaf_count;
	const struct orlix_tcti_target_instruction_artifact_operand *operands;
	size_t operand_count;
	const struct orlix_tcti_target_instruction_artifact_fixed_operand *fixed_operands;
	size_t fixed_operand_count;
	const struct orlix_tcti_target_instruction_artifact_instruction_alias
		*instruction_aliases;
	size_t instruction_alias_count;
	const struct orlix_tcti_target_instruction_artifact_operation_alias
		*operation_aliases;
	size_t operation_alias_count;
	const u8 *string_pool;
	size_t string_pool_size;
	const u8 *condition_pool;
	size_t condition_pool_size;
};

enum orlix_tcti_target_instruction_artifact_validation_error {
	ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_VALID = 0,
	ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_INVALID_ARGUMENT,
	ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_VERSION_MISMATCH,
	ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_PROVENANCE_MISMATCH,
	ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_COUNT_MISMATCH,
	ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_POOL_INVALID,
	ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_STRING_INVALID,
	ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_CONDITION_INVALID,
	ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_LEAF_INVALID,
	ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_OPERAND_INVALID,
	ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_FIXED_OPERAND_INVALID,
	ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_SPAN_INVALID,
	ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_ALIAS_INVALID,
	ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_ALIAS_IDENTITY_INVALID,
};

struct orlix_tcti_target_instruction_artifact_validation_result {
	enum orlix_tcti_target_instruction_artifact_validation_error error;
	u32 leaf_index;
	u32 operand_index;
	u32 alias_index;
};

/*
 * Validates the complete generated artifact, including exact pinned
 * provenance and every offset, string, condition, operand ownership, and
 * fixed/variable encoding bit relationship.  The leaf ordinal is its array
 * index, and the operands of ordinal N must occupy its contiguous span.  For
 * fixed fields, this validates the pinned-SHA:offset:length locator format
 * and its internal relationship to the generated record.  It deliberately
 * does not read raw Instructions.json bytes.  Raw source-span containment and
 * byte identity are importer and maintainer source-check responsibilities.
 */
int orlix_tcti_target_instruction_artifact_validate(
	const struct orlix_tcti_target_instruction_artifact *artifact,
	struct orlix_tcti_target_instruction_artifact_validation_result *result);

const struct orlix_tcti_target_instruction_artifact *
orlix_tcti_target_instruction_artifact_canonical(void);

const char *orlix_tcti_target_instruction_artifact_validation_error_name(
	enum orlix_tcti_target_instruction_artifact_validation_error error);

#endif /* ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_H */
