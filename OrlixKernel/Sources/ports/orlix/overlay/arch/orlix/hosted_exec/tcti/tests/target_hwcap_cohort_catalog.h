/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TARGET_HWCAP_COHORT_CATALOG_H
#define ORLIX_TCTI_TARGET_HWCAP_COHORT_CATALOG_H

#include <stdbool.h>
#include <stddef.h>

#include "target_inventory_import.h"
#include "target_ledger.h"

enum tcti_hwcap_catalog_word {
	TCTI_HWCAP_CATALOG_HWCAP,
	TCTI_HWCAP_CATALOG_HWCAP2,
};

struct tcti_hwcap_catalog_mapping {
	enum tcti_hwcap_catalog_word word;
	unsigned long bit;
	const char *bit_name;
	const char *feature;
};

enum tcti_hwcap_catalog_blocker {
	TCTI_HWCAP_CATALOG_PROVED,
	TCTI_HWCAP_CATALOG_UNCLASSIFIED,
	TCTI_HWCAP_CATALOG_MISSING_PROOF,
	TCTI_HWCAP_CATALOG_UNRESOLVED_PROOF,
};

struct tcti_hwcap_catalog_binding {
	size_t mapping_index;
	size_t leaf_index;
	enum tcti_target_ledger_class classification;
	enum tcti_hwcap_catalog_blocker blocker;
	const char *proof;
};

struct tcti_hwcap_catalog_cohort {
	size_t mapping_index;
	size_t first_binding;
	size_t binding_count;
	size_t blocker_count;
	bool authorized;
};

struct tcti_hwcap_cohort_catalog {
	struct tcti_hwcap_catalog_cohort *cohorts;
	size_t cohort_count;
	struct tcti_hwcap_catalog_binding *bindings;
	size_t binding_count;
	unsigned long mapped_hwcap;
	unsigned long mapped_hwcap2;
	unsigned long authorized_hwcap;
	unsigned long authorized_hwcap2;
	unsigned long advertised_hwcap;
	unsigned long advertised_hwcap2;
	unsigned long advertised_without_proof_hwcap;
	unsigned long advertised_without_proof_hwcap2;
};

enum tcti_hwcap_catalog_error {
	TCTI_HWCAP_CATALOG_OK,
	TCTI_HWCAP_CATALOG_INVALID_ARGUMENT,
	TCTI_HWCAP_CATALOG_BAD_SOURCE_COUNT,
	TCTI_HWCAP_CATALOG_SOURCE_MISMATCH,
	TCTI_HWCAP_CATALOG_BAD_CLASSIFICATION,
	TCTI_HWCAP_CATALOG_INVALID_MAPPING,
	TCTI_HWCAP_CATALOG_DUPLICATE_MAPPING,
	TCTI_HWCAP_CATALOG_MISSING_MAPPING,
	TCTI_HWCAP_CATALOG_EMPTY_COHORT,
	TCTI_HWCAP_CATALOG_INVALID_CONDITION,
	TCTI_HWCAP_CATALOG_NO_MEMORY,
};

const struct tcti_hwcap_catalog_mapping *
tcti_hwcap_catalog_current_mappings(size_t *count);
const struct tcti_target_ledger_source_row *
tcti_hwcap_catalog_pinned_source(size_t *count);
const char *tcti_hwcap_catalog_pinned_source_sha256(void);
unsigned long tcti_hwcap_catalog_current_hwcap(void);
unsigned long tcti_hwcap_catalog_current_hwcap2(void);

int tcti_hwcap_cohort_catalog_build_with_mappings(
	const struct tcti_target_inventory *inventory,
	const struct tcti_target_ledger_classification_row *classifications,
	size_t classification_count, const bool *proof_resolved,
	const struct tcti_hwcap_catalog_mapping *mappings,
	size_t mapping_count, unsigned long advertised_hwcap,
	unsigned long advertised_hwcap2,
	struct tcti_hwcap_cohort_catalog *catalog,
	enum tcti_hwcap_catalog_error *error);

int tcti_hwcap_cohort_catalog_build(
	const struct tcti_target_inventory *inventory,
	const struct tcti_target_ledger_classification_row *classifications,
	size_t classification_count, const bool *proof_resolved,
	struct tcti_hwcap_cohort_catalog *catalog,
	enum tcti_hwcap_catalog_error *error);

void tcti_hwcap_cohort_catalog_destroy(
	struct tcti_hwcap_cohort_catalog *catalog);

#endif
