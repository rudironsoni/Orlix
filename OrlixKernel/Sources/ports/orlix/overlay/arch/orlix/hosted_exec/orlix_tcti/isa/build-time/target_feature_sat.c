/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_feature_sat.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct clause { int *literals; size_t count; };
struct cnf { struct clause *clauses; size_t count, capacity; unsigned int variables; };
struct budget {
	size_t branches, branch_limit, allocations, allocation_limit;
	int exhausted;
};
struct compiler {
	const struct orlix_tcti_feature_model *model;
	const struct orlix_tcti_target_inventory *inventory;
	const struct orlix_tcti_feature_field_domain_bindings *field_domains;
	struct cnf cnf;
	int *feature_literals;
	int *target_literals;
	unsigned char *target_operand_dependencies;
	signed char *seed;
	size_t seed_count, seed_capacity;
	struct field_symbol *field_symbols;
	size_t field_symbol_count, field_symbol_capacity;
	struct operand_symbol *operand_symbols;
	size_t operand_symbol_count, operand_symbol_capacity;
	struct configuration_symbol *configuration_symbols;
	size_t configuration_symbol_count, configuration_symbol_capacity;
	uint32_t *free_feature_nodes;
	size_t free_feature_node_count, free_feature_node_capacity;
	size_t current_leaf;
	struct budget budget;
	struct orlix_tcti_target_feature_sat_error *error;
};
struct solver {
	const struct cnf *cnf;
	signed char *values;
	signed char *phase;
	unsigned int *trail;
	size_t trail_count, trail_capacity;
	struct budget *budget;
};

enum numeric_kind {
	NUMERIC_UNSIGNED,
	NUMERIC_SIGNED,
};

struct numeric_value {
	uint64_t low;
	uint64_t high;
	unsigned int width;
	enum numeric_kind kind;
};

struct field_symbol {
	uint32_t identity_group_index;
	const struct orlix_tcti_feature_field_domain_binding *binding;
	int *bits;
	unsigned int width;
};

struct operand_symbol {
	size_t leaf_index;
	const char *name;
	int *bits;
	uint32_t fixed_value;
	unsigned int width;
};

struct configuration_symbol {
	uint32_t representative;
	struct field_symbol value;
};

static int feature_child(const struct orlix_tcti_feature_model *model,
	uint32_t index, uint32_t child, uint32_t *result);

static void fail(struct orlix_tcti_target_feature_sat_error *error,
		enum orlix_tcti_target_feature_sat_error_code code, size_t leaf,
		struct orlix_tcti_feature_provenance provenance)
{
	if (error && error->code == ORLIX_TCTI_TARGET_FEATURE_SAT_OK)
		*error = (struct orlix_tcti_target_feature_sat_error) {
			.code = code, .leaf_index = leaf, .provenance = provenance,
		};
}

static int multiply(size_t left, size_t right, size_t *result)
{
	if (left && right > SIZE_MAX / left)
		return -1;
	*result = left * right;
	return 0;
}

static int charge(struct budget *budget, size_t count, size_t size)
{
	size_t bytes;

	if (multiply(count, size, &bytes) || bytes > budget->allocation_limit - budget->allocations) {
		budget->exhausted = 1;
		return -1;
	}
	budget->allocations += bytes;
	return 0;
}

static void discharge(struct budget *budget, size_t count, size_t size)
{
	size_t bytes;

	if (!multiply(count, size, &bytes) && bytes <= budget->allocations)
		budget->allocations -= bytes;
}

static void *budget_calloc(struct budget *budget, size_t count, size_t size)
{
	if (charge(budget, count, size))
		return NULL;
	return calloc(count, size);
}

static void *budget_malloc(struct budget *budget, size_t count, size_t size)
{
	if (charge(budget, count, size))
		return NULL;
	return malloc(count * size);
}

static int reserve(struct budget *budget, void **pointer, size_t *capacity,
		   size_t count, size_t size)
{
	size_t next = *capacity ? *capacity : 32U;
	void *grown;

	if (count <= *capacity)
		return 0;

	while (next < count) {
		if (next > SIZE_MAX / 2U)
			return -1;
		next *= 2U;
	}
	if (next > SIZE_MAX / size || charge(budget, next - *capacity, size))
		return -1;
	grown = realloc(*pointer, next * size);
	if (!grown)
		return -1;
	*pointer = grown;
	*capacity = next;
	return 0;
}

static void cnf_destroy(struct cnf *cnf)
{
	size_t index;

	for (index = 0; index < cnf->count; index++)
		free(cnf->clauses[index].literals);
	free(cnf->clauses);
	*cnf = (struct cnf) { 0 };
}

static int cnf_clause(struct compiler *compiler, const int *literals, size_t count)
{
	struct clause *clause;

	if (!count || reserve(&compiler->budget, (void **)&compiler->cnf.clauses,
			&compiler->cnf.capacity, compiler->cnf.count + 1U,
			sizeof(*compiler->cnf.clauses)))
		return -1;
	clause = &compiler->cnf.clauses[compiler->cnf.count++];
	clause->literals = budget_malloc(&compiler->budget, count,
					sizeof(*clause->literals));
	if (!clause->literals)
		return -1;
	memcpy(clause->literals, literals, count * sizeof(*literals));
	clause->count = count;
	return 0;
}

static int fresh(struct compiler *compiler)
{
	if (compiler->cnf.variables >= INT_MAX) {
		compiler->budget.exhausted = 1;
		return 0;
	}
	return (int)++compiler->cnf.variables;
}

static int equivalent_not(struct compiler *compiler, int result, int child)
{
	const int first[] = { -result, -child };
	const int second[] = { result, child };
	return cnf_clause(compiler, first, 2) || cnf_clause(compiler, second, 2);
}

static int equivalent_and(struct compiler *compiler, int result, int left, int right)
{
	const int first[] = { -result, left };
	const int second[] = { -result, right };
	const int third[] = { result, -left, -right };
	return cnf_clause(compiler, first, 2) || cnf_clause(compiler, second, 2) ||
		cnf_clause(compiler, third, 3);
}

static int equivalent_or(struct compiler *compiler, int result, int left, int right)
{
	const int first[] = { result, -left };
	const int second[] = { result, -right };
	const int third[] = { -result, left, right };
	return cnf_clause(compiler, first, 2) || cnf_clause(compiler, second, 2) ||
		cnf_clause(compiler, third, 3);
}

static int equivalent_iff(struct compiler *compiler, int result, int left, int right)
{
	const int first[] = { -result, -left, right };
	const int second[] = { -result, left, -right };
	const int third[] = { result, left, right };
	const int fourth[] = { result, -left, -right };
	return cnf_clause(compiler, first, 3) || cnf_clause(compiler, second, 3) ||
		cnf_clause(compiler, third, 3) || cnf_clause(compiler, fourth, 3);
}

static int parameter_index(const struct orlix_tcti_feature_model *model, const char *name)
{
	size_t index;

	if (!name)
		return -1;
	for (index = 0; index < model->parameter_count; index++)
		if (model->parameters[index].name && !strcmp(model->parameters[index].name, name))
			return (int)index;
	return -1;
}

static int collect_free_feature_nodes(struct compiler *compiler)
{
	size_t index;

	for (index = 0; index < compiler->model->node_count; index++) {
		const struct orlix_tcti_feature_node *node =
			&compiler->model->nodes[index];
		size_t prior;

		if (node->kind != ORLIX_TCTI_FEATURE_IDENTIFIER || !node->text ||
		    parameter_index(compiler->model, node->text) >= 0 ||
		    !compiler->feature_literals[index])
			continue;
		for (prior = 0; prior < compiler->free_feature_node_count; prior++)
			if (!strcmp(compiler->model->nodes[
					compiler->free_feature_nodes[prior]].text,
				    node->text))
				break;
		if (prior != compiler->free_feature_node_count)
			continue;
		if (reserve(&compiler->budget,
			    (void **)&compiler->free_feature_nodes,
			    &compiler->free_feature_node_capacity,
			    compiler->free_feature_node_count + 1U,
			    sizeof(*compiler->free_feature_nodes)))
			return -1;
		compiler->free_feature_nodes[compiler->free_feature_node_count++] =
			(uint32_t)index;
	}
	return 0;
}

static int parse_bit_string(const char *text, struct numeric_value *value)
{
	size_t index, length;

	if (!text || !value)
		return -1;
	length = strlen(text);
	if (length < 3U || text[0] != '\'' || text[length - 1U] != '\'' ||
		length - 2U > 128U)
		return -1;
	*value = (struct numeric_value) {
		.width = (unsigned int)(length - 2U),
		.kind = NUMERIC_UNSIGNED,
	};
	for (index = 1U; index + 1U < length; index++) {
		unsigned int bit = (unsigned int)(length - 2U - index);

		if (text[index] != '0' && text[index] != '1')
			return -1;
		if (text[index] == '1') {
			if (bit < 64U)
				value->low |= UINT64_C(1) << bit;
			else
				value->high |= UINT64_C(1) << (bit - 64U);
		}
	}
	return value->width ? 0 : -1;
}

static int numeric_node(const struct orlix_tcti_feature_model *model,
	uint32_t index, struct numeric_value *value)
{
	const struct orlix_tcti_feature_node *node;
	uint32_t child;

	if (!model || !value || index >= model->node_count)
		return -1;
	node = &model->nodes[index];
	switch (node->kind) {
	case ORLIX_TCTI_FEATURE_INTEGER:
		*value = (struct numeric_value) {
			.low = (uint64_t)node->integer,
			.high = node->integer < 0 ? UINT64_MAX : 0,
			.width = 64U,
			.kind = NUMERIC_SIGNED,
		};
		return 0;
	case ORLIX_TCTI_FEATURE_VALUE:
		return parse_bit_string(node->text, value);
	case ORLIX_TCTI_FEATURE_UINT:
	case ORLIX_TCTI_FEATURE_SINT:
		if (feature_child(model, index, 0, &child) ||
			numeric_node(model, child, value))
			return -1;
		value->kind = node->kind == ORLIX_TCTI_FEATURE_UINT ?
			NUMERIC_UNSIGNED : NUMERIC_SIGNED;
		return 0;
	default:
		return -1;
	}
}

static int numeric_bit(const struct numeric_value *value, unsigned int bit)
{
	if (bit >= value->width)
		return value->kind == NUMERIC_SIGNED &&
			((value->width <= 64U ? value->low : value->high) >>
			 ((value->width - 1U) & 63U)) & 1U;
	return bit < 64U ? (int)((value->low >> bit) & 1U) :
		(int)((value->high >> (bit - 64U)) & 1U);
}

static int compare_numeric(const struct numeric_value *left,
	const struct numeric_value *right, int *order)
{
	unsigned int width, bit;
	int left_sign, right_sign;

	if (!left || !right || !order || !left->width || !right->width ||
		left->width > 128U || right->width > 128U || left->kind != right->kind)
		return -1;
	width = left->width > right->width ? left->width : right->width;
	left_sign = left->kind == NUMERIC_SIGNED ? numeric_bit(left, width - 1U) : 0;
	right_sign = right->kind == NUMERIC_SIGNED ? numeric_bit(right, width - 1U) : 0;
	if (left_sign != right_sign) {
		*order = left_sign ? -1 : 1;
		return 0;
	}
	for (bit = width; bit-- > 0U; ) {
		int left_bit = numeric_bit(left, bit);
		int right_bit = numeric_bit(right, bit);

		if (left_bit != right_bit) {
			*order = left_bit ? 1 : -1;
			return 0;
		}
	}
	*order = 0;
	return 0;
}

static int boolean_constant(struct compiler *compiler, int value, int *literal)
{
	int result = fresh(compiler);
	int unit;

	if (!result)
		return -1;
	unit = value ? result : -result;
	if (cnf_clause(compiler, &unit, 1U))
		return -1;
	*literal = result;
	return 0;
}

static const struct orlix_tcti_feature_field_domain_binding *field_binding(
	const struct compiler *compiler, uint32_t feature_node_index)
{
	size_t index;

	if (!compiler->field_domains)
		return NULL;
	for (index = 0; index < compiler->field_domains->count; index++)
		if (compiler->field_domains->items[index].feature_node_index ==
			feature_node_index)
			return &compiler->field_domains->items[index];
	return NULL;
}

static int field_domain_constrain(
	struct compiler *compiler,
	const struct orlix_tcti_feature_field_domain_binding *binding,
	struct field_symbol *symbol);

static int field_symbol_create(struct compiler *compiler,
	const struct orlix_tcti_feature_field_domain_binding *binding,
	struct field_symbol **symbol)
{
	struct field_symbol *created;
	unsigned int bit;

	if (!compiler->field_domains ||
		binding->disposition != ORLIX_TCTI_FEATURE_FIELD_DOMAIN_MAPPED ||
		!binding->alternative_count || !binding->domain.field_width ||
		binding->domain.field_width > 128U)
		return -1;
	if (reserve(&compiler->budget,
		(void **)&compiler->field_symbols, &compiler->field_symbol_capacity,
		compiler->field_symbol_count + 1U, sizeof(*compiler->field_symbols)))
		return -1;
	created = &compiler->field_symbols[compiler->field_symbol_count++];
	*created = (struct field_symbol) {
		.identity_group_index = binding->identity_group_index,
		.binding = binding,
		.width = binding->domain.field_width,
	};
	created->bits = budget_calloc(&compiler->budget, created->width,
		sizeof(*created->bits));
	if (!created->bits)
		return -1;
	for (bit = 0; bit < created->width; bit++) {
		created->bits[bit] = fresh(compiler);
		if (!created->bits[bit])
			return -1;
	}
	if (field_domain_constrain(compiler, binding, created))
		return -1;
	*symbol = created;
	return 0;
}

static int field_symbol_get(struct compiler *compiler,
	const struct orlix_tcti_feature_field_domain_binding *binding,
	struct field_symbol **symbol)
{
	size_t index;

	for (index = 0; index < compiler->field_symbol_count; index++)
		if (compiler->field_symbols[index].identity_group_index ==
			binding->identity_group_index) {
			*symbol = &compiler->field_symbols[index];
			return 0;
		}
	return field_symbol_create(compiler, binding, symbol);
}

static int field_cast(const struct orlix_tcti_feature_model *model,
	uint32_t index, uint32_t *field_index, enum numeric_kind *kind)
{
	const struct orlix_tcti_feature_node *node;
	uint32_t child;

	if (!model || index >= model->node_count)
		return -1;
	node = &model->nodes[index];
	if (node->kind != ORLIX_TCTI_FEATURE_UINT &&
		node->kind != ORLIX_TCTI_FEATURE_SINT)
		return -1;
	if (feature_child(model, index, 0, &child) || child >= model->node_count ||
		model->nodes[child].kind != ORLIX_TCTI_FEATURE_FIELD)
		return -1;
	*field_index = child;
	*kind = node->kind == ORLIX_TCTI_FEATURE_UINT ?
		NUMERIC_UNSIGNED : NUMERIC_SIGNED;
	return 0;
}

static int configuration_scalar(const struct orlix_tcti_feature_model *model,
	uint32_t index)
{
	uint32_t child;

	if (!model || index >= model->node_count ||
		(model->nodes[index].kind != ORLIX_TCTI_FEATURE_UINT &&
		 model->nodes[index].kind != ORLIX_TCTI_FEATURE_SINT) ||
		feature_child(model, index, 0, &child) || child >= model->node_count)
		return 0;
	return model->nodes[child].kind == ORLIX_TCTI_FEATURE_DOT_ATOM;
}

static int dot_atom_equal(const struct orlix_tcti_feature_model *model,
	uint32_t left, uint32_t right)
{
	uint32_t item, left_child, right_child;

	if (left >= model->node_count || right >= model->node_count ||
		model->nodes[left].kind != ORLIX_TCTI_FEATURE_DOT_ATOM ||
		model->nodes[right].kind != ORLIX_TCTI_FEATURE_DOT_ATOM ||
		model->nodes[left].child_count != model->nodes[right].child_count)
		return 0;
	for (item = 0; item < model->nodes[left].child_count; item++) {
		if (feature_child(model, left, item, &left_child) ||
			feature_child(model, right, item, &right_child) ||
			left_child >= model->node_count || right_child >= model->node_count ||
			model->nodes[left_child].kind != ORLIX_TCTI_FEATURE_IDENTIFIER ||
			model->nodes[right_child].kind != ORLIX_TCTI_FEATURE_IDENTIFIER ||
			!model->nodes[left_child].text || !model->nodes[right_child].text ||
			strcmp(model->nodes[left_child].text, model->nodes[right_child].text))
			return 0;
	}
	return 1;
}

static int configuration_symbol_get(struct compiler *compiler,
	uint32_t dot_atom, struct field_symbol **symbol)
{
	struct configuration_symbol *created;
	size_t index;
	unsigned int bit;

	for (index = 0; index < compiler->configuration_symbol_count; index++)
		if (dot_atom_equal(compiler->model,
			compiler->configuration_symbols[index].representative, dot_atom)) {
			*symbol = &compiler->configuration_symbols[index].value;
			return 0;
		}
	if (reserve(&compiler->budget, (void **)&compiler->configuration_symbols,
		&compiler->configuration_symbol_capacity,
		compiler->configuration_symbol_count + 1U,
		sizeof(*compiler->configuration_symbols)))
		return -1;
	created = &compiler->configuration_symbols[
		compiler->configuration_symbol_count++];
	*created = (struct configuration_symbol) {
		.representative = dot_atom,
		.value = { .width = 128U },
	};
	created->value.bits = budget_calloc(&compiler->budget, 128U,
		sizeof(*created->value.bits));
	if (!created->value.bits)
		return -1;
	for (bit = 0; bit < 128U; bit++) {
		created->value.bits[bit] = fresh(compiler);
		if (!created->value.bits[bit])
			return -1;
	}
	*symbol = &created->value;
	return 0;
}

static int configuration_cast(const struct orlix_tcti_feature_model *model,
	uint32_t index, uint32_t *dot_atom, enum numeric_kind *kind)
{
	const struct orlix_tcti_feature_node *node;
	uint32_t child;

	if (!configuration_scalar(model, index))
		return -1;
	node = &model->nodes[index];
	if (feature_child(model, index, 0, &child))
		return -1;
	*dot_atom = child;
	*kind = node->kind == ORLIX_TCTI_FEATURE_UINT ?
		NUMERIC_UNSIGNED : NUMERIC_SIGNED;
	return 0;
}

static int symbol_comparison(struct compiler *compiler,
	enum orlix_tcti_feature_node_kind operation, struct field_symbol *symbol,
	enum numeric_kind kind, const struct numeric_value *constant,
	int reversed, int *literal)
{
	unsigned int bit, width;
	int equal, less, greater, result;

	if (!constant || constant->kind != kind || !constant->width)
		return -1;
	width = symbol->width > constant->width ? symbol->width : constant->width;
	if (boolean_constant(compiler, 1, &equal) ||
		boolean_constant(compiler, 0, &less) ||
		boolean_constant(compiler, 0, &greater))
		return -1;
	for (bit = width; bit-- > 0U; ) {
		int field_bit = bit < symbol->width ? symbol->bits[bit] :
			(kind == NUMERIC_SIGNED ? symbol->bits[symbol->width - 1U] : 0);
		int constant_bit = numeric_bit(constant, bit);
		int bit_equal, term, next;

		if (kind == NUMERIC_SIGNED && bit == width - 1U) {
			field_bit = field_bit ? -field_bit : 0;
			constant_bit = !constant_bit;
		}
		if (!field_bit) {
			if (constant_bit) {
				next = fresh(compiler);
				if (!next || equivalent_or(compiler, next, less, equal))
					return -1;
				less = next;
			}
			if (constant_bit && boolean_constant(compiler, 0, &next))
				return -1;
			if (constant_bit)
				equal = next;
			continue;
		}
		bit_equal = constant_bit ? field_bit : -field_bit;
		if (constant_bit) {
			term = fresh(compiler);
			next = fresh(compiler);
			if (!term || !next || equivalent_and(compiler, term, equal, -field_bit) ||
				equivalent_or(compiler, next, less, term))
				return -1;
			less = next;
		} else {
			term = fresh(compiler);
			next = fresh(compiler);
			if (!term || !next || equivalent_and(compiler, term, equal, field_bit) ||
				equivalent_or(compiler, next, greater, term))
				return -1;
			greater = next;
		}
		next = fresh(compiler);
		if (!next || equivalent_and(compiler, next, equal, bit_equal))
			return -1;
		equal = next;
	}
	if (reversed) {
		int swap = less;

		less = greater;
		greater = swap;
	}
	if (operation == ORLIX_TCTI_FEATURE_EQ)
		result = equal;
	else if (operation == ORLIX_TCTI_FEATURE_NE) {
		result = fresh(compiler);
		if (!result || equivalent_not(compiler, result, equal))
			return -1;
	} else if (operation == ORLIX_TCTI_FEATURE_LT)
		result = less;
	else if (operation == ORLIX_TCTI_FEATURE_GT)
		result = greater;
	else {
		result = fresh(compiler);
		if (!result || equivalent_not(compiler, result, less))
			return -1;
	}
	if (!result)
		return -1;
	*literal = result;
	return 0;
}

static int combine_literal(struct compiler *compiler, int left, int right,
			   int conjunction, int *result)
{
	int combined;

	if (!left) {
		*result = right;
		return 0;
	}
	if (!right) {
		*result = left;
		return 0;
	}
	combined = fresh(compiler);
	if (!combined || (conjunction ?
		equivalent_and(compiler, combined, left, right) :
		equivalent_or(compiler, combined, left, right)))
		return -1;
	*result = combined;
	return 0;
}

static int field_domain_error(
	struct compiler *compiler,
	const struct orlix_tcti_feature_field_domain_binding *binding,
	enum orlix_tcti_target_feature_sat_error_code code)
{
	fail(compiler->error, code, 0, binding->feature_provenance);
	return -1;
}

static int field_domain_expression_child(
	const struct orlix_tcti_feature_field_domain_bindings *bindings,
	const struct orlix_tcti_feature_field_domain_alternative *alternative,
	const struct orlix_tcti_feature_field_domain_expression *expression,
	uint32_t ordinal, uint32_t *child)
{
	size_t offset;

	if (ordinal >= expression->child_count ||
	    expression->first_child > alternative->expression_child_count ||
	    expression->child_count > alternative->expression_child_count -
		expression->first_child)
		return -1;
	offset = (size_t)alternative->first_expression_child +
		expression->first_child + ordinal;
	if (offset >= bindings->expression_child_count ||
	    bindings->expression_children[offset] >= alternative->expression_count)
		return -1;
	*child = bindings->expression_children[offset];
	return 0;
}

static int field_domain_expression(
	struct compiler *compiler,
	const struct orlix_tcti_feature_field_domain_binding *binding,
	const struct orlix_tcti_feature_field_domain_alternative *alternative,
	uint32_t local_index, size_t depth, int *literal)
{
	const struct orlix_tcti_feature_field_domain_bindings *bindings =
		compiler->field_domains;
	const struct orlix_tcti_feature_field_domain_expression *expression;
	uint32_t left_index, right_index;
	int left, right, parameter;

	if (local_index == UINT32_MAX)
		return boolean_constant(compiler, 1, literal);
	if (depth > 256U || local_index >= alternative->expression_count ||
	    alternative->first_expression > bindings->expression_count ||
	    alternative->expression_count > bindings->expression_count -
		alternative->first_expression)
		return field_domain_error(compiler, binding,
			ORLIX_TCTI_TARGET_FEATURE_SAT_MALFORMED_MODEL);
	expression = &bindings->expressions[alternative->first_expression + local_index];
	if (!expression->type)
		return field_domain_error(compiler, binding,
			ORLIX_TCTI_TARGET_FEATURE_SAT_MALFORMED_MODEL);
	if (!strcmp(expression->type, "AST.Bool"))
		return boolean_constant(compiler, expression->boolean != 0U, literal);
	if (!strcmp(expression->type, "AST.Identifier")) {
		parameter = parameter_index(compiler->model, expression->value);
		if (parameter < 0)
			return field_domain_error(compiler, binding,
				ORLIX_TCTI_TARGET_FEATURE_SAT_UNSUPPORTED_SEMANTICS);
		*literal = parameter + 1;
		return 0;
	}
	if (!strcmp(expression->type, "AST.Function")) {
		if (!expression->name || strcmp(expression->name, "IsFeatureImplemented") ||
		    expression->child_count != 1U ||
		    field_domain_expression_child(bindings, alternative, expression, 0U,
					  &left_index))
			return field_domain_error(compiler, binding,
				ORLIX_TCTI_TARGET_FEATURE_SAT_UNSUPPORTED_SEMANTICS);
		return field_domain_expression(compiler, binding, alternative, left_index,
					       depth + 1U, literal);
	}
	if (!strcmp(expression->type, "AST.BinaryOp")) {
		if (!expression->op || expression->child_count != 2U ||
		    field_domain_expression_child(bindings, alternative, expression, 0U,
					  &left_index) ||
		    field_domain_expression_child(bindings, alternative, expression, 1U,
					  &right_index) ||
		    field_domain_expression(compiler, binding, alternative, left_index,
					    depth + 1U, &left) ||
		    field_domain_expression(compiler, binding, alternative, right_index,
					    depth + 1U, &right))
			return -1;
		if (!strcmp(expression->op, "&&"))
			return combine_literal(compiler, left, right, 1, literal);
		if (!strcmp(expression->op, "||"))
			return combine_literal(compiler, left, right, 0, literal);
		if (!strcmp(expression->op, "==")) {
			int result = fresh(compiler);
			if (!result || equivalent_iff(compiler, result, left, right))
				return -1;
			*literal = result;
			return 0;
		}
		if (!strcmp(expression->op, "!=")) {
			int equal = fresh(compiler), result = fresh(compiler);
			if (!equal || !result || equivalent_iff(compiler, equal, left, right) ||
			    equivalent_not(compiler, result, equal))
				return -1;
			*literal = result;
			return 0;
		}
	}
	return field_domain_error(compiler, binding,
		ORLIX_TCTI_TARGET_FEATURE_SAT_UNSUPPORTED_SEMANTICS);
}

static int field_domain_value(
	struct compiler *compiler,
	const struct orlix_tcti_feature_field_domain_binding *binding,
	const struct orlix_tcti_feature_field_domain_alternative *alternative,
	struct field_symbol *symbol, uint32_t local_index, size_t depth,
	int *literal)
{
	const struct orlix_tcti_feature_field_domain_bindings *bindings =
		compiler->field_domains;
	const struct orlix_tcti_feature_field_domain_node *domain;
	struct numeric_value value, end;
	uint32_t child;
	int result = 0, member, condition;
	size_t index;

	if (depth > 256U || local_index >= alternative->domain_count ||
	    alternative->first_domain > bindings->domain_count ||
	    alternative->domain_count > bindings->domain_count -
		alternative->first_domain)
		return field_domain_error(compiler, binding,
			ORLIX_TCTI_TARGET_FEATURE_SAT_MALFORMED_MODEL);
	domain = &bindings->domains[alternative->first_domain + local_index];
	if (!domain->type)
		return field_domain_error(compiler, binding,
			ORLIX_TCTI_TARGET_FEATURE_SAT_MALFORMED_MODEL);
	if (!strcmp(domain->type, "Values.Value")) {
		if (parse_bit_string(domain->value, &value) || value.width > symbol->width)
			return field_domain_error(compiler, binding,
				ORLIX_TCTI_TARGET_FEATURE_SAT_UNSUPPORTED_SEMANTICS);
		value.kind = NUMERIC_UNSIGNED;
		if (symbol_comparison(compiler, ORLIX_TCTI_FEATURE_EQ, symbol,
				      NUMERIC_UNSIGNED, &value, 0, &result))
			return -1;
	} else if (!strcmp(domain->type, "Values.ValueRange")) {
		int lower, upper;

		if (parse_bit_string(domain->start, &value) ||
		    parse_bit_string(domain->end, &end) ||
		    value.width > symbol->width || end.width > symbol->width)
			return field_domain_error(compiler, binding,
				ORLIX_TCTI_TARGET_FEATURE_SAT_UNSUPPORTED_SEMANTICS);
		value.kind = end.kind = NUMERIC_UNSIGNED;
		if (symbol_comparison(compiler, ORLIX_TCTI_FEATURE_GE, symbol,
				      NUMERIC_UNSIGNED, &value, 0, &lower) ||
		    symbol_comparison(compiler, ORLIX_TCTI_FEATURE_GE, symbol,
				      NUMERIC_UNSIGNED, &end, 1, &upper) ||
		    combine_literal(compiler, lower, upper, 1, &result))
			return -1;
	} else if (!strcmp(domain->type, "Values.ConditionalValue") ||
		   !strcmp(domain->type, "Values.Group")) {
		if (domain->first_child_domain > alternative->domain_count ||
		    domain->child_domain_count > alternative->domain_count -
			domain->first_child_domain)
			return field_domain_error(compiler, binding,
				ORLIX_TCTI_TARGET_FEATURE_SAT_MALFORMED_MODEL);
		for (index = 0; index < domain->child_domain_count; index++) {
			child = domain->first_child_domain + (uint32_t)index;
			if (field_domain_value(compiler, binding, alternative, symbol,
					       child, depth + 1U, &member) ||
			    combine_literal(compiler, result, member, 0, &result))
				return -1;
		}
		if (!result && boolean_constant(compiler, 0, &result))
			return -1;
	} else if (!strcmp(domain->type, "Values.ImplementationDefined")) {
		if (boolean_constant(compiler, 1, &result))
			return -1;
	} else {
		return field_domain_error(compiler, binding,
			ORLIX_TCTI_TARGET_FEATURE_SAT_UNSUPPORTED_SEMANTICS);
	}
	if (domain->condition_expression != UINT32_MAX) {
		if (field_domain_expression(compiler, binding, alternative,
				domain->condition_expression, 0U, &condition) ||
		    combine_literal(compiler, result, condition, 1, &result))
			return -1;
	}
	*literal = result;
	return 0;
}

static int field_domain_slice(
	struct compiler *compiler,
	const struct orlix_tcti_feature_field_domain_binding *binding,
	const struct orlix_tcti_feature_field_domain_alternative *alternative,
	struct field_symbol *symbol, uint32_t first, uint32_t count, int *literal)
{
	uint32_t index;
	int result = 0, member;

	if (first > alternative->domain_count ||
	    count > alternative->domain_count - first)
		return field_domain_error(compiler, binding,
			ORLIX_TCTI_TARGET_FEATURE_SAT_MALFORMED_MODEL);
	for (index = 0; index < count; index++)
		if (field_domain_value(compiler, binding, alternative, symbol,
				       first + index, 0U, &member) ||
		    combine_literal(compiler, result, member, 0, &result))
			return -1;
	if (!result && boolean_constant(compiler, 0, &result))
		return -1;
	*literal = result;
	return 0;
}

static int field_domain_alternative(
	struct compiler *compiler,
	const struct orlix_tcti_feature_field_domain_binding *binding,
	const struct orlix_tcti_feature_field_domain_alternative *alternative,
	struct field_symbol *symbol, int *literal)
{
	const struct orlix_tcti_feature_field_domain_bindings *bindings =
		compiler->field_domains;
	const uint32_t conditions[] = {
		alternative->field_condition_expression,
		alternative->wrapper_condition_expression,
		alternative->fieldset_condition_expression,
		alternative->relation_field_condition_expression,
		alternative->relation_wrapper_condition_expression,
	};
	uint32_t index;
	int guard = 0, values = 0, constraints = 0, member, allowed;

	for (index = 0; index < sizeof(conditions) / sizeof(conditions[0]); index++) {
		if (conditions[index] == UINT32_MAX)
			continue;
		if (field_domain_expression(compiler, binding, alternative,
					    conditions[index], 0U, &member) ||
		    combine_literal(compiler, guard, member, 1, &guard))
			return -1;
	}
	if (!guard && boolean_constant(compiler, 1, &guard))
		return -1;
	if (alternative->first_value_candidate > bindings->value_candidate_count ||
	    alternative->value_candidate_count > bindings->value_candidate_count -
		alternative->first_value_candidate ||
	    alternative->first_constraint_candidate >
		bindings->constraint_candidate_count ||
	    alternative->constraint_candidate_count >
		bindings->constraint_candidate_count -
		alternative->first_constraint_candidate)
		return field_domain_error(compiler, binding,
			ORLIX_TCTI_TARGET_FEATURE_SAT_MALFORMED_MODEL);
	for (index = 0; index < alternative->value_candidate_count; index++) {
		const struct orlix_tcti_feature_field_domain_value_candidate *candidate =
			&bindings->value_candidates[
				alternative->first_value_candidate + index];

		if (candidate->kind == ORLIX_TCTI_REGISTER_FIELD_VALUE_SOURCE_NONE)
			return field_domain_error(compiler, binding,
				ORLIX_TCTI_TARGET_FEATURE_SAT_UNSUPPORTED_SEMANTICS);
		if (!candidate->domain_count) {
			if (candidate->kind !=
			    ORLIX_TCTI_REGISTER_FIELD_VALUE_SOURCE_IMPLEMENTATION_DEFINED ||
			    boolean_constant(compiler, 1, &member))
				return -1;
		} else if (field_domain_slice(compiler, binding, alternative, symbol,
					      candidate->first_domain,
					      candidate->domain_count, &member)) {
			return -1;
		}
		if (combine_literal(compiler, values, member, 0, &values))
			return -1;
	}
	if (!values && boolean_constant(compiler, 1, &values))
		return -1;
	for (index = 0; index < alternative->constraint_candidate_count; index++) {
		const struct orlix_tcti_feature_field_domain_constraint_candidate *candidate =
			&bindings->constraint_candidates[
				alternative->first_constraint_candidate + index];

		if (!candidate->domain_count) {
			if (candidate->kind != ORLIX_TCTI_REGISTER_CONSTRAINT_NULL ||
			    boolean_constant(compiler, 1, &member))
				return field_domain_error(compiler, binding,
					ORLIX_TCTI_TARGET_FEATURE_SAT_UNSUPPORTED_SEMANTICS);
		} else if (field_domain_slice(compiler, binding, alternative, symbol,
					      candidate->first_domain,
					      candidate->domain_count, &member)) {
			return -1;
		}
		if (combine_literal(compiler, constraints, member, 1, &constraints))
			return -1;
	}
	if (!constraints && boolean_constant(compiler, 1, &constraints))
		return -1;
	if (combine_literal(compiler, values, constraints, 1, &allowed))
		return -1;
	*literal = fresh(compiler);
	if (!*literal || equivalent_or(compiler, *literal, -guard, allowed))
		return -1;
	return 0;
}

static int field_domain_constrain(
	struct compiler *compiler,
	const struct orlix_tcti_feature_field_domain_binding *binding,
	struct field_symbol *symbol)
{
	const struct orlix_tcti_feature_field_domain_bindings *bindings =
		compiler->field_domains;
	uint32_t index;
	int domain = 0, alternative;

	if (binding->first_alternative > bindings->alternative_count ||
	    binding->alternative_count > bindings->alternative_count -
		binding->first_alternative)
		return field_domain_error(compiler, binding,
			ORLIX_TCTI_TARGET_FEATURE_SAT_MALFORMED_MODEL);
	for (index = 0; index < binding->alternative_count; index++) {
		if (field_domain_alternative(compiler, binding,
				&bindings->alternatives[binding->first_alternative + index],
				symbol, &alternative) ||
		    combine_literal(compiler, domain, alternative, 1, &domain))
			return -1;
	}
	if (!domain)
		return field_domain_error(compiler, binding,
			ORLIX_TCTI_TARGET_FEATURE_SAT_MALFORMED_MODEL);
	return cnf_clause(compiler, &domain, 1U);
}

static int field_comparison(struct compiler *compiler,
	enum orlix_tcti_feature_node_kind operation, uint32_t field_index,
	enum numeric_kind kind, const struct numeric_value *constant,
	int reversed, int *literal)
{
	const struct orlix_tcti_feature_field_domain_binding *binding =
		field_binding(compiler, field_index);
	struct field_symbol *symbol;

	if (!binding || field_symbol_get(compiler, binding, &symbol))
		return -1;
	return symbol_comparison(compiler, operation, symbol, kind, constant,
		reversed, literal);
}

static int feature_membership(struct compiler *compiler,
	uint32_t scalar_index, uint32_t set_index, int *literal)
{
	struct field_symbol *symbol;
	const struct orlix_tcti_feature_field_domain_binding *binding;
	uint32_t dot_atom, field_index, item, child;
	enum numeric_kind kind;
	int result;

	if (set_index >= compiler->model->node_count ||
		compiler->model->nodes[set_index].kind != ORLIX_TCTI_FEATURE_SET ||
		boolean_constant(compiler, 0, &result))
		return -1;
	if (!configuration_cast(compiler->model, scalar_index, &dot_atom, &kind)) {
		if (configuration_symbol_get(compiler, dot_atom, &symbol))
			return -1;
	} else if (!field_cast(compiler->model, scalar_index, &field_index, &kind)) {
		binding = field_binding(compiler, field_index);
		if (!binding || field_symbol_get(compiler, binding, &symbol))
			return -1;
	} else if (scalar_index < compiler->model->node_count &&
		compiler->model->nodes[scalar_index].kind == ORLIX_TCTI_FEATURE_FIELD) {
		kind = NUMERIC_UNSIGNED;
		binding = field_binding(compiler, scalar_index);
		if (!binding || field_symbol_get(compiler, binding, &symbol))
			return -1;
	} else {
		return -1;
	}
	for (item = 0; item < compiler->model->nodes[set_index].child_count; item++) {
		struct numeric_value value;
		int member, next;

		if (feature_child(compiler->model, set_index, item, &child) ||
			numeric_node(compiler->model, child, &value))
			return -1;
		value.kind = kind;
		if (symbol_comparison(compiler, ORLIX_TCTI_FEATURE_EQ, symbol, kind,
			&value, 0, &member))
			return -1;
		next = fresh(compiler);
		if (!next || equivalent_or(compiler, next, result, member))
			return -1;
		result = next;
	}
	*literal = result;
	return 0;
}

static int index_in_range(uint32_t index, size_t count)
{
	return index < count;
}

static int range_in_range(uint32_t first, uint32_t count, size_t total)
{
	return first <= total && count <= total - first;
}

static int feature_child(const struct orlix_tcti_feature_model *model,
		uint32_t index, uint32_t child, uint32_t *result)
{
	const struct orlix_tcti_feature_node *node = &model->nodes[index];

	switch (node->kind) {
	case ORLIX_TCTI_FEATURE_NOT:
		if (child) return -1;
		*result = node->left;
		return 0;
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
		if (child > 1U) return -1;
		*result = child ? node->right : node->left;
		return 0;
	case ORLIX_TCTI_FEATURE_DOT_ATOM:
	case ORLIX_TCTI_FEATURE_SET:
	case ORLIX_TCTI_FEATURE_UINT:
	case ORLIX_TCTI_FEATURE_SINT:
		if (child >= node->child_count) return -1;
		*result = model->children[node->first_child + child];
		return 0;
	default:
		return -1;
	}
}

static int feature_child_count(const struct orlix_tcti_feature_model *model,
		uint32_t index, uint32_t *count)
{
	const struct orlix_tcti_feature_node *node = &model->nodes[index];

	switch (node->kind) {
	case ORLIX_TCTI_FEATURE_BOOL:
	case ORLIX_TCTI_FEATURE_IDENTIFIER:
	case ORLIX_TCTI_FEATURE_INTEGER:
	case ORLIX_TCTI_FEATURE_VALUE:
	case ORLIX_TCTI_FEATURE_FIELD:
		if ((node->kind == ORLIX_TCTI_FEATURE_IDENTIFIER ||
		     node->kind == ORLIX_TCTI_FEATURE_VALUE) && !node->text)
			return -1;
		if (node->kind == ORLIX_TCTI_FEATURE_FIELD && (!node->field.state ||
		     !node->field.register_name || !node->field.selector))
			return -1;
		*count = 0;
		return 0;
	case ORLIX_TCTI_FEATURE_NOT:
		*count = 1;
		return 0;
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
		*count = 2;
		return 0;
	case ORLIX_TCTI_FEATURE_DOT_ATOM:
	case ORLIX_TCTI_FEATURE_SET:
	case ORLIX_TCTI_FEATURE_UINT:
	case ORLIX_TCTI_FEATURE_SINT:
		if (!range_in_range(node->first_child, node->child_count,
				      model->child_count))
			return -1;
		*count = node->child_count;
		return 0;
	default:
		return -1;
	}
}

static int validate_feature_node(const struct orlix_tcti_feature_model *model,
		uint32_t index, unsigned char *color, size_t depth,
		struct orlix_tcti_target_feature_sat_error *error)
{
	uint32_t count, child, next;

	if (!index_in_range(index, model->node_count) || depth > model->node_count ||
	    color[index] == 1U) {
		fail(error, ORLIX_TCTI_TARGET_FEATURE_SAT_MALFORMED_MODEL, 0,
			index_in_range(index, model->node_count) ?
			model->nodes[index].provenance : (struct orlix_tcti_feature_provenance) { 0 });
		return -1;
	}
	if (color[index] == 2U)
		return 0;
	if (feature_child_count(model, index, &count)) {
		fail(error, ORLIX_TCTI_TARGET_FEATURE_SAT_MALFORMED_MODEL, 0,
			model->nodes[index].provenance);
		return -1;
	}
	color[index] = 1U;
	for (child = 0; child < count; child++) {
		if (feature_child(model, index, child, &next) ||
		    validate_feature_node(model, next, color, depth + 1U, error))
			return -1;
	}
	color[index] = 2U;
	return 0;
}

static int target_child(const struct orlix_tcti_target_inventory *inventory,
		uint32_t index, uint32_t child, uint32_t *result)
{
	const struct orlix_tcti_target_expr *expression = &inventory->expressions[index];

	switch (expression->kind) {
	case ORLIX_TCTI_TARGET_EXPR_NOT:
		if (child) return -1;
		*result = expression->left;
		return 0;
	case ORLIX_TCTI_TARGET_EXPR_AND:
	case ORLIX_TCTI_TARGET_EXPR_OR:
	case ORLIX_TCTI_TARGET_EXPR_EQ:
	case ORLIX_TCTI_TARGET_EXPR_NE:
	case ORLIX_TCTI_TARGET_EXPR_IN:
		if (child > 1U) return -1;
		*result = child ? expression->right : expression->left;
		return 0;
	case ORLIX_TCTI_TARGET_EXPR_SET:
		if (child >= expression->item_count) return -1;
		*result = inventory->set_items[expression->first_item + child];
		return 0;
	default:
		return -1;
	}
}

static int target_child_count(const struct orlix_tcti_target_inventory *inventory,
		uint32_t index, uint32_t *count)
{
	const struct orlix_tcti_target_expr *expression = &inventory->expressions[index];

	switch (expression->kind) {
	case ORLIX_TCTI_TARGET_EXPR_BOOL:
	case ORLIX_TCTI_TARGET_EXPR_FEATURE:
	case ORLIX_TCTI_TARGET_EXPR_OPERAND:
	case ORLIX_TCTI_TARGET_EXPR_VALUE:
		if (expression->kind != ORLIX_TCTI_TARGET_EXPR_BOOL && !expression->text)
			return -1;
		*count = 0;
		return 0;
	case ORLIX_TCTI_TARGET_EXPR_NOT:
		*count = 1;
		return 0;
	case ORLIX_TCTI_TARGET_EXPR_AND:
	case ORLIX_TCTI_TARGET_EXPR_OR:
	case ORLIX_TCTI_TARGET_EXPR_EQ:
	case ORLIX_TCTI_TARGET_EXPR_NE:
	case ORLIX_TCTI_TARGET_EXPR_IN:
		*count = 2;
		return 0;
	case ORLIX_TCTI_TARGET_EXPR_SET:
		if (!range_in_range(expression->first_item, expression->item_count,
				      inventory->set_item_count))
			return -1;
		*count = expression->item_count;
		return 0;
	default:
		return -1;
	}
}

static int validate_target_node(const struct orlix_tcti_target_inventory *inventory,
		uint32_t index, unsigned char *color, size_t depth)
{
	uint32_t count, child, next;

	if (!index_in_range(index, inventory->expression_count) ||
	    depth > inventory->expression_count || color[index] == 1U ||
	    target_child_count(inventory, index, &count))
		return -1;
	if (color[index] == 2U)
		return 0;
	color[index] = 1U;
	for (child = 0; child < count; child++)
		if (target_child(inventory, index, child, &next) ||
		    validate_target_node(inventory, next, color, depth + 1U))
			return -1;
	color[index] = 2U;
	return 0;
}

static int feature_semantics_supported(enum orlix_tcti_feature_node_kind kind)
{
	switch (kind) {
	case ORLIX_TCTI_FEATURE_BOOL:
	case ORLIX_TCTI_FEATURE_IDENTIFIER:
	case ORLIX_TCTI_FEATURE_NOT:
	case ORLIX_TCTI_FEATURE_AND:
	case ORLIX_TCTI_FEATURE_OR:
	case ORLIX_TCTI_FEATURE_EQ:
	case ORLIX_TCTI_FEATURE_NE:
	case ORLIX_TCTI_FEATURE_LT:
	case ORLIX_TCTI_FEATURE_GT:
	case ORLIX_TCTI_FEATURE_GE:
	case ORLIX_TCTI_FEATURE_INTEGER:
	case ORLIX_TCTI_FEATURE_VALUE:
	case ORLIX_TCTI_FEATURE_UINT:
	case ORLIX_TCTI_FEATURE_SINT:
	case ORLIX_TCTI_FEATURE_FIELD:
	case ORLIX_TCTI_FEATURE_DOT_ATOM:
	case ORLIX_TCTI_FEATURE_SET:
	case ORLIX_TCTI_FEATURE_IN:
	case ORLIX_TCTI_FEATURE_IMPLIES:
	case ORLIX_TCTI_FEATURE_IFF:
		return 1;
	default:
		return 0;
	}
}

static int target_semantics_supported(enum orlix_tcti_target_expr_kind kind)
{
	switch (kind) {
	case ORLIX_TCTI_TARGET_EXPR_BOOL:
	case ORLIX_TCTI_TARGET_EXPR_FEATURE:
	case ORLIX_TCTI_TARGET_EXPR_OPERAND:
	case ORLIX_TCTI_TARGET_EXPR_VALUE:
	case ORLIX_TCTI_TARGET_EXPR_SET:
	case ORLIX_TCTI_TARGET_EXPR_NOT:
	case ORLIX_TCTI_TARGET_EXPR_AND:
	case ORLIX_TCTI_TARGET_EXPR_OR:
	case ORLIX_TCTI_TARGET_EXPR_EQ:
	case ORLIX_TCTI_TARGET_EXPR_NE:
	case ORLIX_TCTI_TARGET_EXPR_IN:
		return 1;
	default:
		return 0;
	}
}

static int validate_input_storage(struct compiler *compiler)
{
	const struct orlix_tcti_feature_model *model = compiler->model;
	const struct orlix_tcti_target_inventory *inventory = compiler->inventory;
	size_t index, other;

	if ((model->parameter_count && !model->parameters) ||
	    (model->constraint_count && !model->constraints) ||
	    (model->node_count && !model->nodes) ||
	    (model->child_count && !model->children) ||
	    (inventory->leaf_count && !inventory->leaves) ||
	    (inventory->expression_count && !inventory->expressions) ||
	    (inventory->set_item_count && !inventory->set_items) ||
	    (inventory->operand_count && !inventory->operands) ||
	    (inventory->fixed_operand_count && !inventory->fixed_operands) ||
	    (inventory->condition_operand_count && !inventory->condition_operands))
		return -1;
	for (index = 0; index < model->parameter_count; index++) {
		if (!model->parameters[index].name)
			return -1;
		for (other = 0; other < index; other++)
			if (!strcmp(model->parameters[index].name,
				    model->parameters[other].name))
				return -1;
	}
	for (index = 0; index < model->constraint_count; index++)
		if (!index_in_range(model->constraints[index], model->node_count))
			return -1;
	for (index = 0; index < inventory->leaf_count; index++)
		if (!index_in_range(inventory->leaves[index].condition,
				    inventory->expression_count))
			return -1;
	return 0;
}

static int validate_input_graphs(struct compiler *compiler)
{
	unsigned char *feature_color = NULL, *target_color = NULL;
	size_t index;
	int status = -1;

	if (validate_input_storage(compiler)) {
		fail(compiler->error, ORLIX_TCTI_TARGET_FEATURE_SAT_MALFORMED_MODEL, 0,
			(struct orlix_tcti_feature_provenance) { 0 });
		goto out;
	}
	if (compiler->model->node_count)
		feature_color = budget_calloc(&compiler->budget,
			compiler->model->node_count, sizeof(*feature_color));
	if (compiler->inventory->expression_count)
		target_color = budget_calloc(&compiler->budget,
			compiler->inventory->expression_count, sizeof(*target_color));
	if ((compiler->model->node_count && !feature_color) ||
	    (compiler->inventory->expression_count && !target_color))
		goto out;
	for (index = 0; index < compiler->model->node_count; index++)
		if (validate_feature_node(compiler->model, (uint32_t)index,
				feature_color, 0, compiler->error))
			goto out;
	for (index = 0; index < compiler->inventory->expression_count; index++)
		if (validate_target_node(compiler->inventory, (uint32_t)index,
			target_color, 0)) {
			fail(compiler->error, ORLIX_TCTI_TARGET_FEATURE_SAT_MALFORMED_MODEL, 0,
				(struct orlix_tcti_feature_provenance) { 0 });
			goto out;
		}
	for (index = 0; index < compiler->model->node_count; index++)
		if (!feature_semantics_supported(compiler->model->nodes[index].kind)) {
			fail(compiler->error, ORLIX_TCTI_TARGET_FEATURE_SAT_UNSUPPORTED_SEMANTICS,
				0, compiler->model->nodes[index].provenance);
			goto out;
		}
	for (index = 0; index < compiler->inventory->expression_count; index++)
		if (!target_semantics_supported(compiler->inventory->expressions[index].kind)) {
			fail(compiler->error, ORLIX_TCTI_TARGET_FEATURE_SAT_UNSUPPORTED_SEMANTICS,
				0, (struct orlix_tcti_feature_provenance) { 0 });
			goto out;
		}
	status = 0;
out:
	if (target_color)
		discharge(&compiler->budget, compiler->inventory->expression_count,
			sizeof(*target_color));
	if (feature_color)
		discharge(&compiler->budget, compiler->model->node_count,
			sizeof(*feature_color));
	free(target_color);
	free(feature_color);
	return status;
}

static int feature_boolean(struct compiler *compiler, uint32_t index, int *literal)
{
	const struct orlix_tcti_feature_node *node = NULL;
	int left, right, result, parameter;
	const int unit_true[] = { 1 };
	const int unit_false[] = { -1 };

	if (index >= compiler->model->node_count)
		goto malformed;
	if (compiler->feature_literals[index]) {
		*literal = compiler->feature_literals[index];
		return 0;
	}
	node = &compiler->model->nodes[index];
	switch (node->kind) {
	case ORLIX_TCTI_FEATURE_BOOL:
		result = fresh(compiler);
		if (!result || cnf_clause(compiler, node->integer ? unit_true : unit_false, 1))
			goto memory;
		/* The first created variable is not necessarily 1. */
		compiler->cnf.clauses[compiler->cnf.count - 1U].literals[0] = node->integer ? result : -result;
		break;
	case ORLIX_TCTI_FEATURE_IDENTIFIER:
		parameter = parameter_index(compiler->model, node->text);
		if (parameter >= 0) {
			result = parameter + 1;
		} else {
			size_t candidate;

			result = 0;
			for (candidate = 0; candidate < compiler->model->node_count;
				candidate++)
				if (candidate != index &&
					compiler->feature_literals[candidate] &&
					compiler->model->nodes[candidate].kind ==
						ORLIX_TCTI_FEATURE_IDENTIFIER &&
					compiler->model->nodes[candidate].text && node->text &&
					!strcmp(compiler->model->nodes[candidate].text, node->text)) {
					result = compiler->feature_literals[candidate];
					break;
				}
			if (!result)
				result = fresh(compiler);
			if (!result)
				goto memory;
		}
		break;
	case ORLIX_TCTI_FEATURE_NOT:
		if (feature_boolean(compiler, node->left, &left)) return -1;
		result = fresh(compiler);
		if (!result || equivalent_not(compiler, result, left)) goto memory;
		break;
	case ORLIX_TCTI_FEATURE_AND:
	case ORLIX_TCTI_FEATURE_OR:
	case ORLIX_TCTI_FEATURE_IMPLIES:
	case ORLIX_TCTI_FEATURE_IFF:
	case ORLIX_TCTI_FEATURE_EQ:
	case ORLIX_TCTI_FEATURE_NE:
	case ORLIX_TCTI_FEATURE_LT:
	case ORLIX_TCTI_FEATURE_GT:
	case ORLIX_TCTI_FEATURE_GE: {
		struct numeric_value left_numeric, right_numeric;
		struct field_symbol *configuration;
		uint32_t field_index, dot_atom;
		enum numeric_kind field_kind;
		int order, comparison;

		if (node->kind == ORLIX_TCTI_FEATURE_AND ||
			node->kind == ORLIX_TCTI_FEATURE_OR ||
			node->kind == ORLIX_TCTI_FEATURE_IMPLIES ||
			node->kind == ORLIX_TCTI_FEATURE_IFF)
			goto boolean_relation;
		if (!configuration_cast(compiler->model, node->left, &dot_atom,
				&field_kind) &&
			!numeric_node(compiler->model, node->right, &right_numeric)) {
			right_numeric.kind = field_kind;
			if (configuration_symbol_get(compiler, dot_atom, &configuration) ||
				symbol_comparison(compiler, node->kind, configuration, field_kind,
					&right_numeric, 0, &result))
				goto unsupported;
			break;
		}
		if (!numeric_node(compiler->model, node->left, &left_numeric) &&
			!configuration_cast(compiler->model, node->right, &dot_atom,
				&field_kind)) {
			left_numeric.kind = field_kind;
			if (configuration_symbol_get(compiler, dot_atom, &configuration) ||
				symbol_comparison(compiler, node->kind, configuration, field_kind,
					&left_numeric, 1, &result))
				goto unsupported;
			break;
		}
		if (!numeric_node(compiler->model, node->left, &left_numeric) &&
			!numeric_node(compiler->model, node->right, &right_numeric)) {
			if (compare_numeric(&left_numeric, &right_numeric, &order))
				goto unsupported;
			comparison = node->kind == ORLIX_TCTI_FEATURE_EQ ? order == 0 :
				node->kind == ORLIX_TCTI_FEATURE_NE ? order != 0 :
				node->kind == ORLIX_TCTI_FEATURE_LT ? order < 0 :
				node->kind == ORLIX_TCTI_FEATURE_GT ? order > 0 : order >= 0;
			if (boolean_constant(compiler, comparison, &result))
				goto memory;
			break;
		}
		if (!field_cast(compiler->model, node->left, &field_index, &field_kind) &&
			!numeric_node(compiler->model, node->right, &right_numeric)) {
			right_numeric.kind = field_kind;
			if (field_comparison(compiler, node->kind, field_index, field_kind,
				&right_numeric, 0, &result))
				goto unsupported;
			break;
		}
		if (node->left < compiler->model->node_count &&
			compiler->model->nodes[node->left].kind == ORLIX_TCTI_FEATURE_FIELD &&
			!numeric_node(compiler->model, node->right, &right_numeric)) {
			right_numeric.kind = NUMERIC_UNSIGNED;
			if (field_comparison(compiler, node->kind, node->left,
				NUMERIC_UNSIGNED, &right_numeric, 0, &result))
				goto unsupported;
			break;
		}
		if (!numeric_node(compiler->model, node->left, &left_numeric) &&
			!field_cast(compiler->model, node->right, &field_index, &field_kind)) {
			left_numeric.kind = field_kind;
			if (field_comparison(compiler, node->kind, field_index, field_kind,
				&left_numeric, 1, &result))
				goto unsupported;
			break;
		}
		if (!numeric_node(compiler->model, node->left, &left_numeric) &&
			node->right < compiler->model->node_count &&
			compiler->model->nodes[node->right].kind == ORLIX_TCTI_FEATURE_FIELD) {
			left_numeric.kind = NUMERIC_UNSIGNED;
			if (field_comparison(compiler, node->kind, node->right,
				NUMERIC_UNSIGNED, &left_numeric, 1, &result))
				goto unsupported;
			break;
		}
		if (node->kind != ORLIX_TCTI_FEATURE_EQ &&
			node->kind != ORLIX_TCTI_FEATURE_NE)
			goto unsupported;
	boolean_relation:
		if (feature_boolean(compiler, node->left, &left) ||
			feature_boolean(compiler, node->right, &right)) return -1;
		result = fresh(compiler);
		if (!result) goto memory;
		if (node->kind == ORLIX_TCTI_FEATURE_AND && equivalent_and(compiler, result, left, right)) goto memory;
		if (node->kind == ORLIX_TCTI_FEATURE_OR && equivalent_or(compiler, result, left, right)) goto memory;
		if (node->kind == ORLIX_TCTI_FEATURE_IMPLIES && equivalent_or(compiler, result, -left, right)) goto memory;
		if (node->kind == ORLIX_TCTI_FEATURE_IFF && equivalent_iff(compiler, result, left, right)) goto memory;
		if (node->kind == ORLIX_TCTI_FEATURE_EQ && equivalent_iff(compiler, result, left, right)) goto memory;
		if (node->kind == ORLIX_TCTI_FEATURE_NE) {
			int equal = fresh(compiler);
			if (!equal || equivalent_iff(compiler, equal, left, right) ||
			    equivalent_not(compiler, result, equal)) goto memory;
		}
		break;
	}
	case ORLIX_TCTI_FEATURE_IN:
		if (feature_membership(compiler, node->left, node->right, &result))
			goto unsupported;
		break;
	default:
	unsupported:
		fail(compiler->error, ORLIX_TCTI_TARGET_FEATURE_SAT_UNSUPPORTED_SEMANTICS, 0,
			node->provenance);
		return -1;
	}
	compiler->feature_literals[index] = result;
	*literal = result;
	return 0;
malformed:
	fail(compiler->error, ORLIX_TCTI_TARGET_FEATURE_SAT_MALFORMED_MODEL, 0,
		node ? node->provenance : (struct orlix_tcti_feature_provenance) { 0 });
	return -1;
memory:
	fail(compiler->error, ORLIX_TCTI_TARGET_FEATURE_SAT_NO_MEMORY, 0,
		node ? node->provenance : (struct orlix_tcti_feature_provenance) { 0 });
	return -1;
}

static int operand_symbol_get(struct compiler *compiler, const char *name,
	struct operand_symbol **symbol)
{
	struct operand_symbol *created;
	const struct orlix_tcti_target_leaf *leaf;
	size_t index;
	unsigned int bit;

	if (!name || compiler->current_leaf >= compiler->inventory->leaf_count)
		return -1;
	for (index = 0; index < compiler->operand_symbol_count; index++)
		if (compiler->operand_symbols[index].leaf_index == compiler->current_leaf &&
			!strcmp(compiler->operand_symbols[index].name, name)) {
			*symbol = &compiler->operand_symbols[index];
			return 0;
		}
	if (reserve(&compiler->budget, (void **)&compiler->operand_symbols,
		&compiler->operand_symbol_capacity, compiler->operand_symbol_count + 1U,
		sizeof(*compiler->operand_symbols)))
		return -1;
	created = &compiler->operand_symbols[compiler->operand_symbol_count++];
	*created = (struct operand_symbol) {
		.leaf_index = compiler->current_leaf,
		.name = name,
	};
	leaf = &compiler->inventory->leaves[compiler->current_leaf];
	for (index = 0; index < compiler->inventory->condition_operand_count; index++) {
		const struct orlix_tcti_target_condition_operand *operand =
			&compiler->inventory->condition_operands[index];

		if (operand->leaf_index != compiler->current_leaf ||
		    strcmp(operand->name, name))
			continue;
		created->width = operand->width;
		created->fixed_value = operand->fixed_value >> operand->start;
		created->bits = budget_calloc(&compiler->budget, created->width,
					     sizeof(*created->bits));
		if (!created->bits)
			return -1;
		for (bit = 0; bit < created->width; bit++) {
			uint32_t target_bit = UINT32_C(1) << (operand->start + bit);

			if (operand->variable_mask & target_bit) {
				created->bits[bit] = fresh(compiler);
				if (!created->bits[bit])
					return -1;
			} else {
				created->bits[bit] =
					(operand->fixed_value & target_bit) ? 1 : -1;
			}
		}
		*symbol = created;
		return 0;
	}
	for (index = 0; index < compiler->inventory->operand_count; index++) {
		const struct orlix_tcti_target_operand *operand =
			&compiler->inventory->operands[index];

		if (operand->leaf_index != compiler->current_leaf ||
			strcmp(operand->name, name))
			continue;
		created->width = operand->width;
		created->fixed_value = leaf->encoding_pattern >> operand->start;
		created->bits = budget_calloc(&compiler->budget, created->width,
			sizeof(*created->bits));
		if (!created->bits)
			return -1;
		for (bit = 0; bit < created->width; bit++)
			if (operand->variable_mask &
				(UINT32_C(1) << (operand->start + bit))) {
				created->bits[bit] = fresh(compiler);
				if (!created->bits[bit])
					return -1;
			}
		*symbol = created;
		return 0;
	}
	for (index = 0; index < compiler->inventory->fixed_operand_count; index++) {
		const struct orlix_tcti_target_fixed_operand *operand =
			&compiler->inventory->fixed_operands[index];

		if (operand->leaf_index != compiler->current_leaf ||
			strcmp(operand->name, name))
			continue;
		created->width = operand->width;
		created->fixed_value = operand->fixed_value >> operand->start;
		created->bits = budget_calloc(&compiler->budget, created->width,
			sizeof(*created->bits));
		if (!created->bits)
			return -1;
		*symbol = created;
		return 0;
	}
	return -1;
}

static int operand_value_equal(struct compiler *compiler,
	const struct orlix_tcti_target_expr *operand_expression,
	const struct orlix_tcti_target_expr *value_expression, int *literal)
{
	struct operand_symbol *symbol;
	size_t length;
	unsigned int source_bit;
	int equal;

	if (!operand_expression || !value_expression ||
		operand_expression->kind != ORLIX_TCTI_TARGET_EXPR_OPERAND ||
		value_expression->kind != ORLIX_TCTI_TARGET_EXPR_VALUE ||
		operand_symbol_get(compiler, operand_expression->text, &symbol) ||
		!value_expression->text)
		return -1;
	length = strlen(value_expression->text);
	if (length < 3U || value_expression->text[0] != '\'' ||
		value_expression->text[length - 1U] != '\'')
		return -1;
	if (length - 2U != symbol->width)
		return boolean_constant(compiler, 0, literal);
	if (boolean_constant(compiler, 1, &equal))
		return -1;
	for (source_bit = 0; source_bit < symbol->width; source_bit++) {
		unsigned int bit = symbol->width - source_bit - 1U;
		char pattern = value_expression->text[source_bit + 1U];
		int expected;
		int actual = symbol->bits[bit];
		int next;

		if (pattern == 'x')
			continue;
		if (pattern != '0' && pattern != '1')
			return -1;
		expected = pattern == '1';

		if (!actual) {
			if (((symbol->fixed_value >> bit) & 1U) != (unsigned int)expected)
				return boolean_constant(compiler, 0, literal);
			continue;
		}
		next = fresh(compiler);
		if (!next || equivalent_and(compiler, next, equal,
			expected ? actual : -actual))
			return -1;
		equal = next;
	}
	*literal = equal;
	return 0;
}

static int target_relation(struct compiler *compiler,
	const struct orlix_tcti_target_expr *expression, int *literal)
{
	const struct orlix_tcti_target_expr *left, *right;
	int equal;

	if (expression->left >= compiler->inventory->expression_count ||
		expression->right >= compiler->inventory->expression_count)
		return -1;
	left = &compiler->inventory->expressions[expression->left];
	right = &compiler->inventory->expressions[expression->right];
	if (expression->kind == ORLIX_TCTI_TARGET_EXPR_EQ ||
		expression->kind == ORLIX_TCTI_TARGET_EXPR_NE) {
		if (left->kind == ORLIX_TCTI_TARGET_EXPR_VALUE &&
			right->kind == ORLIX_TCTI_TARGET_EXPR_OPERAND) {
			const struct orlix_tcti_target_expr *swap = left;

			left = right;
			right = swap;
		}
		if (operand_value_equal(compiler, left, right, &equal))
			return -1;
		if (expression->kind == ORLIX_TCTI_TARGET_EXPR_EQ) {
			*literal = equal;
			return 0;
		}
		*literal = fresh(compiler);
		return !*literal || equivalent_not(compiler, *literal, equal) ? -1 : 0;
	}
	if (expression->kind == ORLIX_TCTI_TARGET_EXPR_IN &&
		left->kind == ORLIX_TCTI_TARGET_EXPR_OPERAND &&
		right->kind == ORLIX_TCTI_TARGET_EXPR_SET &&
		right->first_item <= compiler->inventory->set_item_count &&
		right->item_count <= compiler->inventory->set_item_count - right->first_item) {
		size_t item;
		int result;

		if (boolean_constant(compiler, 0, &result))
			return -1;
		for (item = 0; item < right->item_count; item++) {
			uint32_t value_index = compiler->inventory->set_items[
				right->first_item + item];
			int member, next;

			if (value_index >= compiler->inventory->expression_count ||
				operand_value_equal(compiler, left,
					&compiler->inventory->expressions[value_index], &member))
				return -1;
			next = fresh(compiler);
			if (!next || equivalent_or(compiler, next, result, member))
				return -1;
			result = next;
		}
		*literal = result;
		return 0;
	}
	return -1;
}

static int target_operand_dependency(struct compiler *compiler, uint32_t index,
	size_t depth)
{
	const struct orlix_tcti_target_expr *expression;
	int dependent = 0;
	size_t item;

	if (depth > 256U || index >= compiler->inventory->expression_count)
		return -1;
	if (compiler->target_operand_dependencies[index])
		return compiler->target_operand_dependencies[index] == 2U;
	expression = &compiler->inventory->expressions[index];
	switch (expression->kind) {
	case ORLIX_TCTI_TARGET_EXPR_OPERAND:
		dependent = 1;
		break;
	case ORLIX_TCTI_TARGET_EXPR_NOT:
		dependent = target_operand_dependency(compiler, expression->left,
			depth + 1U);
		break;
	case ORLIX_TCTI_TARGET_EXPR_AND:
	case ORLIX_TCTI_TARGET_EXPR_OR:
	case ORLIX_TCTI_TARGET_EXPR_EQ:
	case ORLIX_TCTI_TARGET_EXPR_NE:
	case ORLIX_TCTI_TARGET_EXPR_IN: {
		int left = target_operand_dependency(compiler, expression->left,
			depth + 1U);
		int right = target_operand_dependency(compiler, expression->right,
			depth + 1U);

		if (left < 0 || right < 0)
			return -1;
		dependent = left || right;
		break;
	}
	case ORLIX_TCTI_TARGET_EXPR_SET:
		if (expression->first_item > compiler->inventory->set_item_count ||
			expression->item_count > compiler->inventory->set_item_count -
				expression->first_item)
			return -1;
		for (item = 0; item < expression->item_count; item++) {
			int child = target_operand_dependency(compiler,
				compiler->inventory->set_items[
					expression->first_item + item], depth + 1U);

			if (child < 0)
				return -1;
			dependent = dependent || child;
		}
		break;
	default:
		break;
	}
	compiler->target_operand_dependencies[index] = dependent ? 2U : 1U;
	return dependent;
}

static int target_boolean(struct compiler *compiler, uint32_t index, int *literal)
{
	const struct orlix_tcti_target_expr *expression;
	int left, right, result, parameter;
	int operand_dependent;
	const int unit[] = { 1 };

	if (index >= compiler->inventory->expression_count)
		goto malformed;
	operand_dependent = target_operand_dependency(compiler, index, 0);
	if (operand_dependent < 0)
		goto malformed;
	if (!operand_dependent && compiler->target_literals[index]) {
		*literal = compiler->target_literals[index];
		return 0;
	}
	expression = &compiler->inventory->expressions[index];
	switch (expression->kind) {
	case ORLIX_TCTI_TARGET_EXPR_BOOL:
		result = fresh(compiler);
		if (!result || cnf_clause(compiler, unit, 1)) goto memory;
		compiler->cnf.clauses[compiler->cnf.count - 1U].literals[0] = expression->boolean ? result : -result;
		break;
	case ORLIX_TCTI_TARGET_EXPR_FEATURE:
		parameter = parameter_index(compiler->model, expression->text);
		if (parameter < 0) goto malformed;
		result = parameter + 1;
		break;
	case ORLIX_TCTI_TARGET_EXPR_NOT:
		if (target_boolean(compiler, expression->left, &left)) return -1;
		result = fresh(compiler);
		if (!result || equivalent_not(compiler, result, left)) goto memory;
		break;
	case ORLIX_TCTI_TARGET_EXPR_AND:
	case ORLIX_TCTI_TARGET_EXPR_OR:
		if (target_boolean(compiler, expression->left, &left) ||
		    target_boolean(compiler, expression->right, &right)) return -1;
		result = fresh(compiler);
		if (!result || (expression->kind == ORLIX_TCTI_TARGET_EXPR_AND ?
			equivalent_and(compiler, result, left, right) :
				equivalent_or(compiler, result, left, right))) goto memory;
		break;
	case ORLIX_TCTI_TARGET_EXPR_EQ:
	case ORLIX_TCTI_TARGET_EXPR_NE:
	case ORLIX_TCTI_TARGET_EXPR_IN:
		if (target_relation(compiler, expression, &result))
			goto unsupported;
		break;
	default:
	unsupported:
		fail(compiler->error, ORLIX_TCTI_TARGET_FEATURE_SAT_UNSUPPORTED_SEMANTICS, 0,
			(struct orlix_tcti_feature_provenance) { 0 });
		return -1;
	}
	if (!operand_dependent)
		compiler->target_literals[index] = result;
	*literal = result;
	return 0;
malformed:
	fail(compiler->error, ORLIX_TCTI_TARGET_FEATURE_SAT_MALFORMED_MODEL, 0,
		(struct orlix_tcti_feature_provenance) { 0 });
	return -1;
memory:
	fail(compiler->error, ORLIX_TCTI_TARGET_FEATURE_SAT_NO_MEMORY, 0,
		(struct orlix_tcti_feature_provenance) { 0 });
	return -1;
}

static int literal_value(const struct solver *solver, int literal)
{
	signed char value = solver->values[literal < 0 ? -literal : literal];
	return literal < 0 ? -value : value;
}

static int complete_assignment_satisfies(const struct solver *solver)
{
	size_t clause_index;

	for (clause_index = 0; clause_index < solver->cnf->count; clause_index++) {
		const struct clause *clause = &solver->cnf->clauses[clause_index];
		size_t literal_index;
		int satisfied = 0;

		for (literal_index = 0; literal_index < clause->count; literal_index++)
			if (literal_value(solver, clause->literals[literal_index]) > 0) {
				satisfied = 1;
				break;
			}
		if (!satisfied)
			return 0;
	}
	return 1;
}

static size_t unsatisfied_clause_count(const struct solver *solver)
{
	size_t clause_index, count = 0;

	for (clause_index = 0; clause_index < solver->cnf->count; clause_index++) {
		const struct clause *clause = &solver->cnf->clauses[clause_index];
		size_t literal_index;

		for (literal_index = 0; literal_index < clause->count; literal_index++)
			if (literal_value(solver, clause->literals[literal_index]) > 0)
				break;
		if (literal_index == clause->count)
			count++;
	}
	return count;
}

static const struct clause *unsatisfied_clause(const struct solver *solver,
	size_t ordinal)
{
	size_t clause_index;

	for (clause_index = 0; clause_index < solver->cnf->count; clause_index++) {
		const struct clause *clause = &solver->cnf->clauses[clause_index];
		size_t literal_index;

		for (literal_index = 0; literal_index < clause->count; literal_index++)
			if (literal_value(solver, clause->literals[literal_index]) > 0)
				break;
		if (literal_index == clause->count && !ordinal--)
			return clause;
	}
	return NULL;
}

/*
 * Find a complete candidate cheaply, then validate every clause.  Failure is
 * only a missed seed: the exact DPLL search below remains authoritative.
 */
static int repair_complete_assignment(struct solver *solver,
	const unsigned char *fixed, size_t repair_limit)
{
	uint32_t random = UINT32_C(0x4f524c58);
	size_t repair;

	for (repair = 0; repair < repair_limit; repair++) {
		const struct clause *clause;
		size_t unsatisfied, literal_index, best_count = SIZE_MAX;
		unsigned int best = 0;

		if (++solver->budget->branches > solver->budget->branch_limit) {
			solver->budget->exhausted = 1;
			return -1;
		}
		unsatisfied = unsatisfied_clause_count(solver);
		if (!unsatisfied)
			return complete_assignment_satisfies(solver);
		random = random * UINT32_C(1664525) + UINT32_C(1013904223);
		clause = unsatisfied_clause(solver, random % unsatisfied);
		if (!clause)
			return 0;
		for (literal_index = 0; literal_index < clause->count; literal_index++) {
			unsigned int variable = (unsigned int)(clause->literals[literal_index] < 0 ?
				-clause->literals[literal_index] : clause->literals[literal_index]);
			size_t candidate_count;

			if (fixed[variable])
				continue;
			solver->values[variable] = -solver->values[variable];
			candidate_count = unsatisfied_clause_count(solver);
			solver->values[variable] = -solver->values[variable];
			if (candidate_count < best_count) {
				best = variable;
				best_count = candidate_count;
			}
		}
		if (!best)
			return 0;
		/* A deterministic noise step breaks local minima without adding state. */
		if ((random & 15U) == 0U) {
			size_t start = random % clause->count;

			for (literal_index = 0; literal_index < clause->count; literal_index++) {
				int literal = clause->literals[(start + literal_index) % clause->count];
				unsigned int variable = (unsigned int)(literal < 0 ? -literal : literal);

				if (!fixed[variable]) {
					best = variable;
					break;
				}
			}
		}
		solver->values[best] = -solver->values[best];
	}
	return 0;
}

static int assign_literal(struct solver *solver, int literal)
{
	unsigned int variable = (unsigned int)(literal < 0 ? -literal : literal);
	signed char value = literal < 0 ? -1 : 1;

	if (!solver->values[variable]) {
		if (solver->trail_count >= solver->trail_capacity)
			return -1;
		solver->values[variable] = value;
		solver->trail[solver->trail_count++] = variable;
		return 0;
	}
	return solver->values[variable] == value ? 0 : -1;
}

static void rollback(struct solver *solver, size_t trail_count)
{
	while (solver->trail_count > trail_count)
		solver->values[solver->trail[--solver->trail_count]] = 0;
}

/* Returns 0 when stable, -1 on conflict. */
static int propagate(struct solver *solver)
{
	int changed;
	size_t index;

	do {
		changed = 0;
		for (index = 0; index < solver->cnf->count; index++) {
			const struct clause *clause = &solver->cnf->clauses[index];
			size_t item, unknown = 0;
			int unit = 0, satisfied = 0;

			for (item = 0; item < clause->count; item++) {
				int value = literal_value(solver, clause->literals[item]);
				if (value > 0) { satisfied = 1; break; }
				if (!value) { unknown++; unit = clause->literals[item]; }
			}
			if (satisfied) continue;
			if (!unknown) return -1;
			if (unknown == 1 && !literal_value(solver, unit)) {
				if (assign_literal(solver, unit)) return -1;
				changed = 1;
			}
		}
	} while (changed);
	return 0;
}

static int solve(struct solver *solver)
{
	int decision = 0;
	size_t clause_index, shortest = SIZE_MAX;
	size_t trail_count;
	int result;

	if (propagate(solver))
		return 0;
	for (clause_index = 0; clause_index < solver->cnf->count; clause_index++) {
		const struct clause *clause = &solver->cnf->clauses[clause_index];
		size_t literal_index, unknown = 0;
		int candidate = 0, satisfied = 0;

		for (literal_index = 0; literal_index < clause->count; literal_index++) {
			int value = literal_value(solver, clause->literals[literal_index]);

			if (value > 0) {
				satisfied = 1;
				break;
			}
			if (!value) {
				if (!candidate)
					candidate = clause->literals[literal_index];
				unknown++;
			}
		}
		if (!satisfied && unknown < shortest) {
			shortest = unknown;
			decision = candidate;
		}
	}
	if (!decision)
		return 1;
	if (solver->phase[decision < 0 ? -decision : decision] < 0)
		decision = -(decision < 0 ? -decision : decision);
	else
		decision = decision < 0 ? -decision : decision;
	if (++solver->budget->branches > solver->budget->branch_limit) {
		solver->budget->exhausted = 1;
		return -1;
	}
	trail_count = solver->trail_count;
	if (!assign_literal(solver, decision)) {
		result = solve(solver);
		if (result) return result;
	}
	rollback(solver, trail_count);
	if (!assign_literal(solver, -decision)) {
		result = solve(solver);
		if (result) return result;
	}
	rollback(solver, trail_count);
	return 0;
}

static int save_seed(struct compiler *compiler, const signed char *values)
{
	size_t count = compiler->cnf.variables + 1U;

	if (count > compiler->seed_capacity) {
		signed char *grown;
		size_t added = count - compiler->seed_capacity;

		if (charge(&compiler->budget, added, sizeof(*compiler->seed)))
			return -1;
		grown = realloc(compiler->seed, count * sizeof(*compiler->seed));
		if (!grown)
			return -1;
		compiler->seed = grown;
		compiler->seed_capacity = count;
	}
	memcpy(compiler->seed, values, count * sizeof(*compiler->seed));
	compiler->seed_count = count;
	return 0;
}

static int solve_with(struct compiler *compiler, int assumption,
	signed char *witness, size_t witness_count)
{
	struct solver solver = { .cnf = &compiler->cnf, .budget = &compiler->budget };
	unsigned char *fixed;
	size_t index, repair_limit;
	int candidate_status, status;

	solver.values = budget_calloc(&compiler->budget, compiler->cnf.variables + 1U,
		sizeof(*solver.values));
	solver.phase = budget_calloc(&compiler->budget, compiler->cnf.variables + 1U,
		sizeof(*solver.phase));
	solver.trail = budget_calloc(&compiler->budget, compiler->cnf.variables + 1U,
		sizeof(*solver.trail));
	fixed = budget_calloc(&compiler->budget, compiler->cnf.variables + 1U,
		sizeof(*fixed));
	solver.trail_capacity = compiler->cnf.variables + 1U;
	if (!solver.values || !solver.phase || !solver.trail || !fixed) {
		if (fixed)
			discharge(&compiler->budget, compiler->cnf.variables + 1U,
				sizeof(*fixed));
		if (solver.trail)
			discharge(&compiler->budget, compiler->cnf.variables + 1U,
				sizeof(*solver.trail));
		if (solver.phase)
			discharge(&compiler->budget, compiler->cnf.variables + 1U,
				sizeof(*solver.phase));
		if (solver.values)
			discharge(&compiler->budget, compiler->cnf.variables + 1U,
				sizeof(*solver.values));
		free(fixed);
		free(solver.trail);
		free(solver.phase);
		free(solver.values);
		return -1;
	}
	memset(solver.phase, -1,
		(compiler->cnf.variables + 1U) * sizeof(*solver.phase));
	for (index = 1U; index <= compiler->cnf.variables; index++)
		solver.values[index] = solver.phase[index];
	if (compiler->seed_count) {
		size_t copied = compiler->seed_count;

		if (copied > compiler->cnf.variables + 1U)
			copied = compiler->cnf.variables + 1U;
		memcpy(solver.values, compiler->seed,
			copied * sizeof(*solver.values));
	}
	for (index = 0; index < compiler->cnf.count; index++)
		if (compiler->cnf.clauses[index].count == 1U) {
			int literal = compiler->cnf.clauses[index].literals[0];
			unsigned int variable = (unsigned int)(literal < 0 ? -literal : literal);
			signed char value = literal < 0 ? -1 : 1;

			if (fixed[variable] && solver.values[variable] != value)
				break;
			solver.values[variable] = value;
			fixed[variable] = 1U;
		}
	if (index == compiler->cnf.count && assumption) {
		unsigned int variable = (unsigned int)(assumption < 0 ? -assumption : assumption);
		signed char value = assumption < 0 ? -1 : 1;

		if (!fixed[variable] || solver.values[variable] == value) {
			solver.values[variable] = value;
			fixed[variable] = 1U;
		} else {
			index = compiler->cnf.count + 1U;
		}
	}
	repair_limit = compiler->budget.branch_limit - compiler->budget.branches;
	if (repair_limit > 8U)
		repair_limit = 8U;
	candidate_status = index == compiler->cnf.count ?
		repair_complete_assignment(&solver, fixed, repair_limit) : 0;
	if (candidate_status > 0) {
		status = 1;
	} else if (candidate_status < 0) {
		status = -1;
	} else {
		memset(solver.values, 0,
			(compiler->cnf.variables + 1U) * sizeof(*solver.values));
		solver.trail_count = 0;
		if (assumption && assign_literal(&solver, assumption))
			status = 0;
		else
			status = solve(&solver);
	}
	if (status > 0 && witness) {
		if (witness_count > compiler->model->parameter_count) {
			discharge(&compiler->budget, compiler->cnf.variables + 1U,
				sizeof(*fixed) + sizeof(*solver.trail) +
					sizeof(*solver.phase) + sizeof(*solver.values));
			free(fixed);
			free(solver.trail);
			free(solver.phase);
			free(solver.values);
			return -1;
		}
		for (index = 1U; index <= witness_count; index++)
			if (!solver.values[index])
				solver.values[index] = 1;
		memcpy(witness, solver.values + 1U,
			witness_count * sizeof(*witness));
	}
	if (status > 0) {
		int clauses_valid, assumption_valid, seed_status;

		for (index = 1U; index <= compiler->cnf.variables; index++)
			if (!solver.values[index])
				solver.values[index] = solver.phase[index];
		clauses_valid = complete_assignment_satisfies(&solver);
		assumption_valid = !assumption || literal_value(&solver, assumption) > 0;
		seed_status = clauses_valid && assumption_valid ?
			save_seed(compiler, solver.values) : -1;
		if (!clauses_valid || !assumption_valid || seed_status)
			status = -1;
	}
	discharge(&compiler->budget, compiler->cnf.variables + 1U,
		sizeof(*fixed) + sizeof(*solver.trail) + sizeof(*solver.phase) +
			sizeof(*solver.values));
	free(fixed);
	free(solver.trail);
	free(solver.phase);
	free(solver.values);
	return status;
}

static void field_symbols_destroy(struct compiler *compiler)
{
	size_t index;

	for (index = 0; index < compiler->field_symbol_count; index++) {
		free(compiler->field_symbols[index].bits);
	}
	for (index = 0; index < compiler->operand_symbol_count; index++)
		free(compiler->operand_symbols[index].bits);
	for (index = 0; index < compiler->configuration_symbol_count; index++)
		free(compiler->configuration_symbols[index].value.bits);
	free(compiler->field_symbols);
	free(compiler->operand_symbols);
	free(compiler->configuration_symbols);
	free(compiler->seed);
	compiler->field_symbols = NULL;
	compiler->operand_symbols = NULL;
	compiler->configuration_symbols = NULL;
	compiler->seed = NULL;
	compiler->field_symbol_count = 0;
	compiler->field_symbol_capacity = 0;
	compiler->operand_symbol_count = 0;
	compiler->operand_symbol_capacity = 0;
	compiler->configuration_symbol_count = 0;
	compiler->configuration_symbol_capacity = 0;
	compiler->seed_count = 0;
	compiler->seed_capacity = 0;
}

static uint64_t certificate_hash_u64(uint64_t hash, uint64_t value)
{
	unsigned int byte;

	for (byte = 0; byte < 8U; byte++) {
		hash ^= (value >> (byte * 8U)) & UINT64_C(0xff);
		hash *= UINT64_C(1099511628211);
	}
	return hash;
}

static uint64_t certificate_hash_text(uint64_t hash, const char *text)
{
	if (!text)
		return certificate_hash_u64(hash, UINT64_MAX);
	while (*text) {
		hash ^= (unsigned char)*text++;
		hash *= UINT64_C(1099511628211);
	}
	return certificate_hash_u64(hash, 0U);
}

static int certificate_numeric_value(const struct compiler *compiler,
	const int *bits, unsigned int width, int permit_fixed_bits,
	uint64_t fixed_value, uint64_t *low, uint64_t *high)
{
	unsigned int bit;

	*low = 0;
	*high = 0;
	if (!bits || !width || width > 128U ||
	    compiler->seed_count <= compiler->cnf.variables)
		return -1;
	for (bit = 0; bit < width; bit++) {
		int literal = bits[bit];
		unsigned int variable = (unsigned int)(literal < 0 ? -literal : literal);
		int value;

		if (!literal) {
			if (!permit_fixed_bits)
				return -1;
			value = bit < 64U &&
				(fixed_value & (UINT64_C(1) << bit)) != 0;
		} else {
			if (variable >= compiler->seed_count ||
			 !compiler->seed[variable])
				return -1;
			value = literal < 0 ? compiler->seed[variable] < 0 :
				compiler->seed[variable] > 0;
		}
		if (value && bit < 64U)
			*low |= UINT64_C(1) << bit;
		else if (value)
			*high |= UINT64_C(1) << (bit - 64U);
	}
	return 0;
}

static int certificate_append(struct compiler *compiler,
	struct orlix_tcti_target_feature_sat_audit *audit,
	enum orlix_tcti_target_feature_sat_symbol_kind kind,
	uint32_t identity_group_index, uint32_t leaf_index,
	uint32_t feature_node_index, const char *name,
	const int *bits, unsigned int width, uint64_t fixed_value)
{
	struct orlix_tcti_target_feature_sat_value *value;

	if (reserve(&compiler->budget, (void **)&audit->certificate_values,
		    &audit->certificate_value_capacity,
		    audit->certificate_value_count + 1U,
		    sizeof(*audit->certificate_values)))
		return -1;
	value = &audit->certificate_values[audit->certificate_value_count++];
	*value = (struct orlix_tcti_target_feature_sat_value) {
		.kind = kind,
		.numeric_kind = ORLIX_TCTI_TARGET_FEATURE_SAT_UNSIGNED,
		.identity_group_index = identity_group_index,
		.leaf_index = leaf_index,
		.feature_node_index = feature_node_index,
		.name = name,
		.width = (uint8_t)width,
	};
	if (certificate_numeric_value(compiler, bits, width,
			kind == ORLIX_TCTI_TARGET_FEATURE_SAT_TARGET_OPERAND,
			fixed_value, &value->low, &value->high))
		return -1;
	return 0;
}

static int certificate_capture(struct compiler *compiler,
	struct orlix_tcti_target_feature_sat_audit *audit, size_t leaf_index)
{
	struct orlix_tcti_target_feature_sat_certificate *certificate =
		&audit->certificates[leaf_index];
	uint64_t identity = UINT64_C(1469598103934665603);
	size_t index;

	certificate->condition_index = compiler->inventory->leaves[leaf_index].condition;
	certificate->first_value = audit->certificate_value_count;
	identity = certificate_hash_u64(identity, certificate->condition_index);
	for (index = 0; index < compiler->model->constraint_count; index++)
		identity = certificate_hash_u64(identity,
			compiler->model->constraints[index]);
	for (index = 0; index < compiler->configuration_symbol_count; index++) {
		const struct configuration_symbol *configuration =
			&compiler->configuration_symbols[index];

		if (configuration->representative >= compiler->model->node_count ||
		 certificate_append(compiler, audit,
			ORLIX_TCTI_TARGET_FEATURE_SAT_CONFIGURATION, UINT32_MAX,
			UINT32_MAX, configuration->representative, NULL,
			configuration->value.bits,
			configuration->value.width, 0))
			return -1;
		identity = certificate_hash_u64(identity,
			configuration->representative);
	}
	for (index = 0; index < compiler->free_feature_node_count; index++) {
		uint32_t node_index = compiler->free_feature_nodes[index];
		int literal;

		if (node_index >= compiler->model->node_count ||
		    !compiler->feature_literals[node_index])
			return -1;
		literal = compiler->feature_literals[node_index];
		if (certificate_append(compiler, audit,
			    ORLIX_TCTI_TARGET_FEATURE_SAT_BOOLEAN_IDENTIFIER,
			    UINT32_MAX, UINT32_MAX, node_index,
			    compiler->model->nodes[node_index].text,
			    &literal, 1U, 0))
			return -1;
		identity = certificate_hash_u64(identity, node_index);
	}
	for (index = 0; index < compiler->field_symbol_count; index++) {
		const struct field_symbol *field = &compiler->field_symbols[index];

		if (!field->binding || certificate_append(compiler, audit,
			ORLIX_TCTI_TARGET_FEATURE_SAT_FIELD,
			field->identity_group_index, UINT32_MAX, UINT32_MAX, NULL,
			field->bits,
			field->width, 0))
			return -1;
		identity = certificate_hash_u64(identity,
			field->binding->domain.semantic_identity);
		identity = certificate_hash_u64(identity,
			field->binding->domain.resolution_identity);
	}
	for (index = 0; index < compiler->operand_symbol_count; index++) {
		const struct operand_symbol *operand = &compiler->operand_symbols[index];

		if (operand->leaf_index != leaf_index)
			continue;
		if (certificate_append(compiler, audit,
			ORLIX_TCTI_TARGET_FEATURE_SAT_TARGET_OPERAND, UINT32_MAX,
			(uint32_t)leaf_index, UINT32_MAX, operand->name, operand->bits,
			operand->width, operand->fixed_value))
			return -1;
		identity = certificate_hash_text(identity, operand->name);
	}
	certificate->value_count = audit->certificate_value_count -
		certificate->first_value;
	certificate->formula_identity = identity;
	return 0;
}

int orlix_tcti_target_feature_sat_audit(const struct orlix_tcti_feature_model *model,
	const struct orlix_tcti_target_inventory *inventory, size_t branch_limit,
	size_t allocation_limit,
	const struct orlix_tcti_feature_field_domain_bindings *field_domains,
	struct orlix_tcti_target_feature_sat_audit *audit,
	struct orlix_tcti_target_feature_sat_error *error)
{
	struct compiler compiler = { .model = model, .inventory = inventory,
		.field_domains = field_domains,
		.budget = { .branch_limit = branch_limit, .allocation_limit = allocation_limit },
		.error = error };
	size_t index;
	int literal, status;
	const int unit[] = { 1 };

	if (error) *error = (struct orlix_tcti_target_feature_sat_error) { 0 };
	if (!model || !inventory || !audit || !branch_limit || !allocation_limit) {
		fail(error, ORLIX_TCTI_TARGET_FEATURE_SAT_INVALID_ARGUMENT, 0,
			(struct orlix_tcti_feature_provenance) { 0 });
		return -1;
	}
	*audit = (struct orlix_tcti_target_feature_sat_audit) {
		.leaf_count = inventory->leaf_count,
		.parameter_count = model->parameter_count,
	};
	if (model->parameter_count > INT_MAX || model->node_count > UINT32_MAX ||
	    inventory->expression_count > UINT32_MAX) goto malformed;
	if (validate_input_graphs(&compiler)) {
		if (compiler.budget.exhausted) goto limit;
		goto out;
	}
	compiler.cnf.variables = (unsigned int)model->parameter_count;
	compiler.feature_literals = budget_calloc(&compiler.budget, model->node_count,
					  sizeof(*compiler.feature_literals));
	compiler.target_literals = budget_calloc(&compiler.budget,
		inventory->expression_count, sizeof(*compiler.target_literals));
	compiler.target_operand_dependencies = budget_calloc(&compiler.budget,
		inventory->expression_count,
		sizeof(*compiler.target_operand_dependencies));
	if ((model->node_count && !compiler.feature_literals) ||
	    (inventory->expression_count && (!compiler.target_literals ||
		!compiler.target_operand_dependencies))) goto memory;
	for (index = 0; index < model->constraint_count; index++) {
		if (feature_boolean(&compiler, model->constraints[index], &literal)) goto out;
		if (cnf_clause(&compiler, unit, 1)) goto memory;
		compiler.cnf.clauses[compiler.cnf.count - 1U].literals[0] = literal;
	}
	if (collect_free_feature_nodes(&compiler))
		goto memory;
	status = solve_with(&compiler, 0, NULL, 0);
	if (status < 0) goto limit;
	if (!status) { fail(error, ORLIX_TCTI_TARGET_FEATURE_SAT_UNSAT_BASE, 0,
		(struct orlix_tcti_feature_provenance) { 0 }); goto out; }
	if (inventory->leaf_count) {
		audit->leaves = budget_calloc(&compiler.budget, inventory->leaf_count,
					 sizeof(*audit->leaves));
		audit->certificates = budget_calloc(&compiler.budget,
			inventory->leaf_count, sizeof(*audit->certificates));
		if (!audit->leaves || !audit->certificates) goto memory;
		if (model->parameter_count) {
			size_t witness_count;

			if (multiply(inventory->leaf_count, model->parameter_count,
				     &witness_count)) goto memory;
			audit->witnesses = budget_calloc(&compiler.budget, witness_count,
						 sizeof(*audit->witnesses));
			if (!audit->witnesses) goto memory;
		}
	}
	for (index = 0; index < inventory->leaf_count; index++) {
		compiler.current_leaf = index;
		if (target_boolean(&compiler, inventory->leaves[index].condition, &literal)) {
			if (error) error->leaf_index = index;
			goto out;
		}
		status = solve_with(&compiler, literal,
			audit->witnesses ? audit->witnesses +
				index * audit->parameter_count : NULL,
			audit->parameter_count);
		if (status < 0) { if (error) error->leaf_index = index; goto limit; }
		audit->leaves[index] = status ? ORLIX_TCTI_TARGET_FEATURE_SAT_APPLICABLE :
			ORLIX_TCTI_TARGET_FEATURE_SAT_IMPOSSIBLE;
		if (status && certificate_capture(&compiler, audit, index)) {
			if (error) error->leaf_index = index;
			goto memory;
		}
	}
	field_symbols_destroy(&compiler);
	free(compiler.feature_literals); free(compiler.target_literals);
	free(compiler.free_feature_nodes);
	free(compiler.target_operand_dependencies); cnf_destroy(&compiler.cnf);
	return 0;
malformed:
	fail(error, ORLIX_TCTI_TARGET_FEATURE_SAT_MALFORMED_MODEL, 0,
		(struct orlix_tcti_feature_provenance) { 0 });
	goto out;
memory:
	fail(error, compiler.budget.exhausted ? ORLIX_TCTI_TARGET_FEATURE_SAT_RESOURCE_LIMIT :
		ORLIX_TCTI_TARGET_FEATURE_SAT_NO_MEMORY, 0,
		(struct orlix_tcti_feature_provenance) { 0 });
	goto out;
limit:
	fail(error, ORLIX_TCTI_TARGET_FEATURE_SAT_RESOURCE_LIMIT, 0,
		(struct orlix_tcti_feature_provenance) { 0 });
out:
	field_symbols_destroy(&compiler);
	free(compiler.feature_literals); free(compiler.target_literals);
	free(compiler.free_feature_nodes);
	free(compiler.target_operand_dependencies); cnf_destroy(&compiler.cnf);
	orlix_tcti_target_feature_sat_audit_destroy(audit);
	return -1;
}

void orlix_tcti_target_feature_sat_audit_destroy(struct orlix_tcti_target_feature_sat_audit *audit)
{
	if (!audit) return;
	free(audit->witnesses);
	free(audit->certificate_values);
	free(audit->certificates);
	free(audit->leaves);
	*audit = (struct orlix_tcti_target_feature_sat_audit) { 0 };
}

const signed char *orlix_tcti_target_feature_sat_witness(
	const struct orlix_tcti_target_feature_sat_audit *audit, size_t leaf_index)
{
	if (!audit || leaf_index >= audit->leaf_count ||
	    !audit->leaves ||
	    audit->leaves[leaf_index] != ORLIX_TCTI_TARGET_FEATURE_SAT_APPLICABLE ||
	    !audit->parameter_count || !audit->witnesses)
		return NULL;
	return audit->witnesses + leaf_index * audit->parameter_count;
}

const struct orlix_tcti_target_feature_sat_certificate *
orlix_tcti_target_feature_sat_certificate(
	const struct orlix_tcti_target_feature_sat_audit *audit, size_t leaf_index)
{
	if (!audit || leaf_index >= audit->leaf_count || !audit->leaves ||
	    !audit->certificates ||
	    audit->leaves[leaf_index] != ORLIX_TCTI_TARGET_FEATURE_SAT_APPLICABLE)
		return NULL;
	return &audit->certificates[leaf_index];
}

const struct orlix_tcti_target_feature_sat_value *
orlix_tcti_target_feature_sat_certificate_values(
	const struct orlix_tcti_target_feature_sat_audit *audit,
	size_t leaf_index, size_t *value_count)
{
	const struct orlix_tcti_target_feature_sat_certificate *certificate =
		orlix_tcti_target_feature_sat_certificate(audit, leaf_index);

	if (value_count)
		*value_count = 0;
	if (!certificate || !audit->certificate_values ||
	    certificate->first_value > audit->certificate_value_count ||
	    certificate->value_count > audit->certificate_value_count -
		certificate->first_value)
		return NULL;
	if (value_count)
		*value_count = certificate->value_count;
	return audit->certificate_values + certificate->first_value;
}
