/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_feature_artifact.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expression) do { \
	if (!(expression)) { \
		fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression); \
		return -1; \
	} \
} while (0)

static tcti_feature_artifact_u8 state[TCTI_FEATURE_ARTIFACT_NODE_COUNT];
static struct tcti_feature_artifact_frame
	frames[TCTI_FEATURE_ARTIFACT_NODE_COUNT];
static tcti_feature_artifact_u8
	child_coverage[TCTI_FEATURE_ARTIFACT_CHILD_COUNT];
static struct tcti_feature_artifact_node
	mutable_nodes[TCTI_FEATURE_ARTIFACT_NODE_COUNT];
static struct tcti_feature_artifact_constraint
	mutable_constraints[TCTI_FEATURE_ARTIFACT_CONSTRAINT_COUNT];
static tcti_feature_artifact_u32
	mutable_children[TCTI_FEATURE_ARTIFACT_CHILD_COUNT];

static struct tcti_feature_artifact_scratch scratch = {
	.state = state,
	.state_count = sizeof(state),
	.frames = frames,
	.frame_count = sizeof(frames) / sizeof(frames[0]),
	.child_coverage = child_coverage,
	.child_coverage_count = sizeof(child_coverage),
};

static struct tcti_feature_artifact mutable_artifact(void)
{
	const struct tcti_feature_artifact *canonical =
		tcti_feature_artifact_canonical();
	struct tcti_feature_artifact artifact = *canonical;

	memcpy(mutable_nodes, canonical->nodes, sizeof(mutable_nodes));
	memcpy(mutable_constraints, canonical->constraints,
	       sizeof(mutable_constraints));
	memcpy(mutable_children, canonical->children, sizeof(mutable_children));
	artifact.nodes = mutable_nodes;
	artifact.constraints = mutable_constraints;
	artifact.children = mutable_children;
	return artifact;
}

static int expect_error(const struct tcti_feature_artifact *artifact,
	enum tcti_feature_artifact_error expected)
{
	struct tcti_feature_artifact_diagnostic diagnostic;

	CHECK(tcti_feature_artifact_validate(artifact, &scratch, &diagnostic) ==
	      expected);
	CHECK(diagnostic.error == expected);
	return 0;
}

static int canonical_artifact_is_exact_and_valid(void)
{
	const struct tcti_feature_artifact *artifact =
		tcti_feature_artifact_canonical();
	tcti_feature_artifact_u32 index;
	tcti_feature_artifact_u32 field_count = 0;

	CHECK(artifact != NULL);
	CHECK(artifact->source.length ==
	      TCTI_FEATURE_ARTIFACT_PINNED_SOURCE_LENGTH);
	CHECK(artifact->counts.parameter_count ==
	      TCTI_FEATURE_ARTIFACT_PARAMETER_COUNT);
	CHECK(artifact->counts.constraint_count ==
	      TCTI_FEATURE_ARTIFACT_CONSTRAINT_COUNT);
	CHECK(artifact->counts.parameter_constraint_count ==
	      TCTI_FEATURE_ARTIFACT_PARAMETER_CONSTRAINT_COUNT);
	CHECK(artifact->counts.global_constraint_count ==
	      TCTI_FEATURE_ARTIFACT_GLOBAL_CONSTRAINT_COUNT);
	CHECK(artifact->counts.node_count == TCTI_FEATURE_ARTIFACT_NODE_COUNT);
	CHECK(artifact->counts.child_count == TCTI_FEATURE_ARTIFACT_CHILD_COUNT);
	for (index = 0; index < artifact->counts.node_count; index++) {
		const struct tcti_feature_artifact_node *node = &artifact->nodes[index];

		if (node->kind != TCTI_FEATURE_ARTIFACT_FIELD)
			continue;
		field_count++;
		CHECK(node->field_instance.kind ==
		      TCTI_FEATURE_ARTIFACT_FIELD_QUALIFIER_NULL);
		CHECK(node->field_slices.kind ==
		      TCTI_FEATURE_ARTIFACT_FIELD_QUALIFIER_NULL);
		CHECK(node->field_instance.source.length == 4U);
		CHECK(node->field_slices.source.length == 4U);
		CHECK(node->field_instance.source.offset <
		      node->field_slices.source.offset);
		CHECK(node->field_slices.source.offset <
		      node->field_source.offset + node->field_source.length);
	}
	CHECK(field_count == TCTI_FEATURE_ARTIFACT_FIELD_NODE_COUNT);
	CHECK(tcti_feature_artifact_validate(artifact, &scratch, NULL) ==
	      TCTI_FEATURE_ARTIFACT_VALID);
	return 0;
}

static int rejects_field_qualifier_mutations(void)
{
	struct tcti_feature_artifact artifact = mutable_artifact();
	tcti_feature_artifact_u32 index;

	for (index = 0; index < TCTI_FEATURE_ARTIFACT_NODE_COUNT; index++)
		if (mutable_nodes[index].kind == TCTI_FEATURE_ARTIFACT_FIELD)
			break;
	CHECK(index != TCTI_FEATURE_ARTIFACT_NODE_COUNT);
	mutable_nodes[index].field_instance.kind =
		TCTI_FEATURE_ARTIFACT_FIELD_QUALIFIER_NONE;
	CHECK(expect_error(&artifact, TCTI_FEATURE_ARTIFACT_NODE_INVALID) == 0);

	artifact = mutable_artifact();
	mutable_nodes[index].field_slices.kind =
		TCTI_FEATURE_ARTIFACT_FIELD_QUALIFIER_KIND_COUNT;
	CHECK(expect_error(&artifact, TCTI_FEATURE_ARTIFACT_NODE_INVALID) == 0);

	artifact = mutable_artifact();
	mutable_nodes[index].field_instance.source.offset =
		TCTI_FEATURE_ARTIFACT_PINNED_SOURCE_LENGTH;
	CHECK(expect_error(&artifact,
		   TCTI_FEATURE_ARTIFACT_SOURCE_SPAN_INVALID) == 0);
	return 0;
}

static int rejects_pin_count_span_and_provenance_mutations(void)
{
	struct tcti_feature_artifact artifact = mutable_artifact();
	tcti_feature_artifact_u32 global =
		TCTI_FEATURE_ARTIFACT_PARAMETER_CONSTRAINT_COUNT;

	artifact.source.length--;
	CHECK(expect_error(&artifact, TCTI_FEATURE_ARTIFACT_PIN_MISMATCH) == 0);
	artifact = mutable_artifact();
	artifact.counts.node_count--;
	CHECK(expect_error(&artifact, TCTI_FEATURE_ARTIFACT_COUNT_MISMATCH) == 0);
	artifact = mutable_artifact();
	mutable_constraints[0].source =
		(struct tcti_feature_artifact_span) {
			TCTI_FEATURE_ARTIFACT_PINNED_SOURCE_LENGTH, 1,
		};
	CHECK(expect_error(&artifact,
			   TCTI_FEATURE_ARTIFACT_PROVENANCE_INVALID) == 0);
	artifact = mutable_artifact();
	CHECK(mutable_constraints[global].source.length > 1);
	mutable_constraints[global].source.offset++;
	mutable_constraints[global].source.length--;
	CHECK(expect_error(&artifact,
			   TCTI_FEATURE_ARTIFACT_PROVENANCE_INVALID) == 0);
	return 0;
}

static int node_is_leaf(const struct tcti_feature_artifact_node *node)
{
	return node->left == TCTI_FEATURE_ARTIFACT_NODE_NONE &&
		node->right == TCTI_FEATURE_ARTIFACT_NODE_NONE &&
		!node->child_count;
}

static int rejects_orphan_node(void)
{
	const struct tcti_feature_artifact *canonical =
		tcti_feature_artifact_canonical();
	struct tcti_feature_artifact artifact = mutable_artifact();
	static tcti_feature_artifact_u32
		indegree[TCTI_FEATURE_ARTIFACT_NODE_COUNT];
	static tcti_feature_artifact_u8
		is_root[TCTI_FEATURE_ARTIFACT_NODE_COUNT];
	tcti_feature_artifact_u32 target = TCTI_FEATURE_ARTIFACT_NODE_NONE;
	tcti_feature_artifact_u32 replacement = TCTI_FEATURE_ARTIFACT_NODE_NONE;
	tcti_feature_artifact_u32 index;

	memset(indegree, 0, sizeof(indegree));
	memset(is_root, 0, sizeof(is_root));
	for (index = 0; index < TCTI_FEATURE_ARTIFACT_CONSTRAINT_COUNT; index++) {
		tcti_feature_artifact_u32 root =
			canonical->constraints[index].node_index;

		indegree[root]++;
		is_root[root] = 1;
	}
	for (index = 0; index < TCTI_FEATURE_ARTIFACT_NODE_COUNT; index++) {
		const struct tcti_feature_artifact_node *node =
			&canonical->nodes[index];
		tcti_feature_artifact_u32 child;

		if (node->left != TCTI_FEATURE_ARTIFACT_NODE_NONE)
			indegree[node->left]++;
		if (node->right != TCTI_FEATURE_ARTIFACT_NODE_NONE)
			indegree[node->right]++;
		for (child = 0; child < node->child_count; child++)
			indegree[canonical->children[node->first_child + child]]++;
	}
	for (index = 0; index < TCTI_FEATURE_ARTIFACT_NODE_COUNT; index++)
		if (!is_root[index] && indegree[index] == 1 &&
		    node_is_leaf(&canonical->nodes[index])) {
			target = index;
			break;
		}
	CHECK(target != TCTI_FEATURE_ARTIFACT_NODE_NONE);
	for (index = 0; index < TCTI_FEATURE_ARTIFACT_NODE_COUNT; index++)
		if (index != target && node_is_leaf(&canonical->nodes[index]) &&
		    canonical->nodes[index].kind == canonical->nodes[target].kind) {
			replacement = index;
			break;
		}
	CHECK(replacement != TCTI_FEATURE_ARTIFACT_NODE_NONE);
	for (index = 0; index < TCTI_FEATURE_ARTIFACT_NODE_COUNT; index++) {
		tcti_feature_artifact_u32 child;

		if (mutable_nodes[index].left == target) {
			mutable_nodes[index].left = replacement;
			break;
		}
		if (mutable_nodes[index].right == target) {
			mutable_nodes[index].right = replacement;
			break;
		}
		for (child = 0; child < mutable_nodes[index].child_count; child++) {
			tcti_feature_artifact_u32 slot =
				mutable_nodes[index].first_child + child;

			if (mutable_children[slot] == target) {
				mutable_children[slot] = replacement;
				break;
			}
		}
		if (child != mutable_nodes[index].child_count)
			break;
	}
	CHECK(index != TCTI_FEATURE_ARTIFACT_NODE_COUNT);
	CHECK(expect_error(&artifact,
			   TCTI_FEATURE_ARTIFACT_GRAPH_COVERAGE_INVALID) == 0);
	return 0;
}

static int rejects_overlapping_child_spans(void)
{
	const struct tcti_feature_artifact *canonical =
		tcti_feature_artifact_canonical();
	struct tcti_feature_artifact artifact = mutable_artifact();
	tcti_feature_artifact_u32 first = TCTI_FEATURE_ARTIFACT_NODE_NONE;
	tcti_feature_artifact_u32 second = TCTI_FEATURE_ARTIFACT_NODE_NONE;
	tcti_feature_artifact_u32 index;

	for (index = 0; index < TCTI_FEATURE_ARTIFACT_NODE_COUNT; index++) {
		const struct tcti_feature_artifact_node *node =
			&canonical->nodes[index];

		if (!node->child_count)
			continue;
		if (first == TCTI_FEATURE_ARTIFACT_NODE_NONE) {
			first = index;
			continue;
		}
		if (node->kind == canonical->nodes[first].kind &&
		    node->child_count == canonical->nodes[first].child_count) {
			second = index;
			break;
		}
	}
	CHECK(first != TCTI_FEATURE_ARTIFACT_NODE_NONE);
	CHECK(second != TCTI_FEATURE_ARTIFACT_NODE_NONE);
	mutable_nodes[second].first_child = mutable_nodes[first].first_child;
	CHECK(expect_error(&artifact,
			   TCTI_FEATURE_ARTIFACT_CHILD_COVERAGE_INVALID) == 0);
	return 0;
}

int main(void)
{
	if (canonical_artifact_is_exact_and_valid() ||
	    rejects_field_qualifier_mutations() ||
	    rejects_pin_count_span_and_provenance_mutations() ||
	    rejects_orphan_node() ||
	    rejects_overlapping_child_spans())
		return 1;
	puts("PASS canonical target feature artifact validator");
	return 0;
}
