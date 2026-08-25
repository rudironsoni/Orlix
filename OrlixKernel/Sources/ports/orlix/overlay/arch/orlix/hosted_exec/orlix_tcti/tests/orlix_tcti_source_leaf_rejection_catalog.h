/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_SOURCE_LEAF_REJECTION_CATALOG_H
#define ORLIX_TCTI_SOURCE_LEAF_REJECTION_CATALOG_H

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/types.h>

/*
 * This is deliberately a test-local view of the checked, pinned AARCHMRS
 * source manifest.  The rejection tests select their rows through the proof
 * binding IDs, then obtain every encoding field from that source artifact.
 * Keep no encoding tuple here: a source-manifest update must change the test
 * observation automatically.
 */
struct orlix_tcti_source_leaf_rejection {
	u32 ordinal;
	const char *name;
	const char *operation;
	u32 mask;
	u32 pattern;
};

struct orlix_tcti_source_leaf_manifest_row {
	u32 ordinal;
	const char *name;
	const char *operation;
	u32 mask;
	u32 pattern;
};

#define ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW(ordinal, name, mnemonic, operation, \
					     mask, pattern, condition, source_offset, \
					     source_length) \
	{ ordinal, name, operation, mask, pattern },
#define ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE(...)
static const struct orlix_tcti_source_leaf_manifest_row
	orlix_tcti_source_leaf_manifest_rows[] = {
#include "../isa/source_manifest.def"
};
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW

struct orlix_tcti_source_leaf_proof_binding {
	u32 ordinal;
	const char *proof_id;
};

#define ORLIX_TCTI_A64_SOURCE_BOUND_PROOF(ordinal, proof_id) \
	{ ordinal, proof_id },
static const struct orlix_tcti_source_leaf_proof_binding
	orlix_tcti_source_leaf_proof_bindings[] = {
#include "../isa/source_bound_proof.def"
};
#undef ORLIX_TCTI_A64_SOURCE_BOUND_PROOF

static inline bool
orlix_tcti_source_leaf_is_unconditional_el0_rejection(const char *proof_id)
{
	/* Selector-dependent generic encodings have separate partition tests. */
	static const char * const proof_ids[] = {
		"kunit:source-leaf-udf-undefined",
		"kunit:source-leaf-hvc-non-el0",
		"kunit:source-leaf-smc-non-el0",
		"kunit:source-leaf-dcps1-non-el0",
		"kunit:source-leaf-dcps2-non-el0",
		"kunit:source-leaf-dcps3-non-el0",
		"kunit:source-leaf-tenter-non-el0",
		"kunit:source-leaf-bc-cond-non-el0",
		"kunit:source-leaf-cbbcc-regs-non-el0",
		"kunit:source-leaf-cbhcc-regs-non-el0",
		"kunit:source-leaf-cbcc-regs-non-el0",
		"kunit:source-leaf-cbcc-imm-non-el0",
		"kunit:source-leaf-ctz-non-el0",
		"kunit:source-leaf-cnt-non-el0",
		"kunit:source-leaf-abs-non-el0",
		"kunit:source-leaf-luti4-non-el0",
		"kunit:source-leaf-luti2-non-el0",
		"kunit:source-leaf-sqrdmlah-vec-non-el0",
		"kunit:source-leaf-sqrdmlsh-vec-non-el0",
		"kunit:source-leaf-sqrdmlah-elt-non-el0",
		"kunit:source-leaf-sqrdmlsh-elt-non-el0",
		"kunit:source-leaf-sdot-vec-non-el0",
		"kunit:source-leaf-usdot-vec-non-el0",
		"kunit:source-leaf-udot-vec-non-el0",
		"kunit:source-leaf-sdot-elt-non-el0",
		"kunit:source-leaf-sudot-elt-non-el0",
		"kunit:source-leaf-usdot-elt-non-el0",
		"kunit:source-leaf-udot-elt-non-el0",
		"kunit:source-leaf-smmla-non-el0",
		"kunit:source-leaf-usmmla-non-el0",
		"kunit:source-leaf-ummla-non-el0",

		"kunit:source-leaf-eret-non-el0",
		"kunit:source-leaf-ereta-non-el0",
		"kunit:source-leaf-texit-non-el0",
		"kunit:source-leaf-drps-non-el0",
	};
	size_t index;

	for (index = 0; index < ARRAY_SIZE(proof_ids); index++)
		if (!strcmp(proof_id, proof_ids[index]))
			return true;

	return false;
}

static inline const struct orlix_tcti_source_leaf_manifest_row *
orlix_tcti_source_leaf_manifest_row(u32 ordinal)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_source_leaf_manifest_rows);
	     index++)
		if (orlix_tcti_source_leaf_manifest_rows[index].ordinal == ordinal)
			return &orlix_tcti_source_leaf_manifest_rows[index];

	return NULL;
}

static inline size_t orlix_tcti_source_leaf_rejection_count(void)
{
	size_t index;
	size_t count = 0;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_source_leaf_proof_bindings);
	     index++)
		if (orlix_tcti_source_leaf_is_unconditional_el0_rejection(
			orlix_tcti_source_leaf_proof_bindings[index].proof_id))
			count++;

	return count;
}

static inline const struct orlix_tcti_source_leaf_rejection *
orlix_tcti_source_leaf_rejection_at(size_t rejection_index,
			      struct orlix_tcti_source_leaf_rejection *entry)
{
	const struct orlix_tcti_source_leaf_manifest_row *source;
	size_t index;
	size_t count = 0;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_source_leaf_proof_bindings);
	     index++) {
		const struct orlix_tcti_source_leaf_proof_binding *binding =
			&orlix_tcti_source_leaf_proof_bindings[index];

		if (!orlix_tcti_source_leaf_is_unconditional_el0_rejection(
			binding->proof_id))
			continue;
		if (count++ != rejection_index)
			continue;
		source = orlix_tcti_source_leaf_manifest_row(binding->ordinal);
		if (!source)
			return NULL;
		*entry = (struct orlix_tcti_source_leaf_rejection) {
			.ordinal = source->ordinal,
			.name = source->name,
			.operation = source->operation,
			.mask = source->mask,
			.pattern = source->pattern,
		};
		return entry;
	}

	return NULL;
}

#endif /* ORLIX_TCTI_SOURCE_LEAF_REJECTION_CATALOG_H */
