/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TARGET_SCALAR_OPERATION_CATALOG_H
#define ORLIX_TCTI_TARGET_SCALAR_OPERATION_CATALOG_H

#include <stddef.h>
#include <stdint.h>

#define TCTI_SCALAR_OPERATION_CATALOG_EXPECTED_COUNT 159U
#define TCTI_SCALAR_OPERATION_SHA256_HEX_SIZE 65U
#define TCTI_SCALAR_OPERATION_CATALOG_REVIEWED_SHA256 \
	"d3afb8ddd189d838781d2317c304b5236eb365f374f2314baa261e37e9d538e3"

enum tcti_scalar_operation_obligation {
	TCTI_SCALAR_OBLIGATION_DECODE = 1U << 0,
	TCTI_SCALAR_OBLIGATION_LEGAL_ENCODINGS = 1U << 1,
	TCTI_SCALAR_OBLIGATION_REJECTED_ENCODINGS = 1U << 2,
	TCTI_SCALAR_OBLIGATION_REGISTERS = 1U << 3,
	TCTI_SCALAR_OBLIGATION_PC = 1U << 4,
	TCTI_SCALAR_OBLIGATION_FLAGS = 1U << 5,
	TCTI_SCALAR_OBLIGATION_FAULTS = 1U << 6,
	TCTI_SCALAR_OBLIGATION_ORDERING = 1U << 7,
	TCTI_SCALAR_OBLIGATION_SYSTEM_STATE = 1U << 8,
	TCTI_SCALAR_OBLIGATION_STRUCTURED_EXIT = 1U << 9,
};

enum tcti_scalar_operation_proof_state {
	TCTI_SCALAR_OPERATION_UNPROVED = 0,
};

struct tcti_scalar_operation_catalog_entry {
	const char *leaf_name;
	const char *mnemonic;
	const char *operation_id;
	uint32_t encoding_mask;
	uint32_t encoding_pattern;
	const char *condition_tcnd_hex;
	char source_sha256[TCTI_SCALAR_OPERATION_SHA256_HEX_SIZE];
	uint32_t obligations;
	const char *kunit_source;
	const char *kunit_source_sha256;
	const char *kunit_object;
	const char *kunit_suite;
	const char *kunit_suite_symbol;
	const char *kunit_case_array;
	const char *kunit_case;
	enum tcti_scalar_operation_proof_state proof_state;
};

enum tcti_scalar_operation_catalog_error {
	TCTI_SCALAR_OPERATION_CATALOG_OK,
	TCTI_SCALAR_OPERATION_CATALOG_INVALID_ARGUMENT,
	TCTI_SCALAR_OPERATION_CATALOG_WRONG_COUNT,
	TCTI_SCALAR_OPERATION_CATALOG_MISSING_BINDING,
	TCTI_SCALAR_OPERATION_CATALOG_DUPLICATE_LEAF,
	TCTI_SCALAR_OPERATION_CATALOG_DUPLICATE_SOURCE_ENCODING,
	TCTI_SCALAR_OPERATION_CATALOG_UNKNOWN_OPERATION,
	TCTI_SCALAR_OPERATION_CATALOG_COARSE_OPERATION,
	TCTI_SCALAR_OPERATION_CATALOG_STALE_SOURCE,
	TCTI_SCALAR_OPERATION_CATALOG_INVALID_OBLIGATIONS,
	TCTI_SCALAR_OPERATION_CATALOG_INVALID_PROVENANCE,
	TCTI_SCALAR_OPERATION_CATALOG_ALREADY_PROVED,
	TCTI_SCALAR_OPERATION_CATALOG_SOURCE_MANIFEST_DRIFT,
};

const struct tcti_scalar_operation_catalog_entry *
tcti_scalar_operation_catalog_entries(size_t *count);

int tcti_scalar_operation_catalog_validate(
	const struct tcti_scalar_operation_catalog_entry *entries, size_t count,
	enum tcti_scalar_operation_catalog_error *error);

int tcti_scalar_operation_source_sha256(
	const struct tcti_scalar_operation_catalog_entry *entry,
	char output[TCTI_SCALAR_OPERATION_SHA256_HEX_SIZE]);
int tcti_scalar_operation_catalog_sha256(
	const struct tcti_scalar_operation_catalog_entry *entries, size_t count,
	char output[TCTI_SCALAR_OPERATION_SHA256_HEX_SIZE]);

#endif /* ORLIX_TCTI_TARGET_SCALAR_OPERATION_CATALOG_H */
