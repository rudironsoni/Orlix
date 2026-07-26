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

#define ORLIX_TCTI_RUNTIME_CAPABILITY_COHORT_LEAF_COUNT 4350U
#define ORLIX_TCTI_RUNTIME_CAPABILITY_COHORT_FEATURE_PARAMETER_COUNT 409U
#define ORLIX_TCTI_RUNTIME_CAPABILITY_COHORT_MEMBERSHIP_COUNT 5592U

enum orlix_tcti_runtime_capability_cohort_disposition {
	ORLIX_TCTI_RUNTIME_CAPABILITY_COHORT_UNRESOLVED = 0,
};

struct orlix_tcti_runtime_capability_cohort_source {
	const char *architecture;
	const char *build;
	const char *reference;
	const char *schema;
	const char *instructions_sha256;
	size_t instructions_length;
	const char *features_sha256;
	size_t features_length;
};

struct orlix_tcti_runtime_capability_cohort_counts {
	size_t leaf_count;
	size_t membership_count;
	size_t candidate_membership_count;
	size_t unresolved_membership_count;
};

struct orlix_tcti_runtime_capability_cohort_leaf {
	u32 ordinal;
	u32 first_membership;
	u32 membership_count;
};

struct orlix_tcti_runtime_capability_cohort_membership {
	u32 order;
	u32 leaf_ordinal;
	u32 feature_parameter_index;
	const char *feature_name;
	enum orlix_tcti_runtime_capability_cohort_disposition disposition;
	u32 source_offset;
	u32 source_length;
};

struct orlix_tcti_runtime_capability_cohort_artifact {
	struct orlix_tcti_runtime_capability_cohort_source source;
	struct orlix_tcti_runtime_capability_cohort_counts counts;
	const struct orlix_tcti_runtime_capability_cohort_leaf *leaves;
	const struct orlix_tcti_runtime_capability_cohort_membership *memberships;
};

enum orlix_tcti_runtime_capability_cohort_error {
	ORLIX_TCTI_RUNTIME_CAPABILITY_COHORT_VALID = 0,
	ORLIX_TCTI_RUNTIME_CAPABILITY_COHORT_INVALID_ARGUMENT,
	ORLIX_TCTI_RUNTIME_CAPABILITY_COHORT_SOURCE_MISMATCH,
	ORLIX_TCTI_RUNTIME_CAPABILITY_COHORT_COUNT_MISMATCH,
	ORLIX_TCTI_RUNTIME_CAPABILITY_COHORT_LEAF_MISMATCH,
	ORLIX_TCTI_RUNTIME_CAPABILITY_COHORT_MEMBERSHIP_MISMATCH,
	ORLIX_TCTI_RUNTIME_CAPABILITY_COHORT_FEATURE_MISMATCH,
	ORLIX_TCTI_RUNTIME_CAPABILITY_COHORT_DISPOSITION_MISMATCH,
	ORLIX_TCTI_RUNTIME_CAPABILITY_COHORT_SOURCE_SPAN_MISMATCH,
};

struct orlix_tcti_runtime_capability_cohort_validation_result {
	enum orlix_tcti_runtime_capability_cohort_error error;
	u32 leaf_index;
	u32 membership_index;
};

int orlix_tcti_runtime_capability_cohort_artifact_validate(
	const struct orlix_tcti_runtime_capability_cohort_artifact *artifact,
	struct orlix_tcti_runtime_capability_cohort_validation_result *result);

const struct orlix_tcti_runtime_capability_cohort_artifact *
orlix_tcti_runtime_capability_cohort_artifact_canonical(void);

/*
 * Return the canonical feature candidates for one direct source leaf. All
 * current candidates remain semantically unresolved and therefore cannot
 * authorize a runtime capability.
 */
int orlix_tcti_runtime_capability_cohort_leaf_features(
	u32 leaf_ordinal, const char *const **features, size_t *feature_count,
	bool *unresolved);

const char *orlix_tcti_runtime_capability_cohort_error_name(
	enum orlix_tcti_runtime_capability_cohort_error error);

#endif
