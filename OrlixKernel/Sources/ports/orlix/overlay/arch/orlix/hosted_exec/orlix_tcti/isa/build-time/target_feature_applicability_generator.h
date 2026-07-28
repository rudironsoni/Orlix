/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_GENERATOR_H
#define ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_GENERATOR_H

#include <stdio.h>

#include "target_feature_model.h"
#include "target_feature_sat.h"
#include "target_inventory_import.h"

/*
 * A build-time C artifact which records one exact applicability result for
 * every pinned AARCHMRS leaf.  The SAT audit is its only result authority.
 */
enum orlix_tcti_target_feature_applicability_error {
	ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_OK = 0,
	ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_INVALID_ARGUMENT,
	ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_PIN_MISMATCH,
	ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_AUDIT_MISMATCH,
	ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_WITNESS_UNAVAILABLE,
	ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_WITNESS_INVALID,
	ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_CERTIFICATE_INVALID,
	ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_UNSAT_UNCERTIFIED,
	ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_CONDITION_INVALID,
	ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_IO,
};

/*
 * Emits a deterministic macro include only after model, inventory, and audit
 * agree on every pinned ordinal.  Applicable rows contain the exact signed
 * parameter assignment returned by target_feature_sat, encoded as C bytes.
 * On validation failure the destination receives no bytes.
 */
enum orlix_tcti_target_feature_applicability_error
orlix_tcti_target_feature_applicability_emit(const struct orlix_tcti_feature_model *model,
	const struct orlix_tcti_target_inventory *inventory,
	const struct orlix_tcti_target_feature_sat_audit *audit,
	FILE *output);

const char *orlix_tcti_target_feature_applicability_error_name(
	enum orlix_tcti_target_feature_applicability_error error);

#endif /* ORLIX_TCTI_TARGET_FEATURE_APPLICABILITY_GENERATOR_H */
