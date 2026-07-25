/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TARGET_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_GENERATOR_H
#define ORLIX_TCTI_TARGET_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_GENERATOR_H

#include <stddef.h>
#include <stdio.h>

#include "target_feature_field_domain_binding.h"

#define TCTI_FEATURE_FIELD_DOMAIN_BINDING_EXPECTED_OCCURRENCES 605U
#define TCTI_FEATURE_FIELD_DOMAIN_BINDING_EXPECTED_IDENTITY_GROUPS 362U
#define TCTI_FEATURE_FIELD_DOMAIN_BINDING_EXPECTED_MAPPED 604U
#define TCTI_FEATURE_FIELD_DOMAIN_BINDING_EXPECTED_AMBIGUOUS_FIELD 1U

enum tcti_feature_field_domain_binding_artifact_error {
	TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_OK,
	TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_INVALID_ARGUMENT,
	TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_FEATURE_IMPORT,
	TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_REGISTER_IMPORT,
	TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_BINDING,
	TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_INVALID_BINDING,
	TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_SOURCE_SPAN,
	TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_COUNT_MISMATCH,
	TCTI_FEATURE_FIELD_DOMAIN_BINDING_ARTIFACT_IO,
};

/*
 * Serializes every Types.Field occurrence in source order.  It preserves the
 * exact feature and register spans, rather than lowering a field reference to
 * a runtime capability decision.  Blocking dispositions are emitted too.
 */
enum tcti_feature_field_domain_binding_artifact_error
tcti_target_feature_field_domain_binding_artifact_validate(
	const struct tcti_feature_model *features,
	size_t feature_source_length,
	const struct tcti_register_model *registers,
	size_t register_source_length,
	const struct tcti_feature_field_domain_bindings *bindings);

enum tcti_feature_field_domain_binding_artifact_error
tcti_target_feature_field_domain_binding_artifact_emit_model(
	const struct tcti_feature_model *features,
	size_t feature_source_length,
	const struct tcti_register_model *registers,
	size_t register_source_length, FILE *output);

enum tcti_feature_field_domain_binding_artifact_error
tcti_target_feature_field_domain_binding_artifact_emit(
	const char *feature_source, size_t feature_length,
	const char *register_source, size_t register_length, FILE *output);

const char *tcti_target_feature_field_domain_binding_artifact_error_name(
	enum tcti_feature_field_domain_binding_artifact_error error);

#endif
