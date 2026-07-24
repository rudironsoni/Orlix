/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TARGET_CONDITION_SERIALIZATION_H
#define ORLIX_TCTI_TARGET_CONDITION_SERIALIZATION_H

#include <stddef.h>
#include <stdint.h>

#include "target_condition_format.h"
#include "target_inventory_import.h"

enum tcti_target_condition_serialize_error {
	TCTI_TARGET_CONDITION_SERIALIZE_OK = 0,
	TCTI_TARGET_CONDITION_SERIALIZE_INVALID_ARGUMENT,
	TCTI_TARGET_CONDITION_SERIALIZE_INVALID_EXPRESSION,
	TCTI_TARGET_CONDITION_SERIALIZE_TOO_DEEP,
	TCTI_TARGET_CONDITION_SERIALIZE_TOO_LARGE,
	TCTI_TARGET_CONDITION_SERIALIZE_NO_MEMORY,
};

struct tcti_target_condition_bytes {
	uint8_t *data;
	size_t length;
};

/*
 * Produces: "TCND", version 1, then one length-delimited expression record.
 * Expression children and set members retain their imported source order.
 */
int tcti_target_condition_serialize(
	const struct tcti_target_inventory *inventory, uint32_t condition,
	struct tcti_target_condition_bytes *bytes,
	enum tcti_target_condition_serialize_error *error);

void tcti_target_condition_bytes_destroy(
	struct tcti_target_condition_bytes *bytes);

#endif /* ORLIX_TCTI_TARGET_CONDITION_SERIALIZATION_H */
