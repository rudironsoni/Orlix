/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_condition_serialization.h"

#include <stdlib.h>
#include <string.h>

struct serializer {
	uint8_t *data;
	size_t length;
	size_t capacity;
	const struct orlix_tcti_target_inventory *inventory;
	enum orlix_tcti_target_condition_serialize_error error;
};

static int reserve(struct serializer *serializer, size_t extra)
{
	size_t needed;
	size_t capacity;
	uint8_t *data;

	if (extra > ORLIX_TCTI_TARGET_CONDITION_MAX_SERIALIZED_BYTES -
		    serializer->length) {
		serializer->error = ORLIX_TCTI_TARGET_CONDITION_SERIALIZE_TOO_LARGE;
		return -1;
	}
	needed = serializer->length + extra;
	if (needed <= serializer->capacity)
		return 0;
	capacity = serializer->capacity ? serializer->capacity : 64U;
	while (capacity < needed) {
		if (capacity > ORLIX_TCTI_TARGET_CONDITION_MAX_SERIALIZED_BYTES / 2U) {
			capacity = ORLIX_TCTI_TARGET_CONDITION_MAX_SERIALIZED_BYTES;
			break;
		}
		capacity *= 2U;
	}
	data = realloc(serializer->data, capacity);
	if (!data) {
		serializer->error = ORLIX_TCTI_TARGET_CONDITION_SERIALIZE_NO_MEMORY;
		return -1;
	}
	serializer->data = data;
	serializer->capacity = capacity;
	return 0;
}

static int append_bytes(struct serializer *serializer, const void *data,
				size_t length)
{
	if (reserve(serializer, length))
		return -1;
	memcpy(serializer->data + serializer->length, data, length);
	serializer->length += length;
	return 0;
}

static int append_u32be(struct serializer *serializer, uint32_t value)
{
	uint8_t bytes[4] = {
		(uint8_t)(value >> 24), (uint8_t)(value >> 16),
		(uint8_t)(value >> 8), (uint8_t)value,
	};

	return append_bytes(serializer, bytes, sizeof(bytes));
}

static int append_record(struct serializer *serializer, uint8_t tag,
			 size_t payload_start)
{
	size_t payload_length = serializer->length - payload_start;
	size_t record_start = payload_start - 5U;

	if (payload_length > UINT32_MAX) {
		serializer->error = ORLIX_TCTI_TARGET_CONDITION_SERIALIZE_TOO_LARGE;
		return -1;
	}
	serializer->data[record_start] = tag;
	serializer->data[record_start + 1] = (uint8_t)(payload_length >> 24);
	serializer->data[record_start + 2] = (uint8_t)(payload_length >> 16);
	serializer->data[record_start + 3] = (uint8_t)(payload_length >> 8);
	serializer->data[record_start + 4] = (uint8_t)payload_length;
	return 0;
}

static int begin_record(struct serializer *serializer, size_t *payload_start)
{
	static const uint8_t placeholder[5];

	if (append_bytes(serializer, placeholder, sizeof(placeholder)))
		return -1;
	*payload_start = serializer->length;
	return 0;
}

static int tag_for_kind(enum orlix_tcti_target_expr_kind kind, uint8_t *tag)
{
	switch (kind) {
	case ORLIX_TCTI_TARGET_EXPR_BOOL: *tag = ORLIX_TCTI_TARGET_CONDITION_BOOL; return 0;
	case ORLIX_TCTI_TARGET_EXPR_FEATURE: *tag = ORLIX_TCTI_TARGET_CONDITION_FEATURE; return 0;
	case ORLIX_TCTI_TARGET_EXPR_OPERAND: *tag = ORLIX_TCTI_TARGET_CONDITION_OPERAND; return 0;
	case ORLIX_TCTI_TARGET_EXPR_VALUE: *tag = ORLIX_TCTI_TARGET_CONDITION_VALUE; return 0;
	case ORLIX_TCTI_TARGET_EXPR_SET: *tag = ORLIX_TCTI_TARGET_CONDITION_SET; return 0;
	case ORLIX_TCTI_TARGET_EXPR_NOT: *tag = ORLIX_TCTI_TARGET_CONDITION_NOT; return 0;
	case ORLIX_TCTI_TARGET_EXPR_AND: *tag = ORLIX_TCTI_TARGET_CONDITION_AND; return 0;
	case ORLIX_TCTI_TARGET_EXPR_OR: *tag = ORLIX_TCTI_TARGET_CONDITION_OR; return 0;
	case ORLIX_TCTI_TARGET_EXPR_EQ: *tag = ORLIX_TCTI_TARGET_CONDITION_EQ; return 0;
	case ORLIX_TCTI_TARGET_EXPR_NE: *tag = ORLIX_TCTI_TARGET_CONDITION_NE; return 0;
	case ORLIX_TCTI_TARGET_EXPR_IN: *tag = ORLIX_TCTI_TARGET_CONDITION_IN; return 0;
	}
	return -1;
}

static int serialize_expression(struct serializer *serializer, uint32_t index,
				size_t depth)
{
	const struct orlix_tcti_target_expr *expression;
	size_t payload_start;
	uint8_t tag;
	size_t text_length;
	uint32_t item;

	if (depth >= ORLIX_TCTI_TARGET_CONDITION_MAX_DEPTH) {
		serializer->error = ORLIX_TCTI_TARGET_CONDITION_SERIALIZE_TOO_DEEP;
		return -1;
	}
	if (index == ORLIX_TCTI_TARGET_EXPR_NONE ||
	    index >= serializer->inventory->expression_count) {
		serializer->error = ORLIX_TCTI_TARGET_CONDITION_SERIALIZE_INVALID_EXPRESSION;
		return -1;
	}
	expression = &serializer->inventory->expressions[index];
	if (tag_for_kind(expression->kind, &tag)) {
		serializer->error = ORLIX_TCTI_TARGET_CONDITION_SERIALIZE_INVALID_EXPRESSION;
		return -1;
	}
	if (begin_record(serializer, &payload_start))
		return -1;
	switch (expression->kind) {
	case ORLIX_TCTI_TARGET_EXPR_BOOL:
		if (append_bytes(serializer, &expression->boolean, 1U))
			return -1;
		break;
	case ORLIX_TCTI_TARGET_EXPR_FEATURE:
	case ORLIX_TCTI_TARGET_EXPR_OPERAND:
	case ORLIX_TCTI_TARGET_EXPR_VALUE:
		if (!expression->text) {
			serializer->error = ORLIX_TCTI_TARGET_CONDITION_SERIALIZE_INVALID_EXPRESSION;
			return -1;
		}
		text_length = strlen(expression->text);
		if (text_length > UINT32_MAX ||
		    append_u32be(serializer, (uint32_t)text_length) ||
		    append_bytes(serializer, expression->text, text_length))
			return -1;
		break;
	case ORLIX_TCTI_TARGET_EXPR_SET:
		if (expression->first_item > serializer->inventory->set_item_count ||
		    expression->item_count > serializer->inventory->set_item_count -
			    expression->first_item ||
		    append_u32be(serializer, expression->item_count)) {
			if (!serializer->error)
				serializer->error = ORLIX_TCTI_TARGET_CONDITION_SERIALIZE_INVALID_EXPRESSION;
			return -1;
		}
		for (item = 0; item < expression->item_count; item++)
			if (serialize_expression(serializer,
				serializer->inventory->set_items[expression->first_item + item],
				depth + 1U))
				return -1;
		break;
	case ORLIX_TCTI_TARGET_EXPR_NOT:
		if (serialize_expression(serializer, expression->left, depth + 1U))
			return -1;
		break;
	case ORLIX_TCTI_TARGET_EXPR_AND:
	case ORLIX_TCTI_TARGET_EXPR_OR:
	case ORLIX_TCTI_TARGET_EXPR_EQ:
	case ORLIX_TCTI_TARGET_EXPR_NE:
	case ORLIX_TCTI_TARGET_EXPR_IN:
		if (serialize_expression(serializer, expression->left, depth + 1U) ||
		    serialize_expression(serializer, expression->right, depth + 1U))
			return -1;
		break;
	}
	return append_record(serializer, tag, payload_start);
}

int orlix_tcti_target_condition_serialize(
	const struct orlix_tcti_target_inventory *inventory, uint32_t condition,
	struct orlix_tcti_target_condition_bytes *bytes,
	enum orlix_tcti_target_condition_serialize_error *error)
{
	static const uint8_t header[] = { 'T', 'C', 'N', 'D', 1 };
	struct serializer serializer = { .inventory = inventory };

	if (bytes) {
		bytes->data = NULL;
		bytes->length = 0;
	}
	if (error)
		*error = ORLIX_TCTI_TARGET_CONDITION_SERIALIZE_OK;
	if (!inventory || !bytes) {
		if (error)
			*error = ORLIX_TCTI_TARGET_CONDITION_SERIALIZE_INVALID_ARGUMENT;
		return -1;
	}
	if (append_bytes(&serializer, header, sizeof(header)) ||
	    serialize_expression(&serializer, condition, 0U)) {
		free(serializer.data);
		if (error)
			*error = serializer.error;
		return -1;
	}
	bytes->data = serializer.data;
	bytes->length = serializer.length;
	return 0;
}

void orlix_tcti_target_condition_bytes_destroy(struct orlix_tcti_target_condition_bytes *bytes)
{
	if (!bytes)
		return;
	free(bytes->data);
	bytes->data = NULL;
	bytes->length = 0;
}
