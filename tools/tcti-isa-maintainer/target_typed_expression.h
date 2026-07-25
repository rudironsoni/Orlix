/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TYPED_EXPRESSION_H
#define ORLIX_TCTI_TYPED_EXPRESSION_H
#include <stddef.h>
#include <stdint.h>
#define TCTI_TYPED_NONE UINT32_MAX
#define TCTI_TYPED_MAX_DEPTH 128U
#define TCTI_TYPED_VALUE_MAX_WIDTH 128U
/* The types retain the source-language distinctions until the typed solver
 * deliberately assigns a concrete feature-domain interpretation. */
enum tcti_typed_type {
	TCTI_TYPED_BOOL, TCTI_TYPED_UNSIGNED, TCTI_TYPED_SIGNED,
	TCTI_TYPED_SET, TCTI_TYPED_IDENTIFIER, TCTI_TYPED_FIELD,
	TCTI_TYPED_VALUE, TCTI_TYPED_DOT_ATOM, TCTI_TYPED_OPERAND,
	TCTI_TYPED_UNRESOLVED_BOOLEAN,
};
enum tcti_typed_op {
	TCTI_TYPED_LITERAL, TCTI_TYPED_EQ, TCTI_TYPED_NE, TCTI_TYPED_LT,
	TCTI_TYPED_GT, TCTI_TYPED_GE, TCTI_TYPED_IN, TCTI_TYPED_NOT,
	TCTI_TYPED_AND, TCTI_TYPED_OR, TCTI_TYPED_IMPLIES, TCTI_TYPED_IFF,
	TCTI_TYPED_UINT, TCTI_TYPED_SINT, TCTI_TYPED_SET_LITERAL,
	TCTI_TYPED_DOT_ATOM_LITERAL,
};
struct tcti_typed_provenance { size_t offset, length; };
/* AARCHMRS Values.Value bit strings are MSB-first and may contain wildcard
 * bits. Keep both 64-bit limbs so the maintainer tool never depends on a host
 * integer wider than the C11 types it explicitly names. */
struct tcti_typed_value {
	uint64_t value[2];
	uint64_t known_mask[2];
	uint8_t width;
};
struct tcti_typed_identity {
	char *text;
	char *state;
	char *register_name;
	char *selector;
	uint8_t parameter;
};
struct tcti_typed_node {
	enum tcti_typed_type type, element_type;
	enum tcti_typed_op op;
	uint8_t width, element_width, depth;
	uint32_t first_child, child_count, parent;
	struct tcti_typed_value value;
	struct tcti_typed_identity identity;
	struct tcti_typed_provenance provenance;
};
struct tcti_typed_expression {
	struct tcti_typed_node *nodes;
	size_t node_count, node_capacity;
	uint32_t *children;
	size_t child_count, child_capacity;
};
enum tcti_typed_error {
	TCTI_TYPED_OK, TCTI_TYPED_ARGUMENT, TCTI_TYPED_TYPE, TCTI_TYPED_ARITY,
	TCTI_TYPED_LIMIT, TCTI_TYPED_MEMORY, TCTI_TYPED_OWNED,
	TCTI_TYPED_VALUE_INVALID,
};
int tcti_typed_value_from_u64(struct tcti_typed_value *, uint64_t, uint8_t,
	enum tcti_typed_error *);
int tcti_typed_value_parse(struct tcti_typed_value *, const char *,
	enum tcti_typed_error *);
int tcti_typed_literal(struct tcti_typed_expression *, enum tcti_typed_type,
	uint8_t, const struct tcti_typed_value *, enum tcti_typed_type, uint8_t,
	struct tcti_typed_provenance, uint32_t *, enum tcti_typed_error *);
int tcti_typed_value_atom(struct tcti_typed_expression *, const char *,
	struct tcti_typed_provenance, uint32_t *, enum tcti_typed_error *);
int tcti_typed_atom(struct tcti_typed_expression *, enum tcti_typed_type,
	const char *, const char *, const char *, const char *,
	struct tcti_typed_provenance, uint32_t *, enum tcti_typed_error *);
int tcti_typed_boolean_atom(struct tcti_typed_expression *, const char *,
	int, struct tcti_typed_provenance, uint32_t *,
	enum tcti_typed_error *);
int tcti_typed_compound(struct tcti_typed_expression *,
	enum tcti_typed_type, const uint32_t *, size_t,
	struct tcti_typed_provenance, uint32_t *, enum tcti_typed_error *);
int tcti_typed_apply(struct tcti_typed_expression *, enum tcti_typed_op,
	const uint32_t *, size_t, struct tcti_typed_provenance,
	uint32_t *, enum tcti_typed_error *);
void tcti_typed_destroy(struct tcti_typed_expression *);
#endif
