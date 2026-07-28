/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Generate compact, deterministic C data from the pinned AARCHMRS 2026-06
 * Instructions.json input.  JSON is deliberately confined to this explicit
 * refresh/audit tool.  Kernel and ordinary host consumers use its emitted C
 * rows and byte pools instead.
 */
#include "target_instruction_artifact_generator.h"

#include "target_condition_serialization.h"
#include "target_inventory_import.h"

#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define ORLIX_TCTI_ARTIFACT_MAX_BYTES (64U * 1024U * 1024U)
#define ORLIX_TCTI_ARTIFACT_MAX_SOURCE_BYTES (128U * 1024U * 1024U)

enum artifact_alias_relation_kind {
	ARTIFACT_ALIAS_RELATION_SEMANTIC = 3,
	ARTIFACT_ALIAS_RELATION_ASSEMBLER_ONLY = 4,
};

enum artifact_alias_predicate_kind {
	ARTIFACT_ALIAS_PREDICATE_SOURCE_CONDITION = 1,
	ARTIFACT_ALIAS_PREDICATE_SCHEMA_UNCONDITIONAL = 2,
};

static const char source_architecture[] = "vFATAp1-A";
static const char source_build[] = "818";
static const char source_reference[] = "2026-06_rel";
static const char source_schema[] = "2.9.5";
static const char source_sha256[] =
	"a1ad2c6538a47cd97d8762791ac5af88bce1d5f6aff096c9b77aef853e76acfe";

struct artifact_bytes {
	unsigned char *data;
	size_t length;
	size_t capacity;
};

struct artifact_condition {
	uint32_t offset;
	uint32_t length;
};

struct artifact_leaf {
	uint32_t name_offset;
	uint32_t mnemonic_offset;
	uint32_t operation_offset;
	uint32_t encoding_mask;
	uint32_t encoding_pattern;
	uint32_t condition_offset;
	uint32_t condition_length;
	uint32_t operand_first;
	uint32_t operand_count;
	uint32_t fixed_operand_first;
	uint32_t fixed_operand_count;
};

struct artifact_operand {
	uint32_t name_offset;
	uint32_t leaf_index;
	uint32_t condition_offset;
	uint32_t condition_length;
	uint32_t variable_mask;
	uint8_t start;
	uint8_t width;
};

struct artifact_fixed_operand {
	uint32_t name_offset;
	uint32_t leaf_index;
	uint32_t condition_offset;
	uint32_t condition_length;
	uint32_t fixed_mask;
	uint32_t fixed_value;
	uint32_t source_offset;
	uint32_t source_length;
	uint32_t source_identity_offset;
	uint8_t start;
	uint8_t width;
};

struct artifact_instruction_alias {
	uint32_t ordinal;
	uint32_t name_offset;
	uint32_t declared_operation_offset;
	uint32_t resolved_operation_offset;
	uint32_t condition_offset;
	uint32_t condition_length;
	uint32_t source_offset;
	uint32_t source_length;
	uint32_t source_identity_offset;
	uint32_t condition_source_offset;
	uint32_t condition_source_length;
	uint32_t condition_identity_offset;
	uint32_t preferred_source_offset;
	uint32_t preferred_source_length;
	uint32_t preferred_identity_offset;
	uint32_t predicate_sha256_offset;
	uint8_t relation_kind;
	uint8_t predicate_kind;
	uint8_t preferred_present;
};

struct artifact_operation_alias {
	uint32_t declared_operation_offset;
	uint32_t target_operation_offset;
	uint32_t resolved_operation_offset;
	uint32_t source_offset;
	uint32_t source_length;
	uint32_t source_identity_offset;
	uint32_t predicate_offset;
	uint32_t predicate_length;
	uint32_t predicate_sha256_offset;
	uint8_t relation_kind;
	uint8_t predicate_kind;
};

struct artifact_operational_note {
	uint32_t leaf_index;
	uint32_t source_offset;
	uint32_t source_length;
	uint32_t source_identity_offset;
	uint32_t source_sha256_offset;
	uint8_t kind;
};

struct artifact_model {
	const char *source_sha256;
	struct artifact_bytes strings;
	struct artifact_bytes conditions;
	struct artifact_condition *condition_map;
	struct artifact_leaf *leaves;
	struct artifact_operand *operands;
	size_t operand_count;
	struct artifact_fixed_operand *fixed_operands;
	size_t fixed_operand_count;
	struct artifact_instruction_alias *instruction_aliases;
	size_t instruction_alias_count;
	struct artifact_operation_alias *operation_aliases;
	size_t operation_alias_count;
	struct artifact_operational_note *operational_notes;
	size_t operational_note_count;
};

struct sha256_state {
	uint32_t words[8];
	uint64_t byte_count;
	unsigned char block[64];
	size_t used;
};

static uint32_t rotate_right(uint32_t value, unsigned int shift)
{
	return (value >> shift) | (value << (32U - shift));
}

static void sha256_transform(struct sha256_state *state,
			     const unsigned char block[64])
{
	static const uint32_t constants[64] = {
		0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U,
		0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
		0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
		0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
		0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
		0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
		0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
		0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
		0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
		0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
		0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U,
		0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
		0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U,
		0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
		0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
		0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U,
	};
	uint32_t schedule[64];
	uint32_t a, b, c, d, e, f, g, h;
	size_t index;

	for (index = 0; index < 16; index++)
		schedule[index] = ((uint32_t)block[index * 4] << 24) |
			((uint32_t)block[index * 4 + 1] << 16) |
			((uint32_t)block[index * 4 + 2] << 8) |
			(uint32_t)block[index * 4 + 3];
	for (index = 16; index < 64; index++) {
		uint32_t s0 = rotate_right(schedule[index - 15], 7) ^
			rotate_right(schedule[index - 15], 18) ^
			(schedule[index - 15] >> 3);
		uint32_t s1 = rotate_right(schedule[index - 2], 17) ^
			rotate_right(schedule[index - 2], 19) ^
			(schedule[index - 2] >> 10);

		schedule[index] = schedule[index - 16] + s0 +
			schedule[index - 7] + s1;
	}
	a = state->words[0]; b = state->words[1]; c = state->words[2];
	d = state->words[3]; e = state->words[4]; f = state->words[5];
	g = state->words[6]; h = state->words[7];
	for (index = 0; index < 64; index++) {
		uint32_t sum1 = rotate_right(e, 6) ^ rotate_right(e, 11) ^
			rotate_right(e, 25);
		uint32_t choose = (e & f) ^ (~e & g);
		uint32_t temporary1 = h + sum1 + choose + constants[index] +
			schedule[index];
		uint32_t sum0 = rotate_right(a, 2) ^ rotate_right(a, 13) ^
			rotate_right(a, 22);
		uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
		uint32_t temporary2 = sum0 + majority;

		h = g; g = f; f = e; e = d + temporary1;
		d = c; c = b; b = a; a = temporary1 + temporary2;
	}
	state->words[0] += a; state->words[1] += b; state->words[2] += c;
	state->words[3] += d; state->words[4] += e; state->words[5] += f;
	state->words[6] += g; state->words[7] += h;
}

static void sha256_update(struct sha256_state *state,
			  const unsigned char *data, size_t length)
{
	state->byte_count += length;
	while (length) {
		size_t available = sizeof(state->block) - state->used;
		size_t copied = length < available ? length : available;

		memcpy(state->block + state->used, data, copied);
		state->used += copied;
		data += copied;
		length -= copied;
		if (state->used == sizeof(state->block)) {
			sha256_transform(state, state->block);
			state->used = 0;
		}
	}
}

static void sha256_hex(const char *data, size_t length, char digest[65])
{
	static const char hex[] = "0123456789abcdef";
	struct sha256_state state = { .words = {
		0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
		0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U,
	} };
	uint64_t bits;
	unsigned char bytes[32];
	size_t index;

	sha256_update(&state, (const unsigned char *)data, length);
	bits = state.byte_count * 8U;
	state.block[state.used++] = 0x80;
	if (state.used > 56U) {
		memset(state.block + state.used, 0, sizeof(state.block) - state.used);
		sha256_transform(&state, state.block);
		state.used = 0;
	}
	memset(state.block + state.used, 0, 56U - state.used);
	for (index = 0; index < 8; index++)
		state.block[63U - index] = (unsigned char)(bits >> (index * 8U));
	sha256_transform(&state, state.block);
	for (index = 0; index < 8; index++) {
		bytes[index * 4] = (unsigned char)(state.words[index] >> 24);
		bytes[index * 4 + 1] = (unsigned char)(state.words[index] >> 16);
		bytes[index * 4 + 2] = (unsigned char)(state.words[index] >> 8);
		bytes[index * 4 + 3] = (unsigned char)state.words[index];
	}
	for (index = 0; index < sizeof(bytes); index++) {
		digest[index * 2] = hex[bytes[index] >> 4];
		digest[index * 2 + 1] = hex[bytes[index] & 15U];
	}
	digest[64] = '\0';
}

static int bytes_reserve(struct artifact_bytes *bytes, size_t add)
{
	size_t required;
	size_t capacity;
	unsigned char *data;

	if (add > ORLIX_TCTI_ARTIFACT_MAX_BYTES - bytes->length)
		return -1;
	required = bytes->length + add;
	if (required <= bytes->capacity)
		return 0;
	capacity = bytes->capacity ? bytes->capacity : 256U;
	while (capacity < required) {
		if (capacity > ORLIX_TCTI_ARTIFACT_MAX_BYTES / 2U) {
			capacity = ORLIX_TCTI_ARTIFACT_MAX_BYTES;
			break;
		}
		capacity *= 2U;
	}
	data = realloc(bytes->data, capacity);
	if (!data)
		return -1;
	bytes->data = data;
	bytes->capacity = capacity;
	return 0;
}

static int bytes_append(struct artifact_bytes *bytes, const void *data,
				size_t length)
{
	if (bytes_reserve(bytes, length))
		return -1;
	memcpy(bytes->data + bytes->length, data, length);
	bytes->length += length;
	return 0;
}

static int bytes_append_string(struct artifact_bytes *bytes, const char *text,
				       uint32_t *offset)
{
	size_t length;

	if (!text || bytes->length > UINT32_MAX)
		return -1;
	length = strlen(text);
	if (length == SIZE_MAX || bytes_append(bytes, text, length + 1U))
		return -1;
	*offset = (uint32_t)(bytes->length - length - 1U);
	return 0;
}

static void artifact_model_destroy(struct artifact_model *model)
{
	free(model->strings.data);
	free(model->conditions.data);
	free(model->condition_map);
	free(model->leaves);
	free(model->operands);
	free(model->fixed_operands);
	free(model->instruction_aliases);
	free(model->operation_aliases);
	free(model->operational_notes);
	memset(model, 0, sizeof(*model));
}

static enum orlix_tcti_target_instruction_artifact_generator_error import_category(
	const struct orlix_tcti_target_import_error *error)
{
	if (error->code == ORLIX_TCTI_TARGET_IMPORT_COUNT_MISMATCH)
		return ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_COUNT;
	if (error->code == ORLIX_TCTI_TARGET_IMPORT_HASH_MISMATCH)
		return ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_DIGEST;
	if (error->code == ORLIX_TCTI_TARGET_IMPORT_INVALID_SOURCE &&
	    !strcmp(error->message,
	    "Instructions.json metadata does not match the pinned Arm AARCHMRS 2026-06 source"))
		return ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_METADATA;
	if (error->code == ORLIX_TCTI_TARGET_IMPORT_NO_MEMORY)
		return ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_NO_MEMORY;
	return ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_PARSE;
}

/* Imported names are C strings. Reject the JSON spelling that would decode
 * to an embedded NUL before strlen can silently truncate an identifier. */
static int source_has_embedded_nul(const char *source, size_t length)
{
	size_t index;

	if (memchr(source, '\0', length))
		return 1;
	for (index = 0; index + 6U <= length; index++)
		if (!memcmp(source + index, "\\u0000", 6U))
			return 1;
	return 0;
}

static enum orlix_tcti_target_instruction_artifact_generator_error build_conditions(
	const struct orlix_tcti_target_inventory *inventory, struct artifact_model *model)
{
	size_t index;

	if (inventory->expression_count > SIZE_MAX / sizeof(*model->condition_map))
		return ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_OVERFLOW;
	model->condition_map = calloc(inventory->expression_count,
				      sizeof(*model->condition_map));
	if (inventory->expression_count && !model->condition_map)
		return ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_NO_MEMORY;
	for (index = 0; index < inventory->expression_count; index++) {
		struct orlix_tcti_target_condition_bytes bytes = { 0 };
		enum orlix_tcti_target_condition_serialize_error error;

		if (orlix_tcti_target_condition_serialize(inventory, (uint32_t)index,
					     &bytes, &error))
			return ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_CONDITION;
		if (model->conditions.length > UINT32_MAX ||
		    bytes.length > UINT32_MAX ||
		    bytes_append(&model->conditions, bytes.data, bytes.length)) {
			orlix_tcti_target_condition_bytes_destroy(&bytes);
			return ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_OVERFLOW;
		}
		model->condition_map[index].offset = (uint32_t)(model->conditions.length - bytes.length);
		model->condition_map[index].length = (uint32_t)bytes.length;
		orlix_tcti_target_condition_bytes_destroy(&bytes);
	}
	return ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_OK;
}

static int append_span_identity(struct artifact_bytes *strings,
				 const char *expected_source_sha256,
				 size_t offset, size_t length, uint32_t *identity_offset)
{
	char identity[64U + 2U + 20U + 20U + 1U];
	int count;

	if (!length || offset > UINT32_MAX || length > UINT32_MAX)
		return -1;
	count = snprintf(identity, sizeof(identity), "%s:%zu:%zu",
			 expected_source_sha256, offset, length);
	if (count < 0 || (size_t)count >= sizeof(identity))
		return -1;
	return bytes_append_string(strings, identity, identity_offset);
}

static enum orlix_tcti_target_instruction_artifact_generator_error
build_operational_notes(const struct orlix_tcti_target_inventory *inventory,
			struct artifact_model *model)
{
	size_t leaf_index;
	size_t note_index = 0;

	if (inventory->operational_note_obligation_count > UINT32_MAX ||
	    inventory->operational_note_obligation_count >
		SIZE_MAX / sizeof(*model->operational_notes))
		return ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_OVERFLOW;
	if (inventory->operational_note_obligation_count) {
		model->operational_notes = calloc(
			inventory->operational_note_obligation_count,
			sizeof(*model->operational_notes));
		if (!model->operational_notes)
			return ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_NO_MEMORY;
	}
	for (leaf_index = 0; leaf_index < inventory->leaf_count; leaf_index++) {
		const struct orlix_tcti_target_leaf *leaf = &inventory->leaves[leaf_index];
		struct artifact_operational_note *note;

		if (leaf->operational_note_state ==
		    ORLIX_TCTI_TARGET_OPERATIONAL_NOTE_ABSENT)
			continue;
		if (leaf->operational_note_state !=
		    ORLIX_TCTI_TARGET_OPERATIONAL_NOTE_BEHAVIOR_OBLIGATION ||
		    note_index >= inventory->operational_note_obligation_count ||
		    leaf_index > UINT32_MAX ||
		    leaf->operational_note_source_offset > UINT32_MAX ||
		    !leaf->operational_note_source_length ||
		    leaf->operational_note_source_length > UINT32_MAX)
			return ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_PARSE;
		note = &model->operational_notes[note_index++];
		note->leaf_index = (uint32_t)leaf_index;
		note->source_offset = (uint32_t)leaf->operational_note_source_offset;
		note->source_length = (uint32_t)leaf->operational_note_source_length;
		note->kind = 1U;
		if (append_span_identity(&model->strings, model->source_sha256,
			leaf->operational_note_source_offset,
			leaf->operational_note_source_length,
			&note->source_identity_offset) ||
		    bytes_append_string(&model->strings,
			leaf->operational_note_sha256,
			&note->source_sha256_offset))
			return ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_OVERFLOW;
	}
	if (note_index != inventory->operational_note_obligation_count)
		return ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_COUNT;
	model->operational_note_count = note_index;
	return ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_OK;
}

static int append_predicate_sha256(struct artifact_model *model,
				   uint32_t offset, uint32_t length,
				   uint32_t *digest_offset)
{
	char digest[65];

	if (!length || offset > model->conditions.length ||
	    length > model->conditions.length - offset)
		return -1;
	orlix_tcti_target_inventory_sha256(model->conditions.data + offset,
		length, digest);
	return bytes_append_string(&model->strings, digest, digest_offset);
}

static enum orlix_tcti_target_instruction_artifact_generator_error build_aliases(
	const struct orlix_tcti_target_inventory *inventory, struct artifact_model *model)
{
	static const unsigned char unconditional_predicate[] = {
		'T', 'C', 'N', 'D', 1U,
		ORLIX_TCTI_TARGET_CONDITION_BOOL, 0U, 0U, 0U, 1U, 1U,
	};
	size_t index;
	size_t operation_alias_index = 0;
	uint32_t unconditional_offset;

	if (inventory->instruction_alias_count !=
		ORLIX_TCTI_A64_TARGET_INSTRUCTION_ALIAS_COUNT ||
	    inventory->reachable_operation_alias_count !=
		ORLIX_TCTI_A64_TARGET_REACHABLE_OPERATION_ALIAS_COUNT)
		return ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_COUNT;
	model->instruction_aliases = calloc(inventory->instruction_alias_count,
					  sizeof(*model->instruction_aliases));
	model->operation_aliases = calloc(inventory->reachable_operation_alias_count,
					 sizeof(*model->operation_aliases));
	if (!model->instruction_aliases || !model->operation_aliases)
		return ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_NO_MEMORY;
	if (model->conditions.length > UINT32_MAX ||
	    bytes_append(&model->conditions, unconditional_predicate,
		 sizeof(unconditional_predicate)))
		return ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_OVERFLOW;
	unconditional_offset = (uint32_t)(model->conditions.length -
		sizeof(unconditional_predicate));
	for (index = 0; index < inventory->instruction_alias_count; index++) {
		const struct orlix_tcti_target_instruction_alias *source =
			&inventory->instruction_aliases[index];
		struct artifact_instruction_alias *alias =
			&model->instruction_aliases[index];

		if (source->ordinal != index ||
		    source->condition >= inventory->expression_count ||
		    append_span_identity(&model->strings, model->source_sha256, source->source_offset,
				source->source_length, &alias->source_identity_offset) ||
		    append_span_identity(&model->strings, model->source_sha256, source->condition_source_offset,
				source->condition_source_length,
				&alias->condition_identity_offset) ||
		    append_span_identity(&model->strings, model->source_sha256, source->preferred_source_offset,
				source->preferred_source_length,
				&alias->preferred_identity_offset) ||
		    bytes_append_string(&model->strings, source->name, &alias->name_offset) ||
		    bytes_append_string(&model->strings, source->operation_id,
				&alias->declared_operation_offset) ||
		    bytes_append_string(&model->strings, source->canonical_operation_id,
				&alias->resolved_operation_offset) ||
		    append_predicate_sha256(model,
			model->condition_map[source->condition].offset,
			model->condition_map[source->condition].length,
			&alias->predicate_sha256_offset))
			return ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_OVERFLOW;
		alias->ordinal = source->ordinal;
		alias->condition_offset = model->condition_map[source->condition].offset;
		alias->condition_length = model->condition_map[source->condition].length;
		alias->source_offset = (uint32_t)source->source_offset;
		alias->source_length = (uint32_t)source->source_length;
		alias->condition_source_offset = (uint32_t)source->condition_source_offset;
		alias->condition_source_length = (uint32_t)source->condition_source_length;
		alias->preferred_source_offset = (uint32_t)source->preferred_source_offset;
		alias->preferred_source_length = (uint32_t)source->preferred_source_length;
		alias->preferred_present = source->preferred_present;
		alias->relation_kind = ARTIFACT_ALIAS_RELATION_ASSEMBLER_ONLY;
		alias->predicate_kind = ARTIFACT_ALIAS_PREDICATE_SOURCE_CONDITION;
	}
	for (index = 0; index < inventory->operation_count; index++) {
		const struct orlix_tcti_target_operation *source = &inventory->operations[index];
		struct artifact_operation_alias *alias;

		if (!source->is_alias || !source->canonical_operation_id)
			continue;
		if (!source->alias_predicate_unconditional)
			return ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_PARSE;
		if (operation_alias_index >= inventory->reachable_operation_alias_count)
			return ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_COUNT;
		alias = &model->operation_aliases[operation_alias_index];
		if (append_span_identity(&model->strings, model->source_sha256, source->source_offset,
				source->source_length, &alias->source_identity_offset) ||
		    bytes_append_string(&model->strings, source->id,
				&alias->declared_operation_offset) ||
		    bytes_append_string(&model->strings, source->alias_operation_id,
				&alias->target_operation_offset) ||
		    bytes_append_string(&model->strings, source->canonical_operation_id,
				&alias->resolved_operation_offset) ||
		    append_predicate_sha256(model, unconditional_offset,
			sizeof(unconditional_predicate),
			&alias->predicate_sha256_offset))
			return ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_OVERFLOW;
		alias->source_offset = (uint32_t)source->source_offset;
		alias->source_length = (uint32_t)source->source_length;
		alias->predicate_offset = unconditional_offset;
		alias->predicate_length = sizeof(unconditional_predicate);
		alias->relation_kind = ARTIFACT_ALIAS_RELATION_SEMANTIC;
		alias->predicate_kind = ARTIFACT_ALIAS_PREDICATE_SCHEMA_UNCONDITIONAL;
		operation_alias_index++;
	}
	if (operation_alias_index != inventory->reachable_operation_alias_count)
		return ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_COUNT;
	model->instruction_alias_count = inventory->instruction_alias_count;
	model->operation_alias_count = operation_alias_index;
	return ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_OK;
}

static enum orlix_tcti_target_instruction_artifact_generator_error build_model(
	const struct orlix_tcti_target_inventory *inventory,
	const char *expected_source_sha256, struct artifact_model *model)
{
	uint32_t *counts = NULL;
	uint32_t *cursors = NULL;
	uint32_t *fixed_counts = NULL;
	uint32_t *fixed_cursors = NULL;
	size_t index;
	size_t total_operands = inventory->operand_count;
	size_t total_fixed_operands = inventory->fixed_operand_count;
	enum orlix_tcti_target_instruction_artifact_generator_error result;

	model->source_sha256 = expected_source_sha256;

	if (inventory->leaf_count != ORLIX_TCTI_A64_TARGET_LEAF_COUNT ||
	    inventory->leaf_count > UINT32_MAX || total_operands > UINT32_MAX ||
	    total_fixed_operands > UINT32_MAX ||
	    inventory->leaf_count > SIZE_MAX / sizeof(*model->leaves) ||
	    total_operands > SIZE_MAX / sizeof(*model->operands) ||
	    total_fixed_operands > SIZE_MAX / sizeof(*model->fixed_operands))
		return ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_OVERFLOW;
	model->leaves = calloc(inventory->leaf_count, sizeof(*model->leaves));
	model->operands = calloc(total_operands, sizeof(*model->operands));
	model->fixed_operands = calloc(total_fixed_operands,
					 sizeof(*model->fixed_operands));
	counts = calloc(inventory->leaf_count, sizeof(*counts));
	cursors = calloc(inventory->leaf_count, sizeof(*cursors));
	fixed_counts = calloc(inventory->leaf_count, sizeof(*fixed_counts));
	fixed_cursors = calloc(inventory->leaf_count, sizeof(*fixed_cursors));
	if (!model->leaves || (total_operands && !model->operands) ||
	    (total_fixed_operands && !model->fixed_operands) || !counts || !cursors ||
	    !fixed_counts || !fixed_cursors) {
		result = ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_NO_MEMORY;
		goto out;
	}
	for (index = 0; index < total_operands; index++) {
		const struct orlix_tcti_target_operand *operand = &inventory->operands[index];

		if (operand->leaf_index >= inventory->leaf_count ||
		    operand->condition >= inventory->expression_count ||
		    counts[operand->leaf_index] == UINT32_MAX) {
			result = ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_OVERFLOW;
			goto out;
		}
		counts[operand->leaf_index]++;
	}
	for (index = 0; index < total_fixed_operands; index++) {
		const struct orlix_tcti_target_fixed_operand *operand =
			&inventory->fixed_operands[index];

		if (operand->leaf_index >= inventory->leaf_count ||
		    operand->condition >= inventory->expression_count ||
		    fixed_counts[operand->leaf_index] == UINT32_MAX) {
			result = ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_OVERFLOW;
			goto out;
		}
		fixed_counts[operand->leaf_index]++;
	}
	for (index = 0; index < inventory->leaf_count; index++) {
		struct artifact_leaf *leaf = &model->leaves[index];
		const struct orlix_tcti_target_leaf *source = &inventory->leaves[index];

		if (source->condition >= inventory->expression_count ||
		    (index && (model->leaves[index - 1].operand_first > UINT32_MAX -
			model->leaves[index - 1].operand_count ||
		     model->leaves[index - 1].fixed_operand_first > UINT32_MAX -
			model->leaves[index - 1].fixed_operand_count))) {
			result = ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_OVERFLOW;
			goto out;
		}
		leaf->operand_first = index ? model->leaves[index - 1].operand_first +
			model->leaves[index - 1].operand_count : 0;
		leaf->operand_count = counts[index];
		cursors[index] = leaf->operand_first;
		leaf->fixed_operand_first = index ?
			model->leaves[index - 1].fixed_operand_first +
			model->leaves[index - 1].fixed_operand_count : 0;
		leaf->fixed_operand_count = fixed_counts[index];
		fixed_cursors[index] = leaf->fixed_operand_first;
		leaf->encoding_mask = source->encoding_mask;
		leaf->encoding_pattern = source->encoding_pattern;
		if (bytes_append_string(&model->strings, source->name, &leaf->name_offset) ||
		    bytes_append_string(&model->strings, source->mnemonic, &leaf->mnemonic_offset) ||
		    bytes_append_string(&model->strings, source->operation_id, &leaf->operation_offset)) {
			result = ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_OVERFLOW;
			goto out;
		}
	}
	result = build_conditions(inventory, model);
	if (result != ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_OK)
		goto out;
	result = build_aliases(inventory, model);
	if (result != ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_OK)
		goto out;
	result = build_operational_notes(inventory, model);
	if (result != ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_OK)
		goto out;
	for (index = 0; index < inventory->leaf_count; index++) {
		const struct orlix_tcti_target_leaf *source = &inventory->leaves[index];

		model->leaves[index].condition_offset =
			model->condition_map[source->condition].offset;
		model->leaves[index].condition_length =
			model->condition_map[source->condition].length;
	}
	for (index = 0; index < total_operands; index++) {
		const struct orlix_tcti_target_operand *source = &inventory->operands[index];
		struct artifact_operand *operand;
		uint32_t slot = cursors[source->leaf_index]++;

		if (slot >= total_operands) {
			result = ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_OVERFLOW;
			goto out;
		}
		operand = &model->operands[slot];
		operand->leaf_index = source->leaf_index;
		operand->variable_mask = source->variable_mask;
		operand->start = source->start;
		operand->width = source->width;
		operand->condition_offset = model->condition_map[source->condition].offset;
		operand->condition_length = model->condition_map[source->condition].length;
		if (bytes_append_string(&model->strings, source->name, &operand->name_offset)) {
			result = ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_OVERFLOW;
			goto out;
		}
	}
	for (index = 0; index < total_fixed_operands; index++) {
		const struct orlix_tcti_target_fixed_operand *source =
			&inventory->fixed_operands[index];
		struct artifact_fixed_operand *operand;
		uint32_t slot = fixed_cursors[source->leaf_index]++;

		if (slot >= total_fixed_operands ||
		    source->source_offset > UINT32_MAX ||
		    source->source_length > UINT32_MAX ||
		    append_span_identity(&model->strings, model->source_sha256, source->source_offset,
			 source->source_length, &model->fixed_operands[slot].source_identity_offset)) {
			result = ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_OVERFLOW;
			goto out;
		}
		operand = &model->fixed_operands[slot];
		operand->leaf_index = source->leaf_index;
		operand->fixed_mask = source->fixed_mask;
		operand->fixed_value = source->fixed_value;
		operand->start = source->start;
		operand->width = source->width;
		operand->source_offset = (uint32_t)source->source_offset;
		operand->source_length = (uint32_t)source->source_length;
		operand->condition_offset = model->condition_map[source->condition].offset;
		operand->condition_length = model->condition_map[source->condition].length;
		if (bytes_append_string(&model->strings, source->name,
			&operand->name_offset)) {
			result = ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_OVERFLOW;
			goto out;
		}
	}
	model->operand_count = total_operands;
	model->fixed_operand_count = total_fixed_operands;
	result = ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_OK;
out:
	free(counts);
	free(cursors);
	free(fixed_counts);
	free(fixed_cursors);
	return result;
}

static int outputf(struct artifact_bytes *output, const char *format, ...)
{
	char local[256];
	char *heap = NULL;
	va_list args;
	int count;

	va_start(args, format);
	count = vsnprintf(local, sizeof(local), format, args);
	va_end(args);
	if (count < 0)
		return -1;
	if ((size_t)count < sizeof(local))
		return bytes_append(output, local, (size_t)count);
	heap = malloc((size_t)count + 1U);
	if (!heap)
		return -1;
	va_start(args, format);
	(void)vsnprintf(heap, (size_t)count + 1U, format, args);
	va_end(args);
	if (bytes_append(output, heap, (size_t)count)) {
		free(heap);
		return -1;
	}
	free(heap);
	return 0;
}

static int emit_u8_array(struct artifact_bytes *output, const char *name,
			 const struct artifact_bytes *bytes)
{
	size_t index;

	if (outputf(output, "static const u8 %s[%zu] = {\n", name,
			bytes->length))
		return -1;
	for (index = 0; index < bytes->length; index++)
		if (outputf(output, "%s0x%02xU%s", index % 12U ? " " : "    ",
			bytes->data[index], index + 1U == bytes->length ? "\n" : ","))
			return -1;
	return outputf(output, "};\n\n");
}

static int emit_artifact(struct artifact_bytes *output,
			 const struct artifact_model *model)
{
	size_t index;

	if (outputf(output,
		"/* SPDX-License-Identifier: BSD-3-Clause */\n"
		"/* Generated from the pinned Arm AARCHMRS 2026-06 instruction source. Do not edit. */\n"
		"#ifndef ORLIX_TCTI_A64_INSTRUCTION_ARTIFACT_GENERATED_H\n"
		"#define ORLIX_TCTI_A64_INSTRUCTION_ARTIFACT_GENERATED_H\n"
		"#include \"target_instruction_artifact.h\"\n\n"
		"#define ORLIX_TCTI_A64_INSTRUCTION_ARTIFACT_VERSION 5U\n"
		"#define ORLIX_TCTI_A64_INSTRUCTION_ARTIFACT_ARCHITECTURE \"%s\"\n"
		"#define ORLIX_TCTI_A64_INSTRUCTION_ARTIFACT_BUILD \"%s\"\n"
		"#define ORLIX_TCTI_A64_INSTRUCTION_ARTIFACT_REFERENCE \"%s\"\n"
		"#define ORLIX_TCTI_A64_INSTRUCTION_ARTIFACT_SCHEMA \"%s\"\n"
		"#define ORLIX_TCTI_A64_INSTRUCTION_ARTIFACT_SOURCE_SHA256 \"%s\"\n"
		"#define ORLIX_TCTI_A64_INSTRUCTION_ARTIFACT_LEAF_COUNT %uU\n"
		"#define ORLIX_TCTI_A64_INSTRUCTION_ARTIFACT_OPERAND_COUNT %zuU\n"
		"#define ORLIX_TCTI_A64_INSTRUCTION_ARTIFACT_FIXED_OPERAND_COUNT %zuU\n\n",
		source_architecture, source_build, source_reference, source_schema,
		model->source_sha256, ORLIX_TCTI_A64_TARGET_LEAF_COUNT,
		model->operand_count,
		model->fixed_operand_count))
		return -1;
	if (outputf(output,
		"static const struct orlix_tcti_target_instruction_artifact_leaf "
		"orlix_tcti_a64_instruction_artifact_leaves[%u] = {\n",
		ORLIX_TCTI_A64_TARGET_LEAF_COUNT))
		return -1;
	for (index = 0; index < ORLIX_TCTI_A64_TARGET_LEAF_COUNT; index++) {
		const struct artifact_leaf *leaf = &model->leaves[index];
		if (outputf(output,
			"    { %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, 0x%08" PRIx32 "U, 0x%08" PRIx32 "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U },\n",
			leaf->name_offset, leaf->mnemonic_offset, leaf->operation_offset,
			leaf->encoding_mask, leaf->encoding_pattern, leaf->condition_offset,
			leaf->condition_length, leaf->operand_first, leaf->operand_count,
			leaf->fixed_operand_first, leaf->fixed_operand_count))
			return -1;
	}
	if (outputf(output, "};\n\nstatic const struct orlix_tcti_target_instruction_artifact_operand "
		"orlix_tcti_a64_instruction_artifact_operands[%zu] = {\n", model->operand_count))
		return -1;
	for (index = 0; index < model->operand_count; index++) {
		const struct artifact_operand *operand = &model->operands[index];
		if (outputf(output,
			"    { %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, 0x%08" PRIx32 "U, %uU, %uU },\n",
			operand->name_offset, operand->leaf_index, operand->condition_offset,
			operand->condition_length, operand->variable_mask,
			(unsigned int)operand->start, (unsigned int)operand->width))
			return -1;
	}
	if (outputf(output, "};\n\n"))
		return -1;
	if (outputf(output,
		"static const struct orlix_tcti_target_instruction_artifact_fixed_operand "
		"orlix_tcti_a64_instruction_artifact_fixed_operands[%zu] = {\n",
		model->fixed_operand_count))
		return -1;
	for (index = 0; index < model->fixed_operand_count; index++) {
		const struct artifact_fixed_operand *operand =
			&model->fixed_operands[index];

		if (outputf(output,
			"    { %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, 0x%08" PRIx32 "U, 0x%08" PRIx32 "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %uU, %uU },\n",
			operand->name_offset, operand->leaf_index,
			operand->condition_offset, operand->condition_length,
			operand->fixed_mask, operand->fixed_value,
			operand->source_offset, operand->source_length,
			operand->source_identity_offset, (unsigned int)operand->start,
			(unsigned int)operand->width))
			return -1;
	}
	if (outputf(output, "};\n\n"))
		return -1;
	if (outputf(output,
		"#define ORLIX_TCTI_A64_INSTRUCTION_ARTIFACT_INSTRUCTION_ALIAS_COUNT %zuU\n"
		"#define ORLIX_TCTI_A64_INSTRUCTION_ARTIFACT_OPERATION_ALIAS_COUNT %zuU\n"
		"#define ORLIX_TCTI_A64_INSTRUCTION_ARTIFACT_OPERATIONAL_NOTE_COUNT %zuU\n\n"
		"static const struct orlix_tcti_target_instruction_artifact_instruction_alias "
		"orlix_tcti_a64_instruction_artifact_instruction_aliases[%zu] = {\n",
		model->instruction_alias_count, model->operation_alias_count,
		model->operational_note_count,
		model->instruction_alias_count))
		return -1;
	for (index = 0; index < model->instruction_alias_count; index++) {
		const struct artifact_instruction_alias *alias =
			&model->instruction_aliases[index];

		if (outputf(output,
			"    { %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %uU, %uU, %uU },\n",
			alias->ordinal, alias->name_offset,
			alias->declared_operation_offset, alias->resolved_operation_offset,
			alias->condition_offset, alias->condition_length,
			alias->source_offset, alias->source_length,
			alias->source_identity_offset, alias->condition_source_offset,
			alias->condition_source_length, alias->condition_identity_offset,
			alias->preferred_source_offset, alias->preferred_source_length,
			alias->preferred_identity_offset,
			alias->predicate_sha256_offset,
			(unsigned int)alias->relation_kind,
			(unsigned int)alias->predicate_kind,
			(unsigned int)alias->preferred_present))
			return -1;
	}
	if (outputf(output,
		"};\n\nstatic const struct orlix_tcti_target_instruction_artifact_operation_alias "
		"orlix_tcti_a64_instruction_artifact_operation_aliases[%zu] = {\n",
		model->operation_alias_count))
		return -1;
	for (index = 0; index < model->operation_alias_count; index++) {
		const struct artifact_operation_alias *alias =
			&model->operation_aliases[index];

		if (outputf(output,
			"    { %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %uU, %uU },\n",
			alias->declared_operation_offset,
			alias->target_operation_offset, alias->resolved_operation_offset,
			alias->source_offset, alias->source_length,
			alias->source_identity_offset, alias->predicate_offset,
			alias->predicate_length, alias->predicate_sha256_offset,
			(unsigned int)alias->relation_kind,
			(unsigned int)alias->predicate_kind))
			return -1;
	}
	if (outputf(output, "};\n\n"))
		return -1;
	if (model->operational_note_count) {
		if (outputf(output,
			"static const struct orlix_tcti_target_instruction_artifact_operational_note "
			"orlix_tcti_a64_instruction_artifact_operational_notes[%zu] = {\n",
			model->operational_note_count))
			return -1;
		for (index = 0; index < model->operational_note_count; index++) {
			const struct artifact_operational_note *note =
				&model->operational_notes[index];

			if (outputf(output,
				" { %" PRIu32 "U, %" PRIu32 "U, %" PRIu32
				"U, %" PRIu32 "U, %" PRIu32 "U, %uU },\n",
				note->leaf_index, note->source_offset,
				note->source_length, note->source_identity_offset,
				note->source_sha256_offset, (unsigned int)note->kind))
				return -1;
		}
		if (outputf(output,
			"};\n#define ORLIX_TCTI_A64_INSTRUCTION_ARTIFACT_OPERATIONAL_NOTES "
			"orlix_tcti_a64_instruction_artifact_operational_notes\n\n"))
			return -1;
	} else if (outputf(output,
		"#define ORLIX_TCTI_A64_INSTRUCTION_ARTIFACT_OPERATIONAL_NOTES NULL\n\n")) {
		return -1;
	}
	if (emit_u8_array(output, "orlix_tcti_a64_instruction_artifact_string_pool",
			  &model->strings) ||
	    emit_u8_array(output, "orlix_tcti_a64_instruction_artifact_condition_pool",
			  &model->conditions))
		return -1;
	return outputf(output,
		"static const struct orlix_tcti_target_instruction_artifact "
		"orlix_tcti_a64_instruction_artifact = {\n"
		"    .version = ORLIX_TCTI_A64_INSTRUCTION_ARTIFACT_VERSION,\n"
		"    .architecture = ORLIX_TCTI_A64_INSTRUCTION_ARTIFACT_ARCHITECTURE,\n"
		"    .build = ORLIX_TCTI_A64_INSTRUCTION_ARTIFACT_BUILD,\n"
		"    .reference = ORLIX_TCTI_A64_INSTRUCTION_ARTIFACT_REFERENCE,\n"
		"    .schema = ORLIX_TCTI_A64_INSTRUCTION_ARTIFACT_SCHEMA,\n"
		"    .source_sha256 = ORLIX_TCTI_A64_INSTRUCTION_ARTIFACT_SOURCE_SHA256,\n"
		"    .leaves = orlix_tcti_a64_instruction_artifact_leaves,\n"
		"    .leaf_count = ORLIX_TCTI_A64_INSTRUCTION_ARTIFACT_LEAF_COUNT,\n"
		"    .operands = orlix_tcti_a64_instruction_artifact_operands,\n"
		"    .operand_count = ORLIX_TCTI_A64_INSTRUCTION_ARTIFACT_OPERAND_COUNT,\n"
		"    .fixed_operands = orlix_tcti_a64_instruction_artifact_fixed_operands,\n"
		"    .fixed_operand_count = ORLIX_TCTI_A64_INSTRUCTION_ARTIFACT_FIXED_OPERAND_COUNT,\n"
		"    .instruction_aliases = orlix_tcti_a64_instruction_artifact_instruction_aliases,\n"
		"    .instruction_alias_count = ORLIX_TCTI_A64_INSTRUCTION_ARTIFACT_INSTRUCTION_ALIAS_COUNT,\n"
		"    .operation_aliases = orlix_tcti_a64_instruction_artifact_operation_aliases,\n"
		"    .operation_alias_count = ORLIX_TCTI_A64_INSTRUCTION_ARTIFACT_OPERATION_ALIAS_COUNT,\n"
		"    .operational_notes = ORLIX_TCTI_A64_INSTRUCTION_ARTIFACT_OPERATIONAL_NOTES,\n"
		"    .operational_note_count = ORLIX_TCTI_A64_INSTRUCTION_ARTIFACT_OPERATIONAL_NOTE_COUNT,\n"
		"    .string_pool = orlix_tcti_a64_instruction_artifact_string_pool,\n"
		"    .string_pool_size = sizeof(orlix_tcti_a64_instruction_artifact_string_pool),\n"
		"    .condition_pool = orlix_tcti_a64_instruction_artifact_condition_pool,\n"
		"    .condition_pool_size = sizeof(orlix_tcti_a64_instruction_artifact_condition_pool),\n"
		"};\n\n"
		"#endif /* ORLIX_TCTI_A64_INSTRUCTION_ARTIFACT_GENERATED_H */\n");
}

const char *orlix_tcti_target_instruction_artifact_generator_error_name(
	enum orlix_tcti_target_instruction_artifact_generator_error error)
{
	switch (error) {
	case ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_OK: return "success";
	case ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_INVALID_ARGUMENT: return "invalid argument";
	case ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_PARSE: return "parse failure";
	case ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_METADATA: return "metadata failure";
	case ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_COUNT: return "count failure";
	case ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_DIGEST: return "digest failure";
	case ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_CONDITION: return "condition serialization failure";
	case ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_OVERFLOW: return "size overflow";
	case ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_NO_MEMORY: return "out of memory";
	case ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_IO: return "I/O failure";
	}
	return "unknown failure";
}

/*
 * Keep all fallible construction ahead of the first fwrite.  Focused host
 * tests include this translation unit and exercise this boundary with count
 * and size-overflow mutations that cannot be encoded in the pinned source.
 */
static enum orlix_tcti_target_instruction_artifact_generator_error generate_from_inventory(
	const struct orlix_tcti_target_inventory *inventory,
	const char *expected_source_sha256, struct artifact_bytes *generated)
{
	struct artifact_model model = { 0 };
	enum orlix_tcti_target_instruction_artifact_generator_error result;

	if (!inventory || !generated)
		return ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_INVALID_ARGUMENT;
	if (inventory->leaf_count != ORLIX_TCTI_A64_TARGET_LEAF_COUNT)
		return ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_COUNT;
	result = build_model(inventory, expected_source_sha256, &model);
	if (result == ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_OK &&
	    emit_artifact(generated, &model))
		result = ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_NO_MEMORY;
	artifact_model_destroy(&model);
	return result;
}


enum orlix_tcti_target_instruction_artifact_generator_error
orlix_tcti_target_instruction_artifact_emit_expected(
	const char *source, size_t length, const char *expected_source_sha256,
	FILE *output)
{
	struct orlix_tcti_target_inventory inventory = { 0 };
	struct orlix_tcti_target_import_error import_error = { 0 };
	struct artifact_bytes generated = { 0 };
	char digest[65];
	enum orlix_tcti_target_instruction_artifact_generator_error result;

	if (!source || !expected_source_sha256 ||
	    strlen(expected_source_sha256) != 64U || !output)
		return ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_INVALID_ARGUMENT;
	if (length > ORLIX_TCTI_ARTIFACT_MAX_SOURCE_BYTES)
		return ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_OVERFLOW;
	if (source_has_embedded_nul(source, length))
		return ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_PARSE;
	if (orlix_tcti_target_inventory_import_expected(
		    source, length, expected_source_sha256, &inventory,
		    &import_error)) {
		result = import_category(&import_error);
		goto out;
	}
	sha256_hex(source, length, digest);
	if (strcmp(digest, expected_source_sha256)) {
		result = ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_DIGEST;
		goto out;
	}
	result = generate_from_inventory(&inventory, expected_source_sha256,
					 &generated);
	if (result != ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_OK)
		goto out;
	if (generated.length && fwrite(generated.data, 1, generated.length, output) != generated.length)
		result = ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_IO;
	else
		result = ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_OK;
	out:
	free(generated.data);
	orlix_tcti_target_inventory_destroy(&inventory);
	return result;
}

enum orlix_tcti_target_instruction_artifact_generator_error
orlix_tcti_target_instruction_artifact_emit(const char *source, size_t length,
				      FILE *output)
{
	return orlix_tcti_target_instruction_artifact_emit_expected(
		source, length, source_sha256, output);
}

#ifndef TARGET_INSTRUCTION_ARTIFACT_GENERATOR_NO_MAIN
static int read_source(const char *path, char **source, size_t *length)
{
	FILE *file;
	long file_length;

	*source = NULL;
	*length = 0;
	file = fopen(path, "rb");
	if (!file || fseek(file, 0, SEEK_END) || (file_length = ftell(file)) < 0 ||
	    fseek(file, 0, SEEK_SET) || (uintmax_t)file_length > SIZE_MAX - 1U ||
	    (uintmax_t)file_length > ORLIX_TCTI_ARTIFACT_MAX_SOURCE_BYTES) {
		if (file)
			fclose(file);
		return -1;
	}
	*source = malloc((size_t)file_length + 1U);
	if (!*source || fread(*source, 1, (size_t)file_length, file) != (size_t)file_length ||
	    fclose(file)) {
		free(*source);
		*source = NULL;
		return -1;
	}
	(*source)[file_length] = '\0';
	*length = (size_t)file_length;
	return 0;
}

int main(int argc, char **argv)
{
	char *source;
	size_t length;
	enum orlix_tcti_target_instruction_artifact_generator_error error;

	if (argc != 2) {
		fprintf(stderr, "usage: %s Instructions.json > generated.h\n", argv[0]);
		return EXIT_FAILURE;
	}
	if (read_source(argv[1], &source, &length)) {
		fprintf(stderr, "target instruction artifact generator: cannot read source\n");
		return EXIT_FAILURE;
	}
	error = orlix_tcti_target_instruction_artifact_emit(source, length, stdout);
	if (error != ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_GENERATOR_OK) {
		fprintf(stderr, "target instruction artifact generator: %s\n",
			orlix_tcti_target_instruction_artifact_generator_error_name(error));
		free(source);
		return EXIT_FAILURE;
	}
	free(source);
	return EXIT_SUCCESS;
}
#endif
