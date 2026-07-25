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

#define TCTI_ARTIFACT_MAX_BYTES (64U * 1024U * 1024U)
#define TCTI_ARTIFACT_MAX_SOURCE_BYTES (128U * 1024U * 1024U)

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

struct artifact_model {
	struct artifact_bytes strings;
	struct artifact_bytes conditions;
	struct artifact_condition *condition_map;
	struct artifact_leaf *leaves;
	struct artifact_operand *operands;
	size_t operand_count;
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

	if (add > TCTI_ARTIFACT_MAX_BYTES - bytes->length)
		return -1;
	required = bytes->length + add;
	if (required <= bytes->capacity)
		return 0;
	capacity = bytes->capacity ? bytes->capacity : 256U;
	while (capacity < required) {
		if (capacity > TCTI_ARTIFACT_MAX_BYTES / 2U) {
			capacity = TCTI_ARTIFACT_MAX_BYTES;
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
	memset(model, 0, sizeof(*model));
}

static enum tcti_target_instruction_artifact_error import_category(
	const struct tcti_target_import_error *error)
{
	if (error->code == TCTI_TARGET_IMPORT_COUNT_MISMATCH)
		return TCTI_TARGET_INSTRUCTION_ARTIFACT_COUNT;
	if (error->code == TCTI_TARGET_IMPORT_HASH_MISMATCH)
		return TCTI_TARGET_INSTRUCTION_ARTIFACT_DIGEST;
	if (error->code == TCTI_TARGET_IMPORT_INVALID_SOURCE &&
	    !strcmp(error->message,
	    "Instructions.json metadata does not match the pinned Arm AARCHMRS 2026-06 source"))
		return TCTI_TARGET_INSTRUCTION_ARTIFACT_METADATA;
	if (error->code == TCTI_TARGET_IMPORT_NO_MEMORY)
		return TCTI_TARGET_INSTRUCTION_ARTIFACT_NO_MEMORY;
	return TCTI_TARGET_INSTRUCTION_ARTIFACT_PARSE;
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

static enum tcti_target_instruction_artifact_error build_conditions(
	const struct tcti_target_inventory *inventory, struct artifact_model *model)
{
	size_t index;

	if (inventory->expression_count > SIZE_MAX / sizeof(*model->condition_map))
		return TCTI_TARGET_INSTRUCTION_ARTIFACT_OVERFLOW;
	model->condition_map = calloc(inventory->expression_count,
				      sizeof(*model->condition_map));
	if (inventory->expression_count && !model->condition_map)
		return TCTI_TARGET_INSTRUCTION_ARTIFACT_NO_MEMORY;
	for (index = 0; index < inventory->expression_count; index++) {
		struct tcti_target_condition_bytes bytes = { 0 };
		enum tcti_target_condition_serialize_error error;

		if (tcti_target_condition_serialize(inventory, (uint32_t)index,
					     &bytes, &error))
			return TCTI_TARGET_INSTRUCTION_ARTIFACT_CONDITION;
		if (model->conditions.length > UINT32_MAX ||
		    bytes.length > UINT32_MAX ||
		    bytes_append(&model->conditions, bytes.data, bytes.length)) {
			tcti_target_condition_bytes_destroy(&bytes);
			return TCTI_TARGET_INSTRUCTION_ARTIFACT_OVERFLOW;
		}
		model->condition_map[index].offset = (uint32_t)(model->conditions.length - bytes.length);
		model->condition_map[index].length = (uint32_t)bytes.length;
		tcti_target_condition_bytes_destroy(&bytes);
	}
	return TCTI_TARGET_INSTRUCTION_ARTIFACT_OK;
}

static enum tcti_target_instruction_artifact_error build_model(
	const struct tcti_target_inventory *inventory, struct artifact_model *model)
{
	uint32_t *counts = NULL;
	uint32_t *cursors = NULL;
	size_t index;
	size_t total_operands = inventory->operand_count;
	enum tcti_target_instruction_artifact_error result;

	if (inventory->leaf_count != TCTI_A64_TARGET_LEAF_COUNT ||
	    inventory->leaf_count > UINT32_MAX || total_operands > UINT32_MAX ||
	    inventory->leaf_count > SIZE_MAX / sizeof(*model->leaves) ||
	    total_operands > SIZE_MAX / sizeof(*model->operands))
		return TCTI_TARGET_INSTRUCTION_ARTIFACT_OVERFLOW;
	model->leaves = calloc(inventory->leaf_count, sizeof(*model->leaves));
	model->operands = calloc(total_operands, sizeof(*model->operands));
	counts = calloc(inventory->leaf_count, sizeof(*counts));
	cursors = calloc(inventory->leaf_count, sizeof(*cursors));
	if (!model->leaves || (total_operands && !model->operands) || !counts || !cursors) {
		result = TCTI_TARGET_INSTRUCTION_ARTIFACT_NO_MEMORY;
		goto out;
	}
	for (index = 0; index < total_operands; index++) {
		const struct tcti_target_operand *operand = &inventory->operands[index];

		if (operand->leaf_index >= inventory->leaf_count ||
		    operand->condition >= inventory->expression_count ||
		    counts[operand->leaf_index] == UINT32_MAX) {
			result = TCTI_TARGET_INSTRUCTION_ARTIFACT_OVERFLOW;
			goto out;
		}
		counts[operand->leaf_index]++;
	}
	for (index = 0; index < inventory->leaf_count; index++) {
		struct artifact_leaf *leaf = &model->leaves[index];
		const struct tcti_target_leaf *source = &inventory->leaves[index];

		if (source->condition >= inventory->expression_count ||
		    (index && model->leaves[index - 1].operand_first > UINT32_MAX -
			model->leaves[index - 1].operand_count)) {
			result = TCTI_TARGET_INSTRUCTION_ARTIFACT_OVERFLOW;
			goto out;
		}
		leaf->operand_first = index ? model->leaves[index - 1].operand_first +
			model->leaves[index - 1].operand_count : 0;
		leaf->operand_count = counts[index];
		cursors[index] = leaf->operand_first;
		leaf->encoding_mask = source->encoding_mask;
		leaf->encoding_pattern = source->encoding_pattern;
		if (bytes_append_string(&model->strings, source->name, &leaf->name_offset) ||
		    bytes_append_string(&model->strings, source->mnemonic, &leaf->mnemonic_offset) ||
		    bytes_append_string(&model->strings, source->operation_id, &leaf->operation_offset)) {
			result = TCTI_TARGET_INSTRUCTION_ARTIFACT_OVERFLOW;
			goto out;
		}
	}
	result = build_conditions(inventory, model);
	if (result != TCTI_TARGET_INSTRUCTION_ARTIFACT_OK)
		goto out;
	for (index = 0; index < inventory->leaf_count; index++) {
		const struct tcti_target_leaf *source = &inventory->leaves[index];

		model->leaves[index].condition_offset =
			model->condition_map[source->condition].offset;
		model->leaves[index].condition_length =
			model->condition_map[source->condition].length;
	}
	for (index = 0; index < total_operands; index++) {
		const struct tcti_target_operand *source = &inventory->operands[index];
		struct artifact_operand *operand;
		uint32_t slot = cursors[source->leaf_index]++;

		if (slot >= total_operands) {
			result = TCTI_TARGET_INSTRUCTION_ARTIFACT_OVERFLOW;
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
			result = TCTI_TARGET_INSTRUCTION_ARTIFACT_OVERFLOW;
			goto out;
		}
	}
	model->operand_count = total_operands;
	result = TCTI_TARGET_INSTRUCTION_ARTIFACT_OK;
out:
	free(counts);
	free(cursors);
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
		"#define TCTI_A64_INSTRUCTION_ARTIFACT_VERSION 1U\n"
		"#define TCTI_A64_INSTRUCTION_ARTIFACT_ARCHITECTURE \"%s\"\n"
		"#define TCTI_A64_INSTRUCTION_ARTIFACT_BUILD \"%s\"\n"
		"#define TCTI_A64_INSTRUCTION_ARTIFACT_REFERENCE \"%s\"\n"
		"#define TCTI_A64_INSTRUCTION_ARTIFACT_SCHEMA \"%s\"\n"
		"#define TCTI_A64_INSTRUCTION_ARTIFACT_SOURCE_SHA256 \"%s\"\n"
		"#define TCTI_A64_INSTRUCTION_ARTIFACT_LEAF_COUNT %uU\n"
		"#define TCTI_A64_INSTRUCTION_ARTIFACT_OPERAND_COUNT %zuU\n\n",
		source_architecture, source_build, source_reference, source_schema,
		source_sha256, TCTI_A64_TARGET_LEAF_COUNT, model->operand_count))
		return -1;
	if (outputf(output,
		"static const struct tcti_target_instruction_artifact_leaf "
		"tcti_a64_instruction_artifact_leaves[%u] = {\n",
		TCTI_A64_TARGET_LEAF_COUNT))
		return -1;
	for (index = 0; index < TCTI_A64_TARGET_LEAF_COUNT; index++) {
		const struct artifact_leaf *leaf = &model->leaves[index];
		if (outputf(output,
			"    { %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, 0x%08" PRIx32 "U, 0x%08" PRIx32 "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U, %" PRIu32 "U },\n",
			leaf->name_offset, leaf->mnemonic_offset, leaf->operation_offset,
			leaf->encoding_mask, leaf->encoding_pattern, leaf->condition_offset,
			leaf->condition_length, leaf->operand_first, leaf->operand_count))
			return -1;
	}
	if (outputf(output, "};\n\nstatic const struct tcti_target_instruction_artifact_operand "
		"tcti_a64_instruction_artifact_operands[%zu] = {\n", model->operand_count))
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
	if (emit_u8_array(output, "tcti_a64_instruction_artifact_string_pool",
			  &model->strings) ||
	    emit_u8_array(output, "tcti_a64_instruction_artifact_condition_pool",
			  &model->conditions))
		return -1;
	return outputf(output,
		"static const struct tcti_target_instruction_artifact "
		"tcti_a64_instruction_artifact = {\n"
		"    .version = TCTI_A64_INSTRUCTION_ARTIFACT_VERSION,\n"
		"    .architecture = TCTI_A64_INSTRUCTION_ARTIFACT_ARCHITECTURE,\n"
		"    .build = TCTI_A64_INSTRUCTION_ARTIFACT_BUILD,\n"
		"    .reference = TCTI_A64_INSTRUCTION_ARTIFACT_REFERENCE,\n"
		"    .schema = TCTI_A64_INSTRUCTION_ARTIFACT_SCHEMA,\n"
		"    .source_sha256 = TCTI_A64_INSTRUCTION_ARTIFACT_SOURCE_SHA256,\n"
		"    .leaves = tcti_a64_instruction_artifact_leaves,\n"
		"    .leaf_count = TCTI_A64_INSTRUCTION_ARTIFACT_LEAF_COUNT,\n"
		"    .operands = tcti_a64_instruction_artifact_operands,\n"
		"    .operand_count = TCTI_A64_INSTRUCTION_ARTIFACT_OPERAND_COUNT,\n"
		"    .string_pool = tcti_a64_instruction_artifact_string_pool,\n"
		"    .string_pool_size = sizeof(tcti_a64_instruction_artifact_string_pool),\n"
		"    .condition_pool = tcti_a64_instruction_artifact_condition_pool,\n"
		"    .condition_pool_size = sizeof(tcti_a64_instruction_artifact_condition_pool),\n"
		"};\n\n"
		"#endif /* ORLIX_TCTI_A64_INSTRUCTION_ARTIFACT_GENERATED_H */\n");
}

const char *tcti_target_instruction_artifact_error_name(
	enum tcti_target_instruction_artifact_error error)
{
	switch (error) {
	case TCTI_TARGET_INSTRUCTION_ARTIFACT_OK: return "success";
	case TCTI_TARGET_INSTRUCTION_ARTIFACT_INVALID_ARGUMENT: return "invalid argument";
	case TCTI_TARGET_INSTRUCTION_ARTIFACT_PARSE: return "parse failure";
	case TCTI_TARGET_INSTRUCTION_ARTIFACT_METADATA: return "metadata failure";
	case TCTI_TARGET_INSTRUCTION_ARTIFACT_COUNT: return "count failure";
	case TCTI_TARGET_INSTRUCTION_ARTIFACT_DIGEST: return "digest failure";
	case TCTI_TARGET_INSTRUCTION_ARTIFACT_CONDITION: return "condition serialization failure";
	case TCTI_TARGET_INSTRUCTION_ARTIFACT_OVERFLOW: return "size overflow";
	case TCTI_TARGET_INSTRUCTION_ARTIFACT_NO_MEMORY: return "out of memory";
	case TCTI_TARGET_INSTRUCTION_ARTIFACT_IO: return "I/O failure";
	}
	return "unknown failure";
}

/*
 * Keep all fallible construction ahead of the first fwrite.  Focused host
 * tests include this translation unit and exercise this boundary with count
 * and size-overflow mutations that cannot be encoded in the pinned source.
 */
static enum tcti_target_instruction_artifact_error generate_from_inventory(
	const struct tcti_target_inventory *inventory, struct artifact_bytes *generated)
{
	struct artifact_model model = { 0 };
	enum tcti_target_instruction_artifact_error result;

	if (!inventory || !generated)
		return TCTI_TARGET_INSTRUCTION_ARTIFACT_INVALID_ARGUMENT;
	if (inventory->leaf_count != TCTI_A64_TARGET_LEAF_COUNT)
		return TCTI_TARGET_INSTRUCTION_ARTIFACT_COUNT;
	result = build_model(inventory, &model);
	if (result == TCTI_TARGET_INSTRUCTION_ARTIFACT_OK &&
	    emit_artifact(generated, &model))
		result = TCTI_TARGET_INSTRUCTION_ARTIFACT_NO_MEMORY;
	artifact_model_destroy(&model);
	return result;
}

enum tcti_target_instruction_artifact_error
tcti_target_instruction_artifact_emit(const char *source, size_t length,
				      FILE *output)
{
	struct tcti_target_inventory inventory = { 0 };
	struct tcti_target_import_error import_error = { 0 };
	struct artifact_bytes generated = { 0 };
	char digest[65];
	enum tcti_target_instruction_artifact_error result;

	if (!source || !output)
		return TCTI_TARGET_INSTRUCTION_ARTIFACT_INVALID_ARGUMENT;
	if (length > TCTI_ARTIFACT_MAX_SOURCE_BYTES)
		return TCTI_TARGET_INSTRUCTION_ARTIFACT_OVERFLOW;
	if (source_has_embedded_nul(source, length))
		return TCTI_TARGET_INSTRUCTION_ARTIFACT_PARSE;
	if (tcti_target_inventory_import(source, length, &inventory, &import_error)) {
		result = import_category(&import_error);
		goto out;
	}
	sha256_hex(source, length, digest);
	if (strcmp(digest, source_sha256)) {
		result = TCTI_TARGET_INSTRUCTION_ARTIFACT_DIGEST;
		goto out;
	}
	result = generate_from_inventory(&inventory, &generated);
	if (result != TCTI_TARGET_INSTRUCTION_ARTIFACT_OK)
		goto out;
	if (generated.length && fwrite(generated.data, 1, generated.length, output) != generated.length)
		result = TCTI_TARGET_INSTRUCTION_ARTIFACT_IO;
	else
		result = TCTI_TARGET_INSTRUCTION_ARTIFACT_OK;
	out:
	free(generated.data);
	tcti_target_inventory_destroy(&inventory);
	return result;
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
	    (uintmax_t)file_length > TCTI_ARTIFACT_MAX_SOURCE_BYTES) {
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
	enum tcti_target_instruction_artifact_error error;

	if (argc != 2) {
		fprintf(stderr, "usage: %s Instructions.json > generated.h\n", argv[0]);
		return EXIT_FAILURE;
	}
	if (read_source(argv[1], &source, &length)) {
		fprintf(stderr, "target instruction artifact generator: cannot read source\n");
		return EXIT_FAILURE;
	}
	error = tcti_target_instruction_artifact_emit(source, length, stdout);
	if (error != TCTI_TARGET_INSTRUCTION_ARTIFACT_OK) {
		fprintf(stderr, "target instruction artifact generator: %s\n",
			tcti_target_instruction_artifact_error_name(error));
		free(source);
		return EXIT_FAILURE;
	}
	free(source);
	return EXIT_SUCCESS;
}
#endif
