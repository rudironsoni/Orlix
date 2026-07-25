/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_feature_field_domain_binding_artifact.h"

#ifdef __KERNEL__
#include <linux/string.h>
/* The checked .def uses the standard C spelling, which Linux does not export. */
#ifndef UINT64_C
#define UINT64_C(value) value ## ULL
#endif
#else
#include <string.h>
#endif

#define TCTI_FEATURE_FIELD_DOMAIN_BINDING_CANONICAL_DEF \
	"../isa/target_feature_field_domain_binding.def"

#define TCTI_A64_FEATURE_FIELD_DOMAIN_SOURCE(...)
#define TCTI_A64_FEATURE_FIELD_DOMAIN_COUNTS(...)
#define TCTI_A64_FEATURE_FIELD_DOMAIN_IDENTITY(...)
#define TCTI_A64_FEATURE_FIELD_DOMAIN_OCCURRENCE(order, feature_node, group, \
	members, register_index, field_index, value_relation_index, disposition, \
	state, register_name, selector, instance_kind, instance_offset, \
	instance_length, slices_kind, slices_offset, slices_length, \
	feature_offset, feature_length, register_offset, register_length, \
	field_offset, field_length, value_relation_offset, value_relation_length) \
	[order] = { order, feature_node, group, members, register_index, \
		field_index, value_relation_index, disposition, state, register_name, \
		selector, { instance_kind, { instance_offset, instance_length } }, \
		{ slices_kind, { slices_offset, slices_length } }, \
		{ feature_offset, feature_length }, \
		{ register_offset, register_length }, { field_offset, field_length }, \
		{ value_relation_offset, value_relation_length } },
static const struct tcti_feature_field_domain_binding canonical_bindings[] = {
#include TCTI_FEATURE_FIELD_DOMAIN_BINDING_CANONICAL_DEF
};
#undef TCTI_A64_FEATURE_FIELD_DOMAIN_SOURCE
#undef TCTI_A64_FEATURE_FIELD_DOMAIN_COUNTS
#undef TCTI_A64_FEATURE_FIELD_DOMAIN_IDENTITY
#undef TCTI_A64_FEATURE_FIELD_DOMAIN_OCCURRENCE

static const struct tcti_feature_field_domain_binding_artifact
	canonical_artifact = {
#define TCTI_A64_FEATURE_FIELD_DOMAIN_SOURCE(a, b, r, s, h, l, rh, rl) \
	.source = { a, b, r, s, h, l }, \
	.register_source_sha256 = rh, .register_source_length = rl,
#define TCTI_A64_FEATURE_FIELD_DOMAIN_COUNTS(occurrences, groups, mapped, \
	ambiguous_field) \
	.occurrence_count = occurrences, .identity_group_count = groups, \
	.mapped_count = mapped, .ambiguous_field_count = ambiguous_field,
#define TCTI_A64_FEATURE_FIELD_DOMAIN_IDENTITY(value) .identity = value,
#define TCTI_A64_FEATURE_FIELD_DOMAIN_OCCURRENCE(...)
#include TCTI_FEATURE_FIELD_DOMAIN_BINDING_CANONICAL_DEF
#undef TCTI_A64_FEATURE_FIELD_DOMAIN_SOURCE
#undef TCTI_A64_FEATURE_FIELD_DOMAIN_COUNTS
#undef TCTI_A64_FEATURE_FIELD_DOMAIN_IDENTITY
#undef TCTI_A64_FEATURE_FIELD_DOMAIN_OCCURRENCE
	.bindings = canonical_bindings,
};

const struct tcti_feature_field_domain_binding_artifact *
tcti_feature_field_domain_binding_artifact_canonical(void)
{
	return &canonical_artifact;
}

static enum tcti_feature_field_domain_binding_error fail(
	struct tcti_feature_field_domain_binding_diagnostic *diagnostic,
	enum tcti_feature_field_domain_binding_error error,
	tcti_feature_artifact_u32 index)
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

static int span_is_equal(const struct tcti_feature_artifact_span *left,
	const struct tcti_feature_artifact_span *right)
{
	return left->offset == right->offset && left->length == right->length;
}

static int qualifier_is_equal(
	const struct tcti_feature_artifact_field_qualifier *left,
	const struct tcti_feature_artifact_field_qualifier *right)
{
	return left->kind == right->kind &&
		span_is_equal(&left->source, &right->source);
}

static int binding_is_equal(
	const struct tcti_feature_field_domain_binding *left,
	const struct tcti_feature_field_domain_binding *right)
{
	return left->order == right->order &&
		left->feature_node_index == right->feature_node_index &&
		left->identity_group_index == right->identity_group_index &&
		left->occurrence_count == right->occurrence_count &&
		left->register_index == right->register_index &&
		left->field_index == right->field_index &&
		left->value_relation_index == right->value_relation_index &&
		left->disposition == right->disposition &&
		!strcmp(left->field_state, right->field_state) &&
		!strcmp(left->field_register_name, right->field_register_name) &&
		!strcmp(left->field_selector, right->field_selector) &&
		qualifier_is_equal(&left->field_instance, &right->field_instance) &&
		qualifier_is_equal(&left->field_slices, &right->field_slices) &&
		span_is_equal(&left->feature_source, &right->feature_source) &&
		span_is_equal(&left->register_source, &right->register_source) &&
		span_is_equal(&left->field_source, &right->field_source) &&
		span_is_equal(&left->value_relation_source,
			       &right->value_relation_source);
}

static int source_is_equal(const struct tcti_feature_artifact_source *left,
	const struct tcti_feature_artifact_source *right)
{
	return left->architecture && right->architecture && left->build &&
		right->build && left->reference && right->reference && left->schema &&
		right->schema && left->sha256 && right->sha256 &&
		!strcmp(left->architecture, right->architecture) &&
		!strcmp(left->build, right->build) &&
		!strcmp(left->reference, right->reference) &&
		!strcmp(left->schema, right->schema) &&
		!strcmp(left->sha256, right->sha256) &&
		left->length == right->length;
}

static int span_is_valid(const struct tcti_feature_artifact_span *span,
	tcti_feature_artifact_u32 source_length, int required)
{
	if (required && !span->length)
		return 0;
	return span->offset <= source_length &&
		span->length <= source_length - span->offset;
}

static int span_contains(const struct tcti_feature_artifact_span *outer,
	const struct tcti_feature_artifact_span *inner)
{
	tcti_feature_artifact_u32 relative;

	if (inner->offset < outer->offset)
		return 0;
	relative = inner->offset - outer->offset;
	return relative <= outer->length && inner->length <= outer->length - relative;
}

static tcti_feature_artifact_u64 fnv1a_byte(tcti_feature_artifact_u64 hash,
	tcti_feature_artifact_u8 byte)
{
	return (hash ^ byte) * TCTI_FEATURE_FIELD_DOMAIN_BINDING_FNV1A_PRIME;
}

static tcti_feature_artifact_u64 fnv1a_u64(tcti_feature_artifact_u64 hash,
	tcti_feature_artifact_u64 value)
{
	tcti_feature_artifact_u32 index;

	for (index = 0; index < 8; index++)
		hash = fnv1a_byte(hash,
			(tcti_feature_artifact_u8)(value >> (index * 8)));
	return hash;
}

static tcti_feature_artifact_u64 fnv1a_text(tcti_feature_artifact_u64 hash,
	const char *text)
{
	tcti_feature_artifact_u64 length = 0;

	while (text[length])
		length++;
	hash = fnv1a_u64(hash, length);
	while (*text)
		hash = fnv1a_byte(hash, (tcti_feature_artifact_u8)*text++);
	return hash;
}

static tcti_feature_artifact_u64 fnv1a_span(tcti_feature_artifact_u64 hash,
	const struct tcti_feature_artifact_span *span)
{
	hash = fnv1a_u64(hash, span->offset);
	return fnv1a_u64(hash, span->length);
}

static tcti_feature_artifact_u64 fnv1a_qualifier(
	tcti_feature_artifact_u64 hash,
	const struct tcti_feature_artifact_field_qualifier *qualifier)
{
	hash = fnv1a_u64(hash, qualifier->kind);
	return fnv1a_span(hash, &qualifier->source);
}

static tcti_feature_artifact_u64 fnv1a_binding(
	tcti_feature_artifact_u64 hash,
	const struct tcti_feature_field_domain_binding *binding)
{
	hash = fnv1a_u64(hash, binding->feature_node_index);
	hash = fnv1a_u64(hash, binding->identity_group_index);
	hash = fnv1a_u64(hash, binding->occurrence_count);
	hash = fnv1a_u64(hash, binding->register_index);
	hash = fnv1a_u64(hash, binding->field_index);
	hash = fnv1a_u64(hash, binding->value_relation_index);
	hash = fnv1a_u64(hash, binding->disposition);
	hash = fnv1a_text(hash, binding->field_state);
	hash = fnv1a_text(hash, binding->field_register_name);
	hash = fnv1a_text(hash, binding->field_selector);
	hash = fnv1a_qualifier(hash, &binding->field_instance);
	hash = fnv1a_qualifier(hash, &binding->field_slices);
	hash = fnv1a_span(hash, &binding->feature_source);
	hash = fnv1a_span(hash, &binding->register_source);
	hash = fnv1a_span(hash, &binding->field_source);
	return fnv1a_span(hash, &binding->value_relation_source);
}

static int binding_is_well_formed(
	const struct tcti_feature_field_domain_binding *binding)
{
	return !text_is_empty(binding->field_state) &&
		!text_is_empty(binding->field_register_name) &&
		!text_is_empty(binding->field_selector) &&
		binding->field_instance.kind <
			TCTI_FEATURE_ARTIFACT_FIELD_QUALIFIER_KIND_COUNT &&
		binding->field_slices.kind <
			TCTI_FEATURE_ARTIFACT_FIELD_QUALIFIER_KIND_COUNT;
}

static int binding_matches_feature_node(
	const struct tcti_feature_field_domain_binding *binding,
	const struct tcti_feature_artifact_node *node)
{
	return node->kind == TCTI_FEATURE_ARTIFACT_FIELD &&
		!strcmp(binding->field_state, node->field_state) &&
		!strcmp(binding->field_register_name, node->field_register_name) &&
		!strcmp(binding->field_selector, node->field_selector) &&
		qualifier_is_equal(&binding->field_instance, &node->field_instance) &&
		qualifier_is_equal(&binding->field_slices, &node->field_slices) &&
		/* The binding span is the source AST.FIELD expression, not its parent. */
		span_is_equal(&binding->feature_source, &node->field_source) &&
		span_contains(&binding->feature_source, &binding->field_instance.source) &&
		span_contains(&binding->feature_source, &binding->field_slices.source);
}

tcti_feature_artifact_u64 tcti_feature_field_domain_binding_identity(
	const struct tcti_feature_field_domain_binding *bindings,
	tcti_feature_artifact_u32 occurrence_count)
{
	tcti_feature_artifact_u32 index;
	tcti_feature_artifact_u64 identity =
		TCTI_FEATURE_FIELD_DOMAIN_BINDING_FNV1A_OFFSET;

	if (!bindings && occurrence_count)
		return 0;
	identity = fnv1a_u64(identity, occurrence_count);
	for (index = 0; index < occurrence_count; index++)
		identity = fnv1a_binding(identity, &bindings[index]);
	return identity;
}

enum tcti_feature_field_domain_binding_error
tcti_feature_field_domain_binding_validate_with_contract(
	const struct tcti_feature_artifact *feature_artifact,
	const struct tcti_feature_field_domain_binding_artifact *artifact,
	const struct tcti_feature_field_domain_binding_contract *contract,
	struct tcti_feature_field_domain_binding_scratch *scratch,
	struct tcti_feature_field_domain_binding_diagnostic *diagnostic)
{
	tcti_feature_artifact_u32 index;
	tcti_feature_artifact_u32 field_nodes = 0;
	tcti_feature_artifact_u32 mapped = 0;
	tcti_feature_artifact_u32 ambiguous_field = 0;

	if (diagnostic)
		*diagnostic = (struct tcti_feature_field_domain_binding_diagnostic) {
			.error = TCTI_FEATURE_FIELD_DOMAIN_BINDING_VALID,
		};
	if (!feature_artifact || !artifact || !contract || !scratch)
		return fail(diagnostic,
			TCTI_FEATURE_FIELD_DOMAIN_BINDING_INVALID_ARGUMENT, 0);
	if (!source_is_equal(&artifact->source, &feature_artifact->source) ||
	    text_is_empty(artifact->register_source_sha256) ||
	    text_is_empty(contract->register_source_sha256) ||
	    strcmp(artifact->register_source_sha256,
		   contract->register_source_sha256) ||
	    artifact->register_source_length != contract->register_source_length)
		return fail(diagnostic,
			TCTI_FEATURE_FIELD_DOMAIN_BINDING_PIN_MISMATCH, 0);
	if (artifact->occurrence_count != contract->occurrence_count ||
	    artifact->identity_group_count != contract->identity_group_count ||
	    artifact->mapped_count != contract->mapped_count ||
	    artifact->ambiguous_field_count != contract->ambiguous_field_count)
		return fail(diagnostic,
			TCTI_FEATURE_FIELD_DOMAIN_BINDING_COUNT_MISMATCH, 0);
	if (!artifact->bindings || !scratch->feature_node_coverage ||
	    !scratch->identity_group_coverage)
		return fail(diagnostic,
			TCTI_FEATURE_FIELD_DOMAIN_BINDING_POINTER_MISSING, 0);
	if (scratch->feature_node_coverage_count <
	    feature_artifact->counts.node_count ||
	    scratch->identity_group_coverage_count < artifact->identity_group_count)
		return fail(diagnostic,
			TCTI_FEATURE_FIELD_DOMAIN_BINDING_SCRATCH_TOO_SMALL, 0);
	memset(scratch->feature_node_coverage, 0,
		feature_artifact->counts.node_count);
	memset(scratch->identity_group_coverage, 0,
		artifact->identity_group_count *
		sizeof(*scratch->identity_group_coverage));

	for (index = 0; index < feature_artifact->counts.node_count; index++)
		if (feature_artifact->nodes[index].kind == TCTI_FEATURE_ARTIFACT_FIELD)
			field_nodes++;
	if (field_nodes != artifact->occurrence_count)
		return fail(diagnostic,
			TCTI_FEATURE_FIELD_DOMAIN_BINDING_COVERAGE_INVALID, field_nodes);

	for (index = 0; index < artifact->occurrence_count; index++) {
		const struct tcti_feature_field_domain_binding *binding =
			&artifact->bindings[index];
		const struct tcti_feature_artifact_node *node;

		if (binding->order != index ||
		    binding->feature_node_index >= feature_artifact->counts.node_count ||
		    binding->identity_group_index >= artifact->identity_group_count)
			return fail(diagnostic,
				TCTI_FEATURE_FIELD_DOMAIN_BINDING_INDEX_INVALID, index);
		if (binding->disposition != TCTI_FEATURE_FIELD_DOMAIN_MAPPED &&
		    binding->disposition != TCTI_FEATURE_FIELD_DOMAIN_AMBIGUOUS_FIELD)
			return fail(diagnostic,
				TCTI_FEATURE_FIELD_DOMAIN_BINDING_DISPOSITION_INVALID, index);
		if (!binding_is_well_formed(binding))
			return fail(diagnostic,
				TCTI_FEATURE_FIELD_DOMAIN_BINDING_IDENTITY_INVALID, index);
		if (!span_is_valid(&binding->feature_source,
			feature_artifact->source.length, 1) ||
		    !span_is_valid(&binding->field_instance.source,
			feature_artifact->source.length, 1) ||
		    !span_is_valid(&binding->field_slices.source,
			feature_artifact->source.length, 1))
			return fail(diagnostic,
				TCTI_FEATURE_FIELD_DOMAIN_BINDING_PROVENANCE_INVALID, index);
		if (!span_is_valid(&binding->register_source,
			artifact->register_source_length, 1))
			return fail(diagnostic,
				TCTI_FEATURE_FIELD_DOMAIN_BINDING_PROVENANCE_INVALID,
				index);
		if (binding->disposition == TCTI_FEATURE_FIELD_DOMAIN_MAPPED &&
		    (!span_is_valid(&binding->field_source,
			artifact->register_source_length, 1) ||
		     !span_is_valid(&binding->value_relation_source,
			artifact->register_source_length, 1)))
			return fail(diagnostic,
				TCTI_FEATURE_FIELD_DOMAIN_BINDING_PROVENANCE_INVALID, index);
		if (binding->disposition == TCTI_FEATURE_FIELD_DOMAIN_AMBIGUOUS_FIELD &&
		    (binding->field_index != TCTI_FEATURE_ARTIFACT_NODE_NONE ||
		     binding->value_relation_index != TCTI_FEATURE_ARTIFACT_NODE_NONE ||
		     binding->field_source.offset || binding->field_source.length ||
		     binding->value_relation_source.offset ||
		     binding->value_relation_source.length))
			return fail(diagnostic,
				TCTI_FEATURE_FIELD_DOMAIN_BINDING_PROVENANCE_INVALID, index);
		node = &feature_artifact->nodes[binding->feature_node_index];
		if (!binding_matches_feature_node(binding, node))
			return fail(diagnostic,
				TCTI_FEATURE_FIELD_DOMAIN_BINDING_PROVENANCE_INVALID, index);
		if (scratch->feature_node_coverage[binding->feature_node_index])
			return fail(diagnostic,
				TCTI_FEATURE_FIELD_DOMAIN_BINDING_COVERAGE_INVALID, index);
		scratch->feature_node_coverage[binding->feature_node_index] = 1;
		scratch->identity_group_coverage[binding->identity_group_index]++;
		if (binding->disposition == TCTI_FEATURE_FIELD_DOMAIN_MAPPED)
			mapped++;
		else
			ambiguous_field++;
	}
	if (mapped != artifact->mapped_count ||
	    ambiguous_field != artifact->ambiguous_field_count)
		return fail(diagnostic,
			TCTI_FEATURE_FIELD_DOMAIN_BINDING_DISPOSITION_INVALID, 0);
	if (tcti_feature_field_domain_binding_identity(artifact->bindings,
		artifact->occurrence_count) != artifact->identity)
		return fail(diagnostic,
			TCTI_FEATURE_FIELD_DOMAIN_BINDING_IDENTITY_INVALID, 0);
	for (index = 0; index < artifact->identity_group_count; index++)
		if (!scratch->identity_group_coverage[index])
			return fail(diagnostic,
				TCTI_FEATURE_FIELD_DOMAIN_BINDING_COVERAGE_INVALID, index);
	for (index = 0; index < artifact->occurrence_count; index++) {
		const struct tcti_feature_field_domain_binding *binding =
			&artifact->bindings[index];

		if (binding->occurrence_count !=
		    scratch->identity_group_coverage[binding->identity_group_index])
			return fail(diagnostic,
				TCTI_FEATURE_FIELD_DOMAIN_BINDING_IDENTITY_INVALID, index);
	}
	for (index = 0; index < feature_artifact->counts.node_count; index++)
		if (feature_artifact->nodes[index].kind == TCTI_FEATURE_ARTIFACT_FIELD &&
		    !scratch->feature_node_coverage[index])
			return fail(diagnostic,
				TCTI_FEATURE_FIELD_DOMAIN_BINDING_COVERAGE_INVALID, index);
	return TCTI_FEATURE_FIELD_DOMAIN_BINDING_VALID;
}

enum tcti_feature_field_domain_binding_error
tcti_feature_field_domain_binding_validate(
	const struct tcti_feature_artifact *feature_artifact,
	const struct tcti_feature_field_domain_binding_artifact *artifact,
	struct tcti_feature_field_domain_binding_scratch *scratch,
	struct tcti_feature_field_domain_binding_diagnostic *diagnostic)
{
	static const struct tcti_feature_field_domain_binding_contract contract = {
		.occurrence_count = TCTI_FEATURE_FIELD_DOMAIN_BINDING_OCCURRENCE_COUNT,
		.identity_group_count =
			TCTI_FEATURE_FIELD_DOMAIN_BINDING_IDENTITY_GROUP_COUNT,
		.mapped_count = TCTI_FEATURE_FIELD_DOMAIN_BINDING_MAPPED_COUNT,
		.ambiguous_field_count =
			TCTI_FEATURE_FIELD_DOMAIN_BINDING_AMBIGUOUS_FIELD_COUNT,
		.register_source_sha256 =
			TCTI_FEATURE_FIELD_DOMAIN_BINDING_REGISTER_SOURCE_SHA256,
		.register_source_length =
			TCTI_FEATURE_FIELD_DOMAIN_BINDING_REGISTER_SOURCE_LENGTH,
	};

	{
		enum tcti_feature_field_domain_binding_error error;
		tcti_feature_artifact_u32 index;

		error = tcti_feature_field_domain_binding_validate_with_contract(
			feature_artifact, artifact, &contract, scratch, diagnostic);
		if (error != TCTI_FEATURE_FIELD_DOMAIN_BINDING_VALID)
			return error;
		for (index = 0; index < artifact->occurrence_count; index++)
			if (!binding_is_equal(&artifact->bindings[index],
					      &canonical_bindings[index]))
				return fail(diagnostic,
					TCTI_FEATURE_FIELD_DOMAIN_BINDING_CANONICAL_MISMATCH,
					index);
	}
	return TCTI_FEATURE_FIELD_DOMAIN_BINDING_VALID;
}

const char *tcti_feature_field_domain_binding_error_name(
	enum tcti_feature_field_domain_binding_error error)
{
	switch (error) {
	case TCTI_FEATURE_FIELD_DOMAIN_BINDING_VALID: return "valid";
	case TCTI_FEATURE_FIELD_DOMAIN_BINDING_INVALID_ARGUMENT:
		return "invalid argument";
	case TCTI_FEATURE_FIELD_DOMAIN_BINDING_PIN_MISMATCH: return "pin mismatch";
	case TCTI_FEATURE_FIELD_DOMAIN_BINDING_COUNT_MISMATCH:
		return "count mismatch";
	case TCTI_FEATURE_FIELD_DOMAIN_BINDING_POINTER_MISSING:
		return "pointer missing";
	case TCTI_FEATURE_FIELD_DOMAIN_BINDING_DISPOSITION_INVALID:
		return "invalid disposition";
	case TCTI_FEATURE_FIELD_DOMAIN_BINDING_INDEX_INVALID: return "invalid index";
	case TCTI_FEATURE_FIELD_DOMAIN_BINDING_IDENTITY_INVALID:
		return "invalid identity";
	case TCTI_FEATURE_FIELD_DOMAIN_BINDING_PROVENANCE_INVALID:
		return "invalid provenance";
	case TCTI_FEATURE_FIELD_DOMAIN_BINDING_COVERAGE_INVALID:
		return "invalid coverage";
	case TCTI_FEATURE_FIELD_DOMAIN_BINDING_CANONICAL_MISMATCH:
		return "canonical relation mismatch";
	case TCTI_FEATURE_FIELD_DOMAIN_BINDING_SCRATCH_TOO_SMALL:
		return "scratch too small";
	}
	return "unknown";
}
