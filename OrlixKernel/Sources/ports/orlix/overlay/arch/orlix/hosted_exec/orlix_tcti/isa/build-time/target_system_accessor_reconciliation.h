/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TARGET_SYSTEM_ACCESSOR_RECONCILIATION_H
#define ORLIX_TCTI_TARGET_SYSTEM_ACCESSOR_RECONCILIATION_H

#include "target_register_model.h"

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>

/*
 * Registers.json describes concrete system-access variants.  These records
 * bind those variants back to the generic direct instruction leaves without
 * changing the Instructions.json leaf denominator.
 */
enum orlix_tcti_system_accessor_generic_leaf {
	ORLIX_TCTI_SYSTEM_ACCESSOR_LEAF_NONE,
	ORLIX_TCTI_SYSTEM_ACCESSOR_LEAF_MRS_RS_SYSTEMMOVE,
	ORLIX_TCTI_SYSTEM_ACCESSOR_LEAF_MSR_SI_PSTATE,
	ORLIX_TCTI_SYSTEM_ACCESSOR_LEAF_MSR_SR_SYSTEMMOVE,
	ORLIX_TCTI_SYSTEM_ACCESSOR_LEAF_SYS_CR_SYSTEMINSTRS,
	ORLIX_TCTI_SYSTEM_ACCESSOR_LEAF_SYSL_RC_SYSTEMINSTRS,
	ORLIX_TCTI_SYSTEM_ACCESSOR_LEAF_SYSP_CR_SYSPAIRINSTRS,
	ORLIX_TCTI_SYSTEM_ACCESSOR_LEAF_MSRR_SR_SYSTEMMOVEPR,
	ORLIX_TCTI_SYSTEM_ACCESSOR_LEAF_MRRS_RS_SYSTEMMOVEPR,
};

enum orlix_tcti_system_accessor_direction {
	ORLIX_TCTI_SYSTEM_ACCESSOR_DIRECTION_NONE,
	ORLIX_TCTI_SYSTEM_ACCESSOR_DIRECTION_READ,
	ORLIX_TCTI_SYSTEM_ACCESSOR_DIRECTION_WRITE,
	ORLIX_TCTI_SYSTEM_ACCESSOR_DIRECTION_EXECUTE,
};

enum orlix_tcti_system_accessor_disposition {
	ORLIX_TCTI_SYSTEM_ACCESSOR_MAPPED,
	ORLIX_TCTI_SYSTEM_ACCESSOR_RESERVED,
	ORLIX_TCTI_SYSTEM_ACCESSOR_PRIVILEGED,
	ORLIX_TCTI_SYSTEM_ACCESSOR_UNSUPPORTED,
	ORLIX_TCTI_SYSTEM_ACCESSOR_AMBIGUOUS,
	ORLIX_TCTI_SYSTEM_ACCESSOR_CONTRADICTORY,
	ORLIX_TCTI_SYSTEM_ACCESSOR_INVALID,
};

enum orlix_tcti_system_accessor_reconciliation_error {
	ORLIX_TCTI_SYSTEM_ACCESSOR_RECONCILIATION_OK,
	ORLIX_TCTI_SYSTEM_ACCESSOR_RECONCILIATION_INVALID_ARGUMENT,
	ORLIX_TCTI_SYSTEM_ACCESSOR_RECONCILIATION_NO_MEMORY,
	ORLIX_TCTI_SYSTEM_ACCESSOR_RECONCILIATION_RESERVED,
	ORLIX_TCTI_SYSTEM_ACCESSOR_RECONCILIATION_PRIVILEGED,
	ORLIX_TCTI_SYSTEM_ACCESSOR_RECONCILIATION_UNSUPPORTED,
	ORLIX_TCTI_SYSTEM_ACCESSOR_RECONCILIATION_AMBIGUOUS,
	ORLIX_TCTI_SYSTEM_ACCESSOR_RECONCILIATION_CONTRADICTORY,
	ORLIX_TCTI_SYSTEM_ACCESSOR_RECONCILIATION_INVALID,
};

struct orlix_tcti_system_accessor_reconciliation_entry {
	uint32_t accessor_index;
	uint32_t encoding_index;
	uint32_t condition_expression;
	uint32_t selector_count;
	enum orlix_tcti_system_accessor_generic_leaf generic_leaf;
	enum orlix_tcti_system_accessor_direction direction;
	enum orlix_tcti_system_accessor_disposition disposition;
	/* Exact source identities, not merely a count or parser-local index. */
	uint64_t selector_identity;
	uint64_t condition_identity;
	const char *accessor_name;
	size_t accessor_source_offset;
	size_t accessor_source_length;
	size_t encoding_source_offset;
	size_t encoding_source_length;
	size_t condition_source_offset;
	size_t condition_source_length;
};

struct orlix_tcti_system_accessor_reconciliation_census {
	size_t aarch64_accessors;
	size_t mapped;
	size_t reserved;
	size_t privileged;
	size_t unsupported;
	size_t ambiguous;
	size_t contradictory;
	size_t invalid;
};

struct orlix_tcti_system_accessor_reconciliation_result {
	struct orlix_tcti_system_accessor_reconciliation_entry *entries;
	size_t entry_count;
	struct orlix_tcti_system_accessor_reconciliation_census census;
};

/* Produces exactly one result row for every A64 SystemAccessor variant. */
enum orlix_tcti_system_accessor_reconciliation_error
	orlix_tcti_system_accessor_reconcile(
	const struct orlix_tcti_register_model *model,
	struct orlix_tcti_system_accessor_reconciliation_result *result);

/* Emits a deterministic C macro artifact, including blocking dispositions. */
enum orlix_tcti_system_accessor_reconciliation_error
orlix_tcti_system_accessor_reconciliation_emit(
	const struct orlix_tcti_register_model *model, FILE *output);

/* Fails closed unless every represented A64 accessor maps unambiguously. */
enum orlix_tcti_system_accessor_reconciliation_error
orlix_tcti_system_accessor_reconciliation_validate(
	const struct orlix_tcti_system_accessor_reconciliation_result *result);

void orlix_tcti_system_accessor_reconciliation_destroy(
	struct orlix_tcti_system_accessor_reconciliation_result *result);

const char *orlix_tcti_system_accessor_generic_leaf_name(
	enum orlix_tcti_system_accessor_generic_leaf leaf);
const char *orlix_tcti_system_accessor_reconciliation_error_name(
	enum orlix_tcti_system_accessor_reconciliation_error error);

#endif /* ORLIX_TCTI_TARGET_SYSTEM_ACCESSOR_RECONCILIATION_H */
