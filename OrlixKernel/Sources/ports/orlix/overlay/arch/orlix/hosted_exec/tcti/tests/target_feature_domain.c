/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_feature_domain.h"
#include "target_condition_format.h"

#ifdef __KERNEL__
#include <linux/limits.h>
#include <linux/string.h>
#else
#include <limits.h>
#include <string.h>
#endif

#define TCTI_FEATURE_DOMAIN_MAX_ATOM_DEPTH 256U

static int fail(struct tcti_feature_domain_diagnostic *diagnostic,
			enum tcti_feature_domain_error error,
			tcti_feature_artifact_u32 node_index)
{
	if (diagnostic) {
		diagnostic->error = error;
		diagnostic->node_index = node_index;
	}
	return -1;
}

static int present(const char *text)
{
	return text && text[0];
}

static int reference_valid(const struct tcti_feature_artifact *artifact,
			   tcti_feature_artifact_u32 reference)
{
	return reference != TCTI_FEATURE_ARTIFACT_NODE_NONE &&
		reference < artifact->counts.node_count;
}

static int child_valid(const struct tcti_feature_artifact *artifact,
		       const struct tcti_feature_artifact_node *node,
		       tcti_feature_artifact_u32 child)
{
	return child < node->child_count &&
		node->first_child != TCTI_FEATURE_ARTIFACT_NODE_NONE &&
		node->first_child <= artifact->counts.child_count &&
		node->child_count <= artifact->counts.child_count - node->first_child &&
		artifact->children[node->first_child + child] <
		artifact->counts.node_count;
}

static int scalar(const struct tcti_feature_domain_value *value)
{
	return value->kind == TCTI_FEATURE_DOMAIN_VALUE_BOOL ||
		value->kind == TCTI_FEATURE_DOMAIN_VALUE_SIGNED ||
		value->kind == TCTI_FEATURE_DOMAIN_VALUE_UNSIGNED ||
		value->kind == TCTI_FEATURE_DOMAIN_VALUE_ATOM ||
		value->kind == TCTI_FEATURE_DOMAIN_VALUE_DOT_ATOM;
}

static int integer_valid(const struct tcti_feature_domain_integer *integer);
static int integer_negative(const struct tcti_feature_domain_integer *integer);

static int callback_value_valid(const struct tcti_feature_domain_value *value)
{
	return value && ((value->kind == TCTI_FEATURE_DOMAIN_VALUE_BOOL &&
		 value->boolean <= 1U) ||
		((value->kind == TCTI_FEATURE_DOMAIN_VALUE_SIGNED ||
		  value->kind == TCTI_FEATURE_DOMAIN_VALUE_UNSIGNED) &&
		 integer_valid(&value->integer)) ||
		(value->kind == TCTI_FEATURE_DOMAIN_VALUE_ATOM &&
		 present(value->text)));
}

static tcti_feature_artifact_u64 width_mask(tcti_feature_artifact_u8 width)
{
	if (width >= 64U)
		return ~(tcti_feature_artifact_u64)0;
	return ((tcti_feature_artifact_u64)1U << width) - 1U;
}

static int integer_valid(const struct tcti_feature_domain_integer *integer)
{
	if (!integer || !integer->width || integer->width > 128U)
		return 0;
	if (integer->width <= 64U)
		return !integer->high && !(integer->low & ~width_mask(integer->width));
	return !(integer->high & ~width_mask(integer->width - 64U));
}

static int integer_unsigned_order(const struct tcti_feature_domain_integer *left,
	const struct tcti_feature_domain_integer *right)
{
	if (left->high != right->high)
		return left->high < right->high ? -1 : 1;
	if (left->low != right->low)
		return left->low < right->low ? -1 : 1;
	return 0;
}

static void integer_extend(const struct tcti_feature_domain_integer *source,
	tcti_feature_artifact_u8 width, int sign_extend,
	struct tcti_feature_domain_integer *result)
{
	*result = *source;
	result->width = width;
	if (sign_extend && integer_negative(source) && width != source->width &&
	    source->width <= 64U) {
		result->low |= ~width_mask(source->width);
		if (width > 64U)
			result->high = ~(tcti_feature_artifact_u64)0;
	} else if (sign_extend && integer_negative(source) &&
		   width != source->width) {
		result->high |= ~width_mask(source->width - 64U);
	}
	if (width <= 64U) {
		result->low &= width_mask(width);
		result->high = 0;
	} else {
		result->high &= width_mask(width - 64U);
	}
}

static int integer_negative(const struct tcti_feature_domain_integer *integer)
{
	tcti_feature_artifact_u8 bit = integer->width - 1U;

	if (bit >= 64U)
		return !!(integer->high & ((tcti_feature_artifact_u64)1U <<
			(bit - 64U)));
	return !!(integer->low & ((tcti_feature_artifact_u64)1U << bit));
}

static int parse_integer_literal(const char *text,
	enum tcti_feature_domain_value_kind kind,
	struct tcti_feature_domain_value *value)
{
	struct tcti_feature_domain_integer integer = { 0 };
	size_t index;

	if (!text || !value || text[0] != '\'' || text[1] == '\0')
		return -1;
	for (index = 1; text[index] && text[index] != '\''; index++) {
		if (index > 128U || (text[index] != '0' && text[index] != '1'))
			return -1;
		integer.high = (integer.high << 1) | (integer.low >> 63);
		integer.low = (integer.low << 1) | (text[index] - '0');
	}
	if (index == 1U || index - 1U > 128U || text[index] != '\'' ||
	    text[index + 1U])
		return -1;
	integer.width = (tcti_feature_artifact_u8)(index - 1U);
	if (!integer_valid(&integer))
		return -1;
	*value = (struct tcti_feature_domain_value) {
		.kind = kind, .integer = integer,
	};
	return 0;
}

int tcti_feature_domain_parse_uint_literal(const char *text,
	struct tcti_feature_domain_value *value)
{
	return parse_integer_literal(text, TCTI_FEATURE_DOMAIN_VALUE_UNSIGNED,
		value);
}

int tcti_feature_domain_parse_sint_literal(const char *text,
	struct tcti_feature_domain_value *value)
{
	return parse_integer_literal(text, TCTI_FEATURE_DOMAIN_VALUE_SIGNED,
		value);
}

static int atom_node_length(const struct tcti_feature_artifact *artifact,
			    tcti_feature_artifact_u32 index,
			    tcti_feature_artifact_u32 depth,
			    tcti_feature_artifact_u64 *length)
{
	const struct tcti_feature_artifact_node *node;
	tcti_feature_artifact_u32 child;

	if (depth >= TCTI_FEATURE_DOMAIN_MAX_ATOM_DEPTH ||
	    index >= artifact->counts.node_count)
		return -1;
	node = &artifact->nodes[index];
	if (node->kind == TCTI_FEATURE_ARTIFACT_VALUE) {
		if (!present(node->text))
			return -1;
		*length = strlen(node->text);
		return 0;
	}
	if (node->kind != TCTI_FEATURE_ARTIFACT_DOT_ATOM || !node->child_count)
		return -1;
	*length = 0;
	for (child = 0; child < node->child_count; child++) {
		tcti_feature_artifact_u64 part;
		tcti_feature_artifact_u32 target;

		if (!child_valid(artifact, node, child))
			return -1;
		target = artifact->children[node->first_child + child];
		if (atom_node_length(artifact, target, depth + 1U, &part) ||
		    (child && *length == ~(tcti_feature_artifact_u64)0) ||
		    *length > ~(tcti_feature_artifact_u64)0 - part - (child ? 1U : 0U))
			return -1;
		*length += part + (child ? 1U : 0U);
	}
	return 0;
}

static int atom_node_byte(const struct tcti_feature_artifact *artifact,
			  tcti_feature_artifact_u32 index,
			  tcti_feature_artifact_u64 offset,
			  tcti_feature_artifact_u32 depth, unsigned char *byte)
{
	const struct tcti_feature_artifact_node *node;
	tcti_feature_artifact_u32 child;

	if (depth >= TCTI_FEATURE_DOMAIN_MAX_ATOM_DEPTH ||
	    index >= artifact->counts.node_count || !byte)
		return -1;
	node = &artifact->nodes[index];
	if (node->kind == TCTI_FEATURE_ARTIFACT_VALUE) {
		size_t length;

		if (!present(node->text))
			return -1;
		length = strlen(node->text);
		if (offset >= length)
			return -1;
		*byte = (unsigned char)node->text[offset];
		return 0;
	}
	if (node->kind != TCTI_FEATURE_ARTIFACT_DOT_ATOM || !node->child_count)
		return -1;
	for (child = 0; child < node->child_count; child++) {
		tcti_feature_artifact_u64 length;
		tcti_feature_artifact_u32 target;

		if (!child_valid(artifact, node, child))
			return -1;
		if (child) {
			if (!offset) {
				*byte = '.';
				return 0;
			}
			offset--;
		}
		target = artifact->children[node->first_child + child];
		if (atom_node_length(artifact, target, depth + 1U, &length))
			return -1;
		if (offset < length)
			return atom_node_byte(artifact, target, offset, depth + 1U,
					      byte);
		offset -= length;
	}
	return -1;
}

static int atom_length(const struct tcti_feature_artifact *artifact,
		       const struct tcti_feature_domain_value *value,
		       tcti_feature_artifact_u64 *length)
{
	if (value->kind == TCTI_FEATURE_DOMAIN_VALUE_ATOM) {
		if (!present(value->text))
			return -1;
		*length = strlen(value->text);
		return 0;
	}
	if (value->kind != TCTI_FEATURE_DOMAIN_VALUE_DOT_ATOM)
		return -1;
	return atom_node_length(artifact, value->node_index, 0, length);
}

static int atom_byte(const struct tcti_feature_artifact *artifact,
		     const struct tcti_feature_domain_value *value,
		     tcti_feature_artifact_u64 offset, unsigned char *byte)
{
	if (value->kind == TCTI_FEATURE_DOMAIN_VALUE_ATOM) {
		size_t length;

		if (!present(value->text))
			return -1;
		length = strlen(value->text);
		if (offset >= length)
			return -1;
		*byte = (unsigned char)value->text[offset];
		return 0;
	}
	if (value->kind != TCTI_FEATURE_DOMAIN_VALUE_DOT_ATOM)
		return -1;
	return atom_node_byte(artifact, value->node_index, offset, 0, byte);
}

static int atom_equal(const struct tcti_feature_artifact *artifact,
		      const struct tcti_feature_domain_value *left,
		      const struct tcti_feature_domain_value *right, int *equal)
{
	tcti_feature_artifact_u64 left_length;
	tcti_feature_artifact_u64 right_length;
	tcti_feature_artifact_u64 index;

	if (atom_length(artifact, left, &left_length) ||
	    atom_length(artifact, right, &right_length))
		return -1;
	if (left_length != right_length) {
		*equal = 0;
		return 0;
	}
	for (index = 0; index < left_length; index++) {
		unsigned char left_byte;
		unsigned char right_byte;

		if (atom_byte(artifact, left, index, &left_byte) ||
		    atom_byte(artifact, right, index, &right_byte))
			return -1;
		if (left_byte != right_byte) {
			*equal = 0;
			return 0;
		}
	}
	*equal = 1;
	return 0;
}

static int values_equal(const struct tcti_feature_artifact *artifact,
			const struct tcti_feature_domain_value *left,
			const struct tcti_feature_domain_value *right, int *equal)
{
	if (!scalar(left) || !scalar(right))
		return -1;
	if ((left->kind == TCTI_FEATURE_DOMAIN_VALUE_ATOM ||
	     left->kind == TCTI_FEATURE_DOMAIN_VALUE_DOT_ATOM) &&
	    (right->kind == TCTI_FEATURE_DOMAIN_VALUE_ATOM ||
	     right->kind == TCTI_FEATURE_DOMAIN_VALUE_DOT_ATOM))
		return atom_equal(artifact, left, right, equal);
	if (left->kind != right->kind)
		return -1;
	switch (left->kind) {
	case TCTI_FEATURE_DOMAIN_VALUE_BOOL:
		*equal = left->boolean == right->boolean;
		return 0;
	case TCTI_FEATURE_DOMAIN_VALUE_SIGNED:
	case TCTI_FEATURE_DOMAIN_VALUE_UNSIGNED:
		if (!integer_valid(&left->integer) || !integer_valid(&right->integer) ||
		    left->kind != right->kind)
			return -1;
		if (tcti_feature_domain_compare_numeric(left, right, equal))
			return -1;
		*equal = !*equal;
		return 0;
	default:
		return -1;
	}
}

int tcti_feature_domain_compare_numeric(
	const struct tcti_feature_domain_value *left,
	const struct tcti_feature_domain_value *right, int *order)
{
	int left_negative;
	int right_negative;
	tcti_feature_artifact_u8 width;
	struct tcti_feature_domain_integer normalized_left;
	struct tcti_feature_domain_integer normalized_right;

	if (!left || !right || !order || left->kind != right->kind ||
	    (left->kind != TCTI_FEATURE_DOMAIN_VALUE_SIGNED &&
	     left->kind != TCTI_FEATURE_DOMAIN_VALUE_UNSIGNED) ||
	    !integer_valid(&left->integer) || !integer_valid(&right->integer))
		return -1;
	width = left->integer.width > right->integer.width ?
		left->integer.width : right->integer.width;
	if (left->kind == TCTI_FEATURE_DOMAIN_VALUE_UNSIGNED) {
		integer_extend(&left->integer, width, 0, &normalized_left);
		integer_extend(&right->integer, width, 0, &normalized_right);
		*order = integer_unsigned_order(&normalized_left, &normalized_right);
		return 0;
	}
	left_negative = integer_negative(&left->integer);
	right_negative = integer_negative(&right->integer);
	if (left_negative != right_negative)
		*order = left_negative ? -1 : 1;
	else {
		integer_extend(&left->integer, width, 1, &normalized_left);
		integer_extend(&right->integer, width, 1, &normalized_right);
		*order = integer_unsigned_order(&normalized_left, &normalized_right);
	}
	return 0;
}

static int evaluate_node(const struct tcti_feature_artifact *artifact,
			 tcti_feature_artifact_u32 index,
			 const struct tcti_feature_domain_environment *environment,
			 struct tcti_feature_domain_scratch *scratch,
			 tcti_feature_artifact_u32 depth,
			 struct tcti_feature_domain_value *value,
			 struct tcti_feature_domain_diagnostic *diagnostic)
{
	const struct tcti_feature_artifact_node *node;
	struct tcti_feature_domain_value left;
	struct tcti_feature_domain_value right;
	int relation;

	if (index >= artifact->counts.node_count)
		return fail(diagnostic, TCTI_FEATURE_DOMAIN_REFERENCE, index);
	if (depth >= scratch->max_depth)
		return fail(diagnostic, TCTI_FEATURE_DOMAIN_DEPTH, index);
	if (scratch->active[index])
		return fail(diagnostic, TCTI_FEATURE_DOMAIN_CYCLE, index);
	scratch->active[index] = 1;
	node = &artifact->nodes[index];
	*value = (struct tcti_feature_domain_value) { 0 };
	switch (node->kind) {
	case TCTI_FEATURE_ARTIFACT_BOOL:
		if (node->integer != 0 && node->integer != 1)
			goto node_error;
		value->kind = TCTI_FEATURE_DOMAIN_VALUE_BOOL;
		value->boolean = (tcti_feature_artifact_u8)node->integer;
		break;
	case TCTI_FEATURE_ARTIFACT_IDENTIFIER:
		if (!present(node->text) || !environment->feature ||
		    environment->feature(environment->context, node->text, value))
			goto missing_feature;
		if (!callback_value_valid(value) ||
		    value->kind != TCTI_FEATURE_DOMAIN_VALUE_BOOL)
			goto callback_value;
		break;
	case TCTI_FEATURE_ARTIFACT_INTEGER:
		value->kind = TCTI_FEATURE_DOMAIN_VALUE_SIGNED;
		value->integer = (struct tcti_feature_domain_integer) {
			.low = (tcti_feature_artifact_u64)node->integer,
			.width = 64U,
		};
		break;
	case TCTI_FEATURE_ARTIFACT_VALUE:
		if (!present(node->text))
			goto node_error;
		value->kind = TCTI_FEATURE_DOMAIN_VALUE_ATOM;
		value->text = node->text;
		break;
	case TCTI_FEATURE_ARTIFACT_DOT_ATOM:
		if (!node->child_count)
			goto node_error;
		value->kind = TCTI_FEATURE_DOMAIN_VALUE_DOT_ATOM;
		value->node_index = index;
		if (atom_length(artifact, value, &value->integer.low))
			goto node_error;
		break;
	case TCTI_FEATURE_ARTIFACT_SET:
		if (!node->child_count)
			goto node_error;
		value->kind = TCTI_FEATURE_DOMAIN_VALUE_SET;
		value->node_index = index;
		break;
	case TCTI_FEATURE_ARTIFACT_FIELD:
		if (!present(node->field_state) || !present(node->field_register_name) ||
		    !present(node->field_selector) || !environment->field ||
		    environment->field(environment->context, node->field_state,
				       node->field_register_name, node->field_selector,
				       value))
			goto missing_field;
		if (!callback_value_valid(value))
			goto callback_value;
		break;
	case TCTI_FEATURE_ARTIFACT_NOT:
		if (!reference_valid(artifact, node->left) || node->right !=
		    TCTI_FEATURE_ARTIFACT_NODE_NONE || node->child_count ||
		    evaluate_node(artifact, node->left, environment, scratch,
				  depth + 1U, &left, diagnostic))
			goto reference_or_child_error;
		if (left.kind != TCTI_FEATURE_DOMAIN_VALUE_BOOL)
			goto type_error;
		value->kind = TCTI_FEATURE_DOMAIN_VALUE_BOOL;
		value->boolean = !left.boolean;
		break;
	case TCTI_FEATURE_ARTIFACT_UINT:
	case TCTI_FEATURE_ARTIFACT_SINT:
		if (node->child_count != 1 || !child_valid(artifact, node, 0) ||
		    evaluate_node(artifact, artifact->children[node->first_child],
			  environment, scratch, depth + 1U, &left, diagnostic))
			goto reference_or_child_error;
		if ((left.kind == TCTI_FEATURE_DOMAIN_VALUE_SIGNED ||
		     left.kind == TCTI_FEATURE_DOMAIN_VALUE_UNSIGNED) &&
		    !integer_valid(&left.integer))
			goto type_error;
		if (node->kind == TCTI_FEATURE_ARTIFACT_UINT) {
			if (left.kind == TCTI_FEATURE_DOMAIN_VALUE_UNSIGNED)
				*value = left;
			else if (left.kind == TCTI_FEATURE_DOMAIN_VALUE_SIGNED &&
				 !integer_negative(&left.integer)) {
				value->kind = TCTI_FEATURE_DOMAIN_VALUE_UNSIGNED;
				value->integer = left.integer;
			} else if (left.kind == TCTI_FEATURE_DOMAIN_VALUE_ATOM &&
				   !tcti_feature_domain_parse_uint_literal(left.text, value)) {
				break;
			} else if (left.kind == TCTI_FEATURE_DOMAIN_VALUE_SIGNED)
				goto overflow;
			else
				goto type_error;
		} else if (left.kind == TCTI_FEATURE_DOMAIN_VALUE_SIGNED)
			*value = left;
		else if (left.kind == TCTI_FEATURE_DOMAIN_VALUE_UNSIGNED) {
			value->kind = TCTI_FEATURE_DOMAIN_VALUE_SIGNED;
			value->integer = left.integer;
		} else if (left.kind == TCTI_FEATURE_DOMAIN_VALUE_ATOM &&
			   !tcti_feature_domain_parse_sint_literal(left.text, value)) {
			break;
		} else
			goto type_error;
		break;
	case TCTI_FEATURE_ARTIFACT_AND:
	case TCTI_FEATURE_ARTIFACT_OR:
	case TCTI_FEATURE_ARTIFACT_IMPLIES:
	case TCTI_FEATURE_ARTIFACT_IFF:
		if (!reference_valid(artifact, node->left) ||
		    !reference_valid(artifact, node->right) || node->child_count ||
		    evaluate_node(artifact, node->left, environment, scratch,
				  depth + 1U, &left, diagnostic) ||
		    evaluate_node(artifact, node->right, environment, scratch,
				  depth + 1U, &right, diagnostic))
			goto reference_or_child_error;
		if (left.kind != TCTI_FEATURE_DOMAIN_VALUE_BOOL ||
		    right.kind != TCTI_FEATURE_DOMAIN_VALUE_BOOL)
			goto type_error;
		value->kind = TCTI_FEATURE_DOMAIN_VALUE_BOOL;
		if (node->kind == TCTI_FEATURE_ARTIFACT_AND)
			value->boolean = left.boolean && right.boolean;
		else if (node->kind == TCTI_FEATURE_ARTIFACT_OR)
			value->boolean = left.boolean || right.boolean;
		else if (node->kind == TCTI_FEATURE_ARTIFACT_IMPLIES)
			value->boolean = !left.boolean || right.boolean;
		else
			value->boolean = left.boolean == right.boolean;
		break;
	case TCTI_FEATURE_ARTIFACT_EQ:
	case TCTI_FEATURE_ARTIFACT_NE:
	case TCTI_FEATURE_ARTIFACT_LT:
	case TCTI_FEATURE_ARTIFACT_GT:
	case TCTI_FEATURE_ARTIFACT_GE:
		if (!reference_valid(artifact, node->left) ||
		    !reference_valid(artifact, node->right) || node->child_count ||
		    evaluate_node(artifact, node->left, environment, scratch,
				  depth + 1U, &left, diagnostic) ||
		    evaluate_node(artifact, node->right, environment, scratch,
				  depth + 1U, &right, diagnostic))
			goto reference_or_child_error;
		if (node->kind == TCTI_FEATURE_ARTIFACT_EQ ||
		    node->kind == TCTI_FEATURE_ARTIFACT_NE) {
			if (values_equal(artifact, &left, &right, &relation))
				goto type_error;
			if (node->kind == TCTI_FEATURE_ARTIFACT_NE)
				relation = !relation;
		} else {
			if (tcti_feature_domain_compare_numeric(&left, &right, &relation))
				goto type_error;
			if (node->kind == TCTI_FEATURE_ARTIFACT_LT)
				relation = relation < 0;
			else if (node->kind == TCTI_FEATURE_ARTIFACT_GT)
				relation = relation > 0;
			else
				relation = relation >= 0;
		}
		value->kind = TCTI_FEATURE_DOMAIN_VALUE_BOOL;
		value->boolean = (tcti_feature_artifact_u8)relation;
		break;
	case TCTI_FEATURE_ARTIFACT_IN: {
		tcti_feature_artifact_u32 child;
		int matched = 0;

		if (!reference_valid(artifact, node->left) ||
		    !reference_valid(artifact, node->right) || node->child_count ||
		    evaluate_node(artifact, node->left, environment, scratch,
				  depth + 1U, &left, diagnostic) ||
		    evaluate_node(artifact, node->right, environment, scratch,
				  depth + 1U, &right, diagnostic))
			goto reference_or_child_error;
		if (!scalar(&left) || right.kind != TCTI_FEATURE_DOMAIN_VALUE_SET ||
		    right.node_index >= artifact->counts.node_count)
			goto type_error;
		node = &artifact->nodes[right.node_index];
		for (child = 0; child < node->child_count; child++) {
			if (!child_valid(artifact, node, child) ||
			    evaluate_node(artifact,
				 artifact->children[node->first_child + child], environment,
				 scratch, depth + 1U, &right, diagnostic))
				goto reference_or_child_error;
			if (values_equal(artifact, &left, &right, &relation))
				goto type_error;
			matched |= relation;
		}
		value->kind = TCTI_FEATURE_DOMAIN_VALUE_BOOL;
		value->boolean = (tcti_feature_artifact_u8)matched;
		break;
	}
	default:
		goto node_error;
	}
	scratch->active[index] = 0;
	return 0;

missing_feature:
	scratch->active[index] = 0;
	return fail(diagnostic, TCTI_FEATURE_DOMAIN_MISSING_FEATURE, index);
missing_field:
	scratch->active[index] = 0;
	return fail(diagnostic, TCTI_FEATURE_DOMAIN_MISSING_FIELD, index);
callback_value:
	scratch->active[index] = 0;
	return fail(diagnostic, TCTI_FEATURE_DOMAIN_CALLBACK_VALUE, index);
overflow:
	scratch->active[index] = 0;
	return fail(diagnostic, TCTI_FEATURE_DOMAIN_OVERFLOW, index);
type_error:
	scratch->active[index] = 0;
	return fail(diagnostic, TCTI_FEATURE_DOMAIN_TYPE, index);
reference_or_child_error:
	if (diagnostic && diagnostic->error != TCTI_FEATURE_DOMAIN_OK) {
		scratch->active[index] = 0;
		return -1;
	}
	scratch->active[index] = 0;
	return fail(diagnostic, TCTI_FEATURE_DOMAIN_REFERENCE, index);
node_error:
	scratch->active[index] = 0;
	return fail(diagnostic, TCTI_FEATURE_DOMAIN_NODE, index);
}

int tcti_feature_domain_evaluate(
	const struct tcti_feature_artifact *artifact,
	tcti_feature_artifact_u32 root,
	const struct tcti_feature_domain_environment *environment,
	struct tcti_feature_domain_scratch *scratch,
	struct tcti_feature_domain_value *value,
	struct tcti_feature_domain_diagnostic *diagnostic)
{
	if (diagnostic)
		*diagnostic = (struct tcti_feature_domain_diagnostic) {
			.error = TCTI_FEATURE_DOMAIN_OK,
			.node_index = TCTI_FEATURE_ARTIFACT_NODE_NONE,
		};
	if (!artifact || !environment || !scratch || !value || !scratch->active ||
	    scratch->active_count < artifact->counts.node_count ||
	    !scratch->max_depth)
		return fail(diagnostic, TCTI_FEATURE_DOMAIN_INVALID_ARGUMENT,
			    TCTI_FEATURE_ARTIFACT_NODE_NONE);
	if (!artifact->nodes || !artifact->children ||
	    root >= artifact->counts.node_count)
		return fail(diagnostic, TCTI_FEATURE_DOMAIN_REFERENCE, root);
	memset(scratch->active, 0, artifact->counts.node_count);
	return evaluate_node(artifact, root, environment, scratch, 0, value,
			     diagnostic);
}

int tcti_feature_domain_evaluate_constraints(
	const struct tcti_feature_artifact *artifact,
	const struct tcti_feature_domain_environment *environment,
	struct tcti_feature_domain_scratch *scratch,
	tcti_feature_artifact_u8 *satisfied,
	struct tcti_feature_domain_diagnostic *diagnostic)
{
	tcti_feature_artifact_u32 index;

	if (!artifact || !satisfied || !artifact->constraints)
		return fail(diagnostic, TCTI_FEATURE_DOMAIN_INVALID_ARGUMENT,
			    TCTI_FEATURE_ARTIFACT_NODE_NONE);
	*satisfied = 1;
	for (index = 0; index < artifact->counts.constraint_count; index++) {
		struct tcti_feature_domain_value value;

		if (tcti_feature_domain_evaluate(artifact,
				artifact->constraints[index].node_index, environment,
				scratch, &value, diagnostic))
			return -1;
		if (value.kind != TCTI_FEATURE_DOMAIN_VALUE_BOOL)
			return fail(diagnostic, TCTI_FEATURE_DOMAIN_TYPE,
				    artifact->constraints[index].node_index);
		*satisfied &= value.boolean;
	}
	return 0;
}

int tcti_feature_domain_evaluate_constraint_union(
	const struct tcti_feature_artifact *artifact,
	const struct tcti_feature_domain_union_candidate *candidates,
	size_t candidate_count,
	struct tcti_feature_domain_union_scratch *scratch,
	struct tcti_feature_domain_union_result *result)
{
	size_t index;
	int failed = 0;

	if (!result)
		return -1;
	*result = (struct tcti_feature_domain_union_result) {
		.first_unsupported = {
			.error = TCTI_FEATURE_DOMAIN_OK,
			.node_index = TCTI_FEATURE_ARTIFACT_NODE_NONE,
		},
	};
	if (!artifact || !candidates || !candidate_count || !scratch ||
	    !scratch->evaluators || scratch->evaluator_count < candidate_count)
		return -1;
	result->candidate_count = candidate_count;
	for (index = 0; index < candidate_count; index++) {
		struct tcti_feature_domain_diagnostic diagnostic = {
			.error = TCTI_FEATURE_DOMAIN_OK,
			.node_index = TCTI_FEATURE_ARTIFACT_NODE_NONE,
		};
		tcti_feature_artifact_u8 satisfied = 0;

		if (!candidates[index].environment ||
		    tcti_feature_domain_evaluate_constraints(artifact,
			candidates[index].environment, &scratch->evaluators[index],
			&satisfied, &diagnostic)) {
			result->unsupported_count++;
			if (!failed)
				result->first_unsupported = diagnostic;
			failed = 1;
			continue;
		}
		result->evaluated_count++;
		if (satisfied)
			result->satisfied_count++;
		else
			result->unsatisfied_count++;
	}
	return failed ? -1 : 0;
}

struct tcnd_reader {
	const char *hex;
	size_t byte_count;
	struct tcti_feature_domain_tcnd_diagnostic *diagnostic;
	const struct tcti_feature_artifact *artifact;
};

static int tcnd_fail(struct tcnd_reader *reader,
			enum tcti_feature_domain_tcnd_error error, size_t offset)
{
	if (reader->diagnostic) {
		reader->diagnostic->error = error;
		reader->diagnostic->byte_offset = offset;
	}
	return -1;
}

static int tcnd_nibble(char character, unsigned int *value)
{
	if (character >= '0' && character <= '9') {
		*value = (unsigned int)(character - '0');
		return 0;
	}
	if (character >= 'a' && character <= 'f') {
		*value = (unsigned int)(character - 'a' + 10);
		return 0;
	}
	if (character >= 'A' && character <= 'F') {
		*value = (unsigned int)(character - 'A' + 10);
		return 0;
	}
	return -1;
}

static int tcnd_byte(struct tcnd_reader *reader, size_t offset,
		     unsigned char *value)
{
	unsigned int high;
	unsigned int low;

	if (offset >= reader->byte_count ||
	    tcnd_nibble(reader->hex[offset * 2U], &high) ||
	    tcnd_nibble(reader->hex[offset * 2U + 1U], &low))
		return tcnd_fail(reader, TCTI_FEATURE_DOMAIN_TCND_ENCODING, offset);
	*value = (unsigned char)((high << 4) | low);
	return 0;
}

static int tcnd_u32be(struct tcnd_reader *reader, size_t *offset,
		      size_t limit, tcti_feature_artifact_u32 *value)
{
	unsigned char bytes[4];
	size_t index;

	if (*offset > limit || limit - *offset < sizeof(bytes))
		return tcnd_fail(reader, TCTI_FEATURE_DOMAIN_TCND_ENCODING,
				 *offset);
	for (index = 0; index < sizeof(bytes); index++)
		if (tcnd_byte(reader, *offset + index, &bytes[index]))
			return -1;
	*offset += sizeof(bytes);
	*value = ((tcti_feature_artifact_u32)bytes[0] << 24) |
		 ((tcti_feature_artifact_u32)bytes[1] << 16) |
		 ((tcti_feature_artifact_u32)bytes[2] << 8) |
		 (tcti_feature_artifact_u32)bytes[3];
	return 0;
}

static int tcnd_parameter_matches(struct tcnd_reader *reader, size_t offset,
			  size_t length)
{
	tcti_feature_artifact_u32 index;

	if (!length)
		return 0;
	for (index = 0; index < reader->artifact->counts.parameter_count;
	     index++) {
		const char *name = reader->artifact->parameters[index].name;
		size_t name_length;
		size_t byte;

		if (!name)
			continue;
		name_length = strlen(name);
		if (name_length != length)
			continue;
		for (byte = 0; byte < length; byte++) {
			unsigned char actual;

			if (tcnd_byte(reader, offset + byte, &actual))
				return -1;
			if (actual != (unsigned char)name[byte])
				break;
		}
		if (byte == length)
			return 1;
	}
	return 0;
}

static int tcnd_text(struct tcnd_reader *reader, size_t *offset,
		     size_t limit, int require_feature)
{
	tcti_feature_artifact_u32 length;
	size_t start;

	if (tcnd_u32be(reader, offset, limit, &length) || !length ||
	    (size_t)length > limit - *offset)
		return tcnd_fail(reader, TCTI_FEATURE_DOMAIN_TCND_ENCODING, *offset);
	start = *offset;
	*offset += (size_t)length;
	if (!require_feature)
		return 0;
	{
		int match = tcnd_parameter_matches(reader, start, length);

		if (match < 0)
			return -1;
		if (!match)
		return tcnd_fail(reader, TCTI_FEATURE_DOMAIN_TCND_UNKNOWN_FEATURE,
				 start);
	}
	if (reader->diagnostic)
		reader->diagnostic->feature_terminals++;
	return 0;
}

static int tcnd_record(struct tcnd_reader *reader, size_t *offset,
		       size_t limit, unsigned int depth)
{
	unsigned char tag;
	tcti_feature_artifact_u32 payload_length;
	size_t payload_end;
	tcti_feature_artifact_u32 count;
	tcti_feature_artifact_u32 index;

	if (depth >= TCTI_TARGET_CONDITION_MAX_DEPTH || *offset >= limit ||
	    tcnd_byte(reader, (*offset)++, &tag) ||
	    tcnd_u32be(reader, offset, limit, &payload_length) ||
	    (size_t)payload_length > limit - *offset)
		return tcnd_fail(reader, depth >= TCTI_TARGET_CONDITION_MAX_DEPTH ?
				 TCTI_FEATURE_DOMAIN_TCND_DEPTH :
				 TCTI_FEATURE_DOMAIN_TCND_ENCODING, *offset);
	payload_end = *offset + (size_t)payload_length;
	switch (tag) {
	case TCTI_TARGET_CONDITION_BOOL: {
		unsigned char value;

		if (payload_length != 1U || tcnd_byte(reader, *offset, &value) ||
		    value > 1U)
			return tcnd_fail(reader, TCTI_FEATURE_DOMAIN_TCND_ENCODING,
					 *offset);
		*offset = payload_end;
		return 0;
	}
	case TCTI_TARGET_CONDITION_FEATURE:
		if (tcnd_text(reader, offset, payload_end, 1) ||
		    *offset != payload_end)
			return -1;
		return 0;
	case TCTI_TARGET_CONDITION_OPERAND:
	case TCTI_TARGET_CONDITION_VALUE:
		if (tcnd_text(reader, offset, payload_end, 0) ||
		    *offset != payload_end)
			return -1;
		return 0;
	case TCTI_TARGET_CONDITION_SET:
		if (tcnd_u32be(reader, offset, payload_end, &count))
			return -1;
		for (index = 0; index < count; index++)
			if (tcnd_record(reader, offset, payload_end, depth + 1U))
				return -1;
		break;
	case TCTI_TARGET_CONDITION_NOT:
		if (tcnd_record(reader, offset, payload_end, depth + 1U))
			return -1;
		break;
	case TCTI_TARGET_CONDITION_AND:
	case TCTI_TARGET_CONDITION_OR:
	case TCTI_TARGET_CONDITION_EQ:
	case TCTI_TARGET_CONDITION_NE:
	case TCTI_TARGET_CONDITION_IN:
		if (tcnd_record(reader, offset, payload_end, depth + 1U) ||
		    tcnd_record(reader, offset, payload_end, depth + 1U))
			return -1;
		break;
	default:
		return tcnd_fail(reader, TCTI_FEATURE_DOMAIN_TCND_ENCODING,
				 *offset - 5U);
	}
	if (*offset != payload_end)
		return tcnd_fail(reader, TCTI_FEATURE_DOMAIN_TCND_ENCODING, *offset);
	return 0;
}

int tcti_feature_domain_validate_tcnd_features(
	const struct tcti_feature_artifact *artifact, const char *tcnd_hex,
	struct tcti_feature_domain_tcnd_diagnostic *diagnostic)
{
	struct tcnd_reader reader;
	size_t hex_length;
	size_t offset = 0;
	static const unsigned char header[] = { 'T', 'C', 'N', 'D', 1U };
	size_t index;

	if (diagnostic)
		*diagnostic = (struct tcti_feature_domain_tcnd_diagnostic) {
			.error = TCTI_FEATURE_DOMAIN_TCND_OK,
		};
	if (!artifact || !artifact->parameters || !tcnd_hex)
		return tcnd_fail(&(struct tcnd_reader) { .diagnostic = diagnostic },
				 TCTI_FEATURE_DOMAIN_TCND_INVALID_ARGUMENT, 0);
	hex_length = strlen(tcnd_hex);
	if (!hex_length || (hex_length & 1U) || hex_length / 2U < sizeof(header) ||
	    hex_length / 2U > TCTI_TARGET_CONDITION_MAX_SERIALIZED_BYTES)
		return tcnd_fail(&(struct tcnd_reader) { .diagnostic = diagnostic },
				 TCTI_FEATURE_DOMAIN_TCND_ENCODING, 0);
	reader = (struct tcnd_reader) {
		.hex = tcnd_hex,
		.byte_count = hex_length / 2U,
		.diagnostic = diagnostic,
		.artifact = artifact,
	};
	for (index = 0; index < sizeof(header); index++) {
		unsigned char actual;

		if (tcnd_byte(&reader, index, &actual))
			return -1;
		if (actual != header[index])
			return tcnd_fail(&reader, TCTI_FEATURE_DOMAIN_TCND_ENCODING, index);
	}
	offset = sizeof(header);
	if (tcnd_record(&reader, &offset, reader.byte_count, 0) ||
	    offset != reader.byte_count)
		return -1;
	return 0;
}

enum tcnd_value_kind {
	TCND_VALUE_INVALID,
	TCND_VALUE_BOOL,
	TCND_VALUE_TEXT,
	TCND_VALUE_SET,
};

struct tcnd_value {
	enum tcnd_value_kind kind;
	tcti_feature_artifact_u8 boolean;
	const char *text;
	size_t text_length;
	size_t set_start;
	size_t set_end;
};

static int tcnd_text_equal(const struct tcnd_reader *reader, size_t offset,
			   size_t length, const char *text, size_t text_length)
{
	size_t index;

	if (!text || length != text_length)
		return 0;
	for (index = 0; index < length; index++) {
		unsigned char actual;

		if (tcnd_byte((struct tcnd_reader *)reader, offset + index, &actual))
			return -1;
		if (actual != (unsigned char)text[index])
			return 0;
	}
	return 1;
}

static int tcnd_text_read(const struct tcnd_reader *reader, size_t *offset,
			  size_t limit, size_t *start, size_t *length)
{
	tcti_feature_artifact_u32 encoded_length;

	if (tcnd_u32be((struct tcnd_reader *)reader, offset, limit,
		       &encoded_length) || !encoded_length ||
	    (size_t)encoded_length > limit - *offset)
		return tcnd_fail((struct tcnd_reader *)reader,
			TCTI_FEATURE_DOMAIN_TCND_ENCODING, *offset);
	*start = *offset;
	*length = encoded_length;
	*offset += encoded_length;
	return 0;
}

static int tcnd_feature_known(struct tcnd_reader *reader, size_t offset,
			      size_t length)
{
	return tcnd_parameter_matches(reader, offset, length);
}

static int tcnd_fail_value(struct tcnd_reader *reader,
		enum tcti_feature_domain_tcnd_error error, size_t offset)
{
	return tcnd_fail(reader, error, offset);
}

static int tcnd_evaluate_record(
	struct tcnd_reader *reader, size_t *offset, size_t limit, unsigned int depth,
	const struct tcti_feature_domain_tcnd_environment *environment,
	struct tcnd_value *value);

static int tcnd_values_equal(struct tcnd_reader *reader,
	const struct tcnd_value *left, const struct tcnd_value *right, int *equal);

static int tcnd_set_contains(struct tcnd_reader *reader,
	const struct tcnd_value *set, const struct tcnd_value *needle,
	const struct tcti_feature_domain_tcnd_environment *environment,
	unsigned int depth, int *matched)
{
	size_t offset = set->set_start;
	tcti_feature_artifact_u32 count;
	tcti_feature_artifact_u32 index;

	if (tcnd_u32be(reader, &offset, set->set_end, &count))
		return -1;
	*matched = 0;
	for (index = 0; index < count; index++) {
		struct tcnd_value item;
		int equal;

		if (tcnd_evaluate_record(reader, &offset, set->set_end, depth + 1U,
				environment, &item))
			return -1;
		if (tcnd_values_equal(reader, &item, needle, &equal))
			return -1;
		*matched |= equal;
	}
	if (offset != set->set_end)
		return tcnd_fail_value(reader, TCTI_FEATURE_DOMAIN_TCND_ENCODING,
			offset);
	return 0;
}

static int tcnd_values_equal(struct tcnd_reader *reader,
	const struct tcnd_value *left, const struct tcnd_value *right, int *equal)
{
	size_t index;

	if (left->kind != right->kind ||
	    (left->kind != TCND_VALUE_BOOL && left->kind != TCND_VALUE_TEXT))
		return tcnd_fail_value(reader, TCTI_FEATURE_DOMAIN_TCND_TYPE, 0);
	if (left->kind == TCND_VALUE_BOOL) {
		*equal = left->boolean == right->boolean;
		return 0;
	}
	if (left->text && right->text) {
		*equal = left->text_length == right->text_length &&
			!memcmp(left->text, right->text, left->text_length);
		return 0;
	}
	if (left->text)
		return (*equal = tcnd_text_equal(reader, right->set_start,
			right->text_length, left->text, left->text_length)) < 0 ? -1 : 0;
	if (right->text)
		return (*equal = tcnd_text_equal(reader, left->set_start,
			left->text_length, right->text, right->text_length)) < 0 ? -1 : 0;
	if (left->text_length != right->text_length) {
		*equal = 0;
		return 0;
	}
	for (index = 0; index < left->text_length; index++) {
		unsigned char left_byte;
		unsigned char right_byte;

		if (tcnd_byte(reader, left->set_start + index, &left_byte) ||
		    tcnd_byte(reader, right->set_start + index, &right_byte))
			return -1;
		if (left_byte != right_byte) {
			*equal = 0;
			return 0;
		}
	}
	*equal = 1;
	return 0;
}

static int tcnd_evaluate_record(
	struct tcnd_reader *reader, size_t *offset, size_t limit, unsigned int depth,
	const struct tcti_feature_domain_tcnd_environment *environment,
	struct tcnd_value *value)
{
	unsigned char tag;
	tcti_feature_artifact_u32 payload_length;
	size_t payload_end;
	struct tcnd_value left;
	struct tcnd_value right;

	if (depth >= TCTI_TARGET_CONDITION_MAX_DEPTH || *offset >= limit ||
	    tcnd_byte(reader, (*offset)++, &tag) ||
	    tcnd_u32be(reader, offset, limit, &payload_length) ||
	    (size_t)payload_length > limit - *offset)
		return tcnd_fail_value(reader, depth >= TCTI_TARGET_CONDITION_MAX_DEPTH ?
			TCTI_FEATURE_DOMAIN_TCND_DEPTH : TCTI_FEATURE_DOMAIN_TCND_ENCODING,
			*offset);
	payload_end = *offset + payload_length;
	*value = (struct tcnd_value) { 0 };
	switch (tag) {
	case TCTI_TARGET_CONDITION_BOOL: {
		unsigned char boolean;

		if (payload_length != 1U || tcnd_byte(reader, *offset, &boolean) ||
		    boolean > 1U)
			return tcnd_fail_value(reader, TCTI_FEATURE_DOMAIN_TCND_ENCODING,
				*offset);
		*offset = payload_end;
		value->kind = TCND_VALUE_BOOL;
		value->boolean = boolean;
		return 0;
	}
	case TCTI_TARGET_CONDITION_FEATURE: {
		size_t start;
		size_t length;
		int known;

		if (tcnd_text_read(reader, offset, payload_end, &start, &length) ||
		    *offset != payload_end)
			return -1;
		known = tcnd_feature_known(reader, start, length);
		if (known < 0)
			return -1;
		if (!known)
			return tcnd_fail_value(reader,
				TCTI_FEATURE_DOMAIN_TCND_UNKNOWN_FEATURE, start);
		if (!environment->feature || environment->feature(environment->context,
			reader->hex, start, length, &value->boolean) ||
		    value->boolean > 1U)
			return tcnd_fail_value(reader,
				TCTI_FEATURE_DOMAIN_TCND_MISSING_FEATURE, start);
		value->kind = TCND_VALUE_BOOL;
		return 0;
	}
	case TCTI_TARGET_CONDITION_OPERAND: {
		size_t start;
		size_t length;
		const char *text;
		size_t text_length;

		if (tcnd_text_read(reader, offset, payload_end, &start, &length) ||
		    *offset != payload_end)
			return -1;
		if (!environment->operand || environment->operand(environment->context,
			reader->hex, start, length, &text, &text_length) || !text)
			return tcnd_fail_value(reader,
				TCTI_FEATURE_DOMAIN_TCND_MISSING_OPERAND, start);
		value->kind = TCND_VALUE_TEXT;
		value->text = text;
		value->text_length = text_length;
		return 0;
	}
	case TCTI_TARGET_CONDITION_VALUE: {
		size_t start;
		size_t length;

		if (tcnd_text_read(reader, offset, payload_end, &start, &length) ||
		    *offset != payload_end)
			return -1;
		value->kind = TCND_VALUE_TEXT;
		value->set_start = start;
		value->text_length = length;
		return 0;
	}
	case TCTI_TARGET_CONDITION_SET: {
		tcti_feature_artifact_u32 count;
		size_t start = *offset;

		if (tcnd_u32be(reader, offset, payload_end, &count))
			return -1;
		while (count--) {
			struct tcnd_value item;

			if (tcnd_evaluate_record(reader, offset, payload_end, depth + 1U,
				environment, &item))
				return -1;
			if (item.kind != TCND_VALUE_TEXT)
				return tcnd_fail_value(reader,
					TCTI_FEATURE_DOMAIN_TCND_TYPE, *offset);
		}
		if (*offset != payload_end)
			return tcnd_fail_value(reader, TCTI_FEATURE_DOMAIN_TCND_ENCODING,
				*offset);
		value->kind = TCND_VALUE_SET;
		value->set_start = start;
		value->set_end = payload_end;
		return 0;
	}
	case TCTI_TARGET_CONDITION_NOT:
		if (tcnd_evaluate_record(reader, offset, payload_end, depth + 1U,
			environment, &left))
			return -1;
		if (*offset != payload_end || left.kind != TCND_VALUE_BOOL)
			return tcnd_fail_value(reader, TCTI_FEATURE_DOMAIN_TCND_TYPE,
				*offset);
		value->kind = TCND_VALUE_BOOL;
		value->boolean = !left.boolean;
		return 0;
	case TCTI_TARGET_CONDITION_AND:
	case TCTI_TARGET_CONDITION_OR:
		if (tcnd_evaluate_record(reader, offset, payload_end, depth + 1U,
			environment, &left))
			return -1;
		if (tcnd_evaluate_record(reader, offset, payload_end, depth + 1U,
			environment, &right))
			return -1;
		if (*offset != payload_end || left.kind != TCND_VALUE_BOOL ||
		    right.kind != TCND_VALUE_BOOL)
			return tcnd_fail_value(reader, TCTI_FEATURE_DOMAIN_TCND_TYPE,
				*offset);
		value->kind = TCND_VALUE_BOOL;
		value->boolean = tag == TCTI_TARGET_CONDITION_AND ?
			left.boolean && right.boolean : left.boolean || right.boolean;
		return 0;
	case TCTI_TARGET_CONDITION_EQ:
	case TCTI_TARGET_CONDITION_NE: {
		int equal;

		if (tcnd_evaluate_record(reader, offset, payload_end, depth + 1U,
			environment, &left) ||
		    tcnd_evaluate_record(reader, offset, payload_end, depth + 1U,
			environment, &right))
			return -1;
		if (*offset != payload_end ||
		    tcnd_values_equal(reader, &left, &right, &equal))
			return -1;
		value->kind = TCND_VALUE_BOOL;
		value->boolean = tag == TCTI_TARGET_CONDITION_EQ ? equal : !equal;
		return 0;
	}
	case TCTI_TARGET_CONDITION_IN: {
		int matched;

		if (tcnd_evaluate_record(reader, offset, payload_end, depth + 1U,
			environment, &left) ||
		    tcnd_evaluate_record(reader, offset, payload_end, depth + 1U,
			environment, &right))
			return -1;
		if (*offset != payload_end || right.kind != TCND_VALUE_SET ||
		    tcnd_set_contains(reader, &right, &left, environment, depth,
			&matched))
			return -1;
		value->kind = TCND_VALUE_BOOL;
		value->boolean = matched;
		return 0;
	}
	default:
		return tcnd_fail_value(reader, TCTI_FEATURE_DOMAIN_TCND_ENCODING,
			*offset - 5U);
	}
}

int tcti_feature_domain_evaluate_tcnd(
	const struct tcti_feature_artifact *artifact, const char *tcnd_hex,
	const struct tcti_feature_domain_tcnd_environment *environment,
	tcti_feature_artifact_u8 *satisfied,
	struct tcti_feature_domain_tcnd_diagnostic *diagnostic)
{
	struct tcnd_reader reader;
	static const unsigned char header[] = { 'T', 'C', 'N', 'D', 1U };
	size_t hex_length;
	size_t offset;
	size_t index;
	struct tcnd_value value;

	if (diagnostic)
		*diagnostic = (struct tcti_feature_domain_tcnd_diagnostic) {
			.error = TCTI_FEATURE_DOMAIN_TCND_OK,
		};
	if (!artifact || !artifact->parameters || !tcnd_hex || !environment ||
	    !satisfied)
		return tcnd_fail(&(struct tcnd_reader) { .diagnostic = diagnostic },
			TCTI_FEATURE_DOMAIN_TCND_INVALID_ARGUMENT, 0);
	hex_length = strlen(tcnd_hex);
	if (!hex_length || (hex_length & 1U) ||
	    hex_length / 2U < sizeof(header) ||
	    hex_length / 2U > TCTI_TARGET_CONDITION_MAX_SERIALIZED_BYTES)
		return tcnd_fail(&(struct tcnd_reader) { .diagnostic = diagnostic },
			TCTI_FEATURE_DOMAIN_TCND_ENCODING, 0);
	reader = (struct tcnd_reader) {
		.hex = tcnd_hex,
		.byte_count = hex_length / 2U,
		.diagnostic = diagnostic,
		.artifact = artifact,
	};
	for (index = 0; index < sizeof(header); index++) {
		unsigned char actual;

		if (tcnd_byte(&reader, index, &actual))
			return -1;
		if (actual != header[index])
			return tcnd_fail(&reader, TCTI_FEATURE_DOMAIN_TCND_ENCODING,
				index);
	}
	offset = sizeof(header);
	if (tcnd_evaluate_record(&reader, &offset, reader.byte_count, 0,
		environment, &value) || offset != reader.byte_count)
		return -1;
	if (value.kind != TCND_VALUE_BOOL)
		return tcnd_fail(&reader, TCTI_FEATURE_DOMAIN_TCND_TYPE, offset);
	*satisfied = value.boolean;
	return 0;
}

int tcti_feature_domain_evaluate_tcnd_union(
	const struct tcti_feature_artifact *artifact, const char *tcnd_hex,
	const struct tcti_feature_domain_tcnd_union_candidate *candidates,
	size_t candidate_count, struct tcti_feature_domain_tcnd_union_result *result)
{
	size_t index;
	int failed = 0;

	if (!result)
		return -1;
	*result = (struct tcti_feature_domain_tcnd_union_result) {
		.first_unsupported = {
			.error = TCTI_FEATURE_DOMAIN_TCND_OK,
		},
	};
	if (!artifact || !tcnd_hex || !candidates || !candidate_count)
		return -1;
	result->candidate_count = candidate_count;
	for (index = 0; index < candidate_count; index++) {
		struct tcti_feature_domain_tcnd_diagnostic diagnostic;
		tcti_feature_artifact_u8 satisfied;

		if (!candidates[index].environment ||
		    tcti_feature_domain_evaluate_tcnd(artifact, tcnd_hex,
			candidates[index].environment, &satisfied, &diagnostic)) {
			result->unsupported_count++;
			if (!failed)
				result->first_unsupported = diagnostic;
			failed = 1;
			continue;
		}
		result->evaluated_count++;
		if (satisfied)
			result->satisfied_count++;
		else
			result->unsatisfied_count++;
	}
	return failed ? -1 : 0;
}
