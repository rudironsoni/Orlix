// SPDX-License-Identifier: GPL-2.0-only
/* Durable native-proof wire contract implementation. */
#ifdef __KERNEL__
#include <linux/overflow.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#else
#include <stdbool.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#endif

#include "target_native_proof_contract_private.h"

#define NATIVE_WIRE_HEADER_BYTES 72U
#define NATIVE_WIRE_ITEM_HEADER_BYTES 16U
#define NATIVE_WIRE_TRAILER_BYTES (ORLIX_TCTI_NATIVE_SHA256_SIZE + 4U)

static bool native_wire_add(size_t *value, size_t add)
{
#ifdef __KERNEL__
	return !check_add_overflow(*value, add, value);
#else
	if (add > SIZE_MAX - *value)
		return false;
	*value += add;
	return true;
#endif
}

static void native_wire_put32(orlix_tcti_proof_u8 *bytes,
			      orlix_tcti_proof_u32 value)
{
	bytes[0] = (orlix_tcti_proof_u8)value;
	bytes[1] = (orlix_tcti_proof_u8)(value >> 8);
	bytes[2] = (orlix_tcti_proof_u8)(value >> 16);
	bytes[3] = (orlix_tcti_proof_u8)(value >> 24);
}

static void native_wire_put64(orlix_tcti_proof_u8 *bytes,
			      orlix_tcti_proof_u64 value)
{
	size_t index;

	for (index = 0; index < 8U; index++)
		bytes[index] = (orlix_tcti_proof_u8)(value >> (index * 8U));
}

static orlix_tcti_proof_u32 native_wire_get32(
	const orlix_tcti_proof_u8 *bytes)
{
	return (orlix_tcti_proof_u32)bytes[0] |
		((orlix_tcti_proof_u32)bytes[1] << 8) |
		((orlix_tcti_proof_u32)bytes[2] << 16) |
		((orlix_tcti_proof_u32)bytes[3] << 24);
}

static orlix_tcti_proof_u64 native_wire_get64(
	const orlix_tcti_proof_u8 *bytes)
{
	orlix_tcti_proof_u64 value = 0;
	size_t index;

	for (index = 0; index < 8U; index++)
		value |= (orlix_tcti_proof_u64)bytes[index] << (index * 8U);
	return value;
}

static void contract_fail(enum orlix_tcti_native_contract_error *error,
			  enum orlix_tcti_native_contract_error value)
{
	if (error)
		*error = value;
}

static int text_complete(const char *text, size_t capacity)
{
	return text && text[0] && memchr(text, '\0', capacity);
}


static int sha256_hex_complete(const char *text)
{
	size_t index;

	if (!text || text[64])
		return 0;
	for (index = 0; index < 64; index++)
		if (!((text[index] >= '0' && text[index] <= '9') ||
		      (text[index] >= 'a' && text[index] <= 'f')))
			return 0;
	return 1;
}

static orlix_tcti_proof_u32 obligation_for_kind(
		orlix_tcti_proof_u32 kind)
{
	switch (kind) {
	case ORLIX_TCTI_NATIVE_OBSERVATION_DECODE:
		return ORLIX_TCTI_TARGET_PROOF_OBLIGATION_DECODE;
	case ORLIX_TCTI_NATIVE_OBSERVATION_LEGAL_ENCODING:
		return ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS;
	case ORLIX_TCTI_NATIVE_OBSERVATION_REJECTED_ENCODING:
		return ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS;
	case ORLIX_TCTI_NATIVE_OBSERVATION_RESULT:
		return ORLIX_TCTI_TARGET_PROOF_OBLIGATION_RESULT;
	case ORLIX_TCTI_NATIVE_OBSERVATION_GPR:
		return ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS;
	case ORLIX_TCTI_NATIVE_OBSERVATION_PC:
		return ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC;
	case ORLIX_TCTI_NATIVE_OBSERVATION_PSTATE:
		return ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS;
	case ORLIX_TCTI_NATIVE_OBSERVATION_MEMORY:
		return ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY;
	case ORLIX_TCTI_NATIVE_OBSERVATION_FAULT:
		return ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FAULTS;
	case ORLIX_TCTI_NATIVE_OBSERVATION_FP_SIMD:
		return ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FP_SIMD;
	case ORLIX_TCTI_NATIVE_OBSERVATION_SVE:
		return ORLIX_TCTI_TARGET_PROOF_OBLIGATION_SVE;
	case ORLIX_TCTI_NATIVE_OBSERVATION_SME_TASK_STATE:
		return ORLIX_TCTI_TARGET_PROOF_OBLIGATION_SME_TASK_STATE;
	case ORLIX_TCTI_NATIVE_OBSERVATION_ATOMICITY:
		return ORLIX_TCTI_TARGET_PROOF_OBLIGATION_ATOMICITY;
	case ORLIX_TCTI_NATIVE_OBSERVATION_ORDERING:
		return ORLIX_TCTI_TARGET_PROOF_OBLIGATION_ORDERING;
	case ORLIX_TCTI_NATIVE_OBSERVATION_ARITHMETIC:
		return ORLIX_TCTI_TARGET_PROOF_OBLIGATION_ARITHMETIC;
	case ORLIX_TCTI_NATIVE_OBSERVATION_SYSTEM_PT_CONTROL:
		return ORLIX_TCTI_TARGET_PROOF_OBLIGATION_SYSTEM_PT_CONTROL;
	case ORLIX_TCTI_NATIVE_OBSERVATION_OPERATIONAL_NOTE:
		return ORLIX_TCTI_TARGET_PROOF_OBLIGATION_OPERATIONAL_NOTE;
	case ORLIX_TCTI_NATIVE_OBSERVATION_LINUX_INTERFACE:
		return ORLIX_TCTI_TARGET_PROOF_OBLIGATION_LINUX_INTERFACE;
	}
	return 0;
}

static int source_complete(const struct orlix_tcti_native_source_identity *source)
{
	const int access_projection_absent = source &&
		!source->access_identity && !source->access_offset &&
		!source->access_length && !source->selector_count &&
		!source->access_expression && !source->condition_expression &&
		!source->concrete_selector && !source->accessor_applicability &&
		!source->accessor_semantics && !source->accessor_implementation &&
		!source->accessor_proof_state;
	const int access_projection_complete = source &&
		source->access_identity && source->access_length &&
		source->selector_count &&
		source->accessor_applicability ==
			ORLIX_TCTI_TARGET_SYSTEM_ACCESSOR_EL0_BEHAVIOR_REQUIRED &&
		source->accessor_semantics >=
			ORLIX_TCTI_TARGET_SYSTEM_ACCESSOR_SOURCE_ACCESS_SEMANTICS &&
		source->accessor_semantics <=
			ORLIX_TCTI_TARGET_SYSTEM_ACCESSOR_GENERIC_LEAF_SEMANTICS &&
		source->accessor_implementation >=
			ORLIX_TCTI_TARGET_SYSTEM_ACCESSOR_IMPLEMENTED &&
		source->accessor_implementation <=
			ORLIX_TCTI_TARGET_SYSTEM_ACCESSOR_UNIMPLEMENTED_REJECTION &&
		source->accessor_proof_state ==
			ORLIX_TCTI_TARGET_SYSTEM_ACCESSOR_PROOF_NOT_OBSERVED;

	if (!source || source->source_ordinal >=
			ORLIX_TCTI_TARGET_LINUX_PROOF_SOURCE_ROWS ||
	    !source->classification_present ||
	    source->classification < ORLIX_TCTI_NATIVE_SOURCE_CLASS_UNCLASSIFIED ||
	    source->classification > ORLIX_TCTI_NATIVE_SOURCE_CLASS_ARCH_UNDEFINED ||
	    !source->source_identity || !source->source_length ||
	    !source->condition_identity || !source->condition_length ||
	    source->encoding_pattern & ~source->encoding_mask ||
	    source->source_branch < ORLIX_TCTI_NATIVE_SOURCE_BRANCH_UNCONDITIONAL ||
	    source->source_branch > ORLIX_TCTI_NATIVE_SOURCE_BRANCH_CASE ||
	    !text_complete(source->leaf_name, sizeof(source->leaf_name)) ||
	    !text_complete(source->mnemonic, sizeof(source->mnemonic)) ||
	    !text_complete(source->operation_id, sizeof(source->operation_id)) ||
	    !text_complete(source->condition_tcnd_hex,
			   sizeof(source->condition_tcnd_hex)) ||
	    !text_complete(source->artifact_architecture,
			   sizeof(source->artifact_architecture)) ||
	    !text_complete(source->artifact_build, sizeof(source->artifact_build)) ||
	    !text_complete(source->artifact_reference,
			   sizeof(source->artifact_reference)) ||
	    !text_complete(source->artifact_schema, sizeof(source->artifact_schema)) ||
	    !sha256_hex_complete(source->artifact_source_sha256))
		return 0;
	if (source->subject_kind == ORLIX_TCTI_NATIVE_SUBJECT_SOURCE_LEAF)
		return !source->semantic_variant_ordinal &&
		       !source->semantic_variant_identity &&
		       !source->variant_direction &&
		       !source->variant_disposition &&
		       !source->semantic_variant[0] &&
		       !source->accessor_kunit_suite[0] &&
		       !source->accessor_kunit_case[0] && access_projection_absent;
	if (source->subject_kind == ORLIX_TCTI_NATIVE_SUBJECT_SEMANTIC_VARIANT)
		return source->semantic_variant_identity &&
		       source->variant_direction &&
		       text_complete(source->semantic_variant,
				     sizeof(source->semantic_variant)) &&
		       text_complete(source->accessor_kunit_suite,
				     sizeof(source->accessor_kunit_suite)) &&
		       text_complete(source->accessor_kunit_case,
				     sizeof(source->accessor_kunit_case)) &&
		       access_projection_complete;
	return 0;
}

static int semantics_complete(
	const struct orlix_tcti_native_semantic_provenance *semantics)
{
	const orlix_tcti_proof_u32 classifications =
		ORLIX_TCTI_TARGET_PROOF_CLASS_REQUIRED_EL0 |
		ORLIX_TCTI_TARGET_PROOF_CLASS_NON_EL0 |
		ORLIX_TCTI_TARGET_PROOF_CLASS_ARCH_UNDEFINED_OR_UNALLOCATED |
		ORLIX_TCTI_TARGET_PROOF_CLASS_ALIAS_OR_DUPLICATE;

	return semantics &&
	       text_complete(semantics->ddi0602_locator,
			     sizeof(semantics->ddi0602_locator)) &&
	       sha256_hex_complete(semantics->ddi0602_sha256) &&
	       semantics->classification_mask &&
	       !(semantics->classification_mask & ~classifications) &&
	       text_complete(semantics->implementation_owner,
			     sizeof(semantics->implementation_owner)) &&
	       text_complete(semantics->decoder_owner,
			     sizeof(semantics->decoder_owner)) &&
	       text_complete(semantics->lowering_owner,
			     sizeof(semantics->lowering_owner));
}


static int text_equal(const char *left, const char *right);

int orlix_tcti_native_source_identity_equal(
	const struct orlix_tcti_native_source_identity *left,
	const struct orlix_tcti_native_source_identity *right)
{
	return left && right &&
		left->subject_kind == right->subject_kind &&
		left->classification == right->classification &&
		left->classification_present == right->classification_present &&
		left->source_ordinal == right->source_ordinal &&
		left->source_index == right->source_index &&
		left->secondary_index == right->secondary_index &&
		left->tertiary_index == right->tertiary_index &&
		left->semantic_variant_ordinal == right->semantic_variant_ordinal &&
		left->variant_direction == right->variant_direction &&
		left->variant_disposition == right->variant_disposition &&
		left->semantic_variant_identity == right->semantic_variant_identity &&
		left->encoding_mask == right->encoding_mask &&
		left->encoding_pattern == right->encoding_pattern &&
		left->access_expression == right->access_expression &&
		left->selector_count == right->selector_count &&
		left->condition_expression == right->condition_expression &&
		left->concrete_selector == right->concrete_selector &&
		left->accessor_applicability == right->accessor_applicability &&
		left->accessor_semantics == right->accessor_semantics &&
		left->accessor_implementation == right->accessor_implementation &&
		left->accessor_proof_state == right->accessor_proof_state &&
		left->source_identity == right->source_identity &&
		left->source_offset == right->source_offset &&
		left->source_length == right->source_length &&
		left->secondary_offset == right->secondary_offset &&
		left->secondary_length == right->secondary_length &&
		left->condition_identity == right->condition_identity &&
		left->condition_offset == right->condition_offset &&
		left->condition_length == right->condition_length &&
		left->access_identity == right->access_identity &&
		left->access_offset == right->access_offset &&
		left->access_length == right->access_length &&
		left->source_branch == right->source_branch &&
		left->source_branch_ordinal == right->source_branch_ordinal &&
		text_equal(left->leaf_name, right->leaf_name) &&
		text_equal(left->mnemonic, right->mnemonic) &&
		text_equal(left->operation_id, right->operation_id) &&
		text_equal(left->semantic_variant, right->semantic_variant) &&
		text_equal(left->condition_tcnd_hex, right->condition_tcnd_hex) &&
		text_equal(left->artifact_architecture,
			   right->artifact_architecture) &&
		text_equal(left->artifact_build, right->artifact_build) &&
		text_equal(left->artifact_reference, right->artifact_reference) &&
		text_equal(left->artifact_schema, right->artifact_schema) &&
		text_equal(left->artifact_source_sha256,
			   right->artifact_source_sha256) &&
		text_equal(left->accessor_kunit_suite,
			   right->accessor_kunit_suite) &&
		text_equal(left->accessor_kunit_case,
			   right->accessor_kunit_case);
}

static int text_equal(const char *left, const char *right)
{
	return !strcmp(left, right);
}

static int registry_entry_complete(
	const struct orlix_tcti_native_proof_registry_entry *entry)
{
	int attribution_complete;

	if (!entry)
		return 0;
	attribution_complete =
	       text_complete(entry->proof_id, sizeof(entry->proof_id)) &&
	       text_complete(entry->kunit_source, sizeof(entry->kunit_source)) &&
	       sha256_hex_complete(entry->kunit_source_sha256) &&
	       text_complete(entry->kunit_build_source,
			     sizeof(entry->kunit_build_source)) &&
	       sha256_hex_complete(entry->kunit_build_source_sha256) &&
	       text_complete(entry->kunit_suite, sizeof(entry->kunit_suite)) &&
	       text_complete(entry->kunit_case, sizeof(entry->kunit_case)) &&
	       sha256_hex_complete(entry->kernel_archive_input_sha256) &&
	       sha256_hex_complete(entry->kernel_config_sha256) &&
	       sha256_hex_complete(entry->build_profile_sha256) &&
	       sha256_hex_complete(entry->durable_source_revision) &&
	       sha256_hex_complete(entry->instruction_artifact_sha256) &&
	       entry->instruction_artifact_identity &&
	       (entry->runtime_profile ==
			ORLIX_TCTI_NATIVE_RUNTIME_PROFILE_DEVELOPMENT ||
		entry->runtime_profile ==
			ORLIX_TCTI_NATIVE_RUNTIME_PROFILE_RELEASE);
	if (!source_complete(&entry->source) ||
	    (entry->static_obligation ?
		(!text_complete(entry->semantics.ddi0602_locator,
			       sizeof(entry->semantics.ddi0602_locator)) ||
		 !sha256_hex_complete(entry->semantics.ddi0602_sha256) ||
		 !entry->source.classification_present) :
		!semantics_complete(&entry->semantics)) ||
	    !sha256_hex_complete(entry->kernel_archive_input_sha256) ||
	    !sha256_hex_complete(entry->kernel_config_sha256) ||
	    !sha256_hex_complete(entry->build_profile_sha256) ||
	    !sha256_hex_complete(entry->durable_source_revision) ||
	    !sha256_hex_complete(entry->instruction_artifact_sha256) ||
	    !entry->instruction_artifact_identity ||
	    (entry->runtime_profile !=
		     ORLIX_TCTI_NATIVE_RUNTIME_PROFILE_DEVELOPMENT &&
	     entry->runtime_profile != ORLIX_TCTI_NATIVE_RUNTIME_PROFILE_RELEASE) ||
	    (entry->production_capture && entry->static_obligation))
		return 0;
	if (entry->static_obligation)
		return !entry->production_capture && !entry->observation_kind &&
		       !entry->obligation && !entry->proof_id[0] &&
		       !entry->kunit_source[0] && !entry->kunit_suite[0] &&
		       !entry->kunit_case[0];
	return attribution_complete &&
	       entry->obligation == obligation_for_kind(entry->observation_kind);
}

static int registry_key_equal(
	const struct orlix_tcti_native_proof_registry_entry *left,
	const struct orlix_tcti_native_proof_registry_entry *right)
{
	return left->source.subject_kind == right->source.subject_kind &&
		left->source.source_ordinal == right->source.source_ordinal &&
		left->source.semantic_variant_ordinal ==
			right->source.semantic_variant_ordinal &&
		left->source.semantic_variant_identity ==
			right->source.semantic_variant_identity &&
		left->source.condition_identity == right->source.condition_identity &&
		left->source.access_identity == right->source.access_identity &&
		left->source.source_branch == right->source.source_branch &&
		left->source.source_branch_ordinal ==
			right->source.source_branch_ordinal &&
		left->obligation == right->obligation &&
		left->static_obligation == right->static_obligation &&
		text_equal(left->proof_id, right->proof_id) &&
		text_equal(left->kunit_suite, right->kunit_suite) &&
		text_equal(left->kunit_case, right->kunit_case);
}

int orlix_tcti_native_proof_registry_validate(
	const struct orlix_tcti_native_proof_registry_entry *entries,
	size_t count, enum orlix_tcti_native_contract_error *error)
{
	size_t left;
	size_t right;

	if (error)
		*error = ORLIX_TCTI_NATIVE_CONTRACT_INVALID;
	if ((!entries && count) || (!count && entries))
		return -1;
	for (left = 0; left < count; left++) {
		if (!registry_entry_complete(&entries[left]))
			return -1;
		for (right = left + 1U; right < count; right++)
			if (registry_key_equal(&entries[left], &entries[right]))
				return -1;
	}
	contract_fail(error, ORLIX_TCTI_NATIVE_CONTRACT_OK);
	return 0;
}

static int key_complete(const struct orlix_tcti_native_proof_key *key)
{
	return key && key->source_ordinal <
		ORLIX_TCTI_TARGET_LINUX_PROOF_SOURCE_ROWS && key->obligation &&
		key->proof_id && key->proof_id[0] && key->kunit_case &&
		key->kunit_case[0];
}

const struct orlix_tcti_native_proof_registry_entry *
orlix_tcti_native_proof_registry_lookup(
	const struct orlix_tcti_native_proof_key *key,
	enum orlix_tcti_native_contract_error *error)
{
	const struct orlix_tcti_native_proof_registry_entry *entries;
	size_t count;
	size_t index;

	if (!key_complete(key)) {
		contract_fail(error, ORLIX_TCTI_NATIVE_CONTRACT_INVALID);
		return NULL;
	}
	entries = orlix_tcti_native_proof_registry_entries(&count);
	if (!entries) {
		contract_fail(error, ORLIX_TCTI_NATIVE_CONTRACT_INVALID);
		return NULL;
	}
	for (index = 0; index < count; index++)
		if (entries[index].source.source_ordinal == key->source_ordinal &&
		    entries[index].source.semantic_variant_identity ==
			key->semantic_variant_identity &&
		    entries[index].obligation == key->obligation &&
		    !strcmp(entries[index].proof_id, key->proof_id) &&
		    !strcmp(entries[index].kunit_case, key->kunit_case)) {
			contract_fail(error, ORLIX_TCTI_NATIVE_CONTRACT_OK);
			return &entries[index];
		}
	contract_fail(error, ORLIX_TCTI_NATIVE_CONTRACT_UNKNOWN);
	return NULL;
}

/* The wire seal is deliberately local: host tests and the kernel use the same
 * byte-for-byte SHA-256 without depending on a host crypto ABI. */
struct native_wire_sha256 {
	orlix_tcti_proof_u32 state[8];
	orlix_tcti_proof_u8 block[64];
	orlix_tcti_proof_u64 bytes;
	size_t used;
};

static const orlix_tcti_proof_u32 native_wire_sha256_k[64] = {
	0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
	0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
	0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
	0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
	0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
	0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
	0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
	0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U,
};

static orlix_tcti_proof_u32 native_wire_ror(orlix_tcti_proof_u32 value,
					     unsigned int shift)
{
	return (value >> shift) | (value << (32U - shift));
}

static void native_wire_sha256_block(struct native_wire_sha256 *ctx,
	const orlix_tcti_proof_u8 *block)
{
	orlix_tcti_proof_u32 words[64];
	orlix_tcti_proof_u32 a, b, c, d, e, f, g, h;
	size_t index;

	for (index = 0; index < 16U; index++)
		words[index] = ((orlix_tcti_proof_u32)block[index * 4U] << 24) |
			((orlix_tcti_proof_u32)block[index * 4U + 1U] << 16) |
			((orlix_tcti_proof_u32)block[index * 4U + 2U] << 8) |
			block[index * 4U + 3U];
	for (; index < 64U; index++) {
		orlix_tcti_proof_u32 s0 = native_wire_ror(words[index - 15U], 7U) ^
			native_wire_ror(words[index - 15U], 18U) ^ (words[index - 15U] >> 3);
		orlix_tcti_proof_u32 s1 = native_wire_ror(words[index - 2U], 17U) ^
			native_wire_ror(words[index - 2U], 19U) ^ (words[index - 2U] >> 10);
		words[index] = words[index - 16U] + s0 + words[index - 7U] + s1;
	}
	a = ctx->state[0]; b = ctx->state[1]; c = ctx->state[2]; d = ctx->state[3];
	e = ctx->state[4]; f = ctx->state[5]; g = ctx->state[6]; h = ctx->state[7];
	for (index = 0; index < 64U; index++) {
		orlix_tcti_proof_u32 s1 = native_wire_ror(e, 6U) ^ native_wire_ror(e, 11U) ^ native_wire_ror(e, 25U);
		orlix_tcti_proof_u32 choose = (e & f) ^ ((~e) & g);
		orlix_tcti_proof_u32 t1 = h + s1 + choose + native_wire_sha256_k[index] + words[index];
		orlix_tcti_proof_u32 s0 = native_wire_ror(a, 2U) ^ native_wire_ror(a, 13U) ^ native_wire_ror(a, 22U);
		orlix_tcti_proof_u32 majority = (a & b) ^ (a & c) ^ (b & c);
		h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + s0 + majority;
	}
	ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
	ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;
}

static void native_wire_sha256(const void *data, size_t length,
			       orlix_tcti_proof_u8 digest[ORLIX_TCTI_NATIVE_SHA256_SIZE])
{
	struct native_wire_sha256 ctx = { .state = { 0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU, 0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U } };
	const orlix_tcti_proof_u8 *cursor = data;
	orlix_tcti_proof_u64 bits = (orlix_tcti_proof_u64)length * 8U;
	size_t index;

	while (length) {
		size_t take = length < 64U - ctx.used ? length : 64U - ctx.used;
		memcpy(ctx.block + ctx.used, cursor, take);
		ctx.used += take; cursor += take; length -= take;
		if (ctx.used == 64U) { native_wire_sha256_block(&ctx, ctx.block); ctx.used = 0; }
	}
	ctx.block[ctx.used++] = 0x80U;
	if (ctx.used > 56U) { memset(ctx.block + ctx.used, 0, 64U - ctx.used); native_wire_sha256_block(&ctx, ctx.block); ctx.used = 0; }
	memset(ctx.block + ctx.used, 0, 56U - ctx.used);
	for (index = 0; index < 8U; index++) ctx.block[56U + index] = (orlix_tcti_proof_u8)(bits >> ((7U - index) * 8U));
	native_wire_sha256_block(&ctx, ctx.block);
	for (index = 0; index < 8U; index++) {
		digest[index * 4U] = (orlix_tcti_proof_u8)(ctx.state[index] >> 24);
		digest[index * 4U + 1U] = (orlix_tcti_proof_u8)(ctx.state[index] >> 16);
		digest[index * 4U + 2U] = (orlix_tcti_proof_u8)(ctx.state[index] >> 8);
		digest[index * 4U + 3U] = (orlix_tcti_proof_u8)ctx.state[index];
	}
}

static orlix_tcti_proof_u64 native_wire_identity(const void *bytes, size_t length)
{
	orlix_tcti_proof_u8 digest[ORLIX_TCTI_NATIVE_SHA256_SIZE];
	orlix_tcti_proof_u64 identity = 0;
	size_t index;

	native_wire_sha256(bytes, length, digest);
	for (index = 0; index < sizeof(identity); index++)
		identity |= (orlix_tcti_proof_u64)digest[index] << (index * 8U);
	return identity ? identity : 1U;
}

static void *native_wire_alloc(size_t bytes);
static void native_wire_free(void *bytes);

static int native_wire_declared_payload_length(
	const struct orlix_tcti_native_capture_selector *selector,
	const orlix_tcti_proof_u8 *payload, size_t item_length,
	size_t *declared_length)
{
	size_t declared = selector->source;
	size_t component;

	if (!selector || !payload || !declared_length || item_length < declared)
		return -1;
	switch (selector->kind) {
	case ORLIX_TCTI_NATIVE_CAPTURE_SELECTOR_MEMORY:
		component = native_wire_get32(payload + 8U);
		if (!native_wire_add(&declared, component))
			return -1;
		break;
	case ORLIX_TCTI_NATIVE_CAPTURE_SELECTOR_FP_SIMD:
		component = native_wire_get32(payload + 16U);
		if (component != 64U * sizeof(orlix_tcti_proof_u64))
			return -1;
		if (!native_wire_add(&declared, component))
			return -1;
		break;
	case ORLIX_TCTI_NATIVE_CAPTURE_SELECTOR_SVE:
		component = native_wire_get32(payload + 4U);
		if (!component || component % 16U)
			return -1;
		if (component > SIZE_MAX / 32U ||
		    native_wire_get32(payload + 8U) != 32U * component ||
		    native_wire_get32(payload + 12U) != 2U * component ||
		    native_wire_get32(payload + 16U) != component / 8U)
			return -1;
		component = native_wire_get32(payload + 8U);
		if (!native_wire_add(&declared, component))
			return -1;
		component = native_wire_get32(payload + 12U);
		if (!native_wire_add(&declared, component))
			return -1;
		component = native_wire_get32(payload + 16U);
		if (!native_wire_add(&declared, component))
			return -1;
		break;
	case ORLIX_TCTI_NATIVE_CAPTURE_SELECTOR_SME:
		if (native_wire_get32(payload) == ORLIX_TCTI_NATIVE_STATE_UNAVAILABLE) {
			if (native_wire_get32(payload + 8U) ||
			    native_wire_get32(payload + 12U) ||
			    native_wire_get32(payload + 16U))
				return -1;
			break;
		}
		component = native_wire_get32(payload + 8U);
		if (!component || component > SIZE_MAX / component ||
		    native_wire_get32(payload + 12U) != component * component ||
		    (native_wire_get32(payload + 16U) &&
		     native_wire_get32(payload + 16U) != component))
			return -1;
		component = native_wire_get32(payload + 12U);
		if (!native_wire_add(&declared, component))
			return -1;
		component = native_wire_get32(payload + 16U);
		if (!native_wire_add(&declared, component))
			return -1;
		break;
	default:
		return -1;
	}
	*declared_length = declared;
	return 0;
}

orlix_tcti_proof_u64 orlix_tcti_native_capture_declaration_identity(
	const struct orlix_tcti_native_capture_declaration *declaration)
{
	orlix_tcti_proof_u8 *bytes;
	size_t length = 64U;
	size_t cursor = 0;
	orlix_tcti_proof_u64 identity;
	size_t index;

	if (!declaration)
		return 0;
	if (declaration->selector_count > (SIZE_MAX - length) / 20U)
		return 0;
	length += declaration->selector_count * 20U;
	bytes = native_wire_alloc(length);
	if (!bytes)
		return 0;
#define NATIVE_DECL_PUT32(value) do { native_wire_put32(bytes + cursor, (value)); cursor += 4U; } while (0)
#define NATIVE_DECL_PUT64(value) do { native_wire_put64(bytes + cursor, (value)); cursor += 8U; } while (0)
	NATIVE_DECL_PUT32(declaration->schema_version);
	NATIVE_DECL_PUT32(declaration->source_ordinal);
	NATIVE_DECL_PUT64(declaration->semantic_variant_identity);
	NATIVE_DECL_PUT32(declaration->selector);
	NATIVE_DECL_PUT32(declaration->direction);
	NATIVE_DECL_PUT64(declaration->condition_identity);
	NATIVE_DECL_PUT32(declaration->obligation);
	NATIVE_DECL_PUT32(declaration->required_record_kind);
	NATIVE_DECL_PUT32(declaration->selector_count);
	for (index = 0; index < declaration->selector_count; index++) {
		const struct orlix_tcti_native_capture_selector *selector =
			&declaration->selectors[index];
		NATIVE_DECL_PUT32(selector->kind);
		NATIVE_DECL_PUT32(selector->selector_id);
		NATIVE_DECL_PUT32(selector->source);
		NATIVE_DECL_PUT32(selector->size_rule);
		NATIVE_DECL_PUT32(selector->flags);
	}
#undef NATIVE_DECL_PUT64
#undef NATIVE_DECL_PUT32
	identity = cursor == length ? native_wire_identity(bytes, length) : 0;
	native_wire_free(bytes);
	return identity;
}

int orlix_tcti_native_capture_declaration_validate(
	const struct orlix_tcti_native_capture_declaration *declaration)
{
	size_t index;

	if (!declaration || declaration->schema_version !=
		ORLIX_TCTI_NATIVE_WIRE_SCHEMA_VERSION || !declaration->source_ordinal ||
		!declaration->obligation || !declaration->required_record_kind ||
		!declaration->selector_count || !declaration->selectors ||
		!declaration->declaration_identity || declaration->declaration_identity !=
		orlix_tcti_native_capture_declaration_identity(declaration))
		return -1;
	for (index = 0; index < declaration->selector_count; index++) {
		const struct orlix_tcti_native_capture_selector *selector =
			&declaration->selectors[index];
		size_t earlier;

		if (!selector->kind || selector->selector_id != index || !selector->source ||
		    selector->size_rule < ORLIX_TCTI_NATIVE_CAPTURE_SIZE_EXACT ||
			selector->size_rule > ORLIX_TCTI_NATIVE_CAPTURE_SIZE_DECLARATION_DERIVED ||
			!(selector->flags & ORLIX_TCTI_NATIVE_CAPTURE_REQUIRED))
			return -1;
		for (earlier = 0; earlier < index; earlier++)
			if (declaration->selectors[earlier].kind == selector->kind)
				return -1;
	}
	return 0;
}

static void *native_wire_alloc(size_t bytes)
{
#ifdef __KERNEL__
	return kvmalloc(bytes, GFP_KERNEL);
#else
	return malloc(bytes);
#endif
}

static void native_wire_free(void *bytes)
{
#ifdef __KERNEL__
	kvfree(bytes);
#else
	free(bytes);
#endif
}

static int native_wire_seal_digest(const orlix_tcti_proof_u8 *bytes,
				   size_t payload_length,
				   orlix_tcti_proof_u8 digest[ORLIX_TCTI_NATIVE_SHA256_SIZE])
{
	orlix_tcti_proof_u8 *sealed;
	size_t sealed_length = payload_length;

	if (!native_wire_add(&sealed_length, 4U))
		return -1;
	sealed = native_wire_alloc(sealed_length);
	if (!sealed)
		return -1;
	memcpy(sealed, bytes, payload_length);
	native_wire_put32(sealed + payload_length, ORLIX_TCTI_NATIVE_WIRE_SEAL_VERSION);
	native_wire_sha256(sealed, sealed_length, digest);
	native_wire_free(sealed);
	return 0;
}

bool orlix_tcti_native_wire_record_has_production_origin(
	const struct orlix_tcti_native_wire_record *record)
{
	/* Wire metadata is descriptive only. Credit requires the server-side slot. */
	return record && record->sealed && !record->consumed;
}

int orlix_tcti_native_wire_record_validate(
	const struct orlix_tcti_native_wire_record *record,
	const struct orlix_tcti_native_capture_declaration *declaration,
	orlix_tcti_proof_u64 registry_identity)
{
	const orlix_tcti_proof_u8 *bytes;
	orlix_tcti_proof_u8 digest[ORLIX_TCTI_NATIVE_SHA256_SIZE];
	size_t cursor = NATIVE_WIRE_HEADER_BYTES;
	size_t payload_end;
	orlix_tcti_proof_u32 count;
	orlix_tcti_proof_u32 index;

	if (!record || !record->sealed || !record->bytes ||
	    orlix_tcti_native_capture_declaration_validate(declaration) ||
	    record->length < NATIVE_WIRE_HEADER_BYTES + NATIVE_WIRE_TRAILER_BYTES)
		return -1;
	bytes = record->bytes;
	if (native_wire_get32(bytes) != ORLIX_TCTI_NATIVE_WIRE_MAGIC ||
	    native_wire_get32(bytes + 4U) != ORLIX_TCTI_NATIVE_WIRE_SCHEMA_VERSION ||
	    native_wire_get64(bytes + 8U) != declaration->declaration_identity ||
	    native_wire_get64(bytes + 16U) != registry_identity ||
	    !native_wire_get64(bytes + 24U) ||
	    native_wire_get32(bytes + 32U) != declaration->obligation ||
	    native_wire_get32(bytes + 36U) != declaration->required_record_kind ||
	    !native_wire_get64(bytes + 56U) || !native_wire_get64(bytes + 64U))
		return -1;
	payload_end = cursor;
	if (!native_wire_add(&payload_end, native_wire_get32(bytes + 48U)) ||
	    payload_end > record->length ||
	    record->length - payload_end != NATIVE_WIRE_TRAILER_BYTES)
		return -1;
	count = native_wire_get32(bytes + 52U);
	if (count != declaration->selector_count)
		return -1;
	for (index = 0; index < count; index++) {
		const struct orlix_tcti_native_capture_selector *selector =
			&declaration->selectors[index];
		size_t item_end = cursor;
		size_t declared_payload;
		orlix_tcti_proof_u32 item_length;

		if (cursor > payload_end || payload_end - cursor < NATIVE_WIRE_ITEM_HEADER_BYTES ||
		    native_wire_get32(bytes + cursor) != selector->kind ||
		    native_wire_get32(bytes + cursor + 4U) != index ||
		    native_wire_get32(bytes + cursor + 8U) != selector->flags)
			return -1;
		item_length = native_wire_get32(bytes + cursor + 12U);
		if (!native_wire_add(&item_end, NATIVE_WIRE_ITEM_HEADER_BYTES) ||
		    !native_wire_add(&item_end, item_length) || item_end > payload_end ||
		    (selector->size_rule == ORLIX_TCTI_NATIVE_CAPTURE_SIZE_EXACT &&
		     item_length != selector->source))
			return -1;
		if (selector->size_rule == ORLIX_TCTI_NATIVE_CAPTURE_SIZE_DECLARATION_DERIVED &&
		    native_wire_declared_payload_length(selector,
				bytes + cursor + NATIVE_WIRE_ITEM_HEADER_BYTES,
				item_length, &declared_payload))
			return -1;
		if (selector->size_rule == ORLIX_TCTI_NATIVE_CAPTURE_SIZE_DECLARATION_DERIVED &&
		    item_length != declared_payload)
			return -1;
		cursor = item_end;
	}
	if (cursor != payload_end ||
	    native_wire_get32(bytes + payload_end + ORLIX_TCTI_NATIVE_SHA256_SIZE) !=
		ORLIX_TCTI_NATIVE_WIRE_SEAL_VERSION)
		return -1;
	if (native_wire_seal_digest(bytes, payload_end, digest))
		return -1;
	if (memcmp(digest, bytes + payload_end, sizeof(digest)) ||
	    memcmp(digest, record->digest, sizeof(digest)) ||
	    record->identity != native_wire_identity(bytes, record->length))
		return -1;
	return 0;
}

int orlix_tcti_native_wire_record_header(
	const struct orlix_tcti_native_wire_record *record,
	struct orlix_tcti_native_wire_header *header)
{
	const orlix_tcti_proof_u8 *bytes;

	if (!record || !header || !record->sealed || !record->bytes ||
	    record->length < NATIVE_WIRE_HEADER_BYTES + NATIVE_WIRE_TRAILER_BYTES)
		return -1;
	bytes = record->bytes;
	if (native_wire_get32(bytes) != ORLIX_TCTI_NATIVE_WIRE_MAGIC ||
	    native_wire_get32(bytes + 4U) != ORLIX_TCTI_NATIVE_WIRE_SCHEMA_VERSION)
		return -1;
	*header = (struct orlix_tcti_native_wire_header) {
		.declaration_identity = native_wire_get64(bytes + 8U),
		.registry_identity = native_wire_get64(bytes + 16U),
		.source_identity = native_wire_get64(bytes + 24U),
		.obligation = native_wire_get32(bytes + 32U),
		.record_kind = native_wire_get32(bytes + 36U),
		.execution_identity = native_wire_get64(bytes + 40U),
		.registry_binding = native_wire_get64(bytes + 56U),
		.finalization_nonce = native_wire_get64(bytes + 64U),
	};
	return header->declaration_identity && header->registry_identity &&
		header->source_identity && header->execution_identity &&
		header->obligation && header->record_kind &&
		header->registry_binding && header->finalization_nonce ? 0 : -1;
}

void orlix_tcti_native_wire_record_destroy(
	struct orlix_tcti_native_wire_record *record)
{
	if (!record)
		return;
	/* Wire records are copyable views.  Their producer owns backing storage and
	 * pending capability retirement, so aliases cannot revoke or free it. */
	memset(record, 0, sizeof(*record));
}
