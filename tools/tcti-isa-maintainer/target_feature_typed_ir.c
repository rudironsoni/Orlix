/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_feature_typed_ir.h"
#include <stdlib.h>
#include <string.h>

#define TCTI_FEATURE_TYPED_MAX_DEPTH 256U

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

static int diagnostic(struct tcti_feature_typed_result *result,
			      const struct tcti_feature_node *node,
			      enum tcti_feature_typed_error code)
{
	if (reserve((void **)&result->diagnostics, &result->diagnostic_capacity,
		    result->diagnostic_count + 1, sizeof(*result->diagnostics)))
		return -1;
	result->diagnostics[result->diagnostic_count++] =
		(struct tcti_feature_typed_diagnostic){
			.code = code, .source_kind = node->kind,
			.provenance = node->provenance,
		};
	return 0;
}

static int parameter(const struct tcti_feature_model *model, const char *name)
{
	size_t index;

	if (!name)
		return -1;
	for (index = 0; index < model->parameter_count; index++)
		if (!strcmp(model->parameters[index].name, name))
			return (int)index;
	return -1;
}

static int hard_error(struct tcti_feature_typed_result *result,
		      const struct tcti_feature_node *node,
		      enum tcti_feature_typed_error code)
{
	diagnostic(result, node, code);
	return -1;
}

static struct tcti_typed_provenance provenance(const struct tcti_feature_node *node)
{
	return (struct tcti_typed_provenance){
		node->provenance.offset, node->provenance.length,
	};
}

static enum tcti_typed_op operation(enum tcti_feature_node_kind kind)
{
	switch (kind) {
	case TCTI_FEATURE_NOT:
		return TCTI_TYPED_NOT;
	case TCTI_FEATURE_AND:
		return TCTI_TYPED_AND;
	case TCTI_FEATURE_OR:
		return TCTI_TYPED_OR;
	case TCTI_FEATURE_EQ:
		return TCTI_TYPED_EQ;
	case TCTI_FEATURE_NE:
		return TCTI_TYPED_NE;
	case TCTI_FEATURE_LT:
		return TCTI_TYPED_LT;
	case TCTI_FEATURE_GT:
		return TCTI_TYPED_GT;
	case TCTI_FEATURE_GE:
		return TCTI_TYPED_GE;
	case TCTI_FEATURE_IN:
		return TCTI_TYPED_IN;
	case TCTI_FEATURE_IMPLIES:
		return TCTI_TYPED_IMPLIES;
	case TCTI_FEATURE_IFF:
		return TCTI_TYPED_IFF;
	case TCTI_FEATURE_UINT:
		return TCTI_TYPED_UINT;
	case TCTI_FEATURE_SINT:
		return TCTI_TYPED_SINT;
	default:
		return TCTI_TYPED_LITERAL;
	}
}

static int lower(const struct tcti_feature_model *model, uint32_t index,
		 unsigned depth, struct tcti_typed_expression *expression,
		 struct tcti_feature_typed_result *result, uint32_t *out);

static int lower_children(const struct tcti_feature_model *model,
			  const struct tcti_feature_node *node, unsigned depth,
			  struct tcti_typed_expression *expression,
			  struct tcti_feature_typed_result *result,
			  uint32_t **out_children)
{
	uint32_t *children;
	size_t index;

	if (!node->child_count || node->first_child == TCTI_FEATURE_NODE_NONE ||
	    node->first_child > model->child_count - node->child_count)
		return hard_error(result, node, TCTI_FEATURE_TYPED_TYPE);
	children = calloc(node->child_count, sizeof(*children));
	if (!children)
		return hard_error(result, node, TCTI_FEATURE_TYPED_MEMORY);
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

static int lower_dot_atom(const struct tcti_feature_model *model,
			  const struct tcti_feature_node *node,
			  struct tcti_typed_expression *expression,
			  struct tcti_feature_typed_result *result, uint32_t *out)
{
	uint32_t *children;
	enum tcti_typed_error error;
	size_t index;

	if (!node->child_count || node->first_child == TCTI_FEATURE_NODE_NONE ||
	    node->first_child > model->child_count - node->child_count)
		return hard_error(result, node, TCTI_FEATURE_TYPED_TYPE);
	children = calloc(node->child_count, sizeof(*children));
	if (!children)
		return hard_error(result, node, TCTI_FEATURE_TYPED_MEMORY);
	for (index = 0; index < node->child_count; index++) {
		const struct tcti_feature_node *part =
			&model->nodes[model->children[node->first_child + index]];

		if (part->kind != TCTI_FEATURE_IDENTIFIER || !part->text ||
		    tcti_typed_atom(expression, TCTI_TYPED_IDENTIFIER, part->text,
				    NULL, NULL, NULL, provenance(part), &children[index],
				    &error)) {
			free(children);
			return hard_error(result, node, TCTI_FEATURE_TYPED_TYPE);
		}
	}
	if (tcti_typed_compound(expression, TCTI_TYPED_DOT_ATOM, children,
				 node->child_count, provenance(node), out, &error)) {
		free(children);
		return hard_error(result, node, TCTI_FEATURE_TYPED_TYPE);
	}
	free(children);
	return 0;
}

static int lower(const struct tcti_feature_model *model, uint32_t index,
		 unsigned depth, struct tcti_typed_expression *expression,
		 struct tcti_feature_typed_result *result, uint32_t *out)
{
	const struct tcti_feature_node *node;
	uint32_t children[2];
	uint32_t *compound_children = NULL;
	enum tcti_typed_error error;
	size_t child_count;
	int parameter_index;
	struct tcti_typed_value value;

	if (index >= model->node_count || depth > TCTI_FEATURE_TYPED_MAX_DEPTH)
		return -1;
	node = &model->nodes[index];
	switch (node->kind) {
	case TCTI_FEATURE_BOOL:
		if (tcti_typed_value_from_u64(&value, (uint64_t)node->integer, 1,
					      &error))
			return hard_error(result, node, TCTI_FEATURE_TYPED_TYPE);
		return tcti_typed_literal(expression, TCTI_TYPED_BOOL, 0,
				   &value, 0, 0, provenance(node),
				   out, &error);
	case TCTI_FEATURE_IDENTIFIER:
		/* Feature conditions also name architectural version predicates,
		 * which are not Parameters.Boolean entries. Preserve them as Boolean
		 * atoms so a later domain binding can decide their interpretation. */
		if (!node->text)
			return hard_error(result, node, TCTI_FEATURE_TYPED_TYPE);
		parameter_index = parameter(model, node->text);
		return tcti_typed_boolean_atom(expression, node->text,
					       parameter_index >= 0, provenance(node), out,
					       &error);
	case TCTI_FEATURE_INTEGER:
		if (tcti_typed_value_from_u64(&value, (uint64_t)node->integer, 64,
					      &error))
			return hard_error(result, node, TCTI_FEATURE_TYPED_TYPE);
		return tcti_typed_literal(expression, TCTI_TYPED_OPERAND, 64,
				   &value, 0, 0, provenance(node),
				   out, &error);
	case TCTI_FEATURE_VALUE:
		if (!node->text)
			return hard_error(result, node, TCTI_FEATURE_TYPED_TYPE);
		return tcti_typed_value_atom(expression, node->text,
					     provenance(node), out, &error);
	case TCTI_FEATURE_FIELD:
		return tcti_typed_atom(expression, TCTI_TYPED_FIELD, NULL,
				       node->field.state, node->field.register_name,
				       node->field.selector, provenance(node), out, &error);
	case TCTI_FEATURE_DOT_ATOM:
		return lower_dot_atom(model, node, expression, result, out);
	case TCTI_FEATURE_SET:
		if (lower_children(model, node, depth, expression, result,
				   &compound_children))
			return -1;
		if (tcti_typed_compound(expression, TCTI_TYPED_SET,
					 compound_children, node->child_count,
					 provenance(node), out, &error)) {
			free(compound_children);
			return hard_error(result, node, TCTI_FEATURE_TYPED_TYPE);
		}
		free(compound_children);
		return 0;
	case TCTI_FEATURE_NOT:
		if (node->left == TCTI_FEATURE_NODE_NONE ||
		    lower(model, node->left, depth + 1, expression, result,
			  &children[0]))
			return hard_error(result, node, TCTI_FEATURE_TYPED_TYPE);
		child_count = 1;
		break;
	case TCTI_FEATURE_UINT:
	case TCTI_FEATURE_SINT:
		if (node->child_count != 1 ||
		    node->first_child == TCTI_FEATURE_NODE_NONE ||
		    node->first_child >= model->child_count ||
		    lower(model, model->children[node->first_child], depth + 1,
			  expression, result, &children[0]))
			return hard_error(result, node, TCTI_FEATURE_TYPED_TYPE);
		child_count = 1;
		break;
	case TCTI_FEATURE_AND:
	case TCTI_FEATURE_OR:
	case TCTI_FEATURE_EQ:
	case TCTI_FEATURE_NE:
	case TCTI_FEATURE_LT:
	case TCTI_FEATURE_GT:
	case TCTI_FEATURE_GE:
	case TCTI_FEATURE_IN:
	case TCTI_FEATURE_IMPLIES:
	case TCTI_FEATURE_IFF:
		if (node->left == TCTI_FEATURE_NODE_NONE ||
		    node->right == TCTI_FEATURE_NODE_NONE ||
		    lower(model, node->left, depth + 1, expression, result,
			  &children[0]) ||
		    lower(model, node->right, depth + 1, expression, result,
			  &children[1]))
			return hard_error(result, node, TCTI_FEATURE_TYPED_TYPE);
		child_count = 2;
		break;
	default:
		return hard_error(result, node, TCTI_FEATURE_TYPED_TYPE);
	}
	if (tcti_typed_apply(expression, operation(node->kind), children,
			     child_count, provenance(node), out, &error))
		return hard_error(result, node, TCTI_FEATURE_TYPED_TYPE);
	return 0;
}

int tcti_feature_typed_lower(const struct tcti_feature_model *model,
			     struct tcti_typed_expression *expression,
			     struct tcti_feature_typed_result *result)
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
		enum tcti_typed_error error;
		const struct tcti_feature_parameter *parameter = &model->parameters[index];

		if (!parameter->name ||
		    tcti_typed_boolean_atom(expression, parameter->name, 1,
				    (struct tcti_typed_provenance){
					parameter->provenance.offset,
					parameter->provenance.length,
				    }, &result->parameters[index], &error))
			goto fail;
		result->parameter_count++;
	}
	for (index = 0; index < model->node_count; index++) {
		if (model->nodes[index].kind > TCTI_FEATURE_VALUE)
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

void tcti_feature_typed_result_destroy(struct tcti_feature_typed_result *result)
{
	if (!result)
		return;
	free(result->constraints);
	free(result->parameters);
	free(result->diagnostics);
	memset(result, 0, sizeof(*result));
}
