/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_instruction_artifact.h"
#include "target_condition_format.h"
#ifdef ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_EXTERNAL_GENERATED
#include <target_instruction_artifact_generated.h>
#else
#include "isa/target_instruction_artifact_generated.h"
#endif

#ifdef __KERNEL__
#include <linux/kernel.h>
#include <linux/string.h>
typedef u64 artifact_u64;
#else
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
typedef uint64_t artifact_u64;
#endif

#define ORLIX_TCTI_U32_NONE ((u32)~0U)

static const char pinned_architecture[] = "vFATAp1-A";
static const char pinned_build[] = "818";
static const char pinned_reference[] = "2026-06_rel";
static const char pinned_schema[] = "2.9.5";
static const char pinned_source_sha256[] =
	"a1ad2c6538a47cd97d8762791ac5af88bce1d5f6aff096c9b77aef853e76acfe";

const struct orlix_tcti_target_instruction_artifact *
orlix_tcti_target_instruction_artifact_canonical(void)
{
	return &orlix_tcti_a64_instruction_artifact;
}

static int fail(struct orlix_tcti_target_instruction_artifact_validation_result *result,
		enum orlix_tcti_target_instruction_artifact_validation_error error,
		u32 leaf_index, u32 operand_index)
{
	if (result)
		*result = (struct orlix_tcti_target_instruction_artifact_validation_result) {
			.error = error,
			.leaf_index = leaf_index,
			.operand_index = operand_index,
			.alias_index = ORLIX_TCTI_U32_NONE,
			.operational_note_index = ORLIX_TCTI_U32_NONE,
		};
	return -1;
}

static bool valid_source_span(u32 offset, u32 length)
{
	return length && offset <= ORLIX_TCTI_U32_NONE - length;
}

static bool exact_string(const char *actual, const char *expected)
{
	return actual && !strcmp(actual, expected);
}

static bool valid_span(u32 offset, u32 length, size_t pool_size)
{
	return offset <= pool_size && length <= pool_size - offset;
}

struct artifact_sha256 {
	u32 state[8];
	artifact_u64 bytes;
	u8 block[64];
	size_t used;
};

static u32 sha256_ror(u32 value, unsigned int amount)
{
	return (value >> amount) | (value << (32U - amount));
}

static void sha256_block(struct artifact_sha256 *sha, const u8 *block)
{
	static const u32 k[64] = {
		0x428a2f98U,0x71374491U,0xb5c0fbcfU,0xe9b5dba5U,0x3956c25bU,0x59f111f1U,0x923f82a4U,0xab1c5ed5U,
		0xd807aa98U,0x12835b01U,0x243185beU,0x550c7dc3U,0x72be5d74U,0x80deb1feU,0x9bdc06a7U,0xc19bf174U,
		0xe49b69c1U,0xefbe4786U,0x0fc19dc6U,0x240ca1ccU,0x2de92c6fU,0x4a7484aaU,0x5cb0a9dcU,0x76f988daU,
		0x983e5152U,0xa831c66dU,0xb00327c8U,0xbf597fc7U,0xc6e00bf3U,0xd5a79147U,0x06ca6351U,0x14292967U,
		0x27b70a85U,0x2e1b2138U,0x4d2c6dfcU,0x53380d13U,0x650a7354U,0x766a0abbU,0x81c2c92eU,0x92722c85U,
		0xa2bfe8a1U,0xa81a664bU,0xc24b8b70U,0xc76c51a3U,0xd192e819U,0xd6990624U,0xf40e3585U,0x106aa070U,
		0x19a4c116U,0x1e376c08U,0x2748774cU,0x34b0bcb5U,0x391c0cb3U,0x4ed8aa4aU,0x5b9cca4fU,0x682e6ff3U,
		0x748f82eeU,0x78a5636fU,0x84c87814U,0x8cc70208U,0x90befffaU,0xa4506cebU,0xbef9a3f7U,0xc67178f2U,
	};
	u32 w[64];
	u32 a, b, c, d, e, f, g, h;
	size_t i;

	for (i = 0; i < 16; i++)
		w[i] = ((u32)block[i * 4] << 24) |
			((u32)block[i * 4 + 1] << 16) |
			((u32)block[i * 4 + 2] << 8) | block[i * 4 + 3];
	for (i = 16; i < 64; i++) {
		u32 s0 = sha256_ror(w[i - 15], 7) ^ sha256_ror(w[i - 15], 18) ^
			(w[i - 15] >> 3);
		u32 s1 = sha256_ror(w[i - 2], 17) ^ sha256_ror(w[i - 2], 19) ^
			(w[i - 2] >> 10);
		w[i] = w[i - 16] + s0 + w[i - 7] + s1;
	}
	a = sha->state[0]; b = sha->state[1]; c = sha->state[2]; d = sha->state[3];
	e = sha->state[4]; f = sha->state[5]; g = sha->state[6]; h = sha->state[7];
	for (i = 0; i < 64; i++) {
		u32 s1 = sha256_ror(e, 6) ^ sha256_ror(e, 11) ^ sha256_ror(e, 25);
		u32 ch = (e & f) ^ (~e & g);
		u32 t1 = h + s1 + ch + k[i] + w[i];
		u32 s0 = sha256_ror(a, 2) ^ sha256_ror(a, 13) ^ sha256_ror(a, 22);
		u32 maj = (a & b) ^ (a & c) ^ (b & c);
		u32 t2 = s0 + maj;
		h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
	}
	sha->state[0] += a; sha->state[1] += b; sha->state[2] += c; sha->state[3] += d;
	sha->state[4] += e; sha->state[5] += f; sha->state[6] += g; sha->state[7] += h;
}

static void sha256_update(struct artifact_sha256 *sha, const u8 *data, size_t length)
{
	sha->bytes += length;
	while (length) {
		size_t copied = length < 64U - sha->used ? length : 64U - sha->used;
		memcpy(sha->block + sha->used, data, copied);
		sha->used += copied; data += copied; length -= copied;
		if (sha->used == 64U) { sha256_block(sha, sha->block); sha->used = 0; }
	}
}

static void sha256_hex(const u8 *data, size_t length, char output[65])
{
	static const char hex[] = "0123456789abcdef";
	struct artifact_sha256 sha = { .state = {
		0x6a09e667U,0xbb67ae85U,0x3c6ef372U,0xa54ff53aU,
		0x510e527fU,0x9b05688cU,0x1f83d9abU,0x5be0cd19U } };
	artifact_u64 bits;
	size_t i;

	sha256_update(&sha, data, length);
	bits = sha.bytes * 8U;
	sha.block[sha.used++] = 0x80U;
	if (sha.used > 56U) {
		memset(sha.block + sha.used, 0, 64U - sha.used);
		sha256_block(&sha, sha.block);
		sha.used = 0;
	}
	memset(sha.block + sha.used, 0, 56U - sha.used);
	for (i = 0; i < 8; i++) sha.block[56U + i] = (u8)(bits >> (56U - i * 8U));
	sha256_block(&sha, sha.block);
	for (i = 0; i < 8; i++) {
		u32 word = sha.state[i];
		size_t byte;
		for (byte = 0; byte < 4; byte++) {
			u8 value = (u8)(word >> (24U - byte * 8U));
			output[(i * 4U + byte) * 2U] = hex[value >> 4];
			output[(i * 4U + byte) * 2U + 1U] = hex[value & 15U];
		}
	}
	output[64] = '\0';
}

static bool pool_string(const u8 *pool, size_t pool_size, u32 offset,
		const char **text)
{
	const u8 *nul;

	if (offset >= pool_size || (offset && pool[offset - 1U] != '\0'))
		return false;
	nul = memchr(pool + offset, '\0', pool_size - offset);
	if (!nul || nul == pool + offset)
		return false;
	if (text)
		*text = (const char *)pool + offset;
	return true;
}

static bool span_identity_matches(const char *source_sha256,
				  const u8 *pool, size_t pool_size,
				  u32 identity_offset, u32 source_offset,
				  u32 source_length)
{
	char expected[64U + 2U + 20U + 20U + 1U];
	const char *actual;
	int count;

	if (!valid_source_span(source_offset, source_length) ||
	    !pool_string(pool, pool_size, identity_offset, &actual))
		return false;
	count = snprintf(expected, sizeof(expected), "%s:%u:%u",
			 source_sha256, source_offset, source_length);
	return count > 0 && (size_t)count < sizeof(expected) &&
		!strcmp(actual, expected);
}

static bool read_u32be(const u8 *data, u32 length, u32 *offset, u32 *value)
{
	if (*offset > length || length - *offset < 4U)
		return false;
	*value = ((u32)data[*offset] << 24) |
		((u32)data[*offset + 1U] << 16) |
		((u32)data[*offset + 2U] << 8) |
		(u32)data[*offset + 3U];
	*offset += 4U;
	return true;
}

/* Eight bytes per frame, or 2 KiB at the TCND v1 maximum depth. */
struct tcnd_frame {
	u32 end;
	u32 remaining;
};

enum tcnd_record_result {
	TCND_RECORD_INVALID = -1,
	TCND_RECORD_COMPLETE,
	TCND_RECORD_CHILDREN,
};

static enum tcnd_record_result valid_condition_record(const u8 *data,
	u32 length, u32 *offset, u32 limit, struct tcnd_frame *frames,
	u32 *depth)
{
	u8 tag;
	u32 payload_length;
	u32 payload_offset;
	u32 payload_end;
	u32 item_count;

	if (*depth >= ORLIX_TCTI_TARGET_CONDITION_MAX_DEPTH || *offset > limit ||
	    limit > length || limit - *offset < 5U)
		return TCND_RECORD_INVALID;
	tag = data[(*offset)++];
	if (!read_u32be(data, limit, offset, &payload_length) ||
	    payload_length > limit - *offset)
		return TCND_RECORD_INVALID;
	payload_offset = *offset;
	payload_end = payload_offset + payload_length;
	switch (tag) {
	case ORLIX_TCTI_TARGET_CONDITION_BOOL:
		if (payload_length != 1U || data[payload_offset] > 1U)
			return TCND_RECORD_INVALID;
		*offset = payload_end;
		return TCND_RECORD_COMPLETE;
	case ORLIX_TCTI_TARGET_CONDITION_FEATURE:
	case ORLIX_TCTI_TARGET_CONDITION_OPERAND:
	case ORLIX_TCTI_TARGET_CONDITION_VALUE:
		if (!read_u32be(data, payload_end, offset, &item_count) ||
		    item_count != payload_end - *offset)
			return TCND_RECORD_INVALID;
		*offset = payload_end;
		return TCND_RECORD_COMPLETE;
	case ORLIX_TCTI_TARGET_CONDITION_SET:
		if (!read_u32be(data, payload_end, offset, &item_count))
			return TCND_RECORD_INVALID;
		break;
	case ORLIX_TCTI_TARGET_CONDITION_NOT:
		item_count = 1U;
		break;
	case ORLIX_TCTI_TARGET_CONDITION_AND:
	case ORLIX_TCTI_TARGET_CONDITION_OR:
	case ORLIX_TCTI_TARGET_CONDITION_EQ:
	case ORLIX_TCTI_TARGET_CONDITION_NE:
	case ORLIX_TCTI_TARGET_CONDITION_IN:
		item_count = 2U;
		break;
	default:
		return TCND_RECORD_INVALID;
	}
	if (!item_count)
		return *offset == payload_end ? TCND_RECORD_COMPLETE :
			TCND_RECORD_INVALID;
	if (*depth >= ORLIX_TCTI_TARGET_CONDITION_MAX_DEPTH)
		return TCND_RECORD_INVALID;
	frames[(*depth)++] = (struct tcnd_frame) {
		.end = payload_end,
		.remaining = item_count,
	};
	return TCND_RECORD_CHILDREN;
}

static int tcnd_child_complete(struct tcnd_frame *frames, u32 *depth,
			       u32 *offset, u32 length)
{
	while (*depth) {
		struct tcnd_frame *frame = &frames[*depth - 1U];

		if (!frame->remaining)
			return -1;
		frame->remaining--;
		if (frame->remaining)
			return 0;
		if (*offset != frame->end)
			return -1;
		(*depth)--;
	}
	return *offset == length ? 1 : -1;
}

/*
 * This proves only that the generated bytes are a bounded TCND v1 object.
 * Pinned-source byte equality and the semantics of the expression remain the
 * refresh/audit responsibility, rather than a second source interpreter here.
 */
static bool valid_condition(const u8 *pool, size_t pool_size,
			    u32 offset, u32 length)
{
	static const u8 header[] = { 'T', 'C', 'N', 'D', 1U };
	struct tcnd_frame frames[ORLIX_TCTI_TARGET_CONDITION_MAX_DEPTH];
	u32 record_offset = sizeof(header);
	u32 depth = 0;

	if (length > ORLIX_TCTI_TARGET_CONDITION_MAX_SERIALIZED_BYTES ||
	    length <= sizeof(header) || !valid_span(offset, length, pool_size) ||
	    memcmp(pool + offset, header, sizeof(header)))
		return false;
	for (;;) {
		u32 limit = depth ? frames[depth - 1U].end : length;
		enum tcnd_record_result parsed = valid_condition_record(pool + offset,
			length, &record_offset, limit, frames, &depth);
		int complete;

		if (parsed == TCND_RECORD_INVALID)
			return false;
		if (parsed == TCND_RECORD_CHILDREN)
			continue;
		complete = tcnd_child_complete(frames, &depth, &record_offset, length);
		if (complete < 0)
			return false;
		if (complete > 0)
			return true;
	}
}

static bool valid_operand_shape(
	const struct orlix_tcti_target_instruction_artifact_leaf *leaf,
	const struct orlix_tcti_target_instruction_artifact_operand *operand)
{
	u32 field_mask;

	if (!operand->width || operand->start >= 32U ||
	    operand->width > 32U - operand->start || !operand->variable_mask)
		return false;
	field_mask = operand->width == 32U ? ORLIX_TCTI_U32_NONE :
		((1U << operand->width) - 1U) << operand->start;
	/* A variable bit cannot simultaneously be fixed by this leaf. */
	return !(operand->variable_mask & ~field_mask) &&
		!(operand->variable_mask & leaf->encoding_mask);
}

static bool valid_fixed_operand_shape(
	const struct orlix_tcti_target_instruction_artifact_leaf *leaf,
	const struct orlix_tcti_target_instruction_artifact_fixed_operand *operand)
{
	u32 field_mask;

	if (!operand->width || operand->start >= 32U ||
	    operand->width > 32U - operand->start)
		return false;
	field_mask = operand->width == 32U ? ORLIX_TCTI_U32_NONE :
		((1U << operand->width) - 1U) << operand->start;
	return operand->fixed_mask == field_mask &&
		!(operand->fixed_value & ~field_mask) &&
		(leaf->encoding_mask & field_mask) == field_mask &&
		(leaf->encoding_pattern & field_mask) == operand->fixed_value;
}

static int fail_alias(
	struct orlix_tcti_target_instruction_artifact_validation_result *result,
	enum orlix_tcti_target_instruction_artifact_validation_error error,
	u32 alias_index)
{
	if (result)
		*result = (struct orlix_tcti_target_instruction_artifact_validation_result) {
			.error = error,
			.leaf_index = ORLIX_TCTI_U32_NONE,
			.operand_index = ORLIX_TCTI_U32_NONE,
			.alias_index = alias_index,
			.operational_note_index = ORLIX_TCTI_U32_NONE,
		};
	return -1;
}

static int fail_operational_note(
	struct orlix_tcti_target_instruction_artifact_validation_result *result,
	enum orlix_tcti_target_instruction_artifact_validation_error error,
	u32 note_index)
{
	if (result)
		*result = (struct orlix_tcti_target_instruction_artifact_validation_result) {
			.error = error,
			.leaf_index = ORLIX_TCTI_U32_NONE,
			.operand_index = ORLIX_TCTI_U32_NONE,
			.alias_index = ORLIX_TCTI_U32_NONE,
			.operational_note_index = note_index,
		};
	return -1;
}

static bool sha256_string(const struct orlix_tcti_target_instruction_artifact *artifact,
			  u32 offset)
{
	const char *digest;
	size_t index;

	if (!pool_string(artifact->string_pool, artifact->string_pool_size,
			offset, &digest) || strlen(digest) != 64U)
		return false;
	for (index = 0; index < 64U; index++)
		if (!((digest[index] >= '0' && digest[index] <= '9') ||
		      (digest[index] >= 'a' && digest[index] <= 'f')))
			return false;
	return true;
}

static int validate_operational_notes(
	const struct orlix_tcti_target_instruction_artifact *artifact,
	struct orlix_tcti_target_instruction_artifact_validation_result *result)
{
	size_t index;
	u32 previous_leaf = ORLIX_TCTI_U32_NONE;

	if (artifact->operational_note_count > artifact->leaf_count ||
	    (artifact->operational_note_count && !artifact->operational_notes) ||
	    (!artifact->operational_note_count && artifact->operational_notes))
		return fail_operational_note(result,
			ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_COUNT_MISMATCH,
			ORLIX_TCTI_U32_NONE);
	for (index = 0; index < artifact->operational_note_count; index++) {
		const struct orlix_tcti_target_instruction_artifact_operational_note *note =
			&artifact->operational_notes[index];

		if (note->leaf_index >= artifact->leaf_count ||
		    (index && note->leaf_index <= previous_leaf) ||
		    note->kind !=
			ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_OPERATIONAL_NOTE_BEHAVIOR_OBLIGATION ||
		    !valid_source_span(note->source_offset, note->source_length) ||
		    !sha256_string(artifact, note->source_sha256_offset))
			return fail_operational_note(result,
				ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_OPERATIONAL_NOTE_INVALID,
				(u32)index);
		if (!span_identity_matches(artifact->source_sha256, artifact->string_pool,
			artifact->string_pool_size, note->source_identity_offset,
			note->source_offset, note->source_length))
			return fail_operational_note(result,
				ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_OPERATIONAL_NOTE_IDENTITY_INVALID,
				(u32)index);
		previous_leaf = note->leaf_index;
	}
	return 0;
}

static bool artifact_string(const struct orlix_tcti_target_instruction_artifact *artifact,
			    u32 offset, const char **text)
{
	return pool_string(artifact->string_pool, artifact->string_pool_size,
		offset, text);
}

static bool predicate_digest_matches(
	const struct orlix_tcti_target_instruction_artifact *artifact,
	u32 offset, u32 length, u32 digest_offset)
{
	char expected[65];
	const char *actual;

	if (!valid_condition(artifact->condition_pool,
		artifact->condition_pool_size, offset, length) ||
	    !artifact_string(artifact, digest_offset, &actual) ||
	    strlen(actual) != 64U)
		return false;
	sha256_hex(artifact->condition_pool + offset, length, expected);
	return !strcmp(actual, expected);
}

static bool unconditional_predicate(
	const struct orlix_tcti_target_instruction_artifact *artifact,
	u32 offset, u32 length)
{
	static const u8 expected[] = {
		'T', 'C', 'N', 'D', 1U,
		ORLIX_TCTI_TARGET_CONDITION_BOOL, 0U, 0U, 0U, 1U, 1U,
	};

	return length == sizeof(expected) &&
		valid_span(offset, length, artifact->condition_pool_size) &&
		!memcmp(artifact->condition_pool + offset, expected, sizeof(expected));
}

static int operation_alias_index(const struct orlix_tcti_target_instruction_artifact *artifact,
				 const char *declared_operation)
{
	size_t index;

	for (index = 0; index < artifact->operation_alias_count; index++) {
		const struct orlix_tcti_target_instruction_artifact_operation_alias *alias =
			&artifact->operation_aliases[index];
		const char *declared;

		if (!artifact_string(artifact, alias->declared_operation_offset,
				&declared))
			return -2;
		if (!strcmp(declared, declared_operation))
			return (int)index;
	}
	return -1;
}

static bool operation_resolves(const struct orlix_tcti_target_instruction_artifact *artifact,
			       const char *declared_operation, const char *resolved_operation)
{
	const char *current = declared_operation;
	size_t steps;

	for (steps = 0; steps <= artifact->operation_alias_count; steps++) {
		int index = operation_alias_index(artifact, current);
		const char *target;

		if (index == -2)
			return false;
		if (index < 0)
			return !strcmp(current, resolved_operation);
		if (!artifact_string(artifact,
			artifact->operation_aliases[index].target_operation_offset,
			&target))
			return false;
		current = target;
	}
	return false;
}

/*
 * An OperationAlias may traverse other OperationAlias records, but its final
 * operation must still be represented by a source instruction leaf.  Without
 * this check, a mutually consistent pair of fabricated target and resolved
 * strings can pass validation even though no pinned instruction owns the
 * semantics.  Alias provenance is supplemental, never a substitute for a
 * semantic leaf and its proof obligation.
 */
static bool operation_has_source_leaf(
	const struct orlix_tcti_target_instruction_artifact *artifact,
	const char *operation)
{
	size_t index;

	for (index = 0; index < artifact->leaf_count; index++) {
		const char *leaf_operation;

		if (!artifact_string(artifact,
			artifact->leaves[index].operation_offset, &leaf_operation))
			return false;
		if (!strcmp(leaf_operation, operation))
			return true;
	}
	return false;
}

static bool mark_operation_alias_reachability(
	const struct orlix_tcti_target_instruction_artifact *artifact,
	const char *operation, bool reachable[
		ORLIX_TCTI_A64_INSTRUCTION_ARTIFACT_OPERATION_ALIAS_COUNT])
{
	const char *current = operation;
	size_t steps;

	for (steps = 0; steps <= artifact->operation_alias_count; steps++) {
		int index = operation_alias_index(artifact, current);
		const char *target;

		if (index == -2)
			return false;
		if (index < 0)
			return true;
		reachable[index] = true;
		if (!artifact_string(artifact,
			artifact->operation_aliases[index].target_operation_offset,
			&target))
			return false;
		current = target;
	}
	return false;
}

static int validate_aliases(const struct orlix_tcti_target_instruction_artifact *artifact,
			    struct orlix_tcti_target_instruction_artifact_validation_result *result)
{
	bool reachable[ORLIX_TCTI_A64_INSTRUCTION_ARTIFACT_OPERATION_ALIAS_COUNT] = { 0 };
	size_t index;

	if (artifact->instruction_alias_count !=
		ORLIX_TCTI_A64_INSTRUCTION_ARTIFACT_INSTRUCTION_ALIAS_COUNT ||
	    artifact->operation_alias_count !=
		ORLIX_TCTI_A64_INSTRUCTION_ARTIFACT_OPERATION_ALIAS_COUNT ||
	    !artifact->instruction_aliases || !artifact->operation_aliases)
		return fail_alias(result, ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_COUNT_MISMATCH,
			ORLIX_TCTI_U32_NONE);
	for (index = 0; index < artifact->instruction_alias_count; index++) {
		const struct orlix_tcti_target_instruction_artifact_instruction_alias *alias =
			&artifact->instruction_aliases[index];
		const char *declared;
		const char *resolved;

		if (alias->ordinal != index || alias->preferred_present > 1U ||
		    alias->relation_kind !=
			ORLIX_TCTI_TARGET_ALIAS_RELATION_ASSEMBLER_ONLY ||
		    alias->predicate_kind !=
			ORLIX_TCTI_TARGET_ALIAS_PREDICATE_SOURCE_CONDITION ||
		    !artifact_string(artifact, alias->name_offset, NULL) ||
		    !artifact_string(artifact, alias->declared_operation_offset, &declared) ||
		    !artifact_string(artifact, alias->resolved_operation_offset, &resolved) ||
		    !predicate_digest_matches(artifact, alias->condition_offset,
			alias->condition_length, alias->predicate_sha256_offset))
			return fail_alias(result, ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_ALIAS_INVALID,
				(u32)index);
		if (!span_identity_matches(artifact->source_sha256, artifact->string_pool,
			artifact->string_pool_size, alias->source_identity_offset,
			alias->source_offset, alias->source_length) ||
		    !span_identity_matches(artifact->source_sha256, artifact->string_pool,
			artifact->string_pool_size, alias->condition_identity_offset,
			alias->condition_source_offset, alias->condition_source_length) ||
		    !span_identity_matches(artifact->source_sha256, artifact->string_pool,
			artifact->string_pool_size, alias->preferred_identity_offset,
			alias->preferred_source_offset, alias->preferred_source_length))
			return fail_alias(result,
				ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_ALIAS_IDENTITY_INVALID,
				(u32)index);
		if (!operation_resolves(artifact, declared, resolved) ||
		    !operation_has_source_leaf(artifact, resolved))
			return fail_alias(result, ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_ALIAS_INVALID,
				(u32)index);
	}
	for (index = 0; index < artifact->operation_alias_count; index++) {
		const struct orlix_tcti_target_instruction_artifact_operation_alias *alias =
			&artifact->operation_aliases[index];
		const char *declared;
		const char *resolved;
		size_t previous;

		if (alias->relation_kind !=
			ORLIX_TCTI_TARGET_ALIAS_RELATION_SEMANTIC ||
		    alias->predicate_kind !=
			ORLIX_TCTI_TARGET_ALIAS_PREDICATE_SCHEMA_UNCONDITIONAL ||
		    !artifact_string(artifact, alias->declared_operation_offset, &declared) ||
		    !artifact_string(artifact, alias->target_operation_offset, NULL) ||
		    !artifact_string(artifact, alias->resolved_operation_offset, &resolved) ||
		    !span_identity_matches(artifact->source_sha256, artifact->string_pool,
			artifact->string_pool_size, alias->source_identity_offset,
			alias->source_offset, alias->source_length) ||
		    !unconditional_predicate(artifact, alias->predicate_offset,
			alias->predicate_length) ||
		    !predicate_digest_matches(artifact, alias->predicate_offset,
			alias->predicate_length, alias->predicate_sha256_offset) ||
		    !operation_resolves(artifact, declared, resolved) ||
		    !operation_has_source_leaf(artifact, resolved))
			return fail_alias(result, ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_ALIAS_INVALID,
				(u32)index);
		for (previous = 0; previous < index; previous++) {
			const char *prior;

			if (!artifact_string(artifact,
				artifact->operation_aliases[previous].declared_operation_offset,
				&prior) || !strcmp(prior, declared))
				return fail_alias(result,
					ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_ALIAS_INVALID,
					(u32)index);
		}
	}
	for (index = 0; index < artifact->leaf_count; index++) {
		const char *operation;

		if (!artifact_string(artifact, artifact->leaves[index].operation_offset,
			&operation) || !mark_operation_alias_reachability(artifact,
			operation, reachable))
			return fail_alias(result, ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_ALIAS_INVALID,
			ORLIX_TCTI_U32_NONE);
	}
	for (index = 0; index < artifact->instruction_alias_count; index++) {
		const char *operation;

		if (!artifact_string(artifact,
			artifact->instruction_aliases[index].declared_operation_offset,
			&operation) || !mark_operation_alias_reachability(artifact,
			operation, reachable))
			return fail_alias(result, ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_ALIAS_INVALID,
			(u32)index);
	}
	for (index = 0; index < artifact->operation_alias_count; index++)
		if (!reachable[index])
			return fail_alias(result, ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_ALIAS_INVALID,
			(u32)index);
	return 0;
}

int orlix_tcti_target_instruction_artifact_validate_expected(
	const struct orlix_tcti_target_instruction_artifact *artifact,
	const char *expected_source_sha256,
	struct orlix_tcti_target_instruction_artifact_validation_result *result)
{
	u32 expected_operand = 0;
	u32 expected_fixed_operand = 0;
	size_t leaf_index;

	if (result)
		*result = (struct orlix_tcti_target_instruction_artifact_validation_result) {
			.error = ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_VALID,
			.leaf_index = ORLIX_TCTI_U32_NONE,
			.operand_index = ORLIX_TCTI_U32_NONE,
			.alias_index = ORLIX_TCTI_U32_NONE,
			.operational_note_index = ORLIX_TCTI_U32_NONE,
		};
	if (!artifact || !expected_source_sha256 ||
	    strlen(expected_source_sha256) != 64U)
		return fail(result, ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_INVALID_ARGUMENT,
				    ORLIX_TCTI_U32_NONE, ORLIX_TCTI_U32_NONE);
	if (artifact->version != ORLIX_TCTI_A64_INSTRUCTION_ARTIFACT_VERSION)
		return fail(result, ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_VERSION_MISMATCH,
				    ORLIX_TCTI_U32_NONE, ORLIX_TCTI_U32_NONE);
	if (!exact_string(artifact->architecture, pinned_architecture) ||
	    !exact_string(artifact->build, pinned_build) ||
	    !exact_string(artifact->reference, pinned_reference) ||
	    !exact_string(artifact->schema, pinned_schema) ||
	    !exact_string(artifact->source_sha256, expected_source_sha256))
		return fail(result, ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_PROVENANCE_MISMATCH,
				    ORLIX_TCTI_U32_NONE, ORLIX_TCTI_U32_NONE);
	if (artifact->leaf_count != ORLIX_TCTI_A64_INSTRUCTION_ARTIFACT_LEAF_COUNT ||
	    artifact->operand_count > ORLIX_TCTI_U32_NONE ||
	    artifact->fixed_operand_count !=
		ORLIX_TCTI_A64_INSTRUCTION_ARTIFACT_EXPECTED_FIXED_OPERAND_COUNT ||
	    artifact->fixed_operand_count > ORLIX_TCTI_U32_NONE)
		return fail(result, ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_COUNT_MISMATCH,
				    ORLIX_TCTI_U32_NONE, ORLIX_TCTI_U32_NONE);
	if (!artifact->leaves || !artifact->string_pool || !artifact->condition_pool ||
	    !artifact->string_pool_size || !artifact->condition_pool_size ||
	    (artifact->operand_count && !artifact->operands) ||
	    (artifact->fixed_operand_count && !artifact->fixed_operands))
		return fail(result, ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_POOL_INVALID,
				    ORLIX_TCTI_U32_NONE, ORLIX_TCTI_U32_NONE);

	for (leaf_index = 0; leaf_index < artifact->leaf_count; leaf_index++) {
		const struct orlix_tcti_target_instruction_artifact_leaf *leaf =
			&artifact->leaves[leaf_index];
		const char *name;
		size_t previous;
		u32 operand_mask = 0;
		u32 fixed_mask = 0;

		if (leaf_index > ORLIX_TCTI_U32_NONE ||
		    leaf->encoding_pattern & ~leaf->encoding_mask)
			return fail(result, ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_LEAF_INVALID,
					    (u32)leaf_index, ORLIX_TCTI_U32_NONE);
		if (!pool_string(artifact->string_pool, artifact->string_pool_size,
				 leaf->name_offset, &name) ||
		    !pool_string(artifact->string_pool, artifact->string_pool_size,
				 leaf->mnemonic_offset, NULL) ||
		    !pool_string(artifact->string_pool, artifact->string_pool_size,
				 leaf->operation_offset, NULL))
			return fail(result, ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_STRING_INVALID,
					    (u32)leaf_index, ORLIX_TCTI_U32_NONE);
		if (!valid_condition(artifact->condition_pool,
				     artifact->condition_pool_size, leaf->condition_offset,
				     leaf->condition_length))
			return fail(result, ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_CONDITION_INVALID,
					    (u32)leaf_index, ORLIX_TCTI_U32_NONE);
		if (leaf->operand_first != expected_operand ||
		    leaf->operand_count > artifact->operand_count - expected_operand ||
		    leaf->fixed_operand_first != expected_fixed_operand ||
		    leaf->fixed_operand_count >
			artifact->fixed_operand_count - expected_fixed_operand)
			return fail(result, ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_SPAN_INVALID,
					    (u32)leaf_index, ORLIX_TCTI_U32_NONE);
		/* The imported source rejects duplicate leaf identities. */
		for (previous = 0; previous < leaf_index; previous++) {
			const struct orlix_tcti_target_instruction_artifact_leaf *prior =
				&artifact->leaves[previous];
			const char *prior_name;

			if (!pool_string(artifact->string_pool,
					 artifact->string_pool_size, prior->name_offset,
					 &prior_name))
				return fail(result,
						ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_STRING_INVALID,
						(u32)previous, ORLIX_TCTI_U32_NONE);
			if (!strcmp(name, prior_name))
				return fail(result, ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_LEAF_INVALID,
						    (u32)leaf_index, ORLIX_TCTI_U32_NONE);
		}
		while (expected_operand < leaf->operand_first + leaf->operand_count) {
			const struct orlix_tcti_target_instruction_artifact_operand *operand =
				&artifact->operands[expected_operand];
			const char *operand_name;
			size_t prior;

			if (operand->leaf_index != leaf_index ||
			    !pool_string(artifact->string_pool,
					 artifact->string_pool_size, operand->name_offset,
					 &operand_name))
				return fail(result,
						ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_OPERAND_INVALID,
						(u32)leaf_index, expected_operand);
			if (!valid_condition(artifact->condition_pool,
					     artifact->condition_pool_size,
					     operand->condition_offset,
					     operand->condition_length))
				return fail(result,
						ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_CONDITION_INVALID,
						(u32)leaf_index, expected_operand);
			if (operand->condition_offset != leaf->condition_offset ||
			    operand->condition_length != leaf->condition_length)
				return fail(result,
						ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_OPERAND_INVALID,
						(u32)leaf_index, expected_operand);
			if (!valid_operand_shape(leaf, operand))
				return fail(result,
						ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_OPERAND_INVALID,
						(u32)leaf_index, expected_operand);
			if (operand_mask & operand->variable_mask)
				return fail(result,
						ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_OPERAND_INVALID,
						(u32)leaf_index, expected_operand);
			for (prior = leaf->operand_first; prior < expected_operand; prior++) {
				const char *prior_name;

				if (!pool_string(artifact->string_pool,
					artifact->string_pool_size,
					artifact->operands[prior].name_offset, &prior_name) ||
				    !strcmp(operand_name, prior_name))
					return fail(result,
						ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_OPERAND_INVALID,
						(u32)leaf_index, expected_operand);
			}
			operand_mask |= operand->variable_mask;
			expected_operand++;
		}
		while (expected_fixed_operand < leaf->fixed_operand_first +
			leaf->fixed_operand_count) {
			const struct orlix_tcti_target_instruction_artifact_fixed_operand *operand =
				&artifact->fixed_operands[expected_fixed_operand];
			const char *fixed_name;
			size_t prior;

			if (operand->leaf_index != leaf_index ||
			    !pool_string(artifact->string_pool, artifact->string_pool_size,
				operand->name_offset, &fixed_name) ||
			    !valid_condition(artifact->condition_pool,
				artifact->condition_pool_size, operand->condition_offset,
				operand->condition_length) ||
			    operand->condition_offset != leaf->condition_offset ||
			    operand->condition_length != leaf->condition_length ||
			    !valid_source_span(operand->source_offset,
				operand->source_length) ||
			    !span_identity_matches(artifact->source_sha256, artifact->string_pool,
				artifact->string_pool_size, operand->source_identity_offset,
				operand->source_offset, operand->source_length) ||
			    !valid_fixed_operand_shape(leaf, operand) ||
			    (fixed_mask & operand->fixed_mask) ||
			    (operand_mask & operand->fixed_mask))
				return fail(result,
					ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_FIXED_OPERAND_INVALID,
					(u32)leaf_index, expected_fixed_operand);
			for (prior = leaf->fixed_operand_first;
			     prior < expected_fixed_operand; prior++) {
				const struct orlix_tcti_target_instruction_artifact_fixed_operand *prior_operand =
					&artifact->fixed_operands[prior];
				const char *prior_name;

				if (!pool_string(artifact->string_pool,
					artifact->string_pool_size,
					prior_operand->name_offset, &prior_name) ||
				    !strcmp(fixed_name, prior_name))
					return fail(result,
						ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_FIXED_OPERAND_INVALID,
						(u32)leaf_index, expected_fixed_operand);
			}
			fixed_mask |= operand->fixed_mask;
			expected_fixed_operand++;
		}
	}
	if (expected_operand != artifact->operand_count)
		return fail(result, ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_SPAN_INVALID,
			    ORLIX_TCTI_U32_NONE, expected_operand);
	if (expected_fixed_operand != artifact->fixed_operand_count)
		return fail(result, ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_SPAN_INVALID,
			    ORLIX_TCTI_U32_NONE, expected_fixed_operand);
	if (validate_aliases(artifact, result))
		return -1;
	if (validate_operational_notes(artifact, result))
		return -1;
	return 0;
}

int orlix_tcti_target_instruction_artifact_validate(
	const struct orlix_tcti_target_instruction_artifact *artifact,
	struct orlix_tcti_target_instruction_artifact_validation_result *result)
{
	return orlix_tcti_target_instruction_artifact_validate_expected(
		artifact, pinned_source_sha256, result);
}

const char *orlix_tcti_target_instruction_artifact_validation_error_name(
	enum orlix_tcti_target_instruction_artifact_validation_error error)
{
	switch (error) {
	case ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_VALID: return "valid";
	case ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_INVALID_ARGUMENT: return "invalid argument";
	case ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_VERSION_MISMATCH: return "version mismatch";
	case ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_PROVENANCE_MISMATCH: return "provenance mismatch";
	case ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_COUNT_MISMATCH: return "count mismatch";
	case ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_POOL_INVALID: return "invalid pool";
	case ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_STRING_INVALID: return "invalid string";
	case ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_CONDITION_INVALID: return "invalid condition";
	case ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_LEAF_INVALID: return "invalid leaf";
	case ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_OPERAND_INVALID: return "invalid operand";
	case ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_FIXED_OPERAND_INVALID:
		return "invalid fixed operand";
	case ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_SPAN_INVALID: return "invalid span";
	case ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_ALIAS_INVALID: return "invalid alias";
	case ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_ALIAS_IDENTITY_INVALID:
		return "invalid alias identity";
	case ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_OPERATIONAL_NOTE_INVALID:
		return "invalid operational note";
	case ORLIX_TCTI_TARGET_INSTRUCTION_ARTIFACT_OPERATIONAL_NOTE_IDENTITY_INVALID:
		return "invalid operational note identity";
	}
	return "unknown validation error";
}
