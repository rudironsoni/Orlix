/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TARGET_INVENTORY_IMPORT_H
#define ORLIX_TCTI_TARGET_INVENTORY_IMPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ORLIX_TCTI_A64_TARGET_LEAF_COUNT 4350U
#define ORLIX_TCTI_A64_TARGET_INSTRUCTION_ALIAS_COUNT 292U
#define ORLIX_TCTI_A64_TARGET_REACHABLE_OPERATION_ALIAS_COUNT 171U
#define ORLIX_TCTI_TARGET_EXPR_NONE UINT32_MAX

enum orlix_tcti_target_expr_kind {
	ORLIX_TCTI_TARGET_EXPR_BOOL,
	ORLIX_TCTI_TARGET_EXPR_FEATURE,
	ORLIX_TCTI_TARGET_EXPR_OPERAND,
	ORLIX_TCTI_TARGET_EXPR_VALUE,
	ORLIX_TCTI_TARGET_EXPR_SET,
	ORLIX_TCTI_TARGET_EXPR_NOT,
	ORLIX_TCTI_TARGET_EXPR_AND,
	ORLIX_TCTI_TARGET_EXPR_OR,
	ORLIX_TCTI_TARGET_EXPR_EQ,
	ORLIX_TCTI_TARGET_EXPR_NE,
	ORLIX_TCTI_TARGET_EXPR_IN,
};

struct orlix_tcti_target_expr {
	enum orlix_tcti_target_expr_kind kind;
	uint32_t left;
	uint32_t right;
	uint32_t first_item;
	uint32_t item_count;
	char *text;
	bool boolean;
	/* Exact raw AST object span, retained for source-to-proof bindings. */
	size_t source_offset;
	size_t source_length;
};

struct orlix_tcti_target_leaf {
	char *name;
	char *mnemonic;
	char *operation_id;
	uint32_t encoding_mask;
	uint32_t encoding_pattern;
	uint32_t condition;
	/* Raw byte span of this Instruction.Instruction object in Instructions.json. */
	size_t source_offset;
	size_t source_length;
	size_t condition_source_offset;
	size_t condition_source_length;
	bool preferred_present;
	size_t preferred_source_offset;
	size_t preferred_source_length;
};

/*
 * A named encoding field which contains at least one variable ('x') bit.
 * The field remains owned by one source leaf and carries that leaf's complete
 * inherited source condition. variable_mask is in instruction-bit numbering,
 * so consumers do not have to reconstruct which part of a mixed field varies.
 */
struct orlix_tcti_target_operand {
	char *name;
	uint32_t leaf_index;
	uint32_t condition;
	uint32_t variable_mask;
	uint8_t start;
	uint8_t width;
};

/*
 * A named encoding field whose complete bit range is fixed by one source
 * leaf.  It is retained for source-to-condition provenance, but is never a
 * runtime-variable operand.
 */
struct orlix_tcti_target_fixed_operand {
	char *name;
	uint32_t leaf_index;
	uint32_t condition;
	uint32_t fixed_mask;
	uint32_t fixed_value;
	uint8_t start;
	uint8_t width;
	size_t source_offset;
	size_t source_length;
};

/* An authoritative inline AARCHMRS operation object, not shared-ASL corpus data. */
enum orlix_tcti_target_operation_body_state {
	ORLIX_TCTI_TARGET_OPERATION_BODY_ABSENT,
	ORLIX_TCTI_TARGET_OPERATION_BODY_PLACEHOLDER,
	ORLIX_TCTI_TARGET_OPERATION_BODY_PRESENT,
};

enum orlix_tcti_target_operation_decode_state {
	ORLIX_TCTI_TARGET_OPERATION_DECODE_ABSENT,
	ORLIX_TCTI_TARGET_OPERATION_DECODE_NULL,
	ORLIX_TCTI_TARGET_OPERATION_DECODE_PRESENT,
};

struct orlix_tcti_target_operation {
	char *id;
	/* OperationAlias target before resolution, NULL for a concrete operation. */
	char *alias_operation_id;
	/* Canonical concrete operation after bounded alias-chain resolution. */
	char *canonical_operation_id;
	bool is_alias;
	size_t source_offset;
	size_t source_length;
	/* Raw source provenance of the operation's optional operational_note. */
	bool operational_note_present;
	size_t operational_note_source_offset;
	size_t operational_note_source_length;
	/*
	 * Exact source witnesses for the operation and decode members.  These are
	 * availability facts only.  They cannot supply semantic provenance without
	 * an authoritative shared-ASL corpus and its helper dependencies.
	 */
	enum orlix_tcti_target_operation_body_state semantic_body_state;
	size_t semantic_member_source_offset;
	size_t semantic_member_source_length;
	size_t semantic_body_source_offset;
	size_t semantic_body_source_length;
	char semantic_body_sha256[65];
	enum orlix_tcti_target_operation_decode_state decode_state;
	size_t decode_member_source_offset;
	size_t decode_member_source_length;
	size_t decode_source_offset;
	size_t decode_source_length;
	char decode_sha256[65];
};

/*
 * A source-declared InstructionAlias. It is not a direct A64 target leaf and
 * therefore never changes ORLIX_TCTI_A64_TARGET_LEAF_COUNT. The ordinal records
 * source traversal order because alias display names are not globally unique.
 */
struct orlix_tcti_target_instruction_alias {
	char *name;
	char *operation_id;
	char *canonical_operation_id;
	uint32_t condition;
	uint32_t ordinal;
	bool preferred_present;
	size_t source_offset;
	size_t source_length;
	size_t condition_source_offset;
	size_t condition_source_length;
	size_t preferred_source_offset;
	size_t preferred_source_length;
};

struct orlix_tcti_target_inventory {
	struct orlix_tcti_target_leaf *leaves;
	size_t leaf_count;
	size_t leaf_capacity;
	struct orlix_tcti_target_expr *expressions;
	size_t expression_count;
	size_t expression_capacity;
	uint32_t *set_items;
	size_t set_item_count;
	size_t set_item_capacity;
	struct orlix_tcti_target_operand *operands;
	size_t operand_count;
	size_t operand_capacity;
	size_t operand_name_bytes;
	struct orlix_tcti_target_fixed_operand *fixed_operands;
	size_t fixed_operand_count;
	size_t fixed_operand_capacity;
	size_t fixed_operand_name_bytes;
	struct orlix_tcti_target_operation *operations;
	size_t operation_count;
	size_t operation_capacity;
	struct orlix_tcti_target_instruction_alias *instruction_aliases;
	size_t instruction_alias_count;
	size_t instruction_alias_capacity;
	size_t reachable_operation_alias_count;
};

enum orlix_tcti_target_import_error_code {
	ORLIX_TCTI_TARGET_IMPORT_OK,
	ORLIX_TCTI_TARGET_IMPORT_INVALID_ARGUMENT,
	ORLIX_TCTI_TARGET_IMPORT_NO_MEMORY,
	ORLIX_TCTI_TARGET_IMPORT_INVALID_JSON,
	ORLIX_TCTI_TARGET_IMPORT_INVALID_SOURCE,
	ORLIX_TCTI_TARGET_IMPORT_UNSUPPORTED_GRAMMAR,
	ORLIX_TCTI_TARGET_IMPORT_COUNT_MISMATCH,
	ORLIX_TCTI_TARGET_IMPORT_INPUT_LIMIT,
	ORLIX_TCTI_TARGET_IMPORT_DEPTH_LIMIT,
	ORLIX_TCTI_TARGET_IMPORT_DUPLICATE_KEY,
	ORLIX_TCTI_TARGET_IMPORT_HASH_MISMATCH,
};

struct orlix_tcti_target_import_error {
	enum orlix_tcti_target_import_error_code code;
	size_t offset;
	char message[192];
};

int orlix_tcti_target_inventory_import(const char *json, size_t length,
				 struct orlix_tcti_target_inventory *inventory,
				 struct orlix_tcti_target_import_error *error);

void orlix_tcti_target_inventory_destroy(struct orlix_tcti_target_inventory *inventory);

const struct orlix_tcti_target_operation *
orlix_tcti_target_inventory_operation(const struct orlix_tcti_target_inventory *inventory,
				       const char *id);

/* SHA-256 used to bind a raw authoritative source span into a C artifact. */
void orlix_tcti_target_inventory_sha256(const void *data, size_t length,
					 char digest[65]);

#endif
