/* SPDX-License-Identifier: GPL-2.0-only */
#ifdef ORLIX_TCTI_FEATURE_FIELD_DOMAIN_TEST_DEF_ONLY
ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_SOURCE("fixture-architecture", "fixture-build",
	"fixture-reference", "fixture-schema", "fixture-feature-sha256", 1000U,
	"fixture-register-sha256", 1000U)
ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_COUNTS_V3(1U, 1U, 1U, 0U, 1U, 1U,
	1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U)
ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_IDENTITY(UINT64_C(0x1))
ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_RANGE_V3(0U, 0U, 4U)
ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_VALUESET_V3(0U, "Values")
ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_DOMAIN_V3(0U, "Value", "0", NULL,
	NULL, NULL, NULL, 4294967295U, 4294967295U, 0U, 0U, 0U,
	4294967295U, 0U, 1U)
ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_LINK_V3(0U, "key", "value", 0U)
ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_EXPRESSION_V3(0U, "Bool", NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, 0U, 0U, 1U, 0, 1U, 4294967295U,
	0U, 0U, 0U, 0U, 300U, 2U)
ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_EXPRESSION_CHILD_V3(0U, 0U)
ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_VALUE_CANDIDATE_V3(0U, 1U, 0U, 0U, 1U)
ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_CONSTRAINT_V3(0U, 1U, 0U, 0U, 1U)
ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_CONSTRAINT_ITEM_V3(0U, 0U, 0U, 0U)
ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_CONSTRAINT_CANDIDATE_V3(0U, 0U, 1U, 0U, 1U)
ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_ALTERNATIVE_V3(0U, 2U, 3U, 4U,
	4294967295U, 4294967295U, 0U, 4294967295U, 4294967295U,
	0U, 1U, 0U, 1U, 0U, 1U, 0U, 1U, 0U, 1U, 0U, 1U,
	0U, 1U, 0U, 1U, 0U, 1U, 0U, 1U, 120U, 20U, 140U, 20U,
	100U, 80U, 0U, 0U, 0U, 0U, 0U, 0U)
ORLIX_TCTI_A64_FEATURE_FIELD_DOMAIN_OCCURRENCE_V3(0U, 0U, 0U, 1U,
	1U, 2U, 3U, 0U, "PSTATE", "ID_AA64PFR0_EL1", "CSV2",
	1U, 21U, 1U, 1U, 22U, 1U, 20U, 8U, 100U, 100U, 120U, 20U,
	140U, 20U, 0U, 1U, 4U, 1U, 64U, 4U, 1U, 1U, 0U, 1U, 1U,
	4294967295U, 4294967295U, 0U, 0U, 0U, 0U, UINT64_C(0x1),
	UINT64_C(0x2), 100U, 80U, 0U, 0U, 0U, 0U, 0U, 0U)
#else
#include "target_feature_field_domain_binding_artifact.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { \
	fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); return -1; \
} } while (0)

static struct orlix_tcti_feature_artifact_node feature_nodes[] = {
	{
		.kind = ORLIX_TCTI_FEATURE_ARTIFACT_FIELD,
		.field_state = "PSTATE",
		.field_register_name = "ID_AA64PFR0_EL1",
		.field_selector = "CSV2",
		.field_source = { 20, 8 },
		.field_instance = { ORLIX_TCTI_FEATURE_ARTIFACT_FIELD_QUALIFIER_NULL,
			{ 21, 1 } },
		.field_slices = { ORLIX_TCTI_FEATURE_ARTIFACT_FIELD_QUALIFIER_NULL,
			{ 22, 1 } },
		.source = { 16, 16 },
	},
};

static const struct orlix_tcti_feature_artifact feature_artifact = {
	.source = { "fixture-architecture", "fixture-build", "fixture-reference",
		"fixture-schema", "fixture-feature-sha256", 1000 },
	.counts = { .node_count = 1 },
	.nodes = feature_nodes,
};

static struct orlix_tcti_feature_field_domain_binding bindings[] = {
	{
		.order = 0, .feature_node_index = 0, .identity_group_index = 0,
		.occurrence_count = 1, .register_index = 1, .field_index = 2,
		.value_relation_index = 3,
		.disposition = ORLIX_TCTI_FEATURE_FIELD_DOMAIN_MAPPED,
		.field_state = "PSTATE", .field_register_name = "ID_AA64PFR0_EL1",
		.field_selector = "CSV2",
		.field_instance = { ORLIX_TCTI_FEATURE_ARTIFACT_FIELD_QUALIFIER_NULL,
			{ 21, 1 } },
		.field_slices = { ORLIX_TCTI_FEATURE_ARTIFACT_FIELD_QUALIFIER_NULL,
			{ 22, 1 } },
		.feature_source = { 20, 8 }, .register_source = { 100, 100 },
		.field_source = { 120, 20 }, .value_relation_source = { 140, 20 },
		.first_alternative = 0, .alternative_count = 1,
		.domain = {
			.fieldset_index = 4, .equivalent_field_count = 1,
			.register_width = 64, .field_width = 4, .range_count = 1,
			.value_member_count = 1, .relation_value_count = 1,
			.relation_constraint_count = 1,
			.type = ORLIX_TCTI_FEATURE_FIELD_DOMAIN_TYPE_CONSTANT,
			.signedness = ORLIX_TCTI_FEATURE_FIELD_DOMAIN_UNSIGNED,
			.member_kind = ORLIX_TCTI_FEATURE_FIELD_DOMAIN_ENUM_MEMBERS,
			.semantic_identity = 1, .resolution_identity = 2,
			.fieldset_source = { 100, 80 },
		},
	},
};

static struct orlix_tcti_feature_field_domain_range ranges[] = {
	{ 0, 4 }, { 2, 2 },
};
static struct orlix_tcti_feature_field_domain_valueset valuesets[] = { { "Values" } };
static struct orlix_tcti_feature_field_domain_node domains[] = {
	{ .type = "Value", .value = "0", .parent_domain =
		ORLIX_TCTI_FEATURE_FIELD_DOMAIN_INDEX_NONE,
		.condition_expression = ORLIX_TCTI_FEATURE_FIELD_DOMAIN_INDEX_NONE,
		.first_child_domain = 1, .child_domain_count = 1, .valueset_index = 0,
		.nested_valueset_index = ORLIX_TCTI_FEATURE_FIELD_DOMAIN_INDEX_NONE,
		.first_link = 0, .link_count = 1 },
	{ .type = "Meaning", .meaning = "implemented", .parent_domain = 0,
		.condition_expression = 1,
		.first_child_domain = ORLIX_TCTI_FEATURE_FIELD_DOMAIN_INDEX_NONE,
		.valueset_index = ORLIX_TCTI_FEATURE_FIELD_DOMAIN_INDEX_NONE,
		.nested_valueset_index = ORLIX_TCTI_FEATURE_FIELD_DOMAIN_INDEX_NONE,
		.first_link = 1 },
};
static struct orlix_tcti_feature_field_domain_link links[] = {
	{ "key", "value", 0 },
};
static struct orlix_tcti_feature_field_domain_expression expressions[] = {
	{ .type = "Binary", .op = "==", .first_child = 0, .child_count = 1,
		.parent_expression = ORLIX_TCTI_FEATURE_FIELD_DOMAIN_INDEX_NONE,
		.source = { 300, 20 } },
	{ .type = "Integer", .scalar_kind = 1, .integer = 1,
		.first_child = ORLIX_TCTI_FEATURE_FIELD_DOMAIN_INDEX_NONE,
		.parent_expression = 0, .source = { 305, 2 } },
};
static orlix_tcti_feature_artifact_u32 expression_children[] = { 1 };
static struct orlix_tcti_feature_field_domain_value_candidate value_candidates[] = {
	{ 1, 0, 0, 2 },
};
static struct orlix_tcti_feature_field_domain_constraint constraints[] = {
	{ 1, 0, 0, 1 },
};
static struct orlix_tcti_feature_field_domain_constraint_item constraint_items[] = {
	{ 0, 0, 0 },
};
static struct orlix_tcti_feature_field_domain_constraint_candidate
	constraint_candidates[] = { { 0, 1, 0, 2 } };
static struct orlix_tcti_feature_field_domain_alternative alternatives[] = {
	{
		.field_index = 2, .value_relation_index = 3, .fieldset_index = 4,
		.field_condition_expression = ORLIX_TCTI_FEATURE_FIELD_DOMAIN_INDEX_NONE,
		.wrapper_condition_expression = ORLIX_TCTI_FEATURE_FIELD_DOMAIN_INDEX_NONE,
		.fieldset_condition_expression = 0,
		.relation_field_condition_expression =
			ORLIX_TCTI_FEATURE_FIELD_DOMAIN_INDEX_NONE,
		.relation_wrapper_condition_expression =
			ORLIX_TCTI_FEATURE_FIELD_DOMAIN_INDEX_NONE,
		.first_range = 0, .range_count = 1,
		.first_valueset = 0, .valueset_count = 1,
		.first_domain = 0, .domain_count = 2,
		.first_link = 0, .link_count = 1,
		.first_expression = 0, .expression_count = 2,
		.first_expression_child = 0, .expression_child_count = 1,
		.first_value_candidate = 0, .value_candidate_count = 1,
		.first_constraint = 0, .constraint_count = 1,
		.first_constraint_item = 0, .constraint_item_count = 1,
		.first_constraint_candidate = 0, .constraint_candidate_count = 1,
		.field_source = { 120, 20 }, .value_relation_source = { 140, 20 },
		.fieldset_source = { 100, 80 },
	},
};

static orlix_tcti_feature_artifact_u8 coverage[1];
static orlix_tcti_feature_artifact_u32 groups[1];
static struct orlix_tcti_feature_field_domain_binding_scratch scratch = {
	coverage, 1, groups, 1,
};
static const struct orlix_tcti_feature_field_domain_binding_contract contract = {
	1, 1, 1, 0, 1, "fixture-register-sha256", 1000,
};

static struct orlix_tcti_feature_field_domain_binding_artifact fixture(void)
{
	struct orlix_tcti_feature_field_domain_binding_artifact a = {
		.source = { "fixture-architecture", "fixture-build", "fixture-reference",
			"fixture-schema", "fixture-feature-sha256", 1000 },
		.register_source_sha256 = "fixture-register-sha256",
		.register_source_length = 1000,
		.occurrence_count = 1, .identity_group_count = 1, .mapped_count = 1,
		.alternative_count = 1, .range_count = 1, .valueset_count = 1,
		.domain_count = 2, .link_count = 1, .expression_count = 2,
		.expression_child_count = 1, .value_candidate_count = 1,
		.constraint_count = 1, .constraint_item_count = 1,
		.constraint_candidate_count = 1,
		.bindings = bindings, .alternatives = alternatives, .ranges = ranges,
		.valuesets = valuesets, .domains = domains, .links = links,
		.expressions = expressions, .expression_children = expression_children,
		.value_candidates = value_candidates, .constraints = constraints,
		.constraint_items = constraint_items,
		.constraint_candidates = constraint_candidates,
	};
	return a;
}

static int expect_invalid(struct orlix_tcti_feature_field_domain_binding_artifact *a)
{
	CHECK(orlix_tcti_feature_field_domain_binding_validate_with_contract(
		&feature_artifact, a, &contract, &scratch, NULL) !=
		ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_VALID);
	return 0;
}

static int fixture_and_structural_mutations(void)
{
	struct orlix_tcti_feature_field_domain_binding_artifact a = fixture();
	struct orlix_tcti_feature_field_domain_binding_contract two_alternatives =
		contract;
	struct orlix_tcti_feature_field_domain_alternative overlapping[2];
	CHECK(orlix_tcti_feature_field_domain_binding_validate_with_contract(
		&feature_artifact, &a, &contract, &scratch, NULL) ==
		ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_VALID);

	ranges[0].width = 0; CHECK(expect_invalid(&a) == 0); ranges[0].width = 4;
	a.range_count = 2; alternatives[0].range_count = 2;
	bindings[0].domain.range_count = 2; CHECK(expect_invalid(&a) == 0);
	a.range_count = 1; alternatives[0].range_count = 1;
	bindings[0].domain.range_count = 1;
	domains[1].parent_domain = 1; CHECK(expect_invalid(&a) == 0);
	domains[1].parent_domain = 0;
	domains[0].parent_domain = 1; CHECK(expect_invalid(&a) == 0);
	domains[0].parent_domain = ORLIX_TCTI_FEATURE_FIELD_DOMAIN_INDEX_NONE;
	domains[1].condition_expression = 2; CHECK(expect_invalid(&a) == 0);
	domains[1].condition_expression = 1;
	expression_children[0] = 2; CHECK(expect_invalid(&a) == 0);
	expression_children[0] = 1;
	expressions[0].parent_expression = 1; CHECK(expect_invalid(&a) == 0);
	expressions[0].parent_expression = ORLIX_TCTI_FEATURE_FIELD_DOMAIN_INDEX_NONE;
	value_candidates[0].first_domain = 2; CHECK(expect_invalid(&a) == 0);
	value_candidates[0].first_domain = 0;
	constraint_items[0].constraint_index = 1; CHECK(expect_invalid(&a) == 0);
	constraint_items[0].constraint_index = 0;
	constraint_candidates[0].constraint_index = 1; CHECK(expect_invalid(&a) == 0);
	constraint_candidates[0].constraint_index = 0;
	links[0].domain_index = 2; CHECK(expect_invalid(&a) == 0);
	links[0].domain_index = 0;
	alternatives[0].first_range = 1; CHECK(expect_invalid(&a) == 0);
	alternatives[0].first_range = 0;
	bindings[0].first_alternative = 1; CHECK(expect_invalid(&a) == 0);
	bindings[0].first_alternative = 0;
	overlapping[0] = alternatives[0];
	overlapping[1] = alternatives[0];
	a.alternatives = overlapping;
	a.alternative_count = 2;
	bindings[0].alternative_count = 2;
	bindings[0].domain.equivalent_field_count = 2;
	two_alternatives.alternative_count = 2;
	CHECK(orlix_tcti_feature_field_domain_binding_validate_with_contract(
		&feature_artifact, &a, &two_alternatives, &scratch, NULL) !=
		ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_VALID);
	bindings[0].alternative_count = 1;
	bindings[0].domain.equivalent_field_count = 1;
	return 0;
}

static int canonical_mutations_are_deeply_rejected(void)
{
	const struct orlix_tcti_feature_field_domain_binding_artifact *live =
		orlix_tcti_feature_field_domain_binding_artifact_canonical();
	struct orlix_tcti_feature_field_domain_binding_artifact a = *live;
	struct orlix_tcti_feature_field_domain_binding *binding;
	struct orlix_tcti_feature_field_domain_range *range;
	struct orlix_tcti_feature_field_domain_valueset *valueset;
	struct orlix_tcti_feature_field_domain_node *domain;
	struct orlix_tcti_feature_field_domain_link *link;
	struct orlix_tcti_feature_field_domain_expression *expression;
	orlix_tcti_feature_artifact_u32 *child;
	struct orlix_tcti_feature_field_domain_value_candidate *value_candidate;
	struct orlix_tcti_feature_field_domain_constraint *constraint;
	struct orlix_tcti_feature_field_domain_constraint_item *item;
	struct orlix_tcti_feature_field_domain_constraint_candidate *candidate;
	struct orlix_tcti_feature_field_domain_alternative *alternative;
	struct orlix_tcti_feature_field_domain_link added_link = { "key", "value", 0 };
	struct orlix_tcti_feature_field_domain_value_candidate added_value_candidate = {
		0, 0, 0, 0,
	};
	struct orlix_tcti_feature_field_domain_constraint_candidate added_candidate = {
		0, 0, 0, 0,
	};
	static orlix_tcti_feature_artifact_u8 canonical_coverage[
		ORLIX_TCTI_FEATURE_ARTIFACT_NODE_COUNT];
	static orlix_tcti_feature_artifact_u32 canonical_groups[
		ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_IDENTITY_GROUP_COUNT];
	struct orlix_tcti_feature_field_domain_binding_scratch s = {
		canonical_coverage, sizeof(canonical_coverage), canonical_groups,
		sizeof(canonical_groups) / sizeof(canonical_groups[0]),
	};
	struct orlix_tcti_feature_field_domain_binding_diagnostic diagnostic;
	enum orlix_tcti_feature_field_domain_binding_error baseline;
	/* The isolated compile fixture proves the V3 macro contract, not production. */
	if (live->occurrence_count !=
	    ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_OCCURRENCE_COUNT)
		return 0;
#define COPY_ONE(name, member, count) do { \
	name = malloc(live->count * sizeof(*name)); CHECK(name != NULL); \
	memcpy(name, live->member, live->count * sizeof(*name)); \
	a.member = name; \
} while (0)
#define REJECT_AND_FREE(name, member) do { \
	CHECK(orlix_tcti_feature_field_domain_binding_validate( \
		orlix_tcti_feature_artifact_canonical(), &a, &s, NULL) != \
		ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_VALID); \
	free(name); a.member = live->member; \
} while (0)
	baseline = orlix_tcti_feature_field_domain_binding_validate(
		orlix_tcti_feature_artifact_canonical(), live, &s, &diagnostic);
	if (baseline != ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_VALID) {
		fprintf(stderr, "canonical V3 validation: %s at %u\n",
			orlix_tcti_feature_field_domain_binding_error_name(baseline),
			diagnostic.index);
		return -1;
	}
	a.identity++;
	CHECK(orlix_tcti_feature_field_domain_binding_validate(
		orlix_tcti_feature_artifact_canonical(), &a, &s, NULL) ==
		ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_CANONICAL_MISMATCH);
	a.identity = live->identity;
	COPY_ONE(binding, bindings, occurrence_count);
	binding[0].register_index++; REJECT_AND_FREE(binding, bindings);
	COPY_ONE(range, ranges, range_count);
	range[0].start++; REJECT_AND_FREE(range, ranges);
	COPY_ONE(valueset, valuesets, valueset_count);
	valueset[0].type = "mutated"; REJECT_AND_FREE(valueset, valuesets);
	COPY_ONE(domain, domains, domain_count);
	domain[0].value = "mutated"; REJECT_AND_FREE(domain, domains);
	if (live->link_count) {
		COPY_ONE(link, links, link_count);
		link[0].key = "mutated"; REJECT_AND_FREE(link, links);
	} else {
		a.link_count = 1;
		a.links = &added_link;
		CHECK(orlix_tcti_feature_field_domain_binding_validate(
			orlix_tcti_feature_artifact_canonical(), &a, &s, NULL) !=
			ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_VALID);
		a.link_count = 0;
		a.links = live->links;
	}
	COPY_ONE(expression, expressions, expression_count);
	expression[0].integer++; REJECT_AND_FREE(expression, expressions);
	COPY_ONE(child, expression_children, expression_child_count);
	child[0] = child[0] ? 0 : 1; REJECT_AND_FREE(child, expression_children);
	if (live->value_candidate_count) {
		COPY_ONE(value_candidate, value_candidates, value_candidate_count);
		value_candidate[0].kind++;
		REJECT_AND_FREE(value_candidate, value_candidates);
	} else {
		a.value_candidate_count = 1;
		a.value_candidates = &added_value_candidate;
		CHECK(orlix_tcti_feature_field_domain_binding_validate(
			orlix_tcti_feature_artifact_canonical(), &a, &s, NULL) !=
			ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_VALID);
		a.value_candidate_count = 0;
		a.value_candidates = live->value_candidates;
	}
	COPY_ONE(constraint, constraints, constraint_count);
	constraint[0].kind++; REJECT_AND_FREE(constraint, constraints);
	COPY_ONE(item, constraint_items, constraint_item_count);
	item[0].valueset_index ^= 1U; REJECT_AND_FREE(item, constraint_items);
	if (live->constraint_candidate_count) {
		COPY_ONE(candidate, constraint_candidates, constraint_candidate_count);
		candidate[0].kind++;
		REJECT_AND_FREE(candidate, constraint_candidates);
	} else {
		a.constraint_candidate_count = 1;
		a.constraint_candidates = &added_candidate;
		CHECK(orlix_tcti_feature_field_domain_binding_validate(
			orlix_tcti_feature_artifact_canonical(), &a, &s, NULL) !=
			ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_VALID);
		a.constraint_candidate_count = 0;
		a.constraint_candidates = live->constraint_candidates;
	}
	COPY_ONE(alternative, alternatives, alternative_count);
	alternative[0].field_index++; REJECT_AND_FREE(alternative, alternatives);
#undef REJECT_AND_FREE
#undef COPY_ONE
	return 0;
}

int main(void)
{
	if (fixture_and_structural_mutations() ||
	    canonical_mutations_are_deeply_rejected())
		return 1;
	puts("PASS target feature field-domain V3 artifact validator");
	return 0;
}
#endif
