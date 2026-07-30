/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_feature_artifact.h"

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif

#define ORLIX_TCTI_FEATURE_CANONICAL_DEF "isa/target_feature_artifact.def"

#define ORLIX_TCTI_A64_FEATURE_ARTIFACT_SOURCE(...)
#define ORLIX_TCTI_A64_FEATURE_ARTIFACT_COUNTS(...)
#define ORLIX_TCTI_A64_FEATURE_PARAMETER(i, n, f, c, o, l) \
	[i] = { n, f, c, { o, l } },
#define ORLIX_TCTI_A64_FEATURE_CONSTRAINT(...)
#define ORLIX_TCTI_A64_FEATURE_NODE(...)
#define ORLIX_TCTI_A64_FEATURE_CHILD(...)
static const struct orlix_tcti_feature_artifact_parameter canonical_parameters[] = {
#include ORLIX_TCTI_FEATURE_CANONICAL_DEF
};
#undef ORLIX_TCTI_A64_FEATURE_ARTIFACT_SOURCE
#undef ORLIX_TCTI_A64_FEATURE_ARTIFACT_COUNTS
#undef ORLIX_TCTI_A64_FEATURE_PARAMETER
#undef ORLIX_TCTI_A64_FEATURE_CONSTRAINT
#undef ORLIX_TCTI_A64_FEATURE_NODE
#undef ORLIX_TCTI_A64_FEATURE_CHILD

#define ORLIX_TCTI_A64_FEATURE_ARTIFACT_SOURCE(...)
#define ORLIX_TCTI_A64_FEATURE_ARTIFACT_COUNTS(...)
#define ORLIX_TCTI_A64_FEATURE_PARAMETER(...)
#define ORLIX_TCTI_A64_FEATURE_CONSTRAINT(i, n, o, l) [i] = { n, { o, l } },
#define ORLIX_TCTI_A64_FEATURE_NODE(...)
#define ORLIX_TCTI_A64_FEATURE_CHILD(...)
static const struct orlix_tcti_feature_artifact_constraint canonical_constraints[] = {
#include ORLIX_TCTI_FEATURE_CANONICAL_DEF
};
#undef ORLIX_TCTI_A64_FEATURE_ARTIFACT_SOURCE
#undef ORLIX_TCTI_A64_FEATURE_ARTIFACT_COUNTS
#undef ORLIX_TCTI_A64_FEATURE_PARAMETER
#undef ORLIX_TCTI_A64_FEATURE_CONSTRAINT
#undef ORLIX_TCTI_A64_FEATURE_NODE
#undef ORLIX_TCTI_A64_FEATURE_CHILD

#define ORLIX_TCTI_A64_FEATURE_ARTIFACT_SOURCE(...)
#define ORLIX_TCTI_A64_FEATURE_ARTIFACT_COUNTS(...)
#define ORLIX_TCTI_A64_FEATURE_PARAMETER(...)
#define ORLIX_TCTI_A64_FEATURE_CONSTRAINT(...)
#define ORLIX_TCTI_A64_FEATURE_NODE(i, k, left, right, first, count, integer, text, \
	state, reg, selector, field_o, field_l, instance_k, instance_o, instance_l, \
	slices_k, slices_o, slices_l, o, l) \
	[i] = { k, left, right, first, count, integer, text, state, reg, selector, \
		{ field_o, field_l }, { instance_k, { instance_o, instance_l } }, \
		{ slices_k, { slices_o, slices_l } }, { o, l } },
#define ORLIX_TCTI_A64_FEATURE_CHILD(...)
static const struct orlix_tcti_feature_artifact_node canonical_nodes[] = {
#include ORLIX_TCTI_FEATURE_CANONICAL_DEF
};
#undef ORLIX_TCTI_A64_FEATURE_ARTIFACT_SOURCE
#undef ORLIX_TCTI_A64_FEATURE_ARTIFACT_COUNTS
#undef ORLIX_TCTI_A64_FEATURE_PARAMETER
#undef ORLIX_TCTI_A64_FEATURE_CONSTRAINT
#undef ORLIX_TCTI_A64_FEATURE_NODE
#undef ORLIX_TCTI_A64_FEATURE_CHILD

#define ORLIX_TCTI_A64_FEATURE_ARTIFACT_SOURCE(...)
#define ORLIX_TCTI_A64_FEATURE_ARTIFACT_COUNTS(...)
#define ORLIX_TCTI_A64_FEATURE_PARAMETER(...)
#define ORLIX_TCTI_A64_FEATURE_CONSTRAINT(...)
#define ORLIX_TCTI_A64_FEATURE_NODE(...)
#define ORLIX_TCTI_A64_FEATURE_CHILD(i, n) [i] = n,
static const orlix_tcti_feature_artifact_u32 canonical_children[] = {
#include ORLIX_TCTI_FEATURE_CANONICAL_DEF
};
#undef ORLIX_TCTI_A64_FEATURE_ARTIFACT_SOURCE
#undef ORLIX_TCTI_A64_FEATURE_ARTIFACT_COUNTS
#undef ORLIX_TCTI_A64_FEATURE_PARAMETER
#undef ORLIX_TCTI_A64_FEATURE_CONSTRAINT
#undef ORLIX_TCTI_A64_FEATURE_NODE
#undef ORLIX_TCTI_A64_FEATURE_CHILD

static const struct orlix_tcti_feature_artifact canonical_artifact = {
#define ORLIX_TCTI_A64_FEATURE_ARTIFACT_SOURCE(a, b, r, s, h, l) \
	.source = { a, b, r, s, h, l },
#define ORLIX_TCTI_A64_FEATURE_ARTIFACT_COUNTS(p, c, pc, n, ch, gc) \
	.counts = { p, c, pc, n, ch, gc },
#define ORLIX_TCTI_A64_FEATURE_PARAMETER(...)
#define ORLIX_TCTI_A64_FEATURE_CONSTRAINT(...)
#define ORLIX_TCTI_A64_FEATURE_NODE(...)
#define ORLIX_TCTI_A64_FEATURE_CHILD(...)
#include ORLIX_TCTI_FEATURE_CANONICAL_DEF
#undef ORLIX_TCTI_A64_FEATURE_ARTIFACT_SOURCE
#undef ORLIX_TCTI_A64_FEATURE_ARTIFACT_COUNTS
#undef ORLIX_TCTI_A64_FEATURE_PARAMETER
#undef ORLIX_TCTI_A64_FEATURE_CONSTRAINT
#undef ORLIX_TCTI_A64_FEATURE_NODE
#undef ORLIX_TCTI_A64_FEATURE_CHILD
	.parameters = canonical_parameters,
	.constraints = canonical_constraints,
	.nodes = canonical_nodes,
	.children = canonical_children,
};

const struct orlix_tcti_feature_artifact *orlix_tcti_feature_artifact_canonical(void)
{
	return &canonical_artifact;
}

enum visit_state {
	VISIT_CLEAR,
	VISIT_ACTIVE,
	VISIT_COMPLETE,
};

static enum orlix_tcti_feature_artifact_error fail(
	struct orlix_tcti_feature_artifact_diagnostic *diagnostic,
	enum orlix_tcti_feature_artifact_error error, orlix_tcti_feature_artifact_u32 index)
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

static int span_is_valid(const struct orlix_tcti_feature_artifact_span *span,
	orlix_tcti_feature_artifact_u32 source_length, int required)
{
	if (required && !span->length)
		return 0;
	return span->offset <= source_length &&
		span->length <= source_length - span->offset;
}

static int span_is_equal(const struct orlix_tcti_feature_artifact_span *left,
	const struct orlix_tcti_feature_artifact_span *right)
{
	return left->offset == right->offset && left->length == right->length;
}

static int span_contains(const struct orlix_tcti_feature_artifact_span *outer,
	const struct orlix_tcti_feature_artifact_span *inner)
{
	orlix_tcti_feature_artifact_u32 relative;

	if (inner->offset < outer->offset)
		return 0;
	relative = inner->offset - outer->offset;
	return relative <= outer->length &&
		inner->length <= outer->length - relative;
}

static int source_matches(const struct orlix_tcti_feature_artifact_source *source)
{
	return source->architecture && source->build && source->reference &&
		source->schema && source->sha256 &&
		!strcmp(source->architecture, ORLIX_TCTI_FEATURE_ARTIFACT_ARCHITECTURE) &&
		!strcmp(source->build, ORLIX_TCTI_FEATURE_ARTIFACT_BUILD) &&
		!strcmp(source->reference, ORLIX_TCTI_FEATURE_ARTIFACT_REFERENCE) &&
		!strcmp(source->schema, ORLIX_TCTI_FEATURE_ARTIFACT_SCHEMA) &&
		!strcmp(source->sha256, ORLIX_TCTI_FEATURE_ARTIFACT_SOURCE_SHA256) &&
		source->length == ORLIX_TCTI_FEATURE_ARTIFACT_PINNED_SOURCE_LENGTH;
}

static int reference_is_valid(orlix_tcti_feature_artifact_u32 reference,
	orlix_tcti_feature_artifact_u32 count)
{
	return reference == ORLIX_TCTI_FEATURE_ARTIFACT_NODE_NONE || reference < count;
}

static int no_refs_or_children(const struct orlix_tcti_feature_artifact_node *node)
{
	return node->left == ORLIX_TCTI_FEATURE_ARTIFACT_NODE_NONE &&
		node->right == ORLIX_TCTI_FEATURE_ARTIFACT_NODE_NONE &&
		node->first_child == ORLIX_TCTI_FEATURE_ARTIFACT_NODE_NONE &&
		!node->child_count;
}

static int child_span_is_valid(const struct orlix_tcti_feature_artifact_node *node,
	orlix_tcti_feature_artifact_u32 child_count)
{
	if (!node->child_count)
		return node->first_child == ORLIX_TCTI_FEATURE_ARTIFACT_NODE_NONE;
	return node->first_child != ORLIX_TCTI_FEATURE_ARTIFACT_NODE_NONE &&
		node->first_child < child_count &&
		node->child_count <= child_count - node->first_child;
}

static int node_payload_is_valid(const struct orlix_tcti_feature_artifact_node *node)
{
	int fields_empty = text_is_empty(node->field_state) &&
		text_is_empty(node->field_register_name) &&
		text_is_empty(node->field_selector) && !node->field_source.offset &&
		!node->field_source.length &&
		node->field_instance.kind == ORLIX_TCTI_FEATURE_ARTIFACT_FIELD_QUALIFIER_NONE &&
		!node->field_instance.source.offset &&
		!node->field_instance.source.length &&
		node->field_slices.kind == ORLIX_TCTI_FEATURE_ARTIFACT_FIELD_QUALIFIER_NONE &&
		!node->field_slices.source.offset &&
		!node->field_slices.source.length;

	if (node->kind >= ORLIX_TCTI_FEATURE_ARTIFACT_NODE_KIND_COUNT)
		return 0;
	switch (node->kind) {
	case ORLIX_TCTI_FEATURE_ARTIFACT_BOOL:
		return no_refs_or_children(node) && fields_empty &&
			text_is_empty(node->text) &&
			(node->integer == 0 || node->integer == 1);
	case ORLIX_TCTI_FEATURE_ARTIFACT_IDENTIFIER:
	case ORLIX_TCTI_FEATURE_ARTIFACT_VALUE:
		return no_refs_or_children(node) && fields_empty &&
			!text_is_empty(node->text) && !node->integer;
	case ORLIX_TCTI_FEATURE_ARTIFACT_INTEGER:
		return no_refs_or_children(node) && fields_empty &&
			text_is_empty(node->text);
	case ORLIX_TCTI_FEATURE_ARTIFACT_DOT_ATOM:
	case ORLIX_TCTI_FEATURE_ARTIFACT_SET:
		return node->left == ORLIX_TCTI_FEATURE_ARTIFACT_NODE_NONE &&
			node->right == ORLIX_TCTI_FEATURE_ARTIFACT_NODE_NONE &&
			node->child_count && fields_empty && text_is_empty(node->text) &&
			!node->integer;
	case ORLIX_TCTI_FEATURE_ARTIFACT_NOT:
		return node->left != ORLIX_TCTI_FEATURE_ARTIFACT_NODE_NONE &&
			node->right == ORLIX_TCTI_FEATURE_ARTIFACT_NODE_NONE &&
			!node->child_count && fields_empty && text_is_empty(node->text) &&
			!node->integer;
	case ORLIX_TCTI_FEATURE_ARTIFACT_AND:
	case ORLIX_TCTI_FEATURE_ARTIFACT_OR:
	case ORLIX_TCTI_FEATURE_ARTIFACT_EQ:
	case ORLIX_TCTI_FEATURE_ARTIFACT_NE:
	case ORLIX_TCTI_FEATURE_ARTIFACT_LT:
	case ORLIX_TCTI_FEATURE_ARTIFACT_GT:
	case ORLIX_TCTI_FEATURE_ARTIFACT_GE:
	case ORLIX_TCTI_FEATURE_ARTIFACT_IN:
	case ORLIX_TCTI_FEATURE_ARTIFACT_IMPLIES:
	case ORLIX_TCTI_FEATURE_ARTIFACT_IFF:
		return node->left != ORLIX_TCTI_FEATURE_ARTIFACT_NODE_NONE &&
			node->right != ORLIX_TCTI_FEATURE_ARTIFACT_NODE_NONE &&
			!node->child_count && fields_empty && text_is_empty(node->text) &&
			!node->integer;
	case ORLIX_TCTI_FEATURE_ARTIFACT_UINT:
	case ORLIX_TCTI_FEATURE_ARTIFACT_SINT:
		return node->left == ORLIX_TCTI_FEATURE_ARTIFACT_NODE_NONE &&
			node->right == ORLIX_TCTI_FEATURE_ARTIFACT_NODE_NONE &&
			node->child_count == 1 && fields_empty && text_is_empty(node->text) &&
			!node->integer;
	case ORLIX_TCTI_FEATURE_ARTIFACT_FIELD:
		return no_refs_or_children(node) && text_is_empty(node->text) &&
			!text_is_empty(node->field_state) &&
			!text_is_empty(node->field_register_name) &&
			!text_is_empty(node->field_selector) && !node->integer &&
			node->field_instance.kind ==
				ORLIX_TCTI_FEATURE_ARTIFACT_FIELD_QUALIFIER_NULL &&
			node->field_slices.kind ==
				ORLIX_TCTI_FEATURE_ARTIFACT_FIELD_QUALIFIER_NULL;
	case ORLIX_TCTI_FEATURE_ARTIFACT_NODE_KIND_COUNT:
		break;
	}
	return 0;
}

static orlix_tcti_feature_artifact_u32 node_edge(
	const struct orlix_tcti_feature_artifact *artifact,
	const struct orlix_tcti_feature_artifact_node *node,
	orlix_tcti_feature_artifact_u64 edge)
{
	if (!edge)
		return node->left;
	if (edge == 1)
		return node->right;
	return artifact->children[node->first_child +
		(orlix_tcti_feature_artifact_u32)(edge - 2)];
}

static enum orlix_tcti_feature_artifact_error visit_node(
	const struct orlix_tcti_feature_artifact *artifact,
	struct orlix_tcti_feature_artifact_scratch *scratch,
	orlix_tcti_feature_artifact_u32 index,
	struct orlix_tcti_feature_artifact_diagnostic *diagnostic)
{
	size_t depth = 0;

	if (scratch->state[index] == VISIT_COMPLETE)
		return ORLIX_TCTI_FEATURE_ARTIFACT_VALID;
	scratch->state[index] = VISIT_ACTIVE;
	scratch->frames[depth++] = (struct orlix_tcti_feature_artifact_frame) {
		.node_index = index,
	};
	while (depth) {
		struct orlix_tcti_feature_artifact_frame *frame =
			&scratch->frames[depth - 1];
		const struct orlix_tcti_feature_artifact_node *node =
			&artifact->nodes[frame->node_index];
		orlix_tcti_feature_artifact_u64 edge_count =
			2U + (orlix_tcti_feature_artifact_u64)node->child_count;
		orlix_tcti_feature_artifact_u32 target;

		if (frame->next_edge == edge_count) {
			scratch->state[frame->node_index] = VISIT_COMPLETE;
			depth--;
			continue;
		}
		target = node_edge(artifact, node, frame->next_edge++);
		if (target == ORLIX_TCTI_FEATURE_ARTIFACT_NODE_NONE)
			continue;
		if (scratch->state[target] == VISIT_ACTIVE)
			return fail(diagnostic, ORLIX_TCTI_FEATURE_ARTIFACT_CYCLE, target);
		if (scratch->state[target] == VISIT_COMPLETE)
			continue;
		if (depth == scratch->frame_count)
			return fail(diagnostic, ORLIX_TCTI_FEATURE_ARTIFACT_SCRATCH_TOO_SMALL,
				target);
		scratch->state[target] = VISIT_ACTIVE;
		scratch->frames[depth++] =
			(struct orlix_tcti_feature_artifact_frame) { .node_index = target };
	}
	return ORLIX_TCTI_FEATURE_ARTIFACT_VALID;
}

enum orlix_tcti_feature_artifact_error orlix_tcti_feature_artifact_validate(
	const struct orlix_tcti_feature_artifact *artifact,
	struct orlix_tcti_feature_artifact_scratch *scratch,
	struct orlix_tcti_feature_artifact_diagnostic *diagnostic)
{
	orlix_tcti_feature_artifact_u32 index;
	orlix_tcti_feature_artifact_u32 parameter_constraints = 0;
	enum orlix_tcti_feature_artifact_error error;

	if (diagnostic)
		*diagnostic = (struct orlix_tcti_feature_artifact_diagnostic) {
			.error = ORLIX_TCTI_FEATURE_ARTIFACT_VALID,
		};
	if (!artifact || !scratch || !scratch->state || !scratch->frames)
		return fail(diagnostic, ORLIX_TCTI_FEATURE_ARTIFACT_INVALID_ARGUMENT, 0);
	if (!source_matches(&artifact->source))
		return fail(diagnostic, ORLIX_TCTI_FEATURE_ARTIFACT_PIN_MISMATCH, 0);
	if (artifact->counts.parameter_count != ORLIX_TCTI_FEATURE_ARTIFACT_PARAMETER_COUNT ||
	    artifact->counts.constraint_count != ORLIX_TCTI_FEATURE_ARTIFACT_CONSTRAINT_COUNT ||
	    artifact->counts.parameter_constraint_count !=
		ORLIX_TCTI_FEATURE_ARTIFACT_PARAMETER_CONSTRAINT_COUNT ||
	    artifact->counts.global_constraint_count !=
		ORLIX_TCTI_FEATURE_ARTIFACT_GLOBAL_CONSTRAINT_COUNT ||
	    artifact->counts.node_count != ORLIX_TCTI_FEATURE_ARTIFACT_NODE_COUNT ||
	    artifact->counts.child_count != ORLIX_TCTI_FEATURE_ARTIFACT_CHILD_COUNT)
		return fail(diagnostic, ORLIX_TCTI_FEATURE_ARTIFACT_COUNT_MISMATCH, 0);
	if (scratch->state_count < artifact->counts.node_count ||
	    scratch->frame_count < artifact->counts.node_count ||
	    scratch->child_coverage_count < artifact->counts.child_count)
		return fail(diagnostic, ORLIX_TCTI_FEATURE_ARTIFACT_SCRATCH_TOO_SMALL, 0);
	if (!artifact->parameters || !artifact->constraints || !artifact->nodes ||
	    !artifact->children || !scratch->child_coverage)
		return fail(diagnostic, ORLIX_TCTI_FEATURE_ARTIFACT_POINTER_MISSING, 0);
	memset(scratch->state, VISIT_CLEAR, artifact->counts.node_count);
	memset(scratch->child_coverage, 0, artifact->counts.child_count);
	for (index = 0; index < artifact->counts.parameter_count; index++) {
		const struct orlix_tcti_feature_artifact_parameter *parameter =
			&artifact->parameters[index];
		orlix_tcti_feature_artifact_u32 previous;

		if (text_is_empty(parameter->name))
			return fail(diagnostic, ORLIX_TCTI_FEATURE_ARTIFACT_STRING_INVALID, index);
		if (!span_is_valid(&parameter->source, artifact->source.length, 1))
			return fail(diagnostic, ORLIX_TCTI_FEATURE_ARTIFACT_SOURCE_SPAN_INVALID,
				index);
		if (parameter->first_constraint != parameter_constraints ||
		    parameter->constraint_count >
			artifact->counts.parameter_constraint_count -
			parameter_constraints)
			return fail(diagnostic, ORLIX_TCTI_FEATURE_ARTIFACT_SCOPE_INVALID, index);
		for (previous = 0; previous < index; previous++)
			if (!strcmp(parameter->name, artifact->parameters[previous].name))
				return fail(diagnostic,
					ORLIX_TCTI_FEATURE_ARTIFACT_STRING_INVALID, index);
		parameter_constraints += parameter->constraint_count;
		for (previous = parameter->first_constraint;
		     previous < parameter_constraints; previous++)
			if (!span_contains(&parameter->source,
					&artifact->constraints[previous].source))
				return fail(diagnostic,
					ORLIX_TCTI_FEATURE_ARTIFACT_PROVENANCE_INVALID,
					previous);
	}
	if (parameter_constraints != artifact->counts.parameter_constraint_count)
		return fail(diagnostic, ORLIX_TCTI_FEATURE_ARTIFACT_SCOPE_INVALID, index);
	for (index = 0; index < artifact->counts.constraint_count; index++) {
		const struct orlix_tcti_feature_artifact_constraint *constraint =
			&artifact->constraints[index];

		if (constraint->node_index >= artifact->counts.node_count)
			return fail(diagnostic, ORLIX_TCTI_FEATURE_ARTIFACT_REFERENCE_INVALID,
				index);
		if (!span_is_valid(&constraint->source, artifact->source.length, 1))
			return fail(diagnostic, ORLIX_TCTI_FEATURE_ARTIFACT_SOURCE_SPAN_INVALID,
				index);
		if (!span_is_equal(&constraint->source,
				&artifact->nodes[constraint->node_index].source))
			return fail(diagnostic,
				ORLIX_TCTI_FEATURE_ARTIFACT_PROVENANCE_INVALID, index);
	}
	for (index = 0; index < artifact->counts.node_count; index++) {
		const struct orlix_tcti_feature_artifact_node *node = &artifact->nodes[index];
		orlix_tcti_feature_artifact_u32 child;

		if (!span_is_valid(&node->source, artifact->source.length, 1) ||
		    !span_is_valid(&node->field_source, artifact->source.length,
				node->kind == ORLIX_TCTI_FEATURE_ARTIFACT_FIELD) ||
		    !span_is_valid(&node->field_instance.source,
				artifact->source.length,
				node->kind == ORLIX_TCTI_FEATURE_ARTIFACT_FIELD) ||
		    !span_is_valid(&node->field_slices.source,
				artifact->source.length,
				node->kind == ORLIX_TCTI_FEATURE_ARTIFACT_FIELD))
			return fail(diagnostic, ORLIX_TCTI_FEATURE_ARTIFACT_SOURCE_SPAN_INVALID,
				index);
		if (!reference_is_valid(node->left, artifact->counts.node_count) ||
		    !reference_is_valid(node->right, artifact->counts.node_count) ||
		    !child_span_is_valid(node, artifact->counts.child_count))
			return fail(diagnostic, ORLIX_TCTI_FEATURE_ARTIFACT_REFERENCE_INVALID,
				index);
		if (!node_payload_is_valid(node))
			return fail(diagnostic, ORLIX_TCTI_FEATURE_ARTIFACT_NODE_INVALID, index);
		if (node->kind == ORLIX_TCTI_FEATURE_ARTIFACT_FIELD &&
		    (!span_contains(&node->source, &node->field_source) ||
		     !span_contains(&node->field_source,
			     &node->field_instance.source) ||
		     !span_contains(&node->field_source,
			     &node->field_slices.source)))
			return fail(diagnostic,
				ORLIX_TCTI_FEATURE_ARTIFACT_PROVENANCE_INVALID, index);
		for (child = 0; child < node->child_count; child++) {
			orlix_tcti_feature_artifact_u32 slot = node->first_child + child;

			if (scratch->child_coverage[slot])
				return fail(diagnostic,
					ORLIX_TCTI_FEATURE_ARTIFACT_CHILD_COVERAGE_INVALID,
					slot);
			scratch->child_coverage[slot] = 1;
		}
	}
	for (index = 0; index < artifact->counts.child_count; index++)
		if (!scratch->child_coverage[index])
			return fail(diagnostic,
				ORLIX_TCTI_FEATURE_ARTIFACT_CHILD_COVERAGE_INVALID, index);
		else if (artifact->children[index] >= artifact->counts.node_count)
			return fail(diagnostic, ORLIX_TCTI_FEATURE_ARTIFACT_REFERENCE_INVALID,
				index);
	for (index = 0; index < artifact->counts.constraint_count; index++) {
		error = visit_node(artifact, scratch,
			artifact->constraints[index].node_index, diagnostic);
		if (error != ORLIX_TCTI_FEATURE_ARTIFACT_VALID)
			return error;
	}
	for (index = 0; index < artifact->counts.node_count; index++)
		if (scratch->state[index] != VISIT_COMPLETE)
			return fail(diagnostic,
				ORLIX_TCTI_FEATURE_ARTIFACT_GRAPH_COVERAGE_INVALID, index);
	return ORLIX_TCTI_FEATURE_ARTIFACT_VALID;
}

const char *orlix_tcti_feature_artifact_error_name(
	enum orlix_tcti_feature_artifact_error error)
{
	switch (error) {
	case ORLIX_TCTI_FEATURE_ARTIFACT_VALID: return "valid";
	case ORLIX_TCTI_FEATURE_ARTIFACT_INVALID_ARGUMENT: return "invalid argument";
	case ORLIX_TCTI_FEATURE_ARTIFACT_PIN_MISMATCH: return "pin mismatch";
	case ORLIX_TCTI_FEATURE_ARTIFACT_COUNT_MISMATCH: return "count mismatch";
	case ORLIX_TCTI_FEATURE_ARTIFACT_POINTER_MISSING: return "pointer missing";
	case ORLIX_TCTI_FEATURE_ARTIFACT_STRING_INVALID: return "invalid string";
	case ORLIX_TCTI_FEATURE_ARTIFACT_SOURCE_SPAN_INVALID: return "invalid source span";
	case ORLIX_TCTI_FEATURE_ARTIFACT_SCOPE_INVALID: return "invalid constraint scope";
	case ORLIX_TCTI_FEATURE_ARTIFACT_REFERENCE_INVALID: return "invalid reference";
	case ORLIX_TCTI_FEATURE_ARTIFACT_NODE_INVALID: return "invalid node payload";
	case ORLIX_TCTI_FEATURE_ARTIFACT_CYCLE: return "cycle";
	case ORLIX_TCTI_FEATURE_ARTIFACT_GRAPH_COVERAGE_INVALID:
		return "invalid graph coverage";
	case ORLIX_TCTI_FEATURE_ARTIFACT_CHILD_COVERAGE_INVALID:
		return "invalid child coverage";
	case ORLIX_TCTI_FEATURE_ARTIFACT_PROVENANCE_INVALID:
		return "invalid provenance";
	case ORLIX_TCTI_FEATURE_ARTIFACT_SCRATCH_TOO_SMALL: return "scratch too small";
	}
	return "unknown";
}
