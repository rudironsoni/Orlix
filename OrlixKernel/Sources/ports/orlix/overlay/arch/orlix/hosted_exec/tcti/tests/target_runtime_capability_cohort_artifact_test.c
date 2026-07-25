/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_runtime_capability_cohort_artifact.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expression) do { \
	if (!(expression)) { \
		fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, \
			#expression); \
		return -1; \
	} \
} while (0)

#define TCTI_A64_RUNTIME_CAPABILITY(word, bit, feature, extension) #feature,
static const char *const runtime_features[] = {
#include "../isa/runtime_profile.def"
};
#undef TCTI_A64_RUNTIME_CAPABILITY

static int canonical_is_complete_and_unresolved(void)
{
	const struct tcti_runtime_capability_cohort_artifact *artifact =
		tcti_runtime_capability_cohort_artifact_canonical();
	struct tcti_runtime_capability_cohort_validation_result result;
	size_t mapping;

	if (tcti_runtime_capability_cohort_artifact_validate(artifact, &result)) {
		fprintf(stderr, "canonical cohort invalid: %s leaf=%u member=%u\n",
			tcti_runtime_capability_cohort_error_name(result.error),
			result.leaf_index, result.membership_index);
		return -1;
	}
	CHECK(result.error == TCTI_RUNTIME_CAPABILITY_COHORT_VALID);
	CHECK(artifact->counts.leaf_count ==
	      TCTI_RUNTIME_CAPABILITY_COHORT_LEAF_COUNT);
	CHECK(artifact->counts.membership_count ==
	      TCTI_RUNTIME_CAPABILITY_COHORT_MEMBERSHIP_COUNT);
	CHECK(artifact->counts.candidate_membership_count ==
	      artifact->counts.membership_count);
	CHECK(artifact->counts.unresolved_membership_count ==
	      artifact->counts.membership_count);

	for (mapping = 0;
	     mapping < sizeof(runtime_features) / sizeof(runtime_features[0]);
	     mapping++) {
		size_t member;
		int found = 0;

		for (member = 0; member < artifact->counts.membership_count;
		     member++)
			if (!strcmp(runtime_features[mapping],
				    artifact->memberships[member].feature_name)) {
				found = 1;
				break;
			}
		CHECK(found);
	}
	return 0;
}

static int canonical_leaf_view_is_bounded(void)
{
	const char *const *features;
	size_t feature_count;
	bool unresolved;

	CHECK(!tcti_runtime_capability_cohort_leaf_features(
		0, &features, &feature_count, &unresolved));
	CHECK(features);
	CHECK(feature_count == 2);
	CHECK(unresolved);
	CHECK(tcti_runtime_capability_cohort_leaf_features(
		TCTI_RUNTIME_CAPABILITY_COHORT_LEAF_COUNT, &features,
		&feature_count, &unresolved) < 0);
	CHECK(tcti_runtime_capability_cohort_leaf_features(
		0, NULL, &feature_count, &unresolved) < 0);
	return 0;
}

static int source_and_count_drift_fail(void)
{
	const struct tcti_runtime_capability_cohort_artifact *canonical =
		tcti_runtime_capability_cohort_artifact_canonical();
	struct tcti_runtime_capability_cohort_artifact artifact = *canonical;
	struct tcti_runtime_capability_cohort_validation_result result;

	artifact.source.instructions_sha256 = "stale";
	CHECK(tcti_runtime_capability_cohort_artifact_validate(&artifact,
							      &result) < 0);
	CHECK(result.error == TCTI_RUNTIME_CAPABILITY_COHORT_SOURCE_MISMATCH);
	artifact = *canonical;
	artifact.counts.membership_count--;
	CHECK(tcti_runtime_capability_cohort_artifact_validate(&artifact,
							      &result) < 0);
	CHECK(result.error == TCTI_RUNTIME_CAPABILITY_COHORT_COUNT_MISMATCH);
	artifact = *canonical;
	artifact.counts.unresolved_membership_count--;
	CHECK(tcti_runtime_capability_cohort_artifact_validate(&artifact,
							      &result) < 0);
	CHECK(result.error == TCTI_RUNTIME_CAPABILITY_COHORT_COUNT_MISMATCH);
	return 0;
}

static int leaf_and_membership_drift_fail(void)
{
	const struct tcti_runtime_capability_cohort_artifact *canonical =
		tcti_runtime_capability_cohort_artifact_canonical();
	struct tcti_runtime_capability_cohort_artifact artifact = *canonical;
	struct tcti_runtime_capability_cohort_leaf *leaves;
	struct tcti_runtime_capability_cohort_membership *memberships;
	struct tcti_runtime_capability_cohort_validation_result result;

	leaves = malloc(canonical->counts.leaf_count * sizeof(*leaves));
	memberships = malloc(canonical->counts.membership_count *
			     sizeof(*memberships));
	CHECK(leaves && memberships);
	memcpy(leaves, canonical->leaves,
	       canonical->counts.leaf_count * sizeof(*leaves));
	memcpy(memberships, canonical->memberships,
	       canonical->counts.membership_count * sizeof(*memberships));
	artifact.leaves = leaves;
	artifact.memberships = memberships;

	leaves[0].first_membership++;
	CHECK(tcti_runtime_capability_cohort_artifact_validate(&artifact,
							      &result) < 0);
	CHECK(result.error == TCTI_RUNTIME_CAPABILITY_COHORT_LEAF_MISMATCH);
	leaves[0] = canonical->leaves[0];

	memberships[0].leaf_ordinal++;
	CHECK(tcti_runtime_capability_cohort_artifact_validate(&artifact,
							      &result) < 0);
	CHECK(result.error ==
	      TCTI_RUNTIME_CAPABILITY_COHORT_MEMBERSHIP_MISMATCH);
	memberships[0] = canonical->memberships[0];

	memberships[0].feature_parameter_index =
		canonical->memberships[1].feature_parameter_index;
	CHECK(tcti_runtime_capability_cohort_artifact_validate(&artifact,
							      &result) < 0);
	CHECK(result.error == TCTI_RUNTIME_CAPABILITY_COHORT_FEATURE_MISMATCH);
	memberships[0] = canonical->memberships[0];

	memberships[0].feature_name = "FEAT_NOT_THE_BOUND_PARAMETER";
	CHECK(tcti_runtime_capability_cohort_artifact_validate(&artifact,
							      &result) < 0);
	CHECK(result.error == TCTI_RUNTIME_CAPABILITY_COHORT_FEATURE_MISMATCH);
	memberships[0] = canonical->memberships[0];

	memberships[0].disposition =
		(enum tcti_runtime_capability_cohort_disposition)1;
	CHECK(tcti_runtime_capability_cohort_artifact_validate(&artifact,
							      &result) < 0);
	CHECK(result.error ==
	      TCTI_RUNTIME_CAPABILITY_COHORT_DISPOSITION_MISMATCH);
	memberships[0] = canonical->memberships[0];

	memberships[0].source_length = 0;
	CHECK(tcti_runtime_capability_cohort_artifact_validate(&artifact,
							      &result) < 0);
	CHECK(result.error ==
	      TCTI_RUNTIME_CAPABILITY_COHORT_SOURCE_SPAN_MISMATCH);

	free(memberships);
	free(leaves);
	return 0;
}

int main(void)
{
	if (canonical_is_complete_and_unresolved() ||
	    canonical_leaf_view_is_bounded() ||
	    source_and_count_drift_fail() ||
	    leaf_and_membership_drift_fail())
		return EXIT_FAILURE;
	puts("runtime capability cohort artifact tests: passed");
	return EXIT_SUCCESS;
}
