/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TARGET_CONDITION_SERIALIZATION_H
#define ORLIX_TCTI_TARGET_CONDITION_SERIALIZATION_H

#include <stddef.h>
#include <stdint.h>

#include "target_condition_format.h"
#include "target_inventory_import.h"

enum orlix_tcti_target_condition_serialize_error {
	ORLIX_TCTI_TARGET_CONDITION_SERIALIZE_OK = 0,
	ORLIX_TCTI_TARGET_CONDITION_SERIALIZE_INVALID_ARGUMENT,
	ORLIX_TCTI_TARGET_CONDITION_SERIALIZE_INVALID_EXPRESSION,
	ORLIX_TCTI_TARGET_CONDITION_SERIALIZE_TOO_DEEP,
	ORLIX_TCTI_TARGET_CONDITION_SERIALIZE_TOO_LARGE,
	ORLIX_TCTI_TARGET_CONDITION_SERIALIZE_NO_MEMORY,
};

struct orlix_tcti_target_condition_bytes {
	uint8_t *data;
	size_t length;
};

/*
 * Produces: "TCND", version 1, then one length-delimited expression record.
 * Expression children and set members retain their imported source order.
 */
int orlix_tcti_target_condition_serialize(
	const struct orlix_tcti_target_inventory *inventory, uint32_t condition,
	struct orlix_tcti_target_condition_bytes *bytes,
	enum orlix_tcti_target_condition_serialize_error *error);

void orlix_tcti_target_condition_bytes_destroy(
	struct orlix_tcti_target_condition_bytes *bytes);

#endif /* ORLIX_TCTI_TARGET_CONDITION_SERIALIZATION_H */
