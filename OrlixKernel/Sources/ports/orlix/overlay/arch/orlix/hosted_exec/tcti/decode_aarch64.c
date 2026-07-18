// SPDX-License-Identifier: GPL-2.0-only
#include <linux/bits.h>
#include <linux/bitops.h>

#include "decode_aarch64.h"

#define AARCH64_SVC_MASK 0xffe0001fU
#define AARCH64_SVC_PATTERN 0xd4000001U
#define AARCH64_HINT_MASK 0xfffff01fU
#define AARCH64_HINT_PATTERN 0xd503201fU
#define AARCH64_PC_RELATIVE_ADDRESS_MASK 0x1f000000U
#define AARCH64_PC_RELATIVE_ADDRESS_PATTERN 0x10000000U
#define AARCH64_ADD_SUB_IMM_MASK 0x1f000000U
#define AARCH64_ADD_SUB_IMM_PATTERN 0x11000000U
#define AARCH64_ADD_SUB_SHIFTED_REG_MASK 0x1f200000U
#define AARCH64_ADD_SUB_SHIFTED_REG_PATTERN 0x0b000000U
#define AARCH64_ADD_SUB_EXTENDED_REG_MASK 0x1f200000U
#define AARCH64_ADD_SUB_EXTENDED_REG_PATTERN 0x0b200000U
#define AARCH64_ADD_SUB_WITH_CARRY_MASK 0x1fe00000U
#define AARCH64_ADD_SUB_WITH_CARRY_PATTERN 0x1a000000U
#define AARCH64_UNCONDITIONAL_BRANCH_IMM_MASK 0x7c000000U
#define AARCH64_UNCONDITIONAL_BRANCH_IMM_PATTERN 0x14000000U
#define AARCH64_BRANCH_REGISTER_MASK 0xfffffc1fU
#define AARCH64_BR_PATTERN 0xd61f0000U
#define AARCH64_BLR_PATTERN 0xd63f0000U
#define AARCH64_RET_PATTERN 0xd65f0000U
#define AARCH64_COMPARE_BRANCH_IMM_MASK 0x7e000000U
#define AARCH64_COMPARE_BRANCH_IMM_PATTERN 0x34000000U
#define AARCH64_TEST_BRANCH_IMM_MASK 0x7e000000U
#define AARCH64_TEST_BRANCH_IMM_PATTERN 0x36000000U
#define AARCH64_CONDITIONAL_BRANCH_IMM_MASK 0xff000010U
#define AARCH64_CONDITIONAL_BRANCH_IMM_PATTERN 0x54000000U
#define AARCH64_CONDITIONAL_COMPARE_MASK 0x1fe00000U
#define AARCH64_CONDITIONAL_COMPARE_PATTERN 0x1a400000U
#define AARCH64_CONDITIONAL_SELECT_MASK 0x1fe00000U
#define AARCH64_CONDITIONAL_SELECT_PATTERN 0x1a800000U
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
#define AARCH64_EXTRACT_MASK 0x7f800000U
#define AARCH64_EXTRACT_PATTERN 0x13800000U
#define AARCH64_DATA_PROCESSING_1SOURCE_MASK 0x5fe00000U
#define AARCH64_DATA_PROCESSING_1SOURCE_PATTERN 0x5ac00000U
#define AARCH64_DP1_REV16_32_OPCODE 0x01U
#define AARCH64_DP1_REV32_OPCODE 0x02U
#define AARCH64_DP1_CLZ_OPCODE 0x04U
#define AARCH64_DATA_PROCESSING_2SOURCE_MASK 0x7fe00000U
#define AARCH64_DATA_PROCESSING_2SOURCE_PATTERN 0x1ac00000U
#define AARCH64_MULTIPLY_ADD_SUB_MASK 0x7f000000U
#define AARCH64_MULTIPLY_ADD_SUB_PATTERN 0x1b000000U
#define AARCH64_MOVE_WIDE_IMM_MASK 0x1f800000U
#define AARCH64_MOVE_WIDE_IMM_PATTERN 0x12800000U
#define AARCH64_CLREX 0xd5033f5fU
#define AARCH64_LOAD_STORE_EXCLUSIVE_MASK 0x3f007c00U
#define AARCH64_LOAD_STORE_EXCLUSIVE_PATTERN 0x08007c00U
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
#define AARCH64_SIMD_INS_GPR_MASK 0xffe0fc00U
#define AARCH64_SIMD_INS_GPR_PATTERN 0x4e001c00U
#define AARCH64_SIMD_EXT_MASK 0xbfe08400U
#define AARCH64_SIMD_EXT_PATTERN 0x2e000000U
#define AARCH64_SIMD_DUP_GPR_MASK 0xbfe0fc00U
#define AARCH64_SIMD_DUP_GPR_PATTERN 0x0e000c00U
#define AARCH64_SIMD_DUP_SCALAR_ELEMENT_MASK 0xffe0fc00U
#define AARCH64_SIMD_DUP_SCALAR_ELEMENT_PATTERN 0x5e000400U
#define AARCH64_SIMD_VECTOR_LOGICAL_MASK 0x9f20fc00U
#define AARCH64_SIMD_VECTOR_LOGICAL_PATTERN 0x0e201c00U
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
#define AARCH64_SIMD_CLS_CLZ_PATTERN 0x0e204800U
#define AARCH64_SIMD_REV_PATTERN 0x0e200800U
#define AARCH64_SIMD_REV16_PATTERN 0x0e201800U
#define AARCH64_SIMD_BIT_COUNT_PATTERN 0x0e205800U
#define AARCH64_SIMD_CMGT_ZERO_PATTERN 0x0e208800U
#define AARCH64_SIMD_CMEQ_ZERO_PATTERN 0x0e209800U
#define AARCH64_SIMD_CMLT_ZERO_PATTERN 0x0e20a800U
#define AARCH64_SIMD_FNEG_2D_MASK 0xfffffc00U
#define AARCH64_SIMD_FNEG_2D_PATTERN 0x6ee0f800U
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
#define AARCH64_SIMD_UMAXV_4H_MASK 0xfffffc20U
#define AARCH64_SIMD_UMAXV_4H_PATTERN 0x2e70a800U
#define AARCH64_SIMD_UMAXV_4S_MASK 0xfffffc20U
#define AARCH64_SIMD_UMAXV_4S_PATTERN 0x6eb0a800U
#define AARCH64_SIMD_ADDV_4S_MASK 0xfffffc20U
#define AARCH64_SIMD_ADDV_4S_PATTERN 0x4eb1b800U
#define AARCH64_SIMD_ADDP_D_2D_MASK 0xfffffc20U
#define AARCH64_SIMD_ADDP_D_2D_PATTERN 0x5ef1b800U
#define AARCH64_SIMD_LD1R_4S_MASK 0xfffffc00U
#define AARCH64_SIMD_LD1R_4S_PATTERN 0x4d40c800U
#define AARCH64_FMOV_W_S_MASK 0xfffffc00U
#define AARCH64_FMOV_W_S_PATTERN 0x1e260000U
#define AARCH64_FMOV_S_W_PATTERN 0x1e270000U
#define AARCH64_FMOV_X_D_PATTERN 0x9e660000U
#define AARCH64_FMOV_D_X_PATTERN 0x9e670000U
#define AARCH64_FMOV_IMMEDIATE_MASK 0xffe01fe0U
#define AARCH64_FMOV_S_IMMEDIATE_PATTERN 0x1e201000U
#define AARCH64_FMOV_D_IMMEDIATE_PATTERN 0x1e601000U
#define AARCH64_FMOV_S_S_MASK 0xfffffc00U
#define AARCH64_FMOV_S_S_PATTERN 0x1e204000U
#define AARCH64_FMOV_D_D_MASK 0xfffffc00U
#define AARCH64_FMOV_D_D_PATTERN 0x1e604000U
#define AARCH64_FCVT_D_S_MASK 0xfffffc00U
#define AARCH64_FCVT_D_S_PATTERN 0x1e22c000U
#define AARCH64_FABS_S_MASK 0xfffffc00U
#define AARCH64_FABS_S_PATTERN 0x1e20c000U
#define AARCH64_FABS_D_MASK 0xfffffc00U
#define AARCH64_FABS_D_PATTERN 0x1e60c000U
#define AARCH64_FNEG_S_MASK 0xfffffc00U
#define AARCH64_FNEG_S_PATTERN 0x1e214000U
#define AARCH64_FNEG_D_MASK 0xfffffc00U
#define AARCH64_FNEG_D_PATTERN 0x1e614000U
#define AARCH64_FDIV_S_MASK 0xff20fc00U
#define AARCH64_FDIV_S_PATTERN 0x1e201800U
#define AARCH64_FDIV_D_MASK 0xff20fc00U
#define AARCH64_FDIV_D_PATTERN 0x1e601800U
#define AARCH64_FADD_S_MASK 0xff20fc00U
#define AARCH64_FADD_S_PATTERN 0x1e202800U
#define AARCH64_FADD_D_MASK 0xff20fc00U
#define AARCH64_FADD_D_PATTERN 0x1e602800U
#define AARCH64_FSUB_S_MASK 0xff20fc00U
#define AARCH64_FSUB_S_PATTERN 0x1e203800U
#define AARCH64_FSUB_D_MASK 0xff20fc00U
#define AARCH64_FSUB_D_PATTERN 0x1e603800U
#define AARCH64_FMUL_S_MASK 0xff60fc00U
#define AARCH64_FMUL_S_PATTERN 0x1e200800U
#define AARCH64_FMUL_D_MASK 0xff60fc00U
#define AARCH64_FMUL_D_PATTERN 0x1e600800U
#define AARCH64_FMUL_2D_MASK 0xff20fc00U
#define AARCH64_FMUL_2D_PATTERN 0x6e20dc00U
#define AARCH64_FP_SCALAR_3SOURCE_MASK 0xff000000U
#define AARCH64_FP_SCALAR_3SOURCE_PATTERN 0x1f000000U
#define AARCH64_FCSEL_MASK 0xffa00c00U
#define AARCH64_FCSEL_PATTERN 0x1e200c00U
#define AARCH64_FCMP_S_MASK 0xffe0fc1fU
#define AARCH64_FCMP_S_PATTERN 0x1e202000U
#define AARCH64_FCMP_D_MASK 0xffe0fc1fU
#define AARCH64_FCMP_D_PATTERN 0x1e602000U
#define AARCH64_FCMP_S_ZERO_PATTERN 0x1e202008U
#define AARCH64_FCMP_D_ZERO_PATTERN 0x1e602008U
#define AARCH64_SCVTF_S_W_MASK 0xfffffc00U
#define AARCH64_SCVTF_S_W_PATTERN 0x1e220000U
#define AARCH64_SCVTF_D_W_MASK 0xfffffc00U
#define AARCH64_SCVTF_D_W_PATTERN 0x1e620000U
#define AARCH64_UCVTF_S_GPR_MASK 0x7ffffc00U
#define AARCH64_UCVTF_S_GPR_PATTERN 0x1e230000U
#define AARCH64_UCVTF_D_GPR_MASK 0x7ffffc00U
#define AARCH64_UCVTF_D_GPR_PATTERN 0x1e630000U
#define AARCH64_FCVTZS_D_GPR_MASK 0x7ffffc00U
#define AARCH64_FCVTZS_D_GPR_PATTERN 0x1e780000U
#define AARCH64_FCVTZU_GPR_S_MASK 0x7ffffc00U
#define AARCH64_FCVTZU_GPR_S_PATTERN 0x1e390000U
#define AARCH64_FCVTZU_X_D_MASK 0xfffffc00U
#define AARCH64_FCVTZU_X_D_PATTERN 0x9e790000U
#define AARCH64_FCVTZU_W_D_MASK 0xfffffc00U
#define AARCH64_FCVTZU_W_D_PATTERN 0x1e790000U
#define AARCH64_FCVTZU_X_D_FIXED_MASK 0xffff0000U
#define AARCH64_FCVTZU_X_D_FIXED_PATTERN 0x9e590000U
#define AARCH64_FCVTZU_D_D_MASK 0xfffffc00U
#define AARCH64_FCVTZU_D_D_PATTERN 0x7ee1b800U
#define AARCH64_UCVTF_D_D_MASK 0xfffffc00U
#define AARCH64_UCVTF_D_D_PATTERN 0x7e61d800U
#define AARCH64_SCVTF_D_D_MASK 0xfffffc00U
#define AARCH64_SCVTF_D_D_PATTERN 0x5e61d800U
#define AARCH64_UCVTF_2D_2D_MASK 0xfffffc00U
#define AARCH64_UCVTF_2D_2D_PATTERN 0x6e61d800U
#define AARCH64_SYSTEM_REGISTER_MASK 0xfff00000U
#define AARCH64_MRS_PATTERN 0xd5300000U
#define AARCH64_MSR_PATTERN 0xd5100000U
#define AARCH64_SYSREG_TPIDR_EL0 0xde82U
#define AARCH64_SYSREG_NZCV 0xda10U
#define AARCH64_SYSREG_FPCR 0xda20U
#define AARCH64_SYSREG_FPSR 0xda21U

static u32 tcti_bits(u32 value, u8 shift, u8 width)
{
	return (value >> shift) & ((1U << width) - 1U);
}

static u8 tcti_simd_modified_imm8(u32 instruction)
{
	return (((instruction >> 16) & 0x7U) << 5) |
	       ((instruction >> 5) & 0x1fU);
}

static u64 tcti_replicate_u32(u32 value)
{
	return (u64)value | ((u64)value << 32);
}

static u64 tcti_replicate_u16(u16 value)
{
	u64 pattern = value;

	pattern |= pattern << 16;
	return pattern | (pattern << 32);
}

static u64 tcti_replicate_u8(u8 value)
{
	u64 pattern = value;

	pattern |= pattern << 8;
	pattern |= pattern << 16;
	return pattern | (pattern << 32);
}

static u64 tcti_expand_simd_modified_bitmask(u8 imm8)
{
	u64 pattern = 0;
	u8 byte;

	for (byte = 0; byte < 8; byte++) {
		if (imm8 & BIT(byte))
			pattern |= 0xffULL << (byte * 8);
	}

	return pattern;
}

static u32 tcti_expand_simd_fp32_immediate(u8 imm8)
{
	u32 sign = (imm8 >> 7) & 1U;
	u32 exponent_bit = (imm8 >> 6) & 1U;
	u32 fraction = imm8 & 0x3fU;

	return (sign << 31) | ((!exponent_bit) << 30) |
	       ((exponent_bit ? 0x1fU : 0) << 25) | (fraction << 19);
}

static u64 tcti_expand_simd_fp64_immediate(u8 imm8)
{
	u64 sign = (imm8 >> 7) & 1U;
	u64 exponent_bit = (imm8 >> 6) & 1U;
	u64 fraction = imm8 & 0x3fU;

	return (sign << 63) | ((u64)!exponent_bit << 62) |
	       ((exponent_bit ? 0xffULL : 0) << 54) | (fraction << 48);
}

static bool tcti_decode_simd_modified_immediate(
	u32 instruction, struct tcti_decoded_instruction *decoded)
{
	u8 cmode = (instruction >> 12) & 0xfU;
	u8 imm8 = tcti_simd_modified_imm8(instruction);
	bool q = instruction & BIT(30);
	bool op = instruction & BIT(29);
	u64 pattern;

	decoded->decode_class = TCTI_DECODE_SIMD_MODIFIED_IMMEDIATE;
	decoded->rd = instruction & 0x1fU;
	decoded->result_size = q ? 2 * sizeof(u64) : sizeof(u64);
	decoded->access_size = decoded->result_size;
	decoded->simd_fp = true;
	decoded->simd_modified_immediate_op = TCTI_SIMD_MODIMM_MOVI;

	if (cmode <= 7) {
		u8 base_cmode = cmode & ~1U;
		u32 lane = (u32)imm8 << ((base_cmode >> 1) * 8);

		pattern = tcti_replicate_u32(lane);
		decoded->simd_modified_immediate_op = (cmode & 1U) ?
			(op ? TCTI_SIMD_MODIMM_BIC : TCTI_SIMD_MODIMM_ORR) :
			(op ? TCTI_SIMD_MODIMM_MVNI : TCTI_SIMD_MODIMM_MOVI);
	} else if (cmode <= 11) {
		u8 base_cmode = cmode & ~1U;
		u16 lane = (u16)imm8 << (((base_cmode - 8) >> 1) * 8);

		pattern = tcti_replicate_u16(lane);
		decoded->simd_modified_immediate_op = (cmode & 1U) ?
			(op ? TCTI_SIMD_MODIMM_BIC : TCTI_SIMD_MODIMM_ORR) :
			(op ? TCTI_SIMD_MODIMM_MVNI : TCTI_SIMD_MODIMM_MOVI);
	} else if (cmode == 12 || cmode == 13) {
		u8 shift = cmode == 12 ? 8 : 16;
		u32 lane = ((u32)imm8 << shift) | (BIT(shift) - 1U);

		pattern = tcti_replicate_u32(lane);
		decoded->simd_modified_immediate_op = op ?
			TCTI_SIMD_MODIMM_MVNI : TCTI_SIMD_MODIMM_MOVI;
	} else if (cmode == 14 && !op) {
		pattern = tcti_replicate_u8(imm8);
	} else if (cmode == 14) {
		pattern = tcti_expand_simd_modified_bitmask(imm8);
	} else if (!op) {
		u32 lane = tcti_expand_simd_fp32_immediate(imm8);

		pattern = tcti_replicate_u32(lane);
	} else {
		if (!q)
			return false;
		pattern = tcti_expand_simd_fp64_immediate(imm8);
	}

	decoded->logical_immediate = pattern;
	return true;
}

static bool tcti_decode_load_store_variant(u8 size, u8 opc, bool *load,
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

static bool tcti_decode_simd_fp_load_store_variant(u8 size, u8 opc,
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

static u64 tcti_ror_width(u64 value, u8 rotate, u8 width)
{
	u64 mask = width == 64 ? ~0ULL : (BIT_ULL(width) - 1);

	rotate %= width;
	value &= mask;
	if (!rotate)
		return value;

	return ((value >> rotate) | (value << (width - rotate))) & mask;
}

static u64 tcti_expand_fp_immediate(u8 imm8, u8 exponent_bits,
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

static bool tcti_decode_logical_immediate_mask(bool is_64bit, bool n, u8 immr,
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
	element = tcti_ror_width(element, immr & levels, size);

	for (offset = 0; offset < reg_width; offset += size)
		result |= element << offset;

	*out = result;
	return true;
}

static void tcti_decode_memory_common(struct tcti_decoded_instruction *decoded,
				      u32 instruction, u8 size, u8 opc)
{
	decoded->rt = instruction & 0x1fU;
	decoded->rn = (instruction >> 5) & 0x1fU;
	tcti_decode_load_store_variant(size, opc, &decoded->load,
				       &decoded->sign_extend_load,
				       &decoded->access_size,
				       &decoded->result_size);
}

struct tcti_decoded_instruction tcti_decode_aarch64(u32 instruction)
{
	struct tcti_decoded_instruction decoded = {
		.decode_class = TCTI_DECODE_UNSUPPORTED,
		.instruction = instruction,
	};

	if ((instruction & AARCH64_SVC_MASK) == AARCH64_SVC_PATTERN) {
		decoded.decode_class = TCTI_DECODE_SVC;
		return decoded;
	}

	if ((instruction & AARCH64_HINT_MASK) == AARCH64_HINT_PATTERN) {
		decoded.decode_class = TCTI_DECODE_HINT;
		return decoded;
	}
	if ((instruction & AARCH64_FP_SCALAR_3SOURCE_MASK) ==
	    AARCH64_FP_SCALAR_3SOURCE_PATTERN) {
		decoded.decode_class = TCTI_DECODE_FP_SCALAR_3SOURCE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.ra = (instruction >> 10) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = instruction & BIT(22) ? sizeof(u64) : sizeof(u32);
		decoded.result_size = decoded.access_size;
		decoded.simd_fp = true;
		decoded.fp3_op = (enum tcti_fp_scalar_3source_op)(
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
		decoded.decode_class = TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = BIT(size);
		decoded.result_size = decoded.access_size;
		decoded.simd_scalar = true;
		decoded.simd_source_index = imm5 >> (size + 1);
		decoded.simd_element_move_op = TCTI_SIMD_ELEMENT_MOVE_DUP;
		return decoded;
	}

	if ((instruction & AARCH64_PC_RELATIVE_ADDRESS_MASK) ==
	    AARCH64_PC_RELATIVE_ADDRESS_PATTERN) {
		u64 imm = (tcti_bits(instruction, 5, 19) << 2) |
			  tcti_bits(instruction, 29, 2);

		decoded.decode_class = TCTI_DECODE_PC_RELATIVE_ADDRESS;
		decoded.rd = instruction & 0x1fU;
		decoded.page_relative = instruction & BIT(31);
		decoded.pc_relative_imm = sign_extend64(imm, 20);
		if (decoded.page_relative)
			decoded.pc_relative_imm <<= 12;
		return decoded;
	}

	if ((instruction & AARCH64_ADD_SUB_IMM_MASK) ==
	    AARCH64_ADD_SUB_IMM_PATTERN) {
		u8 shift = (instruction >> 22) & 0x3U;

		if (shift > 1)
			return decoded;

		decoded.decode_class = TCTI_DECODE_ADD_SUB_IMMEDIATE;
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

		decoded.decode_class = TCTI_DECODE_ADD_SUB_SHIFTED_REGISTER;
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

		decoded.decode_class = TCTI_DECODE_ADD_SUB_EXTENDED_REGISTER;
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
		decoded.decode_class = TCTI_DECODE_ADD_SUB_WITH_CARRY;
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
		decoded.decode_class = TCTI_DECODE_UNCONDITIONAL_BRANCH_IMMEDIATE;
		decoded.branch_imm =
			sign_extend64(instruction & 0x03ffffffU, 25) << 2;
		decoded.link = instruction & BIT(31);
		return decoded;
	}

	if ((instruction & AARCH64_BRANCH_REGISTER_MASK) == AARCH64_BR_PATTERN ||
	    (instruction & AARCH64_BRANCH_REGISTER_MASK) == AARCH64_BLR_PATTERN ||
	    (instruction & AARCH64_BRANCH_REGISTER_MASK) == AARCH64_RET_PATTERN) {
		u32 pattern = instruction & AARCH64_BRANCH_REGISTER_MASK;

		decoded.decode_class = TCTI_DECODE_UNCONDITIONAL_BRANCH_REGISTER;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.link = pattern == AARCH64_BLR_PATTERN;
		decoded.branch_register_op =
			pattern == AARCH64_BR_PATTERN ? TCTI_BRANCH_REGISTER_BR :
			pattern == AARCH64_BLR_PATTERN ? TCTI_BRANCH_REGISTER_BLR :
							  TCTI_BRANCH_REGISTER_RET;
		return decoded;
	}

	if ((instruction & AARCH64_COMPARE_BRANCH_IMM_MASK) ==
	    AARCH64_COMPARE_BRANCH_IMM_PATTERN) {
		u64 imm = ((instruction >> 5) & 0x7ffffU);

		decoded.decode_class = TCTI_DECODE_COMPARE_BRANCH_IMMEDIATE;
		decoded.rt = instruction & 0x1fU;
		decoded.is_64bit = instruction & BIT(31);
		decoded.nonzero = instruction & BIT(24);
		decoded.branch_imm = sign_extend64(imm, 18) << 2;
		return decoded;
	}

	if ((instruction & AARCH64_TEST_BRANCH_IMM_MASK) ==
	    AARCH64_TEST_BRANCH_IMM_PATTERN) {
		u8 bit_5 = instruction & BIT(31) ? BIT(5) : 0;
		u64 imm = (instruction >> 5) & 0x3fffU;

		decoded.decode_class = TCTI_DECODE_TEST_BRANCH_IMMEDIATE;
		decoded.rt = instruction & 0x1fU;
		decoded.nonzero = instruction & BIT(24);
		decoded.test_bit = bit_5 | ((instruction >> 19) & 0x1fU);
		decoded.branch_imm = sign_extend64(imm, 13) << 2;
		return decoded;
	}

	if ((instruction & AARCH64_CONDITIONAL_BRANCH_IMM_MASK) ==
	    AARCH64_CONDITIONAL_BRANCH_IMM_PATTERN) {
		u64 imm = (instruction >> 5) & 0x7ffffU;

		decoded.decode_class = TCTI_DECODE_CONDITIONAL_BRANCH_IMMEDIATE;
		decoded.condition = instruction & 0xfU;
		decoded.branch_imm = sign_extend64(imm, 18) << 2;
		return decoded;
	}

	if ((instruction & AARCH64_CONDITIONAL_COMPARE_MASK) ==
	    AARCH64_CONDITIONAL_COMPARE_PATTERN) {
		decoded.decode_class = TCTI_DECODE_CONDITIONAL_COMPARE;
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

		decoded.decode_class = TCTI_DECODE_CONDITIONAL_SELECT;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.condition = (instruction >> 12) & 0xfU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.is_64bit = instruction & BIT(31);
		decoded.conditional_select_op =
			!op && !op2 ? TCTI_CONDITIONAL_SELECT_CSEL :
			!op && op2 ? TCTI_CONDITIONAL_SELECT_CSINC :
			op && !op2 ? TCTI_CONDITIONAL_SELECT_CSINV :
				     TCTI_CONDITIONAL_SELECT_CSNEG;
		return decoded;
	}

	if ((instruction & AARCH64_LOAD_STORE_PAIR_MASK) ==
	    AARCH64_LOAD_STORE_PAIR_PATTERN) {
		u8 opc = (instruction >> 30) & 0x3U;
		u8 mode = (instruction >> 23) & 0x3U;
		bool simd_fp = instruction & BIT(26);
		u8 scale;

		if (mode == 0)
			return decoded;

		if (simd_fp) {
			if (opc != 1 && opc != 2)
				return decoded;
			scale = opc == 2 ? 4 : 3;
		} else {
			if (opc == 0) {
				scale = 2;
			} else if (opc == 1) {
				if (!(instruction & BIT(22)))
					return decoded;
				scale = 2;
				decoded.sign_extend_load = true;
			} else if (opc == 2) {
				scale = 3;
			} else {
				return decoded;
			}
		}

		decoded.decode_class = TCTI_DECODE_LOAD_STORE_PAIR;
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
			mode == 1 ? TCTI_MEMORY_INDEX_POST :
			mode == 2 ? TCTI_MEMORY_INDEX_SIGNED_OFFSET :
				    TCTI_MEMORY_INDEX_PRE;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_LD1R_4S_MASK) ==
	    AARCH64_SIMD_LD1R_4S_PATTERN) {
		decoded.decode_class = TCTI_DECODE_SIMD_LOAD_REPLICATE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.load = true;
		decoded.simd_fp = true;
		decoded.access_size = sizeof(u32);
		decoded.result_size = 2 * sizeof(u64);
		return decoded;
	}

	if ((instruction & AARCH64_LOAD_STORE_UNSIGNED_IMM_MASK) ==
	    AARCH64_LOAD_STORE_UNSIGNED_IMM_PATTERN) {
		u8 size = (instruction >> 30) & 0x3U;
		u8 opc = (instruction >> 22) & 0x3U;
		bool simd_fp = instruction & BIT(26);
		u8 scale = size;

		if (simd_fp) {
			if (size == 2 && opc <= 1) {
				decoded.load = opc == 1;
				decoded.access_size = sizeof(u32);
				decoded.result_size = sizeof(u32);
				scale = 2;
			} else if (size == 3 && opc <= 1) {
				decoded.load = opc == 1;
				decoded.access_size = sizeof(u64);
				decoded.result_size = sizeof(u64);
				scale = 3;
			} else if (size == 0 && opc >= 2) {
				decoded.load = opc == 3;
				decoded.access_size = 2 * sizeof(u64);
				decoded.result_size = 2 * sizeof(u64);
				scale = 4;
			} else {
				return decoded;
			}

			decoded.decode_class =
				TCTI_DECODE_LOAD_STORE_UNSIGNED_IMMEDIATE;
			decoded.rt = instruction & 0x1fU;
			decoded.rn = (instruction >> 5) & 0x1fU;
			decoded.simd_fp = true;
			decoded.memory_offset =
				((instruction >> 10) & 0xfffU) << scale;
			decoded.memory_index_mode =
				TCTI_MEMORY_INDEX_SIGNED_OFFSET;
			return decoded;
		}

		if (!tcti_decode_load_store_variant(size, opc, &decoded.load,
						    &decoded.sign_extend_load,
						    &decoded.access_size,
						    &decoded.result_size))
			return decoded;

		decoded.decode_class = TCTI_DECODE_LOAD_STORE_UNSIGNED_IMMEDIATE;
		decoded.rt = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.memory_offset =
			((instruction >> 10) & 0xfffU) << size;
		decoded.memory_index_mode = TCTI_MEMORY_INDEX_SIGNED_OFFSET;
		return decoded;
	}

	if ((instruction & AARCH64_LOAD_STORE_SIGNED_IMM_MASK) ==
	    AARCH64_LOAD_STORE_SIGNED_IMM_PATTERN) {
		u8 size = (instruction >> 30) & 0x3U;
		u8 opc = (instruction >> 22) & 0x3U;
		u8 mode = (instruction >> 10) & 0x3U;
		bool simd_fp = instruction & BIT(26);

		if (mode == 2)
			return decoded;

		if (simd_fp) {
			if (!tcti_decode_simd_fp_load_store_variant(
					size, opc, &decoded.load,
					&decoded.access_size, &decoded.result_size))
				return decoded;

			decoded.decode_class =
				TCTI_DECODE_LOAD_STORE_SIGNED_IMMEDIATE;
			decoded.rt = instruction & 0x1fU;
			decoded.rn = (instruction >> 5) & 0x1fU;
			decoded.simd_fp = true;
			decoded.memory_offset =
				sign_extend64((instruction >> 12) & 0x1ffU, 8);
			decoded.memory_index_mode =
				mode == 0 ? TCTI_MEMORY_INDEX_SIGNED_OFFSET :
				mode == 1 ? TCTI_MEMORY_INDEX_POST :
					    TCTI_MEMORY_INDEX_PRE;
			return decoded;
		}

		if (!tcti_decode_load_store_variant(size, opc, &decoded.load,
						   &decoded.sign_extend_load,
						   &decoded.access_size,
						   &decoded.result_size))
			return decoded;

		decoded.decode_class = TCTI_DECODE_LOAD_STORE_SIGNED_IMMEDIATE;
		decoded.rt = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.memory_offset =
			sign_extend64((instruction >> 12) & 0x1ffU, 8);
		decoded.memory_index_mode =
			mode == 0 ? TCTI_MEMORY_INDEX_SIGNED_OFFSET :
			mode == 1 ? TCTI_MEMORY_INDEX_POST :
				    TCTI_MEMORY_INDEX_PRE;
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

		if (simd_fp) {
			if (!tcti_decode_simd_fp_load_store_variant(
				    size, opc, &decoded.load,
				    &decoded.access_size, &decoded.result_size))
				return decoded;
			decoded.simd_fp = true;
		} else if (!tcti_decode_load_store_variant(
				   size, opc, &decoded.load,
				   &decoded.sign_extend_load,
				   &decoded.access_size, &decoded.result_size)) {
			return decoded;
		}

		decoded.decode_class = TCTI_DECODE_LOAD_STORE_REGISTER_OFFSET;
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

		decoded.decode_class = TCTI_DECODE_LOGICAL_SHIFTED_REGISTER;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.shift = (instruction >> 22) & 0x3U;
		decoded.shift_amount = amount;
		decoded.is_64bit = is_64bit;
		decoded.invert_second_operand = instruction & BIT(21);
		decoded.set_flags = opc == 3;
		decoded.logical_op =
			opc == 0 ? TCTI_LOGICAL_AND :
			opc == 1 ? TCTI_LOGICAL_ORR :
			opc == 2 ? TCTI_LOGICAL_EOR : TCTI_LOGICAL_AND;
		return decoded;
	}

	if ((instruction & AARCH64_LOGICAL_IMMEDIATE_MASK) ==
	    AARCH64_LOGICAL_IMMEDIATE_PATTERN) {
		u8 opc = (instruction >> 29) & 0x3U;
		bool n = instruction & BIT(22);
		u8 immr = (instruction >> 16) & 0x3fU;
		u8 imms = (instruction >> 10) & 0x3fU;

		if (!tcti_decode_logical_immediate_mask(instruction & BIT(31),
						       n, immr, imms,
						       &decoded.logical_immediate))
			return decoded;

		decoded.decode_class = TCTI_DECODE_LOGICAL_IMMEDIATE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.is_64bit = instruction & BIT(31);
		decoded.set_flags = opc == 3;
		decoded.logical_op =
			opc == 0 ? TCTI_LOGICAL_AND :
			opc == 1 ? TCTI_LOGICAL_ORR :
			opc == 2 ? TCTI_LOGICAL_EOR : TCTI_LOGICAL_AND;
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

		decoded.decode_class = TCTI_DECODE_BITFIELD;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.is_64bit = sf;
		decoded.bitfield_immr = immr;
		decoded.bitfield_imms = imms;
		decoded.bitfield_op =
			opc == 0 ? TCTI_BITFIELD_SBFM :
			opc == 1 ? TCTI_BITFIELD_BFM : TCTI_BITFIELD_UBFM;
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

		decoded.decode_class = TCTI_DECODE_EXTRACT;
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

		if (opcode == 0x00U) {
			decoded.dp1_op = TCTI_DP1_RBIT;
		} else if (opcode == AARCH64_DP1_REV16_32_OPCODE &&
			   !(instruction & BIT(31))) {
			decoded.dp1_op = TCTI_DP1_REV16;
		} else if (opcode == AARCH64_DP1_REV32_OPCODE &&
			   !(instruction & BIT(31))) {
			decoded.dp1_op = TCTI_DP1_REV;
		} else if (opcode == AARCH64_DP1_CLZ_OPCODE) {
			decoded.dp1_op = TCTI_DP1_CLZ;
		} else {
			return decoded;
		}

		decoded.decode_class = TCTI_DECODE_DATA_PROCESSING_1SOURCE;
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
			decoded.dp2_op = TCTI_DP2_UDIV;
			break;
		case 0x03:
			decoded.dp2_op = TCTI_DP2_SDIV;
			break;
		case 0x08:
			decoded.dp2_op = TCTI_DP2_LSLV;
			break;
		case 0x09:
			decoded.dp2_op = TCTI_DP2_LSRV;
			break;
		case 0x0a:
			decoded.dp2_op = TCTI_DP2_ASRV;
			break;
		case 0x0b:
			decoded.dp2_op = TCTI_DP2_RORV;
			break;
		case 0x10 ... 0x17: {
			u8 size = opcode & 0x3U;

			if ((size == 3) != !!(instruction & BIT(31)))
				return decoded;
			decoded.dp2_op = opcode & BIT(2) ?
				TCTI_DP2_CRC32C : TCTI_DP2_CRC32;
			decoded.access_size = BIT(size);
			decoded.result_size = sizeof(u32);
			break;
		}
		default:
			return decoded;
		}

		decoded.decode_class = TCTI_DECODE_DATA_PROCESSING_2SOURCE;
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
			decoded.mul_op = subtract ? TCTI_MUL_MSUB :
						   TCTI_MUL_MADD;
			break;
		case 1:
			if (!is_64bit)
				return decoded;
			decoded.mul_op = subtract ? TCTI_MUL_SMSUBL :
						   TCTI_MUL_SMADDL;
			break;
		case 2:
			if (!is_64bit || subtract || ra != 31)
				return decoded;
			decoded.mul_op = TCTI_MUL_SMULH;
			break;
		case 5:
			if (!is_64bit)
				return decoded;
			decoded.mul_op = subtract ? TCTI_MUL_UMSUBL :
						   TCTI_MUL_UMADDL;
			break;
		case 6:
			if (!is_64bit || subtract || ra != 31)
				return decoded;
			decoded.mul_op = TCTI_MUL_UMULH;
			break;
		default:
			return decoded;
		}

		decoded.decode_class = TCTI_DECODE_MULTIPLY_ADD_SUB;
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

		decoded.decode_class = TCTI_DECODE_MOVE_WIDE_IMMEDIATE;
		decoded.rd = instruction & 0x1fU;
		decoded.imm16 = (instruction >> 5) & 0xffffU;
		decoded.halfword_shift = hw * 16;
		decoded.is_64bit = is_64bit;
		decoded.move_wide_op =
			opc == 0 ? TCTI_MOVE_WIDE_MOVN :
			opc == 2 ? TCTI_MOVE_WIDE_MOVZ : TCTI_MOVE_WIDE_MOVK;
		return decoded;
	}

	if (instruction == AARCH64_CLREX) {
		decoded.decode_class = TCTI_DECODE_EXCLUSIVE_MONITOR_CLEAR;
		return decoded;
	}

	if ((instruction & AARCH64_LOAD_STORE_EXCLUSIVE_MASK) ==
	    AARCH64_LOAD_STORE_EXCLUSIVE_PATTERN) {
		u8 size = (instruction >> 30) & 0x3U;
		bool load = instruction & BIT(22);
		bool ordered_nonexclusive = instruction & BIT(23);
		u8 rs = (instruction >> 16) & 0x1fU;

		if (!load && ordered_nonexclusive && rs != 31)
			return decoded;
		if (load && rs != 31)
			return decoded;

		decoded.decode_class = TCTI_DECODE_LOAD_STORE_EXCLUSIVE;
		decoded.rt = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rs = rs;
		decoded.access_size = BIT(size);
		decoded.result_size = decoded.access_size;
		decoded.load = load;
		decoded.exclusive = !ordered_nonexclusive;
		decoded.acquire = load && (instruction & BIT(15));
		decoded.release = !load && (instruction & BIT(15));
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

		decoded.decode_class = TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
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
			decoded.simd_arithmetic_op = TCTI_SIMD_ARITH_SQSHLU;
		else
			decoded.simd_arithmetic_op = instruction & BIT(29) ?
				TCTI_SIMD_ARITH_UQSHL : TCTI_SIMD_ARITH_SQSHL;
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

		decoded.decode_class = TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = access_size;
		decoded.result_size = scalar ? sizeof(u64) :
			(instruction & BIT(30) ? 2 * sizeof(u64) : sizeof(u64));
		decoded.simd_fp = true;
		decoded.simd_scalar = scalar;
		if (pattern == AARCH64_SIMD_SRI_PATTERN) {
			decoded.shift_amount = access_size * 16 - immediate;
			decoded.simd_arithmetic_op = TCTI_SIMD_ARITH_SRI;
		} else {
			decoded.shift_amount = immediate - access_size * 8;
			decoded.simd_arithmetic_op = instruction & BIT(29) ?
				TCTI_SIMD_ARITH_SLI : TCTI_SIMD_ARITH_SHL;
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

		decoded.decode_class = TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = access_size;
		decoded.result_size = scalar ? sizeof(u64) :
			(instruction & BIT(30) ? 2 * sizeof(u64) : sizeof(u64));
		decoded.shift_amount = access_size * 16 - immediate;
		decoded.simd_fp = true;
		decoded.simd_scalar = scalar;
		if (pattern == AARCH64_SIMD_SSHR_PATTERN)
			decoded.simd_arithmetic_op = u ? TCTI_SIMD_ARITH_USHR :
				TCTI_SIMD_ARITH_SSHR;
		else if (pattern == AARCH64_SIMD_SSRA_PATTERN)
			decoded.simd_arithmetic_op = u ? TCTI_SIMD_ARITH_USRA :
				TCTI_SIMD_ARITH_SSRA;
		else if (pattern == AARCH64_SIMD_SRSHR_PATTERN)
			decoded.simd_arithmetic_op = u ? TCTI_SIMD_ARITH_URSHR :
				TCTI_SIMD_ARITH_SRSHR;
		else
			decoded.simd_arithmetic_op = u ? TCTI_SIMD_ARITH_URSRA :
				TCTI_SIMD_ARITH_SRSRA;
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
		decoded.decode_class = TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = BIT(size);
		decoded.result_size = scalar ? decoded.access_size :
			(q ? 2 * sizeof(u64) : sizeof(u64));
		decoded.simd_fp = true;
		decoded.simd_scalar = scalar;
		decoded.simd_arithmetic_op = TCTI_SIMD_ARITH_SSHL + operation;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_SHIFT_LEFT_LONG_MASK) ==
	    AARCH64_SIMD_SHIFT_LEFT_LONG_PATTERN) {
		u8 immh = (instruction >> 19) & 0xfU;

		/* Reserved immh encodings overlap the modified-immediate mask. */
		if (!immh || (immh & 0x8U))
			return decoded;
	}

	if ((instruction & AARCH64_SIMD_MODIFIED_IMMEDIATE_MASK) ==
	    AARCH64_SIMD_MODIFIED_IMMEDIATE_PATTERN) {
		if (!tcti_decode_simd_modified_immediate(instruction, &decoded))
			decoded.decode_class = TCTI_DECODE_UNSUPPORTED;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_XTN_MASK) ==
	    AARCH64_SIMD_XTN_PATTERN) {
		u8 size = (instruction >> 22) & 0x3U;

		if (size == 3)
			return decoded;

		decoded.decode_class = TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = 2U << size;
		decoded.result_size = 1U << size;
		decoded.simd_destination_index = !!(instruction & BIT(30));
		decoded.simd_fp = true;
		decoded.simd_element_move_op = TCTI_SIMD_ELEMENT_MOVE_XTN;
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
		decoded.decode_class = TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.result_size = 2 * sizeof(u64);
		decoded.shift_amount = ((immh << 3) | immb) - element_bits;
		if (instruction & BIT(30))
			decoded.simd_source_index = sizeof(u64) /
						decoded.access_size;
		decoded.simd_fp = true;
		decoded.simd_element_move_op = instruction & BIT(29) ?
			TCTI_SIMD_ELEMENT_MOVE_USHLL :
			TCTI_SIMD_ELEMENT_MOVE_SSHLL;
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
			decoded.simd_element_move_op = TCTI_SIMD_ELEMENT_MOVE_UZP1;
			break;
		case 5:
			decoded.simd_element_move_op = TCTI_SIMD_ELEMENT_MOVE_UZP2;
			break;
		case 2:
			decoded.simd_element_move_op = TCTI_SIMD_ELEMENT_MOVE_TRN1;
			break;
		case 6:
			decoded.simd_element_move_op = TCTI_SIMD_ELEMENT_MOVE_TRN2;
			break;
		case 3:
			decoded.simd_element_move_op = TCTI_SIMD_ELEMENT_MOVE_ZIP1;
			break;
		case 7:
			decoded.simd_element_move_op = TCTI_SIMD_ELEMENT_MOVE_ZIP2;
			break;
		default:
			return decoded;
		}

		decoded.decode_class = TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE;
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

		decoded.decode_class = TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = BIT(lane_shift);
		decoded.result_size = q ? sizeof(u64) : sizeof(u32);
		decoded.simd_source_index = imm5 >> (lane_shift + 1);
		decoded.simd_fp = true;
		decoded.simd_element_move_op = TCTI_SIMD_ELEMENT_MOVE_UMOV;
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

		decoded.decode_class = TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = BIT(lane_shift);
		decoded.result_size = decoded.access_size;
		decoded.simd_destination_index = imm5 >> (lane_shift + 1);
		decoded.simd_fp = true;
		decoded.simd_element_move_op = TCTI_SIMD_ELEMENT_MOVE_INS_GPR;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_EXT_MASK) ==
	    AARCH64_SIMD_EXT_PATTERN) {
		u8 q = !!(instruction & BIT(30));

		if (!q && (instruction & BIT(14)))
			return decoded;

		decoded.decode_class = TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.shift_amount = (instruction >> 11) & 0xfU;
		decoded.access_size = q ? 2 * sizeof(u64) : sizeof(u64);
		decoded.result_size = decoded.access_size;
		decoded.simd_fp = true;
		decoded.simd_element_move_op = TCTI_SIMD_ELEMENT_MOVE_EXT;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_VECTOR_ELEMENT_MOVE_MASK) ==
	    AARCH64_SIMD_VECTOR_ELEMENT_MOVE_PATTERN) {
		u8 imm5 = (instruction >> 16) & 0x1fU;
		u8 imm4 = (instruction >> 11) & 0xfU;

		if (!((imm5 == 8 && imm4 == 0) ||
		      (imm5 == 20 && imm4 == 8) ||
		      (imm5 == 24 && imm4 == 0)))
			return decoded;

		decoded.decode_class = TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = imm5 == 20 ? sizeof(u32) : sizeof(u64);
		decoded.result_size = decoded.access_size;
		if (imm5 == 24) {
			decoded.simd_destination_index = 1;
			decoded.simd_source_index = 0;
		} else {
			decoded.simd_destination_index = imm5 == 20 ? 2 : 0;
			decoded.simd_source_index = imm4 == 8 ? 2 : 0;
		}
		decoded.simd_fp = true;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_DUP_GPR_MASK) ==
	    AARCH64_SIMD_DUP_GPR_PATTERN) {
		u8 imm5 = (instruction >> 16) & 0x1fU;
		bool q = instruction & BIT(30);

		if (!imm5 || (imm5 & (imm5 - 1)) || imm5 > sizeof(u64) ||
		    (imm5 == sizeof(u64) && !q))
			return decoded;

		decoded.decode_class = TCTI_DECODE_SIMD_VECTOR_ELEMENT_MOVE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = imm5;
		decoded.result_size = q ? 2 * sizeof(u64) : sizeof(u64);
		decoded.simd_fp = true;
		decoded.immediate = true;
		decoded.simd_element_move_op = TCTI_SIMD_ELEMENT_MOVE_DUP;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_VECTOR_LOGICAL_MASK) ==
	    AARCH64_SIMD_VECTOR_LOGICAL_PATTERN) {
		u8 operation = ((instruction >> 22) & 0x3U) |
			       ((instruction >> 27) & 0x4U);

		decoded.decode_class = TCTI_DECODE_SIMD_VECTOR_LOGICAL;
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

	if ((instruction & AARCH64_SIMD_ADD_SUB_MASK) ==
	    AARCH64_SIMD_ADD_SUB_PATTERN ||
	    (instruction & AARCH64_SIMD_SCALAR_ADD_SUB_MASK) ==
	    AARCH64_SIMD_SCALAR_ADD_SUB_PATTERN) {
		u8 size = (instruction >> 22) & 0x3U;
		bool q = instruction & BIT(30);
		bool scalar = instruction & BIT(28);

		if ((scalar && size != 3) || (!scalar && !q && size == 3))
			return decoded;

		decoded.decode_class = TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = BIT(size);
		decoded.result_size = scalar ? sizeof(u64) :
			(q ? 2 * sizeof(u64) : sizeof(u64));
		decoded.simd_fp = true;
		decoded.simd_scalar = scalar;
		decoded.simd_arithmetic_op = instruction & BIT(29) ?
			TCTI_SIMD_ARITH_SUB : TCTI_SIMD_ARITH_ADD;
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

		decoded.decode_class = TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = BIT(size);
		decoded.result_size = q ? 2 * sizeof(u64) : sizeof(u64);
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = TCTI_SIMD_ARITH_SHADD + operation;
		return decoded;
	}
	if ((instruction & AARCH64_SIMD_HALVING_SUB_MASK) ==
	    AARCH64_SIMD_HALVING_SUB_PATTERN) {
		u8 size = (instruction >> 22) & 0x3U;
		bool q = instruction & BIT(30);

		if (size == 3)
			return decoded;

		decoded.decode_class = TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = BIT(size);
		decoded.result_size = q ? 2 * sizeof(u64) : sizeof(u64);
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = instruction & BIT(29) ?
			TCTI_SIMD_ARITH_UHSUB : TCTI_SIMD_ARITH_SHSUB;
		return decoded;
	}
	if ((instruction & AARCH64_SIMD_PAIRWISE_ADD_MASK) ==
	    AARCH64_SIMD_PAIRWISE_ADD_PATTERN) {
		u8 size = (instruction >> 22) & 0x3U;
		bool q = instruction & BIT(30);

		if (!q && size == 3)
			return decoded;

		decoded.decode_class = TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = BIT(size);
		decoded.result_size = q ? 2 * sizeof(u64) : sizeof(u64);
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = TCTI_SIMD_ARITH_ADDP;
		return decoded;
	}
	if ((instruction & AARCH64_SIMD_ADD_SUB_WIDE_MASK) ==
	    AARCH64_SIMD_ADD_SUB_WIDE_PATTERN) {
		u8 size = (instruction >> 22) & 0x3U;
		u8 operation = (instruction & BIT(13) ? 2 : 0) |
			!!(instruction & BIT(29));

		if (size == 3)
			return decoded;

		decoded.decode_class = TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = BIT(size);
		decoded.result_size = 2 * sizeof(u64);
		decoded.simd_source_index = instruction & BIT(30) ?
			sizeof(u64) / decoded.access_size : 0;
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = TCTI_SIMD_ARITH_SADDW + operation;
		return decoded;
	}
	if ((instruction & AARCH64_SIMD_ADD_SUB_LONG_MASK) ==
	    AARCH64_SIMD_ADD_SUB_LONG_PATTERN) {
		u8 size = (instruction >> 22) & 0x3U;
		u8 operation = (instruction & BIT(13) ? 2 : 0) |
			!!(instruction & BIT(29));

		if (size == 3)
			return decoded;

		decoded.decode_class = TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = BIT(size);
		decoded.result_size = 2 * sizeof(u64);
		decoded.simd_source_index = instruction & BIT(30) ?
			sizeof(u64) / decoded.access_size : 0;
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = TCTI_SIMD_ARITH_SADDL + operation;
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

		decoded.decode_class = TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = BIT(size);
		decoded.result_size = q ? 2 * sizeof(u64) : sizeof(u64);
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = TCTI_SIMD_ARITH_SMAXP + operation;
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

		decoded.decode_class = TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = BIT(size);
		decoded.result_size = scalar ? decoded.access_size :
			(q ? 2 * sizeof(u64) : sizeof(u64));
		decoded.simd_fp = true;
		decoded.simd_scalar = scalar;
		decoded.simd_arithmetic_op = TCTI_SIMD_ARITH_SQADD + operation;
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

		decoded.decode_class = TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = BIT(size);
		decoded.result_size = scalar ? decoded.access_size :
			(q ? 2 * sizeof(u64) : sizeof(u64));
		decoded.simd_fp = true;
		decoded.simd_scalar = scalar;
		decoded.simd_arithmetic_op = instruction & BIT(29) ?
			TCTI_SIMD_ARITH_SQRDMULH : TCTI_SIMD_ARITH_SQDMULH;
		return decoded;
	}
	if ((instruction & AARCH64_SIMD_AES_MASK) == AARCH64_SIMD_AES_PATTERN) {
		decoded.decode_class = TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = 2 * sizeof(u64);
		decoded.result_size = 2 * sizeof(u64);
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = TCTI_SIMD_ARITH_AESE +
					     ((instruction >> 12) & 0x3U);
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_SHA1_THREE_REGISTER_MASK) ==
	    AARCH64_SIMD_SHA1_THREE_REGISTER_PATTERN) {
		decoded.decode_class = TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = 2 * sizeof(u64);
		decoded.result_size = 2 * sizeof(u64);
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = TCTI_SIMD_ARITH_SHA1C +
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

		decoded.decode_class = TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = schedule ? 2 * sizeof(u64) : sizeof(u32);
		decoded.result_size = decoded.access_size;
		decoded.simd_fp = true;
		decoded.simd_scalar = !schedule;
		decoded.simd_arithmetic_op = schedule ? TCTI_SIMD_ARITH_SHA1SU1 :
						       TCTI_SIMD_ARITH_SHA1H;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_SHA256_THREE_REGISTER_MASK) ==
	    AARCH64_SIMD_SHA256_THREE_REGISTER_PATTERN) {
		u8 operation = (instruction >> 12) & 0x3U;

		if (operation == 3)
			return decoded;
		decoded.decode_class = TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = 2 * sizeof(u64);
		decoded.result_size = 2 * sizeof(u64);
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = TCTI_SIMD_ARITH_SHA256H + operation;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_SHA256SU0_MASK) ==
	    AARCH64_SIMD_SHA256SU0_PATTERN) {
		decoded.decode_class = TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = 2 * sizeof(u64);
		decoded.result_size = 2 * sizeof(u64);
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = TCTI_SIMD_ARITH_SHA256SU0;
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

		decoded.decode_class = TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = 2 * sizeof(u64);
		decoded.result_size = 2 * sizeof(u64);
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = TCTI_SIMD_ARITH_SHA512H + operation;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_SHA512SU0_MASK) ==
	    AARCH64_SIMD_SHA512SU0_PATTERN) {
		decoded.decode_class = TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = 2 * sizeof(u64);
		decoded.result_size = 2 * sizeof(u64);
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = TCTI_SIMD_ARITH_SHA512SU0;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_SM4E_MASK) ==
		    AARCH64_SIMD_SM4E_PATTERN ||
	    (instruction & AARCH64_SIMD_SM4EKEY_MASK) ==
		    AARCH64_SIMD_SM4EKEY_PATTERN) {
		bool key = (instruction & AARCH64_SIMD_SM4EKEY_MASK) ==
			   AARCH64_SIMD_SM4EKEY_PATTERN;

		decoded.decode_class = TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		if (key)
			decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = 2 * sizeof(u64);
		decoded.result_size = 2 * sizeof(u64);
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = key ? TCTI_SIMD_ARITH_SM4EKEY :
						   TCTI_SIMD_ARITH_SM4E;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_SM3SS1_MASK) ==
	    AARCH64_SIMD_SM3SS1_PATTERN) {
		decoded.decode_class = TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.ra = (instruction >> 10) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = 2 * sizeof(u64);
		decoded.result_size = 2 * sizeof(u64);
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = TCTI_SIMD_ARITH_SM3SS1;
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

		decoded.decode_class = TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.shift_amount = (instruction >> 12) & 0x3U;
		decoded.access_size = 2 * sizeof(u64);
		decoded.result_size = 2 * sizeof(u64);
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = TCTI_SIMD_ARITH_SM3TT1A +
					     operation;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_SM3PARTW_MASK) ==
		    AARCH64_SIMD_SM3PARTW1_PATTERN ||
	    (instruction & AARCH64_SIMD_SM3PARTW_MASK) ==
		    AARCH64_SIMD_SM3PARTW2_PATTERN) {
		bool second = (instruction & AARCH64_SIMD_SM3PARTW_MASK) ==
			      AARCH64_SIMD_SM3PARTW2_PATTERN;

		decoded.decode_class = TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = 2 * sizeof(u64);
		decoded.result_size = 2 * sizeof(u64);
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = second ? TCTI_SIMD_ARITH_SM3PARTW2 :
						      TCTI_SIMD_ARITH_SM3PARTW1;
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

		decoded.decode_class = TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = BIT(size);
		decoded.result_size = 2 * sizeof(u64);
		decoded.simd_source_index = !!(instruction & BIT(30));
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = TCTI_SIMD_ARITH_SMULL + operation;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_PMULL_MASK) ==
	    AARCH64_SIMD_PMULL_PATTERN) {
		u8 size = (instruction >> 22) & 0x3U;

		if (size != 0 && size != 3)
			return decoded;

		decoded.decode_class = TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = BIT(size);
		decoded.result_size = 2 * sizeof(u64);
		decoded.simd_source_index = !!(instruction & BIT(30));
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = TCTI_SIMD_ARITH_PMULL;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_MUL_PMUL_MASK) ==
	    AARCH64_SIMD_MUL_PMUL_PATTERN) {
		u8 size = (instruction >> 22) & 0x3U;
		bool polynomial = instruction & BIT(29);
		bool q = instruction & BIT(30);

		if ((polynomial && size != 0) || (!polynomial && size == 3))
			return decoded;

		decoded.decode_class = TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = BIT(size);
		decoded.result_size = q ? 2 * sizeof(u64) : sizeof(u64);
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = polynomial ?
			TCTI_SIMD_ARITH_PMUL : TCTI_SIMD_ARITH_MUL;
		return decoded;
	}
	if ((instruction & AARCH64_SIMD_MLA_MLS_MASK) ==
	    AARCH64_SIMD_MLA_MLS_PATTERN) {
		u8 size = (instruction >> 22) & 0x3U;
		bool q = instruction & BIT(30);

		if (size == 3)
			return decoded;

		decoded.decode_class = TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = BIT(size);
		decoded.result_size = q ? 2 * sizeof(u64) : sizeof(u64);
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = instruction & BIT(29) ?
			TCTI_SIMD_ARITH_MLS : TCTI_SIMD_ARITH_MLA;
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

		decoded.decode_class = TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = BIT(size);
		decoded.result_size = q ? 2 * sizeof(u64) : sizeof(u64);
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = TCTI_SIMD_ARITH_SMAX + operation;
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

		decoded.decode_class = TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = BIT(size);
		decoded.result_size = 2 * sizeof(u64);
		decoded.simd_source_index = instruction & BIT(30) ?
			sizeof(u64) / decoded.access_size : 0;
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = TCTI_SIMD_ARITH_SABDL + operation;
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

		decoded.decode_class = TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = 2U << size;
		decoded.result_size = q ? 2 * sizeof(u64) : sizeof(u64);
		decoded.simd_destination_index = q;
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = TCTI_SIMD_ARITH_ADDHN + operation;
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

		decoded.decode_class = TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
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
			decoded.simd_arithmetic_op = u ? TCTI_SIMD_ARITH_SQSHRUN :
				TCTI_SIMD_ARITH_SHRN;
		else if (pattern == AARCH64_SIMD_RSHRN_PATTERN)
			decoded.simd_arithmetic_op = u ? TCTI_SIMD_ARITH_SQRSHRUN :
				TCTI_SIMD_ARITH_RSHRN;
		else if (pattern == AARCH64_SIMD_SQSHRN_PATTERN)
			decoded.simd_arithmetic_op = u ? TCTI_SIMD_ARITH_UQSHRN :
				TCTI_SIMD_ARITH_SQSHRN;
		else
			decoded.simd_arithmetic_op = u ? TCTI_SIMD_ARITH_UQRSHRN :
				TCTI_SIMD_ARITH_SQRSHRN;
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

		decoded.decode_class = TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = 2U << size;
		decoded.result_size = scalar ? decoded.access_size / 2 :
			(q ? 2 * sizeof(u64) : sizeof(u64));
		decoded.simd_destination_index = scalar ? 0 : q;
		decoded.simd_fp = true;
		decoded.simd_scalar = scalar;
		if (pattern == sqxtun_pattern)
			decoded.simd_arithmetic_op = TCTI_SIMD_ARITH_SQXTUN;
		else
			decoded.simd_arithmetic_op = instruction & BIT(29) ?
				TCTI_SIMD_ARITH_UQXTN : TCTI_SIMD_ARITH_SQXTN;
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

		decoded.decode_class = TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = BIT(size);
		decoded.result_size = q ? 2 * sizeof(u64) : sizeof(u64);
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = TCTI_SIMD_ARITH_SABD + operation;
		return decoded;
	}
	if ((instruction & AARCH64_SIMD_SCALAR_TWO_REGISTER_MISC_MASK) ==
	    AARCH64_SIMD_ABS_NEG_PATTERN ||
	    (instruction & AARCH64_SIMD_SCALAR_TWO_REGISTER_MISC_MASK) ==
	    AARCH64_SIMD_SQABS_SQNEG_PATTERN ||
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
			    (pattern == AARCH64_SIMD_ABS_NEG_PATTERN && size != 3))
				return decoded;
		} else if ((!q && size == 3) ||
			   (pattern == AARCH64_SIMD_CLS_CLZ_PATTERN && size == 3)) {
			return decoded;
		}

		decoded.decode_class = TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = BIT(size);
		decoded.result_size = scalar ? decoded.access_size :
			(q ? 2 * sizeof(u64) : sizeof(u64));
		decoded.simd_fp = true;
		decoded.simd_scalar = scalar;
		if (pattern == AARCH64_SIMD_ABS_NEG_PATTERN)
			decoded.simd_arithmetic_op = u ? TCTI_SIMD_ARITH_NEG :
				TCTI_SIMD_ARITH_ABS;
		else if (pattern == AARCH64_SIMD_SQABS_SQNEG_PATTERN)
			decoded.simd_arithmetic_op = u ? TCTI_SIMD_ARITH_SQNEG :
				TCTI_SIMD_ARITH_SQABS;
		else
			decoded.simd_arithmetic_op = u ? TCTI_SIMD_ARITH_CLZ :
				TCTI_SIMD_ARITH_CLS;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_TWO_REGISTER_MISC_MASK) ==
	    AARCH64_SIMD_REV_PATTERN) {
		u8 size = (instruction >> 22) & 0x3U;
		bool u = instruction & BIT(29);

		if ((!u && size == 3) || (u && size > 1))
			return decoded;

		decoded.decode_class = TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = BIT(size);
		decoded.result_size = instruction & BIT(30) ?
			2 * sizeof(u64) : sizeof(u64);
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = u ? TCTI_SIMD_ARITH_REV32 :
			TCTI_SIMD_ARITH_REV64;
		return decoded;
	}

	if ((instruction & ~BIT(30) & 0xfffffc00U) ==
	    AARCH64_SIMD_REV16_PATTERN) {
		decoded.decode_class = TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = sizeof(u8);
		decoded.result_size = instruction & BIT(30) ?
			2 * sizeof(u64) : sizeof(u64);
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = TCTI_SIMD_ARITH_REV16;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_TWO_REGISTER_MISC_MASK) ==
	    AARCH64_SIMD_BIT_COUNT_PATTERN) {
		u8 size = (instruction >> 22) & 0x3U;
		bool u = instruction & BIT(29);

		if ((!u && size != 0) || (u && size > 1))
			return decoded;

		decoded.decode_class = TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = sizeof(u8);
		decoded.result_size = instruction & BIT(30) ?
			2 * sizeof(u64) : sizeof(u64);
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = !u ? TCTI_SIMD_ARITH_CNT :
			size == 0 ? TCTI_SIMD_ARITH_NOT : TCTI_SIMD_ARITH_RBIT;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_FNEG_2D_MASK) ==
	    AARCH64_SIMD_FNEG_2D_PATTERN) {
		decoded.decode_class = TCTI_DECODE_SIMD_VECTOR_ARITHMETIC;
		decoded.rd = instruction & 0x1f;
		decoded.rn = (instruction >> 5) & 0x1f;
		decoded.access_size = sizeof(u64);
		decoded.result_size = 2 * sizeof(u64);
		decoded.simd_fp = true;
		decoded.simd_arithmetic_op = TCTI_SIMD_ARITH_FNEG;
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
		enum tcti_simd_vector_compare_op operation;

		if (opcode == 6)
			operation = u ? TCTI_SIMD_COMPARE_CMHI :
				TCTI_SIMD_COMPARE_CMGT;
		else if (opcode == 7)
			operation = u ? TCTI_SIMD_COMPARE_CMHS :
				TCTI_SIMD_COMPARE_CMGE;
		else if (opcode == 17)
			operation = u ? TCTI_SIMD_COMPARE_CMEQ :
				TCTI_SIMD_COMPARE_CMTST;
		else
			goto not_simd_compare_register;

		if (scalar && (!q || size != 3))
			goto not_simd_compare_register;
		if (!scalar && !q && size == 3)
			return decoded;

		decoded.decode_class = TCTI_DECODE_SIMD_VECTOR_COMPARE;
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

		decoded.decode_class = TCTI_DECODE_SIMD_VECTOR_COMPARE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = BIT(size);
		decoded.result_size = (instruction & BIT(30)) ?
				      2 * sizeof(u64) : sizeof(u64);
		decoded.simd_fp = true;
		decoded.simd_compare_op = TCTI_SIMD_COMPARE_CMEQ;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_CMEQ_4S_MASK) ==
	    AARCH64_SIMD_CMEQ_4S_PATTERN) {
		decoded.decode_class = TCTI_DECODE_SIMD_VECTOR_COMPARE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = sizeof(u32);
		decoded.result_size = 2 * sizeof(u64);
		decoded.simd_fp = true;
		decoded.simd_compare_op = TCTI_SIMD_COMPARE_CMEQ;
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
		    (pattern == AARCH64_SIMD_CMLT_ZERO_PATTERN && u))
			return decoded;

		decoded.decode_class = TCTI_DECODE_SIMD_VECTOR_COMPARE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = BIT(size);
		decoded.result_size = scalar ? sizeof(u64) :
			(q ? 2 * sizeof(u64) : sizeof(u64));
		decoded.simd_fp = true;
		decoded.simd_scalar = scalar;
		decoded.immediate = true;
		if (pattern == AARCH64_SIMD_CMGT_ZERO_PATTERN)
			decoded.simd_compare_op = u ? TCTI_SIMD_COMPARE_CMGE :
				TCTI_SIMD_COMPARE_CMGT;
		else if (pattern == AARCH64_SIMD_CMEQ_ZERO_PATTERN)
			decoded.simd_compare_op = u ? TCTI_SIMD_COMPARE_CMLE :
				TCTI_SIMD_COMPARE_CMEQ;
		else
			decoded.simd_compare_op = TCTI_SIMD_COMPARE_CMLT;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_CMHI_2D_MASK) ==
	    AARCH64_SIMD_CMHI_2D_PATTERN) {
		decoded.decode_class = TCTI_DECODE_SIMD_VECTOR_COMPARE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = sizeof(u64);
		decoded.result_size = 2 * sizeof(u64);
		decoded.simd_fp = true;
		decoded.simd_compare_op = TCTI_SIMD_COMPARE_CMHI;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_UMAXV_4S_MASK) ==
	    AARCH64_SIMD_UMAXV_4S_PATTERN) {
		decoded.decode_class = TCTI_DECODE_SIMD_VECTOR_REDUCTION;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = sizeof(u32);
		decoded.result_size = sizeof(u32);
		decoded.simd_fp = true;
		decoded.simd_reduction_op = TCTI_SIMD_REDUCTION_UMAXV;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_UMAXV_4H_MASK) ==
	    AARCH64_SIMD_UMAXV_4H_PATTERN) {
		decoded.decode_class = TCTI_DECODE_SIMD_VECTOR_REDUCTION;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = sizeof(u16);
		decoded.result_size = sizeof(u16);
		decoded.simd_fp = true;
		decoded.simd_reduction_op = TCTI_SIMD_REDUCTION_UMAXV;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_ADDV_4S_MASK) ==
	    AARCH64_SIMD_ADDV_4S_PATTERN) {
		decoded.decode_class = TCTI_DECODE_SIMD_VECTOR_REDUCTION;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = sizeof(u32);
		decoded.result_size = sizeof(u32);
		decoded.simd_fp = true;
		decoded.simd_reduction_op = TCTI_SIMD_REDUCTION_ADDV;
		return decoded;
	}

	if ((instruction & AARCH64_SIMD_ADDP_D_2D_MASK) ==
	    AARCH64_SIMD_ADDP_D_2D_PATTERN) {
		decoded.decode_class = TCTI_DECODE_SIMD_VECTOR_REDUCTION;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = sizeof(u64);
		decoded.result_size = sizeof(u64);
		decoded.simd_fp = true;
		decoded.simd_reduction_op = TCTI_SIMD_REDUCTION_ADDP;
		return decoded;
	}

	if ((instruction & AARCH64_FMOV_W_S_MASK) == AARCH64_FMOV_W_S_PATTERN ||
	    (instruction & AARCH64_FMOV_W_S_MASK) == AARCH64_FMOV_X_D_PATTERN) {
		decoded.decode_class = TCTI_DECODE_FP_SCALAR_MOVE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size =
			(instruction & BIT(31)) ? sizeof(u64) : sizeof(u32);
		decoded.result_size = decoded.access_size;
		decoded.simd_fp = true;
		decoded.fp_move_op = TCTI_FP_MOVE_SIMD_TO_GPR;
		return decoded;
	}

	if ((instruction & AARCH64_FMOV_W_S_MASK) == AARCH64_FMOV_S_W_PATTERN ||
	    (instruction & AARCH64_FMOV_W_S_MASK) == AARCH64_FMOV_D_X_PATTERN) {
		decoded.decode_class = TCTI_DECODE_FP_SCALAR_MOVE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size =
			(instruction & BIT(31)) ? sizeof(u64) : sizeof(u32);
		decoded.result_size = decoded.access_size;
		decoded.simd_fp = true;
		decoded.fp_move_op = TCTI_FP_MOVE_GPR_TO_SIMD;
		return decoded;
	}

	if ((instruction & AARCH64_FMOV_IMMEDIATE_MASK) ==
		    AARCH64_FMOV_S_IMMEDIATE_PATTERN ||
	    (instruction & AARCH64_FMOV_IMMEDIATE_MASK) ==
		    AARCH64_FMOV_D_IMMEDIATE_PATTERN) {
		bool is_double = (instruction & AARCH64_FMOV_IMMEDIATE_MASK) ==
				 AARCH64_FMOV_D_IMMEDIATE_PATTERN;
		u8 imm8 = (instruction >> 13) & 0xffU;

		decoded.decode_class = TCTI_DECODE_SIMD_MODIFIED_IMMEDIATE;
		decoded.rd = instruction & 0x1fU;
		decoded.logical_immediate = is_double ?
			tcti_expand_fp_immediate(imm8, 11, 52) :
			tcti_expand_fp_immediate(imm8, 8, 23);
		decoded.access_size = is_double ? sizeof(u64) : sizeof(u32);
		decoded.result_size = decoded.access_size;
		decoded.simd_fp = true;
		return decoded;
	}

	if ((instruction & AARCH64_FMOV_S_S_MASK) == AARCH64_FMOV_S_S_PATTERN ||
	    (instruction & AARCH64_FMOV_D_D_MASK) == AARCH64_FMOV_D_D_PATTERN) {
		decoded.decode_class = TCTI_DECODE_FP_SCALAR_MOVE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size =
			(instruction & BIT(22)) ? sizeof(u64) : sizeof(u32);
		decoded.result_size = decoded.access_size;
		decoded.simd_fp = true;
		decoded.fp_move_op = TCTI_FP_MOVE_REGISTER;
		return decoded;
	}

	if ((instruction & AARCH64_FCVT_D_S_MASK) == AARCH64_FCVT_D_S_PATTERN) {
		decoded.decode_class = TCTI_DECODE_FP_SCALAR_1SOURCE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = sizeof(u32);
		decoded.result_size = sizeof(u64);
		decoded.simd_fp = true;
		decoded.fp1_op = TCTI_FP1_FCVT;
		return decoded;
	}

	if ((instruction & AARCH64_FABS_S_MASK) == AARCH64_FABS_S_PATTERN ||
	    (instruction & AARCH64_FABS_D_MASK) == AARCH64_FABS_D_PATTERN) {
		decoded.decode_class = TCTI_DECODE_FP_SCALAR_1SOURCE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size =
			(instruction & BIT(22)) ? sizeof(u64) : sizeof(u32);
		decoded.result_size = decoded.access_size;
		decoded.simd_fp = true;
		decoded.fp1_op = TCTI_FP1_FABS;
		return decoded;
	}

	if ((instruction & AARCH64_FNEG_S_MASK) == AARCH64_FNEG_S_PATTERN ||
	    (instruction & AARCH64_FNEG_D_MASK) == AARCH64_FNEG_D_PATTERN) {
		decoded.decode_class = TCTI_DECODE_FP_SCALAR_1SOURCE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size =
			(instruction & BIT(22)) ? sizeof(u64) : sizeof(u32);
		decoded.result_size = decoded.access_size;
		decoded.simd_fp = true;
		decoded.fp1_op = TCTI_FP1_FNEG;
		return decoded;
	}

	if ((instruction & AARCH64_FDIV_S_MASK) == AARCH64_FDIV_S_PATTERN ||
	    (instruction & AARCH64_FDIV_D_MASK) == AARCH64_FDIV_D_PATTERN) {
		decoded.decode_class = TCTI_DECODE_FP_SCALAR_2SOURCE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size =
			(instruction & BIT(22)) ? sizeof(u64) : sizeof(u32);
		decoded.result_size = decoded.access_size;
		decoded.simd_fp = true;
		decoded.fp2_op = TCTI_FP2_FDIV;
		return decoded;
	}

	if ((instruction & AARCH64_FADD_S_MASK) == AARCH64_FADD_S_PATTERN ||
	    (instruction & AARCH64_FADD_D_MASK) == AARCH64_FADD_D_PATTERN) {
		decoded.decode_class = TCTI_DECODE_FP_SCALAR_2SOURCE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size =
			(instruction & BIT(22)) ? sizeof(u64) : sizeof(u32);
		decoded.result_size = decoded.access_size;
		decoded.simd_fp = true;
		decoded.fp2_op = TCTI_FP2_FADD;
		return decoded;
	}

	if ((instruction & AARCH64_FSUB_S_MASK) == AARCH64_FSUB_S_PATTERN ||
	    (instruction & AARCH64_FSUB_D_MASK) == AARCH64_FSUB_D_PATTERN) {
		decoded.decode_class = TCTI_DECODE_FP_SCALAR_2SOURCE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size =
			(instruction & BIT(22)) ? sizeof(u64) : sizeof(u32);
		decoded.result_size = decoded.access_size;
		decoded.simd_fp = true;
		decoded.fp2_op = TCTI_FP2_FSUB;
		return decoded;
	}

	if ((instruction & AARCH64_FMUL_S_MASK) == AARCH64_FMUL_S_PATTERN ||
	    (instruction & AARCH64_FMUL_D_MASK) == AARCH64_FMUL_D_PATTERN) {
		decoded.decode_class = TCTI_DECODE_FP_SCALAR_2SOURCE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size =
			(instruction & BIT(22)) ? sizeof(u64) : sizeof(u32);
		decoded.result_size = decoded.access_size;
		decoded.simd_fp = true;
		decoded.fp2_op = TCTI_FP2_FMUL;
		return decoded;
	}

	if ((instruction & AARCH64_FMUL_2D_MASK) == AARCH64_FMUL_2D_PATTERN) {
		decoded.decode_class = TCTI_DECODE_FP_SCALAR_2SOURCE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size = sizeof(u64);
		decoded.result_size = 2 * sizeof(u64);
		decoded.simd_fp = true;
		decoded.fp2_op = TCTI_FP2_FMUL;
		return decoded;
	}

	if ((instruction & AARCH64_FCSEL_MASK) == AARCH64_FCSEL_PATTERN) {
		decoded.decode_class = TCTI_DECODE_FP_CONDITIONAL_SELECT;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.condition = (instruction >> 12) & 0xfU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size =
			(instruction & BIT(22)) ? sizeof(u64) : sizeof(u32);
		decoded.result_size = decoded.access_size;
		decoded.simd_fp = true;
		return decoded;
	}

	if ((instruction & AARCH64_FCMP_S_MASK) == AARCH64_FCMP_S_PATTERN ||
	    (instruction & AARCH64_FCMP_D_MASK) == AARCH64_FCMP_D_PATTERN ||
	    (instruction & AARCH64_FCMP_S_MASK) == AARCH64_FCMP_S_ZERO_PATTERN ||
	    (instruction & AARCH64_FCMP_D_MASK) == AARCH64_FCMP_D_ZERO_PATTERN) {
		decoded.decode_class = TCTI_DECODE_FP_SCALAR_COMPARE;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.access_size =
			(instruction & BIT(22)) ? sizeof(u64) : sizeof(u32);
		decoded.result_size = decoded.access_size;
		decoded.immediate =
			(instruction & AARCH64_FCMP_S_MASK) ==
				AARCH64_FCMP_S_ZERO_PATTERN ||
			(instruction & AARCH64_FCMP_D_MASK) ==
				AARCH64_FCMP_D_ZERO_PATTERN;
		decoded.simd_fp = true;
		return decoded;
	}

	if ((instruction & AARCH64_SCVTF_S_W_MASK) == AARCH64_SCVTF_S_W_PATTERN ||
	    (instruction & AARCH64_SCVTF_D_W_MASK) == AARCH64_SCVTF_D_W_PATTERN) {
		decoded.decode_class = TCTI_DECODE_FP_INT_CONVERT;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = sizeof(u32);
		decoded.result_size =
			(instruction & BIT(22)) ? sizeof(u64) : sizeof(u32);
		decoded.simd_fp = true;
		decoded.fp_int_op = TCTI_FP_INT_SCVTF;
		return decoded;
	}

	if ((instruction & AARCH64_UCVTF_S_GPR_MASK) ==
	    AARCH64_UCVTF_S_GPR_PATTERN) {
		decoded.decode_class = TCTI_DECODE_FP_INT_CONVERT;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size =
			(instruction & BIT(31)) ? sizeof(u64) : sizeof(u32);
		decoded.result_size = sizeof(u32);
		decoded.simd_fp = true;
		decoded.fp_int_op = TCTI_FP_INT_UCVTF;
		return decoded;
	}

	if ((instruction & AARCH64_UCVTF_D_GPR_MASK) ==
	    AARCH64_UCVTF_D_GPR_PATTERN) {
		decoded.decode_class = TCTI_DECODE_FP_INT_CONVERT;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size =
			(instruction & BIT(31)) ? sizeof(u64) : sizeof(u32);
		decoded.result_size = sizeof(u64);
		decoded.simd_fp = true;
		decoded.fp_int_op = TCTI_FP_INT_UCVTF;
		return decoded;
	}

	if ((instruction & AARCH64_FCVTZS_D_GPR_MASK) ==
	    AARCH64_FCVTZS_D_GPR_PATTERN) {
		decoded.decode_class = TCTI_DECODE_FP_INT_CONVERT;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = sizeof(u64);
		decoded.result_size =
			(instruction & BIT(31)) ? sizeof(u64) : sizeof(u32);
		decoded.simd_fp = true;
		decoded.fp_int_op = TCTI_FP_INT_FCVTZS;
		return decoded;
	}

	if ((instruction & AARCH64_FCVTZU_GPR_S_MASK) ==
	    AARCH64_FCVTZU_GPR_S_PATTERN) {
		decoded.decode_class = TCTI_DECODE_FP_INT_CONVERT;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = sizeof(u32);
		decoded.result_size =
			(instruction & BIT(31)) ? sizeof(u64) : sizeof(u32);
		decoded.simd_fp = true;
		decoded.fp_int_op = TCTI_FP_INT_FCVTZU;
		return decoded;
	}

	if ((instruction & AARCH64_FCVTZU_X_D_MASK) ==
	    AARCH64_FCVTZU_X_D_PATTERN) {
		decoded.decode_class = TCTI_DECODE_FP_INT_CONVERT;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = sizeof(u64);
		decoded.result_size = sizeof(u64);
		decoded.simd_fp = true;
		decoded.fp_int_op = TCTI_FP_INT_FCVTZU;
		return decoded;
	}
	if ((instruction & AARCH64_FCVTZU_W_D_MASK) ==
	    AARCH64_FCVTZU_W_D_PATTERN) {
		decoded.decode_class = TCTI_DECODE_FP_INT_CONVERT;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = sizeof(u64);
		decoded.result_size = sizeof(u32);
		decoded.simd_fp = true;
		decoded.fp_int_op = TCTI_FP_INT_FCVTZU;
		return decoded;
	}

	if ((instruction & AARCH64_FCVTZU_X_D_FIXED_MASK) ==
	    AARCH64_FCVTZU_X_D_FIXED_PATTERN) {
		u8 scale = (instruction >> 10) & 0x3fU;

		decoded.decode_class = TCTI_DECODE_FP_INT_CONVERT;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = sizeof(u64);
		decoded.result_size = sizeof(u64);
		decoded.shift_amount = 64 - scale;
		decoded.simd_fp = true;
		decoded.fp_int_op = TCTI_FP_INT_FCVTZU_FIXED;
		return decoded;
	}

	if ((instruction & AARCH64_FCVTZU_D_D_MASK) == AARCH64_FCVTZU_D_D_PATTERN) {
		decoded.decode_class = TCTI_DECODE_FP_INT_CONVERT;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = sizeof(u64);
		decoded.result_size = sizeof(u64);
		decoded.simd_fp = true;
		decoded.fp_int_op = TCTI_FP_INT_FCVTZU_SIMD;
		return decoded;
	}

	if ((instruction & AARCH64_UCVTF_D_D_MASK) == AARCH64_UCVTF_D_D_PATTERN) {
		decoded.decode_class = TCTI_DECODE_FP_INT_CONVERT;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = sizeof(u64);
		decoded.result_size = sizeof(u64);
		decoded.simd_fp = true;
		decoded.fp_int_op = TCTI_FP_INT_UCVTF_SIMD;
		return decoded;
	}

	if ((instruction & AARCH64_SCVTF_D_D_MASK) ==
	    AARCH64_SCVTF_D_D_PATTERN) {
		decoded.decode_class = TCTI_DECODE_FP_INT_CONVERT;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = sizeof(u64);
		decoded.result_size = sizeof(u64);
		decoded.simd_fp = true;
		decoded.fp_int_op = TCTI_FP_INT_SCVTF_SIMD;
		return decoded;
	}

	if ((instruction & AARCH64_UCVTF_2D_2D_MASK) ==
	    AARCH64_UCVTF_2D_2D_PATTERN) {
		decoded.decode_class = TCTI_DECODE_FP_INT_CONVERT;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.access_size = sizeof(u64);
		decoded.result_size = 2 * sizeof(u64);
		decoded.simd_fp = true;
		decoded.fp_int_op = TCTI_FP_INT_UCVTF_SIMD;
		return decoded;
	}

	if ((instruction & AARCH64_SYSTEM_REGISTER_MASK) == AARCH64_MRS_PATTERN ||
	    (instruction & AARCH64_SYSTEM_REGISTER_MASK) == AARCH64_MSR_PATTERN) {
		u16 sysreg = (instruction >> 5) & 0xffffU;

		if (sysreg != AARCH64_SYSREG_TPIDR_EL0 &&
		    sysreg != AARCH64_SYSREG_NZCV &&
		    sysreg != AARCH64_SYSREG_FPCR &&
		    sysreg != AARCH64_SYSREG_FPSR)
			return decoded;

		decoded.decode_class = TCTI_DECODE_SYSTEM_REGISTER;
		decoded.rt = instruction & 0x1fU;
		switch (sysreg) {
		case AARCH64_SYSREG_TPIDR_EL0:
			decoded.system_register = TCTI_SYSTEM_REGISTER_TPIDR_EL0;
			break;
		case AARCH64_SYSREG_NZCV:
			decoded.system_register = TCTI_SYSTEM_REGISTER_NZCV;
			break;
		case AARCH64_SYSREG_FPCR:
			decoded.system_register = TCTI_SYSTEM_REGISTER_FPCR;
			break;
		case AARCH64_SYSREG_FPSR:
			decoded.system_register = TCTI_SYSTEM_REGISTER_FPSR;
			break;
		}
		decoded.system_register_write =
			(instruction & AARCH64_SYSTEM_REGISTER_MASK) ==
			AARCH64_MSR_PATTERN;
		return decoded;
	}

	return decoded;
}
