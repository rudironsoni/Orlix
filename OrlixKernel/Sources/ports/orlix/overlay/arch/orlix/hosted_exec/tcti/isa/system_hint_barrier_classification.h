/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Pinned Arm AARCHMRS 2026-06 source classification candidates for the
 * system, hint, and barrier range with source ordinals 2227 through 2280.
 *
 * This is durable C-native inventory input.  It deliberately carries the
 * source operation name, feature predicate, and semantic variant relationship
 * instead of treating the current HWCAP profile as a classification source.
 * The target inventory generator owns promotion into target_classification.def
 * once an owning implementation and KUnit proof are bound.
 */
#ifndef __ORLIX_TCTI_SYSTEM_HINT_BARRIER_CLASSIFICATION_H
#define __ORLIX_TCTI_SYSTEM_HINT_BARRIER_CLASSIFICATION_H

#include <linux/types.h>

enum tcti_system_hint_barrier_classification {
	TCTI_SYSTEM_HINT_BARRIER_REQUIRED_EL0,
	TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0,
	TCTI_SYSTEM_HINT_BARRIER_NON_EL0_REJECTION,
	TCTI_SYSTEM_HINT_BARRIER_UNDEFINED_OR_UNALLOCATED,
	TCTI_SYSTEM_HINT_BARRIER_ALIAS_OR_DUPLICATE,
	TCTI_SYSTEM_HINT_BARRIER_VARIANT_SPLIT,
};

enum tcti_system_hint_barrier_relationship {
	TCTI_SYSTEM_HINT_BARRIER_RELATION_NONE,
	TCTI_SYSTEM_HINT_BARRIER_RELATION_SEMANTIC_VARIANT,
	TCTI_SYSTEM_HINT_BARRIER_RELATION_ALIAS,
};

struct tcti_system_hint_barrier_classification_record {
	u32 source_ordinal;
	const char *source_id;
	const char *operation;
	const char *feature_predicate;
	enum tcti_system_hint_barrier_classification classification;
	enum tcti_system_hint_barrier_relationship relationship;
	const char *related_source_id;
	const char *asl_operation;
	const char *implementation_owner;
	const char *kunit_owner;
};

/*
 * The macro is deliberately available to the target inventory generator and
 * to a family-local KUnit.  Every tuple is copied from the pinned manifest,
 * except the explicit target classification and the owning proof obligations.
 */
#define TCTI_SYSTEM_HINT_BARRIER_UNCLASSIFIED_RECORDS(_record) \
	_record(2235U, "TENTER_te_exception", "TENTER", "FEAT_TME", \
		TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		TCTI_SYSTEM_HINT_BARRIER_RELATION_NONE, "", "TENTER", \
		"tcti-transactional-memory", "kunit:tcti-tme-source-bound") \
	_record(2236U, "WFET_only_systeminstrswithreg", "WFET", "FEAT_WFxT", \
		TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		TCTI_SYSTEM_HINT_BARRIER_RELATION_NONE, "", "WFET", \
		"tcti-wfxt", "kunit:tcti-wfxt-source-bound") \
	_record(2237U, "WFIT_only_systeminstrswithreg", "WFIT", "FEAT_WFxT", \
		TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		TCTI_SYSTEM_HINT_BARRIER_RELATION_NONE, "", "WFIT", \
		"tcti-wfxt", "kunit:tcti-wfxt-source-bound") \
	_record(2244U, "DGH_HI_hints", "DGH", "FEAT_DGH", \
		TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		TCTI_SYSTEM_HINT_BARRIER_RELATION_NONE, "", "DGH", \
		"tcti-data-gathering-hint", "kunit:tcti-dgh-source-bound") \
	_record(2245U, "XPACLRI_HI_hints", "XPAC", "FEAT_PAuth", \
		TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		TCTI_SYSTEM_HINT_BARRIER_RELATION_SEMANTIC_VARIANT, "XPACI_64Z_dp_1src", "XPAC", \
		"tcti-pauth", "kunit:tcti-pauth-hint-source-bound") \
	_record(2246U, "PACIA1716_HI_hints", "PACIA", "FEAT_PAuth", \
		TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		TCTI_SYSTEM_HINT_BARRIER_RELATION_SEMANTIC_VARIANT, "PACIA_64P_dp_1src", "PACIA", \
		"tcti-pauth", "kunit:tcti-pauth-hint-source-bound") \
	_record(2247U, "PACIB1716_HI_hints", "PACIB", "FEAT_PAuth", \
		TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		TCTI_SYSTEM_HINT_BARRIER_RELATION_SEMANTIC_VARIANT, "PACIB_64P_dp_1src", "PACIB", \
		"tcti-pauth", "kunit:tcti-pauth-hint-source-bound") \
	_record(2248U, "AUTIA1716_HI_hints", "AUTIA", "FEAT_PAuth", \
		TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		TCTI_SYSTEM_HINT_BARRIER_RELATION_SEMANTIC_VARIANT, "AUTIA_64P_dp_1src", "AUTIA", \
		"tcti-pauth", "kunit:tcti-pauth-hint-source-bound") \
	_record(2249U, "AUTIB1716_HI_hints", "AUTIB", "FEAT_PAuth", \
		TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		TCTI_SYSTEM_HINT_BARRIER_RELATION_SEMANTIC_VARIANT, "AUTIB_64P_dp_1src", "AUTIB", \
		"tcti-pauth", "kunit:tcti-pauth-hint-source-bound") \
	_record(2250U, "ESB_HI_hints", "ESB", "FEAT_RAS", \
		TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		TCTI_SYSTEM_HINT_BARRIER_RELATION_NONE, "", "ESB", \
		"tcti-ras", "kunit:tcti-esb-source-bound") \
	_record(2251U, "PSB_HC_hints", "PSB", "FEAT_SPE", \
		TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		TCTI_SYSTEM_HINT_BARRIER_RELATION_NONE, "", "PSB", \
		"tcti-spe", "kunit:tcti-psb-source-bound") \
	_record(2252U, "TSB_HC_hints", "TSB", "FEAT_TRF", \
		TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		TCTI_SYSTEM_HINT_BARRIER_RELATION_NONE, "", "TSB", \
		"tcti-trf", "kunit:tcti-tsb-source-bound") \
	_record(2253U, "GCSB_HD_hints", "GCSB", "FEAT_GCS", \
		TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		TCTI_SYSTEM_HINT_BARRIER_RELATION_NONE, "", "GCSB", \
		"tcti-gcs", "kunit:tcti-gcsb-source-bound") \
	_record(2255U, "CLRBHB_HI_hints", "CLRBHB", "FEAT_CLRBHB", \
		TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		TCTI_SYSTEM_HINT_BARRIER_RELATION_NONE, "", "CLRBHB", \
		"tcti-clrbhb", "kunit:tcti-clrbhb-source-bound") \
	_record(2256U, "PACIAZ_HI_hints", "PACIA", "FEAT_PAuth", \
		TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		TCTI_SYSTEM_HINT_BARRIER_RELATION_SEMANTIC_VARIANT, "PACIA_64P_dp_1src", "PACIA", \
		"tcti-pauth", "kunit:tcti-pauth-hint-source-bound") \
	_record(2257U, "PACIASP_HI_hints", "PACIA", "FEAT_PAuth", \
		TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		TCTI_SYSTEM_HINT_BARRIER_RELATION_SEMANTIC_VARIANT, "PACIA_64P_dp_1src", "PACIA", \
		"tcti-pauth", "kunit:tcti-pauth-hint-source-bound") \
	_record(2258U, "PACIBZ_HI_hints", "PACIB", "FEAT_PAuth", \
		TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		TCTI_SYSTEM_HINT_BARRIER_RELATION_SEMANTIC_VARIANT, "PACIB_64P_dp_1src", "PACIB", \
		"tcti-pauth", "kunit:tcti-pauth-hint-source-bound") \
	_record(2259U, "PACIBSP_HI_hints", "PACIB", "FEAT_PAuth", \
		TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		TCTI_SYSTEM_HINT_BARRIER_RELATION_SEMANTIC_VARIANT, "PACIB_64P_dp_1src", "PACIB", \
		"tcti-pauth", "kunit:tcti-pauth-hint-source-bound") \
	_record(2260U, "AUTIAZ_HI_hints", "AUTIA", "FEAT_PAuth", \
		TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		TCTI_SYSTEM_HINT_BARRIER_RELATION_SEMANTIC_VARIANT, "AUTIA_64P_dp_1src", "AUTIA", \
		"tcti-pauth", "kunit:tcti-pauth-hint-source-bound") \
	_record(2261U, "AUTIASP_HI_hints", "AUTIA", "FEAT_PAuth", \
		TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		TCTI_SYSTEM_HINT_BARRIER_RELATION_SEMANTIC_VARIANT, "AUTIA_64P_dp_1src", "AUTIA", \
		"tcti-pauth", "kunit:tcti-pauth-hint-source-bound") \
	_record(2262U, "AUTIBZ_HI_hints", "AUTIB", "FEAT_PAuth", \
		TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		TCTI_SYSTEM_HINT_BARRIER_RELATION_SEMANTIC_VARIANT, "AUTIB_64P_dp_1src", "AUTIB", \
		"tcti-pauth", "kunit:tcti-pauth-hint-source-bound") \
	_record(2263U, "AUTIBSP_HI_hints", "AUTIB", "FEAT_PAuth", \
		TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		TCTI_SYSTEM_HINT_BARRIER_RELATION_SEMANTIC_VARIANT, "AUTIB_64P_dp_1src", "AUTIB", \
		"tcti-pauth", "kunit:tcti-pauth-hint-source-bound") \
	_record(2264U, "BTI_HB_hints", "BTI", "FEAT_BTI", \
		TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		TCTI_SYSTEM_HINT_BARRIER_RELATION_NONE, "", "BTI", \
		"tcti-bti", "kunit:tcti-bti-source-bound") \
	_record(2265U, "PACM_HI_hints", "PACM", "FEAT_PAuth_LR", \
		TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		TCTI_SYSTEM_HINT_BARRIER_RELATION_NONE, "", "PACM", \
		"tcti-pauth-lr", "kunit:tcti-pacm-source-bound") \
	_record(2266U, "CHKFEAT_HF_hints", "CHKFEAT", "FEAT_CHK", \
		TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		TCTI_SYSTEM_HINT_BARRIER_RELATION_NONE, "", "CHKFEAT", \
		"tcti-chkfeat", "kunit:tcti-chkfeat-source-bound") \
	_record(2267U, "STSHH_HI_hints", "STSHH", "FEAT_SCPHINT", \
		TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		TCTI_SYSTEM_HINT_BARRIER_RELATION_NONE, "", "STSHH", \
		"tcti-scphint", "kunit:tcti-stshh-source-bound") \
	_record(2268U, "SHUH_HI_hints", "SHUH", "FEAT_CMP", \
		TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		TCTI_SYSTEM_HINT_BARRIER_RELATION_NONE, "", "SHUH", \
		"tcti-cmp", "kunit:tcti-shuh-source-bound") \
	_record(2269U, "STCPH_HI_hints", "STCPH", "FEAT_CMP", \
		TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		TCTI_SYSTEM_HINT_BARRIER_RELATION_NONE, "", "STCPH", \
		"tcti-cmp", "kunit:tcti-stcph-source-bound") \
	_record(2275U, "SB_only_barriers", "SB", "FEAT_SB", \
		TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		TCTI_SYSTEM_HINT_BARRIER_RELATION_NONE, "", "SB", \
		"tcti-speculation-barrier", "kunit:tcti-sb-source-bound") \
	_record(2276U, "DSB_BOn_barriers", "DSB", "FEAT_XS", \
		TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		TCTI_SYSTEM_HINT_BARRIER_RELATION_SEMANTIC_VARIANT, "DSB_BO_barriers", "DSB", \
		"tcti-memory-order", "kunit:tcti-dsb-nxs-source-bound") \
	_record(2277U, "MSR_SI_pstate", "MSR_imm", "mixed PSTATE fields", \
		TCTI_SYSTEM_HINT_BARRIER_VARIANT_SPLIT, \
		TCTI_SYSTEM_HINT_BARRIER_RELATION_SEMANTIC_VARIANT, "PSTATE field encoding", "MSR_imm", \
		"tcti-pstate", "kunit:tcti-msr-pstate-source-bound") \
	_record(2278U, "CFINV_M_pstate", "CFINV", "FEAT_FlagM", \
		TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		TCTI_SYSTEM_HINT_BARRIER_RELATION_NONE, "", "CFINV", \
		"tcti-pstate", "kunit:tcti-cfinv-source-bound") \
	_record(2279U, "XAFLAG_M_pstate", "XAFLAG", "FEAT_FlagM2", \
		TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		TCTI_SYSTEM_HINT_BARRIER_RELATION_NONE, "", "XAFLAG", \
		"tcti-pstate", "kunit:tcti-xaflag-source-bound") \
	_record(2280U, "AXFLAG_M_pstate", "AXFLAG", "FEAT_FlagM2", \
		TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		TCTI_SYSTEM_HINT_BARRIER_RELATION_NONE, "", "AXFLAG", \
		"tcti-pstate", "kunit:tcti-axflag-source-bound")

#endif /* __ORLIX_TCTI_SYSTEM_HINT_BARRIER_CLASSIFICATION_H */
