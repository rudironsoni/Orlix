/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_feature_artifact.h"

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif

#define TCTI_FEATURE_CANONICAL_DEF "../isa/target_feature_artifact.def"

#define TCTI_A64_FEATURE_ARTIFACT_SOURCE(...)
#define TCTI_A64_FEATURE_ARTIFACT_COUNTS(...)
#define TCTI_A64_FEATURE_PARAMETER(i, n, f, c, o, l) \
	[i] = { n, f, c, { o, l } },
#define TCTI_A64_FEATURE_CONSTRAINT(...)
#define TCTI_A64_FEATURE_NODE(...)
#define TCTI_A64_FEATURE_CHILD(...)
static const struct tcti_feature_artifact_parameter canonical_parameters[] = {
#include TCTI_FEATURE_CANONICAL_DEF
};
#undef TCTI_A64_FEATURE_ARTIFACT_SOURCE
#undef TCTI_A64_FEATURE_ARTIFACT_COUNTS
#undef TCTI_A64_FEATURE_PARAMETER
#undef TCTI_A64_FEATURE_CONSTRAINT
#undef TCTI_A64_FEATURE_NODE
#undef TCTI_A64_FEATURE_CHILD

#define TCTI_A64_FEATURE_ARTIFACT_SOURCE(...)
#define TCTI_A64_FEATURE_ARTIFACT_COUNTS(...)
#define TCTI_A64_FEATURE_PARAMETER(...)
#define TCTI_A64_FEATURE_CONSTRAINT(i, n, o, l) [i] = { n, { o, l } },
#define TCTI_A64_FEATURE_NODE(...)
#define TCTI_A64_FEATURE_CHILD(...)
static const struct tcti_feature_artifact_constraint canonical_constraints[] = {
#include TCTI_FEATURE_CANONICAL_DEF
};
#undef TCTI_A64_FEATURE_ARTIFACT_SOURCE
#undef TCTI_A64_FEATURE_ARTIFACT_COUNTS
#undef TCTI_A64_FEATURE_PARAMETER
#undef TCTI_A64_FEATURE_CONSTRAINT
#undef TCTI_A64_FEATURE_NODE
#undef TCTI_A64_FEATURE_CHILD

#define TCTI_A64_FEATURE_ARTIFACT_SOURCE(...)
#define TCTI_A64_FEATURE_ARTIFACT_COUNTS(...)
#define TCTI_A64_FEATURE_PARAMETER(...)
#define TCTI_A64_FEATURE_CONSTRAINT(...)
#define TCTI_A64_FEATURE_NODE(i, k, left, right, first, count, integer, text, \
	state, reg, selector, field_o, field_l, o, l) \
	[i] = { k, left, right, first, count, integer, text, state, reg, selector, \
		{ field_o, field_l }, { o, l } },
#define TCTI_A64_FEATURE_CHILD(...)
static const struct tcti_feature_artifact_node canonical_nodes[] = {
#include TCTI_FEATURE_CANONICAL_DEF
};
#undef TCTI_A64_FEATURE_ARTIFACT_SOURCE
#undef TCTI_A64_FEATURE_ARTIFACT_COUNTS
#undef TCTI_A64_FEATURE_PARAMETER
#undef TCTI_A64_FEATURE_CONSTRAINT
#undef TCTI_A64_FEATURE_NODE
#undef TCTI_A64_FEATURE_CHILD

#define TCTI_A64_FEATURE_ARTIFACT_SOURCE(...)
#define TCTI_A64_FEATURE_ARTIFACT_COUNTS(...)
#define TCTI_A64_FEATURE_PARAMETER(...)
#define TCTI_A64_FEATURE_CONSTRAINT(...)
#define TCTI_A64_FEATURE_NODE(...)
#define TCTI_A64_FEATURE_CHILD(i, n) [i] = n,
static const tcti_feature_artifact_u32 canonical_children[] = {
#include TCTI_FEATURE_CANONICAL_DEF
};
#undef TCTI_A64_FEATURE_ARTIFACT_SOURCE
#undef TCTI_A64_FEATURE_ARTIFACT_COUNTS
#undef TCTI_A64_FEATURE_PARAMETER
#undef TCTI_A64_FEATURE_CONSTRAINT
#undef TCTI_A64_FEATURE_NODE
#undef TCTI_A64_FEATURE_CHILD

static const struct tcti_feature_artifact canonical_artifact = {
#define TCTI_A64_FEATURE_ARTIFACT_SOURCE(a, b, r, s, h, l) \
	.source = { a, b, r, s, h, l },
#define TCTI_A64_FEATURE_ARTIFACT_COUNTS(p, c, pc, n, ch, gc) \
	.counts = { p, c, pc, n, ch, gc },
#define TCTI_A64_FEATURE_PARAMETER(...)
#define TCTI_A64_FEATURE_CONSTRAINT(...)
#define TCTI_A64_FEATURE_NODE(...)
#define TCTI_A64_FEATURE_CHILD(...)
#include TCTI_FEATURE_CANONICAL_DEF
#undef TCTI_A64_FEATURE_ARTIFACT_SOURCE
#undef TCTI_A64_FEATURE_ARTIFACT_COUNTS
#undef TCTI_A64_FEATURE_PARAMETER
#undef TCTI_A64_FEATURE_CONSTRAINT
#undef TCTI_A64_FEATURE_NODE
#undef TCTI_A64_FEATURE_CHILD
	.parameters = canonical_parameters,
	.constraints = canonical_constraints,
	.nodes = canonical_nodes,
	.children = canonical_children,
};

const struct tcti_feature_artifact *tcti_feature_artifact_canonical(void)
{
	return &canonical_artifact;
}

enum visit_state {
	VISIT_CLEAR,
	VISIT_ACTIVE,
	VISIT_COMPLETE,
};

static enum tcti_feature_artifact_error fail(
	struct tcti_feature_artifact_diagnostic *diagnostic,
	enum tcti_feature_artifact_error error, tcti_feature_artifact_u32 index)
{
	if (diagnostic) {
		diagnostic->error = error;
		diagnostic->index = index;
	}
	return error;
}

static int text_is_empty(const char *text)
{
	return !text || !text[0];
}

static int span_is_valid(const struct tcti_feature_artifact_span *span,
	tcti_feature_artifact_u32 source_length, int required)
{
	if (required && !span->length)
		return 0;
	return span->offset <= source_length &&
		span->length <= source_length - span->offset;
}

static int span_is_equal(const struct tcti_feature_artifact_span *left,
	const struct tcti_feature_artifact_span *right)
{
	return left->offset == right->offset && left->length == right->length;
}

static int span_contains(const struct tcti_feature_artifact_span *outer,
	const struct tcti_feature_artifact_span *inner)
{
	tcti_feature_artifact_u32 relative;

	if (inner->offset < outer->offset)
		return 0;
	relative = inner->offset - outer->offset;
	return relative <= outer->length &&
		inner->length <= outer->length - relative;
}

static int source_matches(const struct tcti_feature_artifact_source *source)
{
	return source->architecture && source->build && source->reference &&
		source->schema && source->sha256 &&
		!strcmp(source->architecture, TCTI_FEATURE_ARTIFACT_ARCHITECTURE) &&
		!strcmp(source->build, TCTI_FEATURE_ARTIFACT_BUILD) &&
		!strcmp(source->reference, TCTI_FEATURE_ARTIFACT_REFERENCE) &&
		!strcmp(source->schema, TCTI_FEATURE_ARTIFACT_SCHEMA) &&
		!strcmp(source->sha256, TCTI_FEATURE_ARTIFACT_SOURCE_SHA256) &&
		source->length == TCTI_FEATURE_ARTIFACT_PINNED_SOURCE_LENGTH;
}

static int reference_is_valid(tcti_feature_artifact_u32 reference,
	tcti_feature_artifact_u32 count)
{
	return reference == TCTI_FEATURE_ARTIFACT_NODE_NONE || reference < count;
}

static int no_refs_or_children(const struct tcti_feature_artifact_node *node)
{
	return node->left == TCTI_FEATURE_ARTIFACT_NODE_NONE &&
		node->right == TCTI_FEATURE_ARTIFACT_NODE_NONE &&
		node->first_child == TCTI_FEATURE_ARTIFACT_NODE_NONE &&
		!node->child_count;
}

static int child_span_is_valid(const struct tcti_feature_artifact_node *node,
	tcti_feature_artifact_u32 child_count)
{
	if (!node->child_count)
		return node->first_child == TCTI_FEATURE_ARTIFACT_NODE_NONE;
	return node->first_child != TCTI_FEATURE_ARTIFACT_NODE_NONE &&
		node->first_child < child_count &&
		node->child_count <= child_count - node->first_child;
}

static int node_payload_is_valid(const struct tcti_feature_artifact_node *node)
{
	int fields_empty = text_is_empty(node->field_state) &&
		text_is_empty(node->field_register_name) &&
		text_is_empty(node->field_selector) && !node->field_source.offset &&
		!node->field_source.length;

	if (node->kind >= TCTI_FEATURE_ARTIFACT_NODE_KIND_COUNT)
		return 0;
	switch (node->kind) {
	case TCTI_FEATURE_ARTIFACT_BOOL:
		return no_refs_or_children(node) && fields_empty &&
			text_is_empty(node->text) &&
			(node->integer == 0 || node->integer == 1);
	case TCTI_FEATURE_ARTIFACT_IDENTIFIER:
	case TCTI_FEATURE_ARTIFACT_VALUE:
		return no_refs_or_children(node) && fields_empty &&
			!text_is_empty(node->text) && !node->integer;
	case TCTI_FEATURE_ARTIFACT_INTEGER:
		return no_refs_or_children(node) && fields_empty &&
			text_is_empty(node->text);
	case TCTI_FEATURE_ARTIFACT_DOT_ATOM:
	case TCTI_FEATURE_ARTIFACT_SET:
		return node->left == TCTI_FEATURE_ARTIFACT_NODE_NONE &&
			node->right == TCTI_FEATURE_ARTIFACT_NODE_NONE &&
			node->child_count && fields_empty && text_is_empty(node->text) &&
			!node->integer;
	case TCTI_FEATURE_ARTIFACT_NOT:
		return node->left != TCTI_FEATURE_ARTIFACT_NODE_NONE &&
			node->right == TCTI_FEATURE_ARTIFACT_NODE_NONE &&
			!node->child_count && fields_empty && text_is_empty(node->text) &&
			!node->integer;
	case TCTI_FEATURE_ARTIFACT_AND:
	case TCTI_FEATURE_ARTIFACT_OR:
	case TCTI_FEATURE_ARTIFACT_EQ:
	case TCTI_FEATURE_ARTIFACT_NE:
	case TCTI_FEATURE_ARTIFACT_LT:
	case TCTI_FEATURE_ARTIFACT_GT:
	case TCTI_FEATURE_ARTIFACT_GE:
	case TCTI_FEATURE_ARTIFACT_IN:
	case TCTI_FEATURE_ARTIFACT_IMPLIES:
	case TCTI_FEATURE_ARTIFACT_IFF:
		return node->left != TCTI_FEATURE_ARTIFACT_NODE_NONE &&
			node->right != TCTI_FEATURE_ARTIFACT_NODE_NONE &&
			!node->child_count && fields_empty && text_is_empty(node->text) &&
			!node->integer;
	case TCTI_FEATURE_ARTIFACT_UINT:
	case TCTI_FEATURE_ARTIFACT_SINT:
		return node->left == TCTI_FEATURE_ARTIFACT_NODE_NONE &&
			node->right == TCTI_FEATURE_ARTIFACT_NODE_NONE &&
			node->child_count == 1 && fields_empty && text_is_empty(node->text) &&
			!node->integer;
	case TCTI_FEATURE_ARTIFACT_FIELD:
		return no_refs_or_children(node) && text_is_empty(node->text) &&
			!text_is_empty(node->field_state) &&
			!text_is_empty(node->field_register_name) &&
			!text_is_empty(node->field_selector) && !node->integer;
	case TCTI_FEATURE_ARTIFACT_NODE_KIND_COUNT:
		break;
	}
	return 0;
}

static tcti_feature_artifact_u32 node_edge(
	const struct tcti_feature_artifact *artifact,
	const struct tcti_feature_artifact_node *node,
	tcti_feature_artifact_u64 edge)
{
	if (!edge)
		return node->left;
	if (edge == 1)
		return node->right;
	return artifact->children[node->first_child +
		(tcti_feature_artifact_u32)(edge - 2)];
}

static enum tcti_feature_artifact_error visit_node(
	const struct tcti_feature_artifact *artifact,
	struct tcti_feature_artifact_scratch *scratch,
	tcti_feature_artifact_u32 index,
	struct tcti_feature_artifact_diagnostic *diagnostic)
{
	size_t depth = 0;

	if (scratch->state[index] == VISIT_COMPLETE)
		return TCTI_FEATURE_ARTIFACT_VALID;
	scratch->state[index] = VISIT_ACTIVE;
	scratch->frames[depth++] = (struct tcti_feature_artifact_frame) {
		.node_index = index,
	};
	while (depth) {
		struct tcti_feature_artifact_frame *frame =
			&scratch->frames[depth - 1];
		const struct tcti_feature_artifact_node *node =
			&artifact->nodes[frame->node_index];
		tcti_feature_artifact_u64 edge_count =
			2U + (tcti_feature_artifact_u64)node->child_count;
		tcti_feature_artifact_u32 target;

		if (frame->next_edge == edge_count) {
			scratch->state[frame->node_index] = VISIT_COMPLETE;
			depth--;
			continue;
		}
		target = node_edge(artifact, node, frame->next_edge++);
		if (target == TCTI_FEATURE_ARTIFACT_NODE_NONE)
			continue;
		if (scratch->state[target] == VISIT_ACTIVE)
			return fail(diagnostic, TCTI_FEATURE_ARTIFACT_CYCLE, target);
		if (scratch->state[target] == VISIT_COMPLETE)
			continue;
		if (depth == scratch->frame_count)
			return fail(diagnostic, TCTI_FEATURE_ARTIFACT_SCRATCH_TOO_SMALL,
				target);
		scratch->state[target] = VISIT_ACTIVE;
		scratch->frames[depth++] =
			(struct tcti_feature_artifact_frame) { .node_index = target };
	}
	return TCTI_FEATURE_ARTIFACT_VALID;
}

enum tcti_feature_artifact_error tcti_feature_artifact_validate(
	const struct tcti_feature_artifact *artifact,
	struct tcti_feature_artifact_scratch *scratch,
	struct tcti_feature_artifact_diagnostic *diagnostic)
{
	tcti_feature_artifact_u32 index;
	tcti_feature_artifact_u32 parameter_constraints = 0;
	enum tcti_feature_artifact_error error;

	if (diagnostic)
		*diagnostic = (struct tcti_feature_artifact_diagnostic) {
			.error = TCTI_FEATURE_ARTIFACT_VALID,
		};
	if (!artifact || !scratch || !scratch->state || !scratch->frames)
		return fail(diagnostic, TCTI_FEATURE_ARTIFACT_INVALID_ARGUMENT, 0);
	if (!source_matches(&artifact->source))
		return fail(diagnostic, TCTI_FEATURE_ARTIFACT_PIN_MISMATCH, 0);
	if (artifact->counts.parameter_count != TCTI_FEATURE_ARTIFACT_PARAMETER_COUNT ||
	    artifact->counts.constraint_count != TCTI_FEATURE_ARTIFACT_CONSTRAINT_COUNT ||
	    artifact->counts.parameter_constraint_count !=
		TCTI_FEATURE_ARTIFACT_PARAMETER_CONSTRAINT_COUNT ||
	    artifact->counts.global_constraint_count !=
		TCTI_FEATURE_ARTIFACT_GLOBAL_CONSTRAINT_COUNT ||
	    artifact->counts.node_count != TCTI_FEATURE_ARTIFACT_NODE_COUNT ||
	    artifact->counts.child_count != TCTI_FEATURE_ARTIFACT_CHILD_COUNT)
		return fail(diagnostic, TCTI_FEATURE_ARTIFACT_COUNT_MISMATCH, 0);
	if (scratch->state_count < artifact->counts.node_count ||
	    scratch->frame_count < artifact->counts.node_count ||
	    scratch->child_coverage_count < artifact->counts.child_count)
		return fail(diagnostic, TCTI_FEATURE_ARTIFACT_SCRATCH_TOO_SMALL, 0);
	if (!artifact->parameters || !artifact->constraints || !artifact->nodes ||
	    !artifact->children || !scratch->child_coverage)
		return fail(diagnostic, TCTI_FEATURE_ARTIFACT_POINTER_MISSING, 0);
	memset(scratch->state, VISIT_CLEAR, artifact->counts.node_count);
	memset(scratch->child_coverage, 0, artifact->counts.child_count);
	for (index = 0; index < artifact->counts.parameter_count; index++) {
		const struct tcti_feature_artifact_parameter *parameter =
			&artifact->parameters[index];
		tcti_feature_artifact_u32 previous;

		if (text_is_empty(parameter->name))
			return fail(diagnostic, TCTI_FEATURE_ARTIFACT_STRING_INVALID, index);
		if (!span_is_valid(&parameter->source, artifact->source.length, 1))
			return fail(diagnostic, TCTI_FEATURE_ARTIFACT_SOURCE_SPAN_INVALID,
				index);
		if (parameter->first_constraint != parameter_constraints ||
		    parameter->constraint_count >
			artifact->counts.parameter_constraint_count -
			parameter_constraints)
			return fail(diagnostic, TCTI_FEATURE_ARTIFACT_SCOPE_INVALID, index);
		for (previous = 0; previous < index; previous++)
			if (!strcmp(parameter->name, artifact->parameters[previous].name))
				return fail(diagnostic,
					TCTI_FEATURE_ARTIFACT_STRING_INVALID, index);
		parameter_constraints += parameter->constraint_count;
		for (previous = parameter->first_constraint;
		     previous < parameter_constraints; previous++)
			if (!span_contains(&parameter->source,
					&artifact->constraints[previous].source))
				return fail(diagnostic,
					TCTI_FEATURE_ARTIFACT_PROVENANCE_INVALID,
					previous);
	}
	if (parameter_constraints != artifact->counts.parameter_constraint_count)
		return fail(diagnostic, TCTI_FEATURE_ARTIFACT_SCOPE_INVALID, index);
	for (index = 0; index < artifact->counts.constraint_count; index++) {
		const struct tcti_feature_artifact_constraint *constraint =
			&artifact->constraints[index];

		if (constraint->node_index >= artifact->counts.node_count)
			return fail(diagnostic, TCTI_FEATURE_ARTIFACT_REFERENCE_INVALID,
				index);
		if (!span_is_valid(&constraint->source, artifact->source.length, 1))
			return fail(diagnostic, TCTI_FEATURE_ARTIFACT_SOURCE_SPAN_INVALID,
				index);
		if (!span_is_equal(&constraint->source,
				&artifact->nodes[constraint->node_index].source))
			return fail(diagnostic,
				TCTI_FEATURE_ARTIFACT_PROVENANCE_INVALID, index);
	}
	for (index = 0; index < artifact->counts.node_count; index++) {
		const struct tcti_feature_artifact_node *node = &artifact->nodes[index];
		tcti_feature_artifact_u32 child;

		if (!span_is_valid(&node->source, artifact->source.length, 1) ||
		    !span_is_valid(&node->field_source, artifact->source.length,
				node->kind == TCTI_FEATURE_ARTIFACT_FIELD))
			return fail(diagnostic, TCTI_FEATURE_ARTIFACT_SOURCE_SPAN_INVALID,
				index);
		if (!reference_is_valid(node->left, artifact->counts.node_count) ||
		    !reference_is_valid(node->right, artifact->counts.node_count) ||
		    !child_span_is_valid(node, artifact->counts.child_count))
			return fail(diagnostic, TCTI_FEATURE_ARTIFACT_REFERENCE_INVALID,
				index);
		if (!node_payload_is_valid(node))
			return fail(diagnostic, TCTI_FEATURE_ARTIFACT_NODE_INVALID, index);
		if (node->kind == TCTI_FEATURE_ARTIFACT_FIELD &&
		    !span_contains(&node->source, &node->field_source))
			return fail(diagnostic,
				TCTI_FEATURE_ARTIFACT_PROVENANCE_INVALID, index);
		for (child = 0; child < node->child_count; child++) {
			tcti_feature_artifact_u32 slot = node->first_child + child;

			if (scratch->child_coverage[slot])
				return fail(diagnostic,
					TCTI_FEATURE_ARTIFACT_CHILD_COVERAGE_INVALID,
					slot);
			scratch->child_coverage[slot] = 1;
		}
	}
	for (index = 0; index < artifact->counts.child_count; index++)
		if (!scratch->child_coverage[index])
			return fail(diagnostic,
				TCTI_FEATURE_ARTIFACT_CHILD_COVERAGE_INVALID, index);
		else if (artifact->children[index] >= artifact->counts.node_count)
			return fail(diagnostic, TCTI_FEATURE_ARTIFACT_REFERENCE_INVALID,
				index);
	for (index = 0; index < artifact->counts.constraint_count; index++) {
		error = visit_node(artifact, scratch,
			artifact->constraints[index].node_index, diagnostic);
		if (error != TCTI_FEATURE_ARTIFACT_VALID)
			return error;
	}
	for (index = 0; index < artifact->counts.node_count; index++)
		if (scratch->state[index] != VISIT_COMPLETE)
			return fail(diagnostic,
				TCTI_FEATURE_ARTIFACT_GRAPH_COVERAGE_INVALID, index);
	return TCTI_FEATURE_ARTIFACT_VALID;
}

const char *tcti_feature_artifact_error_name(
	enum tcti_feature_artifact_error error)
{
	switch (error) {
	case TCTI_FEATURE_ARTIFACT_VALID: return "valid";
	case TCTI_FEATURE_ARTIFACT_INVALID_ARGUMENT: return "invalid argument";
	case TCTI_FEATURE_ARTIFACT_PIN_MISMATCH: return "pin mismatch";
	case TCTI_FEATURE_ARTIFACT_COUNT_MISMATCH: return "count mismatch";
	case TCTI_FEATURE_ARTIFACT_POINTER_MISSING: return "pointer missing";
	case TCTI_FEATURE_ARTIFACT_STRING_INVALID: return "invalid string";
	case TCTI_FEATURE_ARTIFACT_SOURCE_SPAN_INVALID: return "invalid source span";
	case TCTI_FEATURE_ARTIFACT_SCOPE_INVALID: return "invalid constraint scope";
	case TCTI_FEATURE_ARTIFACT_REFERENCE_INVALID: return "invalid reference";
	case TCTI_FEATURE_ARTIFACT_NODE_INVALID: return "invalid node payload";
	case TCTI_FEATURE_ARTIFACT_CYCLE: return "cycle";
	case TCTI_FEATURE_ARTIFACT_GRAPH_COVERAGE_INVALID:
		return "invalid graph coverage";
	case TCTI_FEATURE_ARTIFACT_CHILD_COVERAGE_INVALID:
		return "invalid child coverage";
	case TCTI_FEATURE_ARTIFACT_PROVENANCE_INVALID:
		return "invalid provenance";
	case TCTI_FEATURE_ARTIFACT_SCRATCH_TOO_SMALL: return "scratch too small";
	}
	return "unknown";
}
