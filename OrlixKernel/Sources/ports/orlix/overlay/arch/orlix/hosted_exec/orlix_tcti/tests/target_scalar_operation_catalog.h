/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TARGET_SCALAR_OPERATION_CATALOG_H
#define ORLIX_TCTI_TARGET_SCALAR_OPERATION_CATALOG_H

#include <stddef.h>
#include <stdint.h>

#define ORLIX_TCTI_SCALAR_OPERATION_CATALOG_EXPECTED_COUNT 159U
#define ORLIX_TCTI_SCALAR_OPERATION_SHA256_HEX_SIZE 65U
#define ORLIX_TCTI_SCALAR_OPERATION_CATALOG_REVIEWED_SHA256 \
	"2530bd1a0a46f046f0068d2cc6c676e456657fe46f4e70ce8376a03c104ab2a0"

enum orlix_tcti_scalar_operation_obligation {
	ORLIX_TCTI_SCALAR_OBLIGATION_DECODE = 1U << 0,
	ORLIX_TCTI_SCALAR_OBLIGATION_LEGAL_ENCODINGS = 1U << 1,
	ORLIX_TCTI_SCALAR_OBLIGATION_REJECTED_ENCODINGS = 1U << 2,
	ORLIX_TCTI_SCALAR_OBLIGATION_REGISTERS = 1U << 3,
	ORLIX_TCTI_SCALAR_OBLIGATION_PC = 1U << 4,
	ORLIX_TCTI_SCALAR_OBLIGATION_FLAGS = 1U << 5,
	ORLIX_TCTI_SCALAR_OBLIGATION_FAULTS = 1U << 6,
	ORLIX_TCTI_SCALAR_OBLIGATION_ORDERING = 1U << 7,
	ORLIX_TCTI_SCALAR_OBLIGATION_SYSTEM_STATE = 1U << 8,
	ORLIX_TCTI_SCALAR_OBLIGATION_STRUCTURED_EXIT = 1U << 9,
};

enum orlix_tcti_scalar_operation_proof_state {
	ORLIX_TCTI_SCALAR_OPERATION_UNPROVED = 0,
};

struct orlix_tcti_scalar_operation_catalog_entry {
	const char *leaf_name;
	const char *mnemonic;
	const char *operation_id;
	uint32_t encoding_mask;
	uint32_t encoding_pattern;
	const char *condition_tcnd_hex;
	char source_sha256[ORLIX_TCTI_SCALAR_OPERATION_SHA256_HEX_SIZE];
	uint32_t obligations;
	const char *kunit_source;
	const char *kunit_source_sha256;
	const char *kunit_object;
	const char *kunit_suite;
	const char *kunit_suite_symbol;
	const char *kunit_case_array;
	const char *kunit_case;
	enum orlix_tcti_scalar_operation_proof_state proof_state;
};

enum orlix_tcti_scalar_operation_catalog_error {
	ORLIX_TCTI_SCALAR_OPERATION_CATALOG_OK,
	ORLIX_TCTI_SCALAR_OPERATION_CATALOG_INVALID_ARGUMENT,
	ORLIX_TCTI_SCALAR_OPERATION_CATALOG_WRONG_COUNT,
	ORLIX_TCTI_SCALAR_OPERATION_CATALOG_MISSING_BINDING,
	ORLIX_TCTI_SCALAR_OPERATION_CATALOG_DUPLICATE_LEAF,
	ORLIX_TCTI_SCALAR_OPERATION_CATALOG_DUPLICATE_SOURCE_ENCODING,
	ORLIX_TCTI_SCALAR_OPERATION_CATALOG_UNKNOWN_OPERATION,
	ORLIX_TCTI_SCALAR_OPERATION_CATALOG_COARSE_OPERATION,
	ORLIX_TCTI_SCALAR_OPERATION_CATALOG_STALE_SOURCE,
	ORLIX_TCTI_SCALAR_OPERATION_CATALOG_INVALID_OBLIGATIONS,
	ORLIX_TCTI_SCALAR_OPERATION_CATALOG_INVALID_PROVENANCE,
	ORLIX_TCTI_SCALAR_OPERATION_CATALOG_ALREADY_PROVED,
	ORLIX_TCTI_SCALAR_OPERATION_CATALOG_SOURCE_MANIFEST_DRIFT,
};

const struct orlix_tcti_scalar_operation_catalog_entry *
orlix_tcti_scalar_operation_catalog_entries(size_t *count);

int orlix_tcti_scalar_operation_catalog_validate(
	const struct orlix_tcti_scalar_operation_catalog_entry *entries, size_t count,
	enum orlix_tcti_scalar_operation_catalog_error *error);

int orlix_tcti_scalar_operation_source_sha256(
	const struct orlix_tcti_scalar_operation_catalog_entry *entry,
	char output[ORLIX_TCTI_SCALAR_OPERATION_SHA256_HEX_SIZE]);
int orlix_tcti_scalar_operation_catalog_sha256(
	const struct orlix_tcti_scalar_operation_catalog_entry *entries, size_t count,
	char output[ORLIX_TCTI_SCALAR_OPERATION_SHA256_HEX_SIZE]);

#endif /* ORLIX_TCTI_TARGET_SCALAR_OPERATION_CATALOG_H */
