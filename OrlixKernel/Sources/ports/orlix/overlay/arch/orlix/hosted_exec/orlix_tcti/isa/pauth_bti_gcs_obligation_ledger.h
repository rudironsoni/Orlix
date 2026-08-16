/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Pinned Arm AARCHMRS 2026-06 obligations for the pointer-authentication,
 * branch-target-identification, and guarded-control-stack families.
 *
 * These rows retain feature-conditioned EL0 obligations independently from
 * their external DDI0602 provenance, which grants no implementation or proof
 * credit. A generic unsupported decoder result is the current fail-closed
 * implementation state, not a classification of architectural behavior.
 */
#ifndef __ORLIX_TCTI_PAUTH_BTI_GCS_OBLIGATION_LEDGER_H
#define __ORLIX_TCTI_PAUTH_BTI_GCS_OBLIGATION_LEDGER_H

#include <linux/types.h>

enum orlix_tcti_pauth_bti_gcs_obligation {
	ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS,
	ORLIX_TCTI_PAUTH_BTI_GCS_NON_EL0_REJECTION,
};

enum orlix_tcti_pauth_bti_gcs_semantic_provenance {
	ORLIX_TCTI_PAUTH_BTI_GCS_PROVENANCE_EXTERNAL_DDI0602,
};

enum orlix_tcti_pauth_bti_gcs_implementation_status {
	ORLIX_TCTI_PAUTH_BTI_GCS_IMPLEMENTATION_REQUIRED_UNIMPLEMENTED,
	ORLIX_TCTI_PAUTH_BTI_GCS_IMPLEMENTED_PRODUCTION,
};

enum orlix_tcti_pauth_bti_gcs_proof_status {
	ORLIX_TCTI_PAUTH_BTI_GCS_PROOF_REQUIRED_UNPROVEN,
	ORLIX_TCTI_PAUTH_BTI_GCS_PROOF_SOURCE_BOUND_KUNIT,
};

struct orlix_tcti_pauth_bti_gcs_obligation_record {
	u16 source_ordinal;
	const char *source_id;
	const char *operation;
	const char *feature_predicate;
	const char *asl_operation;
	u32 mask;
	u32 pattern;
	enum orlix_tcti_pauth_bti_gcs_obligation required_behavior;
	enum orlix_tcti_pauth_bti_gcs_semantic_provenance semantic_provenance;
	enum orlix_tcti_pauth_bti_gcs_implementation_status implementation_status;
	enum orlix_tcti_pauth_bti_gcs_proof_status proof_status;
};

#define ORLIX_TCTI_POINTER_AUTHENTICATION_OBLIGATION_ROWS(_record) \
	_record(2167U, "AUTIASPPC_only_dp_1src_imm", "AUTIASPPC_imm", "FEAT_PAuth_LR", "operations/AUTIASPPC_imm", 0xffe0001fU, 0xf380001fU, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(2168U, "AUTIBSPPC_only_dp_1src_imm", "AUTIBSPPC_imm", "FEAT_PAuth_LR", "operations/AUTIBSPPC_imm", 0xffe0001fU, 0xf3a0001fU, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(2213U, "RETAASPPC_only_miscbranch", "RETASPPC_imm", "FEAT_PAuth_LR", "operations/RETASPPC_imm", 0xffe0001fU, 0x5500001fU, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(2214U, "RETABSPPC_only_miscbranch", "RETASPPC_imm", "FEAT_PAuth_LR", "operations/RETASPPC_imm", 0xffe0001fU, 0x5520001fU, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(2245U, "XPACLRI_HI_hints", "XPAC", "FEAT_PAuth", "operations/XPAC", 0xffffffffU, 0xd50320ffU, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(2246U, "PACIA1716_HI_hints", "PACIA", "FEAT_PAuth", "operations/PACIA", 0xffffffffU, 0xd503211fU, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(2247U, "PACIB1716_HI_hints", "PACIB", "FEAT_PAuth", "operations/PACIB", 0xffffffffU, 0xd503215fU, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(2248U, "AUTIA1716_HI_hints", "AUTIA", "FEAT_PAuth", "operations/AUTIA", 0xffffffffU, 0xd503219fU, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(2249U, "AUTIB1716_HI_hints", "AUTIB", "FEAT_PAuth", "operations/AUTIB", 0xffffffffU, 0xd50321dfU, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(2256U, "PACIAZ_HI_hints", "PACIA", "FEAT_PAuth", "operations/PACIA", 0xffffffffU, 0xd503231fU, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(2257U, "PACIASP_HI_hints", "PACIA", "FEAT_PAuth", "operations/PACIA", 0xffffffffU, 0xd503233fU, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(2258U, "PACIBZ_HI_hints", "PACIB", "FEAT_PAuth", "operations/PACIB", 0xffffffffU, 0xd503235fU, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(2259U, "PACIBSP_HI_hints", "PACIB", "FEAT_PAuth", "operations/PACIB", 0xffffffffU, 0xd503237fU, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(2260U, "AUTIAZ_HI_hints", "AUTIA", "FEAT_PAuth", "operations/AUTIA", 0xffffffffU, 0xd503239fU, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(2261U, "AUTIASP_HI_hints", "AUTIA", "FEAT_PAuth", "operations/AUTIA", 0xffffffffU, 0xd50323bfU, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(2262U, "AUTIBZ_HI_hints", "AUTIB", "FEAT_PAuth", "operations/AUTIB", 0xffffffffU, 0xd50323dfU, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(2263U, "AUTIBSP_HI_hints", "AUTIB", "FEAT_PAuth", "operations/AUTIB", 0xffffffffU, 0xd50323ffU, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(2265U, "PACM_HI_hints", "PACM", "FEAT_PAuth_LR", "operations/PACM", 0xffffffffU, 0xd50324ffU, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(2289U, "BRAAZ_64_branch_reg", "BRA", "FEAT_PAuth", "operations/BRA", 0xfffffc1fU, 0xd61f081fU, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(2290U, "BRABZ_64_branch_reg", "BRA", "FEAT_PAuth", "operations/BRA", 0xfffffc1fU, 0xd61f0c1fU, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(2292U, "BLRAAZ_64_branch_reg", "BLRA", "FEAT_PAuth", "operations/BLRA", 0xfffffc1fU, 0xd63f081fU, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(2293U, "BLRABZ_64_branch_reg", "BLRA", "FEAT_PAuth", "operations/BLRA", 0xfffffc1fU, 0xd63f0c1fU, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(2295U, "RETAASPPCR_64M_branch_reg", "RETASPPCR_reg", "FEAT_PAuth_LR", "operations/RETASPPCR_reg", 0xffffffe0U, 0xd65f0be0U, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(2296U, "RETAA_64E_branch_reg", "RETA", "FEAT_PAuth", "operations/RETA", 0xffffffffU, 0xd65f0bffU, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(2297U, "RETABSPPCR_64M_branch_reg", "RETASPPCR_reg", "FEAT_PAuth_LR", "operations/RETASPPCR_reg", 0xffffffe0U, 0xd65f0fe0U, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(2298U, "RETAB_64E_branch_reg", "RETA", "FEAT_PAuth", "operations/RETA", 0xffffffffU, 0xd65f0fffU, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(2304U, "BRAA_64P_branch_reg", "BRA", "FEAT_PAuth", "operations/BRA", 0xfffffc00U, 0xd71f0800U, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(2305U, "BRAB_64P_branch_reg", "BRA", "FEAT_PAuth", "operations/BRA", 0xfffffc00U, 0xd71f0c00U, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(2306U, "BLRAA_64P_branch_reg", "BLRA", "FEAT_PAuth", "operations/BLRA", 0xfffffc00U, 0xd73f0800U, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(2307U, "BLRAB_64P_branch_reg", "BLRA", "FEAT_PAuth", "operations/BLRA", 0xfffffc00U, 0xd73f0c00U, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(3381U, "PACGA_64P_dp_2src", "PACGA", "FEAT_PAuth", "operations/PACGA", 0xffe0fc00U, 0x9ac03000U, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(3406U, "PACIA_64P_dp_1src", "PACIA", "FEAT_PAuth", "operations/PACIA", 0xfffffc00U, 0xdac10000U, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(3407U, "PACIB_64P_dp_1src", "PACIB", "FEAT_PAuth", "operations/PACIB", 0xfffffc00U, 0xdac10400U, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(3408U, "PACDA_64P_dp_1src", "PACDA", "FEAT_PAuth", "operations/PACDA", 0xfffffc00U, 0xdac10800U, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(3409U, "PACDB_64P_dp_1src", "PACDB", "FEAT_PAuth", "operations/PACDB", 0xfffffc00U, 0xdac10c00U, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(3410U, "AUTIA_64P_dp_1src", "AUTIA", "FEAT_PAuth", "operations/AUTIA", 0xfffffc00U, 0xdac11000U, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(3411U, "AUTIB_64P_dp_1src", "AUTIB", "FEAT_PAuth", "operations/AUTIB", 0xfffffc00U, 0xdac11400U, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(3412U, "AUTDA_64P_dp_1src", "AUTDA", "FEAT_PAuth", "operations/AUTDA", 0xfffffc00U, 0xdac11800U, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(3413U, "AUTDB_64P_dp_1src", "AUTDB", "FEAT_PAuth", "operations/AUTDB", 0xfffffc00U, 0xdac11c00U, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(3414U, "PACIZA_64Z_dp_1src", "PACIA", "FEAT_PAuth", "operations/PACIA", 0xffffffe0U, 0xdac123e0U, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(3415U, "PACIZB_64Z_dp_1src", "PACIB", "FEAT_PAuth", "operations/PACIB", 0xffffffe0U, 0xdac127e0U, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(3416U, "PACDZA_64Z_dp_1src", "PACDA", "FEAT_PAuth", "operations/PACDA", 0xffffffe0U, 0xdac12be0U, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(3417U, "PACDZB_64Z_dp_1src", "PACDB", "FEAT_PAuth", "operations/PACDB", 0xffffffe0U, 0xdac12fe0U, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(3418U, "AUTIZA_64Z_dp_1src", "AUTIA", "FEAT_PAuth", "operations/AUTIA", 0xffffffe0U, 0xdac133e0U, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(3419U, "AUTIZB_64Z_dp_1src", "AUTIB", "FEAT_PAuth", "operations/AUTIB", 0xffffffe0U, 0xdac137e0U, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(3420U, "AUTDZA_64Z_dp_1src", "AUTDA", "FEAT_PAuth", "operations/AUTDA", 0xffffffe0U, 0xdac13be0U, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(3421U, "AUTDZB_64Z_dp_1src", "AUTDB", "FEAT_PAuth", "operations/AUTDB", 0xffffffe0U, 0xdac13fe0U, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(3422U, "XPACI_64Z_dp_1src", "XPAC", "FEAT_PAuth", "operations/XPAC", 0xffffffe0U, 0xdac143e0U, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(3423U, "XPACD_64Z_dp_1src", "XPAC", "FEAT_PAuth", "operations/XPAC", 0xffffffe0U, 0xdac147e0U, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(3424U, "PACNBIASPPC_64LR_dp_1src", "PACNBIASPPC", "FEAT_PAuth_LR", "operations/PACNBIASPPC", 0xffffffffU, 0xdac183feU, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(3425U, "PACNBIBSPPC_64LR_dp_1src", "PACNBIBSPPC", "FEAT_PAuth_LR", "operations/PACNBIBSPPC", 0xffffffffU, 0xdac187feU, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(3426U, "PACIA171615_64LR_dp_1src", "PACIA171615", "FEAT_PAuth_LR", "operations/PACIA171615", 0xffffffffU, 0xdac18bfeU, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(3427U, "PACIB171615_64LR_dp_1src", "PACIB171615", "FEAT_PAuth_LR", "operations/PACIB171615", 0xffffffffU, 0xdac18ffeU, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(3428U, "AUTIASPPCR_64LRR_dp_1src", "AUTIASPPCR", "FEAT_PAuth_LR", "operations/AUTIASPPCR", 0xfffffc1fU, 0xdac1901eU, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(3429U, "AUTIBSPPCR_64LRR_dp_1src", "AUTIBSPPCR", "FEAT_PAuth_LR", "operations/AUTIBSPPCR", 0xfffffc1fU, 0xdac1941eU, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(3430U, "PACIASPPC_64LR_dp_1src", "PACIASPPC", "FEAT_PAuth_LR", "operations/PACIASPPC", 0xffffffffU, 0xdac1a3feU, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(3431U, "PACIBSPPC_64LR_dp_1src", "PACIBSPPC", "FEAT_PAuth_LR", "operations/PACIBSPPC", 0xffffffffU, 0xdac1a7feU, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(3432U, "AUTIA171615_64LR_dp_1src", "AUTIA171615", "FEAT_PAuth_LR", "operations/AUTIA171615", 0xffffffffU, 0xdac1bbfeU, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(3433U, "AUTIB171615_64LR_dp_1src", "AUTIB171615", "FEAT_PAuth_LR", "operations/AUTIB171615", 0xffffffffU, 0xdac1bffeU, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(3328U, "LDRAA_64_ldst_pac", "LDRA", "FEAT_PAuth", "operations/LDRA", 0xffa00c00U, 0xf8200400U, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(3329U, "LDRAA_64W_ldst_pac", "LDRA", "FEAT_PAuth", "operations/LDRA", 0xffa00c00U, 0xf8200c00U, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(3330U, "LDRAB_64_ldst_pac", "LDRA", "FEAT_PAuth", "operations/LDRA", 0xffa00c00U, 0xf8a00400U, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(3331U, "LDRAB_64W_ldst_pac", "LDRA", "FEAT_PAuth", "operations/LDRA", 0xffa00c00U, 0xf8a00c00U, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS)

#define ORLIX_TCTI_PAUTH_BTI_GCS_OBLIGATION_ROWS(_record) \
	_record(2253U, "GCSB_HD_hints", "GCSB", "FEAT_GCS", "operations/GCSB", 0xffffffffU, 0xd503227fU, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(2264U, "BTI_HB_hints", "BTI", "FEAT_BTI", "operations/BTI", 0xffffff3fU, 0xd503241fU, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(2300U, "ERETAA_64E_branch_reg", "ERETA", "FEAT_PAuth", "operations/ERETA", 0xffffffffU, 0xd69f0bffU, ORLIX_TCTI_PAUTH_BTI_GCS_NON_EL0_REJECTION) \
	_record(2301U, "ERETAB_64E_branch_reg", "ERETA", "FEAT_PAuth", "operations/ERETA", 0xffffffffU, 0xd69f0fffU, ORLIX_TCTI_PAUTH_BTI_GCS_NON_EL0_REJECTION) \
	_record(2565U, "GCSSTR_64_ldst_gcs", "GCSSTR", "FEAT_GCS", "operations/GCSSTR", 0xfffffc00U, 0xd91f0c00U, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS) \
	_record(2566U, "GCSSTTR_64_ldst_gcs", "GCSSTTR", "FEAT_GCS", "operations/GCSSTTR", 0xfffffc00U, 0xd91f1c00U, ORLIX_TCTI_PAUTH_BTI_GCS_FEATURE_EL0_SEMANTICS)

#endif /* __ORLIX_TCTI_PAUTH_BTI_GCS_OBLIGATION_LEDGER_H */
