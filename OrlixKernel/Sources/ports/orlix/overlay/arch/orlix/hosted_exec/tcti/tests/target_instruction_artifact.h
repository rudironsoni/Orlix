/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_H
#define ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_H

#include <stddef.h>

#ifdef __KERNEL__
#include <linux/types.h>
#else
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
#define TCTI_A64_INSTRUCTION_ARTIFACT_VERSION 1U
#define TCTI_A64_INSTRUCTION_ARTIFACT_LEAF_COUNT 4350U

struct tcti_target_instruction_artifact_leaf {
	u32 name_offset;
	u32 mnemonic_offset;
	u32 operation_offset;
	u32 encoding_mask;
	u32 encoding_pattern;
	u32 condition_offset;
	u32 condition_length;
	u32 operand_first;
	u32 operand_count;
};

struct tcti_target_instruction_artifact_operand {
	u32 name_offset;
	u32 leaf_index;
	u32 condition_offset;
	u32 condition_length;
	u32 variable_mask;
	u8 start;
	u8 width;
};

struct tcti_target_instruction_artifact {
	u32 version;
	const char *architecture;
	const char *build;
	const char *reference;
	const char *schema;
	const char *source_sha256;
	const struct tcti_target_instruction_artifact_leaf *leaves;
	size_t leaf_count;
	const struct tcti_target_instruction_artifact_operand *operands;
	size_t operand_count;
	const u8 *string_pool;
	size_t string_pool_size;
	const u8 *condition_pool;
	size_t condition_pool_size;
};

enum tcti_target_instruction_artifact_validation_error {
	TCTI_TARGET_INSTRUCTION_ARTIFACT_VALID = 0,
	TCTI_TARGET_INSTRUCTION_ARTIFACT_INVALID_ARGUMENT,
	TCTI_TARGET_INSTRUCTION_ARTIFACT_VERSION_MISMATCH,
	TCTI_TARGET_INSTRUCTION_ARTIFACT_PROVENANCE_MISMATCH,
	TCTI_TARGET_INSTRUCTION_ARTIFACT_COUNT_MISMATCH,
	TCTI_TARGET_INSTRUCTION_ARTIFACT_POOL_INVALID,
	TCTI_TARGET_INSTRUCTION_ARTIFACT_STRING_INVALID,
	TCTI_TARGET_INSTRUCTION_ARTIFACT_CONDITION_INVALID,
	TCTI_TARGET_INSTRUCTION_ARTIFACT_LEAF_INVALID,
	TCTI_TARGET_INSTRUCTION_ARTIFACT_OPERAND_INVALID,
	TCTI_TARGET_INSTRUCTION_ARTIFACT_SPAN_INVALID,
};

struct tcti_target_instruction_artifact_validation_result {
	enum tcti_target_instruction_artifact_validation_error error;
	u32 leaf_index;
	u32 operand_index;
};

/*
 * Validates the complete generated artifact, including exact pinned
 * provenance and every offset, string, condition, operand ownership, and
 * fixed/variable encoding bit relationship.  The leaf ordinal is its array
 * index, and the operands of ordinal N must occupy its contiguous span.
 */
int tcti_target_instruction_artifact_validate(
	const struct tcti_target_instruction_artifact *artifact,
	struct tcti_target_instruction_artifact_validation_result *result);

const struct tcti_target_instruction_artifact *
tcti_target_instruction_artifact_canonical(void);

const char *tcti_target_instruction_artifact_validation_error_name(
	enum tcti_target_instruction_artifact_validation_error error);

#endif /* ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_H */
