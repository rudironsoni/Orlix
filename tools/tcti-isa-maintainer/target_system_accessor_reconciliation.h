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
enum tcti_system_accessor_generic_leaf {
	TCTI_SYSTEM_ACCESSOR_LEAF_NONE,
	TCTI_SYSTEM_ACCESSOR_LEAF_MRS_RS_SYSTEMMOVE,
	TCTI_SYSTEM_ACCESSOR_LEAF_MSR_SI_PSTATE,
	TCTI_SYSTEM_ACCESSOR_LEAF_MSR_SR_SYSTEMMOVE,
	TCTI_SYSTEM_ACCESSOR_LEAF_SYS_CR_SYSTEMINSTRS,
	TCTI_SYSTEM_ACCESSOR_LEAF_SYSL_RC_SYSTEMINSTRS,
	TCTI_SYSTEM_ACCESSOR_LEAF_SYSP_CR_SYSPAIRINSTRS,
	TCTI_SYSTEM_ACCESSOR_LEAF_MSRR_SR_SYSTEMMOVEPR,
	TCTI_SYSTEM_ACCESSOR_LEAF_MRRS_RS_SYSTEMMOVEPR,
};

enum tcti_system_accessor_direction {
	TCTI_SYSTEM_ACCESSOR_DIRECTION_NONE,
	TCTI_SYSTEM_ACCESSOR_DIRECTION_READ,
	TCTI_SYSTEM_ACCESSOR_DIRECTION_WRITE,
	TCTI_SYSTEM_ACCESSOR_DIRECTION_EXECUTE,
};

enum tcti_system_accessor_disposition {
	TCTI_SYSTEM_ACCESSOR_MAPPED,
	TCTI_SYSTEM_ACCESSOR_RESERVED,
	TCTI_SYSTEM_ACCESSOR_PRIVILEGED,
	TCTI_SYSTEM_ACCESSOR_UNSUPPORTED,
	TCTI_SYSTEM_ACCESSOR_AMBIGUOUS,
	TCTI_SYSTEM_ACCESSOR_CONTRADICTORY,
	TCTI_SYSTEM_ACCESSOR_INVALID,
};

enum tcti_system_accessor_reconciliation_error {
	TCTI_SYSTEM_ACCESSOR_RECONCILIATION_OK,
	TCTI_SYSTEM_ACCESSOR_RECONCILIATION_INVALID_ARGUMENT,
	TCTI_SYSTEM_ACCESSOR_RECONCILIATION_NO_MEMORY,
	TCTI_SYSTEM_ACCESSOR_RECONCILIATION_RESERVED,
	TCTI_SYSTEM_ACCESSOR_RECONCILIATION_PRIVILEGED,
	TCTI_SYSTEM_ACCESSOR_RECONCILIATION_UNSUPPORTED,
	TCTI_SYSTEM_ACCESSOR_RECONCILIATION_AMBIGUOUS,
	TCTI_SYSTEM_ACCESSOR_RECONCILIATION_CONTRADICTORY,
	TCTI_SYSTEM_ACCESSOR_RECONCILIATION_INVALID,
};

struct tcti_system_accessor_reconciliation_entry {
	uint32_t accessor_index;
	uint32_t encoding_index;
	uint32_t condition_expression;
	uint32_t selector_count;
	enum tcti_system_accessor_generic_leaf generic_leaf;
	enum tcti_system_accessor_direction direction;
	enum tcti_system_accessor_disposition disposition;
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

struct tcti_system_accessor_reconciliation_census {
	size_t aarch64_accessors;
	size_t mapped;
	size_t reserved;
	size_t privileged;
	size_t unsupported;
	size_t ambiguous;
	size_t contradictory;
	size_t invalid;
};

struct tcti_system_accessor_reconciliation_result {
	struct tcti_system_accessor_reconciliation_entry *entries;
	size_t entry_count;
	struct tcti_system_accessor_reconciliation_census census;
};

/* Produces exactly one result row for every A64 SystemAccessor variant. */
enum tcti_system_accessor_reconciliation_error
	tcti_system_accessor_reconcile(
	const struct tcti_register_model *model,
	struct tcti_system_accessor_reconciliation_result *result);

/* Emits a deterministic C macro artifact, including blocking dispositions. */
enum tcti_system_accessor_reconciliation_error
tcti_system_accessor_reconciliation_emit(
	const struct tcti_register_model *model, FILE *output);

/* Fails closed unless every represented A64 accessor maps unambiguously. */
enum tcti_system_accessor_reconciliation_error
tcti_system_accessor_reconciliation_validate(
	const struct tcti_system_accessor_reconciliation_result *result);

void tcti_system_accessor_reconciliation_destroy(
	struct tcti_system_accessor_reconciliation_result *result);

const char *tcti_system_accessor_generic_leaf_name(
	enum tcti_system_accessor_generic_leaf leaf);
const char *tcti_system_accessor_reconciliation_error_name(
	enum tcti_system_accessor_reconciliation_error error);

#endif /* ORLIX_TCTI_TARGET_SYSTEM_ACCESSOR_RECONCILIATION_H */
