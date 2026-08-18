// SPDX-License-Identifier: GPL-2.0-only
#include <linux/errno.h>
#include <linux/atomic.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/overflow.h>
#include <linux/random.h>
#include <linux/refcount.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/vmalloc.h>

#include <asm/ptrace.h>
#include <asm/processor.h>

#include "decode_aarch64.h"
#include "engine.h"
#include "native_capture.h"
#include "tests/orlix_tcti_native_observation.h"
#include "tests/target_native_proof_contract_private.h"
#include "tests/target_native_proof_registry_private.h"
#include "tests/target_proof_ingestion.h"
#include "tests/target_proof_ingestion_private.h"

#define NATIVE_WIRE_HEADER_BYTES 72U
#define NATIVE_WIRE_ITEM_HEADER_BYTES 16U
#define NATIVE_WIRE_TRAILER_BYTES (ORLIX_TCTI_NATIVE_SHA256_SIZE + 4U)
#define NATIVE_PENDING_CAPABILITIES 128U

struct native_wire_owner {
	u8 *bytes;
	size_t length;
	refcount_t references;
};

struct orlix_tcti_native_capture_origin {
	u64 registry_identity;
	u64 seal_nonce;
	u64 credential;
	u64 registry_binding;
	bool creditable;
	bool finalized;
};

struct orlix_tcti_native_capture_builder {
	const struct orlix_tcti_native_capture_declaration *declaration;
	struct orlix_tcti_native_capture_origin *origin;
	struct native_wire_owner *owner;
	u8 *bytes;
	size_t length;
	size_t capacity;
	u64 registry_identity;
	u64 source_identity;
	u64 execution_identity;
	u32 item_count;
	u32 next_selector;
	u8 failed;
	u8 sealed;
};

struct native_pending_capability {
	u64 nonce;
	u64 binding;
	u64 registry_identity;
	u64 record_identity;
	struct native_wire_owner *owner;
	bool creditable;
	bool valid;
};

static struct native_pending_capability native_pending[NATIVE_PENDING_CAPABILITIES];
static DEFINE_MUTEX(native_pending_lock);
static atomic64_t native_finalization_nonce = ATOMIC64_INIT(0);

static void native_wire_free(void *bytes);

static void native_wire_owner_put(struct native_wire_owner *owner)
{
	if (!owner || !refcount_dec_and_test(&owner->references))
		return;
	native_wire_free(owner->bytes);
	native_wire_free(owner);
}

static u64 native_wire_next_nonce(void)
{
	return (u64)atomic64_inc_return(&native_finalization_nonce);
}

static bool native_wire_add(size_t *value, size_t add)
{
	return !check_add_overflow(*value, add, value);
}

static void native_wire_put32(u8 *bytes, u32 value)
{
	bytes[0] = (u8)value;
	bytes[1] = (u8)(value >> 8);
	bytes[2] = (u8)(value >> 16);
	bytes[3] = (u8)(value >> 24);
}

static void native_wire_put64(u8 *bytes, u64 value)
{
	size_t index;

	for (index = 0; index < 8U; index++)
		bytes[index] = (u8)(value >> (index * 8U));
}

/* This local codec mirrors the portable verifier's byte-for-byte seal. */
struct native_wire_sha256 {
	u32 state[8];
	u8 block[64];
	u64 bytes;
	size_t used;
};

static const u32 native_wire_sha256_k[64] = {
	0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
	0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
	0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
	0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
	0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
	0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
	0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
	0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U,
};

static u32 native_wire_ror(u32 value, unsigned int shift)
{
	return (value >> shift) | (value << (32U - shift));
}

static void native_wire_sha256_block(struct native_wire_sha256 *ctx,
	const u8 *block)
{
	u32 words[64];
	u32 a, b, c, d, e, f, g, h;
	size_t index;

	for (index = 0; index < 16U; index++)
		words[index] = ((u32)block[index * 4U] << 24) |
			((u32)block[index * 4U + 1U] << 16) |
			((u32)block[index * 4U + 2U] << 8) | block[index * 4U + 3U];
	for (; index < 64U; index++) {
		u32 s0 = native_wire_ror(words[index - 15U], 7U) ^ native_wire_ror(words[index - 15U], 18U) ^ (words[index - 15U] >> 3);
		u32 s1 = native_wire_ror(words[index - 2U], 17U) ^ native_wire_ror(words[index - 2U], 19U) ^ (words[index - 2U] >> 10);
		words[index] = words[index - 16U] + s0 + words[index - 7U] + s1;
	}
	a = ctx->state[0]; b = ctx->state[1]; c = ctx->state[2]; d = ctx->state[3];
	e = ctx->state[4]; f = ctx->state[5]; g = ctx->state[6]; h = ctx->state[7];
	for (index = 0; index < 64U; index++) {
		u32 s1 = native_wire_ror(e, 6U) ^ native_wire_ror(e, 11U) ^ native_wire_ror(e, 25U);
		u32 choose = (e & f) ^ ((~e) & g);
		u32 t1 = h + s1 + choose + native_wire_sha256_k[index] + words[index];
		u32 s0 = native_wire_ror(a, 2U) ^ native_wire_ror(a, 13U) ^ native_wire_ror(a, 22U);
		u32 majority = (a & b) ^ (a & c) ^ (b & c);
		h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + s0 + majority;
	}
	ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
	ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;
}

static void native_wire_sha256(const void *data, size_t length, u8 digest[ORLIX_TCTI_NATIVE_SHA256_SIZE])
{
	struct native_wire_sha256 ctx = { .state = { 0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU, 0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U } };
	const u8 *cursor = data;
	u64 bits = (u64)length * 8U;
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
	for (index = 0; index < 8U; index++) ctx.block[56U + index] = (u8)(bits >> ((7U - index) * 8U));
	native_wire_sha256_block(&ctx, ctx.block);
	for (index = 0; index < 8U; index++) {
		digest[index * 4U] = (u8)(ctx.state[index] >> 24);
		digest[index * 4U + 1U] = (u8)(ctx.state[index] >> 16);
		digest[index * 4U + 2U] = (u8)(ctx.state[index] >> 8);
		digest[index * 4U + 3U] = (u8)ctx.state[index];
	}
}

static u64 native_wire_identity(const void *bytes, size_t length)
{
	u8 digest[ORLIX_TCTI_NATIVE_SHA256_SIZE];
	u64 identity = 0;
	size_t index;

	native_wire_sha256(bytes, length, digest);
	for (index = 0; index < sizeof(identity); index++)
		identity |= (u64)digest[index] << (index * 8U);
	return identity ? identity : 1U;
}

static u64 native_wire_binding(u64 credential, u64 registry_identity, u64 nonce)
{
	u64 material[] = { credential, registry_identity, nonce };

	return native_wire_identity(material, sizeof(material));
}

struct orlix_tcti_result orlix_tcti_resume_user_captured(
	struct task_struct *task, struct pt_regs *regs, struct mm_struct *mm,
	struct orlix_tcti_native_capture *capture);

struct orlix_tcti_native_capture_session {
	struct orlix_tcti_native_capture capture;
	const struct orlix_tcti_native_proof_registry_entry *entry;
	const struct task_struct *task;
	struct orlix_tcti_native_capture_origin origin;
	struct orlix_tcti_native_capture_builder builder;
	struct orlix_tcti_native_wire_record wire;
	struct orlix_tcti_decoded_instruction decoded;
	u64 before_pc;
	u64 before_pstate;
	u64 after_pc;
	u64 after_pstate;
	u64 memory_address;
	u8 *memory_bytes;
	u32 memory_length;
	u32 memory_effect;
	u64 fault_address;
	s32 fault_status;
	u32 fault_access;
	u32 linux_reason;
	s32 linux_status;
	u64 linux_pc;
	bool decoded_seen;
	bool fault_seen;
	bool exit_seen;
	bool resumed;
	bool claimed_by_resume;
	bool failed;
};

static DEFINE_MUTEX(native_capture_resume_lock);
static struct orlix_tcti_native_capture_session *native_capture_resume_pending;

static void *native_wire_alloc(size_t bytes)
{
	return kvmalloc(bytes, GFP_KERNEL);
}

static void native_wire_free(void *bytes)
{
	kvfree(bytes);
}

static int native_wire_seal_digest(const u8 *bytes, size_t payload_length,
	u8 digest[ORLIX_TCTI_NATIVE_SHA256_SIZE])
{
	u8 *sealed;
	size_t sealed_length = payload_length;

	if (!native_wire_add(&sealed_length, 4U))
		return -EINVAL;
	sealed = native_wire_alloc(sealed_length);
	if (!sealed)
		return -ENOMEM;
	memcpy(sealed, bytes, payload_length);
	native_wire_put32(sealed + payload_length, ORLIX_TCTI_NATIVE_WIRE_SEAL_VERSION);
	native_wire_sha256(sealed, sealed_length, digest);
	native_wire_free(sealed);
	return 0;
}

static int native_wire_reserve(struct orlix_tcti_native_capture_builder *builder,
	size_t needed)
{
	u8 *replacement;

	if (needed <= builder->capacity)
		return 0;
	replacement = native_wire_alloc(needed);
	if (!replacement)
		return -ENOMEM;
	memcpy(replacement, builder->bytes, builder->length);
	native_wire_free(builder->bytes);
	builder->bytes = replacement;
	builder->capacity = needed;
	return 0;
}

static bool native_pending_publish_take_ownership(
	const struct orlix_tcti_native_capture_origin *origin, u64 record_identity,
	struct native_wire_owner *owner)
{
	size_t index;

	if (!origin || !origin->seal_nonce || !owner)
		return false;
	if (!origin->creditable)
		return false;
	mutex_lock(&native_pending_lock);
	for (index = 0; index < NATIVE_PENDING_CAPABILITIES; index++)
		if (!native_pending[index].valid)
			break;
	/* A pending capability remains live until its own ingest transaction
	 * consumes it. Exhaustion must leave every extant slot and backing owner
	 * untouched, rather than revoking an ordinary producer's live record. */
	if (index == NATIVE_PENDING_CAPABILITIES) {
		mutex_unlock(&native_pending_lock);
		return false;
	}
	native_pending[index] = (struct native_pending_capability) {
		.nonce = origin->seal_nonce,
		.binding = origin->registry_binding,
		.registry_identity = origin->registry_identity,
		.record_identity = record_identity,
		.owner = owner,
		.creditable = origin->creditable,
		.valid = true,
	};
	mutex_unlock(&native_pending_lock);
	return true;
}

static int native_capture_origin_begin(
	struct orlix_tcti_native_capture_origin *origin, u64 registry_identity)
{
	if (!origin || !registry_identity)
		return -EINVAL;
	*origin = (struct orlix_tcti_native_capture_origin) {
		.registry_identity = registry_identity,
		.seal_nonce = native_wire_next_nonce(),
		.credential = get_random_u64(),
	};
	if (!origin->credential)
		origin->credential = 1U;
	origin->registry_binding = native_wire_binding(origin->credential,
		registry_identity, origin->seal_nonce);
	return 0;
}

static void native_capture_origin_finalize(
	struct orlix_tcti_native_capture_origin *origin)
{
	if (origin)
		origin->finalized = true;
}

static int native_capture_builder_begin(
	struct orlix_tcti_native_capture_builder *builder,
	struct orlix_tcti_native_capture_origin *origin,
	const struct orlix_tcti_native_capture_declaration *declaration,
	u64 registry_identity, u64 source_identity, u64 execution_identity)
{
	if (!builder || !origin || !origin->credential || !origin->registry_binding ||
	    origin->registry_identity != registry_identity ||
	    orlix_tcti_native_capture_declaration_validate(declaration) ||
	    !registry_identity || !source_identity || !execution_identity)
		return -EINVAL;
	memset(builder, 0, sizeof(*builder));
	builder->bytes = native_wire_alloc(NATIVE_WIRE_HEADER_BYTES);
	if (!builder->bytes)
		return -ENOMEM;
	builder->capacity = builder->length = NATIVE_WIRE_HEADER_BYTES;
	builder->declaration = declaration;
	builder->origin = origin;
	builder->registry_identity = registry_identity;
	builder->source_identity = source_identity;
	builder->execution_identity = execution_identity;
	native_wire_put32(builder->bytes, ORLIX_TCTI_NATIVE_WIRE_MAGIC);
	native_wire_put32(builder->bytes + 4U, ORLIX_TCTI_NATIVE_WIRE_SCHEMA_VERSION);
	native_wire_put64(builder->bytes + 8U, declaration->declaration_identity);
	native_wire_put64(builder->bytes + 16U, registry_identity);
	native_wire_put64(builder->bytes + 24U, source_identity);
	native_wire_put32(builder->bytes + 32U, declaration->obligation);
	native_wire_put32(builder->bytes + 36U, declaration->required_record_kind);
	native_wire_put64(builder->bytes + 40U, execution_identity);
	native_wire_put64(builder->bytes + 56U, origin->registry_binding);
	native_wire_put64(builder->bytes + 64U, origin->seal_nonce);
	return 0;
}

static int native_capture_builder_append(
	struct orlix_tcti_native_capture_builder *builder, u32 kind, u32 selector_id,
	u32 flags, const void *payload, size_t payload_bytes)
{
	const struct orlix_tcti_native_capture_selector *selector;
	size_t needed;

	if (!builder || builder->failed || builder->sealed || !payload ||
	    !builder->declaration || selector_id != builder->next_selector ||
	    selector_id >= builder->declaration->selector_count || payload_bytes > U32_MAX)
		goto fail;
	selector = &builder->declaration->selectors[selector_id];
	if (kind != selector->kind || flags != selector->flags)
		goto fail;
	needed = builder->length;
	if (!native_wire_add(&needed, NATIVE_WIRE_ITEM_HEADER_BYTES) ||
	    !native_wire_add(&needed, payload_bytes) || native_wire_reserve(builder, needed))
		goto fail;
	native_wire_put32(builder->bytes + builder->length, kind);
	native_wire_put32(builder->bytes + builder->length + 4U, selector_id);
	native_wire_put32(builder->bytes + builder->length + 8U, flags);
	native_wire_put32(builder->bytes + builder->length + 12U, payload_bytes);
	memcpy(builder->bytes + builder->length + NATIVE_WIRE_ITEM_HEADER_BYTES,
	       payload, payload_bytes);
	builder->length = needed;
	builder->item_count++;
	builder->next_selector++;
	return 0;
fail:
	if (builder)
		builder->failed = 1U;
	return -EINVAL;
}

static int native_capture_builder_seal(
	struct orlix_tcti_native_capture_builder *builder,
	struct orlix_tcti_native_wire_record *record)
{
	size_t sealed_length;
	u8 digest[ORLIX_TCTI_NATIVE_SHA256_SIZE];

	if (!builder || !record || builder->failed || builder->sealed ||
	    !builder->origin || !builder->origin->finalized || !builder->declaration ||
	    builder->item_count != builder->declaration->selector_count)
		return -EINVAL;
	sealed_length = builder->length;
	if (!native_wire_add(&sealed_length, NATIVE_WIRE_TRAILER_BYTES) ||
	    native_wire_reserve(builder, sealed_length))
		return -ENOMEM;
	native_wire_put32(builder->bytes + 48U,
		builder->length - NATIVE_WIRE_HEADER_BYTES);
	native_wire_put32(builder->bytes + 52U, builder->item_count);
	native_wire_put32(builder->bytes + builder->length +
		ORLIX_TCTI_NATIVE_SHA256_SIZE, ORLIX_TCTI_NATIVE_WIRE_SEAL_VERSION);
	if (native_wire_seal_digest(builder->bytes, builder->length, digest))
		return -EINVAL;
	memcpy(builder->bytes + builder->length, digest, sizeof(digest));
	memset(record, 0, sizeof(*record));
	record->bytes = builder->bytes;
	record->length = sealed_length;
	memcpy(record->digest, digest, sizeof(digest));
	record->identity = native_wire_identity(record->bytes, record->length);
	record->sealed = 1U;
	record->production_origin = 0U;
	builder->owner = native_wire_alloc(sizeof(*builder->owner));
	if (!builder->owner)
		return -ENOMEM;
	builder->owner->bytes = builder->bytes;
	builder->owner->length = sealed_length;
	refcount_set(&builder->owner->references, 1U);
	if (builder->origin->creditable) {
		record->producer_nonce = builder->origin->seal_nonce;
		record->producer_binding = builder->origin->registry_binding;
		record->production_origin = 1U;
		if (!native_pending_publish_take_ownership(builder->origin,
			record->identity, builder->owner)) {
			/* Publication failed before touching the pending table. Do not
			 * return a dangling or uncreditable sealed view to the caller. */
			memset(record, 0, sizeof(*record));
			builder->bytes = NULL;
			builder->capacity = builder->length = 0;
			native_wire_owner_put(builder->owner);
			builder->owner = NULL;
			builder->sealed = 1U;
			return -ENOSPC;
		} else {
			builder->owner = NULL;
		}
	}
	builder->bytes = NULL;
	builder->capacity = builder->length = 0;
	builder->sealed = 1U;
	return 0;
}

static void native_capture_builder_discard(
	struct orlix_tcti_native_capture_builder *builder)
{
	if (!builder)
		return;
	native_wire_free(builder->bytes);
	/* A pending slot owns successful backing. An uncreditable producer owns
	 * its sealed backing until session teardown. */
	native_wire_owner_put(builder->owner);
	builder->owner = NULL;
	memset(builder, 0, sizeof(*builder));
}

struct native_pending_reservation {
	struct native_pending_capability *slot;
	struct native_wire_owner *owner;
};

struct native_ingest_plan {
	struct orlix_tcti_target_proof_ingestion_slot slot;
};

static void native_ingest_reject(
	enum orlix_tcti_target_proof_ingestion_error value,
	enum orlix_tcti_target_proof_ingestion_error *error)
{
	if (error)
		*error = value;
}

static bool native_ingest_copy_text(char *destination, size_t capacity,
	const char *source)
{
	size_t length;

	if (!destination || !capacity || !source || !source[0])
		return false;
	length = strlen(source);
	if (length >= capacity)
		return false;
	memcpy(destination, source, length + 1U);
	return true;
}

static const struct orlix_tcti_native_proof_registry_entry *
native_entry_from_wire(const struct orlix_tcti_native_wire_header *header)
{
	const struct orlix_tcti_native_proof_registry_entry *entries;
	const struct orlix_tcti_native_proof_registry_entry *found = NULL;
	size_t count;
	size_t index;

	entries = orlix_tcti_native_proof_registry_entries(&count);
	for (index = 0; entries && index < count; index++) {
		const struct orlix_tcti_native_proof_registry_entry *entry =
			&entries[index];

		if (!entry->production_capture || !entry->capture_declaration ||
		    orlix_tcti_native_proof_registry_entry_identity(entry) !=
				header->registry_identity)
			continue;
		if (found)
			return NULL;
		found = entry;
	}
	return found;
}

static bool native_entry_authorizes_wire(
	const struct orlix_tcti_native_proof_registry_entry *entry,
	const struct orlix_tcti_native_wire_header *header,
	const struct orlix_tcti_native_wire_record *record)
{
	const struct orlix_tcti_native_capture_declaration *declaration;

	if (!entry || !header || !record || !(declaration = entry->capture_declaration) ||
	    header->declaration_identity != declaration->declaration_identity ||
	    header->source_identity != entry->source.source_identity ||
	    header->execution_identity !=
		orlix_tcti_native_proof_registry_entry_identity(entry) ||
	    header->obligation != entry->obligation ||
	    header->record_kind != entry->observation_kind ||
	    declaration->source_ordinal != entry->source.source_ordinal ||
	    declaration->semantic_variant_identity !=
		entry->source.semantic_variant_identity ||
	    declaration->selector != entry->source.concrete_selector ||
	    declaration->direction != entry->source.variant_direction ||
	    declaration->condition_identity != entry->source.condition_identity ||
	    declaration->obligation != entry->obligation ||
	    declaration->required_record_kind != entry->observation_kind)
		return false;
	return !orlix_tcti_native_wire_record_validate(record, declaration,
		orlix_tcti_native_proof_registry_entry_identity(entry));
}

static struct native_ingest_plan *native_ingest_plan_prepare(
	const struct orlix_tcti_native_proof_registry_entry *entry,
	const struct orlix_tcti_native_wire_record *record)
{
	struct native_ingest_plan *plan;

	plan = kzalloc(sizeof(*plan), GFP_KERNEL);
	if (!plan)
		return NULL;
	plan->slot = (struct orlix_tcti_target_proof_ingestion_slot) {
		.identity = record->identity,
		.registry_identity = orlix_tcti_native_proof_registry_entry_identity(entry),
		.source_ordinal = entry->source.source_ordinal,
		.semantic_variant_identity = entry->source.semantic_variant_identity,
		.obligation = entry->obligation,
		.native = true,
	};
	if (!native_ingest_copy_text(plan->slot.proof_id, sizeof(plan->slot.proof_id),
		entry->proof_id) ||
	    !native_ingest_copy_text(plan->slot.kunit_suite,
		sizeof(plan->slot.kunit_suite), entry->kunit_suite) ||
	    !native_ingest_copy_text(plan->slot.kunit_case,
		sizeof(plan->slot.kunit_case), entry->kunit_case)) {
		kfree(plan);
		return NULL;
	}
	return plan;
}

static bool native_slot_replays(
	const struct orlix_tcti_target_proof_ingestion_slot *slot,
	const struct orlix_tcti_target_proof_ingestion_slot *candidate)
{
	return slot->native &&
		(slot->identity == candidate->identity ||
		 (slot->registry_identity == candidate->registry_identity &&
		  slot->source_ordinal == candidate->source_ordinal &&
		  slot->semantic_variant_identity == candidate->semantic_variant_identity &&
		  slot->obligation == candidate->obligation &&
		  !strcmp(slot->proof_id, candidate->proof_id) &&
		  !strcmp(slot->kunit_suite, candidate->kunit_suite) &&
		  !strcmp(slot->kunit_case, candidate->kunit_case)));
}

static bool native_pending_reserve_handle(
	const struct orlix_tcti_native_wire_record *record,
	struct native_pending_reservation *reservation)
{
	struct native_pending_capability *pending = NULL;
	size_t index;

	if (!record || !record->producer_nonce || !record->producer_binding ||
	    !reservation)
		return false;
	reservation->slot = NULL;
	reservation->owner = NULL;
	mutex_lock(&native_pending_lock);
	for (index = 0; index < NATIVE_PENDING_CAPABILITIES; index++)
		if (native_pending[index].valid &&
		    native_pending[index].nonce == record->producer_nonce) {
			pending = &native_pending[index];
			break;
	}
	if (!pending || record->consumed || !pending->creditable ||
	    pending->binding != record->producer_binding ||
	    pending->record_identity != record->identity || !pending->owner ||
	    pending->owner->bytes != record->bytes ||
	    pending->owner->length != record->length) {
		mutex_unlock(&native_pending_lock);
		return false;
	}
	refcount_inc(&pending->owner->references);
	reservation->owner = pending->owner;
	mutex_unlock(&native_pending_lock);
	return true;
}

static bool native_pending_lock_reservation(
	const struct orlix_tcti_native_wire_record *record,
	const struct orlix_tcti_native_wire_header *header, u64 registry_identity,
	struct native_pending_reservation *reservation)
{
	struct native_pending_capability *pending = NULL;
	size_t index;

	if (!record || !header || !reservation || !reservation->owner)
		return false;
	mutex_lock(&native_pending_lock);
	for (index = 0; index < NATIVE_PENDING_CAPABILITIES; index++)
		if (native_pending[index].valid &&
		    native_pending[index].nonce == record->producer_nonce) {
			pending = &native_pending[index];
			break;
	}
	if (!pending || pending->owner != reservation->owner || record->consumed ||
	    !pending->creditable || pending->binding != header->registry_binding ||
	    pending->registry_identity != registry_identity ||
	    pending->record_identity != record->identity ||
	    header->finalization_nonce != record->producer_nonce ||
	    header->registry_binding != record->producer_binding) {
		mutex_unlock(&native_pending_lock);
		return false;
	}
	reservation->slot = pending;
	return true;
}

static void native_pending_release_reservation(
	struct native_pending_reservation *reservation)
{
	if (!reservation)
		return;
	if (reservation->slot) {
		reservation->slot = NULL;
		mutex_unlock(&native_pending_lock);
	}
	native_wire_owner_put(reservation->owner);
	reservation->owner = NULL;
}

static void native_pending_consume_commit(
	struct native_pending_reservation *reservation,
	struct orlix_tcti_native_wire_record *record)
{
	struct native_pending_capability *slot;
	struct native_wire_owner *owner;

	if (!reservation || !(slot = reservation->slot))
		return;
	owner = slot->owner;
	memset(slot, 0, sizeof(*slot));
	record->consumed = 1U;
	reservation->slot = NULL;
	mutex_unlock(&native_pending_lock);
	native_wire_owner_put(owner);
	native_wire_owner_put(reservation->owner);
	reservation->owner = NULL;
}

int orlix_tcti_target_proof_ingest_native(
	struct orlix_tcti_target_proof_ingestion_ledger *ledger,
	struct orlix_tcti_native_wire_record *record,
	enum orlix_tcti_target_proof_ingestion_error *error)
{
	struct orlix_tcti_native_wire_header header;
	const struct orlix_tcti_native_proof_registry_entry *entry;
	struct native_ingest_plan *plan = NULL;
	struct native_pending_reservation reservation = {};
	size_t index;
	bool ledger_locked = false;
	int result = -1;

	if (error)
		*error = ORLIX_TCTI_TARGET_PROOF_INGEST_INVALID;
	if (!record || !orlix_tcti_native_wire_record_has_production_origin(record) ||
	    !ledger || !ledger->slots || !ledger->capacity ||
	    !native_pending_reserve_handle(record, &reservation) ||
	    orlix_tcti_native_wire_record_header(record, &header))
		goto out;
	entry = native_entry_from_wire(&header);
	if (!entry) {
		native_ingest_reject(ORLIX_TCTI_TARGET_PROOF_INGEST_UNKNOWN, error);
		goto out;
	}
	if (!native_entry_authorizes_wire(entry, &header, record)) {
		native_ingest_reject(ORLIX_TCTI_TARGET_PROOF_INGEST_TYPE_SUBSTITUTION,
			error);
		goto out;
	}
	plan = native_ingest_plan_prepare(entry, record);
	if (!plan)
		goto out;
	if (!native_pending_lock_reservation(record, &header,
		orlix_tcti_native_proof_registry_entry_identity(entry), &reservation)) {
		native_ingest_reject(ORLIX_TCTI_TARGET_PROOF_INGEST_NON_PRODUCTION,
			error);
		goto out;
	}
	mutex_lock(&ledger->lock);
	ledger_locked = true;
	if (ledger->count == ledger->capacity) {
		native_ingest_reject(ORLIX_TCTI_TARGET_PROOF_INGEST_INVALID, error);
		goto out;
	}
	for (index = 0; index < ledger->count; index++)
		if (native_slot_replays(&ledger->slots[index], &plan->slot)) {
			native_ingest_reject(ORLIX_TCTI_TARGET_PROOF_INGEST_REPLAY, error);
			goto out;
		}
	ledger->slots[ledger->count++] = plan->slot;
	ledger->native_passed++;
	native_pending_consume_commit(&reservation, record);
	if (error)
		*error = ORLIX_TCTI_TARGET_PROOF_INGEST_OK;
	result = 0;
out:
	if (ledger_locked)
		mutex_unlock(&ledger->lock);
	native_pending_release_reservation(&reservation);
	kfree(plan);
	return result;
}

static int native_capture_append(struct orlix_tcti_native_capture_session *session,
				 size_t selector_id, const void *payload,
				 size_t payload_bytes);

#define NATIVE_CAPTURE_FP_SIMD_HEADER_BYTES 24U
#define NATIVE_CAPTURE_SVE_HEADER_BYTES 24U
#define NATIVE_CAPTURE_SME_HEADER_BYTES 24U
#define NATIVE_CAPTURE_SYSTEM_CONTROL_BYTES \
	ORLIX_TCTI_NATIVE_CAPTURE_SYSTEM_CONTROL_BYTES

static void native_capture_put32(u8 *bytes, size_t offset, u32 value)
{
	bytes[offset] = (u8)value;
	bytes[offset + 1U] = (u8)(value >> 8);
	bytes[offset + 2U] = (u8)(value >> 16);
	bytes[offset + 3U] = (u8)(value >> 24);
}

static void native_capture_put64(u8 *bytes, size_t offset, u64 value)
{
	size_t index;

	for (index = 0; index < 8U; index++)
		bytes[offset + index] = (u8)(value >> (index * 8U));
}

static int native_capture_length_add(size_t *length, size_t add)
{
	return check_add_overflow(*length, add, length) ? -EOVERFLOW : 0;
}

static int native_capture_length_mul(size_t *length, size_t left, size_t right)
{
	return check_mul_overflow(left, right, length) ? -EOVERFLOW : 0;
}

static int native_capture_fp_simd(struct orlix_tcti_native_capture_session *session,
				  const struct task_struct *task)
{
	u8 payload[NATIVE_CAPTURE_FP_SIMD_HEADER_BYTES + 64U * sizeof(u64)] = {};
	size_t index;

	if (!session || !task)
		return -EINVAL;
	/* FPCR/FPSR and every V register are architected task state. */
	native_capture_put64(payload, 0U, task->thread.user_fpcr);
	native_capture_put64(payload, 8U, task->thread.user_fpsr);
	native_capture_put32(payload, 16U, task->thread.user_simd_valid);
	native_capture_put32(payload, 20U, 64U * sizeof(u64));
	for (index = 0; index < ARRAY_SIZE(task->thread.user_simd); index++)
		native_capture_put64(payload, NATIVE_CAPTURE_FP_SIMD_HEADER_BYTES +
			index * sizeof(u64), task->thread.user_simd[index]);
	return native_capture_append(session, 2U, payload, sizeof(payload));
}

static int native_capture_sve(struct orlix_tcti_native_capture_session *session,
			      const struct task_struct *task)
{
	const struct orlix_tcti_sve_state *state;
	u8 *payload;
	size_t z_bytes;
	size_t p_bytes;
	size_t ffr_bytes;
	size_t payload_bytes = NATIVE_CAPTURE_SVE_HEADER_BYTES;
	size_t cursor = NATIVE_CAPTURE_SVE_HEADER_BYTES;
	size_t index;
	int ret;

	if (!session || !task)
		return -EINVAL;
	state = &task->thread.user_sve;
	if (!state->valid || state->vl_bytes < ORLIX_TCTI_SVE_MIN_VL_BYTES ||
	    state->vl_bytes > ORLIX_TCTI_SVE_MAX_VL_BYTES ||
	    state->vl_bytes % ORLIX_TCTI_SVE_MIN_VL_BYTES)
		return -EINVAL;
	if (native_capture_length_mul(&z_bytes, ORLIX_TCTI_SVE_ZREG_COUNT,
				      state->vl_bytes) ||
	    native_capture_length_mul(&p_bytes, ORLIX_TCTI_SVE_PREG_COUNT,
				      state->vl_bytes / 8U) ||
	    native_capture_length_mul(&ffr_bytes, 1U, state->vl_bytes / 8U) ||
	    native_capture_length_add(&payload_bytes, z_bytes) ||
	    native_capture_length_add(&payload_bytes, p_bytes) ||
	    native_capture_length_add(&payload_bytes, ffr_bytes) ||
	    payload_bytes > U32_MAX)
		return -EOVERFLOW;
	payload = kvzalloc(payload_bytes, GFP_KERNEL);
	if (!payload)
		return -ENOMEM;
	native_capture_put32(payload, 0U, state->valid);
	native_capture_put32(payload, 4U, state->vl_bytes);
	native_capture_put32(payload, 8U, z_bytes);
	native_capture_put32(payload, 12U, p_bytes);
	native_capture_put32(payload, 16U, ffr_bytes);
	/* The final field binds the architectural Z/P/FFR register counts. */
	native_capture_put32(payload, 20U, ORLIX_TCTI_SVE_ZREG_COUNT |
			(ORLIX_TCTI_SVE_PREG_COUNT << 16));
	for (index = 0; index < ORLIX_TCTI_SVE_ZREG_COUNT; index++) {
		size_t byte;

		for (byte = 0; byte < state->vl_bytes; byte++)
			payload[cursor++] = state->z[index][byte];
	}
	for (index = 0; index < ORLIX_TCTI_SVE_PREG_COUNT; index++) {
		size_t byte;

		for (byte = 0; byte < state->vl_bytes / 8U; byte++)
			payload[cursor++] = state->p[index][byte];
	}
	for (index = 0; index < state->vl_bytes / 8U; index++)
		payload[cursor++] = state->ffr[index];
	if (cursor != payload_bytes) {
		kvfree(payload);
		return -EINVAL;
	}
	ret = native_capture_append(session, 3U, payload, payload_bytes);
	kvfree(payload);
	return ret;
}

static int native_capture_sme(struct orlix_tcti_native_capture_session *session,
			      const struct task_struct *task)
{
	const struct orlix_tcti_sme_state *state;
	u8 unavailable[NATIVE_CAPTURE_SME_HEADER_BYTES] = {};
	u8 *payload;
	size_t payload_bytes = NATIVE_CAPTURE_SME_HEADER_BYTES;
	size_t expected_za_bytes;
	size_t cursor = NATIVE_CAPTURE_SME_HEADER_BYTES;
	size_t index;
	u32 flags = 0;
	int ret;

	if (!session || !task)
		return -EINVAL;
	state = &task->thread.user_sme;
	if (!state->valid) {
		/* Fail-closed HWCAP has no synthetic SME state to report. */
		native_capture_put32(unavailable, 0U,
			ORLIX_TCTI_NATIVE_STATE_UNAVAILABLE);
		return native_capture_append(session, 4U, unavailable,
			sizeof(unavailable));
	}
	if (!state->svl_bytes || state->svl_bytes % ORLIX_TCTI_SVE_MIN_VL_BYTES ||
	    check_mul_overflow((size_t)state->svl_bytes, (size_t)state->svl_bytes,
				       &expected_za_bytes) ||
	    (!state->za_enabled && state->za_bytes) ||
	    (state->za_enabled && (!state->za || state->za_bytes != expected_za_bytes)) ||
	    (!state->zt0_valid && state->zt0_bytes) ||
	    (state->zt0_valid && (!state->zt0 ||
		state->zt0_bytes != state->svl_bytes)) ||
	    native_capture_length_add(&payload_bytes, state->za_bytes) ||
	    native_capture_length_add(&payload_bytes, state->zt0_bytes) ||
	    payload_bytes > U32_MAX)
		return -EINVAL;
	payload = kvzalloc(payload_bytes, GFP_KERNEL);
	if (!payload)
		return -ENOMEM;
	if (state->streaming_mode)
		flags |= BIT(0);
	if (state->za_enabled)
		flags |= BIT(1);
	if (state->zt0_valid)
		flags |= BIT(2);
	native_capture_put32(payload, 0U, ORLIX_TCTI_NATIVE_STATE_CAPTURED);
	native_capture_put32(payload, 4U, flags);
	native_capture_put32(payload, 8U, state->svl_bytes);
	native_capture_put32(payload, 12U, state->za_bytes);
	native_capture_put32(payload, 16U, state->zt0_bytes);
	for (index = 0; index < state->za_bytes; index++)
		payload[cursor++] = state->za[index];
	for (index = 0; index < state->zt0_bytes; index++)
		payload[cursor++] = state->zt0[index];
	if (cursor != payload_bytes) {
		kvfree(payload);
		return -EINVAL;
	}
	ret = native_capture_append(session, 4U, payload, payload_bytes);
	kvfree(payload);
	return ret;
}

static int native_capture_append(struct orlix_tcti_native_capture_session *session,
				 size_t selector_id, const void *payload,
				 size_t payload_bytes)
{
	const struct orlix_tcti_native_capture_selector *selector;

	if (!session || !session->entry || !session->entry->capture_declaration ||
	    selector_id >= session->entry->capture_declaration->selector_count)
		return -EINVAL;
	selector = &session->entry->capture_declaration->selectors[selector_id];
	return native_capture_builder_append(&session->builder,
		selector->kind, selector->selector_id, selector->flags, payload,
		payload_bytes);
}

static bool native_capture_is_memory(const struct orlix_tcti_decoded_instruction *decoded)
{
	return decoded->decode_class == ORLIX_TCTI_DECODE_LOAD_LITERAL ||
		(decoded->decode_class >= ORLIX_TCTI_DECODE_LOAD_STORE_PAIR &&
		 decoded->decode_class <= ORLIX_TCTI_DECODE_LOAD_STORE_REGISTER_OFFSET) ||
		decoded->decode_class == ORLIX_TCTI_DECODE_LOAD_STORE_EXCLUSIVE ||
		decoded->decode_class == ORLIX_TCTI_DECODE_LSE_ATOMIC;
}

static u64 native_capture_memory_address(
	const struct orlix_tcti_decoded_instruction *decoded,
	const struct pt_regs *regs)
{
	u64 base;

	/* Load-literal has no Rn. The access address is PC-relative. */
	if (decoded->decode_class == ORLIX_TCTI_DECODE_LOAD_LITERAL)
		return regs->pc + decoded->memory_offset;
	base = decoded->rn == 31U ? regs->sp : regs->regs[decoded->rn];
	if (decoded->memory_index_mode == ORLIX_TCTI_MEMORY_INDEX_POST)
		return base;
	return base + decoded->memory_offset;
}

enum native_capture_relevance {
	NATIVE_CAPTURE_IRRELEVANT = 0,
	NATIVE_CAPTURE_MATCH,
	NATIVE_CAPTURE_VARIANT_CONFLICT,
};

static enum native_capture_relevance native_capture_classify_entry(
	const struct orlix_tcti_native_capture_session *session,
	const struct orlix_tcti_decoded_instruction *decoded)
{
	const struct orlix_tcti_native_proof_registry_entry *entry;

	if (!session || !decoded || !(entry = session->entry))
		return NATIVE_CAPTURE_IRRELEVANT;
	/*
	 * SystemAccessor rows share the MRS/MSR encoding space. Any decoded
	 * system-register neighbour of a variant session is a producer
	 * conflict, even when the stored mask is tighter than the generic MRS
	 * pattern.
	 */
	if (entry->source.subject_kind == ORLIX_TCTI_NATIVE_SUBJECT_SEMANTIC_VARIANT) {
		if (decoded->decode_class != ORLIX_TCTI_DECODE_SYSTEM_REGISTER)
			goto encoding;
		if (decoded->system_accessor_id != entry->source.secondary_index ||
		    decoded->system_accessor_selector != entry->source.concrete_selector ||
		    decoded->system_accessor_condition !=
			entry->source.condition_expression ||
		    decoded->system_accessor_access != entry->source.access_expression ||
		    decoded->system_accessor_disposition !=
			entry->source.variant_disposition ||
		    decoded->system_accessor_implementation !=
			entry->source.accessor_implementation ||
		    decoded->system_accessor_selector_identity !=
			entry->source.semantic_variant_identity ||
		    decoded->system_accessor_condition_identity !=
			entry->source.condition_identity ||
		    decoded->system_accessor_access_identity !=
			entry->source.access_identity ||
		    decoded->system_register_write !=
			(entry->source.variant_direction == 2U))
			return NATIVE_CAPTURE_VARIANT_CONFLICT;
		return NATIVE_CAPTURE_MATCH;
	}
encoding:
	if (!entry->source.encoding_mask)
		return NATIVE_CAPTURE_IRRELEVANT;
	if ((decoded->instruction & entry->source.encoding_mask) !=
	    entry->source.encoding_pattern)
		return NATIVE_CAPTURE_IRRELEVANT;
	return NATIVE_CAPTURE_MATCH;
}

static void native_capture_before_decoded(struct orlix_tcti_native_capture *capture,
		struct mm_struct *mm, const struct pt_regs *regs,
		const struct orlix_tcti_decoded_instruction *decoded)
{
	struct orlix_tcti_native_capture_session *session =
		container_of(capture, struct orlix_tcti_native_capture_session, capture);
	u32 length;

	if (!session || !decoded)
		goto fail;
	/* A session is bound to one canonical source row. Other encodings in
	 * the same normal resume are observationally irrelevant. A same-mask
	 * SystemAccessor neighbour cannot seal this row. */
	switch (native_capture_classify_entry(session, decoded)) {
	case NATIVE_CAPTURE_IRRELEVANT:
		return;
	case NATIVE_CAPTURE_VARIANT_CONFLICT:
		goto fail;
	case NATIVE_CAPTURE_MATCH:
		break;
	default:
		goto fail;
	}
	if (!regs)
		goto fail;
	if (session->decoded_seen || ++capture->target_execution_count != 1U)
		goto fail;
	session->decoded = *decoded;
	session->decoded_seen = true;
	capture->target_seen = true;
	session->before_pc = regs->pc;
	session->before_pstate = regs->pstate;
	if (!native_capture_is_memory(decoded))
		return;
	if (!mm)
		goto fail;
	length = decoded->access_size * (decoded->pair ? 2U : 1U);
	session->memory_address = native_capture_memory_address(decoded, regs);
	session->memory_effect = decoded->exclusive ||
		decoded->decode_class == ORLIX_TCTI_DECODE_LSE_ATOMIC ?
		ORLIX_TCTI_NATIVE_MEMORY_READ_MODIFY_WRITE :
		(decoded->load ? ORLIX_TCTI_NATIVE_MEMORY_READ :
		 ORLIX_TCTI_NATIVE_MEMORY_WRITE);
	if (!length)
		return;
	session->memory_bytes = kmalloc(length, GFP_KERNEL);
	if (!session->memory_bytes)
		goto fail;
	/*
	 * An unmapped or unreadable before-image is not producer corruption.
	 * FAULTS observations record the fault. They do not need a snapshot.
	 */
	if (orlix_tcti_read_user_data(mm, session->memory_address,
				       session->memory_bytes, length)) {
		kfree(session->memory_bytes);
		session->memory_bytes = NULL;
		session->memory_length = 0;
		return;
	}
	session->memory_length = length;
	return;
fail:
	session->failed = true;
}

static void native_capture_after_decoded(struct orlix_tcti_native_capture *capture,
		struct mm_struct *mm, const struct pt_regs *regs,
		const struct orlix_tcti_decoded_instruction *decoded)
{
	struct orlix_tcti_native_capture_session *session =
		container_of(capture, struct orlix_tcti_native_capture_session, capture);

	(void)mm;
	if (!session || !regs || !decoded) {
		if (session)
			session->failed = true;
		return;
	}
	/* A capture session observes one canonical target inside a normal
	 * multi-instruction block. Successful non-target encodings are not
	 * evidence for that target. A same-mask SystemAccessor neighbour is. */
	switch (native_capture_classify_entry(session, decoded)) {
	case NATIVE_CAPTURE_IRRELEVANT:
		return;
	case NATIVE_CAPTURE_VARIANT_CONFLICT:
		session->failed = true;
		return;
	case NATIVE_CAPTURE_MATCH:
		break;
	default:
		session->failed = true;
		return;
	}
	if (!session->decoded_seen ||
	    decoded->instruction != session->decoded.instruction) {
		session->failed = true;
		return;
	}
	session->after_pc = regs->pc;
	session->after_pstate = regs->pstate;
}

static void native_capture_fault(struct orlix_tcti_native_capture *capture,
		const struct orlix_tcti_decoded_instruction *decoded,
		unsigned long address, int status)
{
	struct orlix_tcti_native_capture_session *session =
		container_of(capture, struct orlix_tcti_native_capture_session, capture);

	if (!session || !decoded) {
		if (session)
			session->failed = true;
		return;
	}
	/* A fault from another encoding in the same gadget is not evidence for
	 * this session's canonical target. A same-mask SystemAccessor neighbour
	 * is a producer conflict. */
	switch (native_capture_classify_entry(session, decoded)) {
	case NATIVE_CAPTURE_IRRELEVANT:
		return;
	case NATIVE_CAPTURE_VARIANT_CONFLICT:
		session->failed = true;
		return;
	case NATIVE_CAPTURE_MATCH:
		break;
	default:
		session->failed = true;
		return;
	}
	if (!session->decoded_seen ||
	    decoded->instruction != session->decoded.instruction) {
		session->failed = true;
		return;
	}
	session->fault_seen = true;
	session->fault_address = address;
	session->fault_status = status;
	session->fault_access = session->memory_effect;
}

static void native_capture_exit(struct orlix_tcti_native_capture *capture,
		const struct orlix_tcti_result *result, const struct pt_regs *regs)
{
	struct orlix_tcti_native_capture_session *session =
		container_of(capture, struct orlix_tcti_native_capture_session, capture);

	if (!session || !result || !regs) {
		if (session)
			session->failed = true;
		return;
	}
	session->exit_seen = true;
	session->linux_reason = result->reason;
	session->linux_status = result->status;
	session->linux_pc = regs->pc;
	/* Decoder exits do not execute a gadget, so the engine exit is their after
	 * state.  Executed gadgets have already supplied the same fields. */
	if (session->decoded_seen && !session->after_pc) {
		session->after_pc = regs->pc;
		session->after_pstate = regs->pstate;
	}
}

void orlix_tcti_native_capture_complete_successful_gadget(
	struct orlix_tcti_native_capture *capture, const void *evidence)
{
	struct orlix_tcti_native_capture_session *session;

	if (!capture || !orlix_tcti_native_capture_engine_evidence_valid(evidence))
		return;
	session = container_of(capture, struct orlix_tcti_native_capture_session,
		capture);
	/* A successful gadget is not a FAULTS observation. */
	if (session->claimed_by_resume && !session->failed &&
	    session->entry &&
	    session->entry->obligation !=
		ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FAULTS &&
	    !session->origin.creditable)
		session->origin.creditable = true;
}

void orlix_tcti_native_capture_complete_fault_observation(
	struct orlix_tcti_native_capture *capture, const void *evidence)
{
	struct orlix_tcti_native_capture_session *session;

	if (!capture || !orlix_tcti_native_capture_engine_evidence_valid(evidence))
		return;
	session = container_of(capture, struct orlix_tcti_native_capture_session,
		capture);
	if (!session->claimed_by_resume || session->failed ||
	    !session->fault_seen || !session->entry ||
	    session->entry->obligation !=
		ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FAULTS)
		return;
	session->origin.creditable = true;
}

static void native_capture_finalize(struct orlix_tcti_native_capture *capture,
	const struct orlix_tcti_result *result, const struct pt_regs *regs)
{
	struct orlix_tcti_native_capture_session *session =
		container_of(capture, struct orlix_tcti_native_capture_session,
			     capture);
	u8 result_bytes[32] = {};
	u8 gpr_bytes[33U * sizeof(u64)] = {};
	u8 decode_bytes[48] = {};
	u8 fault_bytes[24] = {};
	u8 arithmetic_bytes[32] = {};
	u8 atomicity_bytes[24] = {};
	u8 ordering_bytes[16] = {};
	u8 system_bytes[NATIVE_CAPTURE_SYSTEM_CONTROL_BYTES] = {};
	u8 linux_bytes[24] = {};
	u8 *memory_bytes;
	size_t memory_payload_bytes;
	size_t index;

	if (!session || !result || !regs || session->failed || capture->finalized ||
	    !session->exit_seen)
		goto fail;
	/*
	 * Generic or non-matching events must still seal a fail-closed wire.
	 * Only a mismatched target count is producer corruption.
	 */
	if (capture->target_seen && capture->target_execution_count != 1U)
		goto fail;
	if (session->memory_length > SIZE_MAX - 16U)
		goto fail;
	memory_payload_bytes = 16U + session->memory_length;
	/* Every scalar is encoded field-by-field, never as a native struct. */
	native_capture_put32(result_bytes, 0U, result->reason);
	native_capture_put32(result_bytes, 4U, result->status);
	native_capture_put64(result_bytes, 8U, result->fault_address);
	native_capture_put32(result_bytes, 16U, result->fault_access);
	native_capture_put64(result_bytes, 20U, regs->pc);
	native_capture_put32(result_bytes, 28U, result->instruction);
	for (index = 0; index < 31U; index++)
		native_capture_put64(gpr_bytes, index * sizeof(u64), regs->regs[index]);
	native_capture_put64(gpr_bytes, 31U * sizeof(u64), regs->sp);
	/* PSTATE is explicit after-state for a register declaration. */
	native_capture_put64(gpr_bytes, 32U * sizeof(u64), regs->pstate);
	native_capture_put32(decode_bytes, 0U, session->decoded.instruction);
	native_capture_put32(decode_bytes, 4U, session->decoded.decode_class);
	native_capture_put32(decode_bytes, 8U, session->decoded.access_size);
	native_capture_put32(decode_bytes, 12U, session->decoded.result_size);
	native_capture_put32(decode_bytes, 16U, session->decoded.load);
	native_capture_put32(decode_bytes, 20U, session->decoded.set_flags);
	native_capture_put32(decode_bytes, 24U, session->decoded.acquire);
	native_capture_put32(decode_bytes, 28U, session->decoded.release);
	native_capture_put64(decode_bytes, 32U, session->before_pc);
	native_capture_put64(decode_bytes, 40U, session->after_pc);
	memory_bytes = kmalloc(memory_payload_bytes, GFP_KERNEL);
	if (!memory_bytes)
		goto fail;
	native_capture_put64(memory_bytes, 0U, session->memory_address);
	native_capture_put32(memory_bytes, 8U, session->memory_length);
	native_capture_put32(memory_bytes, 12U, session->memory_effect);
	if (session->memory_length)
		memcpy(memory_bytes + 16U, session->memory_bytes, session->memory_length);
	native_capture_put64(fault_bytes, 0U, session->fault_address);
	native_capture_put32(fault_bytes, 8U, (u32)session->fault_status);
	native_capture_put32(fault_bytes, 12U, session->fault_access);
	native_capture_put32(fault_bytes, 16U, session->fault_seen);
	native_capture_put32(fault_bytes, 20U, session->decoded.decode_class);
	native_capture_put64(arithmetic_bytes, 0U, session->before_pc);
	native_capture_put64(arithmetic_bytes, 8U, session->after_pc);
	native_capture_put64(arithmetic_bytes, 16U, session->before_pstate);
	native_capture_put64(arithmetic_bytes, 24U, session->after_pstate);
	native_capture_put32(atomicity_bytes, 0U, session->decoded.decode_class);
	native_capture_put32(atomicity_bytes, 4U, session->decoded.exclusive);
	native_capture_put32(atomicity_bytes, 8U, session->decoded.lse128);
	native_capture_put32(atomicity_bytes, 12U, session->decoded.pair);
	native_capture_put32(atomicity_bytes, 16U, session->decoded.access_size);
	native_capture_put32(atomicity_bytes, 20U, session->decoded.lse_atomic_op);
	native_capture_put32(ordering_bytes, 0U, session->decoded.barrier_op);
	native_capture_put32(ordering_bytes, 4U, session->decoded.barrier_option);
	native_capture_put32(ordering_bytes, 8U, session->decoded.acquire);
	native_capture_put32(ordering_bytes, 12U, session->decoded.release);
	native_capture_put32(system_bytes, 0U,
		session->decoded.system_accessor_selector);
	native_capture_put32(system_bytes, 4U, session->decoded.system_register_write);
	native_capture_put32(system_bytes, 8U, session->decoded.sme_pstate_operation);
	native_capture_put32(system_bytes, 12U, session->decoded.sme_streaming_mode);
	/* Preserve PSTATE's established offset and append architected EL0 state. */
	native_capture_put64(system_bytes, 16U, regs->pstate);
	native_capture_put64(system_bytes, 24U, session->task->thread.user_tls);
	native_capture_put64(system_bytes, 32U, session->task->thread.user_fpmr);
	native_capture_put32(linux_bytes, 0U, session->linux_reason);
	native_capture_put32(linux_bytes, 4U, (u32)session->linux_status);
	native_capture_put64(linux_bytes, 8U, session->linux_pc);
	native_capture_put64(linux_bytes, 16U, regs->syscallno);
	if (native_capture_append(session, 0U, result_bytes, sizeof(result_bytes)) ||
	    native_capture_append(session, 1U, gpr_bytes, sizeof(gpr_bytes)) ||
	    native_capture_fp_simd(session, session->task) ||
	    native_capture_sve(session, session->task) ||
	    native_capture_sme(session, session->task) ||
	    native_capture_append(session, 5U, decode_bytes, sizeof(decode_bytes)) ||
	    native_capture_append(session, 6U, memory_bytes, memory_payload_bytes) ||
	    native_capture_append(session, 7U, fault_bytes, sizeof(fault_bytes)) ||
	    native_capture_append(session, 8U, arithmetic_bytes, sizeof(arithmetic_bytes)) ||
	    native_capture_append(session, 9U, atomicity_bytes, sizeof(atomicity_bytes)) ||
	    native_capture_append(session, 10U, ordering_bytes, sizeof(ordering_bytes)) ||
	    native_capture_append(session, 11U, system_bytes, sizeof(system_bytes)) ||
	    native_capture_append(session, 12U, linux_bytes, sizeof(linux_bytes))) {
		kfree(memory_bytes);
		goto fail;
	}
	kfree(memory_bytes);
	native_capture_origin_finalize(&session->origin);
	if (native_capture_builder_seal(&session->builder, &session->wire))
		goto fail;
	capture->finalized = true;
	return;
fail:
	session->failed = true;
}

static const struct orlix_tcti_native_capture_ops native_capture_ops = {
	.before_decoded = native_capture_before_decoded,
	.after_decoded = native_capture_after_decoded,
	.fault = native_capture_fault,
	.exit = native_capture_exit,
	.finalize = native_capture_finalize,
};

int orlix_tcti_native_capture_begin(const void *case_token, u32 source_ordinal,
				    u32 obligation,
				    struct orlix_tcti_native_capture_session **out)
{
	struct orlix_tcti_native_capture_session *session;
	const struct orlix_tcti_native_proof_registry_entry *entry;
	u64 semantic_variant_identity;
	int ret;

	if (!out)
		return -EINVAL;
	*out = NULL;
	ret = orlix_tcti_native_proof_registry_capture_token_semantic_variant_identity(
		case_token, &semantic_variant_identity);
	if (ret)
		return -EPERM;
	ret = orlix_tcti_native_proof_registry_resolve_production(case_token,
		source_ordinal, semantic_variant_identity, obligation, &entry, NULL);
	if (ret || !entry || !entry->capture_declaration)
		return -EPERM;
	session = kzalloc(sizeof(*session), GFP_KERNEL);
	if (!session)
		return -ENOMEM;
	ret = native_capture_origin_begin(&session->origin,
		orlix_tcti_native_proof_registry_entry_identity(entry));
	if (!ret)
		ret = native_capture_builder_begin(&session->builder, &session->origin,
			entry->capture_declaration,
			orlix_tcti_native_proof_registry_entry_identity(entry),
			entry->source.source_identity,
			orlix_tcti_native_proof_registry_entry_identity(entry));
	if (ret) {
		kfree(session);
		return ret;
	}
	session->entry = entry;
	session->capture.ops = &native_capture_ops;
	mutex_lock(&native_capture_resume_lock);
	if (native_capture_resume_pending) {
		mutex_unlock(&native_capture_resume_lock);
		native_capture_builder_discard(&session->builder);
		kfree(session);
		return -EBUSY;
	}
	native_capture_resume_pending = session;
	mutex_unlock(&native_capture_resume_lock);
	*out = session;
	return 0;
}

struct orlix_tcti_native_capture *orlix_tcti_native_capture_claim_resume(
	struct task_struct *task)
{
	struct orlix_tcti_native_capture_session *session;

	mutex_lock(&native_capture_resume_lock);
	session = native_capture_resume_pending;
	native_capture_resume_pending = NULL;
	if (session) {
		session->claimed_by_resume = true;
		session->resumed = true;
		session->task = task;
	}
	mutex_unlock(&native_capture_resume_lock);
	return session ? &session->capture : NULL;
}

int orlix_tcti_native_capture_resume(
	struct orlix_tcti_native_capture_session *session,
	struct task_struct *task, struct pt_regs *regs, struct mm_struct *mm,
	struct orlix_tcti_result *result)
{
	if (!session || !task || !regs || !mm || !result || session->resumed ||
	    session->failed)
		return -EINVAL;
	mutex_lock(&native_capture_resume_lock);
	if (native_capture_resume_pending == session)
		native_capture_resume_pending = NULL;
	mutex_unlock(&native_capture_resume_lock);
	session->resumed = true;
	session->task = task;
	*result = orlix_tcti_resume_user_captured(task, regs, mm,
					   &session->capture);
	return session->failed || !session->capture.finalized ? -EBADE : 0;
}

int orlix_tcti_native_capture_take_wire(
	struct orlix_tcti_native_capture_session *session,
	struct orlix_tcti_native_wire_record *record)
{
	if (!session || !record || !session->resumed || session->failed ||
	    !session->capture.finalized || !session->wire.sealed)
		return -EINVAL;
	*record = session->wire;
	memset(&session->wire, 0, sizeof(session->wire));
	return 0;
}

void orlix_tcti_native_capture_destroy(
	struct orlix_tcti_native_capture_session *session)
{
	if (!session)
		return;
	mutex_lock(&native_capture_resume_lock);
	if (native_capture_resume_pending == session)
		native_capture_resume_pending = NULL;
	mutex_unlock(&native_capture_resume_lock);
	native_capture_builder_discard(&session->builder);
	orlix_tcti_native_wire_record_destroy(&session->wire);
	kfree(session->memory_bytes);
	kfree(session);
}
