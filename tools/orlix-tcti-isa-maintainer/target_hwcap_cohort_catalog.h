/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_TARGET_HWCAP_COHORT_CATALOG_H
#define ORLIX_TCTI_TARGET_HWCAP_COHORT_CATALOG_H

#include <stdbool.h>
#include <stddef.h>

#include "target_inventory_import.h"
#include "target_ledger.h"

enum orlix_tcti_hwcap_catalog_word {
	ORLIX_TCTI_HWCAP_CATALOG_HWCAP,
	ORLIX_TCTI_HWCAP_CATALOG_HWCAP2,
};

struct orlix_tcti_hwcap_catalog_mapping {
	enum orlix_tcti_hwcap_catalog_word word;
	unsigned long bit;
	const char *bit_name;
	const char *feature;
};

enum orlix_tcti_hwcap_catalog_blocker {
	ORLIX_TCTI_HWCAP_CATALOG_PROVED,
	ORLIX_TCTI_HWCAP_CATALOG_UNCLASSIFIED,
	ORLIX_TCTI_HWCAP_CATALOG_MISSING_PROOF,
	ORLIX_TCTI_HWCAP_CATALOG_UNRESOLVED_PROOF,
};

struct orlix_tcti_hwcap_catalog_binding {
	size_t mapping_index;
	size_t leaf_index;
	enum orlix_tcti_target_ledger_class classification;
	enum orlix_tcti_hwcap_catalog_blocker blocker;
	const char *proof;
};

struct orlix_tcti_hwcap_catalog_cohort {
	size_t mapping_index;
	size_t first_binding;
	size_t binding_count;
	size_t blocker_count;
	bool authorized;
};

struct orlix_tcti_hwcap_cohort_catalog {
	struct orlix_tcti_hwcap_catalog_cohort *cohorts;
	size_t cohort_count;
	struct orlix_tcti_hwcap_catalog_binding *bindings;
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

enum orlix_tcti_hwcap_catalog_error {
	ORLIX_TCTI_HWCAP_CATALOG_OK,
	ORLIX_TCTI_HWCAP_CATALOG_INVALID_ARGUMENT,
	ORLIX_TCTI_HWCAP_CATALOG_BAD_SOURCE_COUNT,
	ORLIX_TCTI_HWCAP_CATALOG_SOURCE_MISMATCH,
	ORLIX_TCTI_HWCAP_CATALOG_BAD_CLASSIFICATION,
	ORLIX_TCTI_HWCAP_CATALOG_INVALID_MAPPING,
	ORLIX_TCTI_HWCAP_CATALOG_DUPLICATE_MAPPING,
	ORLIX_TCTI_HWCAP_CATALOG_MISSING_MAPPING,
	ORLIX_TCTI_HWCAP_CATALOG_EMPTY_COHORT,
	ORLIX_TCTI_HWCAP_CATALOG_INVALID_CONDITION,
	ORLIX_TCTI_HWCAP_CATALOG_NO_MEMORY,
};

const struct orlix_tcti_hwcap_catalog_mapping *
orlix_tcti_hwcap_catalog_current_mappings(size_t *count);
const struct orlix_tcti_target_ledger_source_row *
orlix_tcti_hwcap_catalog_pinned_source(size_t *count);
const char *orlix_tcti_hwcap_catalog_pinned_source_sha256(void);
unsigned long orlix_tcti_hwcap_catalog_current_hwcap(void);
unsigned long orlix_tcti_hwcap_catalog_current_hwcap2(void);

int orlix_tcti_hwcap_cohort_catalog_build_with_mappings(
	const struct orlix_tcti_target_inventory *inventory,
	const struct orlix_tcti_target_ledger_classification_row *classifications,
	size_t classification_count, const bool *proof_resolved,
	const struct orlix_tcti_hwcap_catalog_mapping *mappings,
	size_t mapping_count, unsigned long advertised_hwcap,
	unsigned long advertised_hwcap2,
	struct orlix_tcti_hwcap_cohort_catalog *catalog,
	enum orlix_tcti_hwcap_catalog_error *error);

int orlix_tcti_hwcap_cohort_catalog_build(
	const struct orlix_tcti_target_inventory *inventory,
	const struct orlix_tcti_target_ledger_classification_row *classifications,
	size_t classification_count, const bool *proof_resolved,
	struct orlix_tcti_hwcap_cohort_catalog *catalog,
	enum orlix_tcti_hwcap_catalog_error *error);

void orlix_tcti_hwcap_cohort_catalog_destroy(
	struct orlix_tcti_hwcap_cohort_catalog *catalog);

#endif
