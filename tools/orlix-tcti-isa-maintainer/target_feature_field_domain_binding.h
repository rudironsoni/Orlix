/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TARGET_FEATURE_FIELD_DOMAIN_BINDING_H
#define ORLIX_TCTI_TARGET_FEATURE_FIELD_DOMAIN_BINDING_H

#include <stddef.h>
#include <stdint.h>

#include "target_feature_model.h"
#include "target_register_model.h"

/*
 * A Types.Field in Features is a reference to a named field of a concrete
 * architectural register.  This bridge retains the exact model indexes and
 * source spans.  It deliberately does not evaluate a predicate or choose a
 * value from the register domain.
 */
enum orlix_tcti_feature_field_domain_disposition {
	ORLIX_TCTI_FEATURE_FIELD_DOMAIN_MAPPED,
	ORLIX_TCTI_FEATURE_FIELD_DOMAIN_MISSING_REGISTER,
	ORLIX_TCTI_FEATURE_FIELD_DOMAIN_AMBIGUOUS_REGISTER,
	ORLIX_TCTI_FEATURE_FIELD_DOMAIN_MISSING_FIELD,
	ORLIX_TCTI_FEATURE_FIELD_DOMAIN_AMBIGUOUS_FIELD,
	ORLIX_TCTI_FEATURE_FIELD_DOMAIN_INVALID_MODEL,
};

struct orlix_tcti_feature_field_domain_binding {
	/* Every source AST.Field has a row. Equal identities share this group. */
	uint32_t feature_node_index;
	uint32_t identity_group_index;
	uint32_t occurrence_count;
	uint32_t register_index;
	uint32_t field_index;
	uint32_t value_relation_index;
	enum orlix_tcti_feature_field_domain_disposition disposition;
	struct orlix_tcti_feature_provenance feature_provenance;
	size_t register_source_offset;
	size_t register_source_length;
	size_t field_source_offset;
	size_t field_source_length;
	size_t value_relation_source_offset;
	size_t value_relation_source_length;
};

struct orlix_tcti_feature_field_domain_bindings {
	struct orlix_tcti_feature_field_domain_binding *items;
	size_t count;
};

enum orlix_tcti_feature_field_domain_binding_error_code {
	ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_OK,
	ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_INVALID_ARGUMENT,
	ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_NO_MEMORY,
	ORLIX_TCTI_FEATURE_FIELD_DOMAIN_BINDING_INVALID_MODEL,
};

struct orlix_tcti_feature_field_domain_binding_error {
	enum orlix_tcti_feature_field_domain_binding_error_code code;
	size_t offset;
	char message[160];
};

int orlix_tcti_target_feature_field_domain_bindings_build(
	const struct orlix_tcti_feature_model *features,
	const struct orlix_tcti_register_model *registers,
	struct orlix_tcti_feature_field_domain_bindings *bindings,
	struct orlix_tcti_feature_field_domain_binding_error *error);
void orlix_tcti_target_feature_field_domain_bindings_destroy(
	struct orlix_tcti_feature_field_domain_bindings *bindings);

#endif
