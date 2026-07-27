/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_typed_expression.h"
#include <stdlib.h>
#include <string.h>

#define ORLIX_TCTI_TYPED_MAX_NODES 65536U

static int fail(enum orlix_tcti_typed_error *error, enum orlix_tcti_typed_error value)
{
	if (error)
		*error = value;
	return -1;
}

static int grow(void **pointer, size_t *capacity, size_t needed, size_t size)
{
	size_t next = *capacity ? *capacity : 32;
	void *replacement;

	while (next < needed) {
		if (next > SIZE_MAX / 2)
			return -1;
		next *= 2;
	}
	replacement = realloc(*pointer, next * size);
	if (!replacement)
		return -1;
	*pointer = replacement;
	*capacity = next;
	return 0;
}

static char *copy_text(const char *text)
{
	char *copy;
	size_t length;

	if (!text)
		return NULL;
	length = strlen(text) + 1;
	copy = malloc(length);
	if (copy)
		memcpy(copy, text, length);
	return copy;
}

static void destroy_identity(struct orlix_tcti_typed_identity *identity)
{
	free(identity->text);
	free(identity->state);
	free(identity->register_name);
	free(identity->selector);
	memset(identity, 0, sizeof(*identity));
}

static int copy_identity(struct orlix_tcti_typed_identity *identity,
			 const char *text, const char *state,
			 const char *register_name, const char *selector)
{
	identity->text = copy_text(text);
	identity->state = copy_text(state);
	identity->register_name = copy_text(register_name);
	identity->selector = copy_text(selector);
	if ((text && !identity->text) || (state && !identity->state) ||
	    (register_name && !identity->register_name) ||
	    (selector && !identity->selector)) {
		destroy_identity(identity);
		return -1;
	}
	return 0;
}

static int add_node(struct orlix_tcti_typed_expression *expression,
			    struct orlix_tcti_typed_node *node, uint32_t *out,
			    enum orlix_tcti_typed_error *error)
{
	if (!expression || !out ||
	    expression->node_count == ORLIX_TCTI_TYPED_MAX_NODES)
		return fail(error, ORLIX_TCTI_TYPED_ARGUMENT);
	if (grow((void **)&expression->nodes, &expression->node_capacity,
		 expression->node_count + 1, sizeof(*expression->nodes)))
		return fail(error, ORLIX_TCTI_TYPED_MEMORY);
	*out = (uint32_t)expression->node_count;
	expression->nodes[expression->node_count++] = *node;
	if (error)
		*error = ORLIX_TCTI_TYPED_OK;
	return 0;
}

static int scalar_type(enum orlix_tcti_typed_type type)
{
	return type == ORLIX_TCTI_TYPED_UNSIGNED || type == ORLIX_TCTI_TYPED_SIGNED ||
	       type == ORLIX_TCTI_TYPED_IDENTIFIER || type == ORLIX_TCTI_TYPED_FIELD ||
	       type == ORLIX_TCTI_TYPED_VALUE || type == ORLIX_TCTI_TYPED_DOT_ATOM ||
	       type == ORLIX_TCTI_TYPED_OPERAND;
}

static int boolean_type(enum orlix_tcti_typed_type type)
{
	return type == ORLIX_TCTI_TYPED_BOOL || type == ORLIX_TCTI_TYPED_UNRESOLVED_BOOLEAN;
}

static int opaque_type(enum orlix_tcti_typed_type type)
{
	return type == ORLIX_TCTI_TYPED_IDENTIFIER || type == ORLIX_TCTI_TYPED_FIELD ||
	       type == ORLIX_TCTI_TYPED_VALUE || type == ORLIX_TCTI_TYPED_DOT_ATOM ||
	       type == ORLIX_TCTI_TYPED_OPERAND;
}

static int numeric_type(enum orlix_tcti_typed_type type)
{
	return type == ORLIX_TCTI_TYPED_UNSIGNED || type == ORLIX_TCTI_TYPED_SIGNED;
}

static int compatible_equality(const struct orlix_tcti_typed_node *left,
			       const struct orlix_tcti_typed_node *right)
{
	if (left->type == right->type) {
		if ((left->type == ORLIX_TCTI_TYPED_UNSIGNED ||
		     left->type == ORLIX_TCTI_TYPED_SIGNED) && left->width != right->width)
			return 0;
		return 1;
	}
	if (((numeric_type(left->type) && right->type == ORLIX_TCTI_TYPED_OPERAND) ||
	     (numeric_type(right->type) && left->type == ORLIX_TCTI_TYPED_OPERAND)) &&
	    (!left->width || !right->width || left->width == right->width))
		return 1;
	return opaque_type(left->type) && opaque_type(right->type);
}

static int value_bit_is_set(const uint64_t limbs[2], uint8_t bit)
{
	return !!(limbs[bit / 64U] & (UINT64_C(1) << (bit % 64U)));
}

static int valid_value(const struct orlix_tcti_typed_value *value)
{
	uint8_t bit;

	if (!value || value->width > ORLIX_TCTI_TYPED_VALUE_MAX_WIDTH)
		return 0;
	for (bit = 0; bit < ORLIX_TCTI_TYPED_VALUE_MAX_WIDTH; bit++) {
		int known = value_bit_is_set(value->known_mask, bit);

		if (value_bit_is_set(value->value, bit) && !known)
			return 0;
		if (bit >= value->width &&
		    (known || value_bit_is_set(value->value, bit)))
			return 0;
	}
	return 1;
}

int orlix_tcti_typed_value_from_u64(struct orlix_tcti_typed_value *out, uint64_t value,
			      uint8_t width, enum orlix_tcti_typed_error *error)
{
	if (!out || width > 64 || (width < 64 && (value >> width)))
		return fail(error, ORLIX_TCTI_TYPED_VALUE_INVALID);
	memset(out, 0, sizeof(*out));
	out->value[0] = value;
	out->known_mask[0] = width == 64 ? UINT64_MAX :
		(width ? (UINT64_C(1) << width) - 1U : 0U);
	out->width = width;
	if (error)
		*error = ORLIX_TCTI_TYPED_OK;
	return 0;
}

int orlix_tcti_typed_value_parse(struct orlix_tcti_typed_value *out, const char *text,
			   enum orlix_tcti_typed_error *error)
{
	struct orlix_tcti_typed_value parsed;
	size_t length, index;

	if (!out || !text)
		return fail(error, ORLIX_TCTI_TYPED_VALUE_INVALID);
	length = strlen(text);
	if (length < 3 || text[0] != '\'' || text[length - 1] != '\'' ||
	    length - 2 > ORLIX_TCTI_TYPED_VALUE_MAX_WIDTH)
		return fail(error, ORLIX_TCTI_TYPED_VALUE_INVALID);
	memset(&parsed, 0, sizeof(parsed));
	parsed.width = (uint8_t)(length - 2);
	for (index = 0; index < parsed.width; index++) {
		uint8_t bit = (uint8_t)(parsed.width - 1U - index);
		uint64_t mask = UINT64_C(1) << (bit % 64U);
		char digit = text[index + 1U];

		if (digit == '0' || digit == '1') {
			parsed.known_mask[bit / 64U] |= mask;
			if (digit == '1')
				parsed.value[bit / 64U] |= mask;
		} else if (digit != 'x' && digit != 'X') {
			return fail(error, ORLIX_TCTI_TYPED_VALUE_INVALID);
		}
	}
	*out = parsed;
	if (error)
		*error = ORLIX_TCTI_TYPED_OK;
	return 0;
}

int orlix_tcti_typed_literal(struct orlix_tcti_typed_expression *expression,
			       enum orlix_tcti_typed_type type, uint8_t width,
			       const struct orlix_tcti_typed_value *value,
			       enum orlix_tcti_typed_type element_type,
			       uint8_t element_width,
			       struct orlix_tcti_typed_provenance provenance,
			       uint32_t *out, enum orlix_tcti_typed_error *error)
{
	struct orlix_tcti_typed_node node = {
		.type = type, .element_type = element_type,
		.op = ORLIX_TCTI_TYPED_LITERAL, .width = width,
		.element_width = element_width, .first_child = ORLIX_TCTI_TYPED_NONE,
		.parent = ORLIX_TCTI_TYPED_NONE,
		.provenance = provenance,
	};

	if (!valid_value(value))
		return fail(error, ORLIX_TCTI_TYPED_VALUE_INVALID);
	if (type == ORLIX_TCTI_TYPED_BOOL && (width || value->width != 1U ||
	    value->known_mask[0] != 1U || value->known_mask[1] ||
	    value->value[0] > 1U || value->value[1]))
		return fail(error, ORLIX_TCTI_TYPED_ARGUMENT);
	if (type == ORLIX_TCTI_TYPED_SET &&
	    (value->width || element_width == 0 ||
	     element_width > ORLIX_TCTI_TYPED_VALUE_MAX_WIDTH))
		return fail(error, ORLIX_TCTI_TYPED_ARGUMENT);
	if (type != ORLIX_TCTI_TYPED_BOOL && type != ORLIX_TCTI_TYPED_SET &&
	    (width == 0 || width > ORLIX_TCTI_TYPED_VALUE_MAX_WIDTH ||
	     value->width != width))
		return fail(error, ORLIX_TCTI_TYPED_ARGUMENT);
	node.value = *value;
	return add_node(expression, &node, out, error);
}

int orlix_tcti_typed_value_atom(struct orlix_tcti_typed_expression *expression,
			  const char *text,
			  struct orlix_tcti_typed_provenance provenance, uint32_t *out,
			  enum orlix_tcti_typed_error *error)
{
	struct orlix_tcti_typed_value value;
	struct orlix_tcti_typed_node node = {
		.type = ORLIX_TCTI_TYPED_VALUE, .op = ORLIX_TCTI_TYPED_LITERAL,
		.first_child = ORLIX_TCTI_TYPED_NONE, .parent = ORLIX_TCTI_TYPED_NONE,
		.provenance = provenance,
	};

	if (!expression || !out || orlix_tcti_typed_value_parse(&value, text, error))
		return -1;
	node.width = value.width;
	node.value = value;
	if (copy_identity(&node.identity, text, NULL, NULL, NULL))
		return fail(error, ORLIX_TCTI_TYPED_MEMORY);
	if (add_node(expression, &node, out, error)) {
		destroy_identity(&node.identity);
		return -1;
	}
	return 0;
}

int orlix_tcti_typed_atom(struct orlix_tcti_typed_expression *expression,
		    enum orlix_tcti_typed_type type, const char *text,
		    const char *state, const char *register_name, const char *selector,
		    struct orlix_tcti_typed_provenance provenance, uint32_t *out,
		    enum orlix_tcti_typed_error *error)
{
	struct orlix_tcti_typed_node node = {
		.type = type, .op = ORLIX_TCTI_TYPED_LITERAL,
		.first_child = ORLIX_TCTI_TYPED_NONE, .parent = ORLIX_TCTI_TYPED_NONE,
		.provenance = provenance,
	};

	if (!expression || !out ||
	    (type != ORLIX_TCTI_TYPED_BOOL && type != ORLIX_TCTI_TYPED_UNRESOLVED_BOOLEAN &&
	     type != ORLIX_TCTI_TYPED_IDENTIFIER && type != ORLIX_TCTI_TYPED_FIELD &&
	     type != ORLIX_TCTI_TYPED_VALUE && type != ORLIX_TCTI_TYPED_OPERAND) ||
	    (type == ORLIX_TCTI_TYPED_FIELD && (!state || !register_name || !selector)) ||
	    (type != ORLIX_TCTI_TYPED_FIELD && !text))
		return fail(error, ORLIX_TCTI_TYPED_ARGUMENT);
	if (copy_identity(&node.identity, text, state, register_name, selector))
		return fail(error, ORLIX_TCTI_TYPED_MEMORY);
	if (add_node(expression, &node, out, error)) {
		destroy_identity(&node.identity);
		return -1;
	}
	return 0;
}

int orlix_tcti_typed_boolean_atom(struct orlix_tcti_typed_expression *expression,
			     const char *text, int parameter,
			     struct orlix_tcti_typed_provenance provenance, uint32_t *out,
			     enum orlix_tcti_typed_error *error)
{
	if (orlix_tcti_typed_atom(expression, ORLIX_TCTI_TYPED_BOOL, text, NULL, NULL, NULL,
			    provenance, out, error))
		return -1;
	expression->nodes[*out].type = parameter ? ORLIX_TCTI_TYPED_BOOL :
		ORLIX_TCTI_TYPED_UNRESOLVED_BOOLEAN;
	expression->nodes[*out].identity.parameter = !!parameter;
	return 0;
}

int orlix_tcti_typed_compound(struct orlix_tcti_typed_expression *expression,
			enum orlix_tcti_typed_type type, const uint32_t *children,
			size_t child_count, struct orlix_tcti_typed_provenance provenance,
			uint32_t *out, enum orlix_tcti_typed_error *error)
{
	struct orlix_tcti_typed_node node;
	const struct orlix_tcti_typed_node *first;
	size_t index;

	if (!expression || !children || !out || child_count == 0 ||
	    (type != ORLIX_TCTI_TYPED_SET && type != ORLIX_TCTI_TYPED_DOT_ATOM) ||
	    expression->node_count == ORLIX_TCTI_TYPED_MAX_NODES)
		return fail(error, ORLIX_TCTI_TYPED_ARGUMENT);
	for (index = 0; index < child_count; index++) {
		if (children[index] >= expression->node_count ||
		    expression->nodes[children[index]].parent != ORLIX_TCTI_TYPED_NONE)
			return fail(error, ORLIX_TCTI_TYPED_OWNED);
		if (index && children[index] == children[0])
			return fail(error, ORLIX_TCTI_TYPED_OWNED);
	}
	first = &expression->nodes[children[0]];
	memset(&node, 0, sizeof(node));
	node.type = type;
	node.op = type == ORLIX_TCTI_TYPED_SET ? ORLIX_TCTI_TYPED_SET_LITERAL :
			ORLIX_TCTI_TYPED_DOT_ATOM_LITERAL;
	node.first_child = (uint32_t)expression->child_count;
	node.child_count = (uint32_t)child_count;
	node.parent = ORLIX_TCTI_TYPED_NONE;
	node.provenance = provenance;
	if (type == ORLIX_TCTI_TYPED_SET) {
		if (!scalar_type(first->type))
			return fail(error, ORLIX_TCTI_TYPED_TYPE);
		node.element_type = first->type;
		node.element_width = first->width;
		for (index = 1; index < child_count; index++) {
			const struct orlix_tcti_typed_node *child =
				&expression->nodes[children[index]];
			if (child->type != node.element_type ||
			    child->width != node.element_width)
				return fail(error, ORLIX_TCTI_TYPED_TYPE);
		}
	} else {
		for (index = 0; index < child_count; index++)
			if (expression->nodes[children[index]].type != ORLIX_TCTI_TYPED_IDENTIFIER)
				return fail(error, ORLIX_TCTI_TYPED_TYPE);
	}
	if (grow((void **)&expression->children, &expression->child_capacity,
		 expression->child_count + child_count, sizeof(*expression->children)) ||
	    grow((void **)&expression->nodes, &expression->node_capacity,
		 expression->node_count + 1, sizeof(*expression->nodes)))
		return fail(error, ORLIX_TCTI_TYPED_MEMORY);
	memcpy(expression->children + expression->child_count, children,
	       child_count * sizeof(*children));
	for (index = 0; index < child_count; index++)
		expression->nodes[children[index]].parent =
			(uint32_t)expression->node_count;
	expression->child_count += child_count;
	return add_node(expression, &node, out, error);
}

int orlix_tcti_typed_apply(struct orlix_tcti_typed_expression *expression,
		     enum orlix_tcti_typed_op op, const uint32_t *children,
		     size_t child_count, struct orlix_tcti_typed_provenance provenance,
		     uint32_t *out, enum orlix_tcti_typed_error *error)
{
	struct orlix_tcti_typed_node node;
	struct orlix_tcti_typed_node *left, *right;
	size_t index;

	if (!expression || !children || !out || child_count == 0 ||
	    child_count > 2 || expression->node_count == ORLIX_TCTI_TYPED_MAX_NODES)
		return fail(error, ORLIX_TCTI_TYPED_ARGUMENT);
	for (index = 0; index < child_count; index++) {
		if (children[index] >= expression->node_count ||
		    expression->nodes[children[index]].parent != ORLIX_TCTI_TYPED_NONE)
			return fail(error, ORLIX_TCTI_TYPED_OWNED);
		if (index && children[index] == children[0])
			return fail(error, ORLIX_TCTI_TYPED_OWNED);
	}
	left = &expression->nodes[children[0]];
	right = child_count == 2 ? &expression->nodes[children[1]] : NULL;
	if (left->depth >= ORLIX_TCTI_TYPED_MAX_DEPTH)
		return fail(error, ORLIX_TCTI_TYPED_LIMIT);
	memset(&node, 0, sizeof(node));
	node.op = op;
	node.first_child = (uint32_t)expression->child_count;
	node.child_count = (uint32_t)child_count;
	node.parent = ORLIX_TCTI_TYPED_NONE;
	node.provenance = provenance;
	node.depth = left->depth + 1;
	if (op == ORLIX_TCTI_TYPED_NOT || op == ORLIX_TCTI_TYPED_UINT || op == ORLIX_TCTI_TYPED_SINT) {
		if (child_count != 1)
			return fail(error, ORLIX_TCTI_TYPED_ARITY);
		if (op == ORLIX_TCTI_TYPED_NOT) {
			if (!boolean_type(left->type))
				return fail(error, ORLIX_TCTI_TYPED_TYPE);
			node.type = ORLIX_TCTI_TYPED_BOOL;
		} else {
			if (!scalar_type(left->type))
				return fail(error, ORLIX_TCTI_TYPED_TYPE);
			node.type = op == ORLIX_TCTI_TYPED_UINT ? ORLIX_TCTI_TYPED_UNSIGNED :
				ORLIX_TCTI_TYPED_SIGNED;
			node.width = left->width;
			node.value = left->value;
		}
	} else {
		if (child_count != 2)
			return fail(error, ORLIX_TCTI_TYPED_ARITY);
		if (right->depth >= ORLIX_TCTI_TYPED_MAX_DEPTH)
			return fail(error, ORLIX_TCTI_TYPED_LIMIT);
		if (right->depth + 1 > node.depth)
			node.depth = right->depth + 1;
		if (op == ORLIX_TCTI_TYPED_IN) {
			struct orlix_tcti_typed_node element = {
				.type = right->element_type,
				.width = right->element_width,
			};

			if (right->type != ORLIX_TCTI_TYPED_SET ||
			    !compatible_equality(left, &element))
				return fail(error, ORLIX_TCTI_TYPED_TYPE);
		} else if (op == ORLIX_TCTI_TYPED_AND || op == ORLIX_TCTI_TYPED_OR ||
			   op == ORLIX_TCTI_TYPED_IMPLIES || op == ORLIX_TCTI_TYPED_IFF) {
			if (!boolean_type(left->type) || !boolean_type(right->type))
				return fail(error, ORLIX_TCTI_TYPED_TYPE);
		} else if (op == ORLIX_TCTI_TYPED_EQ || op == ORLIX_TCTI_TYPED_NE) {
			if (!compatible_equality(left, right))
				return fail(error, ORLIX_TCTI_TYPED_TYPE);
		} else if ((op == ORLIX_TCTI_TYPED_LT || op == ORLIX_TCTI_TYPED_GT ||
			    op == ORLIX_TCTI_TYPED_GE) &&
			   (!((numeric_type(left->type) && numeric_type(right->type) &&
			       left->type == right->type && left->width == right->width) ||
			      (((numeric_type(left->type) && right->type == ORLIX_TCTI_TYPED_OPERAND) ||
			        (numeric_type(right->type) && left->type == ORLIX_TCTI_TYPED_OPERAND)) &&
			       (!left->width || !right->width ||
				left->width == right->width))))) {
			return fail(error, ORLIX_TCTI_TYPED_TYPE);
		} else if (op != ORLIX_TCTI_TYPED_LT && op != ORLIX_TCTI_TYPED_GT &&
			   op != ORLIX_TCTI_TYPED_GE) {
			return fail(error, ORLIX_TCTI_TYPED_ARGUMENT);
		}
		node.type = ORLIX_TCTI_TYPED_BOOL;
	}
	if (grow((void **)&expression->children, &expression->child_capacity,
		 expression->child_count + child_count, sizeof(*expression->children)) ||
	    grow((void **)&expression->nodes, &expression->node_capacity,
		 expression->node_count + 1, sizeof(*expression->nodes)))
		return fail(error, ORLIX_TCTI_TYPED_MEMORY);
	memcpy(expression->children + expression->child_count, children,
	       child_count * sizeof(*children));
	for (index = 0; index < child_count; index++)
		expression->nodes[children[index]].parent =
			(uint32_t)expression->node_count;
	expression->child_count += child_count;
	return add_node(expression, &node, out, error);
}

void orlix_tcti_typed_destroy(struct orlix_tcti_typed_expression *expression)
{
	size_t index;

	if (!expression)
		return;
	for (index = 0; index < expression->node_count; index++)
		destroy_identity(&expression->nodes[index].identity);
	free(expression->nodes);
	free(expression->children);
	memset(expression, 0, sizeof(*expression));
}
