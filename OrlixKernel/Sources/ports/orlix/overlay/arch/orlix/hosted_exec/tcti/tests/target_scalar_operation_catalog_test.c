/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_scalar_operation_catalog.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EXPECT(condition) \
	do { \
		if (!(condition)) { \
			fprintf(stderr, "%s:%d: expectation failed: %s\n", \
				__FILE__, __LINE__, #condition); \
			return -1; \
		} \
	} while (0)

static struct tcti_scalar_operation_catalog_entry *catalog_copy(size_t *count)
{
	const struct tcti_scalar_operation_catalog_entry *entries =
		tcti_scalar_operation_catalog_entries(count);
	struct tcti_scalar_operation_catalog_entry *copy;

	copy = malloc(*count * sizeof(*copy));
	if (!copy)
		return NULL;
	memcpy(copy, entries, *count * sizeof(*copy));
	return copy;
}

static int catalog_is_exact_and_unproved(void)
{
	const struct tcti_scalar_operation_catalog_entry *entries;
	enum tcti_scalar_operation_catalog_error error;
	char catalog_sha256[TCTI_SCALAR_OPERATION_SHA256_HEX_SIZE];
	size_t count;
	size_t index;

	entries = tcti_scalar_operation_catalog_entries(&count);
	EXPECT(entries != NULL);
	EXPECT(count == TCTI_SCALAR_OPERATION_CATALOG_EXPECTED_COUNT);
	EXPECT(tcti_scalar_operation_catalog_sha256(entries, count,
						    catalog_sha256) == 0);
	if (strcmp(catalog_sha256,
		   TCTI_SCALAR_OPERATION_CATALOG_REVIEWED_SHA256))
		fprintf(stderr, "catalog SHA-256: expected %s, actual %s\n",
			TCTI_SCALAR_OPERATION_CATALOG_REVIEWED_SHA256,
			catalog_sha256);
	EXPECT(!strcmp(catalog_sha256,
		       TCTI_SCALAR_OPERATION_CATALOG_REVIEWED_SHA256));
	if (tcti_scalar_operation_catalog_validate(entries, count, &error) < 0) {
		fprintf(stderr, "catalog validation error: %u\n", error);
		return -1;
	}
	EXPECT(error == TCTI_SCALAR_OPERATION_CATALOG_OK);
	for (index = 0; index < count; index++) {
		EXPECT(entries[index].proof_state ==
		       TCTI_SCALAR_OPERATION_UNPROVED);
		EXPECT(strlen(entries[index].source_sha256) == 64);
	}
	return 0;
}

static int missing_binding_is_rejected(void)
{
	struct tcti_scalar_operation_catalog_entry *copy;
	enum tcti_scalar_operation_catalog_error error;
	size_t count;

	copy = catalog_copy(&count);
	EXPECT(copy != NULL);
	copy[0].leaf_name = "";
	EXPECT(tcti_scalar_operation_catalog_validate(copy, count, &error) < 0);
	EXPECT(error == TCTI_SCALAR_OPERATION_CATALOG_MISSING_BINDING);
	free(copy);
	return 0;
}

static int missing_leaf_is_rejected(void)
{
	const struct tcti_scalar_operation_catalog_entry *entries;
	enum tcti_scalar_operation_catalog_error error;
	size_t count;

	entries = tcti_scalar_operation_catalog_entries(&count);
	EXPECT(count > 1);
	EXPECT(tcti_scalar_operation_catalog_validate(entries, count - 1,
						      &error) < 0);
	EXPECT(error == TCTI_SCALAR_OPERATION_CATALOG_WRONG_COUNT);
	return 0;
}

static int duplicate_leaf_is_rejected(void)
{
	struct tcti_scalar_operation_catalog_entry *copy;
	enum tcti_scalar_operation_catalog_error error;
	size_t count;

	copy = catalog_copy(&count);
	EXPECT(copy != NULL);
	copy[1].leaf_name = copy[0].leaf_name;
	EXPECT(tcti_scalar_operation_source_sha256(
		       &copy[1], copy[1].source_sha256) == 0);
	EXPECT(tcti_scalar_operation_catalog_validate(copy, count, &error) < 0);
	EXPECT(error == TCTI_SCALAR_OPERATION_CATALOG_DUPLICATE_LEAF);
	free(copy);
	return 0;
}

static int duplicate_encoding_is_rejected(void)
{
	struct tcti_scalar_operation_catalog_entry *copy;
	enum tcti_scalar_operation_catalog_error error;
	size_t count;

	copy = catalog_copy(&count);
	EXPECT(copy != NULL);
	copy[1].encoding_mask = copy[0].encoding_mask;
	copy[1].encoding_pattern = copy[0].encoding_pattern;
	copy[1].condition_tcnd_hex = copy[0].condition_tcnd_hex;
	EXPECT(tcti_scalar_operation_source_sha256(
		       &copy[1], copy[1].source_sha256) == 0);
	EXPECT(tcti_scalar_operation_catalog_validate(copy, count, &error) < 0);
	EXPECT(error ==
	       TCTI_SCALAR_OPERATION_CATALOG_DUPLICATE_SOURCE_ENCODING);
	free(copy);
	return 0;
}

static int stale_source_is_rejected(void)
{
	struct tcti_scalar_operation_catalog_entry *copy;
	enum tcti_scalar_operation_catalog_error error;
	size_t count;

	copy = catalog_copy(&count);
	EXPECT(copy != NULL);
	copy[0].encoding_pattern ^= 1U;
	EXPECT(tcti_scalar_operation_catalog_validate(copy, count, &error) < 0);
	EXPECT(error == TCTI_SCALAR_OPERATION_CATALOG_STALE_SOURCE);
	free(copy);
	return 0;
}

static int one_byte_digest_mutation_is_rejected(void)
{
	struct tcti_scalar_operation_catalog_entry *copy;
	enum tcti_scalar_operation_catalog_error error;
	size_t count;

	copy = catalog_copy(&count);
	EXPECT(copy != NULL);
	copy[0].source_sha256[17] =
		copy[0].source_sha256[17] == '0' ? '1' : '0';
	EXPECT(tcti_scalar_operation_catalog_validate(copy, count, &error) < 0);
	EXPECT(error == TCTI_SCALAR_OPERATION_CATALOG_STALE_SOURCE);
	free(copy);
	return 0;
}

static int malformed_digest_lengths_are_rejected(void)
{
	struct tcti_scalar_operation_catalog_entry *copy;
	enum tcti_scalar_operation_catalog_error error;
	size_t count;

	copy = catalog_copy(&count);
	EXPECT(copy != NULL);
	copy[0].source_sha256[63] = '\0';
	EXPECT(tcti_scalar_operation_catalog_validate(copy, count, &error) < 0);
	EXPECT(error == TCTI_SCALAR_OPERATION_CATALOG_STALE_SOURCE);
	memcpy(copy, tcti_scalar_operation_catalog_entries(NULL),
	       count * sizeof(*copy));
	memset(copy[0].source_sha256, '0', sizeof(copy[0].source_sha256));
	EXPECT(tcti_scalar_operation_catalog_validate(copy, count, &error) < 0);
	EXPECT(error == TCTI_SCALAR_OPERATION_CATALOG_STALE_SOURCE);
	free(copy);
	return 0;
}

static int exact_field_lengths_are_hashed(void)
{
	const struct tcti_scalar_operation_catalog_entry *entries;
	struct tcti_scalar_operation_catalog_entry first;
	struct tcti_scalar_operation_catalog_entry second;
	char first_sha256[TCTI_SCALAR_OPERATION_SHA256_HEX_SIZE];
	char second_sha256[TCTI_SCALAR_OPERATION_SHA256_HEX_SIZE];

	entries = tcti_scalar_operation_catalog_entries(NULL);
	EXPECT(entries != NULL);
	first = entries[0];
	second = entries[0];
	first.leaf_name = "ab";
	first.mnemonic = "c";
	second.leaf_name = "a";
	second.mnemonic = "bc";
	EXPECT(tcti_scalar_operation_source_sha256(&first, first_sha256) == 0);
	EXPECT(tcti_scalar_operation_source_sha256(&second, second_sha256) == 0);
	EXPECT(strcmp(first_sha256, second_sha256) != 0);
	return 0;
}

static int reviewed_catalog_drift_is_rejected(void)
{
	struct tcti_scalar_operation_catalog_entry *copy;
	enum tcti_scalar_operation_catalog_error error;
	size_t count;

	copy = catalog_copy(&count);
	EXPECT(copy != NULL);
	copy[0].leaf_name = "ADR_reviewed_source_drift";
	EXPECT(tcti_scalar_operation_source_sha256(
		       &copy[0], copy[0].source_sha256) == 0);
	EXPECT(tcti_scalar_operation_catalog_validate(copy, count, &error) < 0);
	EXPECT(error ==
	       TCTI_SCALAR_OPERATION_CATALOG_SOURCE_MANIFEST_DRIFT);
	free(copy);
	return 0;
}

static int coarse_family_is_rejected(void)
{
	struct tcti_scalar_operation_catalog_entry *copy;
	enum tcti_scalar_operation_catalog_error error;
	size_t count;

	copy = catalog_copy(&count);
	EXPECT(copy != NULL);
	copy[0].operation_id = "ADD_*";
	EXPECT(tcti_scalar_operation_source_sha256(
		       &copy[0], copy[0].source_sha256) == 0);
	EXPECT(tcti_scalar_operation_catalog_validate(copy, count, &error) < 0);
	EXPECT(error == TCTI_SCALAR_OPERATION_CATALOG_COARSE_OPERATION);
	free(copy);
	return 0;
}

static int unknown_exact_operation_is_rejected(void)
{
	struct tcti_scalar_operation_catalog_entry *copy;
	enum tcti_scalar_operation_catalog_error error;
	size_t count;

	copy = catalog_copy(&count);
	EXPECT(copy != NULL);
	copy[0].operation_id = "ADD_future_semantics";
	EXPECT(tcti_scalar_operation_source_sha256(
		       &copy[0], copy[0].source_sha256) == 0);
	EXPECT(tcti_scalar_operation_catalog_validate(copy, count, &error) < 0);
	EXPECT(error == TCTI_SCALAR_OPERATION_CATALOG_UNKNOWN_OPERATION);
	free(copy);
	return 0;
}

static int changed_obligations_are_rejected(void)
{
	struct tcti_scalar_operation_catalog_entry *copy;
	enum tcti_scalar_operation_catalog_error error;
	size_t count;

	copy = catalog_copy(&count);
	EXPECT(copy != NULL);
	copy[0].obligations ^= TCTI_SCALAR_OBLIGATION_FLAGS;
	EXPECT(tcti_scalar_operation_catalog_validate(copy, count, &error) < 0);
	EXPECT(error == TCTI_SCALAR_OPERATION_CATALOG_INVALID_OBLIGATIONS);
	free(copy);
	return 0;
}

static int stale_candidate_provenance_is_rejected(void)
{
	struct tcti_scalar_operation_catalog_entry *copy;
	enum tcti_scalar_operation_catalog_error error;
	size_t count;

	copy = catalog_copy(&count);
	EXPECT(copy != NULL);
	copy[0].kunit_case = "coarse_scalar_family";
	EXPECT(tcti_scalar_operation_catalog_validate(copy, count, &error) < 0);
	EXPECT(error == TCTI_SCALAR_OPERATION_CATALOG_INVALID_PROVENANCE);
	free(copy);
	return 0;
}

static int candidate_source_digest_mutation_is_rejected(void)
{
	struct tcti_scalar_operation_catalog_entry *copy;
	enum tcti_scalar_operation_catalog_error error;
	char mutated_sha256[TCTI_SCALAR_OPERATION_SHA256_HEX_SIZE];
	size_t count;

	copy = catalog_copy(&count);
	EXPECT(copy != NULL);
	memcpy(mutated_sha256, copy[0].kunit_source_sha256,
	       sizeof(mutated_sha256));
	mutated_sha256[31] = mutated_sha256[31] == '0' ? '1' : '0';
	copy[0].kunit_source_sha256 = mutated_sha256;
	EXPECT(tcti_scalar_operation_catalog_validate(copy, count, &error) < 0);
	EXPECT(error == TCTI_SCALAR_OPERATION_CATALOG_INVALID_PROVENANCE);
	free(copy);
	return 0;
}

static int candidate_cannot_claim_proof(void)
{
	struct tcti_scalar_operation_catalog_entry *copy;
	enum tcti_scalar_operation_catalog_error error;
	size_t count;

	copy = catalog_copy(&count);
	EXPECT(copy != NULL);
	copy[0].proof_state =
		(enum tcti_scalar_operation_proof_state)1;
	EXPECT(tcti_scalar_operation_catalog_validate(copy, count, &error) < 0);
	EXPECT(error == TCTI_SCALAR_OPERATION_CATALOG_ALREADY_PROVED);
	free(copy);
	return 0;
}

int main(void)
{
	if (catalog_is_exact_and_unproved() ||
	    missing_binding_is_rejected() ||
	    missing_leaf_is_rejected() ||
	    duplicate_leaf_is_rejected() ||
	    duplicate_encoding_is_rejected() ||
	    stale_source_is_rejected() ||
	    one_byte_digest_mutation_is_rejected() ||
	    malformed_digest_lengths_are_rejected() ||
	    exact_field_lengths_are_hashed() ||
	    reviewed_catalog_drift_is_rejected() ||
	    coarse_family_is_rejected() ||
	    unknown_exact_operation_is_rejected() ||
	    changed_obligations_are_rejected() ||
	    stale_candidate_provenance_is_rejected() ||
	    candidate_source_digest_mutation_is_rejected() ||
	    candidate_cannot_claim_proof())
		return 1;
	puts("target scalar operation catalog tests: PASS");
	return 0;
}
