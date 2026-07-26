/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TYPED_EXPRESSION_H
#define ORLIX_TCTI_TYPED_EXPRESSION_H
#include <stddef.h>
#include <stdint.h>
#define ORLIX_TCTI_TYPED_NONE UINT32_MAX
#define ORLIX_TCTI_TYPED_MAX_DEPTH 128U
#define ORLIX_TCTI_TYPED_VALUE_MAX_WIDTH 128U
/* The types retain the source-language distinctions until the typed solver
 * deliberately assigns a concrete feature-domain interpretation. */
enum orlix_tcti_typed_type {
	ORLIX_TCTI_TYPED_BOOL, ORLIX_TCTI_TYPED_UNSIGNED, ORLIX_TCTI_TYPED_SIGNED,
	ORLIX_TCTI_TYPED_SET, ORLIX_TCTI_TYPED_IDENTIFIER, ORLIX_TCTI_TYPED_FIELD,
	ORLIX_TCTI_TYPED_VALUE, ORLIX_TCTI_TYPED_DOT_ATOM, ORLIX_TCTI_TYPED_OPERAND,
	ORLIX_TCTI_TYPED_UNRESOLVED_BOOLEAN,
};
enum orlix_tcti_typed_op {
	ORLIX_TCTI_TYPED_LITERAL, ORLIX_TCTI_TYPED_EQ, ORLIX_TCTI_TYPED_NE, ORLIX_TCTI_TYPED_LT,
	ORLIX_TCTI_TYPED_GT, ORLIX_TCTI_TYPED_GE, ORLIX_TCTI_TYPED_IN, ORLIX_TCTI_TYPED_NOT,
	ORLIX_TCTI_TYPED_AND, ORLIX_TCTI_TYPED_OR, ORLIX_TCTI_TYPED_IMPLIES, ORLIX_TCTI_TYPED_IFF,
	ORLIX_TCTI_TYPED_UINT, ORLIX_TCTI_TYPED_SINT, ORLIX_TCTI_TYPED_SET_LITERAL,
	ORLIX_TCTI_TYPED_DOT_ATOM_LITERAL,
};
struct orlix_tcti_typed_provenance { size_t offset, length; };
/* AARCHMRS Values.Value bit strings are MSB-first and may contain wildcard
 * bits. Keep both 64-bit limbs so the maintainer tool never depends on a host
 * integer wider than the C11 types it explicitly names. */
struct orlix_tcti_typed_value {
	uint64_t value[2];
	uint64_t known_mask[2];
	uint8_t width;
};
struct orlix_tcti_typed_identity {
	char *text;
	char *state;
	char *register_name;
	char *selector;
	uint8_t parameter;
};
struct orlix_tcti_typed_node {
	enum orlix_tcti_typed_type type, element_type;
	enum orlix_tcti_typed_op op;
	uint8_t width, element_width, depth;
	uint32_t first_child, child_count, parent;
	struct orlix_tcti_typed_value value;
	struct orlix_tcti_typed_identity identity;
	struct orlix_tcti_typed_provenance provenance;
};
struct orlix_tcti_typed_expression {
	struct orlix_tcti_typed_node *nodes;
	size_t node_count, node_capacity;
	uint32_t *children;
	size_t child_count, child_capacity;
};
enum orlix_tcti_typed_error {
	ORLIX_TCTI_TYPED_OK, ORLIX_TCTI_TYPED_ARGUMENT, ORLIX_TCTI_TYPED_TYPE, ORLIX_TCTI_TYPED_ARITY,
	ORLIX_TCTI_TYPED_LIMIT, ORLIX_TCTI_TYPED_MEMORY, ORLIX_TCTI_TYPED_OWNED,
	ORLIX_TCTI_TYPED_VALUE_INVALID,
};
int orlix_tcti_typed_value_from_u64(struct orlix_tcti_typed_value *, uint64_t, uint8_t,
	enum orlix_tcti_typed_error *);
int orlix_tcti_typed_value_parse(struct orlix_tcti_typed_value *, const char *,
	enum orlix_tcti_typed_error *);
int orlix_tcti_typed_literal(struct orlix_tcti_typed_expression *, enum orlix_tcti_typed_type,
	uint8_t, const struct orlix_tcti_typed_value *, enum orlix_tcti_typed_type, uint8_t,
	struct orlix_tcti_typed_provenance, uint32_t *, enum orlix_tcti_typed_error *);
int orlix_tcti_typed_value_atom(struct orlix_tcti_typed_expression *, const char *,
	struct orlix_tcti_typed_provenance, uint32_t *, enum orlix_tcti_typed_error *);
int orlix_tcti_typed_atom(struct orlix_tcti_typed_expression *, enum orlix_tcti_typed_type,
	const char *, const char *, const char *, const char *,
	struct orlix_tcti_typed_provenance, uint32_t *, enum orlix_tcti_typed_error *);
int orlix_tcti_typed_boolean_atom(struct orlix_tcti_typed_expression *, const char *,
	int, struct orlix_tcti_typed_provenance, uint32_t *,
	enum orlix_tcti_typed_error *);
int orlix_tcti_typed_compound(struct orlix_tcti_typed_expression *,
	enum orlix_tcti_typed_type, const uint32_t *, size_t,
	struct orlix_tcti_typed_provenance, uint32_t *, enum orlix_tcti_typed_error *);
int orlix_tcti_typed_apply(struct orlix_tcti_typed_expression *, enum orlix_tcti_typed_op,
	const uint32_t *, size_t, struct orlix_tcti_typed_provenance,
	uint32_t *, enum orlix_tcti_typed_error *);
void orlix_tcti_typed_destroy(struct orlix_tcti_typed_expression *);
#endif
