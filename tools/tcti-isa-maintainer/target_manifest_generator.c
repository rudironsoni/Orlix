// SPDX-License-Identifier: GPL-2.0-only
/*
 * Emit the complete source-derived target manifest from one pinned Arm input.
 * This host maintenance tool never evaluates HWCAP. Classifications, evidence,
 * and proof IDs remain in the separate repository-owned target ledger.
 */
#include "target_condition_serialization.h"
#include "target_inventory_import.h"
#include "target_manifest_generator.h"

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char source_architecture[] = "vFATAp1-A";
static const char source_build[] = "818";
static const char source_reference[] = "2026-06_rel";
static const char source_schema[] = "2.9.5";
static const char source_sha256[] =
	"a1ad2c6538a47cd97d8762791ac5af88bce1d5f6aff096c9b77aef853e76acfe";

struct source_bytes {
	char *data;
	size_t length;
};

#ifndef TARGET_MANIFEST_GENERATOR_NO_MAIN
static const char *target_manifest_generator_error_name(
	enum target_manifest_generator_error error)
{
	switch (error) {
	case TCTI_TARGET_MANIFEST_GENERATOR_OK:
		return "success";
	case TCTI_TARGET_MANIFEST_GENERATOR_PARSE:
		return "parse failure";
	case TCTI_TARGET_MANIFEST_GENERATOR_METADATA:
		return "metadata failure";
	case TCTI_TARGET_MANIFEST_GENERATOR_COUNT:
		return "count failure";
	case TCTI_TARGET_MANIFEST_GENERATOR_DIGEST:
		return "digest failure";
	case TCTI_TARGET_MANIFEST_GENERATOR_IO:
		return "I/O failure";
	case TCTI_TARGET_MANIFEST_GENERATOR_MANIFEST_MISMATCH:
		return "manifest mismatch";
	}
	return "unknown failure";
}
#endif

static enum target_manifest_generator_error import_error_category(
	const struct tcti_target_import_error *error)
{
	switch (error->code) {
	case TCTI_TARGET_IMPORT_COUNT_MISMATCH:
		return TCTI_TARGET_MANIFEST_GENERATOR_COUNT;
	case TCTI_TARGET_IMPORT_HASH_MISMATCH:
		return TCTI_TARGET_MANIFEST_GENERATOR_DIGEST;
	case TCTI_TARGET_IMPORT_INVALID_SOURCE:
		if (!strcmp(error->message,
			    "Instructions.json metadata does not match the pinned Arm AARCHMRS 2026-06 source"))
			return TCTI_TARGET_MANIFEST_GENERATOR_METADATA;
		return TCTI_TARGET_MANIFEST_GENERATOR_PARSE;
	case TCTI_TARGET_IMPORT_OK:
		return TCTI_TARGET_MANIFEST_GENERATOR_OK;
	default:
		return TCTI_TARGET_MANIFEST_GENERATOR_PARSE;
	}
}

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
	a = state->words[0];
	b = state->words[1];
	c = state->words[2];
	d = state->words[3];
	e = state->words[4];
	f = state->words[5];
	g = state->words[6];
	h = state->words[7];
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

		h = g;
		g = f;
		f = e;
		e = d + temporary1;
		d = c;
		c = b;
		b = a;
		a = temporary1 + temporary2;
	}
	state->words[0] += a;
	state->words[1] += b;
	state->words[2] += c;
	state->words[3] += d;
	state->words[4] += e;
	state->words[5] += f;
	state->words[6] += g;
	state->words[7] += h;
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
	struct sha256_state state = {
		.words = { 0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U,
			   0xa54ff53aU, 0x510e527fU, 0x9b05688cU,
			   0x1f83d9abU, 0x5be0cd19U },
	};
	uint64_t bits;
	unsigned char output[32];
	size_t index;

	sha256_update(&state, (const unsigned char *)data, length);
	bits = state.byte_count * 8U;
	state.block[state.used++] = 0x80;
	if (state.used > 56) {
		memset(state.block + state.used, 0,
		       sizeof(state.block) - state.used);
		sha256_transform(&state, state.block);
		state.used = 0;
	}
	memset(state.block + state.used, 0, 56 - state.used);
	for (index = 0; index < 8; index++)
		state.block[63 - index] = (unsigned char)(bits >> (index * 8));
	sha256_transform(&state, state.block);
	for (index = 0; index < 8; index++) {
		output[index * 4] = (unsigned char)(state.words[index] >> 24);
		output[index * 4 + 1] = (unsigned char)(state.words[index] >> 16);
		output[index * 4 + 2] = (unsigned char)(state.words[index] >> 8);
		output[index * 4 + 3] = (unsigned char)state.words[index];
	}
	for (index = 0; index < sizeof(output); index++) {
		digest[index * 2] = hex[output[index] >> 4];
		digest[index * 2 + 1] = hex[output[index] & 0xfU];
	}
	digest[64] = '\0';
}

#ifndef TARGET_MANIFEST_GENERATOR_NO_MAIN
static int read_source(const char *path, struct source_bytes *source)
{
	FILE *file;
	long length;

	file = fopen(path, "rb");
	if (!file) {
		fprintf(stderr, "%s: %s\n", path, strerror(errno));
		return -1;
	}
	if (fseek(file, 0, SEEK_END) || (length = ftell(file)) < 0 ||
	    fseek(file, 0, SEEK_SET)) {
		fprintf(stderr, "%s: cannot determine source length\n", path);
		fclose(file);
		return -1;
	}
	if ((uintmax_t)length > SIZE_MAX - 1) {
		fprintf(stderr, "%s: source too large\n", path);
		fclose(file);
		return -1;
	}
	source->data = malloc((size_t)length + 1);
	if (!source->data) {
		fprintf(stderr, "%s: cannot allocate source bytes\n", path);
		fclose(file);
		return -1;
	}
	if (fread(source->data, 1, (size_t)length, file) != (size_t)length ||
	    fclose(file)) {
		fprintf(stderr, "%s: cannot read source\n", path);
		free(source->data);
		*source = (struct source_bytes) { 0 };
		return -1;
	}
	source->data[length] = '\0';
	source->length = (size_t)length;
	return 0;
}
#endif

static void emit_c_string(FILE *output, const char *text)
{
	const unsigned char *bytes = (const unsigned char *)text;

	fputc('"', output);
	while (*bytes) {
		if (*bytes == '"' || *bytes == '\\')
			fputc('\\', output);
		if (*bytes >= 0x20 && *bytes <= 0x7e)
			fputc(*bytes, output);
		else
			fprintf(output, "\\x%02x", *bytes);
		bytes++;
	}
	fputc('"', output);
}

static int emit_tcnd(FILE *output,
		     const struct tcti_target_inventory *inventory,
		     uint32_t condition)
{
	struct tcti_target_condition_bytes bytes = { 0 };
	enum tcti_target_condition_serialize_error error;
	size_t index;

	if (tcti_target_condition_serialize(inventory, condition, &bytes,
					      &error)) {
		fprintf(stderr, "cannot serialize TCND condition: %d\n", error);
		return -1;
	}
	fputc('"', output);
	for (index = 0; index < bytes.length; index++)
		fprintf(output, "%02x", bytes.data[index]);
	fputc('"', output);
	tcti_target_condition_bytes_destroy(&bytes);
	return ferror(output) ? -1 : 0;
}

static int emit_manifest(FILE *output,
			 const struct tcti_target_inventory *inventory)
{
	size_t index;

	fputs("/*\n"
	      " * Copyright (c) 2010-2026 Arm Limited or affiliates. All rights reserved.\n"
	      " * This generated source-derived data is subject to the BSD 3-Clause terms in ARM-AARCHMRS-LICENSE.txt.\n"
	      " * Generated by target_manifest_generator.c. Do not edit.\n"
	      " */\n"
	      "/* SPDX-License-Identifier: BSD-3-Clause */\n"
	      "TCTI_A64_SOURCE_MANIFEST_SOURCE(", output);
	emit_c_string(output, source_architecture);
	fputs(", ", output);
	emit_c_string(output, source_build);
	fputs(", ", output);
	emit_c_string(output, source_reference);
	fputs(", ", output);
	emit_c_string(output, source_schema);
	fputs(", ", output);
	emit_c_string(output, source_sha256);
	fprintf(output, ", %u)\n", TCTI_A64_TARGET_LEAF_COUNT);
	for (index = 0; index < inventory->leaf_count; index++) {
		const struct tcti_target_leaf *leaf = &inventory->leaves[index];

		fputs("TCTI_A64_SOURCE_MANIFEST_ROW(", output);
		fprintf(output, "%zu, ", index);
		emit_c_string(output, leaf->name);
		fputs(", ", output);
		emit_c_string(output, leaf->mnemonic);
		fputs(", ", output);
		emit_c_string(output, leaf->operation_id);
		fprintf(output, ", 0x%08" PRIx32 "U, 0x%08" PRIx32 "U, ",
			leaf->encoding_mask, leaf->encoding_pattern);
		if (emit_tcnd(output, inventory, leaf->condition)) {
			fprintf(stderr, "cannot serialize condition %s\n", leaf->name);
			return -1;
		}
		fputs(")\n", output);
	}
	return ferror(output) ? -1 : 0;
}

/*
 * No byte reaches output until all source-byte and parsed-source pins pass.
 * This entry point is also deliberately testable without a subprocess.
 */
enum target_manifest_generator_error
target_manifest_generator_emit(const char *source, size_t length, FILE *output)
{
	struct tcti_target_inventory inventory = { 0 };
	struct tcti_target_import_error error = { 0 };
	char digest[65];
	enum target_manifest_generator_error status =
		TCTI_TARGET_MANIFEST_GENERATOR_PARSE;

	if (!source || !output)
		return status;
	/* The importer validates parsed metadata and the parsed A64 leaf count. */
	if (tcti_target_inventory_import(source, length, &inventory, &error)) {
		status = import_error_category(&error);
		goto out;
	}
	if (inventory.leaf_count != TCTI_A64_TARGET_LEAF_COUNT) {
		status = TCTI_TARGET_MANIFEST_GENERATOR_COUNT;
		goto out;
	}
	sha256_hex(source, length, digest);
	if (strcmp(digest, source_sha256)) {
		status = TCTI_TARGET_MANIFEST_GENERATOR_DIGEST;
		goto out;
	}
	if (emit_manifest(output, &inventory))
		status = ferror(output) ? TCTI_TARGET_MANIFEST_GENERATOR_IO :
			TCTI_TARGET_MANIFEST_GENERATOR_PARSE;
	else
		status = TCTI_TARGET_MANIFEST_GENERATOR_OK;
out:
	tcti_target_inventory_destroy(&inventory);
	return status;
}

#ifndef TARGET_MANIFEST_GENERATOR_NO_MAIN
static int files_match(FILE *expected, FILE *actual)
{
	unsigned char expected_buffer[4096];
	unsigned char actual_buffer[4096];
	size_t expected_count;
	size_t actual_count;

	for (;;) {
		expected_count = fread(expected_buffer, 1, sizeof(expected_buffer),
				       expected);
		actual_count = fread(actual_buffer, 1, sizeof(actual_buffer), actual);
		if (expected_count != actual_count ||
		    memcmp(expected_buffer, actual_buffer, expected_count))
			return -1;
		if (!expected_count)
			return ferror(expected) || ferror(actual) ? -1 : 0;
	}
}
#endif

#ifndef TARGET_MANIFEST_GENERATOR_NO_MAIN
int main(int argc, char **argv)
{
	int status = EXIT_FAILURE;
	FILE *output = stdout;
	FILE *expected = NULL;
	const char *source_path;
	struct source_bytes source = { 0 };
	enum target_manifest_generator_error error =
		TCTI_TARGET_MANIFEST_GENERATOR_OK;

	if (argc == 2) {
		source_path = argv[1];
	} else if (argc == 4 && !strcmp(argv[1], "--check")) {
		source_path = argv[2];
		expected = fopen(argv[3], "rb");
		if (!expected) {
			fprintf(stderr, "%s: %s\n", argv[3], strerror(errno));
			fprintf(stderr, "target manifest generator: I/O failure\n");
			return EXIT_FAILURE;
		}
		output = tmpfile();
		if (!output) {
			fprintf(stderr, "cannot create temporary manifest: %s\n",
				strerror(errno));
			fclose(expected);
			fprintf(stderr, "target manifest generator: I/O failure\n");
			return EXIT_FAILURE;
		}
	} else {
		fprintf(stderr, "usage: %s Instructions.json\n"
			"       %s --check Instructions.json manifest.def\n",
			argv[0], argv[0]);
		return EXIT_FAILURE;
	}
	if (read_source(source_path, &source)) {
		error = TCTI_TARGET_MANIFEST_GENERATOR_IO;
		goto out;
	}
	error = target_manifest_generator_emit(source.data, source.length, output);
	if (error != TCTI_TARGET_MANIFEST_GENERATOR_OK)
		goto out;
	if (expected && (fflush(output) || fseek(output, 0, SEEK_SET) ||
			 files_match(expected, output))) {
		error = ferror(expected) || ferror(output) ?
			TCTI_TARGET_MANIFEST_GENERATOR_IO :
			TCTI_TARGET_MANIFEST_GENERATOR_MANIFEST_MISMATCH;
		goto out;
	}
	status = EXIT_SUCCESS;
out:
	if (status != EXIT_SUCCESS)
		fprintf(stderr, "target manifest generator: %s\n",
			target_manifest_generator_error_name(error));
	if (expected)
		fclose(expected);
	if (output != stdout)
		fclose(output);
	free(source.data);
	return status;
}
#endif
