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
		"kunit:source-leaf-bf12cvtl-advsimd-non-el0",
		"kunit:source-leaf-bfcvtn-advsimd-non-el0",
		"kunit:source-leaf-bfdot-advsimd-elt-non-el0",
		"kunit:source-leaf-bfdot-advsimd-vec-non-el0",
		"kunit:source-leaf-bfmlal-advsimd-elt-non-el0",
		"kunit:source-leaf-bfmlal-advsimd-vec-non-el0",
		"kunit:source-leaf-bfmmla-advsimd-non-el0",
		"kunit:source-leaf-f12cvtl-advsimd-non-el0",
		"kunit:source-leaf-fabd-advsimd-non-el0",
		"kunit:source-leaf-fabs-advsimd-non-el0",
		"kunit:source-leaf-facge-advsimd-non-el0",
		"kunit:source-leaf-facgt-advsimd-non-el0",
		"kunit:source-leaf-fadd-advsimd-non-el0",
		"kunit:source-leaf-faddp-advsimd-pair-non-el0",
		"kunit:source-leaf-faddp-advsimd-vec-non-el0",
		"kunit:source-leaf-famax-advsimd-non-el0",
		"kunit:source-leaf-famin-advsimd-non-el0",
		"kunit:source-leaf-fcadd-advsimd-vec-non-el0",
		"kunit:source-leaf-fcmeq-advsimd-reg-non-el0",
		"kunit:source-leaf-fcmeq-advsimd-zero-non-el0",
		"kunit:source-leaf-fcmge-advsimd-reg-non-el0",
		"kunit:source-leaf-fcmge-advsimd-zero-non-el0",
		"kunit:source-leaf-fcmgt-advsimd-reg-non-el0",
		"kunit:source-leaf-fcmgt-advsimd-zero-non-el0",
		"kunit:source-leaf-fcmla-advsimd-elt-non-el0",
		"kunit:source-leaf-fcmla-advsimd-vec-non-el0",
		"kunit:source-leaf-fcmle-advsimd-non-el0",
		"kunit:source-leaf-fcmlt-advsimd-non-el0",
		"kunit:source-leaf-fcvtas-advsimd-non-el0",
		"kunit:source-leaf-fcvtau-advsimd-non-el0",
		"kunit:source-leaf-fcvtms-advsimd-non-el0",
		"kunit:source-leaf-fcvtmu-advsimd-non-el0",
		"kunit:source-leaf-fcvtn-advsimd-168-non-el0",
		"kunit:source-leaf-fcvtn-advsimd-328-non-el0",
		"kunit:source-leaf-fcvtns-advsimd-non-el0",
		"kunit:source-leaf-fcvtnu-advsimd-non-el0",
		"kunit:source-leaf-fcvtps-advsimd-non-el0",
		"kunit:source-leaf-fcvtpu-advsimd-non-el0",
		"kunit:source-leaf-fcvtzs-advsimd-int-non-el0",
		"kunit:source-leaf-fcvtzu-advsimd-int-non-el0",
		"kunit:source-leaf-fdiv-advsimd-non-el0",
		"kunit:source-leaf-fdot-advsimd-2wayelem-non-el0",
		"kunit:source-leaf-fdot-advsimd-2wayvec-non-el0",
		"kunit:source-leaf-fdot-advsimd-4wayelem-non-el0",
		"kunit:source-leaf-fdot-advsimd-4wayvec-non-el0",
		"kunit:source-leaf-fdot-advsimd-elt-fp16fp32-non-el0",
		"kunit:source-leaf-fdot-advsimd-fp16fp32-non-el0",
		"kunit:source-leaf-fmax-advsimd-non-el0",
		"kunit:source-leaf-fmaxnm-advsimd-non-el0",
		"kunit:source-leaf-fmaxnmp-advsimd-pair-non-el0",
		"kunit:source-leaf-fmaxnmp-advsimd-vec-non-el0",
		"kunit:source-leaf-fmaxnmv-advsimd-non-el0",
		"kunit:source-leaf-fmaxp-advsimd-pair-non-el0",
		"kunit:source-leaf-fmaxp-advsimd-vec-non-el0",
		"kunit:source-leaf-fmaxv-advsimd-non-el0",
		"kunit:source-leaf-fmin-advsimd-non-el0",
		"kunit:source-leaf-fminnm-advsimd-non-el0",
		"kunit:source-leaf-fminnmp-advsimd-pair-non-el0",
		"kunit:source-leaf-fminnmp-advsimd-vec-non-el0",
		"kunit:source-leaf-fminnmv-advsimd-non-el0",
		"kunit:source-leaf-fminp-advsimd-pair-non-el0",
		"kunit:source-leaf-fminp-advsimd-vec-non-el0",
		"kunit:source-leaf-fminv-advsimd-non-el0",
		"kunit:source-leaf-fmla-advsimd-elt-non-el0",
		"kunit:source-leaf-fmla-advsimd-vec-non-el0",
		"kunit:source-leaf-fmlal-advsimd-elt-non-el0",
		"kunit:source-leaf-fmlal-advsimd-vec-non-el0",
		"kunit:source-leaf-fmlalb-advsimd-elem-non-el0",
		"kunit:source-leaf-fmlalb-advsimd-vec-non-el0",
		"kunit:source-leaf-fmlallbb-advsimd-elem-non-el0",
		"kunit:source-leaf-fmlallbb-advsimd-vec-non-el0",
		"kunit:source-leaf-fmls-advsimd-elt-non-el0",
		"kunit:source-leaf-fmls-advsimd-vec-non-el0",
		"kunit:source-leaf-fmlsl-advsimd-elt-non-el0",
		"kunit:source-leaf-fmlsl-advsimd-vec-non-el0",
		"kunit:source-leaf-fmmla-advsimd-fp16fp16-non-el0",
		"kunit:source-leaf-fmmla-advsimd-fp16fp32-non-el0",
		"kunit:source-leaf-fmmla-fp8fp16-non-el0",
		"kunit:source-leaf-fmmla-fp8fp32-non-el0",
		"kunit:source-leaf-fmov-advsimd-non-el0",
		"kunit:source-leaf-fmul-advsimd-elt-non-el0",
		"kunit:source-leaf-fmul-advsimd-vec-non-el0",
		"kunit:source-leaf-fmulx-advsimd-elt-non-el0",
		"kunit:source-leaf-fmulx-advsimd-vec-non-el0",
		"kunit:source-leaf-fneg-advsimd-non-el0",
		"kunit:source-leaf-frecpe-advsimd-non-el0",
		"kunit:source-leaf-frecps-advsimd-non-el0",
		"kunit:source-leaf-frecpx-advsimd-non-el0",
		"kunit:source-leaf-frint32x-advsimd-non-el0",
		"kunit:source-leaf-frint32z-advsimd-non-el0",
		"kunit:source-leaf-frint64x-advsimd-non-el0",
		"kunit:source-leaf-frint64z-advsimd-non-el0",
		"kunit:source-leaf-frinta-advsimd-non-el0",
		"kunit:source-leaf-frinti-advsimd-non-el0",
		"kunit:source-leaf-frintm-advsimd-non-el0",
		"kunit:source-leaf-frintn-advsimd-non-el0",
		"kunit:source-leaf-frintp-advsimd-non-el0",
		"kunit:source-leaf-frintx-advsimd-non-el0",
		"kunit:source-leaf-frintz-advsimd-non-el0",
		"kunit:source-leaf-frsqrte-advsimd-non-el0",
		"kunit:source-leaf-frsqrts-advsimd-non-el0",
		"kunit:source-leaf-fscale-advsimd-non-el0",
		"kunit:source-leaf-fsqrt-advsimd-non-el0",
		"kunit:source-leaf-fsub-advsimd-non-el0",
		"kunit:source-leaf-scvtf-advsimd-int-non-el0",
		"kunit:source-leaf-ucvtf-advsimd-int-non-el0",

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
