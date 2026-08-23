// SPDX-License-Identifier: GPL-2.0-only
/*
 * Emit a bootstrap classification seed from the pinned Arm source.
 *
 * The checked completion authority is source_manifest.def plus the reviewed
 * target_classification.def ledger.  This tool intentionally does not read
 * the legacy runtime projection, infer a disposition, or assert hard-coded
 * classification totals.  Its output is only a convenient, deliberately
 * blocking starting point when the pinned Arm source changes.
 */
#include "target_inventory_import.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum classification {
	CLASS_UNCLASSIFIED,
	CLASS_NON_EL0,
	CLASS_ARCH_UNDEFINED,
};

struct reviewed_classification_binding {
	const char *name;
	enum classification classification;
	const char *evidence;
	const char *proof_id;
};

struct source_manifest_row {
	const char *name;
	const char *mnemonic;
	const char *operation_id;
	uint32_t mask;
	uint32_t pattern;
};

struct source_manifest_provenance {
	const char *sha256;
	uint32_t source_length;
};

#define ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE(architecture, build, release, schema, \
					 sha256, count, timestamp, length) \
	{ sha256, (length) }
#define ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW(ordinal, name, mnemonic, operation_id, \
				     mask, pattern, condition, offset, length)

static const struct source_manifest_provenance source_provenance =
#include "../source_manifest.def"
;

#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE

#define ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE(...)
#define ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW(ordinal, name, mnemonic, operation_id, \
				     mask, pattern, condition, offset, length) \
	{ name, mnemonic, operation_id, (mask), (pattern) },
static const struct source_manifest_row source_manifest[] = {
#include "../source_manifest.def"
};
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE

static const struct reviewed_classification_binding reviewed_bindings[] = {
	{ "UDF_only_perm_undef", CLASS_ARCH_UNDEFINED,
	  "pinned-source:architecturally-undefined-el0-rejection",
	  "kunit:source-leaf-udf-undefined" },
	{ "HVC_EX_exception", CLASS_NON_EL0,
	  "pinned-source:privileged-el0-rejection", "kunit:source-leaf-hvc-non-el0" },
	{ "SMC_EX_exception", CLASS_NON_EL0,
	  "pinned-source:privileged-el0-rejection", "kunit:source-leaf-smc-non-el0" },
	{ "DCPS1_DC_exception", CLASS_NON_EL0,
	  "pinned-source:privileged-el0-rejection", "kunit:source-leaf-dcps1-non-el0" },
	{ "DCPS2_DC_exception", CLASS_NON_EL0,
	  "pinned-source:privileged-el0-rejection", "kunit:source-leaf-dcps2-non-el0" },
	{ "DCPS3_DC_exception", CLASS_NON_EL0,
	  "pinned-source:privileged-el0-rejection", "kunit:source-leaf-dcps3-non-el0" },
	{ "ERET_64E_branch_reg", CLASS_NON_EL0,
	  "pinned-source:privileged-el0-rejection", "kunit:source-leaf-eret-non-el0" },
	{ "ERETAA_64E_branch_reg", CLASS_NON_EL0,
	  "pinned-source:privileged-el0-rejection", "kunit:source-leaf-ereta-non-el0" },
	{ "ERETAB_64E_branch_reg", CLASS_NON_EL0,
	  "pinned-source:privileged-el0-rejection", "kunit:source-leaf-ereta-non-el0" },
	{ "TENTER_te_exception", CLASS_NON_EL0,
	  "pinned-source:privileged-el0-rejection", "kunit:source-leaf-tenter-non-el0" },
	{ "TEXIT_te_branch_reg", CLASS_NON_EL0,
	  "pinned-source:privileged-el0-rejection", "kunit:source-leaf-texit-non-el0" },
	{ "DRPS_64E_branch_reg", CLASS_NON_EL0,
	  "pinned-source:privileged-el0-rejection", "kunit:source-leaf-drps-non-el0" },
	{ "BC_only_condbranch", CLASS_NON_EL0,
	  "pinned-source:feat-hbc-el0-rejection", "kunit:source-leaf-bc-cond-non-el0" },
	{ "CBBGT_8_regs", CLASS_NON_EL0,
	  "pinned-source:feat-cmpbr-el0-rejection", "kunit:source-leaf-cbbcc-regs-non-el0" },
	{ "CBBGE_8_regs", CLASS_NON_EL0,
	  "pinned-source:feat-cmpbr-el0-rejection", "kunit:source-leaf-cbbcc-regs-non-el0" },
	{ "CBBHI_8_regs", CLASS_NON_EL0,
	  "pinned-source:feat-cmpbr-el0-rejection", "kunit:source-leaf-cbbcc-regs-non-el0" },
	{ "CBBHS_8_regs", CLASS_NON_EL0,
	  "pinned-source:feat-cmpbr-el0-rejection", "kunit:source-leaf-cbbcc-regs-non-el0" },
	{ "CBBEQ_8_regs", CLASS_NON_EL0,
	  "pinned-source:feat-cmpbr-el0-rejection", "kunit:source-leaf-cbbcc-regs-non-el0" },
	{ "CBBNE_8_regs", CLASS_NON_EL0,
	  "pinned-source:feat-cmpbr-el0-rejection", "kunit:source-leaf-cbbcc-regs-non-el0" },
	{ "CBHGT_16_regs", CLASS_NON_EL0,
	  "pinned-source:feat-cmpbr-el0-rejection", "kunit:source-leaf-cbhcc-regs-non-el0" },
	{ "CBHGE_16_regs", CLASS_NON_EL0,
	  "pinned-source:feat-cmpbr-el0-rejection", "kunit:source-leaf-cbhcc-regs-non-el0" },
	{ "CBHHI_16_regs", CLASS_NON_EL0,
	  "pinned-source:feat-cmpbr-el0-rejection", "kunit:source-leaf-cbhcc-regs-non-el0" },
	{ "CBHHS_16_regs", CLASS_NON_EL0,
	  "pinned-source:feat-cmpbr-el0-rejection", "kunit:source-leaf-cbhcc-regs-non-el0" },
	{ "CBHEQ_16_regs", CLASS_NON_EL0,
	  "pinned-source:feat-cmpbr-el0-rejection", "kunit:source-leaf-cbhcc-regs-non-el0" },
	{ "CBHNE_16_regs", CLASS_NON_EL0,
	  "pinned-source:feat-cmpbr-el0-rejection", "kunit:source-leaf-cbhcc-regs-non-el0" },
	{ "CBGT_32_regs", CLASS_NON_EL0,
	  "pinned-source:feat-cmpbr-el0-rejection", "kunit:source-leaf-cbcc-regs-non-el0" },
	{ "CBGE_32_regs", CLASS_NON_EL0,
	  "pinned-source:feat-cmpbr-el0-rejection", "kunit:source-leaf-cbcc-regs-non-el0" },
	{ "CBHI_32_regs", CLASS_NON_EL0,
	  "pinned-source:feat-cmpbr-el0-rejection", "kunit:source-leaf-cbcc-regs-non-el0" },
	{ "CBHS_32_regs", CLASS_NON_EL0,
	  "pinned-source:feat-cmpbr-el0-rejection", "kunit:source-leaf-cbcc-regs-non-el0" },
	{ "CBEQ_32_regs", CLASS_NON_EL0,
	  "pinned-source:feat-cmpbr-el0-rejection", "kunit:source-leaf-cbcc-regs-non-el0" },
	{ "CBNE_32_regs", CLASS_NON_EL0,
	  "pinned-source:feat-cmpbr-el0-rejection", "kunit:source-leaf-cbcc-regs-non-el0" },
	{ "CBGT_64_regs", CLASS_NON_EL0,
	  "pinned-source:feat-cmpbr-el0-rejection", "kunit:source-leaf-cbcc-regs-non-el0" },
	{ "CBGE_64_regs", CLASS_NON_EL0,
	  "pinned-source:feat-cmpbr-el0-rejection", "kunit:source-leaf-cbcc-regs-non-el0" },
	{ "CBHI_64_regs", CLASS_NON_EL0,
	  "pinned-source:feat-cmpbr-el0-rejection", "kunit:source-leaf-cbcc-regs-non-el0" },
	{ "CBHS_64_regs", CLASS_NON_EL0,
	  "pinned-source:feat-cmpbr-el0-rejection", "kunit:source-leaf-cbcc-regs-non-el0" },
	{ "CBEQ_64_regs", CLASS_NON_EL0,
	  "pinned-source:feat-cmpbr-el0-rejection", "kunit:source-leaf-cbcc-regs-non-el0" },
	{ "CBNE_64_regs", CLASS_NON_EL0,
	  "pinned-source:feat-cmpbr-el0-rejection", "kunit:source-leaf-cbcc-regs-non-el0" },
	{ "CBGT_32_imm", CLASS_NON_EL0,
	  "pinned-source:feat-cmpbr-el0-rejection", "kunit:source-leaf-cbcc-imm-non-el0" },
	{ "CBLT_32_imm", CLASS_NON_EL0,
	  "pinned-source:feat-cmpbr-el0-rejection", "kunit:source-leaf-cbcc-imm-non-el0" },
	{ "CBHI_32_imm", CLASS_NON_EL0,
	  "pinned-source:feat-cmpbr-el0-rejection", "kunit:source-leaf-cbcc-imm-non-el0" },
	{ "CBLO_32_imm", CLASS_NON_EL0,
	  "pinned-source:feat-cmpbr-el0-rejection", "kunit:source-leaf-cbcc-imm-non-el0" },
	{ "CBEQ_32_imm", CLASS_NON_EL0,
	  "pinned-source:feat-cmpbr-el0-rejection", "kunit:source-leaf-cbcc-imm-non-el0" },
	{ "CBNE_32_imm", CLASS_NON_EL0,
	  "pinned-source:feat-cmpbr-el0-rejection", "kunit:source-leaf-cbcc-imm-non-el0" },
	{ "CBGT_64_imm", CLASS_NON_EL0,
	  "pinned-source:feat-cmpbr-el0-rejection", "kunit:source-leaf-cbcc-imm-non-el0" },
	{ "CBLT_64_imm", CLASS_NON_EL0,
	  "pinned-source:feat-cmpbr-el0-rejection", "kunit:source-leaf-cbcc-imm-non-el0" },
	{ "CBHI_64_imm", CLASS_NON_EL0,
	  "pinned-source:feat-cmpbr-el0-rejection", "kunit:source-leaf-cbcc-imm-non-el0" },
	{ "CBLO_64_imm", CLASS_NON_EL0,
	  "pinned-source:feat-cmpbr-el0-rejection", "kunit:source-leaf-cbcc-imm-non-el0" },
	{ "CBEQ_64_imm", CLASS_NON_EL0,
	  "pinned-source:feat-cmpbr-el0-rejection", "kunit:source-leaf-cbcc-imm-non-el0" },
	{ "CBNE_64_imm", CLASS_NON_EL0,
	  "pinned-source:feat-cmpbr-el0-rejection", "kunit:source-leaf-cbcc-imm-non-el0" },
	{ "CTZ_32_dp_1src", CLASS_NON_EL0,
	  "pinned-source:feat-cssc-el0-rejection", "kunit:source-leaf-ctz-non-el0" },
	{ "CTZ_64_dp_1src", CLASS_NON_EL0,
	  "pinned-source:feat-cssc-el0-rejection", "kunit:source-leaf-ctz-non-el0" },
	{ "CNT_32_dp_1src", CLASS_NON_EL0,
	  "pinned-source:feat-cssc-el0-rejection", "kunit:source-leaf-cnt-non-el0" },
	{ "CNT_64_dp_1src", CLASS_NON_EL0,
	  "pinned-source:feat-cssc-el0-rejection", "kunit:source-leaf-cnt-non-el0" },
	{ "ABS_32_dp_1src", CLASS_NON_EL0,
	  "pinned-source:feat-cssc-el0-rejection", "kunit:source-leaf-abs-non-el0" },
	{ "ABS_64_dp_1src", CLASS_NON_EL0,
	  "pinned-source:feat-cssc-el0-rejection", "kunit:source-leaf-abs-non-el0" },
	{ "LUTI4_asimdtbl_L7", CLASS_NON_EL0,
	  "pinned-source:feat-lut-el0-rejection", "kunit:source-leaf-luti4-non-el0" },
	{ "LUTI4_asimdtbl_L5", CLASS_NON_EL0,
	  "pinned-source:feat-lut-el0-rejection", "kunit:source-leaf-luti4-non-el0" },
	{ "LUTI2_asimdtbl_L5", CLASS_NON_EL0,
	  "pinned-source:feat-lut-el0-rejection", "kunit:source-leaf-luti2-non-el0" },
	{ "LUTI2_asimdtbl_L6", CLASS_NON_EL0,
	  "pinned-source:feat-lut-el0-rejection", "kunit:source-leaf-luti2-non-el0" },
	{ "SQRDMLAH_asisdsame2_only", CLASS_NON_EL0,
	  "pinned-source:feat-rdm-el0-rejection", "kunit:source-leaf-sqrdmlah-vec-non-el0" },
	{ "SQRDMLSH_asisdsame2_only", CLASS_NON_EL0,
	  "pinned-source:feat-rdm-el0-rejection", "kunit:source-leaf-sqrdmlsh-vec-non-el0" },
	{ "SQRDMLAH_asisdelem_R", CLASS_NON_EL0,
	  "pinned-source:feat-rdm-el0-rejection", "kunit:source-leaf-sqrdmlah-elt-non-el0" },
	{ "SQRDMLSH_asisdelem_R", CLASS_NON_EL0,
	  "pinned-source:feat-rdm-el0-rejection", "kunit:source-leaf-sqrdmlsh-elt-non-el0" },
	{ "SQRDMLAH_asimdsame2_only", CLASS_NON_EL0,
	  "pinned-source:feat-rdm-el0-rejection", "kunit:source-leaf-sqrdmlah-vec-non-el0" },
	{ "SQRDMLSH_asimdsame2_only", CLASS_NON_EL0,
	  "pinned-source:feat-rdm-el0-rejection", "kunit:source-leaf-sqrdmlsh-vec-non-el0" },
	{ "SQRDMLAH_asimdelem_R", CLASS_NON_EL0,
	  "pinned-source:feat-rdm-el0-rejection", "kunit:source-leaf-sqrdmlah-elt-non-el0" },
	{ "SQRDMLSH_asimdelem_R", CLASS_NON_EL0,
	  "pinned-source:feat-rdm-el0-rejection", "kunit:source-leaf-sqrdmlsh-elt-non-el0" },
	{ "SDOT_asimdsame2_D", CLASS_NON_EL0,
	  "pinned-source:feat-dotprod-el0-rejection", "kunit:source-leaf-sdot-vec-non-el0" },
	{ "USDOT_asimdsame2_D", CLASS_NON_EL0,
	  "pinned-source:feat-dotprod-el0-rejection", "kunit:source-leaf-usdot-vec-non-el0" },
	{ "UDOT_asimdsame2_D", CLASS_NON_EL0,
	  "pinned-source:feat-dotprod-el0-rejection", "kunit:source-leaf-udot-vec-non-el0" },
	{ "SDOT_asimdelem_D", CLASS_NON_EL0,
	  "pinned-source:feat-dotprod-el0-rejection", "kunit:source-leaf-sdot-elt-non-el0" },
	{ "SUDOT_asimdelem_D", CLASS_NON_EL0,
	  "pinned-source:feat-dotprod-el0-rejection", "kunit:source-leaf-sudot-elt-non-el0" },
	{ "USDOT_asimdelem_D", CLASS_NON_EL0,
	  "pinned-source:feat-dotprod-el0-rejection", "kunit:source-leaf-usdot-elt-non-el0" },
	{ "UDOT_asimdelem_D", CLASS_NON_EL0,
	  "pinned-source:feat-dotprod-el0-rejection", "kunit:source-leaf-udot-elt-non-el0" },
	{ "SMMLA_asimdsame2_G", CLASS_NON_EL0,
	  "pinned-source:feat-i8mm-el0-rejection", "kunit:source-leaf-smmla-non-el0" },
	{ "USMMLA_asimdsame2_G", CLASS_NON_EL0,
	  "pinned-source:feat-i8mm-el0-rejection", "kunit:source-leaf-usmmla-non-el0" },
	{ "UMMLA_asimdsame2_G", CLASS_NON_EL0,
	  "pinned-source:feat-i8mm-el0-rejection", "kunit:source-leaf-ummla-non-el0" },
	{ "FMULX_asisdsamefp16_only", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fmulx-advsimd-vec-non-el0" },
	{ "FCMEQ_asisdsamefp16_only", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fcmeq-advsimd-reg-non-el0" },
	{ "FRECPS_asisdsamefp16_only", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-frecps-advsimd-non-el0" },
	{ "FRSQRTS_asisdsamefp16_only", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-frsqrts-advsimd-non-el0" },
	{ "FCMGE_asisdsamefp16_only", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fcmge-advsimd-reg-non-el0" },
	{ "FACGE_asisdsamefp16_only", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-facge-advsimd-non-el0" },
	{ "FABD_asisdsamefp16_only", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fabd-advsimd-non-el0" },
	{ "FCMGT_asisdsamefp16_only", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fcmgt-advsimd-reg-non-el0" },
	{ "FACGT_asisdsamefp16_only", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-facgt-advsimd-non-el0" },
	{ "FCVTNS_asisdmiscfp16_R", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fcvtns-advsimd-non-el0" },
	{ "FCVTMS_asisdmiscfp16_R", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fcvtms-advsimd-non-el0" },
	{ "FCVTAS_asisdmiscfp16_R", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fcvtas-advsimd-non-el0" },
	{ "SCVTF_asisdmiscfp16_R", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-scvtf-advsimd-int-non-el0" },
	{ "FCMGT_asisdmiscfp16_FZ", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fcmgt-advsimd-zero-non-el0" },
	{ "FCMEQ_asisdmiscfp16_FZ", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fcmeq-advsimd-zero-non-el0" },
	{ "FCMLT_asisdmiscfp16_FZ", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fcmlt-advsimd-non-el0" },
	{ "FCVTPS_asisdmiscfp16_R", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fcvtps-advsimd-non-el0" },
	{ "FCVTZS_asisdmiscfp16_R", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fcvtzs-advsimd-int-non-el0" },
	{ "FRECPE_asisdmiscfp16_R", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-frecpe-advsimd-non-el0" },
	{ "FRECPX_asisdmiscfp16_R", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-frecpx-advsimd-non-el0" },
	{ "FCVTNU_asisdmiscfp16_R", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fcvtnu-advsimd-non-el0" },
	{ "FCVTMU_asisdmiscfp16_R", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fcvtmu-advsimd-non-el0" },
	{ "FCVTAU_asisdmiscfp16_R", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fcvtau-advsimd-non-el0" },
	{ "UCVTF_asisdmiscfp16_R", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-ucvtf-advsimd-int-non-el0" },
	{ "FCMGE_asisdmiscfp16_FZ", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fcmge-advsimd-zero-non-el0" },
	{ "FCMLE_asisdmiscfp16_FZ", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fcmle-advsimd-non-el0" },
	{ "FCVTPU_asisdmiscfp16_R", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fcvtpu-advsimd-non-el0" },
	{ "FCVTZU_asisdmiscfp16_R", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fcvtzu-advsimd-int-non-el0" },
	{ "FRSQRTE_asisdmiscfp16_R", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-frsqrte-advsimd-non-el0" },
	{ "FMAXNMP_asisdpair_only_H", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fmaxnmp-advsimd-pair-non-el0" },
	{ "FADDP_asisdpair_only_H", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-faddp-advsimd-pair-non-el0" },
	{ "FMAXP_asisdpair_only_H", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fmaxp-advsimd-pair-non-el0" },
	{ "FMINNMP_asisdpair_only_H", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fminnmp-advsimd-pair-non-el0" },
	{ "FMINP_asisdpair_only_H", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fminp-advsimd-pair-non-el0" },
	{ "FMLA_asisdelem_RH_H", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fmla-advsimd-elt-non-el0" },
	{ "FMLS_asisdelem_RH_H", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fmls-advsimd-elt-non-el0" },
	{ "FMUL_asisdelem_RH_H", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fmul-advsimd-elt-non-el0" },
	{ "FMULX_asisdelem_RH_H", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fmulx-advsimd-elt-non-el0" },
	{ "FMAXNM_asimdsamefp16_only", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fmaxnm-advsimd-non-el0" },
	{ "FMLA_asimdsamefp16_only", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fmla-advsimd-vec-non-el0" },
	{ "FADD_asimdsamefp16_only", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fadd-advsimd-non-el0" },
	{ "FMULX_asimdsamefp16_only", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fmulx-advsimd-vec-non-el0" },
	{ "FCMEQ_asimdsamefp16_only", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fcmeq-advsimd-reg-non-el0" },
	{ "FMAX_asimdsamefp16_only", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fmax-advsimd-non-el0" },
	{ "FRECPS_asimdsamefp16_only", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-frecps-advsimd-non-el0" },
	{ "FMINNM_asimdsamefp16_only", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fminnm-advsimd-non-el0" },
	{ "FMLS_asimdsamefp16_only", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fmls-advsimd-vec-non-el0" },
	{ "FSUB_asimdsamefp16_only", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fsub-advsimd-non-el0" },
	{ "FAMAX_asimdsamefp16_only", CLASS_NON_EL0,
	  "pinned-source:feat-faminmax-el0-rejection", "kunit:source-leaf-famax-advsimd-non-el0" },
	{ "FMIN_asimdsamefp16_only", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fmin-advsimd-non-el0" },
	{ "FRSQRTS_asimdsamefp16_only", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-frsqrts-advsimd-non-el0" },
	{ "FMAXNMP_asimdsamefp16_only", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fmaxnmp-advsimd-vec-non-el0" },
	{ "FADDP_asimdsamefp16_only", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-faddp-advsimd-vec-non-el0" },
	{ "FMUL_asimdsamefp16_only", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fmul-advsimd-vec-non-el0" },
	{ "FCMGE_asimdsamefp16_only", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fcmge-advsimd-reg-non-el0" },
	{ "FACGE_asimdsamefp16_only", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-facge-advsimd-non-el0" },
	{ "FMAXP_asimdsamefp16_only", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fmaxp-advsimd-vec-non-el0" },
	{ "FDIV_asimdsamefp16_only", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fdiv-advsimd-non-el0" },
	{ "FMINNMP_asimdsamefp16_only", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fminnmp-advsimd-vec-non-el0" },
	{ "FABD_asimdsamefp16_only", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fabd-advsimd-non-el0" },
	{ "FAMIN_asimdsamefp16_only", CLASS_NON_EL0,
	  "pinned-source:feat-faminmax-el0-rejection", "kunit:source-leaf-famin-advsimd-non-el0" },
	{ "FCMGT_asimdsamefp16_only", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fcmgt-advsimd-reg-non-el0" },
	{ "FACGT_asimdsamefp16_only", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-facgt-advsimd-non-el0" },
	{ "FMINP_asimdsamefp16_only", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fminp-advsimd-vec-non-el0" },
	{ "FSCALE_asimdsamefp16_only", CLASS_NON_EL0,
	  "pinned-source:feat-fscale-el0-rejection", "kunit:source-leaf-fscale-advsimd-non-el0" },
	{ "FRINTN_asimdmiscfp16_R", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-frintn-advsimd-non-el0" },
	{ "FRINTM_asimdmiscfp16_R", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-frintm-advsimd-non-el0" },
	{ "FCVTNS_asimdmiscfp16_R", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fcvtns-advsimd-non-el0" },
	{ "FCVTMS_asimdmiscfp16_R", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fcvtms-advsimd-non-el0" },
	{ "FCVTAS_asimdmiscfp16_R", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fcvtas-advsimd-non-el0" },
	{ "SCVTF_asimdmiscfp16_R", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-scvtf-advsimd-int-non-el0" },
	{ "FCMGT_asimdmiscfp16_FZ", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fcmgt-advsimd-zero-non-el0" },
	{ "FCMEQ_asimdmiscfp16_FZ", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fcmeq-advsimd-zero-non-el0" },
	{ "FCMLT_asimdmiscfp16_FZ", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fcmlt-advsimd-non-el0" },
	{ "FABS_asimdmiscfp16_R", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fabs-advsimd-non-el0" },
	{ "FRINTP_asimdmiscfp16_R", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-frintp-advsimd-non-el0" },
	{ "FRINTZ_asimdmiscfp16_R", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-frintz-advsimd-non-el0" },
	{ "FCVTPS_asimdmiscfp16_R", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fcvtps-advsimd-non-el0" },
	{ "FCVTZS_asimdmiscfp16_R", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fcvtzs-advsimd-int-non-el0" },
	{ "FRECPE_asimdmiscfp16_R", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-frecpe-advsimd-non-el0" },
	{ "FRINTA_asimdmiscfp16_R", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-frinta-advsimd-non-el0" },
	{ "FRINTX_asimdmiscfp16_R", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-frintx-advsimd-non-el0" },
	{ "FCVTNU_asimdmiscfp16_R", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fcvtnu-advsimd-non-el0" },
	{ "FCVTMU_asimdmiscfp16_R", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fcvtmu-advsimd-non-el0" },
	{ "FCVTAU_asimdmiscfp16_R", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fcvtau-advsimd-non-el0" },
	{ "UCVTF_asimdmiscfp16_R", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-ucvtf-advsimd-int-non-el0" },
	{ "FCMGE_asimdmiscfp16_FZ", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fcmge-advsimd-zero-non-el0" },
	{ "FCMLE_asimdmiscfp16_FZ", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fcmle-advsimd-non-el0" },
	{ "FNEG_asimdmiscfp16_R", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fneg-advsimd-non-el0" },
	{ "FRINTI_asimdmiscfp16_R", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-frinti-advsimd-non-el0" },
	{ "FCVTPU_asimdmiscfp16_R", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fcvtpu-advsimd-non-el0" },
	{ "FCVTZU_asimdmiscfp16_R", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fcvtzu-advsimd-int-non-el0" },
	{ "FRSQRTE_asimdmiscfp16_R", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-frsqrte-advsimd-non-el0" },
	{ "FSQRT_asimdmiscfp16_R", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fsqrt-advsimd-non-el0" },
	{ "FCVTN_asimdsame2_H", CLASS_NON_EL0,
	  "pinned-source:feat-fp8-el0-rejection", "kunit:source-leaf-fcvtn-advsimd-328-non-el0" },
	{ "FDOT_asimdsame2_DD", CLASS_NON_EL0,
	  "pinned-source:feat-bf16-el0-rejection", "kunit:source-leaf-fdot-advsimd-4wayvec-non-el0" },
	{ "FCVTN_asimdsame2_D", CLASS_NON_EL0,
	  "pinned-source:feat-fp8-el0-rejection", "kunit:source-leaf-fcvtn-advsimd-168-non-el0" },
	{ "FDOT_asimdsame2_D", CLASS_NON_EL0,
	  "pinned-source:feat-bf16-el0-rejection", "kunit:source-leaf-fdot-advsimd-2wayvec-non-el0" },
	{ "FDOT_asimdsame2_FP16FP32", CLASS_NON_EL0,
	  "pinned-source:feat-bf16-el0-rejection", "kunit:source-leaf-fdot-advsimd-fp16fp32-non-el0" },
	{ "FCMLA_asimdsame2_C", CLASS_NON_EL0,
	  "pinned-source:feat-fcma-el0-rejection", "kunit:source-leaf-fcmla-advsimd-vec-non-el0" },
	{ "FCADD_asimdsame2_C", CLASS_NON_EL0,
	  "pinned-source:feat-fcma-el0-rejection", "kunit:source-leaf-fcadd-advsimd-vec-non-el0" },
	{ "BFDOT_asimdsame2_D", CLASS_NON_EL0,
	  "pinned-source:feat-bf16-el0-rejection", "kunit:source-leaf-bfdot-advsimd-vec-non-el0" },
	{ "BFMLAL_asimdsame2_F_", CLASS_NON_EL0,
	  "pinned-source:feat-fhm-el0-rejection", "kunit:source-leaf-bfmlal-advsimd-vec-non-el0" },
	{ "FMLALLBB_asimdsame2_G", CLASS_NON_EL0,
	  "pinned-source:feat-fhm-el0-rejection", "kunit:source-leaf-fmlallbb-advsimd-vec-non-el0" },
	{ "FMLALLBT_asimdsame2_G", CLASS_NON_EL0,
	  "pinned-source:feat-fhm-el0-rejection", "kunit:source-leaf-fmlallbb-advsimd-vec-non-el0" },
	{ "FMLALB_asimdsame2_J", CLASS_NON_EL0,
	  "pinned-source:feat-fhm-el0-rejection", "kunit:source-leaf-fmlalb-advsimd-vec-non-el0" },
	{ "FMLALLTB_asimdsame2_G", CLASS_NON_EL0,
	  "pinned-source:feat-fhm-el0-rejection", "kunit:source-leaf-fmlallbb-advsimd-vec-non-el0" },
	{ "FMLALLTT_asimdsame2_G", CLASS_NON_EL0,
	  "pinned-source:feat-fhm-el0-rejection", "kunit:source-leaf-fmlallbb-advsimd-vec-non-el0" },
	{ "FMMLA_asimd_FP16FP16", CLASS_NON_EL0,
	  "pinned-source:feat-bf16-el0-rejection", "kunit:source-leaf-fmmla-advsimd-fp16fp16-non-el0" },
	{ "FMLALT_asimdsame2_J", CLASS_NON_EL0,
	  "pinned-source:feat-fhm-el0-rejection", "kunit:source-leaf-fmlalb-advsimd-vec-non-el0" },
	{ "FMMLA_asimd_FP8FP16", CLASS_NON_EL0,
	  "pinned-source:feat-bf16-el0-rejection", "kunit:source-leaf-fmmla-fp8fp16-non-el0" },
	{ "FMMLA_asimd_FP16FP32", CLASS_NON_EL0,
	  "pinned-source:feat-bf16-el0-rejection", "kunit:source-leaf-fmmla-advsimd-fp16fp32-non-el0" },
	{ "BFMMLA_asimdsame2_E", CLASS_NON_EL0,
	  "pinned-source:feat-bf16-el0-rejection", "kunit:source-leaf-bfmmla-advsimd-non-el0" },
	{ "FMMLA_asimd_FP8FP32", CLASS_NON_EL0,
	  "pinned-source:feat-bf16-el0-rejection", "kunit:source-leaf-fmmla-fp8fp32-non-el0" },
	{ "FRINT32Z_asimdmisc_R", CLASS_NON_EL0,
	  "pinned-source:feat-frintts-el0-rejection", "kunit:source-leaf-frint32z-advsimd-non-el0" },
	{ "FRINT64Z_asimdmisc_R", CLASS_NON_EL0,
	  "pinned-source:feat-frintts-el0-rejection", "kunit:source-leaf-frint64z-advsimd-non-el0" },
	{ "BFCVTN_asimdmisc_4S", CLASS_NON_EL0,
	  "pinned-source:feat-bf16-el0-rejection", "kunit:source-leaf-bfcvtn-advsimd-non-el0" },
	{ "FRINT32X_asimdmisc_R", CLASS_NON_EL0,
	  "pinned-source:feat-frintts-el0-rejection", "kunit:source-leaf-frint32x-advsimd-non-el0" },
	{ "FRINT64X_asimdmisc_R", CLASS_NON_EL0,
	  "pinned-source:feat-frintts-el0-rejection", "kunit:source-leaf-frint64x-advsimd-non-el0" },
	{ "F1CVTL_asimdmisc_V", CLASS_NON_EL0,
	  "pinned-source:feat-fp8-el0-rejection", "kunit:source-leaf-f12cvtl-advsimd-non-el0" },
	{ "F2CVTL_asimdmisc_V", CLASS_NON_EL0,
	  "pinned-source:feat-fp8-el0-rejection", "kunit:source-leaf-f12cvtl-advsimd-non-el0" },
	{ "BF1CVTL_asimdmisc_V", CLASS_NON_EL0,
	  "pinned-source:feat-bf16-el0-rejection", "kunit:source-leaf-bf12cvtl-advsimd-non-el0" },
	{ "BF2CVTL_asimdmisc_V", CLASS_NON_EL0,
	  "pinned-source:feat-bf16-el0-rejection", "kunit:source-leaf-bf12cvtl-advsimd-non-el0" },
	{ "FMAXNMV_asimdall_only_H", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fmaxnmv-advsimd-non-el0" },
	{ "FMAXV_asimdall_only_H", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fmaxv-advsimd-non-el0" },
	{ "FMINNMV_asimdall_only_H", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fminnmv-advsimd-non-el0" },
	{ "FMINV_asimdall_only_H", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fminv-advsimd-non-el0" },
	{ "FMLAL_asimdsame_F", CLASS_NON_EL0,
	  "pinned-source:feat-fhm-el0-rejection", "kunit:source-leaf-fmlal-advsimd-vec-non-el0" },
	{ "FAMAX_asimdsame_only", CLASS_NON_EL0,
	  "pinned-source:feat-faminmax-el0-rejection", "kunit:source-leaf-famax-advsimd-non-el0" },
	{ "FMLSL_asimdsame_F", CLASS_NON_EL0,
	  "pinned-source:feat-fhm-el0-rejection", "kunit:source-leaf-fmlsl-advsimd-vec-non-el0" },
	{ "FMLAL2_asimdsame_F", CLASS_NON_EL0,
	  "pinned-source:feat-fhm-el0-rejection", "kunit:source-leaf-fmlal-advsimd-vec-non-el0" },
	{ "FAMIN_asimdsame_only", CLASS_NON_EL0,
	  "pinned-source:feat-faminmax-el0-rejection", "kunit:source-leaf-famin-advsimd-non-el0" },
	{ "FSCALE_asimdsame_only", CLASS_NON_EL0,
	  "pinned-source:feat-fscale-el0-rejection", "kunit:source-leaf-fscale-advsimd-non-el0" },
	{ "FMLSL2_asimdsame_F", CLASS_NON_EL0,
	  "pinned-source:feat-fhm-el0-rejection", "kunit:source-leaf-fmlsl-advsimd-vec-non-el0" },
	{ "FMOV_asimdimm_H_h", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fmov-advsimd-non-el0" },
	{ "FDOT_asimdelem_D", CLASS_NON_EL0,
	  "pinned-source:feat-bf16-el0-rejection", "kunit:source-leaf-fdot-advsimd-4wayelem-non-el0" },
	{ "FMLA_asimdelem_RH_H", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fmla-advsimd-elt-non-el0" },
	{ "FMLS_asimdelem_RH_H", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fmls-advsimd-elt-non-el0" },
	{ "FMUL_asimdelem_RH_H", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fmul-advsimd-elt-non-el0" },
	{ "FDOT_asimdelem_G", CLASS_NON_EL0,
	  "pinned-source:feat-bf16-el0-rejection", "kunit:source-leaf-fdot-advsimd-2wayelem-non-el0" },
	{ "FDOT_asimdelem_FP16FP32", CLASS_NON_EL0,
	  "pinned-source:feat-bf16-el0-rejection", "kunit:source-leaf-fdot-advsimd-elt-fp16fp32-non-el0" },
	{ "BFDOT_asimdelem_E", CLASS_NON_EL0,
	  "pinned-source:feat-bf16-el0-rejection", "kunit:source-leaf-bfdot-advsimd-elt-non-el0" },
	{ "FMLAL_asimdelem_LH", CLASS_NON_EL0,
	  "pinned-source:feat-fhm-el0-rejection", "kunit:source-leaf-fmlal-advsimd-elt-non-el0" },
	{ "FMLSL_asimdelem_LH", CLASS_NON_EL0,
	  "pinned-source:feat-fhm-el0-rejection", "kunit:source-leaf-fmlsl-advsimd-elt-non-el0" },
	{ "BFMLAL_asimdelem_F", CLASS_NON_EL0,
	  "pinned-source:feat-fhm-el0-rejection", "kunit:source-leaf-bfmlal-advsimd-elt-non-el0" },
	{ "FMULX_asimdelem_RH_H", CLASS_NON_EL0,
	  "pinned-source:feat-fp16-el0-rejection", "kunit:source-leaf-fmulx-advsimd-elt-non-el0" },
	{ "FCMLA_advsimd_elt", CLASS_NON_EL0,
	  "pinned-source:feat-fcma-el0-rejection", "kunit:source-leaf-fcmla-advsimd-elt-non-el0" },
	{ "FMLAL2_asimdelem_LH", CLASS_NON_EL0,
	  "pinned-source:feat-fhm-el0-rejection", "kunit:source-leaf-fmlal-advsimd-elt-non-el0" },
	{ "FMLSL2_asimdelem_LH", CLASS_NON_EL0,
	  "pinned-source:feat-fhm-el0-rejection", "kunit:source-leaf-fmlsl-advsimd-elt-non-el0" },
	{ "FMLALB_asimdelem_H", CLASS_NON_EL0,
	  "pinned-source:feat-fhm-el0-rejection", "kunit:source-leaf-fmlalb-advsimd-elem-non-el0" },
	{ "FMLALLBB_asimdelem_J", CLASS_NON_EL0,
	  "pinned-source:feat-fhm-el0-rejection", "kunit:source-leaf-fmlallbb-advsimd-elem-non-el0" },
	{ "FMLALLBT_asimdelem_J", CLASS_NON_EL0,
	  "pinned-source:feat-fhm-el0-rejection", "kunit:source-leaf-fmlallbb-advsimd-elem-non-el0" },
	{ "FMLALT_asimdelem_H", CLASS_NON_EL0,
	  "pinned-source:feat-fhm-el0-rejection", "kunit:source-leaf-fmlalb-advsimd-elem-non-el0" },
	{ "FMLALLTB_asimdelem_J", CLASS_NON_EL0,
	  "pinned-source:feat-fhm-el0-rejection", "kunit:source-leaf-fmlallbb-advsimd-elem-non-el0" },
	{ "FMLALLTT_asimdelem_J", CLASS_NON_EL0,
	  "pinned-source:feat-fhm-el0-rejection", "kunit:source-leaf-fmlallbb-advsimd-elem-non-el0" },
};

static const struct reviewed_classification_binding *
reviewed_binding_for(const char *name)
{
	size_t index;

	for (index = 0; index < sizeof(reviewed_bindings) /
			     sizeof(reviewed_bindings[0]); index++)
		if (!strcmp(reviewed_bindings[index].name, name))
			return &reviewed_bindings[index];
	return NULL;
}

static const char *classification_name(enum classification classification)
{
	switch (classification) {
	case CLASS_UNCLASSIFIED:
		return "UNCLASSIFIED";
	case CLASS_NON_EL0:
		return "NON_EL0";
	case CLASS_ARCH_UNDEFINED:
		return "ARCHITECTURALLY_UNDEFINED";
	}
	return NULL;
}

static int validate_source_manifest(const struct orlix_tcti_target_inventory *inventory,
				    const char *bytes, size_t length)
{
	char digest[65];
	size_t index;

	orlix_tcti_target_inventory_sha256(bytes, length, digest);
	if (length != source_provenance.source_length ||
	    strcmp(digest, source_provenance.sha256)) {
		fprintf(stderr, "pinned source digest differs from source manifest\n");
		return -1;
	}
	if (inventory->leaf_count != sizeof(source_manifest) /
				     sizeof(source_manifest[0])) {
		fprintf(stderr, "pinned source leaf count differs from source manifest\n");
		return -1;
	}
	for (index = 0; index < inventory->leaf_count; index++) {
		const struct orlix_tcti_target_leaf *leaf = &inventory->leaves[index];
		const struct source_manifest_row *manifest = &source_manifest[index];

		if (strcmp(leaf->name, manifest->name) ||
		    strcmp(leaf->mnemonic, manifest->mnemonic) ||
		    strcmp(leaf->operation_id, manifest->operation_id) ||
		    leaf->encoding_mask != manifest->mask ||
		    leaf->encoding_pattern != manifest->pattern) {
			fprintf(stderr, "pinned source differs from manifest at ordinal %zu\n",
				index);
			return -1;
		}
	}
	return 0;
}

static int emit_seed(FILE *output, const struct orlix_tcti_target_inventory *inventory)
{
	size_t index;

	fputs("/* SPDX-License-Identifier: GPL-2.0-only */\n"
	      "/* Bootstrap seed. Copy reviewed fields into target_classification.def. */\n"
	      "/* Every blank field is a deliberate completion blocker. */\n",
	      output);
	for (index = 0; index < inventory->leaf_count; index++) {
		const struct reviewed_classification_binding *binding =
			reviewed_binding_for(inventory->leaves[index].name);
		enum classification classification = binding ? binding->classification :
			CLASS_UNCLASSIFIED;
		const char *evidence = binding ? binding->evidence : "";
		const char *proof_id = binding ? binding->proof_id : "";

		if (fprintf(output,
			"ORLIX_TCTI_A64_TARGET_CLASSIFICATION(%s, %s, "
			"ORLIX_TCTI_A64_TARGET_RELATION_NONE, \"\", \"%s\", \"%s\")\n",
			inventory->leaves[index].name, classification_name(classification),
			evidence, proof_id) < 0)
			return -1;
	}
	return ferror(output) ? -1 : 0;
}

int main(int argc, char **argv)
{
	struct orlix_tcti_target_inventory inventory = { 0 };
	struct orlix_tcti_target_import_error error = { 0 };
	const char *instructions;
	FILE *input;
	long length;
	char *bytes = NULL;
	int status = EXIT_FAILURE;

	if (argc != 2) {
		fprintf(stderr, "usage: %s Instructions.json\n", argv[0]);
		return EXIT_FAILURE;
	}
	instructions = argv[1];
	input = fopen(instructions, "rb");
	if (!input || fseek(input, 0, SEEK_END) || (length = ftell(input)) < 0 ||
	    fseek(input, 0, SEEK_SET))
		goto out;
	if ((uintmax_t)length > SIZE_MAX - 1U)
		goto out;
	bytes = malloc((size_t)length + 1U);
	if (!bytes || fread(bytes, 1, (size_t)length, input) != (size_t)length)
		goto out;
	bytes[length] = '\0';
	if (orlix_tcti_target_inventory_import(bytes, (size_t)length, &inventory, &error)) {
		fprintf(stderr, "import error %d at %zu: %s\n", error.code,
			error.offset, error.message);
		goto out;
	}
	status = validate_source_manifest(&inventory, bytes, (size_t)length) ||
		emit_seed(stdout, &inventory) ?
		EXIT_FAILURE : EXIT_SUCCESS;
out:
	if (input)
		fclose(input);
	orlix_tcti_target_inventory_destroy(&inventory);
	free(bytes);
	return status;
}
