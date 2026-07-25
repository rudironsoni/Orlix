/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TARGET_INVENTORY_IMPORT_H
#define ORLIX_TCTI_TARGET_INVENTORY_IMPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TCTI_A64_TARGET_LEAF_COUNT 4350U
#define TCTI_A64_TARGET_INSTRUCTION_ALIAS_COUNT 292U
#define TCTI_A64_TARGET_REACHABLE_OPERATION_ALIAS_COUNT 171U
#define TCTI_TARGET_EXPR_NONE UINT32_MAX

enum tcti_target_expr_kind {
	TCTI_TARGET_EXPR_BOOL,
	TCTI_TARGET_EXPR_FEATURE,
	TCTI_TARGET_EXPR_OPERAND,
	TCTI_TARGET_EXPR_VALUE,
	TCTI_TARGET_EXPR_SET,
	TCTI_TARGET_EXPR_NOT,
	TCTI_TARGET_EXPR_AND,
	TCTI_TARGET_EXPR_OR,
	TCTI_TARGET_EXPR_EQ,
	TCTI_TARGET_EXPR_NE,
	TCTI_TARGET_EXPR_IN,
};

struct tcti_target_expr {
	enum tcti_target_expr_kind kind;
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

struct tcti_target_leaf {
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
struct tcti_target_operand {
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
struct tcti_target_fixed_operand {
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
struct tcti_target_operation {
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
};

/*
 * A source-declared InstructionAlias. It is not a direct A64 target leaf and
 * therefore never changes TCTI_A64_TARGET_LEAF_COUNT. The ordinal records
 * source traversal order because alias display names are not globally unique.
 */
struct tcti_target_instruction_alias {
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

struct tcti_target_inventory {
	struct tcti_target_leaf *leaves;
	size_t leaf_count;
	size_t leaf_capacity;
	struct tcti_target_expr *expressions;
	size_t expression_count;
	size_t expression_capacity;
	uint32_t *set_items;
	size_t set_item_count;
	size_t set_item_capacity;
	struct tcti_target_operand *operands;
	size_t operand_count;
	size_t operand_capacity;
	size_t operand_name_bytes;
	struct tcti_target_fixed_operand *fixed_operands;
	size_t fixed_operand_count;
	size_t fixed_operand_capacity;
	size_t fixed_operand_name_bytes;
	struct tcti_target_operation *operations;
	size_t operation_count;
	size_t operation_capacity;
	struct tcti_target_instruction_alias *instruction_aliases;
	size_t instruction_alias_count;
	size_t instruction_alias_capacity;
	size_t reachable_operation_alias_count;
};

enum tcti_target_import_error_code {
	TCTI_TARGET_IMPORT_OK,
	TCTI_TARGET_IMPORT_INVALID_ARGUMENT,
	TCTI_TARGET_IMPORT_NO_MEMORY,
	TCTI_TARGET_IMPORT_INVALID_JSON,
	TCTI_TARGET_IMPORT_INVALID_SOURCE,
	TCTI_TARGET_IMPORT_UNSUPPORTED_GRAMMAR,
	TCTI_TARGET_IMPORT_COUNT_MISMATCH,
	TCTI_TARGET_IMPORT_INPUT_LIMIT,
	TCTI_TARGET_IMPORT_DEPTH_LIMIT,
	TCTI_TARGET_IMPORT_DUPLICATE_KEY,
	TCTI_TARGET_IMPORT_HASH_MISMATCH,
};

struct tcti_target_import_error {
	enum tcti_target_import_error_code code;
	size_t offset;
	char message[192];
};

int tcti_target_inventory_import(const char *json, size_t length,
				 struct tcti_target_inventory *inventory,
				 struct tcti_target_import_error *error);

void tcti_target_inventory_destroy(struct tcti_target_inventory *inventory);

const struct tcti_target_operation *
tcti_target_inventory_operation(const struct tcti_target_inventory *inventory,
				       const char *id);

/* SHA-256 used to bind a raw authoritative source span into a C artifact. */
void tcti_target_inventory_sha256(const void *data, size_t length,
					 char digest[65]);

#endif
