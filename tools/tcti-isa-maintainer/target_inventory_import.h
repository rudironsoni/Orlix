/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TARGET_INVENTORY_IMPORT_H
#define ORLIX_TCTI_TARGET_INVENTORY_IMPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TCTI_A64_TARGET_LEAF_COUNT 4350U
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
};

struct tcti_target_leaf {
	char *name;
	char *mnemonic;
	char *operation_id;
	uint32_t encoding_mask;
	uint32_t encoding_pattern;
	uint32_t condition;
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

#endif
