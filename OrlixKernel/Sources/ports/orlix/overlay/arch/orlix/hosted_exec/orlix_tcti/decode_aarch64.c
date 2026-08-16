// SPDX-License-Identifier: GPL-2.0-only
#include <linux/array_size.h>
#include <linux/bits.h>
#include <linux/bitops.h>
#include <linux/errno.h>

#include "decode_aarch64.h"
#include "system_accessor.h"

#define AARCH64_SVC_MASK 0xffe0001fU
#define AARCH64_SVC_PATTERN 0xd4000001U
#define AARCH64_BRK_MASK 0xffe0001fU
#define AARCH64_BRK_PATTERN 0xd4200000U
#define AARCH64_HLT_MASK 0xffe0001fU
#define AARCH64_HLT_PATTERN 0xd4400000U
#define AARCH64_UDF_MASK 0xffff0000U
#define AARCH64_UDF_PATTERN 0x00000000U
#define AARCH64_HVC_PATTERN 0xd4000002U
#define AARCH64_SMC_PATTERN 0xd4000003U
#define AARCH64_DCPS1_PATTERN 0xd4a00001U
#define AARCH64_DCPS2_PATTERN 0xd4a00002U
#define AARCH64_DCPS3_PATTERN 0xd4a00003U
#define AARCH64_SMSTOP_SM 0xd503427fU
#define AARCH64_SMSTART_SM 0xd503437fU
#define AARCH64_SMSTOP_ZA 0xd503447fU
#define AARCH64_SMSTART_ZA 0xd503457fU
#define AARCH64_SMSTOP_SM_ZA 0xd503467fU
#define AARCH64_SMSTART_SM_ZA 0xd503477fU
#define AARCH64_HINT_MASK 0xfffff01fU
#define AARCH64_HINT_PATTERN 0xd503201fU
#define AARCH64_BARRIER_MASK 0xfffff0ffU
#define AARCH64_DSB_PATTERN 0xd503309fU
/* AARCHMRS 2026-06 source ordinal 2276, DSB_BOn_barriers, FEAT_XS. */
#define AARCH64_DSB_NXS_MASK 0xfffff3ffU
#define AARCH64_DSB_NXS_PATTERN 0xd503323fU
#define AARCH64_DMB_PATTERN 0xd50330bfU
#define AARCH64_ISB_PATTERN 0xd50330dfU
#define AARCH64_CLREX_PATTERN 0xd503305fU
#define AARCH64_CACHE_MAINTENANCE_MASK 0xffffffe0U
#define AARCH64_IC_IVAU_PATTERN 0xd50b7520U
#define AARCH64_DC_CVAC_PATTERN 0xd50b7a20U
#define AARCH64_DC_CVAU_PATTERN 0xd50b7b20U
#define AARCH64_DC_CIVAC_PATTERN 0xd50b7e20U
#define AARCH64_PC_RELATIVE_ADDRESS_MASK 0x1f000000U
#define AARCH64_PC_RELATIVE_ADDRESS_PATTERN 0x10000000U
#define AARCH64_ADD_SUB_IMM_MASK 0x1f000000U
#define AARCH64_ADD_SUB_IMM_PATTERN 0x11000000U
#define AARCH64_MIN_MAX_IMM_MASK 0x7ff00000U
#define AARCH64_MIN_MAX_IMM_PATTERN 0x11c00000U
#define AARCH64_ADD_SUB_SHIFTED_REG_MASK 0x1f200000U
#define AARCH64_ADD_SUB_SHIFTED_REG_PATTERN 0x0b000000U
#define AARCH64_ADD_SUB_EXTENDED_REG_MASK 0x1f200000U
#define AARCH64_ADD_SUB_EXTENDED_REG_PATTERN 0x0b200000U
#define AARCH64_ADD_SUB_WITH_CARRY_MASK 0x1fe0fc00U
#define AARCH64_ADD_SUB_WITH_CARRY_PATTERN 0x1a000000U
#define AARCH64_UNCONDITIONAL_BRANCH_IMM_MASK 0x7c000000U
#define AARCH64_UNCONDITIONAL_BRANCH_IMM_PATTERN 0x14000000U
#define AARCH64_BRANCH_REGISTER_MASK 0xfffffc1fU
#define AARCH64_BR_PATTERN 0xd61f0000U
#define AARCH64_BLR_PATTERN 0xd63f0000U
#define AARCH64_RET_PATTERN 0xd65f0000U
#define AARCH64_COMPARE_BRANCH_IMM_MASK 0x7e000000U
#define AARCH64_COMPARE_BRANCH_IMM_PATTERN 0x34000000U
#define AARCH64_COMPARE_BRANCH_EXTENSION_MASK 0x7c000000U
#define AARCH64_COMPARE_BRANCH_EXTENSION_PATTERN 0x74000000U
#define AARCH64_TEST_BRANCH_IMM_MASK 0x7e000000U
#define AARCH64_TEST_BRANCH_IMM_PATTERN 0x36000000U
#define AARCH64_CONDITIONAL_BRANCH_IMM_MASK 0xff000010U
#define AARCH64_CONDITIONAL_BRANCH_IMM_PATTERN 0x54000000U
#define AARCH64_CONDITIONAL_COMPARE_MASK 0x3fe00410U
#define AARCH64_CONDITIONAL_COMPARE_PATTERN 0x3a400000U
#define AARCH64_CONDITIONAL_SELECT_MASK 0x3fe00800U
#define AARCH64_CONDITIONAL_SELECT_PATTERN 0x1a800000U
#define AARCH64_RMIF_MASK 0xffe07c10U
#define AARCH64_RMIF_PATTERN 0xba000400U
#define AARCH64_SETF_MASK 0xfffffc1fU
#define AARCH64_SETF8_PATTERN 0x3a00080dU
#define AARCH64_SETF16_PATTERN 0x3a00480dU
#define AARCH64_LOAD_LITERAL_MASK 0x3b000000U
#define AARCH64_LOAD_LITERAL_PATTERN 0x18000000U
#define AARCH64_LOAD_STORE_PAIR_MASK 0x3a000000U
#define AARCH64_LOAD_STORE_PAIR_PATTERN 0x28000000U
#define AARCH64_LOAD_STORE_UNSIGNED_IMM_MASK 0x3b000000U
#define AARCH64_LOAD_STORE_UNSIGNED_IMM_PATTERN 0x39000000U
#define AARCH64_LOAD_STORE_SIGNED_IMM_MASK 0x3b200000U
#define AARCH64_LOAD_STORE_SIGNED_IMM_PATTERN 0x38000000U
#define AARCH64_LOAD_STORE_REGISTER_OFFSET_MASK 0x3b200c00U
#define AARCH64_LOAD_STORE_REGISTER_OFFSET_PATTERN 0x38200800U
#define AARCH64_LOGICAL_SHIFTED_REGISTER_MASK 0x1f000000U
#define AARCH64_LOGICAL_SHIFTED_REGISTER_PATTERN 0x0a000000U
#define AARCH64_LOGICAL_IMMEDIATE_MASK 0x1f800000U
#define AARCH64_LOGICAL_IMMEDIATE_PATTERN 0x12000000U
#define AARCH64_BITFIELD_MASK 0x1f800000U
#define AARCH64_BITFIELD_PATTERN 0x13000000U
#define AARCH64_EXTRACT_MASK 0x7fa00000U
#define AARCH64_EXTRACT_PATTERN 0x13800000U
#define AARCH64_DATA_PROCESSING_1SOURCE_MASK 0x5fe00000U
#define AARCH64_DATA_PROCESSING_1SOURCE_PATTERN 0x5ac00000U
#define AARCH64_DP1_RBIT_OPCODE 0x00U
#define AARCH64_DP1_REV16_OPCODE 0x01U
#define AARCH64_DP1_REV32_OPCODE 0x02U
#define AARCH64_DP1_REV64_OPCODE 0x03U
#define AARCH64_DP1_CLZ_OPCODE 0x04U
#define AARCH64_DP1_CLS_OPCODE 0x05U
#define AARCH64_DATA_PROCESSING_2SOURCE_MASK 0x7fe00000U
#define AARCH64_DATA_PROCESSING_2SOURCE_PATTERN 0x1ac00000U
#define AARCH64_MULTIPLY_ADD_SUB_MASK 0x7f000000U
#define AARCH64_MULTIPLY_ADD_SUB_PATTERN 0x1b000000U
#define AARCH64_MOVE_WIDE_IMM_MASK 0x1f800000U
#define AARCH64_MOVE_WIDE_IMM_PATTERN 0x12800000U
#define AARCH64_LOAD_STORE_EXCLUSIVE_MASK 0x3f000000U
#define AARCH64_LOAD_STORE_EXCLUSIVE_PATTERN 0x08000000U
#define AARCH64_LSE_CAS_MASK 0x3fa07c00U
#define AARCH64_LSE_CAS_PATTERN 0x08a07c00U
#define AARCH64_LSE_CASP_MASK 0xbfa07c00U
#define AARCH64_LSE_CASP_PATTERN 0x08207c00U
#define AARCH64_LSE_RMW_MASK 0x3f200c00U
#define AARCH64_LSE_RMW_PATTERN 0x38200000U
#define AARCH64_LSE128_RMW_MASK 0xff200c00U
#define AARCH64_LSE128_RMW_PATTERN 0x19200000U
#define AARCH64_MOPS_COPY_MASK 0x3b200c00U
#define AARCH64_MOPS_COPY_PATTERN 0x19000400U
#define AARCH64_SIMD_MODIFIED_IMMEDIATE_MASK 0x9ff80c00U
#define AARCH64_SIMD_MODIFIED_IMMEDIATE_PATTERN 0x0f000400U
#define AARCH64_SIMD_VECTOR_ELEMENT_MOVE_MASK 0xffe08400U
#define AARCH64_SIMD_VECTOR_ELEMENT_MOVE_PATTERN 0x6e000400U
#define AARCH64_SIMD_XTN_MASK 0xbf3ffc00U
#define AARCH64_SIMD_XTN_PATTERN 0x0e212800U
#define AARCH64_SIMD_SHIFT_LEFT_LONG_MASK 0x8f80fc00U
#define AARCH64_SIMD_SHIFT_LEFT_LONG_PATTERN 0x0f00a400U
#define AARCH64_SIMD_PERMUTE_MASK 0xbf208c00U
#define AARCH64_SIMD_PERMUTE_PATTERN 0x0e000800U
#define AARCH64_SIMD_UMOV_MASK 0xbfe0fc00U
#define AARCH64_SIMD_UMOV_PATTERN 0x0e003c00U
#define AARCH64_SIMD_SMOV_MASK 0xbfe0fc00U
#define AARCH64_SIMD_SMOV_PATTERN 0x0e002c00U
#define AARCH64_SIMD_INS_GPR_MASK 0xffe0fc00U
#define AARCH64_SIMD_INS_GPR_PATTERN 0x4e001c00U
#define AARCH64_SIMD_EXT_MASK 0xbfe08400U
#define AARCH64_SIMD_EXT_PATTERN 0x2e000000U
#define AARCH64_SIMD_TABLE_LOOKUP_MASK 0xbfe08c00U
#define AARCH64_SIMD_TABLE_LOOKUP_PATTERN 0x0e000000U
#define AARCH64_SIMD_DUP_GPR_MASK 0xbfe0fc00U
#define AARCH64_SIMD_DUP_GPR_PATTERN 0x0e000c00U
#define AARCH64_SIMD_DUP_ELEMENT_MASK 0xbfe0fc00U
#define AARCH64_SIMD_DUP_ELEMENT_PATTERN 0x0e000400U
#define AARCH64_SIMD_DUP_SCALAR_ELEMENT_MASK 0xffe0fc00U
#define AARCH64_SIMD_DUP_SCALAR_ELEMENT_PATTERN 0x5e000400U
#define AARCH64_SIMD_VECTOR_LOGICAL_MASK 0x9f20fc00U
#define AARCH64_SIMD_VECTOR_LOGICAL_PATTERN 0x0e201c00U
#define AARCH64_SIMD_FP_THREE_SAME_MASK 0xafa0fc00U
#define AARCH64_SIMD_FABD_PATTERN 0x2ea0d400U
#define AARCH64_SIMD_FACGE_PATTERN 0x2e20ec00U
#define AARCH64_SIMD_FACGT_PATTERN 0x2ea0ec00U
#define AARCH64_SIMD_FCMEQ_PATTERN 0x0e20e400U
#define AARCH64_SIMD_FCMGE_PATTERN 0x2e20e400U
#define AARCH64_SIMD_FCMGT_PATTERN 0x2ea0e400U
#define AARCH64_SIMD_FMULX_PATTERN 0x0e20dc00U
#define AARCH64_SIMD_FRECPS_PATTERN 0x0e20fc00U
#define AARCH64_SIMD_FRSQRTS_PATTERN 0x0ea0fc00U
#define AARCH64_SIMD_FADDP_PATTERN 0x2e20d400U
#define AARCH64_SIMD_FADD_PATTERN 0x0e20d400U
#define AARCH64_SIMD_FDIV_PATTERN 0x2e20fc00U
#define AARCH64_SIMD_FMAXNMP_PATTERN 0x2e20c400U
#define AARCH64_SIMD_FMAXNM_PATTERN 0x0e20c400U
#define AARCH64_SIMD_FMAXP_PATTERN 0x2e20f400U
#define AARCH64_SIMD_FMAX_PATTERN 0x0e20f400U
#define AARCH64_SIMD_FMINNMP_PATTERN 0x2ea0c400U
#define AARCH64_SIMD_FMINNM_PATTERN 0x0ea0c400U
#define AARCH64_SIMD_FMINP_PATTERN 0x2ea0f400U
#define AARCH64_SIMD_FMIN_PATTERN 0x0ea0f400U
#define AARCH64_SIMD_FMLA_PATTERN 0x0e20cc00U
#define AARCH64_SIMD_FMLS_PATTERN 0x0ea0cc00U
#define AARCH64_SIMD_FMUL_PATTERN 0x2e20dc00U
#define AARCH64_SIMD_FSUB_PATTERN 0x0ea0d400U
#define AARCH64_SIMD_FP16_THREE_SAME_MASK 0xbfe0fc00U
#define AARCH64_SIMD_FMAXNM_FP16_PATTERN 0x0e400400U
#define AARCH64_SIMD_FMLA_FP16_PATTERN 0x0e400c00U
#define AARCH64_SIMD_FADD_FP16_PATTERN 0x0e401400U
#define AARCH64_SIMD_FMULX_FP16_PATTERN 0x0e401c00U
#define AARCH64_SIMD_FCMEQ_FP16_PATTERN 0x0e402400U
#define AARCH64_SIMD_FMAX_FP16_PATTERN 0x0e403400U
#define AARCH64_SIMD_FRECPS_FP16_PATTERN 0x0e403c00U
#define AARCH64_SIMD_FMINNM_FP16_PATTERN 0x0ec00400U
#define AARCH64_SIMD_FMLS_FP16_PATTERN 0x0ec00c00U
#define AARCH64_SIMD_FMIN_FP16_PATTERN 0x0ec03400U
#define AARCH64_SIMD_FRSQRTS_FP16_PATTERN 0x0ec03c00U
#define AARCH64_SIMD_FMAXNMP_FP16_PATTERN 0x2e400400U
#define AARCH64_SIMD_FADDP_FP16_PATTERN 0x2e401400U
#define AARCH64_SIMD_FMUL_FP16_PATTERN 0x2e401c00U
#define AARCH64_SIMD_FCMGE_FP16_PATTERN 0x2e402400U
#define AARCH64_SIMD_FACGE_FP16_PATTERN 0x2e402c00U
#define AARCH64_SIMD_FMAXP_FP16_PATTERN 0x2e403400U
#define AARCH64_SIMD_FDIV_FP16_PATTERN 0x2e403c00U
#define AARCH64_SIMD_FMINNMP_FP16_PATTERN 0x2ec00400U
#define AARCH64_SIMD_FABD_FP16_PATTERN 0x2ec01400U
#define AARCH64_SIMD_FCMGT_FP16_PATTERN 0x2ec02400U
#define AARCH64_SIMD_FACGT_FP16_PATTERN 0x2ec02c00U
#define AARCH64_SIMD_FMINP_FP16_PATTERN 0x2ec03400U
#define AARCH64_SIMD_FP16_SCALAR_THREE_SAME_MASK 0xffe0fc00U
#define AARCH64_SIMD_FMULX_FP16_SCALAR_PATTERN 0x5e401c00U
#define AARCH64_SIMD_FCMEQ_FP16_SCALAR_PATTERN 0x5e402400U
#define AARCH64_SIMD_FRECPS_FP16_SCALAR_PATTERN 0x5e403c00U
#define AARCH64_SIMD_FRSQRTS_FP16_SCALAR_PATTERN 0x5ec03c00U
#define AARCH64_SIMD_FCMGE_FP16_SCALAR_PATTERN 0x7e402400U
#define AARCH64_SIMD_FACGE_FP16_SCALAR_PATTERN 0x7e402c00U
#define AARCH64_SIMD_FABD_FP16_SCALAR_PATTERN 0x7ec01400U
#define AARCH64_SIMD_FCMGT_FP16_SCALAR_PATTERN 0x7ec02400U
#define AARCH64_SIMD_FACGT_FP16_SCALAR_PATTERN 0x7ec02c00U
#define AARCH64_SIMD_SCALAR_FP_ESTIMATE_MASK 0xffbffc00U
#define AARCH64_SIMD_FRECPE_SCALAR_PATTERN 0x5ea1d800U
#define AARCH64_SIMD_FRECPX_SCALAR_PATTERN 0x5ea1f800U
#define AARCH64_SIMD_FRSQRTE_SCALAR_PATTERN 0x7ea1d800U
#define AARCH64_SIMD_FCVTXN_SCALAR_MASK 0xfffffc00U
#define AARCH64_SIMD_FCVTXN_SCALAR_PATTERN 0x7e616800U
#define AARCH64_SIMD_FCVTXN_VECTOR_MASK 0xbffffc00U
#define AARCH64_SIMD_FCVTXN_VECTOR_PATTERN 0x2e616800U
#define AARCH64_SIMD_ADD_SUB_MASK 0x9f20fc00U
#define AARCH64_SIMD_ADD_SUB_PATTERN 0x0e208400U
#define AARCH64_SIMD_SCALAR_ADD_SUB_MASK 0xdf20fc00U
#define AARCH64_SIMD_SCALAR_ADD_SUB_PATTERN 0x5e208400U
#define AARCH64_SIMD_HALVING_ADD_MASK 0x9f20ec00U
#define AARCH64_SIMD_HALVING_ADD_PATTERN 0x0e200400U
#define AARCH64_SIMD_HALVING_SUB_MASK 0x9f20fc00U
#define AARCH64_SIMD_HALVING_SUB_PATTERN 0x0e202400U
#define AARCH64_SIMD_PAIRWISE_ADD_MASK 0xbf20fc00U
#define AARCH64_SIMD_PAIRWISE_ADD_PATTERN 0x0e20bc00U
#define AARCH64_SIMD_PAIRWISE_LONG_MASK 0x9f3ffc00U
#define AARCH64_SIMD_PAIRWISE_LONG_PATTERN 0x0e202800U
#define AARCH64_SIMD_PAIRWISE_LONG_ACCUMULATE_PATTERN 0x0e206800U
#define AARCH64_SIMD_ADD_SUB_WIDE_MASK 0x9f20dc00U
#define AARCH64_SIMD_ADD_SUB_WIDE_PATTERN 0x0e201000U
#define AARCH64_SIMD_ADD_SUB_LONG_MASK 0x9f20dc00U
#define AARCH64_SIMD_ADD_SUB_LONG_PATTERN 0x0e200000U
#define AARCH64_SIMD_PAIRWISE_MIN_MAX_MASK 0x9f20f400U
#define AARCH64_SIMD_PAIRWISE_MIN_MAX_PATTERN 0x0e20a400U
#define AARCH64_SIMD_SATURATING_ADD_SUB_MASK 0x9f20dc00U
#define AARCH64_SIMD_SATURATING_ADD_SUB_PATTERN 0x0e200c00U
#define AARCH64_SIMD_SCALAR_SATURATING_ADD_SUB_MASK 0xdf20dc00U
#define AARCH64_SIMD_SCALAR_SATURATING_ADD_SUB_PATTERN 0x5e200c00U
#define AARCH64_SIMD_SATURATING_MUL_HIGH_MASK 0x9f20fc00U
#define AARCH64_SIMD_SATURATING_MUL_HIGH_PATTERN 0x0e20b400U
#define AARCH64_SIMD_SCALAR_SATURATING_MUL_HIGH_MASK 0xdf20fc00U
#define AARCH64_SIMD_SCALAR_SATURATING_MUL_HIGH_PATTERN 0x5e20b400U
#define AARCH64_SIMD_MUL_PMUL_MASK 0x9f20fc00U
#define AARCH64_SIMD_MUL_PMUL_PATTERN 0x0e209c00U
#define AARCH64_SIMD_PMULL_MASK 0xbf20fc00U
#define AARCH64_SIMD_PMULL_PATTERN 0x0e20e000U
#define AARCH64_SIMD_MULTIPLY_LONG_MASK 0x9f20fc00U
#define AARCH64_SIMD_MLAL_PATTERN 0x0e208000U
#define AARCH64_SIMD_MLSL_PATTERN 0x0e20a000U
#define AARCH64_SIMD_MULL_PATTERN 0x0e20c000U
#define AARCH64_SIMD_SCALAR_SQDM_LONG_MASK 0xff20fc00U
#define AARCH64_SIMD_SCALAR_SQDMLAL_PATTERN 0x5e209000U
#define AARCH64_SIMD_SCALAR_SQDMLSL_PATTERN 0x5e20b000U
#define AARCH64_SIMD_SCALAR_SQDMULL_PATTERN 0x5e20d000U
#define AARCH64_SIMD_VECTOR_SQDM_LONG_MASK 0xbf20fc00U
#define AARCH64_SIMD_VECTOR_SQDMLAL_PATTERN 0x0e209000U
#define AARCH64_SIMD_VECTOR_SQDMLSL_PATTERN 0x0e20b000U
#define AARCH64_SIMD_VECTOR_SQDMULL_PATTERN 0x0e20d000U
#define AARCH64_SIMD_AES_MASK 0xffff8c00U
#define AARCH64_SIMD_AES_PATTERN 0x4e280800U
#define AARCH64_SIMD_SHA1_THREE_REGISTER_MASK 0xffe04c00U
#define AARCH64_SIMD_SHA1_THREE_REGISTER_PATTERN 0x5e000000U
#define AARCH64_SIMD_SHA1_TWO_REGISTER_MASK 0xfffffc00U
#define AARCH64_SIMD_SHA1H_PATTERN 0x5e280800U
#define AARCH64_SIMD_SHA1SU1_PATTERN 0x5e281800U
#define AARCH64_SIMD_SHA256_THREE_REGISTER_MASK 0xffe04c00U
#define AARCH64_SIMD_SHA256_THREE_REGISTER_PATTERN 0x5e004000U
#define AARCH64_SIMD_SHA256SU0_MASK 0xfffffc00U
#define AARCH64_SIMD_SHA256SU0_PATTERN 0x5e282800U
#define AARCH64_SIMD_SHA512_THREE_REGISTER_MASK 0xffe0fc00U
#define AARCH64_SIMD_SHA512H_PATTERN 0xce608000U
#define AARCH64_SIMD_SHA512H2_PATTERN 0xce608400U
#define AARCH64_SIMD_SHA512SU1_PATTERN 0xce608800U
#define AARCH64_SIMD_SHA512SU0_MASK 0xfffffc00U
#define AARCH64_SIMD_SHA512SU0_PATTERN 0xcec08000U
#define AARCH64_SIMD_EOR3_MASK 0xffe08000U
#define AARCH64_SIMD_EOR3_PATTERN 0xce000000U
#define AARCH64_SIMD_RAX1_MASK 0xffe0fc00U
#define AARCH64_SIMD_RAX1_PATTERN 0xce608c00U
#define AARCH64_SIMD_XAR_MASK 0xffe00000U
#define AARCH64_SIMD_XAR_PATTERN 0xce800000U
#define AARCH64_SIMD_BCAX_MASK 0xffe08000U
#define AARCH64_SIMD_BCAX_PATTERN 0xce200000U
#define AARCH64_SIMD_SM4E_MASK 0xfffffc00U
#define AARCH64_SIMD_SM4E_PATTERN 0xcec08400U
#define AARCH64_SIMD_SM4EKEY_MASK 0xffe0fc00U
#define AARCH64_SIMD_SM4EKEY_PATTERN 0xce60c800U
#define AARCH64_SIMD_SM3SS1_MASK 0xffe08000U
#define AARCH64_SIMD_SM3SS1_PATTERN 0xce400000U
#define AARCH64_SIMD_SM3TT_MASK 0xffe0cc00U
#define AARCH64_SIMD_SM3TT1A_PATTERN 0xce408000U
#define AARCH64_SIMD_SM3TT1B_PATTERN 0xce408400U
#define AARCH64_SIMD_SM3TT2A_PATTERN 0xce408800U
#define AARCH64_SIMD_SM3TT2B_PATTERN 0xce408c00U
#define AARCH64_SIMD_SM3PARTW_MASK 0xffe0fc00U
#define AARCH64_SIMD_SM3PARTW1_PATTERN 0xce60c000U
#define AARCH64_SIMD_SM3PARTW2_PATTERN 0xce60c400U
#define AARCH64_SIMD_MLA_MLS_MASK 0x9f20fc00U
#define AARCH64_SIMD_MLA_MLS_PATTERN 0x0e209400U
#define AARCH64_SIMD_MIN_MAX_MASK 0x9f20f400U
#define AARCH64_SIMD_MIN_MAX_PATTERN 0x0e206400U
#define AARCH64_SIMD_ABSOLUTE_DIFFERENCE_MASK 0x9f20f400U
#define AARCH64_SIMD_ABSOLUTE_DIFFERENCE_PATTERN 0x0e207400U
#define AARCH64_SIMD_ABSOLUTE_DIFFERENCE_LONG_MASK 0x9f20fc00U
#define AARCH64_SIMD_ABSOLUTE_DIFFERENCE_LONG_PATTERN 0x0e207000U
#define AARCH64_SIMD_ABSOLUTE_DIFFERENCE_ACCUMULATE_LONG_PATTERN 0x0e205000U
#define AARCH64_SIMD_ADD_SUB_NARROW_HIGH_MASK 0x9f20dc00U
#define AARCH64_SIMD_ADD_SUB_NARROW_HIGH_PATTERN 0x0e204000U
#define AARCH64_SIMD_SATURATING_NARROW_MASK 0x9f3ffc00U
#define AARCH64_SIMD_SQXTN_UQXTN_PATTERN 0x0e214800U
#define AARCH64_SIMD_SQXTUN_PATTERN 0x0e212800U
#define AARCH64_SIMD_SCALAR_SATURATING_NARROW_MASK 0xdf3ffc00U
#define AARCH64_SIMD_SCALAR_SQXTN_UQXTN_PATTERN 0x5e214800U
#define AARCH64_SIMD_SCALAR_SQXTUN_PATTERN 0x5e212800U
#define AARCH64_SIMD_SHIFT_NARROW_MASK 0x9f80fc00U
#define AARCH64_SIMD_SHRN_PATTERN 0x0f008400U
#define AARCH64_SIMD_RSHRN_PATTERN 0x0f008c00U
#define AARCH64_SIMD_SQSHRN_PATTERN 0x0f009400U
#define AARCH64_SIMD_SQRSHRN_PATTERN 0x0f009c00U
#define AARCH64_SIMD_SCALAR_SHIFT_NARROW_MASK 0xdf80fc00U
#define AARCH64_SIMD_SCALAR_SHRN_PATTERN 0x5f008400U
#define AARCH64_SIMD_SCALAR_RSHRN_PATTERN 0x5f008c00U
#define AARCH64_SIMD_SCALAR_SQSHRN_PATTERN 0x5f009400U
#define AARCH64_SIMD_SCALAR_SQRSHRN_PATTERN 0x5f009c00U
#define AARCH64_SIMD_SHIFT_RIGHT_MASK 0x8f80fc00U
#define AARCH64_SIMD_SSHR_PATTERN 0x0f000400U
#define AARCH64_SIMD_SSRA_PATTERN 0x0f001400U
#define AARCH64_SIMD_SRSHR_PATTERN 0x0f002400U
#define AARCH64_SIMD_SRSRA_PATTERN 0x0f003400U
#define AARCH64_SIMD_SHIFT_LEFT_INSERT_MASK 0x8f80fc00U
#define AARCH64_SIMD_SHL_SLI_PATTERN 0x0f005400U
#define AARCH64_SIMD_SRI_PATTERN 0x0f004400U
#define AARCH64_SIMD_SATURATING_SHIFT_LEFT_MASK 0x8f80fc00U
#define AARCH64_SIMD_SQSHL_UQSHL_IMMEDIATE_PATTERN 0x0f007400U
#define AARCH64_SIMD_SQSHLU_IMMEDIATE_PATTERN 0x0f006400U
#define AARCH64_SIMD_TWO_REGISTER_MISC_MASK 0x9f3ffc00U
#define AARCH64_SIMD_SCALAR_TWO_REGISTER_MISC_MASK 0x8f3ffc00U
#define AARCH64_SIMD_ABS_NEG_PATTERN 0x0e20b800U
#define AARCH64_SIMD_SQABS_SQNEG_PATTERN 0x0e207800U
#define AARCH64_SIMD_SUQADD_USQADD_PATTERN 0x0e203800U
#define AARCH64_SIMD_CLS_CLZ_PATTERN 0x0e204800U
#define AARCH64_SIMD_REV_PATTERN 0x0e200800U
#define AARCH64_SIMD_REV16_PATTERN 0x0e201800U
#define AARCH64_SIMD_BIT_COUNT_PATTERN 0x0e205800U
#define AARCH64_SIMD_CMGT_ZERO_PATTERN 0x0e208800U
#define AARCH64_SIMD_CMEQ_ZERO_PATTERN 0x0e209800U
#define AARCH64_SIMD_CMLT_ZERO_PATTERN 0x0e20a800U
#define AARCH64_SIMD_FNEG_2D_MASK 0xfffffc00U
#define AARCH64_SIMD_FNEG_2D_PATTERN 0x6ee0f800U
#define AARCH64_SIMD_FP_TWO_REGISTER_MASK 0xbfbbfc00U
#define AARCH64_SIMD_FABS_VECTOR_PATTERN 0x0ea0f800U
#define AARCH64_SIMD_FNEG_VECTOR_PATTERN 0x2ea0f800U
#define AARCH64_SIMD_FSQRT_VECTOR_PATTERN 0x2ea1f800U
#define AARCH64_SIMD_FRECPE_VECTOR_PATTERN 0x0ea1d800U
#define AARCH64_SIMD_FRSQRTE_VECTOR_PATTERN 0x2ea1d800U
#define AARCH64_SIMD_FRINTN_VECTOR_PATTERN 0x0e218800U
#define AARCH64_SIMD_FRINTP_VECTOR_PATTERN 0x0ea18800U
#define AARCH64_SIMD_FRINTM_VECTOR_PATTERN 0x0e219800U
#define AARCH64_SIMD_FRINTZ_VECTOR_PATTERN 0x0ea19800U
#define AARCH64_SIMD_FRINTA_VECTOR_PATTERN 0x2e218800U
#define AARCH64_SIMD_FRINTX_VECTOR_PATTERN 0x2e219800U
#define AARCH64_SIMD_FRINTI_VECTOR_PATTERN 0x2ea19800U
#define AARCH64_SIMD_FCMGT_ZERO_VECTOR_PATTERN 0x0ea0c800U
#define AARCH64_SIMD_FCMEQ_ZERO_VECTOR_PATTERN 0x0ea0d800U
#define AARCH64_SIMD_FCMLT_ZERO_VECTOR_PATTERN 0x0ea0e800U
#define AARCH64_SIMD_FCMGE_ZERO_VECTOR_PATTERN 0x2ea0c800U
#define AARCH64_SIMD_FCMLE_ZERO_VECTOR_PATTERN 0x2ea0d800U
#define AARCH64_SIMD_FCVT_LONG_NARROW_MASK 0xbf3ffc00U
#define AARCH64_SIMD_FCVTN_VECTOR_PATTERN 0x0e216800U
#define AARCH64_SIMD_FCVTL_VECTOR_PATTERN 0x0e217800U
#define AARCH64_SIMD_SHLL_MASK 0xbf3ffc00U
#define AARCH64_SIMD_SHLL_PATTERN 0x2e213800U
#define AARCH64_SIMD_UINT_ESTIMATE_MASK 0xbffffc00U
#define AARCH64_SIMD_URECPE_VECTOR_PATTERN 0x0ea1c800U
#define AARCH64_SIMD_URSQRTE_VECTOR_PATTERN 0x2ea1c800U
#define AARCH64_SIMD_SHIFT_BY_REGISTER_MASK 0x8f20e400U
#define AARCH64_SIMD_SHIFT_BY_REGISTER_PATTERN 0x0e204400U
#define AARCH64_SIMD_CMEQ_MASK 0xbf20fc00U
#define AARCH64_SIMD_CMEQ_PATTERN 0x2e208c00U
#define AARCH64_SIMD_THREE_SAME_MASK 0x9f200400U
#define AARCH64_SIMD_THREE_SAME_PATTERN 0x0e200400U
#define AARCH64_SIMD_SCALAR_THREE_SAME_PATTERN 0x1e200400U
#define AARCH64_SIMD_CMEQ_4S_MASK 0xffe0fc00U
#define AARCH64_SIMD_CMEQ_4S_PATTERN 0x6ea08c00U
#define AARCH64_SIMD_CMHI_2D_MASK 0xff20fc00U
#define AARCH64_SIMD_CMHI_2D_PATTERN 0x6e203400U
#define AARCH64_SIMD_MIN_MAXV_MASK 0x9f3efc00U
#define AARCH64_SIMD_MIN_MAXV_PATTERN 0x0e30a800U
#define AARCH64_SIMD_ADDV_MASK 0xbf3ffc00U
#define AARCH64_SIMD_ADDV_PATTERN 0x0e31b800U
#define AARCH64_SIMD_ADD_LONGV_MASK 0x9f3ffc00U
#define AARCH64_SIMD_ADD_LONGV_PATTERN 0x0e303800U
#define AARCH64_SIMD_INDEXED_MASK 0x8f000400U
#define AARCH64_SIMD_INDEXED_PATTERN 0x0f000000U
#define AARCH64_SIMD_FP_ACROSS_LANES_MASK 0xfffffc00U
#define AARCH64_SIMD_FMAXNMV_PATTERN 0x6e30c800U
#define AARCH64_SIMD_FMAXV_PATTERN 0x6e30f800U
#define AARCH64_SIMD_FMINNMV_PATTERN 0x6eb0c800U
#define AARCH64_SIMD_FMINV_PATTERN 0x6eb0f800U
#define AARCH64_SIMD_ADDP_D_2D_MASK 0xfffffc20U
#define AARCH64_SIMD_ADDP_D_2D_PATTERN 0x5ef1b800U
#define AARCH64_SIMD_FP_PAIRWISE_MASK 0xffbffc00U
#define AARCH64_SIMD_FADDP_SCALAR_PATTERN 0x7e30d800U
#define AARCH64_SIMD_FMAXNMP_SCALAR_PATTERN 0x7e30c800U
#define AARCH64_SIMD_FMAXP_SCALAR_PATTERN 0x7e30f800U
#define AARCH64_SIMD_FMINNMP_SCALAR_PATTERN 0x7eb0c800U
#define AARCH64_SIMD_FMINP_SCALAR_PATTERN 0x7eb0f800U
#define AARCH64_SIMD_SINGLE_STRUCTURE_MASK 0xbf000000U
#define AARCH64_SIMD_SINGLE_STRUCTURE_PATTERN 0x0d000000U
#define AARCH64_SIMD_MULTIPLE_STRUCTURE_MASK 0xbf200000U
#define AARCH64_SIMD_MULTIPLE_STRUCTURE_PATTERN 0x0c000000U
#define AARCH64_FMOV_W_S_MASK 0xfffffc00U
#define AARCH64_FMOV_W_S_PATTERN 0x1e260000U
#define AARCH64_FMOV_S_W_PATTERN 0x1e270000U
#define AARCH64_FMOV_X_D_PATTERN 0x9e660000U
#define AARCH64_FMOV_D_X_PATTERN 0x9e670000U
#define AARCH64_FMOV_X_V_HIGH_PATTERN 0x9eae0000U
#define AARCH64_FMOV_V_HIGH_X_PATTERN 0x9eaf0000U
#define AARCH64_FMOV_IMMEDIATE_MASK 0xffe01fe0U
#define AARCH64_FMOV_S_IMMEDIATE_PATTERN 0x1e201000U
#define AARCH64_FMOV_D_IMMEDIATE_PATTERN 0x1e601000U
#define AARCH64_FMOV_S_S_MASK 0xfffffc00U
#define AARCH64_FMOV_S_S_PATTERN 0x1e204000U
#define AARCH64_FMOV_D_D_MASK 0xfffffc00U
#define AARCH64_FMOV_D_D_PATTERN 0x1e604000U
#define AARCH64_FCVT_D_S_MASK 0xfffffc00U
#define AARCH64_FCVT_D_S_PATTERN 0x1e22c000U
#define AARCH64_FCVT_S_D_MASK 0xfffffc00U
#define AARCH64_FCVT_S_D_PATTERN 0x1e624000U
#define AARCH64_FCVT_D_H_PATTERN 0x1ee2c000U
#define AARCH64_FCVT_H_D_PATTERN 0x1e63c000U
#define AARCH64_FCVT_H_S_PATTERN 0x1e23c000U
#define AARCH64_FCVT_S_H_PATTERN 0x1ee24000U
#define AARCH64_FABS_S_MASK 0xfffffc00U
#define AARCH64_FABS_S_PATTERN 0x1e20c000U
#define AARCH64_FABS_D_MASK 0xfffffc00U
#define AARCH64_FABS_D_PATTERN 0x1e60c000U
#define AARCH64_FABS_H_PATTERN 0x1ee0c000U
#define AARCH64_FNEG_S_MASK 0xfffffc00U
#define AARCH64_FNEG_S_PATTERN 0x1e214000U
#define AARCH64_FNEG_D_MASK 0xfffffc00U
#define AARCH64_FNEG_D_PATTERN 0x1e614000U
#define AARCH64_FNEG_H_PATTERN 0x1ee14000U
#define AARCH64_FSQRT_S_MASK 0xfffffc00U
#define AARCH64_FSQRT_S_PATTERN 0x1e21c000U
#define AARCH64_FSQRT_D_MASK 0xfffffc00U
#define AARCH64_FSQRT_D_PATTERN 0x1e61c000U
#define AARCH64_FSQRT_H_PATTERN 0x1ee1c000U
#define AARCH64_FP_SCALAR_1SOURCE_MASK 0xff207c00U
#define AARCH64_FP_SCALAR_1SOURCE_PATTERN 0x1e204000U
#define AARCH64_FP_SCALAR_2SOURCE_MASK 0xff200c00U
#define AARCH64_FP_SCALAR_2SOURCE_PATTERN 0x1e200800U
#define AARCH64_FMUL_2D_MASK 0xff20fc00U
#define AARCH64_FMUL_2D_PATTERN 0x6e20dc00U
#define AARCH64_FP_SCALAR_3SOURCE_MASK 0xff000000U
#define AARCH64_FP_SCALAR_3SOURCE_PATTERN 0x1f000000U
#define AARCH64_FCSEL_MASK 0xffa00c00U
#define AARCH64_FCSEL_PATTERN 0x1e200c00U
#define AARCH64_FCCMP_MASK 0xff200c00U
#define AARCH64_FCCMP_PATTERN 0x1e200400U
#define AARCH64_FCMP_S_MASK 0xffe0fc0fU
#define AARCH64_FCMP_S_PATTERN 0x1e202000U
#define AARCH64_FCMP_D_MASK 0xffe0fc0fU
#define AARCH64_FCMP_D_PATTERN 0x1e602000U
#define AARCH64_FCMP_H_PATTERN 0x1ee02000U
#define AARCH64_FCMP_S_ZERO_PATTERN 0x1e202008U
#define AARCH64_FCMP_D_ZERO_PATTERN 0x1e602008U
#define AARCH64_FCMP_H_ZERO_PATTERN 0x1ee02008U
#define AARCH64_FP_INT_GPR_MASK 0x7fa00c00U
#define AARCH64_FP_INT_GPR_PATTERN 0x1e200000U
#define AARCH64_SCVTF_S_W_MASK 0xfffffc00U
#define AARCH64_SCVTF_S_W_PATTERN 0x1e220000U
#define AARCH64_SCVTF_D_W_MASK 0xfffffc00U
#define AARCH64_SCVTF_D_W_PATTERN 0x1e620000U
#define AARCH64_SCVTF_S_X_MASK 0xfffffc00U
#define AARCH64_SCVTF_S_X_PATTERN 0x9e220000U
#define AARCH64_SCVTF_D_X_MASK 0xfffffc00U
#define AARCH64_SCVTF_D_X_PATTERN 0x9e620000U
#define AARCH64_UCVTF_S_GPR_MASK 0x7ffffc00U
#define AARCH64_UCVTF_S_GPR_PATTERN 0x1e230000U
#define AARCH64_UCVTF_D_GPR_MASK 0x7ffffc00U
#define AARCH64_UCVTF_D_GPR_PATTERN 0x1e630000U
#define AARCH64_FCVTZS_D_GPR_MASK 0x7ffffc00U
#define AARCH64_FCVTZS_D_GPR_PATTERN 0x1e780000U
#define AARCH64_FCVTZS_GPR_MASK 0x7fbffc00U
#define AARCH64_FCVTZS_GPR_PATTERN 0x1e380000U
#define AARCH64_FCVTZU_GPR_MASK 0x7fbffc00U
#define AARCH64_FCVTZU_GPR_PATTERN 0x1e390000U
#define AARCH64_FCVTZU_GPR_S_MASK 0x7ffffc00U
#define AARCH64_FCVTZU_GPR_S_PATTERN 0x1e390000U
#define AARCH64_FCVTZU_X_D_MASK 0xfffffc00U
#define AARCH64_FCVTZU_X_D_PATTERN 0x9e790000U
#define AARCH64_FCVTZU_W_D_MASK 0xfffffc00U
#define AARCH64_FCVTZU_W_D_PATTERN 0x1e790000U
#define AARCH64_FCVTZ_FIXED_GPR_MASK 0x7f3e0000U
#define AARCH64_FCVTZ_FIXED_GPR_PATTERN 0x1e180000U
#define AARCH64_CVTF_FIXED_GPR_PATTERN 0x1e020000U
#define AARCH64_FP_FIXED_SIMD_SCALAR_MASK 0xdf80fc00U
#define AARCH64_FP_FIXED_SIMD_VECTOR_MASK 0x9f80fc00U
#define AARCH64_FP_INT_SIMD_SCALAR_MASK 0xdfbffc00U
#define AARCH64_FP_INT_SIMD_VECTOR_MASK 0x9fbffc00U
#define AARCH64_FCVTZS_SIMD_SCALAR_MASK 0xffbffc00U
#define AARCH64_FCVTZS_SIMD_SCALAR_PATTERN 0x5ea1b800U
#define AARCH64_FCVTZU_SIMD_SCALAR_MASK 0xffbffc00U
#define AARCH64_FCVTZU_SIMD_SCALAR_PATTERN 0x7ea1b800U
#define AARCH64_UCVTF_D_D_MASK 0xfffffc00U
#define AARCH64_UCVTF_D_D_PATTERN 0x7e61d800U
#define AARCH64_SCVTF_D_D_MASK 0xfffffc00U
#define AARCH64_SCVTF_D_D_PATTERN 0x5e61d800U
#define AARCH64_UCVTF_2D_2D_MASK 0xfffffc00U
#define AARCH64_UCVTF_2D_2D_PATTERN 0x6e61d800U
#define AARCH64_SYSTEM_REGISTER_MASK 0xfff00000U
#define AARCH64_MRS_PATTERN 0xd5300000U
#define AARCH64_MSR_PATTERN 0xd5100000U
#define AARCH64_SYSTEM_INSTRUCTION_MASK 0xfff80000U
#define AARCH64_SYS_PATTERN 0xd5080000U
#define AARCH64_SYSL_PATTERN 0xd5280000U
#define AARCH64_MSRR_PATTERN 0xd5500000U
#define AARCH64_MRRS_PATTERN 0xd5700000U
static u32 orlix_tcti_bits(u32 value, u8 shift, u8 width)
{
	return (value >> shift) & ((1U << width) - 1U);
}

static u8 orlix_tcti_simd_modified_imm8(u32 instruction)
{
	return (((instruction >> 16) & 0x7U) << 5) |
	       ((instruction >> 5) & 0x1fU);
}

static u64 orlix_tcti_replicate_u32(u32 value)
{
	return (u64)value | ((u64)value << 32);
}

static u64 orlix_tcti_replicate_u16(u16 value)
{
	u64 pattern = value;

	pattern |= pattern << 16;
	return pattern | (pattern << 32);
}

static u64 orlix_tcti_replicate_u8(u8 value)
{
	u64 pattern = value;

	pattern |= pattern << 8;
	pattern |= pattern << 16;
	return pattern | (pattern << 32);
}

static u64 orlix_tcti_expand_simd_modified_bitmask(u8 imm8)
{
	u64 pattern = 0;
	u8 byte;

	for (byte = 0; byte < 8; byte++) {
		if (imm8 & BIT(byte))
			pattern |= 0xffULL << (byte * 8);
	}

	return pattern;
}

static u32 orlix_tcti_expand_simd_fp32_immediate(u8 imm8)
{
	u32 sign = (imm8 >> 7) & 1U;
	u32 exponent_bit = (imm8 >> 6) & 1U;
	u32 fraction = imm8 & 0x3fU;

	return (sign << 31) | ((!exponent_bit) << 30) |
	       ((exponent_bit ? 0x1fU : 0) << 25) | (fraction << 19);
}

static u64 orlix_tcti_expand_simd_fp64_immediate(u8 imm8)
{
	u64 sign = (imm8 >> 7) & 1U;
	u64 exponent_bit = (imm8 >> 6) & 1U;
	u64 fraction = imm8 & 0x3fU;

	return (sign << 63) | ((u64)!exponent_bit << 62) |
	       ((exponent_bit ? 0xffULL : 0) << 54) | (fraction << 48);
}

static bool orlix_tcti_decode_simd_modified_immediate(
	u32 instruction, struct orlix_tcti_decoded_instruction *decoded)
{
	u8 cmode = (instruction >> 12) & 0xfU;
	u8 imm8 = orlix_tcti_simd_modified_imm8(instruction);
	bool q = instruction & BIT(30);
	bool op = instruction & BIT(29);
	u64 pattern;

	decoded->decode_class = ORLIX_TCTI_DECODE_SIMD_MODIFIED_IMMEDIATE;
	decoded->rd = instruction & 0x1fU;
	decoded->result_size = q ? 2 * sizeof(u64) : sizeof(u64);
	decoded->access_size = decoded->result_size;
	decoded->simd_fp = true;
	decoded->simd_modified_immediate_op = ORLIX_TCTI_SIMD_MODIMM_MOVI;

	if (cmode <= 7) {
		u8 base_cmode = cmode & ~1U;
		u32 lane = (u32)imm8 << ((base_cmode >> 1) * 8);

		pattern = orlix_tcti_replicate_u32(lane);
		decoded->simd_modified_immediate_op = (cmode & 1U) ?
			(op ? ORLIX_TCTI_SIMD_MODIMM_BIC : ORLIX_TCTI_SIMD_MODIMM_ORR) :
			(op ? ORLIX_TCTI_SIMD_MODIMM_MVNI : ORLIX_TCTI_SIMD_MODIMM_MOVI);
	} else if (cmode <= 11) {
		u8 base_cmode = cmode & ~1U;
		u16 lane = (u16)imm8 << (((base_cmode - 8) >> 1) * 8);

		pattern = orlix_tcti_replicate_u16(lane);
		decoded->simd_modified_immediate_op = (cmode & 1U) ?
			(op ? ORLIX_TCTI_SIMD_MODIMM_BIC : ORLIX_TCTI_SIMD_MODIMM_ORR) :
			(op ? ORLIX_TCTI_SIMD_MODIMM_MVNI : ORLIX_TCTI_SIMD_MODIMM_MOVI);
	} else if (cmode == 12 || cmode == 13) {
		u8 shift = cmode == 12 ? 8 : 16;
		u32 lane = ((u32)imm8 << shift) | (BIT(shift) - 1U);

		pattern = orlix_tcti_replicate_u32(lane);
		decoded->simd_modified_immediate_op = op ?
			ORLIX_TCTI_SIMD_MODIMM_MVNI : ORLIX_TCTI_SIMD_MODIMM_MOVI;
	} else if (cmode == 14 && !op) {
		pattern = orlix_tcti_replicate_u8(imm8);
	} else if (cmode == 14) {
		pattern = orlix_tcti_expand_simd_modified_bitmask(imm8);
	} else if (!op) {
		u32 lane = orlix_tcti_expand_simd_fp32_immediate(imm8);

		pattern = orlix_tcti_replicate_u32(lane);
	} else {
		if (!q)
			return false;
		pattern = orlix_tcti_expand_simd_fp64_immediate(imm8);
	}

	decoded->logical_immediate = pattern;
	return true;
}

static bool orlix_tcti_decode_load_store_variant(u8 size, u8 opc, bool *load,
					   bool *sign_extend_load,
					   u8 *access_size, u8 *result_size)
{
	if (size > 3)
		return false;

	*access_size = 1U << size;
	*result_size = *access_size;
	*load = opc != 0;
	*sign_extend_load = false;

	switch (opc) {
	case 0:
	case 1:
		return true;
	case 2:
		if (size == 3)
			return false;
		*sign_extend_load = true;
		*result_size = sizeof(u64);
		return true;
	case 3:
		if (size >= 2)
			return false;
		*sign_extend_load = true;
		*result_size = sizeof(u32);
		return true;
	default:
		return false;
	}
}

static bool orlix_tcti_decode_simd_fp_load_store_variant(u8 size, u8 opc,
						   bool *load,
						   u8 *access_size,
						   u8 *result_size)
{
	if (size == 0 && opc <= 1) {
		*load = opc == 1;
		*access_size = sizeof(u8);
		*result_size = sizeof(u8);
		return true;
	}
	if (size == 1 && opc <= 1) {
		*load = opc == 1;
		*access_size = sizeof(u16);
		*result_size = sizeof(u16);
		return true;
	}
	if (size == 2 && opc <= 1) {
		*load = opc == 1;
		*access_size = sizeof(u32);
		*result_size = sizeof(u32);
		return true;
	}

	if (size == 3 && opc <= 1) {
		*load = opc == 1;
		*access_size = sizeof(u64);
		*result_size = sizeof(u64);
		return true;
	}

	if (size == 0 && opc >= 2) {
		*load = opc == 3;
		*access_size = 2 * sizeof(u64);
		*result_size = 2 * sizeof(u64);
		return true;
	}

	return false;
}

static u64 orlix_tcti_ror_width(u64 value, u8 rotate, u8 width)
{
	u64 mask = width == 64 ? ~0ULL : (BIT_ULL(width) - 1);

	rotate %= width;
	value &= mask;
	if (!rotate)
		return value;

	return ((value >> rotate) | (value << (width - rotate))) & mask;
}

static u64 orlix_tcti_expand_fp_immediate(u8 imm8, u8 exponent_bits,
				    u8 fraction_bits)
{
	bool bit6 = imm8 & BIT(6);
	u64 exponent;
	u64 fraction;
	u64 sign;

	sign = imm8 & BIT(7) ? BIT_ULL(exponent_bits + fraction_bits) : 0;
	exponent = bit6 ? ((BIT_ULL(exponent_bits - 3) - 1) << 2) :
			  BIT_ULL(exponent_bits - 1);
	exponent |= (imm8 >> 4) & 0x3U;
	fraction = (u64)(imm8 & 0xfU) << (fraction_bits - 4);

	return sign | (exponent << fraction_bits) | fraction;
}

static bool orlix_tcti_decode_logical_immediate_mask(bool is_64bit, bool n, u8 immr,
					       u8 imms, u64 *out)
{
	u8 reg_width = is_64bit ? 64 : 32;
	u8 len_source = (n ? BIT(6) : 0) | ((~imms) & 0x3fU);
	u8 length;
	u8 levels;
	u8 size;
	u64 element;
	u64 result = 0;
	u8 offset;

	if (!len_source)
		return false;

	length = fls(len_source) - 1;
	if (!is_64bit && length == 6)
		return false;

	levels = BIT(length) - 1;
	if ((imms & levels) == levels)
		return false;

	size = BIT(length);
	element = BIT_ULL((imms & levels) + 1) - 1;
	element = orlix_tcti_ror_width(element, immr & levels, size);

	for (offset = 0; offset < reg_width; offset += size)
		result |= element << offset;

	*out = result;
	return true;
}

static void orlix_tcti_decode_memory_common(struct orlix_tcti_decoded_instruction *decoded,
				      u32 instruction, u8 size, u8 opc)
{
	decoded->rt = instruction & 0x1fU;
	decoded->rn = (instruction >> 5) & 0x1fU;
	orlix_tcti_decode_load_store_variant(size, opc, &decoded->load,
				       &decoded->sign_extend_load,
				       &decoded->access_size,
				       &decoded->result_size);
}

/*
 * AARCHMRS 2026-06 leaves 2245-2249 and 2256-2265.  These encodings overlap the
 * generic HINT class, but their PAuth and BTI ASL operations are absent from
 * the pinned source bundle.  Do not turn an unimplemented extension into a
 * silently successful HINT.  They remain undefined to the OrlixTCTI guest until
 * their individual source leaves have production semantics and proof.
 */
static bool orlix_tcti_is_unimplemented_pauth_or_bti_hint(u32 instruction)
{
	static const u32 pauth_hints[] = {
		0xd50320ffU, /* XPACLRI */
		0xd503211fU, /* PACIA1716 */
		0xd503215fU, /* PACIB1716 */
		0xd503219fU, /* AUTIA1716 */
		0xd50321dfU, /* AUTIB1716 */
		0xd503231fU, /* PACIAZ */
		0xd503233fU, /* PACIASP */
		0xd503235fU, /* PACIBZ */
		0xd503237fU, /* PACIBSP */
		0xd503239fU, /* AUTIAZ */
		0xd50323bfU, /* AUTIASP */
		0xd50323dfU, /* AUTIBZ */
		0xd50323ffU, /* AUTIBSP */
		0xd50324ffU, /* PACM, FEAT_PAuth_LR */
	};
	size_t index;

	for (index = 0; index < ARRAY_SIZE(pauth_hints); index++)
		if (instruction == pauth_hints[index])
			return true;

	/* BTI permits the four BType encodings selected by bits [7:6]. */
	return (instruction & 0xffffff3fU) == 0xd503241fU;
}

struct orlix_tcti_decoded_instruction orlix_tcti_decode_aarch64(u32 instruction)
{
	struct orlix_tcti_decoded_instruction decoded = {
		.decode_class = ORLIX_TCTI_DECODE_UNSUPPORTED,
		.instruction = instruction,
	};
	struct orlix_tcti_sve_predicated_integer_binary sve_predicated_binary;
	struct orlix_tcti_sve_crypto_instruction sve_crypto;
	int sve_ret;

	if ((instruction & AARCH64_SVC_MASK) == AARCH64_SVC_PATTERN) {
		decoded.decode_class = ORLIX_TCTI_DECODE_SVC;
		decoded.imm16 = (instruction >> 5) & 0xffffU;
		return decoded;
	}

	if ((instruction & AARCH64_BRK_MASK) == AARCH64_BRK_PATTERN) {
		decoded.decode_class = ORLIX_TCTI_DECODE_BRK;
		decoded.imm16 = (instruction >> 5) & 0xffffU;
		return decoded;
	}

	if ((instruction & AARCH64_HLT_MASK) == AARCH64_HLT_PATTERN) {
		decoded.decode_class = ORLIX_TCTI_DECODE_HLT;
		decoded.imm16 = (instruction >> 5) & 0xffffU;
		return decoded;
	}

	if ((instruction & AARCH64_RMIF_MASK) == AARCH64_RMIF_PATTERN) {
		decoded.decode_class = ORLIX_TCTI_DECODE_FLAG_MANIPULATION;
		decoded.flag_manipulation_op = ORLIX_TCTI_FLAG_MANIPULATION_RMIF;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.imm6 = (instruction >> 15) & 0x3fU;
		decoded.nzcv = instruction & 0xfU;
		return decoded;
	}

	if ((instruction & AARCH64_SETF_MASK) == AARCH64_SETF8_PATTERN ||
	    (instruction & AARCH64_SETF_MASK) == AARCH64_SETF16_PATTERN) {
		decoded.decode_class = ORLIX_TCTI_DECODE_FLAG_MANIPULATION;
		decoded.flag_manipulation_op =
			(instruction & AARCH64_SETF_MASK) == AARCH64_SETF8_PATTERN ?
			ORLIX_TCTI_FLAG_MANIPULATION_SETF8 :
			ORLIX_TCTI_FLAG_MANIPULATION_SETF16;
		decoded.rn = (instruction >> 5) & 0x1fU;
		return decoded;
	}

	if ((instruction & AARCH64_UDF_MASK) == AARCH64_UDF_PATTERN ||
	    (instruction & AARCH64_SVC_MASK) == AARCH64_HVC_PATTERN ||
	    (instruction & AARCH64_SVC_MASK) == AARCH64_SMC_PATTERN ||
	    (instruction & AARCH64_SVC_MASK) == AARCH64_DCPS1_PATTERN ||
	    (instruction & AARCH64_SVC_MASK) == AARCH64_DCPS2_PATTERN ||
	    (instruction & AARCH64_SVC_MASK) == AARCH64_DCPS3_PATTERN) {
		decoded.decode_class = ORLIX_TCTI_DECODE_UNDEFINED;
		decoded.imm16 = (instruction >> 5) & 0xffffU;
		return decoded;
	}

	switch (instruction) {
	case AARCH64_SMSTART_SM:
	case AARCH64_SMSTART_ZA:
	case AARCH64_SMSTART_SM_ZA:
		decoded.decode_class = ORLIX_TCTI_DECODE_SME_PSTATE_IMMEDIATE;
		decoded.sme_pstate_operation = ORLIX_TCTI_SME_PSTATE_SMSTART;
		decoded.sme_streaming_mode =
			instruction != AARCH64_SMSTART_ZA;
		decoded.sme_za = instruction != AARCH64_SMSTART_SM;
		return decoded;
	case AARCH64_SMSTOP_SM:
	case AARCH64_SMSTOP_ZA:
	case AARCH64_SMSTOP_SM_ZA:
		decoded.decode_class = ORLIX_TCTI_DECODE_SME_PSTATE_IMMEDIATE;
		decoded.sme_pstate_operation = ORLIX_TCTI_SME_PSTATE_SMSTOP;
		decoded.sme_streaming_mode =
			instruction != AARCH64_SMSTOP_ZA;
		decoded.sme_za = instruction != AARCH64_SMSTOP_SM;
		return decoded;
	default:
		break;
	}

	sve_ret = orlix_tcti_decode_sve_predicated_integer_binary(instruction,
						      &sve_predicated_binary);
	if (!sve_ret) {
		decoded.decode_class = ORLIX_TCTI_DECODE_SVE_PREDICATED_INTEGER_BINARY;
		decoded.sve_integer_binary_op = sve_predicated_binary.op;
		decoded.sve_predication = sve_predicated_binary.predication;
		decoded.rd = sve_predicated_binary.zd;
		decoded.rn = sve_predicated_binary.zn;
		decoded.rm = sve_predicated_binary.zm;
		decoded.sve_pg = sve_predicated_binary.pg;
		decoded.sve_element_bytes = sve_predicated_binary.element_bytes;
		return decoded;
	}
	if (sve_ret == -EINVAL)
		return decoded;
	sve_ret = orlix_tcti_decode_sve_crypto(instruction, &sve_crypto);
	if (!sve_ret) {
		decoded.decode_class = ORLIX_TCTI_DECODE_SVE_CRYPTO;
		decoded.sve_crypto_op = sve_crypto.op;
		decoded.sve_crypto_condition = sve_crypto.condition;
		decoded.rd = sve_crypto.zd;
		decoded.rn = sve_crypto.zn;
		decoded.rm = sve_crypto.zm;
		decoded.sve_crypto_zk = sve_crypto.zk;
		decoded.sve_crypto_index = sve_crypto.index;
		decoded.sve_crypto_nregs = sve_crypto.nregs;
		decoded.sve_element_bytes = sve_crypto.element_bytes;
		return decoded;
	}
	if (sve_ret == -EINVAL)
		return decoded;

	if (orlix_tcti_is_unimplemented_pauth_or_bti_hint(instruction))
		return decoded;

	/* DDI0602 2026-06: CPYFP/CPYFM/CPYFE and CPYP/CPYM/CPYE. */
	if ((instruction & AARCH64_MOPS_COPY_MASK) ==
	    AARCH64_MOPS_COPY_PATTERN) {
		u8 stage = (instruction >> 22) & 0x3U;
		bool forward_only = !(instruction & BIT(26));

		/* sz != 00 and op1 == 11 are reserved. */
		if ((instruction & GENMASK(31, 30)) || stage == 3)
			return decoded;
		decoded.decode_class = ORLIX_TCTI_DECODE_MOPS_COPY;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.mops_copy_stage = stage;
		decoded.mops_forward_only = forward_only;
		decoded.mops_options = (instruction >> 12) & 0xfU;
		decoded.mops_source_ordinal = (forward_only ? 2704U : 2764U) +
			stage * 16U + decoded.mops_options;
		return decoded;
	}

	if ((instruction & AARCH64_HINT_MASK) == AARCH64_HINT_PATTERN) {
		decoded.decode_class = ORLIX_TCTI_DECODE_HINT;
		decoded.hint_imm = (instruction >> 5) & 0x7fU;
		return decoded;
	}

	if ((instruction & AARCH64_BARRIER_MASK) == AARCH64_DSB_PATTERN ||
	    (instruction & AARCH64_BARRIER_MASK) == AARCH64_DMB_PATTERN ||
	    (instruction & AARCH64_BARRIER_MASK) == AARCH64_ISB_PATTERN ||
	    (instruction & AARCH64_DSB_NXS_MASK) == AARCH64_DSB_NXS_PATTERN) {
		u32 pattern = instruction & AARCH64_BARRIER_MASK;
		bool nxs = (instruction & AARCH64_DSB_NXS_MASK) ==
			AARCH64_DSB_NXS_PATTERN;

		decoded.barrier_option = (instruction >> 8) & 0xfU;
		if (nxs) {
			/* DSB <imm2>nXS permits only options 2, 6, 10, and 14. */
			if ((decoded.barrier_option & 0x3U) != 0x2U)
				return decoded;
			decoded.barrier_op = ORLIX_TCTI_BARRIER_DSB;
			decoded.barrier_nxs = true;
		} else if (pattern == AARCH64_ISB_PATTERN) {
			if (decoded.barrier_option != 0xfU)
				return decoded;
			decoded.barrier_op = ORLIX_TCTI_BARRIER_ISB;
		} else {
			if (!(decoded.barrier_option & 0x3U))
				return decoded;
			decoded.barrier_op = pattern == AARCH64_DSB_PATTERN ?
				ORLIX_TCTI_BARRIER_DSB : ORLIX_TCTI_BARRIER_DMB;
		}
		decoded.decode_class = ORLIX_TCTI_DECODE_BARRIER;
		return decoded;
	}

	if ((instruction & AARCH64_BARRIER_MASK) == AARCH64_CLREX_PATTERN) {
		decoded.decode_class = ORLIX_TCTI_DECODE_EXCLUSIVE_MONITOR_CLEAR;
		decoded.barrier_option = (instruction >> 8) & 0xfU;
		return decoded;
	}

	if ((instruction & AARCH64_CACHE_MAINTENANCE_MASK) ==
			AARCH64_IC_IVAU_PATTERN ||
	    (instruction & AARCH64_CACHE_MAINTENANCE_MASK) ==
			AARCH64_DC_CVAC_PATTERN ||
	    (instruction & AARCH64_CACHE_MAINTENANCE_MASK) ==
			AARCH64_DC_CVAU_PATTERN ||
	    (instruction & AARCH64_CACHE_MAINTENANCE_MASK) ==
			AARCH64_DC_CIVAC_PATTERN) {
		u32 pattern = instruction & AARCH64_CACHE_MAINTENANCE_MASK;

		decoded.decode_class = ORLIX_TCTI_DECODE_CACHE_MAINTENANCE;
		decoded.rt = instruction & 0x1fU;
		switch (pattern) {
		case AARCH64_IC_IVAU_PATTERN:
			decoded.cache_maintenance_op = ORLIX_TCTI_CACHE_IC_IVAU;
			break;
		case AARCH64_DC_CVAC_PATTERN:
			decoded.cache_maintenance_op = ORLIX_TCTI_CACHE_DC_CVAC;
			break;
		case AARCH64_DC_CVAU_PATTERN:
			decoded.cache_maintenance_op = ORLIX_TCTI_CACHE_DC_CVAU;
			break;
		case AARCH64_DC_CIVAC_PATTERN:
			decoded.cache_maintenance_op = ORLIX_TCTI_CACHE_DC_CIVAC;
			break;
		}
		return decoded;
	}

	if ((instruction & AARCH64_FCSEL_MASK) == AARCH64_FCSEL_PATTERN) {
		u8 type = (instruction >> 22) & 0x3U;

		if (type == 2)
			return decoded;
		decoded.decode_class = ORLIX_TCTI_DECODE_FP_CONDITIONAL_SELECT;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.condition = (instruction >> 12) & 0xfU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = type == 3 ? sizeof(u16) :
			type ? sizeof(u64) : sizeof(u32);
		decoded.result_size = decoded.access_size;
		decoded.simd_fp = true;
		return decoded;
	}

	if ((instruction & AARCH64_FCCMP_MASK) == AARCH64_FCCMP_PATTERN) {
		u8 type = (instruction >> 22) & 0x3U;

		if (type == 2)
			goto fp_int_gpr_unclaimed;
		decoded.decode_class = ORLIX_TCTI_DECODE_FP_SCALAR_COMPARE;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.condition = (instruction >> 12) & 0xfU;
		decoded.nzcv = instruction & 0xfU;
		decoded.access_size = type == 3 ? sizeof(u16) :
			type ? sizeof(u64) : sizeof(u32);
		decoded.result_size = decoded.access_size;
		decoded.fp_conditional = true;
		decoded.fp_signal_all_nans = instruction & BIT(4);
		decoded.simd_fp = true;
		return decoded;
	}

	if ((instruction & AARCH64_FP_SCALAR_3SOURCE_MASK) ==
	    AARCH64_FP_SCALAR_3SOURCE_PATTERN) {
		u8 type = (instruction >> 22) & 0x3U;

		if (type == 2)
			return decoded;
		decoded.decode_class = ORLIX_TCTI_DECODE_FP_SCALAR_3SOURCE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.ra = (instruction >> 10) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = type == 3 ? sizeof(u16) :
			type ? sizeof(u64) : sizeof(u32);
		decoded.result_size = decoded.access_size;
		decoded.simd_fp = true;
		decoded.fp3_op = (enum orlix_tcti_fp_scalar_3source_op)(
			((instruction & BIT(21)) ? 2 : 0) |
			((instruction & BIT(15)) ? 1 : 0));
		return decoded;
	}
	if ((instruction & AARCH64_SIMD_DUP_SCALAR_ELEMENT_MASK) ==
	    AARCH64_SIMD_DUP_SCALAR_ELEMENT_PATTERN) {
		u8 imm5 = (instruction >> 16) & 0x1fU;
		u8 size;

		if (!imm5)
			return decoded;
		size = __ffs(imm5);
		if (size > 3)
			return decoded;
		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = BIT(size);
		decoded.result_size = decoded.access_size;
		decoded.simd_scalar = true;
		decoded.simd_source_index = imm5 >> (size + 1);
		decoded.simd_element_move_op = ORLIX_TCTI_SIMD_ELEMENT_MOVE_DUP;
		return decoded;
	}

	if ((instruction & AARCH64_PC_RELATIVE_ADDRESS_MASK) ==
	    AARCH64_PC_RELATIVE_ADDRESS_PATTERN) {
		u64 imm = (orlix_tcti_bits(instruction, 5, 19) << 2) |
			  orlix_tcti_bits(instruction, 29, 2);

		decoded.decode_class = ORLIX_TCTI_DECODE_PC_RELATIVE_ADDRESS;
		decoded.rd = instruction & 0x1fU;
		decoded.page_relative = instruction & BIT(31);
		decoded.pc_relative_imm = decoded.page_relative ?
			sign_extend64(imm << 12, 32) : sign_extend64(imm, 20);
		return decoded;
	}

	/*
	 * FEAT_CSSC occupies the shift == 3 reservation in the generic
	 * add/sub-immediate encoding. Decode it before that broader family so a
	 * legal min/max immediate never becomes an add/sub instruction.
	 */
	if ((instruction & AARCH64_MIN_MAX_IMM_MASK) ==
	    AARCH64_MIN_MAX_IMM_PATTERN) {
		decoded.decode_class = ORLIX_TCTI_DECODE_MIN_MAX_IMMEDIATE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.is_64bit = instruction & BIT(31);
		decoded.min_max_immediate = (instruction >> 10) & 0xffU;
		decoded.min_max_immediate_op =
			(enum orlix_tcti_min_max_immediate_op)((instruction >> 18) & 0x3U);
		return decoded;
	}

	if ((instruction & AARCH64_ADD_SUB_IMM_MASK) ==
	    AARCH64_ADD_SUB_IMM_PATTERN) {
		u8 shift = (instruction >> 22) & 0x3U;

		if (shift > 1)
			return decoded;

		decoded.decode_class = ORLIX_TCTI_DECODE_ADD_SUB_IMMEDIATE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.imm12 = (instruction >> 10) & 0xfffU;
		decoded.shift = shift;
		decoded.is_64bit = instruction & BIT(31);
		decoded.subtract = instruction & BIT(30);
		decoded.set_flags = instruction & BIT(29);
		return decoded;
	}

	if ((instruction & AARCH64_ADD_SUB_SHIFTED_REG_MASK) ==
	    AARCH64_ADD_SUB_SHIFTED_REG_PATTERN) {
		u8 shift = (instruction >> 22) & 0x3U;
		u8 amount = (instruction >> 10) & 0x3fU;
		bool is_64bit = instruction & BIT(31);

		if (shift == 3 || (!is_64bit && (amount & BIT(5))))
			return decoded;

		decoded.decode_class = ORLIX_TCTI_DECODE_ADD_SUB_SHIFTED_REGISTER;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.shift = shift;
		decoded.shift_amount = amount;
		decoded.is_64bit = is_64bit;
		decoded.subtract = instruction & BIT(30);
		decoded.set_flags = instruction & BIT(29);
		return decoded;
	}

	if ((instruction & AARCH64_ADD_SUB_EXTENDED_REG_MASK) ==
	    AARCH64_ADD_SUB_EXTENDED_REG_PATTERN) {
		u8 amount = (instruction >> 10) & 0x7U;

		if (amount > 4)
			return decoded;

		decoded.decode_class = ORLIX_TCTI_DECODE_ADD_SUB_EXTENDED_REGISTER;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.shift_amount = amount;
		decoded.offset_extend = (instruction >> 13) & 0x7U;
		decoded.is_64bit = instruction & BIT(31);
		decoded.subtract = instruction & BIT(30);
		decoded.set_flags = instruction & BIT(29);
		return decoded;
	}

	if ((instruction & AARCH64_ADD_SUB_WITH_CARRY_MASK) ==
	    AARCH64_ADD_SUB_WITH_CARRY_PATTERN) {
		decoded.decode_class = ORLIX_TCTI_DECODE_ADD_SUB_WITH_CARRY;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.is_64bit = instruction & BIT(31);
		decoded.subtract = instruction & BIT(30);
		decoded.set_flags = instruction & BIT(29);
		return decoded;
	}

	if ((instruction & AARCH64_UNCONDITIONAL_BRANCH_IMM_MASK) ==
	    AARCH64_UNCONDITIONAL_BRANCH_IMM_PATTERN) {
		decoded.decode_class = ORLIX_TCTI_DECODE_UNCONDITIONAL_BRANCH_IMMEDIATE;
		decoded.branch_imm = sign_extend64(
			(u64)(instruction & 0x03ffffffU) << 2, 27);
		decoded.link = instruction & BIT(31);
		return decoded;
	}

	if ((instruction & AARCH64_BRANCH_REGISTER_MASK) == AARCH64_BR_PATTERN ||
	    (instruction & AARCH64_BRANCH_REGISTER_MASK) == AARCH64_BLR_PATTERN ||
	    (instruction & AARCH64_BRANCH_REGISTER_MASK) == AARCH64_RET_PATTERN) {
		u32 pattern = instruction & AARCH64_BRANCH_REGISTER_MASK;

		decoded.decode_class = ORLIX_TCTI_DECODE_UNCONDITIONAL_BRANCH_REGISTER;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.link = pattern == AARCH64_BLR_PATTERN;
		decoded.branch_register_op =
			pattern == AARCH64_BR_PATTERN ? ORLIX_TCTI_BRANCH_REGISTER_BR :
			pattern == AARCH64_BLR_PATTERN ? ORLIX_TCTI_BRANCH_REGISTER_BLR :
							  ORLIX_TCTI_BRANCH_REGISTER_RET;
		return decoded;
	}

	if ((instruction & AARCH64_COMPARE_BRANCH_IMM_MASK) ==
	    AARCH64_COMPARE_BRANCH_IMM_PATTERN) {
		u64 imm = ((instruction >> 5) & 0x7ffffU);

		decoded.decode_class = ORLIX_TCTI_DECODE_COMPARE_BRANCH_IMMEDIATE;
		decoded.rt = instruction & 0x1fU;
		decoded.is_64bit = instruction & BIT(31);
		decoded.nonzero = instruction & BIT(24);
		decoded.branch_imm = sign_extend64(imm << 2, 20);
		return decoded;
	}

	if ((instruction & AARCH64_COMPARE_BRANCH_EXTENSION_MASK) ==
	    AARCH64_COMPARE_BRANCH_EXTENSION_PATTERN) {
		u8 condition = (instruction >> 21) & 0x7U;
		u8 size = (instruction >> 14) & 0x3U;

		if (condition == 4 || condition == 5)
			return decoded;
		decoded.compare_branch_immediate = instruction & BIT(24);
		if (decoded.compare_branch_immediate) {
			if (instruction & BIT(14))
				return decoded;
			decoded.compare_branch_access_size =
				instruction & BIT(31) ? sizeof(u64) : sizeof(u32);
			decoded.imm6 = ((instruction >> 16) & 0x1fU) |
				       ((instruction >> 10) & 0x20U);
		} else {
			if (size == 1 || (size && (instruction & BIT(31))))
				return decoded;
			decoded.compare_branch_access_size =
				size == 2 ? sizeof(u8) :
				size == 3 ? sizeof(u16) :
				instruction & BIT(31) ? sizeof(u64) : sizeof(u32);
			decoded.rm = (instruction >> 16) & 0x1fU;
		}

		decoded.decode_class = ORLIX_TCTI_DECODE_COMPARE_BRANCH_EXTENSION;
		decoded.rt = instruction & 0x1fU;
		decoded.branch_imm = sign_extend64(
			((instruction >> 5) & 0x1ffU) << 2, 11);
		switch (condition) {
		case 0:
			decoded.compare_branch_condition = ORLIX_TCTI_COMPARE_BRANCH_GT;
			break;
		case 1:
			decoded.compare_branch_condition =
				decoded.compare_branch_immediate ? ORLIX_TCTI_COMPARE_BRANCH_LT :
				ORLIX_TCTI_COMPARE_BRANCH_GE;
			break;
		case 2:
			decoded.compare_branch_condition = ORLIX_TCTI_COMPARE_BRANCH_HI;
			break;
		case 3:
			decoded.compare_branch_condition =
				decoded.compare_branch_immediate ? ORLIX_TCTI_COMPARE_BRANCH_LO :
				ORLIX_TCTI_COMPARE_BRANCH_HS;
			break;
		case 6:
			decoded.compare_branch_condition = ORLIX_TCTI_COMPARE_BRANCH_EQ;
			break;
		default:
			decoded.compare_branch_condition = ORLIX_TCTI_COMPARE_BRANCH_NE;
			break;
		}
		return decoded;
	}

	if ((instruction & AARCH64_TEST_BRANCH_IMM_MASK) ==
	    AARCH64_TEST_BRANCH_IMM_PATTERN) {
		u8 bit_5 = instruction & BIT(31) ? BIT(5) : 0;
		u64 imm = (instruction >> 5) & 0x3fffU;

		decoded.decode_class = ORLIX_TCTI_DECODE_TEST_BRANCH_IMMEDIATE;
		decoded.rt = instruction & 0x1fU;
		decoded.nonzero = instruction & BIT(24);
		decoded.test_bit = bit_5 | ((instruction >> 19) & 0x1fU);
		decoded.branch_imm = sign_extend64(imm << 2, 15);
		return decoded;
	}

	if ((instruction & AARCH64_CONDITIONAL_BRANCH_IMM_MASK) ==
	    AARCH64_CONDITIONAL_BRANCH_IMM_PATTERN) {
		u64 imm = (instruction >> 5) & 0x7ffffU;

		decoded.decode_class = ORLIX_TCTI_DECODE_CONDITIONAL_BRANCH_IMMEDIATE;
		decoded.condition = instruction & 0xfU;
		decoded.branch_imm = sign_extend64(imm << 2, 20);
		return decoded;
	}

	if ((instruction & AARCH64_CONDITIONAL_COMPARE_MASK) ==
	    AARCH64_CONDITIONAL_COMPARE_PATTERN) {
		decoded.decode_class = ORLIX_TCTI_DECODE_CONDITIONAL_COMPARE;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.imm12 = (instruction >> 16) & 0x1fU;
		decoded.condition = (instruction >> 12) & 0xfU;
		decoded.nzcv = instruction & 0xfU;
		decoded.is_64bit = instruction & BIT(31);
		decoded.subtract = instruction & BIT(30);
		decoded.immediate = instruction & BIT(11);
		return decoded;
	}

	if ((instruction & AARCH64_CONDITIONAL_SELECT_MASK) ==
	    AARCH64_CONDITIONAL_SELECT_PATTERN) {
		bool op = instruction & BIT(30);
		bool op2 = instruction & BIT(10);

		decoded.decode_class = ORLIX_TCTI_DECODE_CONDITIONAL_SELECT;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.condition = (instruction >> 12) & 0xfU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.is_64bit = instruction & BIT(31);
		decoded.conditional_select_op =
			!op && !op2 ? ORLIX_TCTI_CONDITIONAL_SELECT_CSEL :
			!op && op2 ? ORLIX_TCTI_CONDITIONAL_SELECT_CSINC :
			op && !op2 ? ORLIX_TCTI_CONDITIONAL_SELECT_CSINV :
				     ORLIX_TCTI_CONDITIONAL_SELECT_CSNEG;
		return decoded;
	}

	if ((instruction & AARCH64_LOAD_LITERAL_MASK) ==
	    AARCH64_LOAD_LITERAL_PATTERN) {
		u8 opc = (instruction >> 30) & 0x3U;

		decoded.decode_class = ORLIX_TCTI_DECODE_LOAD_LITERAL;
		decoded.rt = instruction & 0x1fU;
		decoded.memory_offset =
			sign_extend64((instruction >> 5) & 0x7ffffU, 18) *
			sizeof(u32);
		decoded.load = true;
		decoded.memory_index_mode = ORLIX_TCTI_MEMORY_INDEX_SIGNED_OFFSET;

		if (instruction & BIT(26)) {
			if (opc == 3)
				return (struct orlix_tcti_decoded_instruction) {
					.decode_class = ORLIX_TCTI_DECODE_UNSUPPORTED,
					.instruction = instruction,
				};

			decoded.simd_fp = true;
			decoded.access_size = 1U << (opc + 2);
			decoded.result_size = decoded.access_size;
			return decoded;
		}

		switch (opc) {
		case 0:
			decoded.access_size = sizeof(u32);
			decoded.result_size = sizeof(u32);
			break;
		case 1:
			decoded.access_size = sizeof(u64);
			decoded.result_size = sizeof(u64);
			break;
		case 2:
			decoded.access_size = sizeof(u32);
			decoded.result_size = sizeof(u64);
			decoded.sign_extend_load = true;
			break;
		case 3:
			decoded.load = false;
			decoded.prefetch = true;
			break;
		}

		return decoded;
	}

	if ((instruction & AARCH64_LOAD_STORE_PAIR_MASK) ==
	    AARCH64_LOAD_STORE_PAIR_PATTERN) {
		u8 opc = (instruction >> 30) & 0x3U;
		u8 mode = (instruction >> 23) & 0x3U;
		bool simd_fp = instruction & BIT(26);
		u8 scale;

		if (simd_fp) {
			if (opc == 3)
				return decoded;
			scale = opc + 2;
		} else {
			if (opc == 0) {
				scale = 2;
			} else if (opc == 1) {
				if (!(instruction & BIT(22)) || mode == 0)
					return decoded;
				scale = 2;
				decoded.sign_extend_load = true;
			} else if (opc == 2) {
				scale = 3;
			} else {
				return decoded;
			}
		}

		decoded.decode_class = ORLIX_TCTI_DECODE_LOAD_STORE_PAIR;
		decoded.rt = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rt2 = (instruction >> 10) & 0x1fU;
		decoded.load = instruction & BIT(22);
		decoded.simd_fp = simd_fp;
		decoded.access_size = BIT(scale);
		decoded.result_size = decoded.sign_extend_load ?
				      sizeof(u64) : decoded.access_size;
		decoded.memory_offset =
			sign_extend64((instruction >> 15) & 0x7fU, 6) << scale;
		decoded.memory_index_mode =
			mode == 1 ? ORLIX_TCTI_MEMORY_INDEX_POST :
			(mode == 0 || mode == 2) ?
				ORLIX_TCTI_MEMORY_INDEX_SIGNED_OFFSET :
				    ORLIX_TCTI_MEMORY_INDEX_PRE;

		/*
		 * The pinned source omits the pair overlap ASL. Reject shared
		 * destinations and writeback overlaps instead of inventing an
		 * execution order. This is fail-closed containment and remains
		 * unproved until the official pinned ASL is available.
		 */
		if ((decoded.load && decoded.rt == decoded.rt2) ||
		    ((mode == 1 || mode == 3) &&
		     (decoded.rn == decoded.rt || decoded.rn == decoded.rt2)))
			return (struct orlix_tcti_decoded_instruction) {
				.decode_class = ORLIX_TCTI_DECODE_UNSUPPORTED,
				.instruction = instruction,
			};
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_MULTIPLE_STRUCTURE_MASK) ==
	    AARCH64_SIMD_MULTIPLE_STRUCTURE_PATTERN) {
		u8 opcode = (instruction >> 12) & 0xfU;
		u8 rm = (instruction >> 16) & 0x1fU;
		bool post_index = instruction & BIT(23);

		if (!post_index && rm)
			return decoded;

		decoded.simd_interleaved = opcode == 0 || opcode == 4 ||
					   opcode == 8;
		switch (opcode) {
		case 7:
			decoded.simd_structure_count = 1;
			break;
		case 8:
		case 10:
			decoded.simd_structure_count = 2;
			break;
		case 4:
		case 6:
			decoded.simd_structure_count = 3;
			break;
		case 0:
		case 2:
			decoded.simd_structure_count = 4;
			break;
		default:
			return decoded;
		}

		decoded.decode_class =
			ORLIX_TCTI_DECODE_SIMD_LOAD_STORE_MULTIPLE_STRUCTURE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = rm;
		decoded.load = instruction & BIT(22);
		decoded.simd_fp = true;
		decoded.simd_q = instruction & BIT(30);
		decoded.access_size = BIT((instruction >> 10) & 0x3U);
		decoded.result_size = decoded.simd_q ? 2 * sizeof(u64) :
						       sizeof(u64);
		decoded.memory_index_mode = post_index ? ORLIX_TCTI_MEMORY_INDEX_POST :
							 ORLIX_TCTI_MEMORY_INDEX_SIGNED_OFFSET;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_SINGLE_STRUCTURE_MASK) ==
	    AARCH64_SIMD_SINGLE_STRUCTURE_PATTERN) {
		u8 opcode = (instruction >> 13) & 0x7U;
		u8 size = (instruction >> 10) & 0x3U;
		u8 rm = (instruction >> 16) & 0x1fU;
		u8 base_opcode = opcode;
		bool post_index = instruction & BIT(23);
		bool replicate;

		if (!post_index && rm)
			return decoded;

		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = rm;
		decoded.load = instruction & BIT(22);
		decoded.simd_fp = true;
		decoded.simd_q = instruction & BIT(30);
		decoded.memory_index_mode = post_index ?
			ORLIX_TCTI_MEMORY_INDEX_POST : ORLIX_TCTI_MEMORY_INDEX_SIGNED_OFFSET;
		replicate = decoded.load && !(instruction & BIT(12)) &&
			(opcode == 6 || opcode == 7);

		if (replicate) {
			bool r = instruction & BIT(21);

			decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_LOAD_REPLICATE;
			decoded.simd_replicate = true;
			decoded.simd_structure_count =
				opcode == 6 ? (r ? 2 : 1) : (r ? 4 : 3);
			decoded.access_size = BIT(size);
			decoded.result_size = decoded.simd_q ?
				2 * sizeof(u64) : sizeof(u64);
			return decoded;
		}

		if (opcode & 1) {
			decoded.simd_structure_count =
				(instruction & BIT(21)) ? 4 : 3;
			base_opcode--;
		} else {
			decoded.simd_structure_count =
				(instruction & BIT(21)) ? 2 : 1;
		}

		if (base_opcode == 0) {
			decoded.access_size = sizeof(u8);
			decoded.simd_lane_index =
				(decoded.simd_q << 3) |
				(((instruction >> 12) & 1U) << 2) | size;
		} else if (base_opcode == 2 && !(size & 1U)) {
			decoded.access_size = sizeof(u16);
			decoded.simd_lane_index =
				(decoded.simd_q << 2) |
				(((instruction >> 12) & 1U) << 1) |
				(size >> 1);
		} else if (base_opcode == 4 && size == 0) {
			decoded.access_size = sizeof(u32);
			decoded.simd_lane_index =
				(decoded.simd_q << 1) |
				((instruction >> 12) & 1U);
		} else if (base_opcode == 4 && size == 1 &&
			   !(instruction & BIT(12))) {
			decoded.access_size = sizeof(u64);
			decoded.simd_lane_index = decoded.simd_q;
		} else {
			return (struct orlix_tcti_decoded_instruction) {
				.decode_class = ORLIX_TCTI_DECODE_UNSUPPORTED,
				.instruction = instruction,
			};
		}

		decoded.decode_class =
			ORLIX_TCTI_DECODE_SIMD_LOAD_STORE_SINGLE_STRUCTURE;
		return decoded;
	}

	if ((instruction & AARCH64_LOAD_STORE_UNSIGNED_IMM_MASK) ==
	    AARCH64_LOAD_STORE_UNSIGNED_IMM_PATTERN) {
		u8 size = (instruction >> 30) & 0x3U;
		u8 opc = (instruction >> 22) & 0x3U;
		bool simd_fp = instruction & BIT(26);
		u8 scale = size;

		if (!simd_fp && size == 3 && opc == 2) {
			decoded.decode_class = ORLIX_TCTI_DECODE_HINT;
			return decoded;
		}

		if (simd_fp) {
			if (!orlix_tcti_decode_simd_fp_load_store_variant(
				    size, opc, &decoded.load, &decoded.access_size,
				    &decoded.result_size))
				return decoded;
			if (decoded.access_size == 2 * sizeof(u64))
				scale = 4;

			decoded.decode_class =
				ORLIX_TCTI_DECODE_LOAD_STORE_UNSIGNED_IMMEDIATE;
			decoded.rt = instruction & 0x1fU;
			decoded.rn = (instruction >> 5) & 0x1fU;
			decoded.simd_fp = true;
			decoded.memory_offset =
				((instruction >> 10) & 0xfffU) << scale;
			decoded.memory_index_mode =
				ORLIX_TCTI_MEMORY_INDEX_SIGNED_OFFSET;
			return decoded;
		}

		if (!orlix_tcti_decode_load_store_variant(size, opc, &decoded.load,
						    &decoded.sign_extend_load,
						    &decoded.access_size,
						    &decoded.result_size))
			return decoded;

		decoded.decode_class = ORLIX_TCTI_DECODE_LOAD_STORE_UNSIGNED_IMMEDIATE;
		decoded.rt = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.memory_offset =
			((instruction >> 10) & 0xfffU) << size;
		decoded.memory_index_mode = ORLIX_TCTI_MEMORY_INDEX_SIGNED_OFFSET;
		return decoded;
	}

	if ((instruction & AARCH64_LOAD_STORE_SIGNED_IMM_MASK) ==
	    AARCH64_LOAD_STORE_SIGNED_IMM_PATTERN) {
		u8 size = (instruction >> 30) & 0x3U;
		u8 opc = (instruction >> 22) & 0x3U;
		u8 mode = (instruction >> 10) & 0x3U;
		bool simd_fp = instruction & BIT(26);

		if (!simd_fp && size == 3 && opc == 2) {
			if (mode == 0)
				decoded.decode_class = ORLIX_TCTI_DECODE_HINT;
			return decoded;
		}

		if (simd_fp && mode == 2)
			return decoded;

		if (simd_fp) {
			if (!orlix_tcti_decode_simd_fp_load_store_variant(
					size, opc, &decoded.load,
					&decoded.access_size, &decoded.result_size))
				return decoded;

			decoded.decode_class =
				ORLIX_TCTI_DECODE_LOAD_STORE_SIGNED_IMMEDIATE;
			decoded.rt = instruction & 0x1fU;
			decoded.rn = (instruction >> 5) & 0x1fU;
			decoded.simd_fp = true;
			decoded.memory_offset =
				sign_extend64((instruction >> 12) & 0x1ffU, 8);
			decoded.memory_index_mode =
				mode == 1 ? ORLIX_TCTI_MEMORY_INDEX_POST :
				mode == 3 ? ORLIX_TCTI_MEMORY_INDEX_PRE :
					    ORLIX_TCTI_MEMORY_INDEX_SIGNED_OFFSET;
			return decoded;
		}

		if (!orlix_tcti_decode_load_store_variant(size, opc, &decoded.load,
						   &decoded.sign_extend_load,
						   &decoded.access_size,
						   &decoded.result_size))
			return decoded;

		decoded.decode_class = ORLIX_TCTI_DECODE_LOAD_STORE_SIGNED_IMMEDIATE;
		decoded.rt = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.memory_offset =
			sign_extend64((instruction >> 12) & 0x1ffU, 8);
		decoded.memory_index_mode =
			mode == 1 ? ORLIX_TCTI_MEMORY_INDEX_POST :
			mode == 3 ? ORLIX_TCTI_MEMORY_INDEX_PRE :
				    ORLIX_TCTI_MEMORY_INDEX_SIGNED_OFFSET;

		/*
		 * The pinned source does not include the ASL needed to select an
		 * allowed writeback-overlap outcome. Reject overlapping ordinary
		 * integer pre/post-index forms instead of inventing a local order.
		 * This is fail-closed containment, not source-bound ISA proof.
		 * Rn == 31 is SP while Rt == 31 is ZR, so that case does not overlap.
		 */
		if (!simd_fp && (mode == 1 || mode == 3) &&
		    decoded.rn == decoded.rt && decoded.rn != 31)
			return (struct orlix_tcti_decoded_instruction) {
				.decode_class = ORLIX_TCTI_DECODE_UNSUPPORTED,
				.instruction = instruction,
			};
		return decoded;
	}

	if ((instruction & AARCH64_LOAD_STORE_REGISTER_OFFSET_MASK) ==
	    AARCH64_LOAD_STORE_REGISTER_OFFSET_PATTERN) {
		u8 size = (instruction >> 30) & 0x3U;
		u8 opc = (instruction >> 22) & 0x3U;
		u8 option = (instruction >> 13) & 0x7U;
		bool simd_fp = instruction & BIT(26);

		if (option != 2 && option != 3 && option != 6 && option != 7)
			return decoded;
		if (!simd_fp && size == 3 && opc == 2) {
			if ((instruction & 0x1fU) >= 24)
				return decoded;
			decoded.decode_class = ORLIX_TCTI_DECODE_HINT;
			return decoded;
		}

		if (simd_fp) {
			if (!orlix_tcti_decode_simd_fp_load_store_variant(
				    size, opc, &decoded.load,
				    &decoded.access_size, &decoded.result_size))
				return decoded;
			decoded.simd_fp = true;
		} else if (!orlix_tcti_decode_load_store_variant(
				   size, opc, &decoded.load,
				   &decoded.sign_extend_load,
				   &decoded.access_size, &decoded.result_size)) {
			return decoded;
		}

		decoded.decode_class = ORLIX_TCTI_DECODE_LOAD_STORE_REGISTER_OFFSET;
		decoded.rt = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.offset_extend = option;
		decoded.offset_shift = instruction & BIT(12);
		return decoded;
	}

	if ((instruction & AARCH64_LOGICAL_SHIFTED_REGISTER_MASK) ==
	    AARCH64_LOGICAL_SHIFTED_REGISTER_PATTERN) {
		u8 opc = (instruction >> 29) & 0x3U;
		u8 amount = (instruction >> 10) & 0x3fU;
		bool is_64bit = instruction & BIT(31);

		if (!is_64bit && (amount & BIT(5)))
			return decoded;

		decoded.decode_class = ORLIX_TCTI_DECODE_LOGICAL_SHIFTED_REGISTER;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.shift = (instruction >> 22) & 0x3U;
		decoded.shift_amount = amount;
		decoded.is_64bit = is_64bit;
		decoded.invert_second_operand = instruction & BIT(21);
		decoded.set_flags = opc == 3;
		decoded.logical_op =
			opc == 0 ? ORLIX_TCTI_LOGICAL_AND :
			opc == 1 ? ORLIX_TCTI_LOGICAL_ORR :
			opc == 2 ? ORLIX_TCTI_LOGICAL_EOR : ORLIX_TCTI_LOGICAL_AND;
		return decoded;
	}

	if ((instruction & AARCH64_LOGICAL_IMMEDIATE_MASK) ==
	    AARCH64_LOGICAL_IMMEDIATE_PATTERN) {
		u8 opc = (instruction >> 29) & 0x3U;
		bool n = instruction & BIT(22);
		u8 immr = (instruction >> 16) & 0x3fU;
		u8 imms = (instruction >> 10) & 0x3fU;

		if (!orlix_tcti_decode_logical_immediate_mask(instruction & BIT(31),
						       n, immr, imms,
						       &decoded.logical_immediate))
			return decoded;

		decoded.decode_class = ORLIX_TCTI_DECODE_LOGICAL_IMMEDIATE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.is_64bit = instruction & BIT(31);
		decoded.set_flags = opc == 3;
		decoded.logical_op =
			opc == 0 ? ORLIX_TCTI_LOGICAL_AND :
			opc == 1 ? ORLIX_TCTI_LOGICAL_ORR :
			opc == 2 ? ORLIX_TCTI_LOGICAL_EOR : ORLIX_TCTI_LOGICAL_AND;
		return decoded;
	}

	if ((instruction & AARCH64_BITFIELD_MASK) == AARCH64_BITFIELD_PATTERN) {
		u8 opc = (instruction >> 29) & 0x3U;
		bool sf = instruction & BIT(31);
		bool n = instruction & BIT(22);
		u8 immr = (instruction >> 16) & 0x3fU;
		u8 imms = (instruction >> 10) & 0x3fU;

		if (opc == 3)
			return decoded;
		if (!sf && (n || (immr & BIT(5)) || (imms & BIT(5))))
			return decoded;
		if (sf && !n)
			return decoded;

		decoded.decode_class = ORLIX_TCTI_DECODE_BITFIELD;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.is_64bit = sf;
		decoded.bitfield_immr = immr;
		decoded.bitfield_imms = imms;
		decoded.bitfield_op =
			opc == 0 ? ORLIX_TCTI_BITFIELD_SBFM :
			opc == 1 ? ORLIX_TCTI_BITFIELD_BFM : ORLIX_TCTI_BITFIELD_UBFM;
		return decoded;
	}

	if ((instruction & AARCH64_EXTRACT_MASK) == AARCH64_EXTRACT_PATTERN) {
		bool sf = instruction & BIT(31);
		bool n = instruction & BIT(22);
		u8 lsb = (instruction >> 10) & 0x3fU;

		if (n != sf)
			return decoded;
		if (!sf && (lsb & BIT(5)))
			return decoded;

		decoded.decode_class = ORLIX_TCTI_DECODE_EXTRACT;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.shift_amount = lsb;
		decoded.is_64bit = sf;
		return decoded;
	}

	if ((instruction & AARCH64_DATA_PROCESSING_1SOURCE_MASK) ==
	    AARCH64_DATA_PROCESSING_1SOURCE_PATTERN) {
		u8 opcode = (instruction >> 10) & 0x3fU;

		if (opcode == AARCH64_DP1_RBIT_OPCODE) {
			decoded.dp1_op = ORLIX_TCTI_DP1_RBIT;
		} else if (opcode == AARCH64_DP1_REV16_OPCODE) {
			decoded.dp1_op = ORLIX_TCTI_DP1_REV16;
		} else if (opcode == AARCH64_DP1_REV32_OPCODE) {
			decoded.dp1_op = instruction & BIT(31) ? ORLIX_TCTI_DP1_REV32 :
									 ORLIX_TCTI_DP1_REV;
		} else if (opcode == AARCH64_DP1_REV64_OPCODE &&
			   instruction & BIT(31)) {
			decoded.dp1_op = ORLIX_TCTI_DP1_REV;
		} else if (opcode == AARCH64_DP1_CLZ_OPCODE) {
			decoded.dp1_op = ORLIX_TCTI_DP1_CLZ;
		} else if (opcode == AARCH64_DP1_CLS_OPCODE) {
			decoded.dp1_op = ORLIX_TCTI_DP1_CLS;
		} else if (opcode == 0x06) {
			decoded.dp1_op = ORLIX_TCTI_DP1_CTZ;
		} else if (opcode == 0x07) {
			decoded.dp1_op = ORLIX_TCTI_DP1_CNT;
		} else if (opcode == 0x08) {
			decoded.dp1_op = ORLIX_TCTI_DP1_ABS;
		} else {
			goto fp_int_gpr_unclaimed;
		}

		decoded.decode_class = ORLIX_TCTI_DECODE_DATA_PROCESSING_1SOURCE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.is_64bit = instruction & BIT(31);
		return decoded;
	}

	if ((instruction & AARCH64_DATA_PROCESSING_2SOURCE_MASK) ==
	    AARCH64_DATA_PROCESSING_2SOURCE_PATTERN) {
		u8 opcode = (instruction >> 10) & 0x3fU;

		switch (opcode) {
		case 0x02:
			decoded.dp2_op = ORLIX_TCTI_DP2_UDIV;
			break;
		case 0x03:
			decoded.dp2_op = ORLIX_TCTI_DP2_SDIV;
			break;
		case 0x08:
			decoded.dp2_op = ORLIX_TCTI_DP2_LSLV;
			break;
		case 0x09:
			decoded.dp2_op = ORLIX_TCTI_DP2_LSRV;
			break;
		case 0x0a:
			decoded.dp2_op = ORLIX_TCTI_DP2_ASRV;
			break;
		case 0x0b:
			decoded.dp2_op = ORLIX_TCTI_DP2_RORV;
			break;
		case 0x18:
			decoded.dp2_op = ORLIX_TCTI_DP2_SMAX;
			break;
		case 0x19:
			decoded.dp2_op = ORLIX_TCTI_DP2_UMAX;
			break;
		case 0x1a:
			decoded.dp2_op = ORLIX_TCTI_DP2_SMIN;
			break;
		case 0x1b:
			decoded.dp2_op = ORLIX_TCTI_DP2_UMIN;
			break;
		case 0x10 ... 0x17: {
			u8 size = opcode & 0x3U;

			if ((size == 3) != !!(instruction & BIT(31)))
				return decoded;
			decoded.dp2_op = opcode & BIT(2) ?
				ORLIX_TCTI_DP2_CRC32C : ORLIX_TCTI_DP2_CRC32;
			decoded.access_size = BIT(size);
			decoded.result_size = sizeof(u32);
			break;
		}
		default:
			return decoded;
		}

		decoded.decode_class = ORLIX_TCTI_DECODE_DATA_PROCESSING_2SOURCE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.is_64bit = instruction & BIT(31);
		return decoded;
	}

	if ((instruction & AARCH64_MULTIPLY_ADD_SUB_MASK) ==
	    AARCH64_MULTIPLY_ADD_SUB_PATTERN) {
		u8 family = (instruction >> 21) & 0x7U;
		bool subtract = instruction & BIT(15);
		bool is_64bit = instruction & BIT(31);
		u8 ra = (instruction >> 10) & 0x1fU;

		switch (family) {
		case 0:
			decoded.mul_op = subtract ? ORLIX_TCTI_MUL_MSUB :
						   ORLIX_TCTI_MUL_MADD;
			break;
		case 1:
			if (!is_64bit)
				return decoded;
			decoded.mul_op = subtract ? ORLIX_TCTI_MUL_SMSUBL :
						   ORLIX_TCTI_MUL_SMADDL;
			break;
		case 2:
			if (!is_64bit || subtract || ra != 31)
				return decoded;
			decoded.mul_op = ORLIX_TCTI_MUL_SMULH;
			break;
		case 5:
			if (!is_64bit)
				return decoded;
			decoded.mul_op = subtract ? ORLIX_TCTI_MUL_UMSUBL :
						   ORLIX_TCTI_MUL_UMADDL;
			break;
		case 6:
			if (!is_64bit || subtract || ra != 31)
				return decoded;
			decoded.mul_op = ORLIX_TCTI_MUL_UMULH;
			break;
		default:
			return decoded;
		}

		decoded.decode_class = ORLIX_TCTI_DECODE_MULTIPLY_ADD_SUB;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.ra = ra;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.is_64bit = is_64bit;
		return decoded;
	}

	if ((instruction & AARCH64_MOVE_WIDE_IMM_MASK) ==
	    AARCH64_MOVE_WIDE_IMM_PATTERN) {
		u8 opc = (instruction >> 29) & 0x3U;
		u8 hw = (instruction >> 21) & 0x3U;
		bool is_64bit = instruction & BIT(31);

		if (opc == 1)
			return decoded;
		if (!is_64bit && (hw & BIT(1)))
			return decoded;

		decoded.decode_class = ORLIX_TCTI_DECODE_MOVE_WIDE_IMMEDIATE;
		decoded.rd = instruction & 0x1fU;
		decoded.imm16 = (instruction >> 5) & 0xffffU;
		decoded.halfword_shift = hw * 16;
		decoded.is_64bit = is_64bit;
		decoded.move_wide_op =
			opc == 0 ? ORLIX_TCTI_MOVE_WIDE_MOVN :
			opc == 2 ? ORLIX_TCTI_MOVE_WIDE_MOVZ : ORLIX_TCTI_MOVE_WIDE_MOVK;
		return decoded;
	}

	if ((instruction & AARCH64_LSE_CAS_MASK) == AARCH64_LSE_CAS_PATTERN) {
		decoded.decode_class = ORLIX_TCTI_DECODE_LSE_ATOMIC;
		decoded.lse_atomic_op = ORLIX_TCTI_LSE_ATOMIC_CAS;
		decoded.rs = orlix_tcti_bits(instruction, 16, 5);
		decoded.rn = orlix_tcti_bits(instruction, 5, 5);
		decoded.rt = orlix_tcti_bits(instruction, 0, 5);
		decoded.access_size = 1U << orlix_tcti_bits(instruction, 30, 2);
		decoded.result_size = decoded.access_size;
		decoded.acquire = instruction & BIT(22);
		decoded.release = instruction & BIT(15);
		return decoded;
	}

	if ((instruction & AARCH64_LSE_CASP_MASK) == AARCH64_LSE_CASP_PATTERN) {
		decoded.rs = orlix_tcti_bits(instruction, 16, 5);
		decoded.rt = orlix_tcti_bits(instruction, 0, 5);
		if ((decoded.rs & 1U) || (decoded.rt & 1U))
			return decoded;
		decoded.decode_class = ORLIX_TCTI_DECODE_LSE_ATOMIC;
		decoded.lse_atomic_op = ORLIX_TCTI_LSE_ATOMIC_CAS;
		decoded.rn = orlix_tcti_bits(instruction, 5, 5);
		decoded.rt2 = decoded.rt + 1;
		decoded.access_size = 4U << orlix_tcti_bits(instruction, 30, 1);
		decoded.result_size = decoded.access_size;
		decoded.is_64bit = orlix_tcti_bits(instruction, 30, 1);
		decoded.acquire = instruction & BIT(22);
		decoded.release = instruction & BIT(15);
		decoded.pair = true;
		return decoded;
	}

	/*
	 * FEAT_LSE128 uses two independently encoded 64-bit registers as the
	 * 128-bit operand and returned old value. DDI 0602 declares XZR in either
	 * result lane UNDEFINED and Rt == Rt2 CONSTRAINED UNPREDICTABLE, so reject
	 * all three forms rather than assigning local execution semantics.
	 */
	if ((instruction & AARCH64_LSE128_RMW_MASK) ==
	    AARCH64_LSE128_RMW_PATTERN) {
		u8 op = orlix_tcti_bits(instruction, 12, 4);

		switch (op) {
		case 1:
			decoded.lse_atomic_op = ORLIX_TCTI_LSE_ATOMIC_CLR;
			break;
		case 3:
			decoded.lse_atomic_op = ORLIX_TCTI_LSE_ATOMIC_SET;
			break;
		case 8:
			decoded.lse_atomic_op = ORLIX_TCTI_LSE_ATOMIC_SWP;
			break;
		default:
			return decoded;
		}
		decoded.rt = orlix_tcti_bits(instruction, 0, 5);
		decoded.rt2 = orlix_tcti_bits(instruction, 16, 5);
		if (decoded.rt == 31 || decoded.rt2 == 31 ||
		    decoded.rt == decoded.rt2)
			return decoded;
		decoded.decode_class = ORLIX_TCTI_DECODE_LSE_ATOMIC;
		decoded.rn = orlix_tcti_bits(instruction, 5, 5);
		decoded.access_size = sizeof(u64);
		decoded.result_size = decoded.access_size;
		decoded.is_64bit = true;
		decoded.acquire = instruction & BIT(23);
		decoded.release = instruction & BIT(22);
		decoded.pair = true;
		decoded.lse128 = true;
		return decoded;
	}

	if ((instruction & AARCH64_LSE_RMW_MASK) == AARCH64_LSE_RMW_PATTERN) {
		u8 op = orlix_tcti_bits(instruction, 12, 4);

		if (op > 8)
			return decoded;
		decoded.decode_class = ORLIX_TCTI_DECODE_LSE_ATOMIC;
		decoded.lse_atomic_op = op == 8 ? ORLIX_TCTI_LSE_ATOMIC_SWP :
			ORLIX_TCTI_LSE_ATOMIC_ADD + op;
		decoded.rs = orlix_tcti_bits(instruction, 16, 5);
		decoded.rn = orlix_tcti_bits(instruction, 5, 5);
		decoded.rt = orlix_tcti_bits(instruction, 0, 5);
		decoded.access_size = 1U << orlix_tcti_bits(instruction, 30, 2);
		decoded.result_size = decoded.access_size;
		decoded.acquire = instruction & BIT(23);
		decoded.release = instruction & BIT(22);
		return decoded;
	}

	/* Reject reserved LSE encodings before the broader exclusive decoder. */
	if ((instruction & 0xbf200000U) == 0x08200000U ||
	    (instruction & 0x3f208000U) == AARCH64_LSE_RMW_PATTERN)
		return decoded;

	if ((instruction & AARCH64_LOAD_STORE_EXCLUSIVE_MASK) ==
	    AARCH64_LOAD_STORE_EXCLUSIVE_PATTERN) {
		u8 size = (instruction >> 30) & 0x3U;
		bool load = instruction & BIT(22);
		bool o2 = instruction & BIT(23);
		bool o1 = instruction & BIT(21);
		bool o0 = instruction & BIT(15);
		u8 rs = (instruction >> 16) & 0x1fU;
		u8 rt2 = (instruction >> 10) & 0x1fU;
		bool pair = !o2 && o1;

		if (o2) {
			if (o1 || !o0 || rs != 31 || rt2 != 31)
				return decoded;
		} else {
			if ((!pair && rt2 != 31) || (pair && size < 2))
				return decoded;
			if (load && rs != 31)
				return decoded;
		}

		decoded.decode_class = ORLIX_TCTI_DECODE_LOAD_STORE_EXCLUSIVE;
		decoded.rt = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rt2 = rt2;
		decoded.rs = rs;
		decoded.access_size = BIT(size);
		decoded.result_size = decoded.access_size;
		decoded.load = load;
		decoded.exclusive = !o2;
		decoded.pair = pair;
		decoded.acquire = load && o0;
		decoded.release = !load && o0;
		return decoded;
	}

	if ((((instruction & AARCH64_SIMD_SATURATING_SHIFT_LEFT_MASK) ==
	      AARCH64_SIMD_SQSHL_UQSHL_IMMEDIATE_PATTERN) ||
	     ((instruction & AARCH64_SIMD_SATURATING_SHIFT_LEFT_MASK) ==
	      AARCH64_SIMD_SQSHLU_IMMEDIATE_PATTERN &&
	      (instruction & BIT(29)))) &&
	    ((instruction >> 16) & 0x7fU) >= 8) {
		u8 immediate = (instruction >> 16) & 0x7fU;
		u8 access_size;
		u32 pattern =
			instruction & AARCH64_SIMD_SATURATING_SHIFT_LEFT_MASK;
		bool scalar = instruction & BIT(28);

		if (immediate < 16)
			access_size = sizeof(u8);
		else if (immediate < 32)
			access_size = sizeof(u16);
		else if (immediate < 64)
			access_size = sizeof(u32);
		else
			access_size = sizeof(u64);
		if ((scalar && !(instruction & BIT(30))) ||
		    (!scalar && !(instruction & BIT(30)) &&
		     access_size == sizeof(u64)))
			return decoded;

		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = access_size;
		decoded.result_size = scalar ? access_size :
			(instruction & BIT(30) ? 2 * sizeof(u64) : sizeof(u64));
		decoded.shift_amount = immediate - access_size * 8;
		decoded.immediate = true;
		decoded.simd_fp = true;
		decoded.simd_scalar = scalar;
		if (pattern == AARCH64_SIMD_SQSHLU_IMMEDIATE_PATTERN)
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_SQSHLU;
		else
			decoded.simd_arithmetic_op = instruction & BIT(29) ?
				ORLIX_TCTI_SIMD_ARITH_UQSHL : ORLIX_TCTI_SIMD_ARITH_SQSHL;
		return decoded;
	}

	if ((((instruction & AARCH64_SIMD_SHIFT_LEFT_INSERT_MASK) ==
	      AARCH64_SIMD_SHL_SLI_PATTERN) ||
	     ((instruction & AARCH64_SIMD_SHIFT_LEFT_INSERT_MASK) ==
	      AARCH64_SIMD_SRI_PATTERN && (instruction & BIT(29)))) &&
	    ((instruction >> 16) & 0x7fU) >= 8) {
		u8 immediate = (instruction >> 16) & 0x7fU;
		u8 access_size;
		u32 pattern = instruction & AARCH64_SIMD_SHIFT_LEFT_INSERT_MASK;
		bool scalar = instruction & BIT(28);

		if (immediate < 16)
			access_size = sizeof(u8);
		else if (immediate < 32)
			access_size = sizeof(u16);
		else if (immediate < 64)
			access_size = sizeof(u32);
		else
			access_size = sizeof(u64);
		if ((scalar && (!(instruction & BIT(30)) ||
			       access_size != sizeof(u64))) ||
		    (!scalar && !(instruction & BIT(30)) &&
		     access_size == sizeof(u64)))
			return decoded;

		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = access_size;
		decoded.result_size = scalar ? sizeof(u64) :
			(instruction & BIT(30) ? 2 * sizeof(u64) : sizeof(u64));
		decoded.simd_fp = true;
		decoded.simd_scalar = scalar;
		if (pattern == AARCH64_SIMD_SRI_PATTERN) {
			decoded.shift_amount = access_size * 16 - immediate;
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_SRI;
		} else {
			decoded.shift_amount = immediate - access_size * 8;
			decoded.simd_arithmetic_op = instruction & BIT(29) ?
				ORLIX_TCTI_SIMD_ARITH_SLI : ORLIX_TCTI_SIMD_ARITH_SHL;
		}
		return decoded;
	}

	if (((instruction & AARCH64_SIMD_SHIFT_RIGHT_MASK) ==
	     AARCH64_SIMD_SSHR_PATTERN ||
	     (instruction & AARCH64_SIMD_SHIFT_RIGHT_MASK) ==
	     AARCH64_SIMD_SSRA_PATTERN ||
	     (instruction & AARCH64_SIMD_SHIFT_RIGHT_MASK) ==
	     AARCH64_SIMD_SRSHR_PATTERN ||
	     (instruction & AARCH64_SIMD_SHIFT_RIGHT_MASK) ==
	     AARCH64_SIMD_SRSRA_PATTERN) &&
	    ((instruction >> 16) & 0x7fU) >= 8) {
		u8 immediate = (instruction >> 16) & 0x7fU;
		u8 access_size;
		u32 pattern = instruction & AARCH64_SIMD_SHIFT_RIGHT_MASK;
		bool u = instruction & BIT(29);
		bool scalar = instruction & BIT(28);

		if (immediate < 16)
			access_size = sizeof(u8);
		else if (immediate < 32)
			access_size = sizeof(u16);
		else if (immediate < 64)
			access_size = sizeof(u32);
		else
			access_size = sizeof(u64);
		if ((scalar && (!(instruction & BIT(30)) ||
			       access_size != sizeof(u64))) ||
		    (!scalar && !(instruction & BIT(30)) &&
		     access_size == sizeof(u64)))
			return decoded;

		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = access_size;
		decoded.result_size = scalar ? sizeof(u64) :
			(instruction & BIT(30) ? 2 * sizeof(u64) : sizeof(u64));
		decoded.shift_amount = access_size * 16 - immediate;
		decoded.simd_fp = true;
		decoded.simd_scalar = scalar;
		if (pattern == AARCH64_SIMD_SSHR_PATTERN)
			decoded.simd_arithmetic_op = u ? ORLIX_TCTI_SIMD_ARITH_USHR :
				ORLIX_TCTI_SIMD_ARITH_SSHR;
		else if (pattern == AARCH64_SIMD_SSRA_PATTERN)
			decoded.simd_arithmetic_op = u ? ORLIX_TCTI_SIMD_ARITH_USRA :
				ORLIX_TCTI_SIMD_ARITH_SSRA;
		else if (pattern == AARCH64_SIMD_SRSHR_PATTERN)
			decoded.simd_arithmetic_op = u ? ORLIX_TCTI_SIMD_ARITH_URSHR :
				ORLIX_TCTI_SIMD_ARITH_SRSHR;
		else
			decoded.simd_arithmetic_op = u ? ORLIX_TCTI_SIMD_ARITH_URSRA :
				ORLIX_TCTI_SIMD_ARITH_SRSRA;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_SHIFT_BY_REGISTER_MASK) ==
	    AARCH64_SIMD_SHIFT_BY_REGISTER_PATTERN) {
		u8 size = (instruction >> 22) & 0x3U;
		u8 opcode = (instruction >> 11) & 0x1fU;
		u8 operation;
		bool q = instruction & BIT(30);
		bool scalar = instruction & BIT(28);

		if (opcode < 8 || opcode > 11 || (scalar && !q) ||
		    (!scalar && !q && size == 3) ||
		    (scalar && (opcode == 8 || opcode == 10) && size != 3))
			return decoded;

		operation = (opcode - 8) * 2 + !!(instruction & BIT(29));
		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = BIT(size);
		decoded.result_size = scalar ? decoded.access_size :
			(q ? 2 * sizeof(u64) : sizeof(u64));
		decoded.simd_fp = true;
		decoded.simd_scalar = scalar;
		decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_SSHL + operation;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_MODIFIED_IMMEDIATE_MASK) ==
	    AARCH64_SIMD_MODIFIED_IMMEDIATE_PATTERN) {
		if (!orlix_tcti_decode_simd_modified_immediate(instruction, &decoded))
			decoded.decode_class = ORLIX_TCTI_DECODE_UNSUPPORTED;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_XTN_MASK) ==
	    AARCH64_SIMD_XTN_PATTERN) {
		u8 size = (instruction >> 22) & 0x3U;

		if (size == 3)
			return decoded;

		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = 2U << size;
		decoded.result_size = 1U << size;
		decoded.simd_destination_index = !!(instruction & BIT(30));
		decoded.simd_fp = true;
		decoded.simd_element_move_op = ORLIX_TCTI_SIMD_ELEMENT_MOVE_XTN;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_SHIFT_LEFT_LONG_MASK) ==
	    AARCH64_SIMD_SHIFT_LEFT_LONG_PATTERN) {
		u8 immh = (instruction >> 19) & 0xfU;
		u8 immb = (instruction >> 16) & 0x7U;
		u8 element_bits;

		if (!immh || (immh & 0x8U))
			return decoded;

		if (immh & 0x4U)
			decoded.access_size = sizeof(u32);
		else if (immh & 0x2U)
			decoded.access_size = sizeof(u16);
		else
			decoded.access_size = sizeof(u8);

		element_bits = decoded.access_size * 8;
		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.result_size = 2 * sizeof(u64);
		decoded.shift_amount = ((immh << 3) | immb) - element_bits;
		if (instruction & BIT(30))
			decoded.simd_source_index = sizeof(u64) /
						decoded.access_size;
		decoded.simd_fp = true;
		decoded.simd_element_move_op = instruction & BIT(29) ?
			ORLIX_TCTI_SIMD_ELEMENT_MOVE_USHLL :
			ORLIX_TCTI_SIMD_ELEMENT_MOVE_SSHLL;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_SHLL_MASK) ==
	    AARCH64_SIMD_SHLL_PATTERN) {
		u8 size = (instruction >> 22) & 0x3U;

		if (size == 3)
			return decoded;

		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = BIT(size);
		decoded.result_size = 2 * sizeof(u64);
		decoded.shift_amount = decoded.access_size * 8;
		decoded.simd_source_index = instruction & BIT(30) ?
			sizeof(u64) / decoded.access_size : 0;
		decoded.simd_fp = true;
		decoded.simd_element_move_op = ORLIX_TCTI_SIMD_ELEMENT_MOVE_USHLL;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_PERMUTE_MASK) ==
	    AARCH64_SIMD_PERMUTE_PATTERN) {
		u8 size = (instruction >> 22) & 0x3U;
		u8 operation = (instruction >> 12) & 0x7U;

		if (size == 3 && !(instruction & BIT(30)))
			return decoded;

		switch (operation) {
		case 1:
			decoded.simd_element_move_op = ORLIX_TCTI_SIMD_ELEMENT_MOVE_UZP1;
			break;
		case 5:
			decoded.simd_element_move_op = ORLIX_TCTI_SIMD_ELEMENT_MOVE_UZP2;
			break;
		case 2:
			decoded.simd_element_move_op = ORLIX_TCTI_SIMD_ELEMENT_MOVE_TRN1;
			break;
		case 6:
			decoded.simd_element_move_op = ORLIX_TCTI_SIMD_ELEMENT_MOVE_TRN2;
			break;
		case 3:
			decoded.simd_element_move_op = ORLIX_TCTI_SIMD_ELEMENT_MOVE_ZIP1;
			break;
		case 7:
			decoded.simd_element_move_op = ORLIX_TCTI_SIMD_ELEMENT_MOVE_ZIP2;
			break;
		default:
			return decoded;
		}

		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = 1U << size;
		decoded.result_size = (instruction & BIT(30)) ?
			2 * sizeof(u64) : sizeof(u64);
		decoded.simd_fp = true;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_UMOV_MASK) ==
	    AARCH64_SIMD_UMOV_PATTERN) {
		u8 imm5 = (instruction >> 16) & 0x1fU;
		u8 lane_shift;
		bool q = instruction & BIT(30);

		if (!imm5)
			return decoded;

		lane_shift = __builtin_ctz((unsigned int)imm5);
		if (lane_shift > 3 || q != (lane_shift == 3))
			return decoded;

		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = BIT(lane_shift);
		decoded.result_size = q ? sizeof(u64) : sizeof(u32);
		decoded.simd_source_index = imm5 >> (lane_shift + 1);
		decoded.simd_fp = true;
		decoded.simd_element_move_op = ORLIX_TCTI_SIMD_ELEMENT_MOVE_UMOV;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_SMOV_MASK) ==
	    AARCH64_SIMD_SMOV_PATTERN) {
		u8 imm5 = (instruction >> 16) & 0x1fU;
		u8 lane_shift;
		bool q = instruction & BIT(30);

		if (!imm5)
			return decoded;

		lane_shift = __builtin_ctz((unsigned int)imm5);
		if (lane_shift > 2 || (!q && lane_shift > 1))
			return decoded;

		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = BIT(lane_shift);
		decoded.result_size = q ? sizeof(u64) : sizeof(u32);
		decoded.simd_source_index = imm5 >> (lane_shift + 1);
		decoded.simd_fp = true;
		decoded.simd_element_move_op = ORLIX_TCTI_SIMD_ELEMENT_MOVE_SMOV;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_INS_GPR_MASK) ==
	    AARCH64_SIMD_INS_GPR_PATTERN) {
		u8 imm5 = (instruction >> 16) & 0x1fU;
		u8 lane_shift;

		if (!imm5)
			return decoded;
		lane_shift = __builtin_ctz((unsigned int)imm5);
		if (lane_shift > 3)
			return decoded;

		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = BIT(lane_shift);
		decoded.result_size = decoded.access_size;
		decoded.simd_destination_index = imm5 >> (lane_shift + 1);
		decoded.simd_fp = true;
		decoded.simd_element_move_op = ORLIX_TCTI_SIMD_ELEMENT_MOVE_INS_GPR;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_EXT_MASK) ==
	    AARCH64_SIMD_EXT_PATTERN) {
		u8 q = !!(instruction & BIT(30));

		if (!q && (instruction & BIT(14)))
			return decoded;

		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.shift_amount = (instruction >> 11) & 0xfU;
		decoded.access_size = q ? 2 * sizeof(u64) : sizeof(u64);
		decoded.result_size = decoded.access_size;
		decoded.simd_fp = true;
		decoded.simd_element_move_op = ORLIX_TCTI_SIMD_ELEMENT_MOVE_EXT;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_TABLE_LOOKUP_MASK) ==
	    AARCH64_SIMD_TABLE_LOOKUP_PATTERN) {
		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_TABLE_LOOKUP;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.simd_q = !!(instruction & BIT(30));
		decoded.result_size = decoded.simd_q ? 2 * sizeof(u64) :
						       sizeof(u64);
		decoded.simd_table_count = ((instruction >> 13) & 0x3U) + 1;
		decoded.simd_table_lookup_op = instruction & BIT(12) ?
			ORLIX_TCTI_SIMD_TABLE_LOOKUP_TBX : ORLIX_TCTI_SIMD_TABLE_LOOKUP_TBL;
		decoded.simd_fp = true;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_DUP_ELEMENT_MASK) ==
	    AARCH64_SIMD_DUP_ELEMENT_PATTERN) {
		u8 imm5 = (instruction >> 16) & 0x1fU;
		u8 size;
		bool q = instruction & BIT(30);

		if (!imm5)
			return decoded;

		size = __ffs(imm5);
		if (size > 3 || (size == 3 && !q))
			return decoded;

		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = BIT(size);
		decoded.result_size = q ? 2 * sizeof(u64) : sizeof(u64);
		decoded.simd_source_index = imm5 >> (size + 1);
		decoded.simd_fp = true;
		decoded.simd_element_move_op = ORLIX_TCTI_SIMD_ELEMENT_MOVE_DUP;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_VECTOR_ELEMENT_MOVE_MASK) ==
	    AARCH64_SIMD_VECTOR_ELEMENT_MOVE_PATTERN) {
		u8 imm5 = (instruction >> 16) & 0x1fU;
		u8 imm4 = (instruction >> 11) & 0xfU;
		u8 size;

		if (!imm5)
			return decoded;
		size = __ffs(imm5);
		if (size > 3 || (imm4 & (BIT(size) - 1)))
			return decoded;

		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = BIT(size);
		decoded.result_size = 2 * sizeof(u64);
		decoded.simd_destination_index = imm5 >> (size + 1);
		decoded.simd_source_index = imm4 >> size;
		decoded.simd_fp = true;
		decoded.simd_element_move_op = ORLIX_TCTI_SIMD_ELEMENT_MOVE_INS_ELEMENT;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_DUP_GPR_MASK) ==
	    AARCH64_SIMD_DUP_GPR_PATTERN) {
		u8 imm5 = (instruction >> 16) & 0x1fU;
		bool q = instruction & BIT(30);

		if (!imm5 || (imm5 & (imm5 - 1)) || imm5 > sizeof(u64) ||
		    (imm5 == sizeof(u64) && !q))
			return decoded;

		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = imm5;
		decoded.result_size = q ? 2 * sizeof(u64) : sizeof(u64);
		decoded.simd_fp = true;
		decoded.immediate = true;
		decoded.simd_element_move_op = ORLIX_TCTI_SIMD_ELEMENT_MOVE_DUP;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_VECTOR_LOGICAL_MASK) ==
	    AARCH64_SIMD_VECTOR_LOGICAL_PATTERN) {
		u8 operation = ((instruction >> 22) & 0x3U) |
			       ((instruction >> 27) & 0x4U);

		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_LOGICAL;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = instruction & BIT(30) ?
			2 * sizeof(u64) : sizeof(u64);
		decoded.result_size = decoded.access_size;
		decoded.simd_fp = true;
		decoded.logical_op = operation;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_SCALAR_FP_ESTIMATE_MASK) ==
	    AARCH64_SIMD_FRECPE_SCALAR_PATTERN ||
	    (instruction & AARCH64_SIMD_SCALAR_FP_ESTIMATE_MASK) ==
	    AARCH64_SIMD_FRECPX_SCALAR_PATTERN ||
	    (instruction & AARCH64_SIMD_SCALAR_FP_ESTIMATE_MASK) ==
	    AARCH64_SIMD_FRSQRTE_SCALAR_PATTERN ||
	    (instruction & AARCH64_SIMD_FCVTXN_SCALAR_MASK) ==
	    AARCH64_SIMD_FCVTXN_SCALAR_PATTERN) {
		u32 estimate_pattern = instruction &
			AARCH64_SIMD_SCALAR_FP_ESTIMATE_MASK;
		bool fcvtxn = (instruction & AARCH64_SIMD_FCVTXN_SCALAR_MASK) ==
			AARCH64_SIMD_FCVTXN_SCALAR_PATTERN;

		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = fcvtxn || (instruction & BIT(22)) ?
			sizeof(u64) : sizeof(u32);
		decoded.result_size = fcvtxn ? sizeof(u32) :
			decoded.access_size;
		decoded.simd_fp = true;
		decoded.simd_scalar = true;
		if (fcvtxn)
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FCVTXN;
		else if (estimate_pattern == AARCH64_SIMD_FRECPE_SCALAR_PATTERN)
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FRECPE;
		else if (estimate_pattern == AARCH64_SIMD_FRECPX_SCALAR_PATTERN)
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FRECPX;
		else
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FRSQRTE;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_FP16_SCALAR_THREE_SAME_MASK) ==
	    AARCH64_SIMD_FMULX_FP16_SCALAR_PATTERN ||
	    (instruction & AARCH64_SIMD_FP16_SCALAR_THREE_SAME_MASK) ==
	    AARCH64_SIMD_FCMEQ_FP16_SCALAR_PATTERN ||
	    (instruction & AARCH64_SIMD_FP16_SCALAR_THREE_SAME_MASK) ==
	    AARCH64_SIMD_FRECPS_FP16_SCALAR_PATTERN ||
	    (instruction & AARCH64_SIMD_FP16_SCALAR_THREE_SAME_MASK) ==
	    AARCH64_SIMD_FRSQRTS_FP16_SCALAR_PATTERN ||
	    (instruction & AARCH64_SIMD_FP16_SCALAR_THREE_SAME_MASK) ==
	    AARCH64_SIMD_FCMGE_FP16_SCALAR_PATTERN ||
	    (instruction & AARCH64_SIMD_FP16_SCALAR_THREE_SAME_MASK) ==
	    AARCH64_SIMD_FACGE_FP16_SCALAR_PATTERN ||
	    (instruction & AARCH64_SIMD_FP16_SCALAR_THREE_SAME_MASK) ==
	    AARCH64_SIMD_FABD_FP16_SCALAR_PATTERN ||
	    (instruction & AARCH64_SIMD_FP16_SCALAR_THREE_SAME_MASK) ==
	    AARCH64_SIMD_FCMGT_FP16_SCALAR_PATTERN ||
	    (instruction & AARCH64_SIMD_FP16_SCALAR_THREE_SAME_MASK) ==
	    AARCH64_SIMD_FACGT_FP16_SCALAR_PATTERN) {
		u32 pattern = instruction & AARCH64_SIMD_FP16_SCALAR_THREE_SAME_MASK;

		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = sizeof(u16);
		decoded.result_size = sizeof(u16);
		decoded.simd_fp = true;
		decoded.simd_scalar = true;
		decoded.simd_q = true;
		switch (pattern) {
		case AARCH64_SIMD_FMULX_FP16_SCALAR_PATTERN:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FMULX;
			break;
		case AARCH64_SIMD_FCMEQ_FP16_SCALAR_PATTERN:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FCMEQ;
			break;
		case AARCH64_SIMD_FRECPS_FP16_SCALAR_PATTERN:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FRECPS;
			break;
		case AARCH64_SIMD_FRSQRTS_FP16_SCALAR_PATTERN:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FRSQRTS;
			break;
		case AARCH64_SIMD_FCMGE_FP16_SCALAR_PATTERN:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FCMGE;
			break;
		case AARCH64_SIMD_FACGE_FP16_SCALAR_PATTERN:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FACGE;
			break;
		case AARCH64_SIMD_FABD_FP16_SCALAR_PATTERN:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FABD;
			break;
		case AARCH64_SIMD_FCMGT_FP16_SCALAR_PATTERN:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FCMGT;
			break;
		default:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FACGT;
			break;
		}
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_FP16_THREE_SAME_MASK) ==
	    AARCH64_SIMD_FMAXNM_FP16_PATTERN ||
	    (instruction & AARCH64_SIMD_FP16_THREE_SAME_MASK) ==
	    AARCH64_SIMD_FMLA_FP16_PATTERN ||
	    (instruction & AARCH64_SIMD_FP16_THREE_SAME_MASK) ==
	    AARCH64_SIMD_FADD_FP16_PATTERN ||
	    (instruction & AARCH64_SIMD_FP16_THREE_SAME_MASK) ==
	    AARCH64_SIMD_FMULX_FP16_PATTERN ||
	    (instruction & AARCH64_SIMD_FP16_THREE_SAME_MASK) ==
	    AARCH64_SIMD_FCMEQ_FP16_PATTERN ||
	    (instruction & AARCH64_SIMD_FP16_THREE_SAME_MASK) ==
	    AARCH64_SIMD_FMAX_FP16_PATTERN ||
	    (instruction & AARCH64_SIMD_FP16_THREE_SAME_MASK) ==
	    AARCH64_SIMD_FRECPS_FP16_PATTERN ||
	    (instruction & AARCH64_SIMD_FP16_THREE_SAME_MASK) ==
	    AARCH64_SIMD_FMINNM_FP16_PATTERN ||
	    (instruction & AARCH64_SIMD_FP16_THREE_SAME_MASK) ==
	    AARCH64_SIMD_FMLS_FP16_PATTERN ||
	    (instruction & AARCH64_SIMD_FP16_THREE_SAME_MASK) ==
	    AARCH64_SIMD_FMIN_FP16_PATTERN ||
	    (instruction & AARCH64_SIMD_FP16_THREE_SAME_MASK) ==
	    AARCH64_SIMD_FRSQRTS_FP16_PATTERN ||
	    (instruction & AARCH64_SIMD_FP16_THREE_SAME_MASK) ==
	    AARCH64_SIMD_FMAXNMP_FP16_PATTERN ||
	    (instruction & AARCH64_SIMD_FP16_THREE_SAME_MASK) ==
	    AARCH64_SIMD_FADDP_FP16_PATTERN ||
	    (instruction & AARCH64_SIMD_FP16_THREE_SAME_MASK) ==
	    AARCH64_SIMD_FMUL_FP16_PATTERN ||
	    (instruction & AARCH64_SIMD_FP16_THREE_SAME_MASK) ==
	    AARCH64_SIMD_FCMGE_FP16_PATTERN ||
	    (instruction & AARCH64_SIMD_FP16_THREE_SAME_MASK) ==
	    AARCH64_SIMD_FACGE_FP16_PATTERN ||
	    (instruction & AARCH64_SIMD_FP16_THREE_SAME_MASK) ==
	    AARCH64_SIMD_FMAXP_FP16_PATTERN ||
	    (instruction & AARCH64_SIMD_FP16_THREE_SAME_MASK) ==
	    AARCH64_SIMD_FDIV_FP16_PATTERN ||
	    (instruction & AARCH64_SIMD_FP16_THREE_SAME_MASK) ==
	    AARCH64_SIMD_FMINNMP_FP16_PATTERN ||
	    (instruction & AARCH64_SIMD_FP16_THREE_SAME_MASK) ==
	    AARCH64_SIMD_FABD_FP16_PATTERN ||
	    (instruction & AARCH64_SIMD_FP16_THREE_SAME_MASK) ==
	    AARCH64_SIMD_FCMGT_FP16_PATTERN ||
	    (instruction & AARCH64_SIMD_FP16_THREE_SAME_MASK) ==
	    AARCH64_SIMD_FACGT_FP16_PATTERN ||
	    (instruction & AARCH64_SIMD_FP16_THREE_SAME_MASK) ==
	    AARCH64_SIMD_FMINP_FP16_PATTERN) {
		u32 pattern = instruction & AARCH64_SIMD_FP16_THREE_SAME_MASK;

		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = sizeof(u16);
		decoded.result_size = instruction & BIT(30) ?
			2 * sizeof(u64) : sizeof(u64);
		decoded.simd_fp = true;
		decoded.simd_q = !!(instruction & BIT(30));
		switch (pattern) {
		case AARCH64_SIMD_FMAXNM_FP16_PATTERN:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FMAXNM;
			break;
		case AARCH64_SIMD_FMLA_FP16_PATTERN:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FMLA;
			break;
		case AARCH64_SIMD_FADD_FP16_PATTERN:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FADD;
			break;
		case AARCH64_SIMD_FMULX_FP16_PATTERN:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FMULX;
			break;
		case AARCH64_SIMD_FCMEQ_FP16_PATTERN:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FCMEQ;
			break;
		case AARCH64_SIMD_FMAX_FP16_PATTERN:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FMAX;
			break;
		case AARCH64_SIMD_FRECPS_FP16_PATTERN:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FRECPS;
			break;
		case AARCH64_SIMD_FMINNM_FP16_PATTERN:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FMINNM;
			break;
		case AARCH64_SIMD_FMLS_FP16_PATTERN:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FMLS;
			break;
		case AARCH64_SIMD_FMIN_FP16_PATTERN:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FMIN;
			break;
		case AARCH64_SIMD_FRSQRTS_FP16_PATTERN:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FRSQRTS;
			break;
		case AARCH64_SIMD_FMAXNMP_FP16_PATTERN:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FMAXNMP;
			break;
		case AARCH64_SIMD_FADDP_FP16_PATTERN:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FADDP;
			break;
		case AARCH64_SIMD_FMUL_FP16_PATTERN:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FMUL;
			break;
		case AARCH64_SIMD_FCMGE_FP16_PATTERN:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FCMGE;
			break;
		case AARCH64_SIMD_FACGE_FP16_PATTERN:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FACGE;
			break;
		case AARCH64_SIMD_FMAXP_FP16_PATTERN:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FMAXP;
			break;
		case AARCH64_SIMD_FDIV_FP16_PATTERN:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FDIV;
			break;
		case AARCH64_SIMD_FMINNMP_FP16_PATTERN:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FMINNMP;
			break;
		case AARCH64_SIMD_FABD_FP16_PATTERN:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FABD;
			break;
		case AARCH64_SIMD_FCMGT_FP16_PATTERN:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FCMGT;
			break;
		case AARCH64_SIMD_FACGT_FP16_PATTERN:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FACGT;
			break;
		default:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FMINP;
			break;
		}
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_FP_THREE_SAME_MASK) ==
	    AARCH64_SIMD_FABD_PATTERN ||
	    (instruction & AARCH64_SIMD_FP_THREE_SAME_MASK) ==
	    AARCH64_SIMD_FACGE_PATTERN ||
	    (instruction & AARCH64_SIMD_FP_THREE_SAME_MASK) ==
	    AARCH64_SIMD_FACGT_PATTERN ||
	    (instruction & AARCH64_SIMD_FP_THREE_SAME_MASK) ==
	    AARCH64_SIMD_FCMEQ_PATTERN ||
	    (instruction & AARCH64_SIMD_FP_THREE_SAME_MASK) ==
	    AARCH64_SIMD_FCMGE_PATTERN ||
	    (instruction & AARCH64_SIMD_FP_THREE_SAME_MASK) ==
	    AARCH64_SIMD_FCMGT_PATTERN ||
	    (instruction & AARCH64_SIMD_FP_THREE_SAME_MASK) ==
	    AARCH64_SIMD_FMULX_PATTERN ||
	    (instruction & AARCH64_SIMD_FP_THREE_SAME_MASK) ==
	    AARCH64_SIMD_FRECPS_PATTERN ||
	    (instruction & AARCH64_SIMD_FP_THREE_SAME_MASK) ==
	    AARCH64_SIMD_FRSQRTS_PATTERN ||
	    (!(instruction & BIT(28)) &&
	     ((instruction & AARCH64_SIMD_FP_THREE_SAME_MASK) ==
	      AARCH64_SIMD_FADDP_PATTERN ||
	      (instruction & AARCH64_SIMD_FP_THREE_SAME_MASK) ==
	      AARCH64_SIMD_FADD_PATTERN ||
	      (instruction & AARCH64_SIMD_FP_THREE_SAME_MASK) ==
	      AARCH64_SIMD_FDIV_PATTERN ||
	      (instruction & AARCH64_SIMD_FP_THREE_SAME_MASK) ==
	      AARCH64_SIMD_FMAXNMP_PATTERN ||
	      (instruction & AARCH64_SIMD_FP_THREE_SAME_MASK) ==
	      AARCH64_SIMD_FMAXNM_PATTERN ||
	      (instruction & AARCH64_SIMD_FP_THREE_SAME_MASK) ==
	      AARCH64_SIMD_FMAXP_PATTERN ||
	      (instruction & AARCH64_SIMD_FP_THREE_SAME_MASK) ==
	      AARCH64_SIMD_FMAX_PATTERN ||
	      (instruction & AARCH64_SIMD_FP_THREE_SAME_MASK) ==
	      AARCH64_SIMD_FMINNMP_PATTERN ||
	      (instruction & AARCH64_SIMD_FP_THREE_SAME_MASK) ==
	      AARCH64_SIMD_FMINNM_PATTERN ||
	      (instruction & AARCH64_SIMD_FP_THREE_SAME_MASK) ==
	      AARCH64_SIMD_FMINP_PATTERN ||
	      (instruction & AARCH64_SIMD_FP_THREE_SAME_MASK) ==
	      AARCH64_SIMD_FMIN_PATTERN ||
	      (instruction & AARCH64_SIMD_FP_THREE_SAME_MASK) ==
	      AARCH64_SIMD_FMLA_PATTERN ||
	      (instruction & AARCH64_SIMD_FP_THREE_SAME_MASK) ==
	      AARCH64_SIMD_FMLS_PATTERN ||
	      ((instruction & AARCH64_SIMD_FP_THREE_SAME_MASK) ==
	       AARCH64_SIMD_FMUL_PATTERN &&
	       (instruction & (BIT(30) | BIT(22))) !=
	       (BIT(30) | BIT(22))) ||
	      (instruction & AARCH64_SIMD_FP_THREE_SAME_MASK) ==
	      AARCH64_SIMD_FSUB_PATTERN))) {
		u32 pattern = instruction & AARCH64_SIMD_FP_THREE_SAME_MASK;
		bool scalar = instruction & BIT(28);
		bool q = instruction & BIT(30);
		u8 access_size = instruction & BIT(22) ? sizeof(u64) : sizeof(u32);

		if ((scalar && !q) || (!scalar && !q && access_size == sizeof(u64)))
			return decoded;
		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = access_size;
		decoded.result_size = scalar ? access_size :
			q ? 2 * sizeof(u64) : sizeof(u64);
		decoded.simd_fp = true;
		decoded.simd_scalar = scalar;
		switch (pattern) {
		case AARCH64_SIMD_FABD_PATTERN:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FABD;
			break;
		case AARCH64_SIMD_FACGE_PATTERN:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FACGE;
			break;
		case AARCH64_SIMD_FACGT_PATTERN:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FACGT;
			break;
		case AARCH64_SIMD_FCMEQ_PATTERN:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FCMEQ;
			break;
		case AARCH64_SIMD_FCMGE_PATTERN:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FCMGE;
			break;
		case AARCH64_SIMD_FCMGT_PATTERN:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FCMGT;
			break;
		case AARCH64_SIMD_FMULX_PATTERN:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FMULX;
			break;
		case AARCH64_SIMD_FRECPS_PATTERN:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FRECPS;
			break;
		case AARCH64_SIMD_FRSQRTS_PATTERN:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FRSQRTS;
			break;
		case AARCH64_SIMD_FADDP_PATTERN:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FADDP;
			break;
		case AARCH64_SIMD_FADD_PATTERN:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FADD;
			break;
		case AARCH64_SIMD_FDIV_PATTERN:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FDIV;
			break;
		case AARCH64_SIMD_FMAXNMP_PATTERN:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FMAXNMP;
			break;
		case AARCH64_SIMD_FMAXNM_PATTERN:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FMAXNM;
			break;
		case AARCH64_SIMD_FMAXP_PATTERN:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FMAXP;
			break;
		case AARCH64_SIMD_FMAX_PATTERN:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FMAX;
			break;
		case AARCH64_SIMD_FMINNMP_PATTERN:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FMINNMP;
			break;
		case AARCH64_SIMD_FMINNM_PATTERN:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FMINNM;
			break;
		case AARCH64_SIMD_FMINP_PATTERN:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FMINP;
			break;
		case AARCH64_SIMD_FMIN_PATTERN:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FMIN;
			break;
		case AARCH64_SIMD_FMLA_PATTERN:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FMLA;
			break;
		case AARCH64_SIMD_FMLS_PATTERN:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FMLS;
			break;
		case AARCH64_SIMD_FMUL_PATTERN:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FMUL;
			break;
		default:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FSUB;
			break;
		}
		return decoded;
	}
	if ((instruction & AARCH64_SIMD_ADD_SUB_MASK) ==
	    AARCH64_SIMD_ADD_SUB_PATTERN ||
	    (instruction & AARCH64_SIMD_SCALAR_ADD_SUB_MASK) ==
	    AARCH64_SIMD_SCALAR_ADD_SUB_PATTERN) {
		u8 size = (instruction >> 22) & 0x3U;
		bool q = instruction & BIT(30);
		bool scalar = instruction & BIT(28);

		if ((scalar && size != 3) || (!scalar && !q && size == 3))
			return decoded;

		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = BIT(size);
		decoded.result_size = scalar ? sizeof(u64) :
			(q ? 2 * sizeof(u64) : sizeof(u64));
		decoded.simd_fp = true;
		decoded.simd_scalar = scalar;
		decoded.simd_arithmetic_op = instruction & BIT(29) ?
			ORLIX_TCTI_SIMD_ARITH_SUB : ORLIX_TCTI_SIMD_ARITH_ADD;
		return decoded;
	}
	if ((instruction & AARCH64_SIMD_HALVING_ADD_MASK) ==
	    AARCH64_SIMD_HALVING_ADD_PATTERN) {
		u8 size = (instruction >> 22) & 0x3U;
		u8 operation = (instruction & BIT(12) ? 2 : 0) |
			!!(instruction & BIT(29));
		bool q = instruction & BIT(30);

		if (size == 3)
			return decoded;

		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = BIT(size);
		decoded.result_size = q ? 2 * sizeof(u64) : sizeof(u64);
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_SHADD + operation;
		return decoded;
	}
	if ((instruction & AARCH64_SIMD_HALVING_SUB_MASK) ==
	    AARCH64_SIMD_HALVING_SUB_PATTERN) {
		u8 size = (instruction >> 22) & 0x3U;
		bool q = instruction & BIT(30);

		if (size == 3)
			return decoded;

		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = BIT(size);
		decoded.result_size = q ? 2 * sizeof(u64) : sizeof(u64);
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = instruction & BIT(29) ?
			ORLIX_TCTI_SIMD_ARITH_UHSUB : ORLIX_TCTI_SIMD_ARITH_SHSUB;
		return decoded;
	}
	if ((instruction & AARCH64_SIMD_PAIRWISE_ADD_MASK) ==
	    AARCH64_SIMD_PAIRWISE_ADD_PATTERN) {
		u8 size = (instruction >> 22) & 0x3U;
		bool q = instruction & BIT(30);

		if (!q && size == 3)
			return decoded;

		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = BIT(size);
		decoded.result_size = q ? 2 * sizeof(u64) : sizeof(u64);
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_ADDP;
		return decoded;
	}
	if ((instruction & AARCH64_SIMD_PAIRWISE_LONG_MASK) ==
	    AARCH64_SIMD_PAIRWISE_LONG_PATTERN ||
	    (instruction & AARCH64_SIMD_PAIRWISE_LONG_MASK) ==
	    AARCH64_SIMD_PAIRWISE_LONG_ACCUMULATE_PATTERN) {
		u8 size = (instruction >> 22) & 0x3U;
		bool q = instruction & BIT(30);
		bool u = instruction & BIT(29);
		bool accumulate =
			(instruction & AARCH64_SIMD_PAIRWISE_LONG_MASK) ==
			AARCH64_SIMD_PAIRWISE_LONG_ACCUMULATE_PATTERN;

		if (size == 3)
			return decoded;
		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = BIT(size);
		decoded.result_size = q ? 2 * sizeof(u64) : sizeof(u64);
		decoded.simd_fp = true;
		if (accumulate)
			decoded.simd_arithmetic_op = u ? ORLIX_TCTI_SIMD_ARITH_UADALP :
				ORLIX_TCTI_SIMD_ARITH_SADALP;
		else
			decoded.simd_arithmetic_op = u ? ORLIX_TCTI_SIMD_ARITH_UADDLP :
				ORLIX_TCTI_SIMD_ARITH_SADDLP;
		return decoded;
	}
	if ((instruction & AARCH64_SIMD_ADD_SUB_WIDE_MASK) ==
	    AARCH64_SIMD_ADD_SUB_WIDE_PATTERN) {
		u8 size = (instruction >> 22) & 0x3U;
		u8 operation = (instruction & BIT(13) ? 2 : 0) |
			!!(instruction & BIT(29));

		if (size == 3)
			return decoded;

		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = BIT(size);
		decoded.result_size = 2 * sizeof(u64);
		decoded.simd_source_index = instruction & BIT(30) ?
			sizeof(u64) / decoded.access_size : 0;
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_SADDW + operation;
		return decoded;
	}
	if ((instruction & AARCH64_SIMD_ADD_SUB_LONG_MASK) ==
	    AARCH64_SIMD_ADD_SUB_LONG_PATTERN) {
		u8 size = (instruction >> 22) & 0x3U;
		u8 operation = (instruction & BIT(13) ? 2 : 0) |
			!!(instruction & BIT(29));

		if (size == 3)
			return decoded;

		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = BIT(size);
		decoded.result_size = 2 * sizeof(u64);
		decoded.simd_source_index = instruction & BIT(30) ?
			sizeof(u64) / decoded.access_size : 0;
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_SADDL + operation;
		return decoded;
	}
	if ((instruction & AARCH64_SIMD_PAIRWISE_MIN_MAX_MASK) ==
	    AARCH64_SIMD_PAIRWISE_MIN_MAX_PATTERN) {
		u8 size = (instruction >> 22) & 0x3U;
		u8 operation = (instruction & BIT(11) ? 2 : 0) |
			!!(instruction & BIT(29));
		bool q = instruction & BIT(30);

		if (size == 3)
			return decoded;

		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = BIT(size);
		decoded.result_size = q ? 2 * sizeof(u64) : sizeof(u64);
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_SMAXP + operation;
		return decoded;
	}
	if ((instruction & AARCH64_SIMD_SATURATING_ADD_SUB_MASK) ==
	    AARCH64_SIMD_SATURATING_ADD_SUB_PATTERN ||
	    (instruction & AARCH64_SIMD_SCALAR_SATURATING_ADD_SUB_MASK) ==
	    AARCH64_SIMD_SCALAR_SATURATING_ADD_SUB_PATTERN) {
		u8 size = (instruction >> 22) & 0x3U;
		u8 operation = (instruction & BIT(13) ? 2 : 0) |
			!!(instruction & BIT(29));
		bool q = instruction & BIT(30);
		bool scalar = instruction & BIT(28);

		if (!q && size == 3)
			return decoded;

		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = BIT(size);
		decoded.result_size = scalar ? decoded.access_size :
			(q ? 2 * sizeof(u64) : sizeof(u64));
		decoded.simd_fp = true;
		decoded.simd_scalar = scalar;
		decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_SQADD + operation;
		return decoded;
	}
	if ((instruction & AARCH64_SIMD_SATURATING_MUL_HIGH_MASK) ==
	    AARCH64_SIMD_SATURATING_MUL_HIGH_PATTERN ||
	    (instruction & AARCH64_SIMD_SCALAR_SATURATING_MUL_HIGH_MASK) ==
	    AARCH64_SIMD_SCALAR_SATURATING_MUL_HIGH_PATTERN) {
		u8 size = (instruction >> 22) & 0x3U;
		bool q = instruction & BIT(30);
		bool scalar = instruction & BIT(28);

		if (size == 0 || size == 3)
			return decoded;

		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = BIT(size);
		decoded.result_size = scalar ? decoded.access_size :
			(q ? 2 * sizeof(u64) : sizeof(u64));
		decoded.simd_fp = true;
		decoded.simd_scalar = scalar;
		decoded.simd_arithmetic_op = instruction & BIT(29) ?
			ORLIX_TCTI_SIMD_ARITH_SQRDMULH : ORLIX_TCTI_SIMD_ARITH_SQDMULH;
		return decoded;
	}
	if ((instruction & AARCH64_SIMD_AES_MASK) == AARCH64_SIMD_AES_PATTERN) {
		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = 2 * sizeof(u64);
		decoded.result_size = 2 * sizeof(u64);
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_AESE +
					     ((instruction >> 12) & 0x3U);
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_SHA1_THREE_REGISTER_MASK) ==
	    AARCH64_SIMD_SHA1_THREE_REGISTER_PATTERN) {
		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = 2 * sizeof(u64);
		decoded.result_size = 2 * sizeof(u64);
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_SHA1C +
					     ((instruction >> 12) & 0x3U);
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_SHA1_TWO_REGISTER_MASK) ==
		    AARCH64_SIMD_SHA1H_PATTERN ||
	    (instruction & AARCH64_SIMD_SHA1_TWO_REGISTER_MASK) ==
		    AARCH64_SIMD_SHA1SU1_PATTERN) {
		bool schedule = (instruction &
				 AARCH64_SIMD_SHA1_TWO_REGISTER_MASK) ==
				AARCH64_SIMD_SHA1SU1_PATTERN;

		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = schedule ? 2 * sizeof(u64) : sizeof(u32);
		decoded.result_size = decoded.access_size;
		decoded.simd_fp = true;
		decoded.simd_scalar = !schedule;
		decoded.simd_arithmetic_op = schedule ? ORLIX_TCTI_SIMD_ARITH_SHA1SU1 :
						       ORLIX_TCTI_SIMD_ARITH_SHA1H;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_SHA256_THREE_REGISTER_MASK) ==
	    AARCH64_SIMD_SHA256_THREE_REGISTER_PATTERN) {
		u8 operation = (instruction >> 12) & 0x3U;

		if (operation == 3)
			return decoded;
		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = 2 * sizeof(u64);
		decoded.result_size = 2 * sizeof(u64);
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_SHA256H + operation;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_SHA256SU0_MASK) ==
	    AARCH64_SIMD_SHA256SU0_PATTERN) {
		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = 2 * sizeof(u64);
		decoded.result_size = 2 * sizeof(u64);
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_SHA256SU0;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_SHA512_THREE_REGISTER_MASK) ==
		    AARCH64_SIMD_SHA512H_PATTERN ||
	    (instruction & AARCH64_SIMD_SHA512_THREE_REGISTER_MASK) ==
		    AARCH64_SIMD_SHA512H2_PATTERN ||
	    (instruction & AARCH64_SIMD_SHA512_THREE_REGISTER_MASK) ==
		    AARCH64_SIMD_SHA512SU1_PATTERN) {
		u32 pattern = instruction &
			      AARCH64_SIMD_SHA512_THREE_REGISTER_MASK;
		u8 operation = pattern == AARCH64_SIMD_SHA512H_PATTERN ? 0 :
			       pattern == AARCH64_SIMD_SHA512H2_PATTERN ? 1 : 2;

		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = 2 * sizeof(u64);
		decoded.result_size = 2 * sizeof(u64);
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_SHA512H + operation;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_SHA512SU0_MASK) ==
	    AARCH64_SIMD_SHA512SU0_PATTERN) {
		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = 2 * sizeof(u64);
		decoded.result_size = 2 * sizeof(u64);
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_SHA512SU0;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_EOR3_MASK) ==
	    AARCH64_SIMD_EOR3_PATTERN) {
		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.ra = (instruction >> 10) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = 2 * sizeof(u64);
		decoded.result_size = 2 * sizeof(u64);
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_EOR3;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_RAX1_MASK) ==
	    AARCH64_SIMD_RAX1_PATTERN) {
		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = 2 * sizeof(u64);
		decoded.result_size = 2 * sizeof(u64);
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_RAX1;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_XAR_MASK) ==
	    AARCH64_SIMD_XAR_PATTERN) {
		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.shift_amount = (instruction >> 10) & 0x3fU;
		decoded.access_size = 2 * sizeof(u64);
		decoded.result_size = 2 * sizeof(u64);
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_XAR;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_BCAX_MASK) ==
	    AARCH64_SIMD_BCAX_PATTERN) {
		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.ra = (instruction >> 10) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = 2 * sizeof(u64);
		decoded.result_size = 2 * sizeof(u64);
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_BCAX;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_SM4E_MASK) ==
		    AARCH64_SIMD_SM4E_PATTERN ||
	    (instruction & AARCH64_SIMD_SM4EKEY_MASK) ==
		    AARCH64_SIMD_SM4EKEY_PATTERN) {
		bool key = (instruction & AARCH64_SIMD_SM4EKEY_MASK) ==
			   AARCH64_SIMD_SM4EKEY_PATTERN;

		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		if (key)
			decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = 2 * sizeof(u64);
		decoded.result_size = 2 * sizeof(u64);
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = key ? ORLIX_TCTI_SIMD_ARITH_SM4EKEY :
						   ORLIX_TCTI_SIMD_ARITH_SM4E;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_SM3SS1_MASK) ==
	    AARCH64_SIMD_SM3SS1_PATTERN) {
		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.ra = (instruction >> 10) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = 2 * sizeof(u64);
		decoded.result_size = 2 * sizeof(u64);
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_SM3SS1;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_SM3TT_MASK) ==
		    AARCH64_SIMD_SM3TT1A_PATTERN ||
	    (instruction & AARCH64_SIMD_SM3TT_MASK) ==
		    AARCH64_SIMD_SM3TT1B_PATTERN ||
	    (instruction & AARCH64_SIMD_SM3TT_MASK) ==
		    AARCH64_SIMD_SM3TT2A_PATTERN ||
	    (instruction & AARCH64_SIMD_SM3TT_MASK) ==
		    AARCH64_SIMD_SM3TT2B_PATTERN) {
		u32 pattern = instruction & AARCH64_SIMD_SM3TT_MASK;
		u8 operation = pattern == AARCH64_SIMD_SM3TT1A_PATTERN ? 0 :
			       pattern == AARCH64_SIMD_SM3TT1B_PATTERN ? 1 :
			       pattern == AARCH64_SIMD_SM3TT2A_PATTERN ? 2 : 3;

		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.shift_amount = (instruction >> 12) & 0x3U;
		decoded.access_size = 2 * sizeof(u64);
		decoded.result_size = 2 * sizeof(u64);
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_SM3TT1A +
					     operation;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_SM3PARTW_MASK) ==
		    AARCH64_SIMD_SM3PARTW1_PATTERN ||
	    (instruction & AARCH64_SIMD_SM3PARTW_MASK) ==
		    AARCH64_SIMD_SM3PARTW2_PATTERN) {
		bool second = (instruction & AARCH64_SIMD_SM3PARTW_MASK) ==
			      AARCH64_SIMD_SM3PARTW2_PATTERN;

		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = 2 * sizeof(u64);
		decoded.result_size = 2 * sizeof(u64);
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = second ? ORLIX_TCTI_SIMD_ARITH_SM3PARTW2 :
						      ORLIX_TCTI_SIMD_ARITH_SM3PARTW1;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_SCALAR_SQDM_LONG_MASK) ==
		    AARCH64_SIMD_SCALAR_SQDMLAL_PATTERN ||
	    (instruction & AARCH64_SIMD_SCALAR_SQDM_LONG_MASK) ==
		    AARCH64_SIMD_SCALAR_SQDMLSL_PATTERN ||
	    (instruction & AARCH64_SIMD_SCALAR_SQDM_LONG_MASK) ==
		    AARCH64_SIMD_SCALAR_SQDMULL_PATTERN) {
		u8 size = (instruction >> 22) & 0x3U;
		u32 pattern = instruction & AARCH64_SIMD_SCALAR_SQDM_LONG_MASK;

		if (size != 1 && size != 2)
			return decoded;
		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = BIT(size);
		decoded.result_size = 2 * decoded.access_size;
		decoded.simd_fp = true;
		decoded.simd_scalar = true;
		if (pattern == AARCH64_SIMD_SCALAR_SQDMLAL_PATTERN)
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_SQDMLAL;
		else if (pattern == AARCH64_SIMD_SCALAR_SQDMLSL_PATTERN)
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_SQDMLSL;
		else
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_SQDMULL;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_VECTOR_SQDM_LONG_MASK) ==
		    AARCH64_SIMD_VECTOR_SQDMLAL_PATTERN ||
	    (instruction & AARCH64_SIMD_VECTOR_SQDM_LONG_MASK) ==
		    AARCH64_SIMD_VECTOR_SQDMLSL_PATTERN ||
	    (instruction & AARCH64_SIMD_VECTOR_SQDM_LONG_MASK) ==
		    AARCH64_SIMD_VECTOR_SQDMULL_PATTERN) {
		u8 size = (instruction >> 22) & 0x3U;
		u32 pattern = instruction & AARCH64_SIMD_VECTOR_SQDM_LONG_MASK;

		if (size != 1 && size != 2)
			return decoded;

		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = BIT(size);
		decoded.result_size = 2 * sizeof(u64);
		decoded.simd_source_index = !!(instruction & BIT(30));
		decoded.simd_fp = true;
		if (pattern == AARCH64_SIMD_VECTOR_SQDMLAL_PATTERN)
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_SQDMLAL;
		else if (pattern == AARCH64_SIMD_VECTOR_SQDMLSL_PATTERN)
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_SQDMLSL;
		else
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_SQDMULL;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_MULTIPLY_LONG_MASK) ==
		    AARCH64_SIMD_MLAL_PATTERN ||
	    (instruction & AARCH64_SIMD_MULTIPLY_LONG_MASK) ==
		    AARCH64_SIMD_MLSL_PATTERN ||
	    (instruction & AARCH64_SIMD_MULTIPLY_LONG_MASK) ==
		    AARCH64_SIMD_MULL_PATTERN) {
		u8 size = (instruction >> 22) & 0x3U;
		u8 operation = ((instruction &
				 AARCH64_SIMD_MULTIPLY_LONG_MASK) ==
				AARCH64_SIMD_MLAL_PATTERN ? 2 :
			       (instruction &
				AARCH64_SIMD_MULTIPLY_LONG_MASK) ==
				AARCH64_SIMD_MLSL_PATTERN ? 4 : 0) |
			      !!(instruction & BIT(29));

		if (size == 3)
			return decoded;

		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = BIT(size);
		decoded.result_size = 2 * sizeof(u64);
		decoded.simd_source_index = !!(instruction & BIT(30));
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_SMULL + operation;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_PMULL_MASK) ==
	    AARCH64_SIMD_PMULL_PATTERN) {
		u8 size = (instruction >> 22) & 0x3U;

		if (size != 0 && size != 3)
			return decoded;

		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = BIT(size);
		decoded.result_size = 2 * sizeof(u64);
		decoded.simd_source_index = !!(instruction & BIT(30));
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_PMULL;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_MUL_PMUL_MASK) ==
	    AARCH64_SIMD_MUL_PMUL_PATTERN) {
		u8 size = (instruction >> 22) & 0x3U;
		bool polynomial = instruction & BIT(29);
		bool q = instruction & BIT(30);

		if ((polynomial && size != 0) || (!polynomial && size == 3))
			return decoded;

		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = BIT(size);
		decoded.result_size = q ? 2 * sizeof(u64) : sizeof(u64);
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = polynomial ?
			ORLIX_TCTI_SIMD_ARITH_PMUL : ORLIX_TCTI_SIMD_ARITH_MUL;
		return decoded;
	}
	if ((instruction & AARCH64_SIMD_MLA_MLS_MASK) ==
	    AARCH64_SIMD_MLA_MLS_PATTERN) {
		u8 size = (instruction >> 22) & 0x3U;
		bool q = instruction & BIT(30);

		if (size == 3)
			return decoded;

		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = BIT(size);
		decoded.result_size = q ? 2 * sizeof(u64) : sizeof(u64);
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = instruction & BIT(29) ?
			ORLIX_TCTI_SIMD_ARITH_MLS : ORLIX_TCTI_SIMD_ARITH_MLA;
		return decoded;
	}
	if ((instruction & AARCH64_SIMD_MIN_MAX_MASK) ==
	    AARCH64_SIMD_MIN_MAX_PATTERN) {
		u8 size = (instruction >> 22) & 0x3U;
		u8 operation = (instruction & BIT(11) ? 2 : 0) |
			!!(instruction & BIT(29));
		bool q = instruction & BIT(30);

		if (size == 3)
			return decoded;

		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = BIT(size);
		decoded.result_size = q ? 2 * sizeof(u64) : sizeof(u64);
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_SMAX + operation;
		return decoded;
	}
	if ((instruction & AARCH64_SIMD_ABSOLUTE_DIFFERENCE_LONG_MASK) ==
	    AARCH64_SIMD_ABSOLUTE_DIFFERENCE_LONG_PATTERN ||
	    (instruction & AARCH64_SIMD_ABSOLUTE_DIFFERENCE_LONG_MASK) ==
	    AARCH64_SIMD_ABSOLUTE_DIFFERENCE_ACCUMULATE_LONG_PATTERN) {
		u8 size = (instruction >> 22) & 0x3U;
		u8 operation =
			((instruction & AARCH64_SIMD_ABSOLUTE_DIFFERENCE_LONG_MASK) ==
			 AARCH64_SIMD_ABSOLUTE_DIFFERENCE_ACCUMULATE_LONG_PATTERN ?
			 2 : 0) | !!(instruction & BIT(29));

		if (size == 3)
			return decoded;

		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = BIT(size);
		decoded.result_size = 2 * sizeof(u64);
		decoded.simd_source_index = instruction & BIT(30) ?
			sizeof(u64) / decoded.access_size : 0;
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_SABDL + operation;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_ADD_SUB_NARROW_HIGH_MASK) ==
	    AARCH64_SIMD_ADD_SUB_NARROW_HIGH_PATTERN) {
		u8 size = (instruction >> 22) & 0x3U;
		u8 operation = (instruction & BIT(29) ? 2 : 0) |
			!!(instruction & BIT(13));
		bool q = instruction & BIT(30);

		if (size == 3)
			return decoded;

		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = 2U << size;
		decoded.result_size = q ? 2 * sizeof(u64) : sizeof(u64);
		decoded.simd_destination_index = q;
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_ADDHN + operation;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_SHIFT_NARROW_MASK) ==
		    AARCH64_SIMD_SHRN_PATTERN ||
	    (instruction & AARCH64_SIMD_SHIFT_NARROW_MASK) ==
	    AARCH64_SIMD_RSHRN_PATTERN ||
	    (instruction & AARCH64_SIMD_SHIFT_NARROW_MASK) ==
	    AARCH64_SIMD_SQSHRN_PATTERN ||
	    (instruction & AARCH64_SIMD_SHIFT_NARROW_MASK) ==
		    AARCH64_SIMD_SQRSHRN_PATTERN ||
	    (instruction & AARCH64_SIMD_SCALAR_SHIFT_NARROW_MASK) ==
		    AARCH64_SIMD_SCALAR_SHRN_PATTERN ||
	    (instruction & AARCH64_SIMD_SCALAR_SHIFT_NARROW_MASK) ==
		    AARCH64_SIMD_SCALAR_RSHRN_PATTERN ||
	    (instruction & AARCH64_SIMD_SCALAR_SHIFT_NARROW_MASK) ==
		    AARCH64_SIMD_SCALAR_SQSHRN_PATTERN ||
	    (instruction & AARCH64_SIMD_SCALAR_SHIFT_NARROW_MASK) ==
		    AARCH64_SIMD_SCALAR_SQRSHRN_PATTERN) {
		u8 immediate = (instruction >> 16) & 0x7fU;
		u8 source_bits;
		bool scalar = (instruction &
			       AARCH64_SIMD_SCALAR_SHIFT_NARROW_MASK) ==
			      AARCH64_SIMD_SCALAR_SHRN_PATTERN ||
			      (instruction &
			       AARCH64_SIMD_SCALAR_SHIFT_NARROW_MASK) ==
			      AARCH64_SIMD_SCALAR_RSHRN_PATTERN ||
			      (instruction &
			       AARCH64_SIMD_SCALAR_SHIFT_NARROW_MASK) ==
			      AARCH64_SIMD_SCALAR_SQSHRN_PATTERN ||
			      (instruction &
			       AARCH64_SIMD_SCALAR_SHIFT_NARROW_MASK) ==
			      AARCH64_SIMD_SCALAR_SQRSHRN_PATTERN;
		u32 pattern = instruction & AARCH64_SIMD_SHIFT_NARROW_MASK;
		bool u = instruction & BIT(29);
		bool q = instruction & BIT(30);

		if (immediate < 8 || immediate >= 64)
			return decoded;
		if (immediate < 16)
			source_bits = 16;
		else if (immediate < 32)
			source_bits = 32;
		else
			source_bits = 64;
		if (scalar)
			pattern &= ~(BIT(28) | BIT(30));
		if (scalar && !u &&
		    (pattern == AARCH64_SIMD_SHRN_PATTERN ||
		     pattern == AARCH64_SIMD_RSHRN_PATTERN))
			return decoded;

		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = source_bits / 8;
		decoded.result_size = scalar ? source_bits / 16 :
					       (q ? 2 * sizeof(u64) : sizeof(u64));
		decoded.simd_destination_index = scalar ? false : q;
		decoded.shift_amount = source_bits - immediate;
		decoded.simd_fp = true;
		decoded.simd_scalar = scalar;
		if (pattern == AARCH64_SIMD_SHRN_PATTERN)
			decoded.simd_arithmetic_op = u ? ORLIX_TCTI_SIMD_ARITH_SQSHRUN :
				ORLIX_TCTI_SIMD_ARITH_SHRN;
		else if (pattern == AARCH64_SIMD_RSHRN_PATTERN)
			decoded.simd_arithmetic_op = u ? ORLIX_TCTI_SIMD_ARITH_SQRSHRUN :
				ORLIX_TCTI_SIMD_ARITH_RSHRN;
		else if (pattern == AARCH64_SIMD_SQSHRN_PATTERN)
			decoded.simd_arithmetic_op = u ? ORLIX_TCTI_SIMD_ARITH_UQSHRN :
				ORLIX_TCTI_SIMD_ARITH_SQSHRN;
		else
			decoded.simd_arithmetic_op = u ? ORLIX_TCTI_SIMD_ARITH_UQRSHRN :
				ORLIX_TCTI_SIMD_ARITH_SQRSHRN;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_SATURATING_NARROW_MASK) ==
	    AARCH64_SIMD_SQXTN_UQXTN_PATTERN ||
	    (instruction & AARCH64_SIMD_SATURATING_NARROW_MASK) ==
	    AARCH64_SIMD_SQXTUN_PATTERN ||
	    (instruction & AARCH64_SIMD_SCALAR_SATURATING_NARROW_MASK) ==
	    AARCH64_SIMD_SCALAR_SQXTN_UQXTN_PATTERN ||
	    (instruction & AARCH64_SIMD_SCALAR_SATURATING_NARROW_MASK) ==
	    AARCH64_SIMD_SCALAR_SQXTUN_PATTERN) {
		u8 size = (instruction >> 22) & 0x3U;
		bool q = instruction & BIT(30);
		bool scalar = instruction & BIT(28);
		u32 pattern = instruction & (scalar ?
			AARCH64_SIMD_SCALAR_SATURATING_NARROW_MASK :
			AARCH64_SIMD_SATURATING_NARROW_MASK);
		u32 sqxtun_pattern = scalar ? AARCH64_SIMD_SCALAR_SQXTUN_PATTERN :
			AARCH64_SIMD_SQXTUN_PATTERN;

		if (size == 3 ||
		    (pattern == sqxtun_pattern &&
		     !(instruction & BIT(29))))
			return decoded;

		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = 2U << size;
		decoded.result_size = scalar ? decoded.access_size / 2 :
			(q ? 2 * sizeof(u64) : sizeof(u64));
		decoded.simd_destination_index = scalar ? 0 : q;
		decoded.simd_fp = true;
		decoded.simd_scalar = scalar;
		if (pattern == sqxtun_pattern)
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_SQXTUN;
		else
			decoded.simd_arithmetic_op = instruction & BIT(29) ?
				ORLIX_TCTI_SIMD_ARITH_UQXTN : ORLIX_TCTI_SIMD_ARITH_SQXTN;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_ABSOLUTE_DIFFERENCE_MASK) ==
	    AARCH64_SIMD_ABSOLUTE_DIFFERENCE_PATTERN) {
		u8 size = (instruction >> 22) & 0x3U;
		u8 operation = (instruction & BIT(11) ? 2 : 0) |
			!!(instruction & BIT(29));
		bool q = instruction & BIT(30);

		if (size == 3)
			return decoded;

		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = BIT(size);
		decoded.result_size = q ? 2 * sizeof(u64) : sizeof(u64);
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_SABD + operation;
		return decoded;
	}
	if ((instruction & AARCH64_SIMD_SCALAR_TWO_REGISTER_MISC_MASK) ==
	    AARCH64_SIMD_ABS_NEG_PATTERN ||
	    (instruction & AARCH64_SIMD_SCALAR_TWO_REGISTER_MISC_MASK) ==
	    AARCH64_SIMD_SQABS_SQNEG_PATTERN ||
	    (instruction & AARCH64_SIMD_SCALAR_TWO_REGISTER_MISC_MASK) ==
		    AARCH64_SIMD_SUQADD_USQADD_PATTERN ||
	    (instruction & AARCH64_SIMD_SCALAR_TWO_REGISTER_MISC_MASK) ==
	    AARCH64_SIMD_CLS_CLZ_PATTERN) {
		u8 size = (instruction >> 22) & 0x3U;
		bool q = instruction & BIT(30);
		bool u = instruction & BIT(29);
		bool scalar = instruction & BIT(28);
		u32 pattern = instruction &
			AARCH64_SIMD_SCALAR_TWO_REGISTER_MISC_MASK;

		if (scalar) {
			if (!q || pattern == AARCH64_SIMD_CLS_CLZ_PATTERN ||
			    (pattern == AARCH64_SIMD_ABS_NEG_PATTERN && size != 3)) {
				if ((instruction & AARCH64_FP_SCALAR_2SOURCE_MASK) ==
				    AARCH64_FP_SCALAR_2SOURCE_PATTERN)
					goto simd_two_register_misc_unclaimed;
				return decoded;
			}
		} else if ((!q && size == 3) ||
			   (pattern == AARCH64_SIMD_CLS_CLZ_PATTERN && size == 3)) {
			return decoded;
		}

		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = BIT(size);
		decoded.result_size = scalar ? decoded.access_size :
			(q ? 2 * sizeof(u64) : sizeof(u64));
		decoded.simd_fp = true;
		decoded.simd_scalar = scalar;
		if (pattern == AARCH64_SIMD_ABS_NEG_PATTERN)
			decoded.simd_arithmetic_op = u ? ORLIX_TCTI_SIMD_ARITH_NEG :
				ORLIX_TCTI_SIMD_ARITH_ABS;
		else if (pattern == AARCH64_SIMD_SQABS_SQNEG_PATTERN)
			decoded.simd_arithmetic_op = u ? ORLIX_TCTI_SIMD_ARITH_SQNEG :
				ORLIX_TCTI_SIMD_ARITH_SQABS;
		else if (pattern == AARCH64_SIMD_SUQADD_USQADD_PATTERN)
			decoded.simd_arithmetic_op = u ? ORLIX_TCTI_SIMD_ARITH_USQADD :
				ORLIX_TCTI_SIMD_ARITH_SUQADD;
		else
			decoded.simd_arithmetic_op = u ? ORLIX_TCTI_SIMD_ARITH_CLZ :
				ORLIX_TCTI_SIMD_ARITH_CLS;
		return decoded;
	}

simd_two_register_misc_unclaimed:

	if ((instruction & AARCH64_SIMD_TWO_REGISTER_MISC_MASK) ==
	    AARCH64_SIMD_REV_PATTERN) {
		u8 size = (instruction >> 22) & 0x3U;
		bool u = instruction & BIT(29);

		if ((!u && size == 3) || (u && size > 1))
			return decoded;

		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = BIT(size);
		decoded.result_size = instruction & BIT(30) ?
			2 * sizeof(u64) : sizeof(u64);
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = u ? ORLIX_TCTI_SIMD_ARITH_REV32 :
			ORLIX_TCTI_SIMD_ARITH_REV64;
		return decoded;
	}

	if ((instruction & ~BIT(30) & 0xfffffc00U) ==
	    AARCH64_SIMD_REV16_PATTERN) {
		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = sizeof(u8);
		decoded.result_size = instruction & BIT(30) ?
			2 * sizeof(u64) : sizeof(u64);
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_REV16;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_TWO_REGISTER_MISC_MASK) ==
	    AARCH64_SIMD_BIT_COUNT_PATTERN) {
		u8 size = (instruction >> 22) & 0x3U;
		bool u = instruction & BIT(29);

		if ((!u && size != 0) || (u && size > 1))
			return decoded;

		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = sizeof(u8);
		decoded.result_size = instruction & BIT(30) ?
			2 * sizeof(u64) : sizeof(u64);
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = !u ? ORLIX_TCTI_SIMD_ARITH_CNT :
			size == 0 ? ORLIX_TCTI_SIMD_ARITH_NOT : ORLIX_TCTI_SIMD_ARITH_RBIT;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_FCVT_LONG_NARROW_MASK) ==
	    AARCH64_SIMD_FCVTN_VECTOR_PATTERN ||
	    (instruction & AARCH64_SIMD_FCVT_LONG_NARROW_MASK) ==
	    AARCH64_SIMD_FCVTL_VECTOR_PATTERN) {
		bool narrow = (instruction & AARCH64_SIMD_FCVT_LONG_NARROW_MASK) ==
			AARCH64_SIMD_FCVTN_VECTOR_PATTERN;

		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = narrow ? sizeof(u64) : sizeof(u32);
		decoded.result_size = narrow && !(instruction & BIT(30)) ?
			sizeof(u64) : 2 * sizeof(u64);
		decoded.simd_source_index = narrow ? 0 : !!(instruction & BIT(30));
		decoded.simd_destination_index = narrow && !!(instruction & BIT(30));
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = narrow ? ORLIX_TCTI_SIMD_ARITH_FCVTN :
			ORLIX_TCTI_SIMD_ARITH_FCVTL;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_FCVTXN_VECTOR_MASK) ==
	    AARCH64_SIMD_FCVTXN_VECTOR_PATTERN) {
		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = sizeof(u64);
		decoded.result_size = instruction & BIT(30) ?
			2 * sizeof(u64) : sizeof(u64);
		decoded.simd_destination_index = !!(instruction & BIT(30));
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FCVTXN;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_UINT_ESTIMATE_MASK) ==
	    AARCH64_SIMD_URECPE_VECTOR_PATTERN ||
	    (instruction & AARCH64_SIMD_UINT_ESTIMATE_MASK) ==
	    AARCH64_SIMD_URSQRTE_VECTOR_PATTERN) {
		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = sizeof(u32);
		decoded.result_size = instruction & BIT(30) ?
			2 * sizeof(u64) : sizeof(u64);
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = instruction & BIT(29) ?
			ORLIX_TCTI_SIMD_ARITH_URSQRTE : ORLIX_TCTI_SIMD_ARITH_URECPE;
		return decoded;
	}

	{
		static const struct {
			u32 pattern;
			enum orlix_tcti_simd_vector_arithmetic_op operation;
		} operations[] = {
			{ AARCH64_SIMD_FABS_VECTOR_PATTERN, ORLIX_TCTI_SIMD_ARITH_FABS },
			{ AARCH64_SIMD_FNEG_VECTOR_PATTERN, ORLIX_TCTI_SIMD_ARITH_FNEG },
			{ AARCH64_SIMD_FSQRT_VECTOR_PATTERN, ORLIX_TCTI_SIMD_ARITH_FSQRT },
			{ AARCH64_SIMD_FRECPE_VECTOR_PATTERN, ORLIX_TCTI_SIMD_ARITH_FRECPE },
			{ AARCH64_SIMD_FRSQRTE_VECTOR_PATTERN, ORLIX_TCTI_SIMD_ARITH_FRSQRTE },
			{ AARCH64_SIMD_FRINTN_VECTOR_PATTERN, ORLIX_TCTI_SIMD_ARITH_FRINTN },
			{ AARCH64_SIMD_FRINTP_VECTOR_PATTERN, ORLIX_TCTI_SIMD_ARITH_FRINTP },
			{ AARCH64_SIMD_FRINTM_VECTOR_PATTERN, ORLIX_TCTI_SIMD_ARITH_FRINTM },
			{ AARCH64_SIMD_FRINTZ_VECTOR_PATTERN, ORLIX_TCTI_SIMD_ARITH_FRINTZ },
			{ AARCH64_SIMD_FRINTA_VECTOR_PATTERN, ORLIX_TCTI_SIMD_ARITH_FRINTA },
			{ AARCH64_SIMD_FRINTX_VECTOR_PATTERN, ORLIX_TCTI_SIMD_ARITH_FRINTX },
			{ AARCH64_SIMD_FRINTI_VECTOR_PATTERN, ORLIX_TCTI_SIMD_ARITH_FRINTI },
			{ AARCH64_SIMD_FCMEQ_ZERO_VECTOR_PATTERN,
			  ORLIX_TCTI_SIMD_ARITH_FCMEQ_ZERO },
			{ AARCH64_SIMD_FCMGE_ZERO_VECTOR_PATTERN,
			  ORLIX_TCTI_SIMD_ARITH_FCMGE_ZERO },
			{ AARCH64_SIMD_FCMGT_ZERO_VECTOR_PATTERN,
			  ORLIX_TCTI_SIMD_ARITH_FCMGT_ZERO },
			{ AARCH64_SIMD_FCMLE_ZERO_VECTOR_PATTERN,
			  ORLIX_TCTI_SIMD_ARITH_FCMLE_ZERO },
			{ AARCH64_SIMD_FCMLT_ZERO_VECTOR_PATTERN,
			  ORLIX_TCTI_SIMD_ARITH_FCMLT_ZERO },
			{ AARCH64_SIMD_FCMEQ_ZERO_VECTOR_PATTERN | BIT(28),
			  ORLIX_TCTI_SIMD_ARITH_FCMEQ_ZERO },
			{ AARCH64_SIMD_FCMGE_ZERO_VECTOR_PATTERN | BIT(28),
			  ORLIX_TCTI_SIMD_ARITH_FCMGE_ZERO },
			{ AARCH64_SIMD_FCMGT_ZERO_VECTOR_PATTERN | BIT(28),
			  ORLIX_TCTI_SIMD_ARITH_FCMGT_ZERO },
			{ AARCH64_SIMD_FCMLE_ZERO_VECTOR_PATTERN | BIT(28),
			  ORLIX_TCTI_SIMD_ARITH_FCMLE_ZERO },
			{ AARCH64_SIMD_FCMLT_ZERO_VECTOR_PATTERN | BIT(28),
			  ORLIX_TCTI_SIMD_ARITH_FCMLT_ZERO },
		};
		u32 pattern = instruction & AARCH64_SIMD_FP_TWO_REGISTER_MASK;
		size_t index;

		for (index = 0; index < ARRAY_SIZE(operations); index++) {
			if (pattern != operations[index].pattern)
				continue;
			if ((instruction & BIT(28)) &&
			    (!(instruction & BIT(30)) ||
			     ((instruction >> 22) & 0x3U) < 2))
				return decoded;
			if (!(instruction & BIT(28)) &&
			    (instruction & BIT(22)) && !(instruction & BIT(30)))
				return decoded;
			decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
			decoded.rd = instruction & 0x1fU;
			decoded.rn = (instruction >> 5) & 0x1fU;
			decoded.access_size = instruction & BIT(22) ? sizeof(u64) :
				sizeof(u32);
			decoded.simd_fp = true;
			decoded.simd_scalar = !!(instruction & BIT(28));
			decoded.result_size = decoded.simd_scalar ?
				decoded.access_size :
				instruction & BIT(30) ?
					2 * sizeof(u64) : sizeof(u64);
			decoded.simd_arithmetic_op = operations[index].operation;
			return decoded;
		}
	}

	if ((instruction & AARCH64_SIMD_FNEG_2D_MASK) ==
	    AARCH64_SIMD_FNEG_2D_PATTERN) {
		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1f;
		decoded.rn = (instruction >> 5) & 0x1f;
		decoded.access_size = sizeof(u64);
		decoded.result_size = 2 * sizeof(u64);
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FNEG;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_THREE_SAME_MASK) ==
	    AARCH64_SIMD_THREE_SAME_PATTERN ||
	    (instruction & AARCH64_SIMD_THREE_SAME_MASK) ==
	    AARCH64_SIMD_SCALAR_THREE_SAME_PATTERN) {
		u8 opcode = (instruction >> 11) & 0x1fU;
		u8 size = (instruction >> 22) & 0x3U;
		bool q = instruction & BIT(30);
		bool u = instruction & BIT(29);
		bool scalar = (instruction & AARCH64_SIMD_THREE_SAME_MASK) ==
			AARCH64_SIMD_SCALAR_THREE_SAME_PATTERN;
		enum orlix_tcti_simd_vector_compare_op operation;

		if (opcode == 6)
			operation = u ? ORLIX_TCTI_SIMD_COMPARE_CMHI :
				ORLIX_TCTI_SIMD_COMPARE_CMGT;
		else if (opcode == 7)
			operation = u ? ORLIX_TCTI_SIMD_COMPARE_CMHS :
				ORLIX_TCTI_SIMD_COMPARE_CMGE;
		else if (opcode == 17)
			operation = u ? ORLIX_TCTI_SIMD_COMPARE_CMEQ :
				ORLIX_TCTI_SIMD_COMPARE_CMTST;
		else
			goto not_simd_compare_register;

		if (scalar && (!q || size != 3))
			goto not_simd_compare_register;
		if (!scalar && !q && size == 3)
			return decoded;

		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_COMPARE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = BIT(size);
		decoded.result_size = scalar ? sizeof(u64) :
			(q ? 2 * sizeof(u64) : sizeof(u64));
		decoded.simd_fp = true;
		decoded.simd_scalar = scalar;
		decoded.simd_compare_op = operation;
		return decoded;
	}
not_simd_compare_register:
	if ((instruction & AARCH64_SIMD_CMEQ_MASK) ==
	    AARCH64_SIMD_CMEQ_PATTERN) {
		u8 size = (instruction >> 22) & 0x3U;

		if (size > 2)
			return decoded;

		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_COMPARE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = BIT(size);
		decoded.result_size = (instruction & BIT(30)) ?
				      2 * sizeof(u64) : sizeof(u64);
		decoded.simd_fp = true;
		decoded.simd_compare_op = ORLIX_TCTI_SIMD_COMPARE_CMEQ;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_CMEQ_4S_MASK) ==
	    AARCH64_SIMD_CMEQ_4S_PATTERN) {
		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_COMPARE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = sizeof(u32);
		decoded.result_size = 2 * sizeof(u64);
		decoded.simd_fp = true;
		decoded.simd_compare_op = ORLIX_TCTI_SIMD_COMPARE_CMEQ;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_SCALAR_TWO_REGISTER_MISC_MASK) ==
	    AARCH64_SIMD_CMGT_ZERO_PATTERN ||
	    (instruction & AARCH64_SIMD_SCALAR_TWO_REGISTER_MISC_MASK) ==
	    AARCH64_SIMD_CMEQ_ZERO_PATTERN ||
	    (instruction & AARCH64_SIMD_SCALAR_TWO_REGISTER_MISC_MASK) ==
	    AARCH64_SIMD_CMLT_ZERO_PATTERN) {
		u32 pattern = instruction &
			AARCH64_SIMD_SCALAR_TWO_REGISTER_MISC_MASK;
		u8 size = (instruction >> 22) & 0x3U;
		bool q = instruction & BIT(30);
		bool u = instruction & BIT(29);
		bool scalar = instruction & BIT(28);

		if ((scalar && (!q || size != 3)) ||
		    (!scalar && !q && size == 3) ||
		    (pattern == AARCH64_SIMD_CMLT_ZERO_PATTERN && u)) {
			if ((instruction & AARCH64_FP_SCALAR_2SOURCE_MASK) ==
			    AARCH64_FP_SCALAR_2SOURCE_PATTERN)
				goto simd_compare_zero_unclaimed;
			return decoded;
		}

		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_COMPARE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = BIT(size);
		decoded.result_size = scalar ? sizeof(u64) :
			(q ? 2 * sizeof(u64) : sizeof(u64));
		decoded.simd_fp = true;
		decoded.simd_scalar = scalar;
		decoded.immediate = true;
		if (pattern == AARCH64_SIMD_CMGT_ZERO_PATTERN)
			decoded.simd_compare_op = u ? ORLIX_TCTI_SIMD_COMPARE_CMGE :
				ORLIX_TCTI_SIMD_COMPARE_CMGT;
		else if (pattern == AARCH64_SIMD_CMEQ_ZERO_PATTERN)
			decoded.simd_compare_op = u ? ORLIX_TCTI_SIMD_COMPARE_CMLE :
				ORLIX_TCTI_SIMD_COMPARE_CMEQ;
		else
			decoded.simd_compare_op = ORLIX_TCTI_SIMD_COMPARE_CMLT;
		return decoded;
	}

simd_compare_zero_unclaimed:

	if ((instruction & AARCH64_SIMD_CMHI_2D_MASK) ==
	    AARCH64_SIMD_CMHI_2D_PATTERN) {
		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_COMPARE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = sizeof(u64);
		decoded.result_size = 2 * sizeof(u64);
		decoded.simd_fp = true;
		decoded.simd_compare_op = ORLIX_TCTI_SIMD_COMPARE_CMHI;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_INDEXED_MASK) ==
	    AARCH64_SIMD_INDEXED_PATTERN) {
		u8 size = (instruction >> 22) & 0x3U;
		u8 opcode = (instruction >> 12) & 0xfU;
		u8 operation_key = opcode |
			(instruction & BIT(29) ? 0x10U : 0);
		bool q = instruction & BIT(30);
		bool scalar = instruction & BIT(28);
		bool fp_operation = false;
		bool long_operation = false;
		bool scalar_allowed = false;

		switch (operation_key) {
		case 0x01:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FMLA;
			fp_operation = true;
			scalar_allowed = true;
			break;
		case 0x05:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FMLS;
			fp_operation = true;
			scalar_allowed = true;
			break;
		case 0x09:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FMUL;
			fp_operation = true;
			scalar_allowed = true;
			break;
		case 0x19:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_FMULX;
			fp_operation = true;
			scalar_allowed = true;
			break;
		case 0x0c:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_SQDMULH;
			scalar_allowed = true;
			break;
		case 0x0d:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_SQRDMULH;
			scalar_allowed = true;
			break;
		case 0x10:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_MLA;
			break;
		case 0x14:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_MLS;
			break;
		case 0x08:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_MUL;
			break;
		case 0x02:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_SMLAL;
			long_operation = true;
			break;
		case 0x06:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_SMLSL;
			long_operation = true;
			break;
		case 0x0a:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_SMULL;
			long_operation = true;
			break;
		case 0x03:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_SQDMLAL;
			long_operation = true;
			scalar_allowed = true;
			break;
		case 0x07:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_SQDMLSL;
			long_operation = true;
			scalar_allowed = true;
			break;
		case 0x0b:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_SQDMULL;
			long_operation = true;
			scalar_allowed = true;
			break;
		case 0x12:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_UMLAL;
			long_operation = true;
			break;
		case 0x16:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_UMLSL;
			long_operation = true;
			break;
		case 0x1a:
			decoded.simd_arithmetic_op = ORLIX_TCTI_SIMD_ARITH_UMULL;
			long_operation = true;
			break;
		default:
			return decoded;
		}
		if (scalar && (!q || !scalar_allowed))
			return decoded;

		if (fp_operation) {
			if ((size != 2 && size != 3) ||
			    (size == 3 && (!q || instruction & BIT(21))))
				return decoded;
		} else if (size != 1 && size != 2) {
			return decoded;
		}

		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = size == 1 ? (instruction >> 16) & 0xfU :
			(instruction >> 16) & 0x1fU;
		decoded.access_size = BIT(size);
		decoded.result_size = scalar ?
			(long_operation ? 2 * decoded.access_size :
			 decoded.access_size) :
			(long_operation ? 2 * sizeof(u64) :
			 (q ? 2 * sizeof(u64) : sizeof(u64)));
		if (size == 1)
			decoded.simd_source_index =
				!!(instruction & BIT(20)) |
				(!!(instruction & BIT(21)) << 1) |
				(!!(instruction & BIT(11)) << 2);
		else if (size == 2)
			decoded.simd_source_index =
				!!(instruction & BIT(21)) |
				(!!(instruction & BIT(11)) << 1);
		else
			decoded.simd_source_index = !!(instruction & BIT(11));
		decoded.simd_fp = true;
		decoded.simd_scalar = scalar;
		decoded.simd_q = q;
		decoded.simd_indexed = true;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_MIN_MAXV_MASK) ==
	    AARCH64_SIMD_MIN_MAXV_PATTERN) {
		u8 size = (instruction >> 22) & 0x3U;
		bool q = instruction & BIT(30);
		bool unsigned_compare = instruction & BIT(29);
		bool minimum = instruction & BIT(16);

		if (size == 3 || (size == 2 && !q))
			return decoded;
		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_REDUCTION;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = 1U << size;
		decoded.result_size = decoded.access_size;
		decoded.simd_q = q;
		decoded.simd_fp = true;
		if (unsigned_compare)
			decoded.simd_reduction_op = minimum ?
				ORLIX_TCTI_SIMD_REDUCTION_UMINV :
				ORLIX_TCTI_SIMD_REDUCTION_UMAXV;
		else
			decoded.simd_reduction_op = minimum ?
				ORLIX_TCTI_SIMD_REDUCTION_SMINV :
				ORLIX_TCTI_SIMD_REDUCTION_SMAXV;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_ADDV_MASK) ==
	    AARCH64_SIMD_ADDV_PATTERN) {
		u8 size = (instruction >> 22) & 0x3U;
		bool q = instruction & BIT(30);

		if (size == 3 || (size == 2 && !q))
			return decoded;

		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_REDUCTION;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = 1U << size;
		decoded.result_size = decoded.access_size;
		decoded.simd_q = q;
		decoded.simd_fp = true;
		decoded.simd_reduction_op = ORLIX_TCTI_SIMD_REDUCTION_ADDV;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_ADD_LONGV_MASK) ==
	    AARCH64_SIMD_ADD_LONGV_PATTERN) {
		u8 size = (instruction >> 22) & 0x3U;
		bool q = instruction & BIT(30);

		if (size == 3 || (size == 2 && !q))
			return decoded;
		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_REDUCTION;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = 1U << size;
		decoded.result_size = 2 * decoded.access_size;
		decoded.simd_q = q;
		decoded.simd_fp = true;
		decoded.simd_reduction_op = instruction & BIT(29) ?
			ORLIX_TCTI_SIMD_REDUCTION_UADDLV :
			ORLIX_TCTI_SIMD_REDUCTION_SADDLV;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_FP_ACROSS_LANES_MASK) ==
		    AARCH64_SIMD_FMAXNMV_PATTERN ||
	    (instruction & AARCH64_SIMD_FP_ACROSS_LANES_MASK) ==
		    AARCH64_SIMD_FMAXV_PATTERN ||
	    (instruction & AARCH64_SIMD_FP_ACROSS_LANES_MASK) ==
		    AARCH64_SIMD_FMINNMV_PATTERN ||
	    (instruction & AARCH64_SIMD_FP_ACROSS_LANES_MASK) ==
		    AARCH64_SIMD_FMINV_PATTERN) {
		u32 pattern = instruction & AARCH64_SIMD_FP_ACROSS_LANES_MASK;

		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_REDUCTION;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = sizeof(u32);
		decoded.result_size = sizeof(u32);
		decoded.simd_q = true;
		decoded.simd_fp = true;
		switch (pattern) {
		case AARCH64_SIMD_FMAXNMV_PATTERN:
			decoded.simd_reduction_op = ORLIX_TCTI_SIMD_REDUCTION_FMAXNMV;
			break;
		case AARCH64_SIMD_FMAXV_PATTERN:
			decoded.simd_reduction_op = ORLIX_TCTI_SIMD_REDUCTION_FMAXV;
			break;
		case AARCH64_SIMD_FMINNMV_PATTERN:
			decoded.simd_reduction_op = ORLIX_TCTI_SIMD_REDUCTION_FMINNMV;
			break;
		default:
			decoded.simd_reduction_op = ORLIX_TCTI_SIMD_REDUCTION_FMINV;
			break;
		}
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_ADDP_D_2D_MASK) ==
	    AARCH64_SIMD_ADDP_D_2D_PATTERN) {
		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_REDUCTION;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = sizeof(u64);
		decoded.result_size = sizeof(u64);
		decoded.simd_fp = true;
		decoded.simd_reduction_op = ORLIX_TCTI_SIMD_REDUCTION_ADDP;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_FP_PAIRWISE_MASK) ==
		    AARCH64_SIMD_FADDP_SCALAR_PATTERN ||
	    (instruction & AARCH64_SIMD_FP_PAIRWISE_MASK) ==
		    AARCH64_SIMD_FMAXNMP_SCALAR_PATTERN ||
	    (instruction & AARCH64_SIMD_FP_PAIRWISE_MASK) ==
		    AARCH64_SIMD_FMAXP_SCALAR_PATTERN ||
	    (instruction & AARCH64_SIMD_FP_PAIRWISE_MASK) ==
		    AARCH64_SIMD_FMINNMP_SCALAR_PATTERN ||
	    (instruction & AARCH64_SIMD_FP_PAIRWISE_MASK) ==
		    AARCH64_SIMD_FMINP_SCALAR_PATTERN) {
		u32 pattern = instruction & AARCH64_SIMD_FP_PAIRWISE_MASK;

		decoded.decode_class = ORLIX_TCTI_DECODE_SIMD_VECTOR_REDUCTION;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = instruction & BIT(22) ? sizeof(u64) :
							      sizeof(u32);
		decoded.result_size = decoded.access_size;
		decoded.simd_fp = true;
		switch (pattern) {
		case AARCH64_SIMD_FADDP_SCALAR_PATTERN:
			decoded.simd_reduction_op = ORLIX_TCTI_SIMD_REDUCTION_FADDP;
			break;
		case AARCH64_SIMD_FMAXNMP_SCALAR_PATTERN:
			decoded.simd_reduction_op = ORLIX_TCTI_SIMD_REDUCTION_FMAXNMP;
			break;
		case AARCH64_SIMD_FMAXP_SCALAR_PATTERN:
			decoded.simd_reduction_op = ORLIX_TCTI_SIMD_REDUCTION_FMAXP;
			break;
		case AARCH64_SIMD_FMINNMP_SCALAR_PATTERN:
			decoded.simd_reduction_op = ORLIX_TCTI_SIMD_REDUCTION_FMINNMP;
			break;
		default:
			decoded.simd_reduction_op = ORLIX_TCTI_SIMD_REDUCTION_FMINP;
			break;
		}
		return decoded;
	}

	if ((instruction & AARCH64_FMOV_W_S_MASK) == AARCH64_FMOV_W_S_PATTERN ||
	    (instruction & AARCH64_FMOV_W_S_MASK) == AARCH64_FMOV_X_D_PATTERN) {
		decoded.decode_class = ORLIX_TCTI_DECODE_FP_SCALAR_MOVE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size =
			(instruction & BIT(31)) ? sizeof(u64) : sizeof(u32);
		decoded.result_size = decoded.access_size;
		decoded.simd_fp = true;
		decoded.fp_move_op = ORLIX_TCTI_FP_MOVE_SIMD_TO_GPR;
		return decoded;
	}

	if ((instruction & AARCH64_FMOV_W_S_MASK) == AARCH64_FMOV_S_W_PATTERN ||
	    (instruction & AARCH64_FMOV_W_S_MASK) == AARCH64_FMOV_D_X_PATTERN) {
		decoded.decode_class = ORLIX_TCTI_DECODE_FP_SCALAR_MOVE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size =
			(instruction & BIT(31)) ? sizeof(u64) : sizeof(u32);
		decoded.result_size = decoded.access_size;
		decoded.simd_fp = true;
		decoded.fp_move_op = ORLIX_TCTI_FP_MOVE_GPR_TO_SIMD;
		return decoded;
	}

	if ((instruction & AARCH64_FMOV_W_S_MASK) ==
	    AARCH64_FMOV_X_V_HIGH_PATTERN) {
		decoded.decode_class = ORLIX_TCTI_DECODE_FP_SCALAR_MOVE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = sizeof(u64);
		decoded.result_size = sizeof(u64);
		decoded.simd_fp = true;
		decoded.fp_move_op = ORLIX_TCTI_FP_MOVE_SIMD_HIGH_TO_GPR;
		return decoded;
	}

	if ((instruction & AARCH64_FMOV_W_S_MASK) ==
	    AARCH64_FMOV_V_HIGH_X_PATTERN) {
		decoded.decode_class = ORLIX_TCTI_DECODE_FP_SCALAR_MOVE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = sizeof(u64);
		decoded.result_size = sizeof(u64);
		decoded.simd_fp = true;
		decoded.fp_move_op = ORLIX_TCTI_FP_MOVE_GPR_TO_SIMD_HIGH;
		return decoded;
	}

	if ((instruction & AARCH64_FMOV_IMMEDIATE_MASK) ==
		    AARCH64_FMOV_S_IMMEDIATE_PATTERN ||
	    (instruction & AARCH64_FMOV_IMMEDIATE_MASK) ==
		    AARCH64_FMOV_D_IMMEDIATE_PATTERN) {
		bool is_double = (instruction & AARCH64_FMOV_IMMEDIATE_MASK) ==
				 AARCH64_FMOV_D_IMMEDIATE_PATTERN;
		u8 imm8 = (instruction >> 13) & 0xffU;

		decoded.decode_class = ORLIX_TCTI_DECODE_FP_SCALAR_IMMEDIATE;
		decoded.rd = instruction & 0x1fU;
		decoded.logical_immediate = is_double ?
			orlix_tcti_expand_fp_immediate(imm8, 11, 52) :
			orlix_tcti_expand_fp_immediate(imm8, 8, 23);
		decoded.access_size = is_double ? sizeof(u64) : sizeof(u32);
		decoded.result_size = decoded.access_size;
		decoded.simd_fp = true;
		return decoded;
	}

	if ((instruction & AARCH64_FMOV_S_S_MASK) == AARCH64_FMOV_S_S_PATTERN ||
	    (instruction & AARCH64_FMOV_D_D_MASK) == AARCH64_FMOV_D_D_PATTERN) {
		decoded.decode_class = ORLIX_TCTI_DECODE_FP_SCALAR_MOVE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size =
			(instruction & BIT(22)) ? sizeof(u64) : sizeof(u32);
		decoded.result_size = decoded.access_size;
		decoded.simd_fp = true;
		decoded.fp_move_op = ORLIX_TCTI_FP_MOVE_REGISTER;
		return decoded;
	}

	if ((instruction & AARCH64_FCVT_D_S_MASK) == AARCH64_FCVT_D_S_PATTERN) {
		decoded.decode_class = ORLIX_TCTI_DECODE_FP_SCALAR_1SOURCE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = sizeof(u32);
		decoded.result_size = sizeof(u64);
		decoded.simd_fp = true;
		decoded.fp1_op = ORLIX_TCTI_FP1_FCVT;
		return decoded;
	}

	if ((instruction & AARCH64_FCVT_S_D_MASK) == AARCH64_FCVT_S_D_PATTERN) {
		decoded.decode_class = ORLIX_TCTI_DECODE_FP_SCALAR_1SOURCE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = sizeof(u64);
		decoded.result_size = sizeof(u32);
		decoded.simd_fp = true;
		decoded.fp1_op = ORLIX_TCTI_FP1_FCVT;
		return decoded;
	}
	if ((instruction & AARCH64_FCVT_D_S_MASK) == AARCH64_FCVT_D_H_PATTERN ||
	    (instruction & AARCH64_FCVT_D_S_MASK) == AARCH64_FCVT_H_D_PATTERN ||
	    (instruction & AARCH64_FCVT_D_S_MASK) == AARCH64_FCVT_H_S_PATTERN ||
	    (instruction & AARCH64_FCVT_D_S_MASK) == AARCH64_FCVT_S_H_PATTERN) {
		u32 pattern = instruction & AARCH64_FCVT_D_S_MASK;

		decoded.decode_class = ORLIX_TCTI_DECODE_FP_SCALAR_1SOURCE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		if (pattern == AARCH64_FCVT_D_H_PATTERN) {
			decoded.access_size = sizeof(u16);
			decoded.result_size = sizeof(u64);
		} else if (pattern == AARCH64_FCVT_H_D_PATTERN) {
			decoded.access_size = sizeof(u64);
			decoded.result_size = sizeof(u16);
		} else if (pattern == AARCH64_FCVT_H_S_PATTERN) {
			decoded.access_size = sizeof(u32);
			decoded.result_size = sizeof(u16);
		} else {
			decoded.access_size = sizeof(u16);
			decoded.result_size = sizeof(u32);
		}
		decoded.simd_fp = true;
		decoded.fp1_op = ORLIX_TCTI_FP1_FCVT;
		return decoded;
	}

	if ((instruction & AARCH64_FSQRT_S_MASK) == AARCH64_FSQRT_S_PATTERN ||
	    (instruction & AARCH64_FSQRT_D_MASK) == AARCH64_FSQRT_D_PATTERN ||
	    (instruction & AARCH64_FSQRT_S_MASK) == AARCH64_FSQRT_H_PATTERN) {
		decoded.decode_class = ORLIX_TCTI_DECODE_FP_SCALAR_1SOURCE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = ((instruction >> 22) & 0x3U) == 3 ?
			sizeof(u16) : instruction & BIT(22) ? sizeof(u64) :
			sizeof(u32);
		decoded.result_size = decoded.access_size;
		decoded.simd_fp = true;
		decoded.fp1_op = ORLIX_TCTI_FP1_FSQRT;
		return decoded;
	}
	if ((instruction & AARCH64_FABS_S_MASK) == AARCH64_FABS_S_PATTERN ||
	    (instruction & AARCH64_FABS_D_MASK) == AARCH64_FABS_D_PATTERN ||
	    (instruction & AARCH64_FABS_S_MASK) == AARCH64_FABS_H_PATTERN) {
		decoded.decode_class = ORLIX_TCTI_DECODE_FP_SCALAR_1SOURCE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = ((instruction >> 22) & 0x3U) == 3 ?
			sizeof(u16) : instruction & BIT(22) ? sizeof(u64) :
			sizeof(u32);
		decoded.result_size = decoded.access_size;
		decoded.simd_fp = true;
		decoded.fp1_op = ORLIX_TCTI_FP1_FABS;
		return decoded;
	}

	if ((instruction & AARCH64_FNEG_S_MASK) == AARCH64_FNEG_S_PATTERN ||
	    (instruction & AARCH64_FNEG_D_MASK) == AARCH64_FNEG_D_PATTERN ||
	    (instruction & AARCH64_FNEG_S_MASK) == AARCH64_FNEG_H_PATTERN) {
		decoded.decode_class = ORLIX_TCTI_DECODE_FP_SCALAR_1SOURCE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = ((instruction >> 22) & 0x3U) == 3 ?
			sizeof(u16) : instruction & BIT(22) ? sizeof(u64) :
			sizeof(u32);
		decoded.result_size = decoded.access_size;
		decoded.simd_fp = true;
		decoded.fp1_op = ORLIX_TCTI_FP1_FNEG;
		return decoded;
	}

	if ((instruction & AARCH64_FP_SCALAR_1SOURCE_MASK) ==
	    AARCH64_FP_SCALAR_1SOURCE_PATTERN) {
		u8 type = (instruction >> 22) & 0x3U;
		u8 opcode = (instruction >> 15) & 0x3fU;

		if (type == 2)
			return decoded;
		switch (opcode) {
		case 8:
			decoded.fp1_op = ORLIX_TCTI_FP1_FRINTN;
			break;
		case 9:
			decoded.fp1_op = ORLIX_TCTI_FP1_FRINTP;
			break;
		case 10:
			decoded.fp1_op = ORLIX_TCTI_FP1_FRINTM;
			break;
		case 11:
			decoded.fp1_op = ORLIX_TCTI_FP1_FRINTZ;
			break;
		case 12:
			decoded.fp1_op = ORLIX_TCTI_FP1_FRINTA;
			break;
		case 14:
			decoded.fp1_op = ORLIX_TCTI_FP1_FRINTX;
			break;
		case 15:
			decoded.fp1_op = ORLIX_TCTI_FP1_FRINTI;
			break;
		default:
			return decoded;
		}
		decoded.decode_class = ORLIX_TCTI_DECODE_FP_SCALAR_1SOURCE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = type == 3 ? sizeof(u16) :
			type ? sizeof(u64) : sizeof(u32);
		decoded.result_size = decoded.access_size;
		decoded.simd_fp = true;
		return decoded;
	}

	if ((instruction & AARCH64_FP_SCALAR_2SOURCE_MASK) ==
	    AARCH64_FP_SCALAR_2SOURCE_PATTERN) {
		u8 type = (instruction >> 22) & 0x3U;
		u8 opcode = (instruction >> 12) & 0xfU;

		if (type == 2)
			return decoded;

		switch (opcode) {
		case 0:
			decoded.fp2_op = ORLIX_TCTI_FP2_FMUL;
			break;
		case 1:
			decoded.fp2_op = ORLIX_TCTI_FP2_FDIV;
			break;
		case 2:
			decoded.fp2_op = ORLIX_TCTI_FP2_FADD;
			break;
		case 3:
			decoded.fp2_op = ORLIX_TCTI_FP2_FSUB;
			break;
		case 4:
			decoded.fp2_op = ORLIX_TCTI_FP2_FMAX;
			break;
		case 5:
			decoded.fp2_op = ORLIX_TCTI_FP2_FMIN;
			break;
		case 6:
			decoded.fp2_op = ORLIX_TCTI_FP2_FMAXNM;
			break;
		case 7:
			decoded.fp2_op = ORLIX_TCTI_FP2_FMINNM;
			break;
		case 8:
			decoded.fp2_op = ORLIX_TCTI_FP2_FNMUL;
			break;
		default:
			return decoded;
		}

		decoded.decode_class = ORLIX_TCTI_DECODE_FP_SCALAR_2SOURCE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = type == 3 ? sizeof(u16) :
			type ? sizeof(u64) : sizeof(u32);
		decoded.result_size = decoded.access_size;
		decoded.simd_fp = true;
		return decoded;
	}

	if ((instruction & AARCH64_FMUL_2D_MASK) == AARCH64_FMUL_2D_PATTERN) {
		decoded.decode_class = ORLIX_TCTI_DECODE_FP_SCALAR_2SOURCE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = sizeof(u64);
		decoded.result_size = 2 * sizeof(u64);
		decoded.simd_fp = true;
		decoded.fp2_op = ORLIX_TCTI_FP2_FMUL;
		return decoded;
	}

	if ((instruction & AARCH64_FCMP_S_MASK) == AARCH64_FCMP_S_PATTERN ||
	    (instruction & AARCH64_FCMP_D_MASK) == AARCH64_FCMP_D_PATTERN ||
	    (instruction & AARCH64_FCMP_S_MASK) == AARCH64_FCMP_H_PATTERN ||
	    (instruction & AARCH64_FCMP_S_MASK) == AARCH64_FCMP_S_ZERO_PATTERN ||
	    (instruction & AARCH64_FCMP_D_MASK) == AARCH64_FCMP_D_ZERO_PATTERN ||
	    (instruction & AARCH64_FCMP_S_MASK) == AARCH64_FCMP_H_ZERO_PATTERN) {
		bool compare_zero =
			(instruction & AARCH64_FCMP_S_MASK) ==
				AARCH64_FCMP_S_ZERO_PATTERN ||
			(instruction & AARCH64_FCMP_D_MASK) ==
				AARCH64_FCMP_D_ZERO_PATTERN ||
			(instruction & AARCH64_FCMP_S_MASK) ==
				AARCH64_FCMP_H_ZERO_PATTERN;

		decoded.decode_class = ORLIX_TCTI_DECODE_FP_SCALAR_COMPARE;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = ((instruction >> 22) & 0x3U) == 3 ?
			sizeof(u16) : instruction & BIT(22) ? sizeof(u64) :
			sizeof(u32);
		decoded.result_size = decoded.access_size;
		decoded.immediate = compare_zero;
		decoded.fp_signal_all_nans = instruction & BIT(4);
		decoded.simd_fp = true;
		return decoded;
	}

	if ((instruction & AARCH64_FP_INT_GPR_MASK) ==
	    AARCH64_FP_INT_GPR_PATTERN) {
		u8 type = (instruction >> 22) & 0x3U;
		u8 rounding = (instruction >> 19) & 0x3U;
		u8 opcode = (instruction >> 16) & 0x7U;

		if (type > 1)
			return decoded;
		if (opcode <= 1) {
			static const enum orlix_tcti_fp_int_convert_op operations[][2] = {
				{ ORLIX_TCTI_FP_INT_FCVTNS, ORLIX_TCTI_FP_INT_FCVTNU },
				{ ORLIX_TCTI_FP_INT_FCVTPS, ORLIX_TCTI_FP_INT_FCVTPU },
				{ ORLIX_TCTI_FP_INT_FCVTMS, ORLIX_TCTI_FP_INT_FCVTMU },
				{ ORLIX_TCTI_FP_INT_FCVTZS, ORLIX_TCTI_FP_INT_FCVTZU },
			};

			decoded.fp_int_op = operations[rounding][opcode];
			decoded.access_size = type ? sizeof(u64) : sizeof(u32);
			decoded.result_size = instruction & BIT(31) ?
				sizeof(u64) : sizeof(u32);
		} else if (!rounding && (opcode == 4 || opcode == 5)) {
			decoded.fp_int_op = opcode == 4 ? ORLIX_TCTI_FP_INT_FCVTAS :
				ORLIX_TCTI_FP_INT_FCVTAU;
			decoded.access_size = type ? sizeof(u64) : sizeof(u32);
			decoded.result_size = instruction & BIT(31) ?
				sizeof(u64) : sizeof(u32);
		} else if (!rounding && (opcode == 2 || opcode == 3)) {
			decoded.fp_int_op = opcode == 2 ? ORLIX_TCTI_FP_INT_SCVTF :
				ORLIX_TCTI_FP_INT_UCVTF;
			decoded.access_size = instruction & BIT(31) ?
				sizeof(u64) : sizeof(u32);
			decoded.result_size = type ? sizeof(u64) : sizeof(u32);
		} else {
			return decoded;
		}
		decoded.decode_class = ORLIX_TCTI_DECODE_FP_INT_CONVERT;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.simd_fp = true;
		return decoded;
	}
fp_int_gpr_unclaimed:
	if ((instruction & AARCH64_SCVTF_S_W_MASK) == AARCH64_SCVTF_S_W_PATTERN ||
	    (instruction & AARCH64_SCVTF_D_W_MASK) == AARCH64_SCVTF_D_W_PATTERN ||
	    (instruction & AARCH64_SCVTF_S_X_MASK) == AARCH64_SCVTF_S_X_PATTERN ||
	    (instruction & AARCH64_SCVTF_D_X_MASK) == AARCH64_SCVTF_D_X_PATTERN) {
		decoded.decode_class = ORLIX_TCTI_DECODE_FP_INT_CONVERT;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size =
			(instruction & BIT(31)) ? sizeof(u64) : sizeof(u32);
		decoded.result_size =
			(instruction & BIT(22)) ? sizeof(u64) : sizeof(u32);
		decoded.simd_fp = true;
		decoded.fp_int_op = ORLIX_TCTI_FP_INT_SCVTF;
		return decoded;
	}

	if ((instruction & AARCH64_UCVTF_S_GPR_MASK) ==
	    AARCH64_UCVTF_S_GPR_PATTERN) {
		decoded.decode_class = ORLIX_TCTI_DECODE_FP_INT_CONVERT;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size =
			(instruction & BIT(31)) ? sizeof(u64) : sizeof(u32);
		decoded.result_size = sizeof(u32);
		decoded.simd_fp = true;
		decoded.fp_int_op = ORLIX_TCTI_FP_INT_UCVTF;
		return decoded;
	}

	if ((instruction & AARCH64_UCVTF_D_GPR_MASK) ==
	    AARCH64_UCVTF_D_GPR_PATTERN) {
		decoded.decode_class = ORLIX_TCTI_DECODE_FP_INT_CONVERT;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size =
			(instruction & BIT(31)) ? sizeof(u64) : sizeof(u32);
		decoded.result_size = sizeof(u64);
		decoded.simd_fp = true;
		decoded.fp_int_op = ORLIX_TCTI_FP_INT_UCVTF;
		return decoded;
	}

	if ((instruction & AARCH64_FCVTZS_GPR_MASK) ==
	    AARCH64_FCVTZS_GPR_PATTERN ||
	    (instruction & AARCH64_FCVTZU_GPR_MASK) ==
	    AARCH64_FCVTZU_GPR_PATTERN) {
		decoded.decode_class = ORLIX_TCTI_DECODE_FP_INT_CONVERT;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = instruction & BIT(22) ? sizeof(u64) :
							 sizeof(u32);
		decoded.result_size = instruction & BIT(31) ? sizeof(u64) :
							 sizeof(u32);
		decoded.simd_fp = true;
		decoded.fp_int_op = instruction & BIT(16) ?
			ORLIX_TCTI_FP_INT_FCVTZU : ORLIX_TCTI_FP_INT_FCVTZS;
		return decoded;
	}

	if ((instruction & AARCH64_FCVTZS_D_GPR_MASK) ==
	    AARCH64_FCVTZS_D_GPR_PATTERN) {
		decoded.decode_class = ORLIX_TCTI_DECODE_FP_INT_CONVERT;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = sizeof(u64);
		decoded.result_size =
			(instruction & BIT(31)) ? sizeof(u64) : sizeof(u32);
		decoded.simd_fp = true;
		decoded.fp_int_op = ORLIX_TCTI_FP_INT_FCVTZS;
		return decoded;
	}

	if ((instruction & AARCH64_FCVTZU_GPR_S_MASK) ==
	    AARCH64_FCVTZU_GPR_S_PATTERN) {
		decoded.decode_class = ORLIX_TCTI_DECODE_FP_INT_CONVERT;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = sizeof(u32);
		decoded.result_size =
			(instruction & BIT(31)) ? sizeof(u64) : sizeof(u32);
		decoded.simd_fp = true;
		decoded.fp_int_op = ORLIX_TCTI_FP_INT_FCVTZU;
		return decoded;
	}

	if ((instruction & AARCH64_FCVTZU_X_D_MASK) ==
	    AARCH64_FCVTZU_X_D_PATTERN) {
		decoded.decode_class = ORLIX_TCTI_DECODE_FP_INT_CONVERT;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = sizeof(u64);
		decoded.result_size = sizeof(u64);
		decoded.simd_fp = true;
		decoded.fp_int_op = ORLIX_TCTI_FP_INT_FCVTZU;
		return decoded;
	}
	if ((instruction & AARCH64_FCVTZU_W_D_MASK) ==
	    AARCH64_FCVTZU_W_D_PATTERN) {
		decoded.decode_class = ORLIX_TCTI_DECODE_FP_INT_CONVERT;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = sizeof(u64);
		decoded.result_size = sizeof(u32);
		decoded.simd_fp = true;
		decoded.fp_int_op = ORLIX_TCTI_FP_INT_FCVTZU;
		return decoded;
	}

	if ((instruction & AARCH64_FCVTZ_FIXED_GPR_MASK) ==
	    AARCH64_FCVTZ_FIXED_GPR_PATTERN ||
	    (instruction & AARCH64_FCVTZ_FIXED_GPR_MASK) ==
	    AARCH64_CVTF_FIXED_GPR_PATTERN) {
		u8 scale = (instruction >> 10) & 0x3fU;
		u8 type = (instruction >> 22) & 0x3U;
		bool is_64bit = instruction & BIT(31);
		bool int_to_fp = (instruction & AARCH64_FCVTZ_FIXED_GPR_MASK) ==
			AARCH64_CVTF_FIXED_GPR_PATTERN;

		if (type > 1)
			return decoded;
		if (!is_64bit && !(scale & BIT(5)))
			return decoded;

		decoded.decode_class = ORLIX_TCTI_DECODE_FP_INT_CONVERT;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = int_to_fp ?
			(is_64bit ? sizeof(u64) : sizeof(u32)) :
			(instruction & BIT(22) ? sizeof(u64) : sizeof(u32));
		decoded.result_size = int_to_fp ?
			(instruction & BIT(22) ? sizeof(u64) : sizeof(u32)) :
			(is_64bit ? sizeof(u64) : sizeof(u32));
		decoded.shift_amount = 64 - scale;
		decoded.simd_fp = true;
		if (int_to_fp)
			decoded.fp_int_op = instruction & BIT(16) ?
				ORLIX_TCTI_FP_INT_UCVTF_FIXED :
				ORLIX_TCTI_FP_INT_SCVTF_FIXED;
		else
			decoded.fp_int_op = instruction & BIT(16) ?
				ORLIX_TCTI_FP_INT_FCVTZU_FIXED :
				ORLIX_TCTI_FP_INT_FCVTZS_FIXED;
		return decoded;
	}

	if ((instruction & AARCH64_FP_FIXED_SIMD_SCALAR_MASK) ==
	    0x5f00fc00U ||
	    (instruction & AARCH64_FP_FIXED_SIMD_SCALAR_MASK) ==
	    0x5f00e400U ||
	    (instruction & AARCH64_FP_FIXED_SIMD_VECTOR_MASK) ==
	    0x0f00fc00U ||
	    (instruction & AARCH64_FP_FIXED_SIMD_VECTOR_MASK) ==
	    0x0f00e400U) {
		u8 immediate = (instruction >> 16) & 0x7fU;
		bool scalar = instruction & BIT(28);
		bool int_to_fp = !(instruction & BIT(12));
		bool is_double = immediate & BIT(6);

		if (!(immediate & (BIT(5) | BIT(6))))
			return decoded;
		if (!scalar && is_double && !(instruction & BIT(30)))
			return decoded;

		decoded.decode_class = ORLIX_TCTI_DECODE_FP_INT_CONVERT;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = is_double ? sizeof(u64) : sizeof(u32);
		decoded.result_size = decoded.access_size;
		decoded.shift_amount = (is_double ? 128 : 64) - immediate;
		decoded.simd_fp = true;
		decoded.simd_scalar = scalar;
		decoded.simd_q = instruction & BIT(30);
		if (int_to_fp)
			decoded.fp_int_op = instruction & BIT(29) ?
				ORLIX_TCTI_FP_INT_UCVTF_FIXED_SIMD :
				ORLIX_TCTI_FP_INT_SCVTF_FIXED_SIMD;
		else
			decoded.fp_int_op = instruction & BIT(29) ?
				ORLIX_TCTI_FP_INT_FCVTZU_FIXED_SIMD :
				ORLIX_TCTI_FP_INT_FCVTZS_FIXED_SIMD;
		return decoded;
	}

	{
		static const struct {
			u32 scalar_pattern;
			u32 vector_pattern;
			enum orlix_tcti_fp_int_convert_op signed_operation;
			enum orlix_tcti_fp_int_convert_op unsigned_operation;
		} operations[] = {
			{ 0x5e21a800U, 0x0e21a800U,
			  ORLIX_TCTI_FP_INT_FCVTNS_SIMD,
			  ORLIX_TCTI_FP_INT_FCVTNU_SIMD },
			{ 0x5ea1a800U, 0x0ea1a800U,
			  ORLIX_TCTI_FP_INT_FCVTPS_SIMD,
			  ORLIX_TCTI_FP_INT_FCVTPU_SIMD },
			{ 0x5e21b800U, 0x0e21b800U,
			  ORLIX_TCTI_FP_INT_FCVTMS_SIMD,
			  ORLIX_TCTI_FP_INT_FCVTMU_SIMD },
			{ 0x5ea1b800U, 0x0ea1b800U,
			  ORLIX_TCTI_FP_INT_FCVTZS_SIMD,
			  ORLIX_TCTI_FP_INT_FCVTZU_SIMD },
			{ 0x5e21c800U, 0x0e21c800U,
			  ORLIX_TCTI_FP_INT_FCVTAS_SIMD,
			  ORLIX_TCTI_FP_INT_FCVTAU_SIMD },
			{ 0x5e21d800U, 0x0e21d800U,
			  ORLIX_TCTI_FP_INT_SCVTF_SIMD,
			  ORLIX_TCTI_FP_INT_UCVTF_SIMD },
		};
		bool scalar = instruction & BIT(28);
		bool is_double = instruction & BIT(22);
		bool q = instruction & BIT(30);
		u32 mask = scalar ? AARCH64_FP_INT_SIMD_SCALAR_MASK :
				    AARCH64_FP_INT_SIMD_VECTOR_MASK;
		u32 pattern = instruction & mask;
		size_t index;

		for (index = 0; index < ARRAY_SIZE(operations); index++) {
			u32 expected = scalar ? operations[index].scalar_pattern :
						operations[index].vector_pattern;

			if (pattern != expected)
				continue;
			if (!scalar && is_double && !q)
				break;
			decoded.decode_class = ORLIX_TCTI_DECODE_FP_INT_CONVERT;
			decoded.rd = instruction & 0x1fU;
			decoded.rn = (instruction >> 5) & 0x1fU;
			decoded.access_size = is_double ? sizeof(u64) :
							 sizeof(u32);
			decoded.result_size = scalar ? decoded.access_size :
						(q ? 2 * sizeof(u64) :
						     sizeof(u64));
			decoded.simd_fp = true;
			decoded.simd_scalar = scalar;
			decoded.simd_q = scalar ? false : q;
			decoded.fp_int_op = instruction & BIT(29) ?
				operations[index].unsigned_operation :
				operations[index].signed_operation;
			return decoded;
		}
	}

	if ((instruction & AARCH64_FCVTZS_SIMD_SCALAR_MASK) ==
		    AARCH64_FCVTZS_SIMD_SCALAR_PATTERN ||
	    (instruction & AARCH64_FCVTZU_SIMD_SCALAR_MASK) ==
		    AARCH64_FCVTZU_SIMD_SCALAR_PATTERN) {
		decoded.decode_class = ORLIX_TCTI_DECODE_FP_INT_CONVERT;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size =
			(instruction & BIT(22)) ? sizeof(u64) : sizeof(u32);
		decoded.result_size = decoded.access_size;
		decoded.simd_fp = true;
		decoded.fp_int_op = (instruction & BIT(29)) ?
					    ORLIX_TCTI_FP_INT_FCVTZU_SIMD :
					    ORLIX_TCTI_FP_INT_FCVTZS_SIMD;
		return decoded;
	}

	if ((instruction & AARCH64_UCVTF_D_D_MASK) == AARCH64_UCVTF_D_D_PATTERN) {
		decoded.decode_class = ORLIX_TCTI_DECODE_FP_INT_CONVERT;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = sizeof(u64);
		decoded.result_size = sizeof(u64);
		decoded.simd_fp = true;
		decoded.fp_int_op = ORLIX_TCTI_FP_INT_UCVTF_SIMD;
		return decoded;
	}

	if ((instruction & AARCH64_SCVTF_D_D_MASK) ==
	    AARCH64_SCVTF_D_D_PATTERN) {
		decoded.decode_class = ORLIX_TCTI_DECODE_FP_INT_CONVERT;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = sizeof(u64);
		decoded.result_size = sizeof(u64);
		decoded.simd_fp = true;
		decoded.fp_int_op = ORLIX_TCTI_FP_INT_SCVTF_SIMD;
		return decoded;
	}

	if ((instruction & AARCH64_UCVTF_2D_2D_MASK) ==
	    AARCH64_UCVTF_2D_2D_PATTERN) {
		decoded.decode_class = ORLIX_TCTI_DECODE_FP_INT_CONVERT;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = sizeof(u64);
		decoded.result_size = 2 * sizeof(u64);
		decoded.simd_fp = true;
		decoded.fp_int_op = ORLIX_TCTI_FP_INT_UCVTF_SIMD;
		return decoded;
	}

	if ((instruction & AARCH64_SYSTEM_REGISTER_MASK) == AARCH64_MRS_PATTERN ||
	    (instruction & AARCH64_SYSTEM_REGISTER_MASK) == AARCH64_MSR_PATTERN) {
		u16 sysreg = (instruction >> 5) & 0xffffU;

		if (!orlix_tcti_system_accessor_decode(sysreg,
			(instruction & AARCH64_SYSTEM_REGISTER_MASK) == AARCH64_MSR_PATTERN,
			&decoded))
			return decoded;
		decoded.rt = instruction & 0x1fU;
		return decoded;
	}

	if ((instruction & AARCH64_SYSTEM_INSTRUCTION_MASK) == AARCH64_SYS_PATTERN ||
	    (instruction & AARCH64_SYSTEM_INSTRUCTION_MASK) == AARCH64_SYSL_PATTERN) {
		u16 selector = (((instruction >> 16) & 7U) << 11) |
			(((instruction >> 12) & 15U) << 7) |
			(((instruction >> 8) & 15U) << 3) |
			((instruction >> 5) & 7U);
		enum orlix_tcti_system_accessor_route route =
			(instruction & AARCH64_SYSTEM_INSTRUCTION_MASK) == AARCH64_SYS_PATTERN ?
			ORLIX_TCTI_SYSTEM_ACCESSOR_ROUTE_SYS :
			ORLIX_TCTI_SYSTEM_ACCESSOR_ROUTE_SYSL;

		if (!orlix_tcti_system_accessor_decode_route(selector, route, &decoded))
			return decoded;
		decoded.rt = instruction & 0x1fU;
		return decoded;
	}

	if ((instruction & AARCH64_SYSTEM_REGISTER_MASK) == AARCH64_MSRR_PATTERN ||
	    (instruction & AARCH64_SYSTEM_REGISTER_MASK) == AARCH64_MRRS_PATTERN) {
		u16 selector = (instruction >> 5) & 0xffffU;
		enum orlix_tcti_system_accessor_route route =
			(instruction & AARCH64_SYSTEM_REGISTER_MASK) == AARCH64_MSRR_PATTERN ?
			ORLIX_TCTI_SYSTEM_ACCESSOR_ROUTE_MSRR :
			ORLIX_TCTI_SYSTEM_ACCESSOR_ROUTE_MRRS;

		if (!orlix_tcti_system_accessor_decode_route(selector, route, &decoded))
			return decoded;
		decoded.rt = instruction & 0x1fU;
		return decoded;
	}

	return decoded;
}
