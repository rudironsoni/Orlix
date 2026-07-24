/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_RUNTIME_PROJECTION_H
#define ORLIX_TCTI_RUNTIME_PROJECTION_H

#ifdef TCTI_RUNTIME_PROJECTION_HOST_TEST
#include <stdbool.h>
#include <stddef.h>
#else
#include <linux/types.h>
#endif

#define TCTI_RUNTIME_PROJECTION_MAX_TARGET_LEAVES 4350U
#define TCTI_RUNTIME_PROJECTION_MAX_FEATURES_PER_LEAF 4350U
#define TCTI_RUNTIME_PROJECTION_MAX_CAPABILITY_MAPPINGS \
	(2U * sizeof(unsigned long) * 8U)

enum tcti_runtime_capability_word {
	TCTI_RUNTIME_CAPABILITY_HWCAP,
	TCTI_RUNTIME_CAPABILITY_HWCAP2,
};

/*
 * This is deliberately a source-leaf ledger, not the decoder-family summary.
 * A capability may be promoted only after every source leaf whose predicate
 * includes its Arm feature has its owning classification and proof recorded.
 */
enum tcti_runtime_leaf_classification {
	TCTI_RUNTIME_LEAF_UNCLASSIFIED = 0,
	TCTI_RUNTIME_LEAF_REQUIRED_EL0,
	TCTI_RUNTIME_LEAF_NON_EL0,
	TCTI_RUNTIME_LEAF_ARCH_UNDEFINED_OR_UNALLOCATED,
	TCTI_RUNTIME_LEAF_ALIAS_OR_DUPLICATE,
};

struct tcti_runtime_projection_leaf {
	const char *name;
	const char *const *features;
	size_t feature_count;
	enum tcti_runtime_leaf_classification classification;
	const char *proof;
	bool proved;
};

struct tcti_runtime_projection_ledger {
	const struct tcti_runtime_projection_leaf *leaves;
	size_t leaf_count;
	size_t target_leaf_count;
};

struct tcti_runtime_projection_profile {
	unsigned long hwcap;
	unsigned long hwcap2;
};

struct tcti_runtime_projection_capability {
	enum tcti_runtime_capability_word word;
	unsigned long bit;
	const char *feature;
};

struct tcti_runtime_projection_result {
	unsigned long advertised_hwcap;
	unsigned long advertised_hwcap2;
	unsigned long mapped_hwcap;
	unsigned long mapped_hwcap2;
	unsigned long proved_hwcap;
	unsigned long proved_hwcap2;
	unsigned long unmapped_advertised_hwcap;
	unsigned long unmapped_advertised_hwcap2;
	unsigned long advertised_without_proof_hwcap;
	unsigned long advertised_without_proof_hwcap2;
	size_t mapping_count;
	size_t unadvertised_mapping_count;
	size_t target_leaf_count;
	size_t classified_leaf_count;
	size_t unproved_leaf_count;
	size_t unadvertised_incomplete_feature_count;
};

/*
 * Audit a complete pinned-source ledger against a runtime capability profile.
 * The target count is supplied by the ledger and must exactly match the leaves
 * supplied. Unadvertised gaps remain visible in the result but do not make
 * Linux advertise a capability or erase target leaves.
 */
int tcti_runtime_projection_audit_ledger(
	const struct tcti_runtime_projection_ledger *ledger,
	const struct tcti_runtime_projection_profile *profile,
	const struct tcti_runtime_projection_capability *capabilities,
	size_t capability_count,
	struct tcti_runtime_projection_result *result);

/* Audit the actual asm/isa.h policy against the repository's complete ledger. */
int tcti_runtime_projection_audit(
	struct tcti_runtime_projection_result *result);

#endif /* ORLIX_TCTI_RUNTIME_PROJECTION_H */
