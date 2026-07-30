/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_RUNTIME_FEATURE_CONDITION_H
#define ORLIX_TCTI_RUNTIME_FEATURE_CONDITION_H

#include "decode_aarch64.h"
#include "target_feature_domain.h"

enum orlix_tcti_runtime_feature_condition_status {
	ORLIX_TCTI_RUNTIME_FEATURE_CONDITION_ALLOWED,
	ORLIX_TCTI_RUNTIME_FEATURE_CONDITION_FALSE,
	ORLIX_TCTI_RUNTIME_FEATURE_CONDITION_NO_CANDIDATE,
	ORLIX_TCTI_RUNTIME_FEATURE_CONDITION_AMBIGUOUS,
	ORLIX_TCTI_RUNTIME_FEATURE_CONDITION_INVALID_ARTIFACT,
	ORLIX_TCTI_RUNTIME_FEATURE_CONDITION_EVALUATION_ERROR,
};

/* A named EL0 execution assignment. It is deliberately unrelated to HWCAP. */
struct orlix_tcti_el0_feature_profile {
	const char *identity;
	const struct orlix_tcti_feature_domain_tcnd_environment *environment;
};

struct orlix_tcti_runtime_feature_condition_result {
	enum orlix_tcti_runtime_feature_condition_status status;
	u32 source_ordinal;
	u32 matching_candidates;
	struct orlix_tcti_feature_domain_tcnd_diagnostic diagnostic;
};

struct orlix_tcti_target_instruction_artifact;
struct orlix_tcti_feature_artifact;

int orlix_tcti_runtime_feature_condition_validate_artifacts(
	const struct orlix_tcti_target_instruction_artifact *instructions,
	const struct orlix_tcti_feature_artifact *features);

const struct orlix_tcti_el0_feature_profile *
orlix_tcti_production_el0_execution_profile(void);

int orlix_tcti_runtime_feature_condition_authorize(
	u32 instruction, struct orlix_tcti_decoded_instruction *decoded,
	const struct orlix_tcti_el0_feature_profile *profile,
	struct orlix_tcti_runtime_feature_condition_result *result);

#endif /* ORLIX_TCTI_RUNTIME_FEATURE_CONDITION_H */
