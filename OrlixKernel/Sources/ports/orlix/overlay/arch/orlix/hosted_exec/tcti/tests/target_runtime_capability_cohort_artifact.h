/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TARGET_RUNTIME_CAPABILITY_COHORT_ARTIFACT_H
#define ORLIX_TCTI_TARGET_RUNTIME_CAPABILITY_COHORT_ARTIFACT_H

#ifdef __KERNEL__
#include <linux/stddef.h>
#include <linux/types.h>
#else
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef uint32_t u32;
#endif

#define TCTI_RUNTIME_CAPABILITY_COHORT_LEAF_COUNT 4350U
#define TCTI_RUNTIME_CAPABILITY_COHORT_FEATURE_PARAMETER_COUNT 409U
#define TCTI_RUNTIME_CAPABILITY_COHORT_MEMBERSHIP_COUNT 5592U

enum tcti_runtime_capability_cohort_disposition {
	TCTI_RUNTIME_CAPABILITY_COHORT_UNRESOLVED = 0,
};

struct tcti_runtime_capability_cohort_source {
	const char *architecture;
	const char *build;
	const char *reference;
	const char *schema;
	const char *instructions_sha256;
	size_t instructions_length;
	const char *features_sha256;
	size_t features_length;
};

struct tcti_runtime_capability_cohort_counts {
	size_t leaf_count;
	size_t membership_count;
	size_t candidate_membership_count;
	size_t unresolved_membership_count;
};

struct tcti_runtime_capability_cohort_leaf {
	u32 ordinal;
	u32 first_membership;
	u32 membership_count;
};

struct tcti_runtime_capability_cohort_membership {
	u32 order;
	u32 leaf_ordinal;
	u32 feature_parameter_index;
	const char *feature_name;
	enum tcti_runtime_capability_cohort_disposition disposition;
	u32 source_offset;
	u32 source_length;
};

struct tcti_runtime_capability_cohort_artifact {
	struct tcti_runtime_capability_cohort_source source;
	struct tcti_runtime_capability_cohort_counts counts;
	const struct tcti_runtime_capability_cohort_leaf *leaves;
	const struct tcti_runtime_capability_cohort_membership *memberships;
};

enum tcti_runtime_capability_cohort_error {
	TCTI_RUNTIME_CAPABILITY_COHORT_VALID = 0,
	TCTI_RUNTIME_CAPABILITY_COHORT_INVALID_ARGUMENT,
	TCTI_RUNTIME_CAPABILITY_COHORT_SOURCE_MISMATCH,
	TCTI_RUNTIME_CAPABILITY_COHORT_COUNT_MISMATCH,
	TCTI_RUNTIME_CAPABILITY_COHORT_LEAF_MISMATCH,
	TCTI_RUNTIME_CAPABILITY_COHORT_MEMBERSHIP_MISMATCH,
	TCTI_RUNTIME_CAPABILITY_COHORT_FEATURE_MISMATCH,
	TCTI_RUNTIME_CAPABILITY_COHORT_DISPOSITION_MISMATCH,
	TCTI_RUNTIME_CAPABILITY_COHORT_SOURCE_SPAN_MISMATCH,
};

struct tcti_runtime_capability_cohort_validation_result {
	enum tcti_runtime_capability_cohort_error error;
	u32 leaf_index;
	u32 membership_index;
};

int tcti_runtime_capability_cohort_artifact_validate(
	const struct tcti_runtime_capability_cohort_artifact *artifact,
	struct tcti_runtime_capability_cohort_validation_result *result);

const struct tcti_runtime_capability_cohort_artifact *
tcti_runtime_capability_cohort_artifact_canonical(void);

/*
 * Return the canonical feature candidates for one direct source leaf. All
 * current candidates remain semantically unresolved and therefore cannot
 * authorize a runtime capability.
 */
int tcti_runtime_capability_cohort_leaf_features(
	u32 leaf_ordinal, const char *const **features, size_t *feature_count,
	bool *unresolved);

const char *tcti_runtime_capability_cohort_error_name(
	enum tcti_runtime_capability_cohort_error error);

#endif
