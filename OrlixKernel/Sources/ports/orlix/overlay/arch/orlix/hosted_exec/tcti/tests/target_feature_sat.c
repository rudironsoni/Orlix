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
	const struct tcti_feature_model *model;
	const struct tcti_target_inventory *inventory;
	struct cnf cnf;
	int *feature_literals;
	int *target_literals;
	struct budget budget;
	struct tcti_target_feature_sat_error *error;
};
struct solver { const struct cnf *cnf; signed char *values; struct budget *budget; };

static void fail(struct tcti_target_feature_sat_error *error,
		enum tcti_target_feature_sat_error_code code, size_t leaf,
		struct tcti_feature_provenance provenance)
{
	if (error && error->code == TCTI_TARGET_FEATURE_SAT_OK)
		*error = (struct tcti_target_feature_sat_error) {
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

	while (next < count) {
		if (next > SIZE_MAX / 2U)
			return -1;
		next *= 2U;
	}
	if (next > SIZE_MAX / size || charge(budget, next, size))
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

static int parameter_index(const struct tcti_feature_model *model, const char *name)
{
	size_t index;

	if (!name)
		return -1;
	for (index = 0; index < model->parameter_count; index++)
		if (model->parameters[index].name && !strcmp(model->parameters[index].name, name))
			return (int)index;
	return -1;
}

static int index_in_range(uint32_t index, size_t count)
{
	return index < count;
}

static int range_in_range(uint32_t first, uint32_t count, size_t total)
{
	return first <= total && count <= total - first;
}

static int feature_child(const struct tcti_feature_model *model,
		uint32_t index, uint32_t child, uint32_t *result)
{
	const struct tcti_feature_node *node = &model->nodes[index];

	switch (node->kind) {
	case TCTI_FEATURE_NOT:
		if (child) return -1;
		*result = node->left;
		return 0;
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
		if (child > 1U) return -1;
		*result = child ? node->right : node->left;
		return 0;
	case TCTI_FEATURE_DOT_ATOM:
	case TCTI_FEATURE_SET:
	case TCTI_FEATURE_UINT:
	case TCTI_FEATURE_SINT:
		if (child >= node->child_count) return -1;
		*result = model->children[node->first_child + child];
		return 0;
	default:
		return -1;
	}
}

static int feature_child_count(const struct tcti_feature_model *model,
		uint32_t index, uint32_t *count)
{
	const struct tcti_feature_node *node = &model->nodes[index];

	switch (node->kind) {
	case TCTI_FEATURE_BOOL:
	case TCTI_FEATURE_IDENTIFIER:
	case TCTI_FEATURE_INTEGER:
	case TCTI_FEATURE_VALUE:
	case TCTI_FEATURE_FIELD:
		if ((node->kind == TCTI_FEATURE_IDENTIFIER ||
		     node->kind == TCTI_FEATURE_VALUE) && !node->text)
			return -1;
		if (node->kind == TCTI_FEATURE_FIELD && (!node->field.state ||
		     !node->field.register_name || !node->field.selector))
			return -1;
		*count = 0;
		return 0;
	case TCTI_FEATURE_NOT:
		*count = 1;
		return 0;
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
		*count = 2;
		return 0;
	case TCTI_FEATURE_DOT_ATOM:
	case TCTI_FEATURE_SET:
	case TCTI_FEATURE_UINT:
	case TCTI_FEATURE_SINT:
		if (!range_in_range(node->first_child, node->child_count,
				      model->child_count))
			return -1;
		*count = node->child_count;
		return 0;
	default:
		return -1;
	}
}

static int validate_feature_node(const struct tcti_feature_model *model,
		uint32_t index, unsigned char *color, size_t depth,
		struct tcti_target_feature_sat_error *error)
{
	uint32_t count, child, next;

	if (!index_in_range(index, model->node_count) || depth > model->node_count ||
	    color[index] == 1U) {
		fail(error, TCTI_TARGET_FEATURE_SAT_MALFORMED_MODEL, 0,
			index_in_range(index, model->node_count) ?
			model->nodes[index].provenance : (struct tcti_feature_provenance) { 0 });
		return -1;
	}
	if (color[index] == 2U)
		return 0;
	if (feature_child_count(model, index, &count)) {
		fail(error, TCTI_TARGET_FEATURE_SAT_MALFORMED_MODEL, 0,
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

static int target_child(const struct tcti_target_inventory *inventory,
		uint32_t index, uint32_t child, uint32_t *result)
{
	const struct tcti_target_expr *expression = &inventory->expressions[index];

	switch (expression->kind) {
	case TCTI_TARGET_EXPR_NOT:
		if (child) return -1;
		*result = expression->left;
		return 0;
	case TCTI_TARGET_EXPR_AND:
	case TCTI_TARGET_EXPR_OR:
	case TCTI_TARGET_EXPR_EQ:
	case TCTI_TARGET_EXPR_NE:
	case TCTI_TARGET_EXPR_IN:
		if (child > 1U) return -1;
		*result = child ? expression->right : expression->left;
		return 0;
	case TCTI_TARGET_EXPR_SET:
		if (child >= expression->item_count) return -1;
		*result = inventory->set_items[expression->first_item + child];
		return 0;
	default:
		return -1;
	}
}

static int target_child_count(const struct tcti_target_inventory *inventory,
		uint32_t index, uint32_t *count)
{
	const struct tcti_target_expr *expression = &inventory->expressions[index];

	switch (expression->kind) {
	case TCTI_TARGET_EXPR_BOOL:
	case TCTI_TARGET_EXPR_FEATURE:
	case TCTI_TARGET_EXPR_OPERAND:
	case TCTI_TARGET_EXPR_VALUE:
		if (expression->kind != TCTI_TARGET_EXPR_BOOL && !expression->text)
			return -1;
		*count = 0;
		return 0;
	case TCTI_TARGET_EXPR_NOT:
		*count = 1;
		return 0;
	case TCTI_TARGET_EXPR_AND:
	case TCTI_TARGET_EXPR_OR:
	case TCTI_TARGET_EXPR_EQ:
	case TCTI_TARGET_EXPR_NE:
	case TCTI_TARGET_EXPR_IN:
		*count = 2;
		return 0;
	case TCTI_TARGET_EXPR_SET:
		if (!range_in_range(expression->first_item, expression->item_count,
				      inventory->set_item_count))
			return -1;
		*count = expression->item_count;
		return 0;
	default:
		return -1;
	}
}

static int validate_target_node(const struct tcti_target_inventory *inventory,
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

static int feature_semantics_supported(enum tcti_feature_node_kind kind)
{
	switch (kind) {
	case TCTI_FEATURE_BOOL:
	case TCTI_FEATURE_IDENTIFIER:
	case TCTI_FEATURE_NOT:
	case TCTI_FEATURE_AND:
	case TCTI_FEATURE_OR:
	case TCTI_FEATURE_EQ:
	case TCTI_FEATURE_NE:
	case TCTI_FEATURE_IMPLIES:
	case TCTI_FEATURE_IFF:
		return 1;
	default:
		return 0;
	}
}

static int target_semantics_supported(enum tcti_target_expr_kind kind)
{
	switch (kind) {
	case TCTI_TARGET_EXPR_BOOL:
	case TCTI_TARGET_EXPR_FEATURE:
	case TCTI_TARGET_EXPR_NOT:
	case TCTI_TARGET_EXPR_AND:
	case TCTI_TARGET_EXPR_OR:
		return 1;
	default:
		return 0;
	}
}

static int validate_input_storage(struct compiler *compiler)
{
	const struct tcti_feature_model *model = compiler->model;
	const struct tcti_target_inventory *inventory = compiler->inventory;
	size_t index, other;

	if ((model->parameter_count && !model->parameters) ||
	    (model->constraint_count && !model->constraints) ||
	    (model->node_count && !model->nodes) ||
	    (model->child_count && !model->children) ||
	    (inventory->leaf_count && !inventory->leaves) ||
	    (inventory->expression_count && !inventory->expressions) ||
	    (inventory->set_item_count && !inventory->set_items))
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
		fail(compiler->error, TCTI_TARGET_FEATURE_SAT_MALFORMED_MODEL, 0,
			(struct tcti_feature_provenance) { 0 });
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
			fail(compiler->error, TCTI_TARGET_FEATURE_SAT_MALFORMED_MODEL, 0,
				(struct tcti_feature_provenance) { 0 });
			goto out;
		}
	for (index = 0; index < compiler->model->node_count; index++)
		if (!feature_semantics_supported(compiler->model->nodes[index].kind)) {
			fail(compiler->error, TCTI_TARGET_FEATURE_SAT_UNSUPPORTED_SEMANTICS,
				0, compiler->model->nodes[index].provenance);
			goto out;
		}
	for (index = 0; index < compiler->inventory->expression_count; index++)
		if (!target_semantics_supported(compiler->inventory->expressions[index].kind)) {
			fail(compiler->error, TCTI_TARGET_FEATURE_SAT_UNSUPPORTED_SEMANTICS,
				0, (struct tcti_feature_provenance) { 0 });
			goto out;
		}
	status = 0;
out:
	free(target_color);
	free(feature_color);
	return status;
}

static int feature_boolean(struct compiler *compiler, uint32_t index, int *literal)
{
	const struct tcti_feature_node *node = NULL;
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
	case TCTI_FEATURE_BOOL:
		result = fresh(compiler);
		if (!result || cnf_clause(compiler, node->integer ? unit_true : unit_false, 1))
			goto memory;
		/* The first created variable is not necessarily 1. */
		compiler->cnf.clauses[compiler->cnf.count - 1U].literals[0] = node->integer ? result : -result;
		break;
	case TCTI_FEATURE_IDENTIFIER:
		parameter = parameter_index(compiler->model, node->text);
		if (parameter < 0)
			goto malformed;
		result = parameter + 1;
		break;
	case TCTI_FEATURE_NOT:
		if (feature_boolean(compiler, node->left, &left)) return -1;
		result = fresh(compiler);
		if (!result || equivalent_not(compiler, result, left)) goto memory;
		break;
	case TCTI_FEATURE_AND:
	case TCTI_FEATURE_OR:
	case TCTI_FEATURE_IMPLIES:
	case TCTI_FEATURE_IFF:
	case TCTI_FEATURE_EQ:
	case TCTI_FEATURE_NE:
		if (feature_boolean(compiler, node->left, &left) ||
		    feature_boolean(compiler, node->right, &right)) return -1;
		result = fresh(compiler);
		if (!result) goto memory;
		if (node->kind == TCTI_FEATURE_AND && equivalent_and(compiler, result, left, right)) goto memory;
		if (node->kind == TCTI_FEATURE_OR && equivalent_or(compiler, result, left, right)) goto memory;
		if (node->kind == TCTI_FEATURE_IMPLIES && equivalent_or(compiler, result, -left, right)) goto memory;
		if (node->kind == TCTI_FEATURE_IFF && equivalent_iff(compiler, result, left, right)) goto memory;
		if (node->kind == TCTI_FEATURE_EQ && equivalent_iff(compiler, result, left, right)) goto memory;
		if (node->kind == TCTI_FEATURE_NE) {
			int equal = fresh(compiler);
			if (!equal || equivalent_iff(compiler, equal, left, right) ||
			    equivalent_not(compiler, result, equal)) goto memory;
		}
		break;
	default:
		fail(compiler->error, TCTI_TARGET_FEATURE_SAT_UNSUPPORTED_SEMANTICS, 0,
			node->provenance);
		return -1;
	}
	compiler->feature_literals[index] = result;
	*literal = result;
	return 0;
malformed:
	fail(compiler->error, TCTI_TARGET_FEATURE_SAT_MALFORMED_MODEL, 0,
		node ? node->provenance : (struct tcti_feature_provenance) { 0 });
	return -1;
memory:
	fail(compiler->error, TCTI_TARGET_FEATURE_SAT_NO_MEMORY, 0,
		node ? node->provenance : (struct tcti_feature_provenance) { 0 });
	return -1;
}

static int target_boolean(struct compiler *compiler, uint32_t index, int *literal)
{
	const struct tcti_target_expr *expression;
	int left, right, result, parameter;
	const int unit[] = { 1 };

	if (index >= compiler->inventory->expression_count)
		goto malformed;
	if (compiler->target_literals[index]) {
		*literal = compiler->target_literals[index];
		return 0;
	}
	expression = &compiler->inventory->expressions[index];
	switch (expression->kind) {
	case TCTI_TARGET_EXPR_BOOL:
		result = fresh(compiler);
		if (!result || cnf_clause(compiler, unit, 1)) goto memory;
		compiler->cnf.clauses[compiler->cnf.count - 1U].literals[0] = expression->boolean ? result : -result;
		break;
	case TCTI_TARGET_EXPR_FEATURE:
		parameter = parameter_index(compiler->model, expression->text);
		if (parameter < 0) goto malformed;
		result = parameter + 1;
		break;
	case TCTI_TARGET_EXPR_NOT:
		if (target_boolean(compiler, expression->left, &left)) return -1;
		result = fresh(compiler);
		if (!result || equivalent_not(compiler, result, left)) goto memory;
		break;
	case TCTI_TARGET_EXPR_AND:
	case TCTI_TARGET_EXPR_OR:
		if (target_boolean(compiler, expression->left, &left) ||
		    target_boolean(compiler, expression->right, &right)) return -1;
		result = fresh(compiler);
		if (!result || (expression->kind == TCTI_TARGET_EXPR_AND ?
			equivalent_and(compiler, result, left, right) :
			equivalent_or(compiler, result, left, right))) goto memory;
		break;
	default:
		fail(compiler->error, TCTI_TARGET_FEATURE_SAT_UNSUPPORTED_SEMANTICS, 0,
			(struct tcti_feature_provenance) { 0 });
		return -1;
	}
	compiler->target_literals[index] = result;
	*literal = result;
	return 0;
malformed:
	fail(compiler->error, TCTI_TARGET_FEATURE_SAT_MALFORMED_MODEL, 0,
		(struct tcti_feature_provenance) { 0 });
	return -1;
memory:
	fail(compiler->error, TCTI_TARGET_FEATURE_SAT_NO_MEMORY, 0,
		(struct tcti_feature_provenance) { 0 });
	return -1;
}

static int literal_value(const struct solver *solver, int literal)
{
	signed char value = solver->values[literal < 0 ? -literal : literal];
	return literal < 0 ? -value : value;
}

static int assign_literal(struct solver *solver, int literal)
{
	unsigned int variable = (unsigned int)(literal < 0 ? -literal : literal);
	signed char value = literal < 0 ? -1 : 1;

	if (!solver->values[variable]) {
		solver->values[variable] = value;
		return 0;
	}
	return solver->values[variable] == value ? 0 : -1;
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
	unsigned int variable;
	signed char *saved;
	int result;

	if (propagate(solver)) return 0;
	for (variable = 1; variable <= solver->cnf->variables; variable++)
		if (!solver->values[variable]) break;
	if (variable > solver->cnf->variables) return 1;
	if (++solver->budget->branches > solver->budget->branch_limit) {
		solver->budget->exhausted = 1;
		return -1;
	}
	saved = budget_malloc(solver->budget, solver->cnf->variables + 1U,
			     sizeof(*saved));
	if (!saved) return -1;
	memcpy(saved, solver->values, (solver->cnf->variables + 1U) * sizeof(*saved));
	if (!assign_literal(solver, (int)variable)) {
		result = solve(solver);
		if (result) { free(saved); return result; }
	}
	memcpy(solver->values, saved, (solver->cnf->variables + 1U) * sizeof(*saved));
	if (!assign_literal(solver, -(int)variable)) {
		result = solve(solver);
		if (result) { free(saved); return result; }
	}
	memcpy(solver->values, saved, (solver->cnf->variables + 1U) * sizeof(*saved));
	free(saved);
	return 0;
}

static int solve_with(struct compiler *compiler, int assumption)
{
	struct solver solver = { .cnf = &compiler->cnf, .budget = &compiler->budget };
	int status;

	solver.values = budget_calloc(&compiler->budget, compiler->cnf.variables + 1U,
				     sizeof(*solver.values));
	if (!solver.values) return -1;
	if (assumption && assign_literal(&solver, assumption)) status = 0;
	else status = solve(&solver);
	free(solver.values);
	return status;
}

int tcti_target_feature_sat_audit(const struct tcti_feature_model *model,
	const struct tcti_target_inventory *inventory, size_t branch_limit,
	size_t allocation_limit,
	struct tcti_target_feature_sat_audit *audit,
	struct tcti_target_feature_sat_error *error)
{
	struct compiler compiler = { .model = model, .inventory = inventory,
		.budget = { .branch_limit = branch_limit, .allocation_limit = allocation_limit },
		.error = error };
	size_t index;
	int literal, status;
	const int unit[] = { 1 };

	if (error) *error = (struct tcti_target_feature_sat_error) { 0 };
	if (!model || !inventory || !audit || !branch_limit || !allocation_limit) {
		fail(error, TCTI_TARGET_FEATURE_SAT_INVALID_ARGUMENT, 0,
			(struct tcti_feature_provenance) { 0 });
		return -1;
	}
	*audit = (struct tcti_target_feature_sat_audit) { .leaf_count = inventory->leaf_count };
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
	if ((model->node_count && !compiler.feature_literals) ||
	    (inventory->expression_count && !compiler.target_literals)) goto memory;
	for (index = 0; index < model->constraint_count; index++) {
		if (feature_boolean(&compiler, model->constraints[index], &literal)) goto out;
		if (cnf_clause(&compiler, unit, 1)) goto memory;
		compiler.cnf.clauses[compiler.cnf.count - 1U].literals[0] = literal;
	}
	status = solve_with(&compiler, 0);
	if (status < 0) goto limit;
	if (!status) { fail(error, TCTI_TARGET_FEATURE_SAT_UNSAT_BASE, 0,
		(struct tcti_feature_provenance) { 0 }); goto out; }
	if (inventory->leaf_count) {
		audit->leaves = budget_calloc(&compiler.budget, inventory->leaf_count,
					      sizeof(*audit->leaves));
		if (!audit->leaves) goto memory;
	}
	for (index = 0; index < inventory->leaf_count; index++) {
		if (target_boolean(&compiler, inventory->leaves[index].condition, &literal)) {
			if (error) error->leaf_index = index;
			goto out;
		}
		status = solve_with(&compiler, literal);
		if (status < 0) { if (error) error->leaf_index = index; goto limit; }
		audit->leaves[index] = status ? TCTI_TARGET_FEATURE_SAT_APPLICABLE :
			TCTI_TARGET_FEATURE_SAT_IMPOSSIBLE;
	}
	free(compiler.feature_literals); free(compiler.target_literals); cnf_destroy(&compiler.cnf);
	return 0;
malformed:
	fail(error, TCTI_TARGET_FEATURE_SAT_MALFORMED_MODEL, 0,
		(struct tcti_feature_provenance) { 0 });
	goto out;
memory:
	fail(error, compiler.budget.exhausted ? TCTI_TARGET_FEATURE_SAT_RESOURCE_LIMIT :
		TCTI_TARGET_FEATURE_SAT_NO_MEMORY, 0,
		(struct tcti_feature_provenance) { 0 });
	goto out;
limit:
	fail(error, TCTI_TARGET_FEATURE_SAT_RESOURCE_LIMIT, 0,
		(struct tcti_feature_provenance) { 0 });
out:
	free(compiler.feature_literals); free(compiler.target_literals); cnf_destroy(&compiler.cnf);
	tcti_target_feature_sat_audit_destroy(audit);
	return -1;
}

void tcti_target_feature_sat_audit_destroy(struct tcti_target_feature_sat_audit *audit)
{
	if (!audit) return;
	free(audit->leaves);
	*audit = (struct tcti_target_feature_sat_audit) { 0 };
}
