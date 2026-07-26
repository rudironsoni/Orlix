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

enum orlix_tcti_system_hint_barrier_classification {
	ORLIX_TCTI_SYSTEM_HINT_BARRIER_REQUIRED_EL0,
	ORLIX_TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0,
	ORLIX_TCTI_SYSTEM_HINT_BARRIER_NON_EL0_REJECTION,
	ORLIX_TCTI_SYSTEM_HINT_BARRIER_UNDEFINED_OR_UNALLOCATED,
	ORLIX_TCTI_SYSTEM_HINT_BARRIER_ALIAS_OR_DUPLICATE,
	ORLIX_TCTI_SYSTEM_HINT_BARRIER_VARIANT_SPLIT,
};

enum orlix_tcti_system_hint_barrier_relationship {
	ORLIX_TCTI_SYSTEM_HINT_BARRIER_RELATION_NONE,
	ORLIX_TCTI_SYSTEM_HINT_BARRIER_RELATION_SEMANTIC_VARIANT,
	ORLIX_TCTI_SYSTEM_HINT_BARRIER_RELATION_ALIAS,
};

struct orlix_tcti_system_hint_barrier_classification_record {
	u32 source_ordinal;
	const char *source_id;
	const char *operation;
	const char *feature_predicate;
	enum orlix_tcti_system_hint_barrier_classification classification;
	enum orlix_tcti_system_hint_barrier_relationship relationship;
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
#define ORLIX_TCTI_SYSTEM_HINT_BARRIER_UNCLASSIFIED_RECORDS(_record) \
	_record(2235U, "TENTER_te_exception", "TENTER", "FEAT_TME", \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_RELATION_NONE, "", "TENTER", \
		"orlix-tcti-transactional-memory", "kunit:orlix-tcti-tme-source-bound") \
	_record(2236U, "WFET_only_systeminstrswithreg", "WFET", "FEAT_WFxT", \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_RELATION_NONE, "", "WFET", \
		"orlix-tcti-wfxt", "kunit:orlix-tcti-wfxt-source-bound") \
	_record(2237U, "WFIT_only_systeminstrswithreg", "WFIT", "FEAT_WFxT", \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_RELATION_NONE, "", "WFIT", \
		"orlix-tcti-wfxt", "kunit:orlix-tcti-wfxt-source-bound") \
	_record(2244U, "DGH_HI_hints", "DGH", "FEAT_DGH", \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_RELATION_NONE, "", "DGH", \
		"orlix-tcti-data-gathering-hint", "kunit:orlix-tcti-dgh-source-bound") \
	_record(2245U, "XPACLRI_HI_hints", "XPAC", "FEAT_PAuth", \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_RELATION_SEMANTIC_VARIANT, "XPACI_64Z_dp_1src", "XPAC", \
		"orlix-tcti-pauth", "kunit:orlix-tcti-pauth-hint-source-bound") \
	_record(2246U, "PACIA1716_HI_hints", "PACIA", "FEAT_PAuth", \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_RELATION_SEMANTIC_VARIANT, "PACIA_64P_dp_1src", "PACIA", \
		"orlix-tcti-pauth", "kunit:orlix-tcti-pauth-hint-source-bound") \
	_record(2247U, "PACIB1716_HI_hints", "PACIB", "FEAT_PAuth", \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_RELATION_SEMANTIC_VARIANT, "PACIB_64P_dp_1src", "PACIB", \
		"orlix-tcti-pauth", "kunit:orlix-tcti-pauth-hint-source-bound") \
	_record(2248U, "AUTIA1716_HI_hints", "AUTIA", "FEAT_PAuth", \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_RELATION_SEMANTIC_VARIANT, "AUTIA_64P_dp_1src", "AUTIA", \
		"orlix-tcti-pauth", "kunit:orlix-tcti-pauth-hint-source-bound") \
	_record(2249U, "AUTIB1716_HI_hints", "AUTIB", "FEAT_PAuth", \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_RELATION_SEMANTIC_VARIANT, "AUTIB_64P_dp_1src", "AUTIB", \
		"orlix-tcti-pauth", "kunit:orlix-tcti-pauth-hint-source-bound") \
	_record(2250U, "ESB_HI_hints", "ESB", "FEAT_RAS", \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_RELATION_NONE, "", "ESB", \
		"orlix-tcti-ras", "kunit:orlix-tcti-esb-source-bound") \
	_record(2251U, "PSB_HC_hints", "PSB", "FEAT_SPE", \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_RELATION_NONE, "", "PSB", \
		"orlix-tcti-spe", "kunit:orlix-tcti-psb-source-bound") \
	_record(2252U, "TSB_HC_hints", "TSB", "FEAT_TRF", \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_RELATION_NONE, "", "TSB", \
		"orlix-tcti-trf", "kunit:orlix-tcti-tsb-source-bound") \
	_record(2253U, "GCSB_HD_hints", "GCSB", "FEAT_GCS", \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_RELATION_NONE, "", "GCSB", \
		"orlix-tcti-gcs", "kunit:orlix-tcti-gcsb-source-bound") \
	_record(2255U, "CLRBHB_HI_hints", "CLRBHB", "FEAT_CLRBHB", \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_RELATION_NONE, "", "CLRBHB", \
		"orlix-tcti-clrbhb", "kunit:orlix-tcti-clrbhb-source-bound") \
	_record(2256U, "PACIAZ_HI_hints", "PACIA", "FEAT_PAuth", \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_RELATION_SEMANTIC_VARIANT, "PACIA_64P_dp_1src", "PACIA", \
		"orlix-tcti-pauth", "kunit:orlix-tcti-pauth-hint-source-bound") \
	_record(2257U, "PACIASP_HI_hints", "PACIA", "FEAT_PAuth", \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_RELATION_SEMANTIC_VARIANT, "PACIA_64P_dp_1src", "PACIA", \
		"orlix-tcti-pauth", "kunit:orlix-tcti-pauth-hint-source-bound") \
	_record(2258U, "PACIBZ_HI_hints", "PACIB", "FEAT_PAuth", \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_RELATION_SEMANTIC_VARIANT, "PACIB_64P_dp_1src", "PACIB", \
		"orlix-tcti-pauth", "kunit:orlix-tcti-pauth-hint-source-bound") \
	_record(2259U, "PACIBSP_HI_hints", "PACIB", "FEAT_PAuth", \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_RELATION_SEMANTIC_VARIANT, "PACIB_64P_dp_1src", "PACIB", \
		"orlix-tcti-pauth", "kunit:orlix-tcti-pauth-hint-source-bound") \
	_record(2260U, "AUTIAZ_HI_hints", "AUTIA", "FEAT_PAuth", \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_RELATION_SEMANTIC_VARIANT, "AUTIA_64P_dp_1src", "AUTIA", \
		"orlix-tcti-pauth", "kunit:orlix-tcti-pauth-hint-source-bound") \
	_record(2261U, "AUTIASP_HI_hints", "AUTIA", "FEAT_PAuth", \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_RELATION_SEMANTIC_VARIANT, "AUTIA_64P_dp_1src", "AUTIA", \
		"orlix-tcti-pauth", "kunit:orlix-tcti-pauth-hint-source-bound") \
	_record(2262U, "AUTIBZ_HI_hints", "AUTIB", "FEAT_PAuth", \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_RELATION_SEMANTIC_VARIANT, "AUTIB_64P_dp_1src", "AUTIB", \
		"orlix-tcti-pauth", "kunit:orlix-tcti-pauth-hint-source-bound") \
	_record(2263U, "AUTIBSP_HI_hints", "AUTIB", "FEAT_PAuth", \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_RELATION_SEMANTIC_VARIANT, "AUTIB_64P_dp_1src", "AUTIB", \
		"orlix-tcti-pauth", "kunit:orlix-tcti-pauth-hint-source-bound") \
	_record(2264U, "BTI_HB_hints", "BTI", "FEAT_BTI", \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_RELATION_NONE, "", "BTI", \
		"orlix-tcti-bti", "kunit:orlix-tcti-bti-source-bound") \
	_record(2265U, "PACM_HI_hints", "PACM", "FEAT_PAuth_LR", \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_RELATION_NONE, "", "PACM", \
		"orlix-tcti-pauth-lr", "kunit:orlix-tcti-pacm-source-bound") \
	_record(2266U, "CHKFEAT_HF_hints", "CHKFEAT", "FEAT_CHK", \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_RELATION_NONE, "", "CHKFEAT", \
		"orlix-tcti-chkfeat", "kunit:orlix-tcti-chkfeat-source-bound") \
	_record(2267U, "STSHH_HI_hints", "STSHH", "FEAT_SCPHINT", \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_RELATION_NONE, "", "STSHH", \
		"orlix-tcti-scphint", "kunit:orlix-tcti-stshh-source-bound") \
	_record(2268U, "SHUH_HI_hints", "SHUH", "FEAT_CMP", \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_RELATION_NONE, "", "SHUH", \
		"orlix-tcti-cmp", "kunit:orlix-tcti-shuh-source-bound") \
	_record(2269U, "STCPH_HI_hints", "STCPH", "FEAT_CMP", \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_RELATION_NONE, "", "STCPH", \
		"orlix-tcti-cmp", "kunit:orlix-tcti-stcph-source-bound") \
	_record(2275U, "SB_only_barriers", "SB", "FEAT_SB", \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_RELATION_NONE, "", "SB", \
		"orlix-tcti-speculation-barrier", "kunit:orlix-tcti-sb-source-bound") \
	_record(2276U, "DSB_BOn_barriers", "DSB", "FEAT_XS", \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_RELATION_SEMANTIC_VARIANT, "DSB_BO_barriers", "DSB", \
		"orlix-tcti-memory-order", "kunit:orlix-tcti-dsb-nxs-source-bound") \
	_record(2277U, "MSR_SI_pstate", "MSR_imm", "mixed PSTATE fields", \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_VARIANT_SPLIT, \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_RELATION_SEMANTIC_VARIANT, "PSTATE field encoding", "MSR_imm", \
		"orlix-tcti-pstate", "kunit:orlix-tcti-msr-pstate-source-bound") \
	_record(2278U, "CFINV_M_pstate", "CFINV", "FEAT_FlagM", \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_RELATION_NONE, "", "CFINV", \
		"orlix-tcti-pstate", "kunit:orlix-tcti-cfinv-source-bound") \
	_record(2279U, "XAFLAG_M_pstate", "XAFLAG", "FEAT_FlagM2", \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_RELATION_NONE, "", "XAFLAG", \
		"orlix-tcti-pstate", "kunit:orlix-tcti-xaflag-source-bound") \
	_record(2280U, "AXFLAG_M_pstate", "AXFLAG", "FEAT_FlagM2", \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_FEATURE_CONDITIONED_EL0, \
		ORLIX_TCTI_SYSTEM_HINT_BARRIER_RELATION_NONE, "", "AXFLAG", \
		"orlix-tcti-pstate", "kunit:orlix-tcti-axflag-source-bound")

#endif /* __ORLIX_TCTI_SYSTEM_HINT_BARRIER_CLASSIFICATION_H */
