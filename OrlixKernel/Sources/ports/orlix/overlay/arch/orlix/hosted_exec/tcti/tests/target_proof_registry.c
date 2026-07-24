/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_proof_registry.h"

#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KNOWN_CLASS_MASK \
	(TCTI_TARGET_PROOF_CLASS_REQUIRED_EL0 | \
	 TCTI_TARGET_PROOF_CLASS_NON_EL0 | \
	 TCTI_TARGET_PROOF_CLASS_ARCH_UNDEFINED_OR_UNALLOCATED)
#define KNOWN_OBLIGATION_MASK \
	(TCTI_TARGET_PROOF_OBLIGATION_DECODE | \
	 TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS | \
	 TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS | \
	 TCTI_TARGET_PROOF_OBLIGATION_REGISTERS | \
	 TCTI_TARGET_PROOF_OBLIGATION_MEMORY | \
	 TCTI_TARGET_PROOF_OBLIGATION_PC | \
	 TCTI_TARGET_PROOF_OBLIGATION_FAULTS | \
	 TCTI_TARGET_PROOF_OBLIGATION_ATOMICITY | \
	 TCTI_TARGET_PROOF_OBLIGATION_ORDERING | \
	 TCTI_TARGET_PROOF_OBLIGATION_FLAGS | \
	 TCTI_TARGET_PROOF_OBLIGATION_LINUX_INTERFACE)
#define BASELINE_OBLIGATIONS \
	(TCTI_TARGET_PROOF_OBLIGATION_DECODE | \
	 TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS | \
	 TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS)
#define NON_EL0_REJECTION_OBLIGATIONS \
	(BASELINE_OBLIGATIONS | TCTI_TARGET_PROOF_OBLIGATION_REGISTERS | \
	 TCTI_TARGET_PROOF_OBLIGATION_MEMORY | TCTI_TARGET_PROOF_OBLIGATION_PC | \
	 TCTI_TARGET_PROOF_OBLIGATION_FAULTS)
#define UNDEFINED_REJECTION_OBLIGATIONS \
	(TCTI_TARGET_PROOF_OBLIGATION_DECODE | \
	 TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS | \
	 TCTI_TARGET_PROOF_OBLIGATION_REGISTERS | \
	 TCTI_TARGET_PROOF_OBLIGATION_MEMORY | TCTI_TARGET_PROOF_OBLIGATION_PC | \
	 TCTI_TARGET_PROOF_OBLIGATION_FAULTS)
#define TCTI_TARGET_PROOF_MAX_ENTRIES 4350U
#define TCTI_TARGET_PROOF_MAX_BINDINGS 4350U
#define TCTI_TARGET_PROOF_MAX_SOURCE_BYTES (2U * 1024U * 1024U)

struct operation_requirements {
	const char *operation_id;
	uint32_t obligations;
};

struct kunit_source_provenance {
	const char *source;
	const char *sha256;
	const char *object;
};

struct kunit_case_provenance {
	const char *source;
	const char *suite;
	const char *suite_symbol;
	const char *case_array;
	const char *name;
	uint32_t maximum_obligations;
};

#define DECODE_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/tests/tcti_decode_test.c"
#define LSE_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/tests/tcti_lse_decode_test.c"
#define KUNIT_BUILD_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/tests/Makefile"
#define KUNIT_BUILD_SOURCE_SHA256 \
	"afe70b7fd88e1d20ed3dab97d7e32c4db0a545e2b2e7df2836771f05a6ad35a2"
#define KSELFTEST_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/tools/testing/selftests/orlix/tcti_lse_atomic_probe.c"
#define KSELFTEST_SOURCE_SHA256 \
	"dfe85ec0e2761dca4e15a57a0900e89ae82232351bf2f9e5deef1573f8f9bb3f"
#define KSELFTEST_BUILD_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/tools/testing/selftests/orlix/Makefile"
#define KSELFTEST_BUILD_SOURCE_SHA256 \
	"1d858a5791a52f39bd0b5616b57c8003eda2c95cda61fc2091b25d0dbfa81a9c"

/* Reviewed Kbuild inputs. The digest makes source/index drift fail closed. */
static const struct kunit_source_provenance kunit_sources[] = {
	{ DECODE_SOURCE,
	  "d670fd77b154e08eefeedca0d7e24c4bed5cb1f1b0afe5e266069f2dabae0231",
	  "tcti_decode_test.o" },
	{ LSE_SOURCE,
	  "49e3d4cc6b7db58162bead19c795d51197c3186d0c4cd0f1bd0b9c475180b997",
	  "tcti_lse_decode_test.o" },
};

/* Per-case upper bounds prevent a registered case from self-proving new duties. */
static const struct kunit_case_provenance kunit_case_provenance[] = {
	{ DECODE_SOURCE, "orlix-tcti-decode",
	  "tcti_decode_test_suite", "tcti_decode_test_cases",
	  "tcti_gadget_executes_complete_add_sub_immediate_family",
	  BASELINE_OBLIGATIONS | TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  TCTI_TARGET_PROOF_OBLIGATION_PC |
		  TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ LSE_SOURCE, "orlix-tcti-lse-decode",
	  "tcti_lse_decode_test_suite", "tcti_lse_decode_test_cases",
	  "tcti_lse_execute_rmw_matrix",
	  TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS |
		  TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  TCTI_TARGET_PROOF_OBLIGATION_MEMORY |
		  TCTI_TARGET_PROOF_OBLIGATION_PC |
		  TCTI_TARGET_PROOF_OBLIGATION_FAULTS },
};

/* Exact Arm operation_id values. Missing rows are audit blockers. */
static const struct operation_requirements operation_requirements[] = {
	{ "ADD_addsub_imm", BASELINE_OBLIGATIONS |
		TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "ADDS_addsub_imm", BASELINE_OBLIGATIONS |
		TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		TCTI_TARGET_PROOF_OBLIGATION_PC |
		TCTI_TARGET_PROOF_OBLIGATION_FLAGS },
	{ "LDADD", BASELINE_OBLIGATIONS |
		TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		TCTI_TARGET_PROOF_OBLIGATION_MEMORY |
		TCTI_TARGET_PROOF_OBLIGATION_PC |
		TCTI_TARGET_PROOF_OBLIGATION_FAULTS |
		TCTI_TARGET_PROOF_OBLIGATION_ATOMICITY |
		TCTI_TARGET_PROOF_OBLIGATION_ORDERING },
	{ "LDXR", BASELINE_OBLIGATIONS |
		TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		TCTI_TARGET_PROOF_OBLIGATION_MEMORY |
		TCTI_TARGET_PROOF_OBLIGATION_PC |
		TCTI_TARGET_PROOF_OBLIGATION_FAULTS |
		TCTI_TARGET_PROOF_OBLIGATION_ATOMICITY |
		TCTI_TARGET_PROOF_OBLIGATION_ORDERING },
};

static bool empty(const char *text)
{
	return !text || !text[0];
}

static bool one_bit(uint32_t value)
{
	return value && !(value & (value - 1));
}

int tcti_target_proof_operation_requirements(
	const char *operation_id, unsigned int classification,
	uint32_t *requirements)
{
	size_t index;

	if (empty(operation_id) || !requirements)
		return -1;
	if (classification == 2) {
		*requirements = NON_EL0_REJECTION_OBLIGATIONS;
		return 0;
	}
	if (classification == 3) {
		*requirements = UNDEFINED_REJECTION_OBLIGATIONS;
		return 0;
	}
	if (classification != 1)
		return -1;
	for (index = 0; index < sizeof(operation_requirements) /
				      sizeof(operation_requirements[0]); index++)
		if (!strcmp(operation_id,
			    operation_requirements[index].operation_id)) {
			*requirements = operation_requirements[index].obligations;
			return 0;
		}
	return -1;
}

struct sha256_state {
	uint32_t hash[8];
	uint64_t bytes;
	uint8_t block[64];
	size_t used;
};

static uint32_t rotate_right(uint32_t value, unsigned int shift)
{
	return (value >> shift) | (value << (32 - shift));
}

static void sha256_transform(struct sha256_state *state, const uint8_t *block)
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
	uint32_t words[64];
	uint32_t a, b, c, d, e, f, g, h;
	size_t index;

	for (index = 0; index < 16; index++)
		words[index] = ((uint32_t)block[index * 4] << 24) |
			((uint32_t)block[index * 4 + 1] << 16) |
			((uint32_t)block[index * 4 + 2] << 8) |
			block[index * 4 + 3];
	for (index = 16; index < 64; index++) {
		uint32_t s0 = rotate_right(words[index - 15], 7) ^
			rotate_right(words[index - 15], 18) ^
			(words[index - 15] >> 3);
		uint32_t s1 = rotate_right(words[index - 2], 17) ^
			rotate_right(words[index - 2], 19) ^
			(words[index - 2] >> 10);

		words[index] = words[index - 16] + s0 + words[index - 7] + s1;
	}
	a = state->hash[0]; b = state->hash[1]; c = state->hash[2];
	d = state->hash[3]; e = state->hash[4]; f = state->hash[5];
	g = state->hash[6]; h = state->hash[7];
	for (index = 0; index < 64; index++) {
		uint32_t sum1 = rotate_right(e, 6) ^ rotate_right(e, 11) ^
			rotate_right(e, 25);
		uint32_t choice = (e & f) ^ (~e & g);
		uint32_t temporary1 = h + sum1 + choice + constants[index] +
			words[index];
		uint32_t sum0 = rotate_right(a, 2) ^ rotate_right(a, 13) ^
			rotate_right(a, 22);
		uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
		uint32_t temporary2 = sum0 + majority;

		h = g; g = f; f = e; e = d + temporary1;
		d = c; c = b; b = a; a = temporary1 + temporary2;
	}
	state->hash[0] += a; state->hash[1] += b; state->hash[2] += c;
	state->hash[3] += d; state->hash[4] += e; state->hash[5] += f;
	state->hash[6] += g; state->hash[7] += h;
}

static void sha256_update(struct sha256_state *state, const uint8_t *data,
			  size_t length)
{
	state->bytes += length;
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

static void sha256_digest(const uint8_t *data, size_t length, uint8_t out[32])
{
	struct sha256_state state = {
		.hash = { 0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U,
			  0xa54ff53aU, 0x510e527fU, 0x9b05688cU,
			  0x1f83d9abU, 0x5be0cd19U },
	};
	uint64_t bits;
	size_t index;

	sha256_update(&state, data, length);
	bits = state.bytes * 8;
	state.block[state.used++] = 0x80;
	if (state.used > 56) {
		memset(state.block + state.used, 0, sizeof(state.block) - state.used);
		sha256_transform(&state, state.block);
		state.used = 0;
	}
	memset(state.block + state.used, 0, 56 - state.used);
	for (index = 0; index < 8; index++)
		state.block[63 - index] = (uint8_t)(bits >> (index * 8));
	sha256_transform(&state, state.block);
	for (index = 0; index < 8; index++) {
		out[index * 4] = (uint8_t)(state.hash[index] >> 24);
		out[index * 4 + 1] = (uint8_t)(state.hash[index] >> 16);
		out[index * 4 + 2] = (uint8_t)(state.hash[index] >> 8);
		out[index * 4 + 3] = (uint8_t)state.hash[index];
	}
}

static bool sha256_matches(const uint8_t *data, size_t length, const char *hex)
{
	static const char digits[] = "0123456789abcdef";
	uint8_t digest[32];
	size_t index;

	if (!hex || strlen(hex) != 64)
		return false;
	sha256_digest(data, length, digest);
	for (index = 0; index < sizeof(digest); index++)
		if (hex[index * 2] != digits[digest[index] >> 4] ||
		    hex[index * 2 + 1] != digits[digest[index] & 0xf])
			return false;
	return true;
}

int tcti_target_proof_source_size_allowed(uint64_t size)
{
	return size <= TCTI_TARGET_PROOF_MAX_SOURCE_BYTES &&
		size < (uint64_t)SIZE_MAX;
}

static char *read_source(const char *path, size_t *length)
{
	FILE *file;
	char *data;
	long size;

	file = fopen(path, "rb");
	if (!length || !file || fseek(file, 0, SEEK_END) ||
	    (size = ftell(file)) < 0 ||
	    !tcti_target_proof_source_size_allowed((uint64_t)size) ||
	    fseek(file, 0, SEEK_SET))
		goto fail;
	data = malloc((size_t)size + 1);
	if (!data || fread(data, 1, (size_t)size, file) != (size_t)size) {
		free(data);
		goto fail;
	}
	fclose(file);
	data[size] = '\0';
	*length = (size_t)size;
	return data;
fail:
	if (file)
		fclose(file);
	return NULL;
}

static bool source_registers_case(const char *source, const char *case_name)
{
	const char *cursor = source;
	size_t length = strlen(case_name);

	while ((cursor = strstr(cursor, "KUNIT_CASE"))) {
		cursor += strlen("KUNIT_CASE");
		while (isspace((unsigned char)*cursor))
			cursor++;
		if (*cursor++ != '(')
			continue;
		while (isspace((unsigned char)*cursor))
			cursor++;
		if (!strncmp(cursor, case_name, length) &&
		    (cursor[length] == ')' ||
		     isspace((unsigned char)cursor[length])))
			return true;
	}
	return false;
}

static bool source_registers_suite(const char *source, const char *suite)
{
	const char *cursor = source;
	size_t length = strlen(suite);

	while ((cursor = strstr(cursor, ".name"))) {
		cursor += strlen(".name");
		while (isspace((unsigned char)*cursor))
			cursor++;
		if (*cursor++ != '=')
			continue;
		while (isspace((unsigned char)*cursor))
			cursor++;
		if (*cursor++ != '\"')
			continue;
		if (!strncmp(cursor, suite, length) && cursor[length] == '\"')
			return true;
	}
	return false;
}

static bool source_connects_case_to_suite(
	const char *source, const struct kunit_case_provenance *provenance)
{
	char declaration[160];
	char name_assignment[160];
	char cases_assignment[160];
	char registration[160];
	char array_declaration[160];
	char case_registration[192];
	const char *array;
	const char *array_end;
	const char *registered_case;

	if (snprintf(declaration, sizeof(declaration), "struct kunit_suite %s",
		     provenance->suite_symbol) >= (int)sizeof(declaration) ||
	    snprintf(name_assignment, sizeof(name_assignment), ".name = \"%s\"",
		     provenance->suite) >= (int)sizeof(name_assignment) ||
	    snprintf(cases_assignment, sizeof(cases_assignment),
		     ".test_cases = %s", provenance->case_array) >=
		     (int)sizeof(cases_assignment) ||
	    snprintf(registration, sizeof(registration), "kunit_test_suite(%s)",
		     provenance->suite_symbol) >= (int)sizeof(registration) ||
	    snprintf(array_declaration, sizeof(array_declaration),
		     "struct kunit_case %s[] = {", provenance->case_array) >=
		     (int)sizeof(array_declaration) ||
	    snprintf(case_registration, sizeof(case_registration), "KUNIT_CASE(%s)",
		     provenance->name) >= (int)sizeof(case_registration))
		return false;
	if (!strstr(source, declaration) || !strstr(source, name_assignment) ||
	    !strstr(source, cases_assignment) || !strstr(source, registration))
		return false;
	array = strstr(source, array_declaration);
	if (!array || !(array_end = strstr(array, "};")))
		return false;
	registered_case = strstr(array, case_registration);
	return registered_case && registered_case < array_end;
}

static const struct kunit_source_provenance *
find_kunit_source(const char *path)
{
	size_t index;

	for (index = 0; index < sizeof(kunit_sources) /
				      sizeof(kunit_sources[0]); index++)
		if (!strcmp(path, kunit_sources[index].source))
			return &kunit_sources[index];
	return NULL;
}

static const struct kunit_case_provenance *
find_kunit_case(const struct tcti_target_proof_registry_entry *entry,
		const struct tcti_target_proof_case *proof_case)
{
	size_t index;

	for (index = 0; index < sizeof(kunit_case_provenance) /
				      sizeof(kunit_case_provenance[0]); index++) {
		const struct kunit_case_provenance *item =
			&kunit_case_provenance[index];

		if (!strcmp(entry->kunit_source, item->source) &&
		    !strcmp(entry->kunit_suite, item->suite) &&
		    !strcmp(proof_case->name, item->name))
			return item;
	}
	return NULL;
}

static bool valid_kunit_provenance(
	const struct tcti_target_proof_registry_entry *entry)
{
	char *build_source;
	char *source;
	const struct kunit_source_provenance *source_metadata;
	size_t build_length;
	size_t source_length;
	size_t index;
	bool valid = false;

	source_metadata = find_kunit_source(entry->kunit_source);
	if (!source_metadata)
		return false;
	build_source = read_source(KUNIT_BUILD_SOURCE, &build_length);
	if (!build_source)
		return false;
	if (!sha256_matches((const uint8_t *)build_source, build_length,
			    KUNIT_BUILD_SOURCE_SHA256) ||
	    !strstr(build_source, source_metadata->object)) {
		free(build_source);
		return false;
	}
	free(build_source);
	source = read_source(entry->kunit_source, &source_length);
	if (!source)
		return false;
	if (memchr(source, '\0', source_length) ||
	    !sha256_matches((const uint8_t *)source, source_length,
			    source_metadata->sha256) ||
	    !source_registers_suite(source, entry->kunit_suite))
		goto out;
	for (index = 0; index < entry->kunit_case_count; index++) {
		const struct kunit_case_provenance *case_metadata =
			find_kunit_case(entry, &entry->kunit_cases[index]);

		if (!case_metadata ||
		    (entry->kunit_cases[index].obligations &
		     ~case_metadata->maximum_obligations) ||
		    !source_registers_case(source, entry->kunit_cases[index].name) ||
		    !source_connects_case_to_suite(source, case_metadata))
			goto out;
	}
	valid = true;
out:
	free(source);
	return valid;
}

int tcti_target_kselftest_provenance_validate(
	const struct tcti_target_kselftest_provenance *provenance)
{
	char *build_source;
	char *source;
	size_t build_length;
	size_t source_length;
	bool valid;

	if (!provenance || empty(provenance->source) ||
	    empty(provenance->source_sha256) ||
	    empty(provenance->build_source) ||
	    empty(provenance->build_source_sha256) ||
	    empty(provenance->program) || empty(provenance->case_name) ||
	    strcmp(provenance->source, KSELFTEST_SOURCE) ||
	    strcmp(provenance->source_sha256, KSELFTEST_SOURCE_SHA256) ||
	    strcmp(provenance->build_source, KSELFTEST_BUILD_SOURCE) ||
	    strcmp(provenance->build_source_sha256,
		   KSELFTEST_BUILD_SOURCE_SHA256) ||
	    strcmp(provenance->program, "tcti_lse_atomic_probe") ||
	    strcmp(provenance->case_name, "main"))
		return -1;
	build_source = read_source(provenance->build_source, &build_length);
	if (!build_source)
		return -1;
	source = read_source(provenance->source, &source_length);
	if (!source) {
		free(build_source);
		return -1;
	}
	valid = !memchr(build_source, '\0', build_length) &&
		!memchr(source, '\0', source_length) &&
		sha256_matches((const uint8_t *)build_source, build_length,
			       provenance->build_source_sha256) &&
		sha256_matches((const uint8_t *)source, source_length,
			       provenance->source_sha256) &&
		strstr(build_source, provenance->program) &&
		strstr(source, "int main(void)");
	free(source);
	free(build_source);
	return valid ? 0 : -1;
}

int tcti_target_proof_source_evidence_validate(
	const char *source_path, const char *source_sha256, const char *assertion)
{
	char *source;
	size_t source_length;
	bool valid;

	if (empty(source_path) || empty(source_sha256) || empty(assertion))
		return -1;
	source = read_source(source_path, &source_length);
	if (!source)
		return -1;
	valid = !memchr(source, '\0', source_length) &&
		sha256_matches((const uint8_t *)source, source_length,
			       source_sha256) && strstr(source, assertion);
	free(source);
	return valid ? 0 : -1;
}

int tcti_target_proof_registry_validate(
	const struct tcti_target_proof_registry_entry *entries, size_t count,
	enum tcti_target_proof_registry_error *error)
{
	size_t index;
	size_t previous;

	if (error)
		*error = TCTI_TARGET_PROOF_REGISTRY_OK;
	if ((!entries && count) || count > TCTI_TARGET_PROOF_MAX_ENTRIES)
		goto invalid;
	for (index = 0; index < count; index++) {
		const struct tcti_target_proof_registry_entry *entry = &entries[index];
		uint32_t case_obligations = 0;
		uint32_t kunit_obligations;
		uint32_t required;
		unsigned int classification;
		size_t binding;
		size_t proof_case;

		if (empty(entry->id) || empty(entry->operation_id) ||
		    !one_bit(entry->classification_mask) ||
		    (entry->classification_mask & ~KNOWN_CLASS_MASK) ||
		    !entry->obligations ||
		    (entry->obligations & ~KNOWN_OBLIGATION_MASK) ||
		    entry->linux_interface >
			TCTI_TARGET_PROOF_LINUX_INTERFACE_REQUIRED ||
		    empty(entry->kunit_source) || empty(entry->kunit_suite) ||
		    !entry->kunit_cases || !entry->kunit_case_count ||
		    entry->kunit_case_count > 64 ||
		    !entry->bindings || !entry->binding_count ||
		    entry->binding_count > TCTI_TARGET_PROOF_MAX_BINDINGS)
			goto invalid;
		if (entry->classification_mask ==
		    TCTI_TARGET_PROOF_CLASS_REQUIRED_EL0)
			classification = 1;
		else if (entry->classification_mask ==
			 TCTI_TARGET_PROOF_CLASS_NON_EL0)
			classification = 2;
		else
			classification = 3;
		if (tcti_target_proof_operation_requirements(
			    entry->operation_id, classification, &required)) {
			if (error)
				*error =
					TCTI_TARGET_PROOF_REGISTRY_UNKNOWN_FAMILY_REQUIREMENTS;
			return -1;
		}
		kunit_obligations = required;
		if (entry->linux_interface ==
		    TCTI_TARGET_PROOF_LINUX_INTERFACE_REQUIRED)
			required |= TCTI_TARGET_PROOF_OBLIGATION_LINUX_INTERFACE;
		if (entry->obligations != required) {
			if (error)
				*error =
					TCTI_TARGET_PROOF_REGISTRY_INSUFFICIENT_OBLIGATIONS;
			return -1;
		}
		if (entry->linux_interface ==
		    TCTI_TARGET_PROOF_LINUX_INTERFACE_REQUIRED) {
			/* KUnit cannot satisfy Linux-visible interface proof. */
			if (tcti_target_kselftest_provenance_validate(entry->kselftest)) {
				if (error)
					*error =
						TCTI_TARGET_PROOF_REGISTRY_INVALID_KSELFTEST_PROVENANCE;
				return -1;
			}
		}
		if (entry->linux_interface ==
			    TCTI_TARGET_PROOF_LINUX_INTERFACE_NOT_APPLICABLE &&
		    entry->kselftest)
			goto invalid;
		for (proof_case = 0; proof_case < entry->kunit_case_count;
		     proof_case++) {
			const struct tcti_target_proof_case *item =
				&entry->kunit_cases[proof_case];

			if (empty(item->name) || !item->obligations ||
			    (item->obligations & ~KNOWN_OBLIGATION_MASK))
				goto invalid;
			if (item->obligations &
			    TCTI_TARGET_PROOF_OBLIGATION_LINUX_INTERFACE)
				goto invalid;
			for (previous = 0; previous < proof_case; previous++)
				if (!strcmp(item->name,
					    entry->kunit_cases[previous].name))
					goto invalid;
			case_obligations |= item->obligations;
		}
		if (case_obligations != kunit_obligations) {
			if (error)
				*error =
					TCTI_TARGET_PROOF_REGISTRY_INSUFFICIENT_OBLIGATIONS;
			return -1;
		}
		if (!valid_kunit_provenance(entry)) {
			if (error)
				*error =
					TCTI_TARGET_PROOF_REGISTRY_INVALID_KUNIT_PROVENANCE;
			return -1;
		}
		for (binding = 0; binding < entry->binding_count; binding++) {
			const struct tcti_target_proof_binding *item =
				&entry->bindings[binding];
			uint32_t binding_obligations = 0;
			size_t case_index;

			if (empty(item->leaf_name) || empty(item->mnemonic) ||
			    empty(item->condition_tcnd_hex) ||
			    !item->kunit_case_mask ||
			    (entry->kunit_case_count < 64 &&
			     (item->kunit_case_mask >> entry->kunit_case_count)))
				goto invalid;
			for (case_index = 0; case_index < entry->kunit_case_count;
			     case_index++)
				if (item->kunit_case_mask & (UINT64_C(1) << case_index))
					binding_obligations |=
						entry->kunit_cases[case_index].obligations;
			if (binding_obligations != kunit_obligations) {
				if (error)
					*error =
						TCTI_TARGET_PROOF_REGISTRY_INSUFFICIENT_OBLIGATIONS;
				return -1;
			}
			for (previous = 0; previous < binding; previous++)
				if (!strcmp(item->leaf_name,
					    entry->bindings[previous].leaf_name))
					goto invalid;
		}
		for (previous = 0; previous < index; previous++) {
			if (!strcmp(entry->id, entries[previous].id)) {
				if (error)
					*error =
						TCTI_TARGET_PROOF_REGISTRY_DUPLICATE_ID;
				return -1;
			}
			if (entry->classification_mask ==
				    entries[previous].classification_mask &&
			    !strcmp(entry->operation_id,
				    entries[previous].operation_id))
				goto invalid;
		}
	}
	return 0;
invalid:
	if (error)
		*error = TCTI_TARGET_PROOF_REGISTRY_INVALID_ENTRY;
	return -1;
}

enum tcti_target_proof_registry_error tcti_target_proof_registry_lookup(
	const struct tcti_target_proof_registry_entry *entries, size_t count,
	const struct tcti_target_proof_reference *reference)
{
	const struct tcti_target_proof_registry_entry *entry = NULL;
	size_t binding;
	size_t index;
	uint32_t class_bit;

	if (!reference || empty(reference->id) || empty(reference->leaf_name) ||
	    empty(reference->mnemonic) || empty(reference->operation_id) ||
	    empty(reference->condition_tcnd_hex))
		return TCTI_TARGET_PROOF_REGISTRY_MISSING_REFERENCE;
	if (reference->classification >= 32)
		return TCTI_TARGET_PROOF_REGISTRY_CLASSIFICATION_MISMATCH;
	class_bit = 1U << reference->classification;
	for (index = 0; index < count; index++)
		if (!strcmp(entries[index].id, reference->id)) {
			entry = &entries[index];
			break;
		}
	if (!entry)
		return TCTI_TARGET_PROOF_REGISTRY_UNKNOWN_PROOF;
	if (!(entry->classification_mask & class_bit))
		return TCTI_TARGET_PROOF_REGISTRY_CLASSIFICATION_MISMATCH;
	if (strcmp(entry->operation_id, reference->operation_id))
		return TCTI_TARGET_PROOF_REGISTRY_FAMILY_MISMATCH;
	for (binding = 0; binding < entry->binding_count; binding++)
		if (!strcmp(entry->bindings[binding].leaf_name,
			    reference->leaf_name) &&
		    !strcmp(entry->bindings[binding].mnemonic,
			    reference->mnemonic) &&
		    entry->bindings[binding].encoding_mask ==
			    reference->encoding_mask &&
		    entry->bindings[binding].encoding_pattern ==
			    reference->encoding_pattern &&
		    !strcmp(entry->bindings[binding].condition_tcnd_hex,
			    reference->condition_tcnd_hex))
			return TCTI_TARGET_PROOF_REGISTRY_OK;
	return TCTI_TARGET_PROOF_REGISTRY_BINDING_MISMATCH;
}

const struct tcti_target_proof_registry_entry *
tcti_target_proof_registry_entries(size_t *count)
{
	if (count)
		*count = 0;
	return NULL;
}
