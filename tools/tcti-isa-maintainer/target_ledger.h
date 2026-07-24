/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TARGET_LEDGER_H
#define ORLIX_TCTI_TARGET_LEDGER_H

#include <stddef.h>
#include <stdint.h>

#include "target_inventory_import.h"
#include "target_proof_registry.h"

enum tcti_target_ledger_class {
	TCTI_TARGET_LEDGER_UNCLASSIFIED,
	TCTI_TARGET_LEDGER_REQUIRED_EL0,
	TCTI_TARGET_LEDGER_NON_EL0,
	TCTI_TARGET_LEDGER_ARCH_UNDEFINED_OR_UNALLOCATED,
	TCTI_TARGET_LEDGER_ALIAS_OR_DUPLICATE,
};

enum tcti_target_ledger_relation {
	TCTI_A64_TARGET_RELATION_NONE,
	TCTI_A64_TARGET_RELATION_ALIAS,
	TCTI_A64_TARGET_RELATION_DUPLICATE,
};

struct tcti_target_ledger_source_row {
	uint32_t ordinal;
	const char *name;
	const char *mnemonic;
	const char *operation_id;
	uint32_t mask;
	uint32_t pattern;
	const char *condition_tcnd_hex;
};

struct tcti_target_ledger_classification_row {
	const char *name;
	enum tcti_target_ledger_class classification;
	enum tcti_target_ledger_relation relation;
	const char *canonical_name;
	const char *evidence;
	const char *proof;
	const char *relation_evidence;
	const char *relation_source;
	const char *relation_source_sha256;
};

enum tcti_target_ledger_error {
	TCTI_TARGET_LEDGER_OK,
	TCTI_TARGET_LEDGER_BAD_COUNT,
	TCTI_TARGET_LEDGER_BAD_ORDINAL,
	TCTI_TARGET_LEDGER_DUPLICATE_SOURCE_NAME,
	TCTI_TARGET_LEDGER_DUPLICATE_CLASSIFICATION_NAME,
	TCTI_TARGET_LEDGER_MISSING_CLASSIFICATION,
	TCTI_TARGET_LEDGER_STALE_CLASSIFICATION,
	TCTI_TARGET_LEDGER_ERROR_UNCLASSIFIED,
	TCTI_TARGET_LEDGER_INVALID_CLASSIFICATION,
	TCTI_TARGET_LEDGER_INVALID_RELATION,
	TCTI_TARGET_LEDGER_MISSING_CANONICAL,
	TCTI_TARGET_LEDGER_UNKNOWN_CANONICAL,
	TCTI_TARGET_LEDGER_SELF_CANONICAL,
	TCTI_TARGET_LEDGER_NONCANONICAL_TARGET,
	TCTI_TARGET_LEDGER_UNEXPECTED_CANONICAL,
	TCTI_TARGET_LEDGER_CONDITION_MISMATCH,
	TCTI_TARGET_LEDGER_MISSING_EVIDENCE,
	TCTI_TARGET_LEDGER_MISSING_PROOF,
	TCTI_TARGET_LEDGER_STALE_SOURCE_FACT,
	TCTI_TARGET_LEDGER_STALE_SOURCE_CONDITION,
	TCTI_TARGET_LEDGER_INVALID_PROOF_REGISTRY,
	TCTI_TARGET_LEDGER_UNKNOWN_PROOF,
	TCTI_TARGET_LEDGER_PROOF_CLASSIFICATION_MISMATCH,
	TCTI_TARGET_LEDGER_PROOF_FAMILY_MISMATCH,
	TCTI_TARGET_LEDGER_PROOF_INSUFFICIENT_OBLIGATIONS,
	TCTI_TARGET_LEDGER_UNKNOWN_PROOF_FAMILY_REQUIREMENTS,
	TCTI_TARGET_LEDGER_PROOF_BINDING_MISMATCH,
	TCTI_TARGET_LEDGER_REQUIRED_OPERATION_PROOF_COUNT,
	TCTI_TARGET_LEDGER_ALIAS_PROOF_MISMATCH,
	TCTI_TARGET_LEDGER_UNKNOWN_PROOF_BINDING_LEAF,
	TCTI_TARGET_LEDGER_STALE_PROOF_BINDING,
	TCTI_TARGET_LEDGER_PROOF_BINDING_OPERATION_MISMATCH,
	TCTI_TARGET_LEDGER_PROOF_BINDING_CLASSIFICATION_MISMATCH,
	TCTI_TARGET_LEDGER_MISSING_RELATION_EVIDENCE,
	TCTI_TARGET_LEDGER_INVALID_RELATION_EVIDENCE,
	TCTI_TARGET_LEDGER_UNKNOWN_OPERATION_REQUIREMENTS,
};

struct tcti_target_ledger_result {
	enum tcti_target_ledger_error first_error;
	size_t first_error_index;
	size_t errors;
	size_t source_rows;
	size_t classified_rows;
	size_t unclassified_rows;
	size_t proof_gaps;
};

int tcti_target_ledger_validate(const struct tcti_target_ledger_source_row *source,
				size_t source_count,
				const struct tcti_target_ledger_classification_row *classification,
				size_t classification_count,
				size_t required_count,
				const struct tcti_target_proof_registry_entry *proof_registry,
				size_t proof_registry_count,
				struct tcti_target_ledger_result *result);
int tcti_target_ledger_verify_import(const struct tcti_target_ledger_source_row *source,
				     size_t count,
				     const struct tcti_target_inventory *inventory,
				     struct tcti_target_ledger_result *result);

const struct tcti_target_ledger_source_row *tcti_target_ledger_source(size_t *count);
const struct tcti_target_ledger_classification_row *tcti_target_ledger_classification(size_t *count);

#endif /* ORLIX_TCTI_TARGET_LEDGER_H */
