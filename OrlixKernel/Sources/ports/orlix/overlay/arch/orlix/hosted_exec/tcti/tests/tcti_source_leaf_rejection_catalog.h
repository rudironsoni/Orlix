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
struct tcti_source_leaf_rejection {
	u32 ordinal;
	const char *name;
	const char *operation;
	u32 mask;
	u32 pattern;
};

struct tcti_source_leaf_manifest_row {
	u32 ordinal;
	const char *name;
	const char *operation;
	u32 mask;
	u32 pattern;
};

#define TCTI_A64_SOURCE_MANIFEST_ROW(ordinal, name, mnemonic, operation, \
					     mask, pattern, condition, source_offset, \
					     source_length) \
	{ ordinal, name, operation, mask, pattern },
#define TCTI_A64_SOURCE_MANIFEST_SOURCE(...)
static const struct tcti_source_leaf_manifest_row
	tcti_source_leaf_manifest_rows[] = {
#include "../isa/source_manifest.def"
};
#undef TCTI_A64_SOURCE_MANIFEST_SOURCE
#undef TCTI_A64_SOURCE_MANIFEST_ROW

struct tcti_source_leaf_proof_binding {
	u32 ordinal;
	const char *proof_id;
};

#define TCTI_A64_SOURCE_BOUND_PROOF(ordinal, proof_id) \
	{ ordinal, proof_id },
static const struct tcti_source_leaf_proof_binding
	tcti_source_leaf_proof_bindings[] = {
#include "../isa/source_bound_proof.def"
};
#undef TCTI_A64_SOURCE_BOUND_PROOF

static inline bool
tcti_source_leaf_is_unconditional_el0_rejection(const char *proof_id)
{
	/* Selector-dependent generic encodings have separate partition tests. */
	static const char * const proof_ids[] = {
		"kunit:source-leaf-udf-undefined",
		"kunit:source-leaf-hvc-non-el0",
		"kunit:source-leaf-smc-non-el0",
		"kunit:source-leaf-dcps1-non-el0",
		"kunit:source-leaf-dcps2-non-el0",
		"kunit:source-leaf-dcps3-non-el0",
		"kunit:source-leaf-eret-non-el0",
		"kunit:source-leaf-ereta-non-el0",
		"kunit:source-leaf-drps-non-el0",
	};
	size_t index;

	for (index = 0; index < ARRAY_SIZE(proof_ids); index++)
		if (!strcmp(proof_id, proof_ids[index]))
			return true;

	return false;
}

static inline const struct tcti_source_leaf_manifest_row *
tcti_source_leaf_manifest_row(u32 ordinal)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(tcti_source_leaf_manifest_rows);
	     index++)
		if (tcti_source_leaf_manifest_rows[index].ordinal == ordinal)
			return &tcti_source_leaf_manifest_rows[index];

	return NULL;
}

static inline size_t tcti_source_leaf_rejection_count(void)
{
	size_t index;
	size_t count = 0;

	for (index = 0; index < ARRAY_SIZE(tcti_source_leaf_proof_bindings);
	     index++)
		if (tcti_source_leaf_is_unconditional_el0_rejection(
			tcti_source_leaf_proof_bindings[index].proof_id))
			count++;

	return count;
}

static inline const struct tcti_source_leaf_rejection *
tcti_source_leaf_rejection_at(size_t rejection_index,
			      struct tcti_source_leaf_rejection *entry)
{
	const struct tcti_source_leaf_manifest_row *source;
	size_t index;
	size_t count = 0;

	for (index = 0; index < ARRAY_SIZE(tcti_source_leaf_proof_bindings);
	     index++) {
		const struct tcti_source_leaf_proof_binding *binding =
			&tcti_source_leaf_proof_bindings[index];

		if (!tcti_source_leaf_is_unconditional_el0_rejection(
			binding->proof_id))
			continue;
		if (count++ != rejection_index)
			continue;
		source = tcti_source_leaf_manifest_row(binding->ordinal);
		if (!source)
			return NULL;
		*entry = (struct tcti_source_leaf_rejection) {
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
