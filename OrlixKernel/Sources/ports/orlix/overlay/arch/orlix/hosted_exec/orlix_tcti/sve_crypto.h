/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_SVE_CRYPTO_H
#define ORLIX_TCTI_SVE_CRYPTO_H

#include <linux/bitops.h>
#include <linux/types.h>

#include "sve_state.h"

struct pt_regs;

enum orlix_tcti_sve_crypto_feature {
	ORLIX_TCTI_SVE_CRYPTO_FEAT_SVE2 = BIT(0),
	ORLIX_TCTI_SVE_CRYPTO_FEAT_SME = BIT(1),
	ORLIX_TCTI_SVE_CRYPTO_FEAT_SVE_AES = BIT(2),
	ORLIX_TCTI_SVE_CRYPTO_FEAT_SVE_AES2 = BIT(3),
	ORLIX_TCTI_SVE_CRYPTO_FEAT_SVE_SM4 = BIT(4),
	ORLIX_TCTI_SVE_CRYPTO_FEAT_SVE_SHA3 = BIT(5),
};

enum orlix_tcti_sve_crypto_condition {
	ORLIX_TCTI_SVE_CRYPTO_COND_SVE2_OR_SME,
	ORLIX_TCTI_SVE_CRYPTO_COND_SVE_AES,
	ORLIX_TCTI_SVE_CRYPTO_COND_SVE_AES2,
	ORLIX_TCTI_SVE_CRYPTO_COND_SVE_SM4,
	ORLIX_TCTI_SVE_CRYPTO_COND_SVE_SHA3,
};

enum orlix_tcti_sve_crypto_op {
	ORLIX_TCTI_SVE_CRYPTO_XAR,
	ORLIX_TCTI_SVE_CRYPTO_EOR3,
	ORLIX_TCTI_SVE_CRYPTO_BCAX,
	ORLIX_TCTI_SVE_CRYPTO_AESMC,
	ORLIX_TCTI_SVE_CRYPTO_AESIMC,
	ORLIX_TCTI_SVE_CRYPTO_AESE,
	ORLIX_TCTI_SVE_CRYPTO_AESD,
	ORLIX_TCTI_SVE_CRYPTO_SM4E,
	ORLIX_TCTI_SVE_CRYPTO_AESEMC,
	ORLIX_TCTI_SVE_CRYPTO_AESDIMC,
	ORLIX_TCTI_SVE_CRYPTO_SM4EKEY,
	ORLIX_TCTI_SVE_CRYPTO_RAX1,
};

struct orlix_tcti_sve_crypto_instruction {
	enum orlix_tcti_sve_crypto_op op;
	u8 zd;
	u8 zn;
	u8 zm;
	u8 zk;
	u8 index;
	u8 nregs;
	u8 element_bytes;
	enum orlix_tcti_sve_crypto_condition condition;
};

int orlix_tcti_decode_sve_crypto(u32 instruction,
		struct orlix_tcti_sve_crypto_instruction *decoded);
int orlix_tcti_execute_sve_crypto(struct orlix_tcti_sve_state *state,
		struct pt_regs *regs, unsigned long *user_simd, bool available,
		const struct orlix_tcti_sve_crypto_instruction *decoded);

#endif /* ORLIX_TCTI_SVE_CRYPTO_H */
