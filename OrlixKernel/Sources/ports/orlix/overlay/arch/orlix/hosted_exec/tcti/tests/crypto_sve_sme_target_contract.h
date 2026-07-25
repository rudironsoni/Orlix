/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Pinned AARCHMRS 2026-06 obligations for the crypto leaves which are either
 * classic AdvSIMD crypto, SVE crypto, or SME crypto.  This is deliberately a
 * contract, not an alternate decoder or a semantic model.  The canonical
 * target instruction artifact supplies the exact source condition.  Until the
 * corresponding official shared ASL entry is available, every row remains an
 * audit blocker even when the production decoder can execute it.
 */
#ifndef ORLIX_TCTI_CRYPTO_SVE_SME_TARGET_CONTRACT_H
#define ORLIX_TCTI_CRYPTO_SVE_SME_TARGET_CONTRACT_H

enum tcti_crypto_target_family {
	TCTI_CRYPTO_TARGET_ADVSIMD,
	TCTI_CRYPTO_TARGET_SVE,
	TCTI_CRYPTO_TARGET_SME,
};

enum tcti_crypto_target_status {
	/* Decoder/executor evidence exists, but official shared ASL is unavailable. */
	TCTI_CRYPTO_TARGET_IMPLEMENTED_ASL_BLOCKED,
	/* A source leaf is required and has no production TCTI implementation. */
	TCTI_CRYPTO_TARGET_REQUIRED_UNIMPLEMENTED,
};

struct tcti_crypto_target_contract_row {
	u32 source_ordinal;
	const char *source_id;
	const char *mnemonic;
	const char *asl_operation;
	enum tcti_crypto_target_family family;
	enum tcti_crypto_target_status status;
};

/*
 * The leaf-specific feature predicate is not duplicated here.  It is the
 * bounded condition in tcti_target_instruction_artifact, emitted from the
 * pinned source.  The KUnit consumer asserts that it is present for every row.
 */
static const struct tcti_crypto_target_contract_row
tcti_crypto_sve_sme_target_contract[] = {
	{ 587U, "pmullb_z_zz_q", "PMULLB", "operations/pmullb_z_zz", TCTI_CRYPTO_TARGET_SVE, TCTI_CRYPTO_TARGET_REQUIRED_UNIMPLEMENTED },
	{ 588U, "pmullb_z_zz_", "PMULLB", "operations/pmullb_z_zz", TCTI_CRYPTO_TARGET_SVE, TCTI_CRYPTO_TARGET_REQUIRED_UNIMPLEMENTED },
	{ 591U, "pmullt_z_zz_q", "PMULLT", "operations/pmullt_z_zz", TCTI_CRYPTO_TARGET_SVE, TCTI_CRYPTO_TARGET_REQUIRED_UNIMPLEMENTED },
	{ 592U, "pmullt_z_zz_", "PMULLT", "operations/pmullt_z_zz", TCTI_CRYPTO_TARGET_SVE, TCTI_CRYPTO_TARGET_REQUIRED_UNIMPLEMENTED },
	{ 682U, "aesmc_z_z_", "AESMC", "operations/aesmc_z_z", TCTI_CRYPTO_TARGET_SVE, TCTI_CRYPTO_TARGET_REQUIRED_UNIMPLEMENTED },
	{ 683U, "aesimc_z_z_", "AESIMC", "operations/aesimc_z_z", TCTI_CRYPTO_TARGET_SVE, TCTI_CRYPTO_TARGET_REQUIRED_UNIMPLEMENTED },
	{ 684U, "aese_z_zz_", "AESE", "operations/aese_z_zz", TCTI_CRYPTO_TARGET_SVE, TCTI_CRYPTO_TARGET_REQUIRED_UNIMPLEMENTED },
	{ 685U, "aesd_z_zz_", "AESD", "operations/aesd_z_zz", TCTI_CRYPTO_TARGET_SVE, TCTI_CRYPTO_TARGET_REQUIRED_UNIMPLEMENTED },
	{ 686U, "sm4e_z_zz_", "SM4E", "operations/sm4e_z_zz", TCTI_CRYPTO_TARGET_SVE, TCTI_CRYPTO_TARGET_REQUIRED_UNIMPLEMENTED },
	{ 687U, "aese_mz_zzi_2x1", "AESE", "operations/aese_mz_zzi", TCTI_CRYPTO_TARGET_SME, TCTI_CRYPTO_TARGET_REQUIRED_UNIMPLEMENTED },
	{ 688U, "aesd_mz_zzi_2x1", "AESD", "operations/aesd_mz_zzi", TCTI_CRYPTO_TARGET_SME, TCTI_CRYPTO_TARGET_REQUIRED_UNIMPLEMENTED },
	{ 689U, "aesemc_mz_zzi_2x1", "AESEMC", "operations/aesemc_mz_zzi", TCTI_CRYPTO_TARGET_SME, TCTI_CRYPTO_TARGET_REQUIRED_UNIMPLEMENTED },
	{ 690U, "aesdimc_mz_zzi_2x1", "AESDIMC", "operations/aesdimc_mz_zzi", TCTI_CRYPTO_TARGET_SME, TCTI_CRYPTO_TARGET_REQUIRED_UNIMPLEMENTED },
	{ 691U, "aese_mz_zzi_4x1", "AESE", "operations/aese_mz_zzi", TCTI_CRYPTO_TARGET_SME, TCTI_CRYPTO_TARGET_REQUIRED_UNIMPLEMENTED },
	{ 692U, "aesd_mz_zzi_4x1", "AESD", "operations/aesd_mz_zzi", TCTI_CRYPTO_TARGET_SME, TCTI_CRYPTO_TARGET_REQUIRED_UNIMPLEMENTED },
	{ 693U, "aesemc_mz_zzi_4x1", "AESEMC", "operations/aesemc_mz_zzi", TCTI_CRYPTO_TARGET_SME, TCTI_CRYPTO_TARGET_REQUIRED_UNIMPLEMENTED },
	{ 694U, "aesdimc_mz_zzi_4x1", "AESDIMC", "operations/aesdimc_mz_zzi", TCTI_CRYPTO_TARGET_SME, TCTI_CRYPTO_TARGET_REQUIRED_UNIMPLEMENTED },
	{ 695U, "sm4ekey_z_zz_", "SM4EKEY", "operations/sm4ekey_z_zz", TCTI_CRYPTO_TARGET_SVE, TCTI_CRYPTO_TARGET_REQUIRED_UNIMPLEMENTED },
	{ 697U, "pmull_mz_zzw_1x2", "PMULL", "operations/pmull_mz_zzw", TCTI_CRYPTO_TARGET_SME, TCTI_CRYPTO_TARGET_REQUIRED_UNIMPLEMENTED },
	{ 3507U, "AESE_B_cryptoaes", "AESE", "operations/AESE_advsimd", TCTI_CRYPTO_TARGET_ADVSIMD, TCTI_CRYPTO_TARGET_IMPLEMENTED_ASL_BLOCKED },
	{ 3508U, "AESD_B_cryptoaes", "AESD", "operations/AESD_advsimd", TCTI_CRYPTO_TARGET_ADVSIMD, TCTI_CRYPTO_TARGET_IMPLEMENTED_ASL_BLOCKED },
	{ 3509U, "AESMC_B_cryptoaes", "AESMC", "operations/AESMC_advsimd", TCTI_CRYPTO_TARGET_ADVSIMD, TCTI_CRYPTO_TARGET_IMPLEMENTED_ASL_BLOCKED },
	{ 3510U, "AESIMC_B_cryptoaes", "AESIMC", "operations/AESIMC_advsimd", TCTI_CRYPTO_TARGET_ADVSIMD, TCTI_CRYPTO_TARGET_IMPLEMENTED_ASL_BLOCKED },
	{ 3511U, "SHA1C_QSV_cryptosha3", "SHA1C", "operations/SHA1C_advsimd", TCTI_CRYPTO_TARGET_ADVSIMD, TCTI_CRYPTO_TARGET_IMPLEMENTED_ASL_BLOCKED },
	{ 3512U, "SHA1P_QSV_cryptosha3", "SHA1P", "operations/SHA1P_advsimd", TCTI_CRYPTO_TARGET_ADVSIMD, TCTI_CRYPTO_TARGET_IMPLEMENTED_ASL_BLOCKED },
	{ 3513U, "SHA1M_QSV_cryptosha3", "SHA1M", "operations/SHA1M_advsimd", TCTI_CRYPTO_TARGET_ADVSIMD, TCTI_CRYPTO_TARGET_IMPLEMENTED_ASL_BLOCKED },
	{ 3514U, "SHA1SU0_VVV_cryptosha3", "SHA1SU0", "operations/SHA1SU0_advsimd", TCTI_CRYPTO_TARGET_ADVSIMD, TCTI_CRYPTO_TARGET_IMPLEMENTED_ASL_BLOCKED },
	{ 3515U, "SHA256H_QQV_cryptosha3", "SHA256H", "operations/SHA256H_advsimd", TCTI_CRYPTO_TARGET_ADVSIMD, TCTI_CRYPTO_TARGET_IMPLEMENTED_ASL_BLOCKED },
	{ 3516U, "SHA256H2_QQV_cryptosha3", "SHA256H2", "operations/SHA256H2_advsimd", TCTI_CRYPTO_TARGET_ADVSIMD, TCTI_CRYPTO_TARGET_IMPLEMENTED_ASL_BLOCKED },
	{ 3517U, "SHA256SU1_VVV_cryptosha3", "SHA256SU1", "operations/SHA256SU1_advsimd", TCTI_CRYPTO_TARGET_ADVSIMD, TCTI_CRYPTO_TARGET_IMPLEMENTED_ASL_BLOCKED },
	{ 3518U, "SHA1H_SS_cryptosha2", "SHA1H", "operations/SHA1H_advsimd", TCTI_CRYPTO_TARGET_ADVSIMD, TCTI_CRYPTO_TARGET_IMPLEMENTED_ASL_BLOCKED },
	{ 3519U, "SHA1SU1_VV_cryptosha2", "SHA1SU1", "operations/SHA1SU1_advsimd", TCTI_CRYPTO_TARGET_ADVSIMD, TCTI_CRYPTO_TARGET_IMPLEMENTED_ASL_BLOCKED },
	{ 3520U, "SHA256SU0_VV_cryptosha2", "SHA256SU0", "operations/SHA256SU0_advsimd", TCTI_CRYPTO_TARGET_ADVSIMD, TCTI_CRYPTO_TARGET_IMPLEMENTED_ASL_BLOCKED },
	{ 3883U, "PMULL_asimddiff_L", "PMULL", "operations/PMULL_advsimd", TCTI_CRYPTO_TARGET_ADVSIMD, TCTI_CRYPTO_TARGET_IMPLEMENTED_ASL_BLOCKED },
	{ 3955U, "PMUL_asimdsame_only", "PMUL", "operations/PMUL_advsimd", TCTI_CRYPTO_TARGET_ADVSIMD, TCTI_CRYPTO_TARGET_IMPLEMENTED_ASL_BLOCKED },
	{ 4067U, "SM3TT1A_VVV4_crypto3_imm2", "SM3TT1A", "operations/SM3TT1A_advsimd", TCTI_CRYPTO_TARGET_ADVSIMD, TCTI_CRYPTO_TARGET_IMPLEMENTED_ASL_BLOCKED },
	{ 4068U, "SM3TT1B_VVV4_crypto3_imm2", "SM3TT1B", "operations/SM3TT1B_advsimd", TCTI_CRYPTO_TARGET_ADVSIMD, TCTI_CRYPTO_TARGET_IMPLEMENTED_ASL_BLOCKED },
	{ 4069U, "SM3TT2A_VVV4_crypto3_imm2", "SM3TT2A", "operations/SM3TT2A_advsimd", TCTI_CRYPTO_TARGET_ADVSIMD, TCTI_CRYPTO_TARGET_IMPLEMENTED_ASL_BLOCKED },
	{ 4070U, "SM3TT2B_VVV_crypto3_imm2", "SM3TT2B", "operations/SM3TT2B_advsimd", TCTI_CRYPTO_TARGET_ADVSIMD, TCTI_CRYPTO_TARGET_IMPLEMENTED_ASL_BLOCKED },
	{ 4071U, "SHA512H_QQV_cryptosha512_3", "SHA512H", "operations/SHA512H_advsimd", TCTI_CRYPTO_TARGET_ADVSIMD, TCTI_CRYPTO_TARGET_IMPLEMENTED_ASL_BLOCKED },
	{ 4072U, "SHA512H2_QQV_cryptosha512_3", "SHA512H2", "operations/SHA512H2_advsimd", TCTI_CRYPTO_TARGET_ADVSIMD, TCTI_CRYPTO_TARGET_IMPLEMENTED_ASL_BLOCKED },
	{ 4073U, "SHA512SU1_VVV2_cryptosha512_3", "SHA512SU1", "operations/SHA512SU1_advsimd", TCTI_CRYPTO_TARGET_ADVSIMD, TCTI_CRYPTO_TARGET_IMPLEMENTED_ASL_BLOCKED },
	{ 4074U, "RAX1_VVV2_cryptosha512_3", "RAX1", "operations/RAX1_advsimd", TCTI_CRYPTO_TARGET_ADVSIMD, TCTI_CRYPTO_TARGET_IMPLEMENTED_ASL_BLOCKED },
	{ 4075U, "SM3PARTW1_VVV4_cryptosha512_3", "SM3PARTW1", "operations/SM3PARTW1_advsimd", TCTI_CRYPTO_TARGET_ADVSIMD, TCTI_CRYPTO_TARGET_IMPLEMENTED_ASL_BLOCKED },
	{ 4076U, "SM3PARTW2_VVV4_cryptosha512_3", "SM3PARTW2", "operations/SM3PARTW2_advsimd", TCTI_CRYPTO_TARGET_ADVSIMD, TCTI_CRYPTO_TARGET_IMPLEMENTED_ASL_BLOCKED },
	{ 4077U, "SM4EKEY_VVV4_cryptosha512_3", "SM4EKEY", "operations/SM4EKEY_advsimd", TCTI_CRYPTO_TARGET_ADVSIMD, TCTI_CRYPTO_TARGET_IMPLEMENTED_ASL_BLOCKED },
	{ 4078U, "EOR3_VVV16_crypto4", "EOR3", "operations/EOR3_advsimd", TCTI_CRYPTO_TARGET_ADVSIMD, TCTI_CRYPTO_TARGET_IMPLEMENTED_ASL_BLOCKED },
	{ 4079U, "BCAX_VVV16_crypto4", "BCAX", "operations/BCAX_advsimd", TCTI_CRYPTO_TARGET_ADVSIMD, TCTI_CRYPTO_TARGET_IMPLEMENTED_ASL_BLOCKED },
	{ 4080U, "SM3SS1_VVV4_crypto4", "SM3SS1", "operations/SM3SS1_advsimd", TCTI_CRYPTO_TARGET_ADVSIMD, TCTI_CRYPTO_TARGET_IMPLEMENTED_ASL_BLOCKED },
	{ 4081U, "XAR_VVV2_crypto3_imm6", "XAR", "operations/XAR_advsimd", TCTI_CRYPTO_TARGET_ADVSIMD, TCTI_CRYPTO_TARGET_IMPLEMENTED_ASL_BLOCKED },
	{ 4082U, "SHA512SU0_VV2_cryptosha512_2", "SHA512SU0", "operations/SHA512SU0_advsimd", TCTI_CRYPTO_TARGET_ADVSIMD, TCTI_CRYPTO_TARGET_IMPLEMENTED_ASL_BLOCKED },
	{ 4083U, "SM4E_VV4_cryptosha512_2", "SM4E", "operations/SM4E_advsimd", TCTI_CRYPTO_TARGET_ADVSIMD, TCTI_CRYPTO_TARGET_IMPLEMENTED_ASL_BLOCKED },
};

#endif
