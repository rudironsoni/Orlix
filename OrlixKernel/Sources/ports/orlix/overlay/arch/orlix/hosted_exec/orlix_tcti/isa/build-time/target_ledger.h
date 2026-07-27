/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TARGET_LEDGER_H
#define ORLIX_TCTI_TARGET_LEDGER_H

#include <stddef.h>
#include <stdint.h>

#include "target_inventory_import.h"
#include "target_proof_registry.h"

enum orlix_tcti_target_ledger_class {
	ORLIX_TCTI_TARGET_LEDGER_UNCLASSIFIED,
	ORLIX_TCTI_TARGET_LEDGER_REQUIRED_EL0,
	ORLIX_TCTI_TARGET_LEDGER_NON_EL0,
	ORLIX_TCTI_TARGET_LEDGER_ARCH_UNDEFINED_OR_UNALLOCATED,
	ORLIX_TCTI_TARGET_LEDGER_ALIAS_OR_DUPLICATE,
};

enum orlix_tcti_target_ledger_relation {
	ORLIX_TCTI_A64_TARGET_RELATION_NONE,
	ORLIX_TCTI_A64_TARGET_RELATION_ALIAS,
	ORLIX_TCTI_A64_TARGET_RELATION_DUPLICATE,
};

struct orlix_tcti_target_ledger_source_row {
	uint32_t ordinal;
	const char *name;
	const char *mnemonic;
	const char *operation_id;
	uint32_t mask;
	uint32_t pattern;
	const char *condition_tcnd_hex;
};

struct orlix_tcti_target_ledger_classification_row {
	const char *name;
	enum orlix_tcti_target_ledger_class classification;
	enum orlix_tcti_target_ledger_relation relation;
	const char *canonical_name;
	const char *evidence;
	const char *proof;
	const char *relation_evidence;
	const char *relation_source;
	const char *relation_source_sha256;
};

enum orlix_tcti_target_ledger_error {
	ORLIX_TCTI_TARGET_LEDGER_OK,
	ORLIX_TCTI_TARGET_LEDGER_BAD_COUNT,
	ORLIX_TCTI_TARGET_LEDGER_BAD_ORDINAL,
	ORLIX_TCTI_TARGET_LEDGER_DUPLICATE_SOURCE_NAME,
	ORLIX_TCTI_TARGET_LEDGER_DUPLICATE_CLASSIFICATION_NAME,
	ORLIX_TCTI_TARGET_LEDGER_MISSING_CLASSIFICATION,
	ORLIX_TCTI_TARGET_LEDGER_STALE_CLASSIFICATION,
	ORLIX_TCTI_TARGET_LEDGER_ERROR_UNCLASSIFIED,
	ORLIX_TCTI_TARGET_LEDGER_INVALID_CLASSIFICATION,
	ORLIX_TCTI_TARGET_LEDGER_INVALID_RELATION,
	ORLIX_TCTI_TARGET_LEDGER_MISSING_CANONICAL,
	ORLIX_TCTI_TARGET_LEDGER_UNKNOWN_CANONICAL,
	ORLIX_TCTI_TARGET_LEDGER_SELF_CANONICAL,
	ORLIX_TCTI_TARGET_LEDGER_NONCANONICAL_TARGET,
	ORLIX_TCTI_TARGET_LEDGER_UNEXPECTED_CANONICAL,
	ORLIX_TCTI_TARGET_LEDGER_CONDITION_MISMATCH,
	ORLIX_TCTI_TARGET_LEDGER_MISSING_EVIDENCE,
	ORLIX_TCTI_TARGET_LEDGER_MISSING_PROOF,
	ORLIX_TCTI_TARGET_LEDGER_STALE_SOURCE_FACT,
	ORLIX_TCTI_TARGET_LEDGER_STALE_SOURCE_CONDITION,
	ORLIX_TCTI_TARGET_LEDGER_INVALID_PROOF_REGISTRY,
	ORLIX_TCTI_TARGET_LEDGER_UNKNOWN_PROOF,
	ORLIX_TCTI_TARGET_LEDGER_PROOF_CLASSIFICATION_MISMATCH,
	ORLIX_TCTI_TARGET_LEDGER_PROOF_FAMILY_MISMATCH,
	ORLIX_TCTI_TARGET_LEDGER_PROOF_INSUFFICIENT_OBLIGATIONS,
	ORLIX_TCTI_TARGET_LEDGER_UNKNOWN_PROOF_FAMILY_REQUIREMENTS,
	ORLIX_TCTI_TARGET_LEDGER_PROOF_BINDING_MISMATCH,
	ORLIX_TCTI_TARGET_LEDGER_REQUIRED_OPERATION_PROOF_COUNT,
	ORLIX_TCTI_TARGET_LEDGER_ALIAS_PROOF_MISMATCH,
	ORLIX_TCTI_TARGET_LEDGER_UNKNOWN_PROOF_BINDING_LEAF,
	ORLIX_TCTI_TARGET_LEDGER_STALE_PROOF_BINDING,
	ORLIX_TCTI_TARGET_LEDGER_PROOF_BINDING_OPERATION_MISMATCH,
	ORLIX_TCTI_TARGET_LEDGER_PROOF_BINDING_CLASSIFICATION_MISMATCH,
	ORLIX_TCTI_TARGET_LEDGER_MISSING_RELATION_EVIDENCE,
	ORLIX_TCTI_TARGET_LEDGER_INVALID_RELATION_EVIDENCE,
	ORLIX_TCTI_TARGET_LEDGER_UNKNOWN_OPERATION_REQUIREMENTS,
};

struct orlix_tcti_target_ledger_result {
	enum orlix_tcti_target_ledger_error first_error;
	size_t first_error_index;
	size_t errors;
	size_t source_rows;
	size_t classified_rows;
	size_t unclassified_rows;
	size_t proof_gaps;
};

int orlix_tcti_target_ledger_validate(const struct orlix_tcti_target_ledger_source_row *source,
				size_t source_count,
				const struct orlix_tcti_target_ledger_classification_row *classification,
				size_t classification_count,
				size_t required_count,
				const struct orlix_tcti_target_proof_registry_entry *proof_registry,
				size_t proof_registry_count,
				struct orlix_tcti_target_ledger_result *result);
int orlix_tcti_target_ledger_verify_import(const struct orlix_tcti_target_ledger_source_row *source,
				     size_t count,
				     const struct orlix_tcti_target_inventory *inventory,
				     struct orlix_tcti_target_ledger_result *result);

const struct orlix_tcti_target_ledger_source_row *orlix_tcti_target_ledger_source(size_t *count);
const struct orlix_tcti_target_ledger_classification_row *orlix_tcti_target_ledger_classification(size_t *count);

#endif /* ORLIX_TCTI_TARGET_LEDGER_H */
