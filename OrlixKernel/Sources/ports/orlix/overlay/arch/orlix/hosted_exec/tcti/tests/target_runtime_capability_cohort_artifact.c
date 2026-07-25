/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_runtime_capability_cohort_artifact.h"

#include "target_feature_artifact.h"

#ifdef __KERNEL__
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/string.h>
#else
#include <errno.h>
#include <string.h>
#define ARRAY_SIZE(values) (sizeof(values) / sizeof((values)[0]))
#endif

#define TCTI_RUNTIME_COHORT_DEF \
	"../isa/target_runtime_capability_cohort_artifact.def"

#define TCTI_A64_RUNTIME_CAPABILITY_COHORT_SOURCE(...)
#define TCTI_A64_RUNTIME_CAPABILITY_COHORT_COUNTS(...)
#define TCTI_A64_RUNTIME_CAPABILITY_COHORT_LEAF(i, f, c) [i] = { i, f, c },
#define TCTI_A64_RUNTIME_CAPABILITY_COHORT_MEMBERSHIP(...)
static const struct tcti_runtime_capability_cohort_leaf canonical_leaves[] = {
#include TCTI_RUNTIME_COHORT_DEF
};
#undef TCTI_A64_RUNTIME_CAPABILITY_COHORT_SOURCE
#undef TCTI_A64_RUNTIME_CAPABILITY_COHORT_COUNTS
#undef TCTI_A64_RUNTIME_CAPABILITY_COHORT_LEAF
#undef TCTI_A64_RUNTIME_CAPABILITY_COHORT_MEMBERSHIP

#define TCTI_A64_RUNTIME_CAPABILITY_COHORT_SOURCE(...)
#define TCTI_A64_RUNTIME_CAPABILITY_COHORT_COUNTS(...)
#define TCTI_A64_RUNTIME_CAPABILITY_COHORT_LEAF(...)
#define TCTI_A64_RUNTIME_CAPABILITY_COHORT_MEMBERSHIP(i, l, p, n, d, o, z) \
	[i] = { i, l, p, n, d, o, z },
static const struct tcti_runtime_capability_cohort_membership
canonical_memberships[] = {
#include TCTI_RUNTIME_COHORT_DEF
};
#undef TCTI_A64_RUNTIME_CAPABILITY_COHORT_SOURCE
#undef TCTI_A64_RUNTIME_CAPABILITY_COHORT_COUNTS
#undef TCTI_A64_RUNTIME_CAPABILITY_COHORT_LEAF
#undef TCTI_A64_RUNTIME_CAPABILITY_COHORT_MEMBERSHIP

#define TCTI_A64_RUNTIME_CAPABILITY_COHORT_SOURCE(...)
#define TCTI_A64_RUNTIME_CAPABILITY_COHORT_COUNTS(...)
#define TCTI_A64_RUNTIME_CAPABILITY_COHORT_LEAF(...)
#define TCTI_A64_RUNTIME_CAPABILITY_COHORT_MEMBERSHIP(i, l, p, n, d, o, z) \
	[i] = n,
static const char *const canonical_feature_names[] = {
#include TCTI_RUNTIME_COHORT_DEF
};
#undef TCTI_A64_RUNTIME_CAPABILITY_COHORT_SOURCE
#undef TCTI_A64_RUNTIME_CAPABILITY_COHORT_COUNTS
#undef TCTI_A64_RUNTIME_CAPABILITY_COHORT_LEAF
#undef TCTI_A64_RUNTIME_CAPABILITY_COHORT_MEMBERSHIP

static const struct tcti_runtime_capability_cohort_artifact canonical_artifact = {
#define TCTI_A64_RUNTIME_CAPABILITY_COHORT_SOURCE(a, b, r, s, ih, il, fh, fl) \
	.source = { a, b, r, s, ih, il, fh, fl },
#define TCTI_A64_RUNTIME_CAPABILITY_COHORT_COUNTS(l, m, c, u) \
	.counts = { l, m, c, u },
#define TCTI_A64_RUNTIME_CAPABILITY_COHORT_LEAF(...)
#define TCTI_A64_RUNTIME_CAPABILITY_COHORT_MEMBERSHIP(...)
#include TCTI_RUNTIME_COHORT_DEF
#undef TCTI_A64_RUNTIME_CAPABILITY_COHORT_SOURCE
#undef TCTI_A64_RUNTIME_CAPABILITY_COHORT_COUNTS
#undef TCTI_A64_RUNTIME_CAPABILITY_COHORT_LEAF
#undef TCTI_A64_RUNTIME_CAPABILITY_COHORT_MEMBERSHIP
	.leaves = canonical_leaves,
	.memberships = canonical_memberships,
};

static int fail(
	struct tcti_runtime_capability_cohort_validation_result *result,
	enum tcti_runtime_capability_cohort_error error, u32 leaf, u32 membership)
{
	if (result) {
		result->error = error;
		result->leaf_index = leaf;
		result->membership_index = membership;
	}
	return -EINVAL;
}

static int source_matches(
	const struct tcti_runtime_capability_cohort_source *source)
{
	const struct tcti_feature_artifact *features =
		tcti_feature_artifact_canonical();

	return source->architecture && source->build && source->reference &&
		source->schema && source->instructions_sha256 &&
		source->features_sha256 &&
		!strcmp(source->architecture, "vFATAp1-A") &&
		!strcmp(source->build, "818") &&
		!strcmp(source->reference, "2026-06_rel") &&
		!strcmp(source->schema, "2.9.5") &&
		!strcmp(source->instructions_sha256,
			"a1ad2c6538a47cd97d8762791ac5af88"
			"bce1d5f6aff096c9b77aef853e76acfe") &&
		source->instructions_length == 115441429U &&
		features && features->source.sha256 &&
		!strcmp(source->features_sha256, features->source.sha256) &&
		source->features_length == features->source.length &&
		features->counts.parameter_count ==
			TCTI_RUNTIME_CAPABILITY_COHORT_FEATURE_PARAMETER_COUNT;
}

int tcti_runtime_capability_cohort_artifact_validate(
	const struct tcti_runtime_capability_cohort_artifact *artifact,
	struct tcti_runtime_capability_cohort_validation_result *result)
{
	const struct tcti_feature_artifact *features =
		tcti_feature_artifact_canonical();
	size_t expected_first = 0;
	size_t leaf_index;

	if (result)
		*result = (struct tcti_runtime_capability_cohort_validation_result) {
			.error = TCTI_RUNTIME_CAPABILITY_COHORT_VALID,
		};
	if (!artifact || !artifact->leaves || !artifact->memberships)
		return fail(result, TCTI_RUNTIME_CAPABILITY_COHORT_INVALID_ARGUMENT,
			    0, 0);
	if (!source_matches(&artifact->source))
		return fail(result, TCTI_RUNTIME_CAPABILITY_COHORT_SOURCE_MISMATCH,
			    0, 0);
	if (artifact->counts.leaf_count !=
			TCTI_RUNTIME_CAPABILITY_COHORT_LEAF_COUNT ||
	    artifact->counts.membership_count !=
			TCTI_RUNTIME_CAPABILITY_COHORT_MEMBERSHIP_COUNT ||
	    artifact->counts.candidate_membership_count !=
			artifact->counts.membership_count ||
	    artifact->counts.unresolved_membership_count !=
			artifact->counts.membership_count ||
	    ARRAY_SIZE(canonical_leaves) !=
			TCTI_RUNTIME_CAPABILITY_COHORT_LEAF_COUNT ||
	    ARRAY_SIZE(canonical_memberships) !=
			TCTI_RUNTIME_CAPABILITY_COHORT_MEMBERSHIP_COUNT)
		return fail(result, TCTI_RUNTIME_CAPABILITY_COHORT_COUNT_MISMATCH,
			    0, 0);

	for (leaf_index = 0; leaf_index < artifact->counts.leaf_count;
	     leaf_index++) {
		const struct tcti_runtime_capability_cohort_leaf *leaf =
			&artifact->leaves[leaf_index];
		const struct tcti_runtime_capability_cohort_leaf *canonical =
			&canonical_leaves[leaf_index];
		size_t membership_index;
		u32 previous_parameter = 0;

		if (leaf->ordinal != leaf_index ||
		    leaf->first_membership != expected_first ||
		    leaf->membership_count >
			artifact->counts.membership_count - expected_first ||
		    leaf->ordinal != canonical->ordinal ||
		    leaf->first_membership != canonical->first_membership ||
		    leaf->membership_count != canonical->membership_count)
			return fail(result,
				    TCTI_RUNTIME_CAPABILITY_COHORT_LEAF_MISMATCH,
				    (u32)leaf_index, (u32)expected_first);
		for (membership_index = expected_first;
		     membership_index < expected_first + leaf->membership_count;
		     membership_index++) {
			const struct tcti_runtime_capability_cohort_membership *member =
				&artifact->memberships[membership_index];
			const struct tcti_runtime_capability_cohort_membership
				*canonical_member =
					&canonical_memberships[membership_index];

			if (member->order != membership_index ||
			    member->leaf_ordinal != leaf_index ||
			    member->order != canonical_member->order ||
			    member->leaf_ordinal != canonical_member->leaf_ordinal)
				return fail(result,
					    TCTI_RUNTIME_CAPABILITY_COHORT_MEMBERSHIP_MISMATCH,
					    (u32)leaf_index,
					    (u32)membership_index);
			if (member->feature_parameter_index >=
				    features->counts.parameter_count ||
			    !member->feature_name || !member->feature_name[0] ||
			    strcmp(member->feature_name,
				   features->parameters[
					   member->feature_parameter_index].name) ||
			    member->feature_parameter_index !=
				    canonical_member->feature_parameter_index ||
			    strcmp(member->feature_name,
				   canonical_member->feature_name) ||
			    (membership_index > expected_first &&
			     member->feature_parameter_index <= previous_parameter))
				return fail(result,
					    TCTI_RUNTIME_CAPABILITY_COHORT_FEATURE_MISMATCH,
					    (u32)leaf_index,
					    (u32)membership_index);
			if (member->disposition !=
				    TCTI_RUNTIME_CAPABILITY_COHORT_UNRESOLVED ||
			    member->disposition != canonical_member->disposition)
				return fail(result,
					    TCTI_RUNTIME_CAPABILITY_COHORT_DISPOSITION_MISMATCH,
					    (u32)leaf_index,
					    (u32)membership_index);
			if (!member->source_length ||
			    member->source_offset >=
				    artifact->source.instructions_length ||
			    member->source_length >
				    artifact->source.instructions_length -
					    member->source_offset ||
			    member->source_offset != canonical_member->source_offset ||
			    member->source_length != canonical_member->source_length)
				return fail(result,
					    TCTI_RUNTIME_CAPABILITY_COHORT_SOURCE_SPAN_MISMATCH,
					    (u32)leaf_index,
					    (u32)membership_index);
			previous_parameter = member->feature_parameter_index;
		}
		expected_first += leaf->membership_count;
	}
	if (expected_first != artifact->counts.membership_count)
		return fail(result, TCTI_RUNTIME_CAPABILITY_COHORT_COUNT_MISMATCH,
			    (u32)artifact->counts.leaf_count,
			    (u32)expected_first);
	return 0;
}

const struct tcti_runtime_capability_cohort_artifact *
tcti_runtime_capability_cohort_artifact_canonical(void)
{
	return &canonical_artifact;
}

int tcti_runtime_capability_cohort_leaf_features(
	u32 leaf_ordinal, const char *const **features, size_t *feature_count,
	bool *unresolved)
{
	const struct tcti_runtime_capability_cohort_leaf *leaf;

	if (!features || !feature_count || !unresolved ||
	    leaf_ordinal >= ARRAY_SIZE(canonical_leaves))
		return -EINVAL;
	leaf = &canonical_leaves[leaf_ordinal];
	if (leaf->first_membership > ARRAY_SIZE(canonical_feature_names) ||
	    leaf->membership_count >
		    ARRAY_SIZE(canonical_feature_names) - leaf->first_membership)
		return -EINVAL;
	*features = canonical_feature_names + leaf->first_membership;
	*feature_count = leaf->membership_count;
	*unresolved = leaf->membership_count != 0;
	return 0;
}

const char *tcti_runtime_capability_cohort_error_name(
	enum tcti_runtime_capability_cohort_error error)
{
	switch (error) {
	case TCTI_RUNTIME_CAPABILITY_COHORT_VALID:
		return "valid";
	case TCTI_RUNTIME_CAPABILITY_COHORT_INVALID_ARGUMENT:
		return "invalid argument";
	case TCTI_RUNTIME_CAPABILITY_COHORT_SOURCE_MISMATCH:
		return "source mismatch";
	case TCTI_RUNTIME_CAPABILITY_COHORT_COUNT_MISMATCH:
		return "count mismatch";
	case TCTI_RUNTIME_CAPABILITY_COHORT_LEAF_MISMATCH:
		return "leaf mismatch";
	case TCTI_RUNTIME_CAPABILITY_COHORT_MEMBERSHIP_MISMATCH:
		return "membership mismatch";
	case TCTI_RUNTIME_CAPABILITY_COHORT_FEATURE_MISMATCH:
		return "feature mismatch";
	case TCTI_RUNTIME_CAPABILITY_COHORT_DISPOSITION_MISMATCH:
		return "disposition mismatch";
	case TCTI_RUNTIME_CAPABILITY_COHORT_SOURCE_SPAN_MISMATCH:
		return "source span mismatch";
	}
	return "unknown";
}
