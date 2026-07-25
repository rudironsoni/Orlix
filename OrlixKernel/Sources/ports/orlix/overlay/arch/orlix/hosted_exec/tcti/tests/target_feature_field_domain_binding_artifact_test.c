/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_feature_field_domain_binding_artifact.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expression) do { \
	if (!(expression)) { \
		fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression); \
		return -1; \
	} \
} while (0)

static struct tcti_feature_artifact_node feature_nodes[] = {
	[0] = {
		.kind = TCTI_FEATURE_ARTIFACT_IDENTIFIER,
		.left = TCTI_FEATURE_ARTIFACT_NODE_NONE,
		.right = TCTI_FEATURE_ARTIFACT_NODE_NONE,
		.first_child = TCTI_FEATURE_ARTIFACT_NODE_NONE,
		.text = "ordinary",
		.source = { 1, 4 },
	},
	[1] = {
		.kind = TCTI_FEATURE_ARTIFACT_FIELD,
		.left = TCTI_FEATURE_ARTIFACT_NODE_NONE,
		.right = TCTI_FEATURE_ARTIFACT_NODE_NONE,
		.first_child = TCTI_FEATURE_ARTIFACT_NODE_NONE,
		.field_state = "PSTATE",
		.field_register_name = "ID_AA64PFR0_EL1",
		.field_selector = "CSV2",
		.field_source = { 24, 24 },
		.field_instance = { TCTI_FEATURE_ARTIFACT_FIELD_QUALIFIER_NULL,
			{ 28, 4 } },
		.field_slices = { TCTI_FEATURE_ARTIFACT_FIELD_QUALIFIER_NULL,
			{ 36, 4 } },
		.source = { 20, 40 },
	},
	[2] = {
		.kind = TCTI_FEATURE_ARTIFACT_FIELD,
		.left = TCTI_FEATURE_ARTIFACT_NODE_NONE,
		.right = TCTI_FEATURE_ARTIFACT_NODE_NONE,
		.first_child = TCTI_FEATURE_ARTIFACT_NODE_NONE,
		.field_state = "PSTATE",
		.field_register_name = "ID_AA64PFR0_EL1",
		.field_selector = "CSV2",
		.field_source = { 124, 24 },
		.field_instance = { TCTI_FEATURE_ARTIFACT_FIELD_QUALIFIER_NULL,
			{ 128, 4 } },
		.field_slices = { TCTI_FEATURE_ARTIFACT_FIELD_QUALIFIER_NULL,
			{ 136, 4 } },
		.source = { 120, 40 },
	},
	[3] = {
		.kind = TCTI_FEATURE_ARTIFACT_FIELD,
		.left = TCTI_FEATURE_ARTIFACT_NODE_NONE,
		.right = TCTI_FEATURE_ARTIFACT_NODE_NONE,
		.first_child = TCTI_FEATURE_ARTIFACT_NODE_NONE,
		.field_state = "CurrentState",
		.field_register_name = "ID_AA64ISAR0_EL1",
		.field_selector = "AES",
		.field_source = { 224, 24 },
		.field_instance = { TCTI_FEATURE_ARTIFACT_FIELD_QUALIFIER_NULL,
			{ 228, 4 } },
		.field_slices = { TCTI_FEATURE_ARTIFACT_FIELD_QUALIFIER_NULL,
			{ 236, 4 } },
		.source = { 220, 40 },
	},
};

static const struct tcti_feature_artifact feature_artifact = {
	.source = {
		.architecture = "fixture-architecture",
		.build = "fixture-build",
		.reference = "fixture-reference",
		.schema = "fixture-schema",
		.sha256 = "fixture-feature-sha256",
		.length = 1000,
	},
	.counts = { .node_count = 4 },
	.nodes = feature_nodes,
};

#define FIELD_BINDING(_order, _node, _group, _members, _reg, _field, _value, \
		_disposition, _state, _register, _selector, _feature_offset, \
		_feature_length, \
		_instance_offset, _slices_offset) \
	{ \
		.order = _order, .feature_node_index = _node, \
		.identity_group_index = _group, .occurrence_count = _members, \
		.register_index = _reg, .field_index = _field, \
		.value_relation_index = _value, .disposition = _disposition, \
		.field_state = _state, .field_register_name = _register, \
		.field_selector = _selector, \
		.field_instance = { TCTI_FEATURE_ARTIFACT_FIELD_QUALIFIER_NULL, \
			{ _instance_offset, 4 } }, \
		.field_slices = { TCTI_FEATURE_ARTIFACT_FIELD_QUALIFIER_NULL, \
			{ _slices_offset, 4 } }, \
		.feature_source = { _feature_offset, _feature_length }, \
		.register_source = { 100, 40 }, .field_source = { 120, 12 }, \
		.value_relation_source = { 140, 16 }, \
	}

static struct tcti_feature_field_domain_binding bindings[] = {
	FIELD_BINDING(0, 1, 0, 2, 7, 8, 9,
		TCTI_FEATURE_FIELD_DOMAIN_MAPPED, "PSTATE", "ID_AA64PFR0_EL1", "CSV2",
		24, 24, 28, 36),
	FIELD_BINDING(1, 2, 0, 2, 7, 8, 9,
		TCTI_FEATURE_FIELD_DOMAIN_MAPPED, "PSTATE", "ID_AA64PFR0_EL1", "CSV2",
		124, 24, 128, 136),
	{
		.order = 2, .feature_node_index = 3, .identity_group_index = 1,
		.occurrence_count = 1, .register_index = 13,
		.field_index = TCTI_FEATURE_ARTIFACT_NODE_NONE,
		.value_relation_index = TCTI_FEATURE_ARTIFACT_NODE_NONE,
		.disposition = TCTI_FEATURE_FIELD_DOMAIN_AMBIGUOUS_FIELD,
		.field_state = "CurrentState",
		.field_register_name = "ID_AA64ISAR0_EL1", .field_selector = "AES",
		.field_instance = { TCTI_FEATURE_ARTIFACT_FIELD_QUALIFIER_NULL,
			{ 228, 4 } },
		.field_slices = { TCTI_FEATURE_ARTIFACT_FIELD_QUALIFIER_NULL,
			{ 236, 4 } },
		.feature_source = { 224, 24 }, .register_source = { 100, 40 },
	},
};

static tcti_feature_artifact_u8 feature_node_coverage[4];
static tcti_feature_artifact_u32 identity_group_coverage[2];

static struct tcti_feature_field_domain_binding_scratch scratch = {
	.feature_node_coverage = feature_node_coverage,
	.feature_node_coverage_count = 4,
	.identity_group_coverage = identity_group_coverage,
	.identity_group_coverage_count = 2,
};

static const struct tcti_feature_field_domain_binding_contract contract = {
	.occurrence_count = 3,
	.identity_group_count = 2,
	.mapped_count = 2,
	.ambiguous_field_count = 1,
	.register_source_sha256 = "fixture-register-sha256",
	.register_source_length = 10000,
};

static struct tcti_feature_field_domain_binding_artifact artifact(void)
{
	struct tcti_feature_field_domain_binding_artifact result = {
		.source = {
			.architecture = "fixture-architecture",
			.build = "fixture-build",
			.reference = "fixture-reference",
			.schema = "fixture-schema",
			.sha256 = "fixture-feature-sha256",
			.length = 1000,
		},
		.register_source_sha256 = "fixture-register-sha256",
		.register_source_length = 10000,
		.occurrence_count = 3,
		.identity_group_count = 2,
		.mapped_count = 2,
		.ambiguous_field_count = 1,
		.bindings = bindings,
	};

	result.identity = tcti_feature_field_domain_binding_identity(
		result.bindings, result.occurrence_count);
	return result;
}

static int expect_error(
	const struct tcti_feature_field_domain_binding_artifact *candidate,
	enum tcti_feature_field_domain_binding_error expected)
{
	struct tcti_feature_field_domain_binding_diagnostic diagnostic;

	CHECK(tcti_feature_field_domain_binding_validate_with_contract(
		&feature_artifact, candidate, &contract, &scratch, &diagnostic) == expected);
	CHECK(diagnostic.error == expected);
	return 0;
}

static int canonical_fixture_is_valid(void)
{
	struct tcti_feature_field_domain_binding_artifact candidate = artifact();

	CHECK(tcti_feature_field_domain_binding_validate_with_contract(
		&feature_artifact, &candidate, &contract, &scratch, NULL) ==
		TCTI_FEATURE_FIELD_DOMAIN_BINDING_VALID);
	return 0;
}

static int checked_canonical_artifact_is_valid(void)
{
	struct tcti_feature_field_domain_binding_diagnostic diagnostic;
	enum tcti_feature_field_domain_binding_error error;
	static tcti_feature_artifact_u8 coverage[
		TCTI_FEATURE_ARTIFACT_NODE_COUNT];
	static tcti_feature_artifact_u32 groups[
		TCTI_FEATURE_FIELD_DOMAIN_BINDING_IDENTITY_GROUP_COUNT];
	struct tcti_feature_field_domain_binding_scratch canonical_scratch = {
		.feature_node_coverage = coverage,
		.feature_node_coverage_count = sizeof(coverage),
		.identity_group_coverage = groups,
		.identity_group_coverage_count = sizeof(groups) / sizeof(groups[0]),
	};

	error = tcti_feature_field_domain_binding_validate(
		tcti_feature_artifact_canonical(),
		tcti_feature_field_domain_binding_artifact_canonical(),
		&canonical_scratch, &diagnostic);
	if (error != TCTI_FEATURE_FIELD_DOMAIN_BINDING_VALID) {
		fprintf(stderr, "canonical field-domain binding: %s at %u\n",
			tcti_feature_field_domain_binding_error_name(error),
			diagnostic.index);
		return -1;
	}
	return 0;
}

static int rejects_provenance_coverage_disposition_and_identity_drift(void)
{
	struct tcti_feature_field_domain_binding_artifact candidate;
	struct tcti_feature_field_domain_binding original_binding = bindings[1];
	struct tcti_feature_field_domain_binding original_ambiguous = bindings[2];
	tcti_feature_artifact_u32 original_offset = bindings[0].feature_source.offset;
	enum tcti_feature_field_domain_disposition original_disposition =
		bindings[2].disposition;

	candidate = artifact();
	bindings[1] = bindings[0];
	bindings[1].order = 1;
	candidate.identity = tcti_feature_field_domain_binding_identity(
		candidate.bindings, candidate.occurrence_count);
	CHECK(expect_error(&candidate,
		TCTI_FEATURE_FIELD_DOMAIN_BINDING_COVERAGE_INVALID) == 0);
	bindings[1] = original_binding;

	candidate = artifact();
	bindings[0].feature_source.offset = 21;
	candidate.identity = tcti_feature_field_domain_binding_identity(
		candidate.bindings, candidate.occurrence_count);
	CHECK(expect_error(&candidate,
		TCTI_FEATURE_FIELD_DOMAIN_BINDING_PROVENANCE_INVALID) == 0);
	bindings[0].feature_source.offset = original_offset;

	candidate = artifact();
	bindings[2].disposition = TCTI_FEATURE_FIELD_DOMAIN_MISSING_FIELD;
	candidate.identity = tcti_feature_field_domain_binding_identity(
		candidate.bindings, candidate.occurrence_count);
	CHECK(expect_error(&candidate,
		TCTI_FEATURE_FIELD_DOMAIN_BINDING_DISPOSITION_INVALID) == 0);
	bindings[2].disposition = original_disposition;

	candidate = artifact();
	/* Ambiguity may identify a register, never a fabricated resolved field. */
	bindings[2].field_index = 14;
	bindings[2].value_relation_index = 14;
	bindings[2].field_source = (struct tcti_feature_artifact_span) { 120, 12 };
	bindings[2].value_relation_source =
		(struct tcti_feature_artifact_span) { 140, 16 };
	candidate.identity = tcti_feature_field_domain_binding_identity(
		candidate.bindings, candidate.occurrence_count);
	CHECK(expect_error(&candidate,
		TCTI_FEATURE_FIELD_DOMAIN_BINDING_PROVENANCE_INVALID) == 0);
	bindings[2] = original_ambiguous;

	candidate = artifact();
	candidate.identity++;
	CHECK(expect_error(&candidate,
		TCTI_FEATURE_FIELD_DOMAIN_BINDING_IDENTITY_INVALID) == 0);
	return 0;
}

static int rejects_group_cardinality_and_source_pin_drift(void)
{
	struct tcti_feature_field_domain_binding_artifact candidate = artifact();
	tcti_feature_artifact_u32 original_members = bindings[0].occurrence_count;

	bindings[0].occurrence_count = 1;
	candidate.identity = tcti_feature_field_domain_binding_identity(
		candidate.bindings, candidate.occurrence_count);
	CHECK(expect_error(&candidate,
		TCTI_FEATURE_FIELD_DOMAIN_BINDING_IDENTITY_INVALID) == 0);
	bindings[0].occurrence_count = original_members;

	candidate = artifact();
	candidate.source.length--;
	CHECK(expect_error(&candidate,
		TCTI_FEATURE_FIELD_DOMAIN_BINDING_PIN_MISMATCH) == 0);
	return 0;
}

static int canonical_wrapper_rejects_recomputed_relation_drift(void)
{
	const struct tcti_feature_field_domain_binding_artifact *live;
	struct tcti_feature_field_domain_binding_artifact candidate;
	struct tcti_feature_field_domain_binding *copy;
	struct tcti_feature_field_domain_binding_diagnostic diagnostic;
	static tcti_feature_artifact_u8 coverage[
		TCTI_FEATURE_ARTIFACT_NODE_COUNT];
	static tcti_feature_artifact_u32 groups[
		TCTI_FEATURE_FIELD_DOMAIN_BINDING_IDENTITY_GROUP_COUNT];
	struct tcti_feature_field_domain_binding_scratch canonical_scratch = {
		.feature_node_coverage = coverage,
		.feature_node_coverage_count = sizeof(coverage),
		.identity_group_coverage = groups,
		.identity_group_coverage_count = sizeof(groups) / sizeof(groups[0]),
	};

	live = tcti_feature_field_domain_binding_artifact_canonical();
	copy = malloc(live->occurrence_count * sizeof(*copy));
	CHECK(copy != NULL);
	memcpy(copy, live->bindings, live->occurrence_count * sizeof(*copy));
	candidate = *live;
	candidate.bindings = copy;
	/* This remains in range, so only the exact checked relation can reject it. */
	copy[0].register_index++;
	candidate.identity = tcti_feature_field_domain_binding_identity(
		candidate.bindings, candidate.occurrence_count);
	CHECK(tcti_feature_field_domain_binding_validate(
		      tcti_feature_artifact_canonical(), &candidate,
		      &canonical_scratch, &diagnostic) ==
		TCTI_FEATURE_FIELD_DOMAIN_BINDING_CANONICAL_MISMATCH);
	CHECK(diagnostic.index == 0);
	free(copy);
	return 0;
}

int main(void)
{
	if (canonical_fixture_is_valid() ||
	    checked_canonical_artifact_is_valid() ||
	    rejects_provenance_coverage_disposition_and_identity_drift() ||
	    rejects_group_cardinality_and_source_pin_drift() ||
	    canonical_wrapper_rejects_recomputed_relation_drift())
		return 1;
	puts("PASS target feature field-domain binding artifact validator");
	return 0;
}
