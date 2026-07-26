/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_feature_typed_ir.h"
#include <stdlib.h>
#include <string.h>

#define ORLIX_TCTI_FEATURE_TYPED_MAX_DEPTH 256U

static int reserve(void **pointer, size_t *capacity, size_t needed, size_t size)
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

static int diagnostic(struct orlix_tcti_feature_typed_result *result,
			      const struct orlix_tcti_feature_node *node,
			      enum orlix_tcti_feature_typed_error code)
{
	if (reserve((void **)&result->diagnostics, &result->diagnostic_capacity,
		    result->diagnostic_count + 1, sizeof(*result->diagnostics)))
		return -1;
	result->diagnostics[result->diagnostic_count++] =
		(struct orlix_tcti_feature_typed_diagnostic){
			.code = code, .source_kind = node->kind,
			.provenance = node->provenance,
		};
	return 0;
}

static int parameter(const struct orlix_tcti_feature_model *model, const char *name)
{
	size_t index;

	if (!name)
		return -1;
	for (index = 0; index < model->parameter_count; index++)
		if (!strcmp(model->parameters[index].name, name))
			return (int)index;
	return -1;
}

static int hard_error(struct orlix_tcti_feature_typed_result *result,
		      const struct orlix_tcti_feature_node *node,
		      enum orlix_tcti_feature_typed_error code)
{
	diagnostic(result, node, code);
	return -1;
}

static struct orlix_tcti_typed_provenance provenance(const struct orlix_tcti_feature_node *node)
{
	return (struct orlix_tcti_typed_provenance){
		node->provenance.offset, node->provenance.length,
	};
}

static enum orlix_tcti_typed_op operation(enum orlix_tcti_feature_node_kind kind)
{
	switch (kind) {
	case ORLIX_TCTI_FEATURE_NOT:
		return ORLIX_TCTI_TYPED_NOT;
	case ORLIX_TCTI_FEATURE_AND:
		return ORLIX_TCTI_TYPED_AND;
	case ORLIX_TCTI_FEATURE_OR:
		return ORLIX_TCTI_TYPED_OR;
	case ORLIX_TCTI_FEATURE_EQ:
		return ORLIX_TCTI_TYPED_EQ;
	case ORLIX_TCTI_FEATURE_NE:
		return ORLIX_TCTI_TYPED_NE;
	case ORLIX_TCTI_FEATURE_LT:
		return ORLIX_TCTI_TYPED_LT;
	case ORLIX_TCTI_FEATURE_GT:
		return ORLIX_TCTI_TYPED_GT;
	case ORLIX_TCTI_FEATURE_GE:
		return ORLIX_TCTI_TYPED_GE;
	case ORLIX_TCTI_FEATURE_IN:
		return ORLIX_TCTI_TYPED_IN;
	case ORLIX_TCTI_FEATURE_IMPLIES:
		return ORLIX_TCTI_TYPED_IMPLIES;
	case ORLIX_TCTI_FEATURE_IFF:
		return ORLIX_TCTI_TYPED_IFF;
	case ORLIX_TCTI_FEATURE_UINT:
		return ORLIX_TCTI_TYPED_UINT;
	case ORLIX_TCTI_FEATURE_SINT:
		return ORLIX_TCTI_TYPED_SINT;
	default:
		return ORLIX_TCTI_TYPED_LITERAL;
	}
}

static int lower(const struct orlix_tcti_feature_model *model, uint32_t index,
		 unsigned depth, struct orlix_tcti_typed_expression *expression,
		 struct orlix_tcti_feature_typed_result *result, uint32_t *out);

static int lower_children(const struct orlix_tcti_feature_model *model,
			  const struct orlix_tcti_feature_node *node, unsigned depth,
			  struct orlix_tcti_typed_expression *expression,
			  struct orlix_tcti_feature_typed_result *result,
			  uint32_t **out_children)
{
	uint32_t *children;
	size_t index;

	if (!node->child_count || node->first_child == ORLIX_TCTI_FEATURE_NODE_NONE ||
	    node->first_child > model->child_count - node->child_count)
		return hard_error(result, node, ORLIX_TCTI_FEATURE_TYPED_TYPE);
	children = calloc(node->child_count, sizeof(*children));
	if (!children)
		return hard_error(result, node, ORLIX_TCTI_FEATURE_TYPED_MEMORY);
	for (index = 0; index < node->child_count; index++) {
		if (lower(model, model->children[node->first_child + index], depth + 1,
			  expression, result, &children[index])) {
			free(children);
			return -1;
		}
	}
	*out_children = children;
	return 0;
}

static int lower_dot_atom(const struct orlix_tcti_feature_model *model,
			  const struct orlix_tcti_feature_node *node,
			  struct orlix_tcti_typed_expression *expression,
			  struct orlix_tcti_feature_typed_result *result, uint32_t *out)
{
	uint32_t *children;
	enum orlix_tcti_typed_error error;
	size_t index;

	if (!node->child_count || node->first_child == ORLIX_TCTI_FEATURE_NODE_NONE ||
	    node->first_child > model->child_count - node->child_count)
		return hard_error(result, node, ORLIX_TCTI_FEATURE_TYPED_TYPE);
	children = calloc(node->child_count, sizeof(*children));
	if (!children)
		return hard_error(result, node, ORLIX_TCTI_FEATURE_TYPED_MEMORY);
	for (index = 0; index < node->child_count; index++) {
		const struct orlix_tcti_feature_node *part =
			&model->nodes[model->children[node->first_child + index]];

		if (part->kind != ORLIX_TCTI_FEATURE_IDENTIFIER || !part->text ||
		    orlix_tcti_typed_atom(expression, ORLIX_TCTI_TYPED_IDENTIFIER, part->text,
				    NULL, NULL, NULL, provenance(part), &children[index],
				    &error)) {
			free(children);
			return hard_error(result, node, ORLIX_TCTI_FEATURE_TYPED_TYPE);
		}
	}
	if (orlix_tcti_typed_compound(expression, ORLIX_TCTI_TYPED_DOT_ATOM, children,
				 node->child_count, provenance(node), out, &error)) {
		free(children);
		return hard_error(result, node, ORLIX_TCTI_FEATURE_TYPED_TYPE);
	}
	free(children);
	return 0;
}

static int lower(const struct orlix_tcti_feature_model *model, uint32_t index,
		 unsigned depth, struct orlix_tcti_typed_expression *expression,
		 struct orlix_tcti_feature_typed_result *result, uint32_t *out)
{
	const struct orlix_tcti_feature_node *node;
	uint32_t children[2];
	uint32_t *compound_children = NULL;
	enum orlix_tcti_typed_error error;
	size_t child_count;
	int parameter_index;
	struct orlix_tcti_typed_value value;

	if (index >= model->node_count || depth > ORLIX_TCTI_FEATURE_TYPED_MAX_DEPTH)
		return -1;
	node = &model->nodes[index];
	switch (node->kind) {
	case ORLIX_TCTI_FEATURE_BOOL:
		if (orlix_tcti_typed_value_from_u64(&value, (uint64_t)node->integer, 1,
					      &error))
			return hard_error(result, node, ORLIX_TCTI_FEATURE_TYPED_TYPE);
		return orlix_tcti_typed_literal(expression, ORLIX_TCTI_TYPED_BOOL, 0,
				   &value, 0, 0, provenance(node),
				   out, &error);
	case ORLIX_TCTI_FEATURE_IDENTIFIER:
		/* Feature conditions also name architectural version predicates,
		 * which are not Parameters.Boolean entries. Preserve them as Boolean
		 * atoms so a later domain binding can decide their interpretation. */
		if (!node->text)
			return hard_error(result, node, ORLIX_TCTI_FEATURE_TYPED_TYPE);
		parameter_index = parameter(model, node->text);
		return orlix_tcti_typed_boolean_atom(expression, node->text,
					       parameter_index >= 0, provenance(node), out,
					       &error);
	case ORLIX_TCTI_FEATURE_INTEGER:
		if (orlix_tcti_typed_value_from_u64(&value, (uint64_t)node->integer, 64,
					      &error))
			return hard_error(result, node, ORLIX_TCTI_FEATURE_TYPED_TYPE);
		return orlix_tcti_typed_literal(expression, ORLIX_TCTI_TYPED_OPERAND, 64,
				   &value, 0, 0, provenance(node),
				   out, &error);
	case ORLIX_TCTI_FEATURE_VALUE:
		if (!node->text)
			return hard_error(result, node, ORLIX_TCTI_FEATURE_TYPED_TYPE);
		return orlix_tcti_typed_value_atom(expression, node->text,
					     provenance(node), out, &error);
	case ORLIX_TCTI_FEATURE_FIELD:
		return orlix_tcti_typed_atom(expression, ORLIX_TCTI_TYPED_FIELD, NULL,
				       node->field.state, node->field.register_name,
				       node->field.selector, provenance(node), out, &error);
	case ORLIX_TCTI_FEATURE_DOT_ATOM:
		return lower_dot_atom(model, node, expression, result, out);
	case ORLIX_TCTI_FEATURE_SET:
		if (lower_children(model, node, depth, expression, result,
				   &compound_children))
			return -1;
		if (orlix_tcti_typed_compound(expression, ORLIX_TCTI_TYPED_SET,
					 compound_children, node->child_count,
					 provenance(node), out, &error)) {
			free(compound_children);
			return hard_error(result, node, ORLIX_TCTI_FEATURE_TYPED_TYPE);
		}
		free(compound_children);
		return 0;
	case ORLIX_TCTI_FEATURE_NOT:
		if (node->left == ORLIX_TCTI_FEATURE_NODE_NONE ||
		    lower(model, node->left, depth + 1, expression, result,
			  &children[0]))
			return hard_error(result, node, ORLIX_TCTI_FEATURE_TYPED_TYPE);
		child_count = 1;
		break;
	case ORLIX_TCTI_FEATURE_UINT:
	case ORLIX_TCTI_FEATURE_SINT:
		if (node->child_count != 1 ||
		    node->first_child == ORLIX_TCTI_FEATURE_NODE_NONE ||
		    node->first_child >= model->child_count ||
		    lower(model, model->children[node->first_child], depth + 1,
			  expression, result, &children[0]))
			return hard_error(result, node, ORLIX_TCTI_FEATURE_TYPED_TYPE);
		child_count = 1;
		break;
	case ORLIX_TCTI_FEATURE_AND:
	case ORLIX_TCTI_FEATURE_OR:
	case ORLIX_TCTI_FEATURE_EQ:
	case ORLIX_TCTI_FEATURE_NE:
	case ORLIX_TCTI_FEATURE_LT:
	case ORLIX_TCTI_FEATURE_GT:
	case ORLIX_TCTI_FEATURE_GE:
	case ORLIX_TCTI_FEATURE_IN:
	case ORLIX_TCTI_FEATURE_IMPLIES:
	case ORLIX_TCTI_FEATURE_IFF:
		if (node->left == ORLIX_TCTI_FEATURE_NODE_NONE ||
		    node->right == ORLIX_TCTI_FEATURE_NODE_NONE ||
		    lower(model, node->left, depth + 1, expression, result,
			  &children[0]) ||
		    lower(model, node->right, depth + 1, expression, result,
			  &children[1]))
			return hard_error(result, node, ORLIX_TCTI_FEATURE_TYPED_TYPE);
		child_count = 2;
		break;
	default:
		return hard_error(result, node, ORLIX_TCTI_FEATURE_TYPED_TYPE);
	}
	if (orlix_tcti_typed_apply(expression, operation(node->kind), children,
			     child_count, provenance(node), out, &error))
		return hard_error(result, node, ORLIX_TCTI_FEATURE_TYPED_TYPE);
	return 0;
}

int orlix_tcti_feature_typed_lower(const struct orlix_tcti_feature_model *model,
			     struct orlix_tcti_typed_expression *expression,
			     struct orlix_tcti_feature_typed_result *result)
{
	size_t index;

	if (!model || !expression || !result)
		return -1;
	memset(result, 0, sizeof(*result));
	result->parameters = calloc(model->parameter_count, sizeof(*result->parameters));
	result->constraints = calloc(model->constraint_count, sizeof(*result->constraints));
	if ((model->parameter_count && !result->parameters) ||
	    (model->constraint_count && !result->constraints))
		goto fail;
	for (index = 0; index < model->parameter_count; index++) {
		enum orlix_tcti_typed_error error;
		const struct orlix_tcti_feature_parameter *parameter = &model->parameters[index];

		if (!parameter->name ||
		    orlix_tcti_typed_boolean_atom(expression, parameter->name, 1,
				    (struct orlix_tcti_typed_provenance){
					parameter->provenance.offset,
					parameter->provenance.length,
				    }, &result->parameters[index], &error))
			goto fail;
		result->parameter_count++;
	}
	for (index = 0; index < model->node_count; index++) {
		if (model->nodes[index].kind > ORLIX_TCTI_FEATURE_VALUE)
			goto fail;
		result->grammar_counts[model->nodes[index].kind]++;
	}
	for (index = 0; index < model->constraint_count; index++) {
		if (lower(model, model->constraints[index], 0, expression, result,
			  &result->constraints[index]))
			goto fail;
		result->constraint_count++;
	}
	return 0;
fail:
	return -1;
}

void orlix_tcti_feature_typed_result_destroy(struct orlix_tcti_feature_typed_result *result)
{
	if (!result)
		return;
	free(result->constraints);
	free(result->parameters);
	free(result->diagnostics);
	memset(result, 0, sizeof(*result));
}
