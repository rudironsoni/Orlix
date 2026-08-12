/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/bitops.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/unaligned.h>
#include <asm/ptrace.h>

#include "sve_crypto.h"

struct orlix_tcti_sve_crypto_encoding {
	u32 mask;
	u32 value;
	enum orlix_tcti_sve_crypto_op op;
	enum orlix_tcti_sve_crypto_condition condition;
	u8 nregs;
};

/* Exact AARCHMRS 2026-06 #147 source-manifest encodings. */
static const struct orlix_tcti_sve_crypto_encoding sve_crypto_encodings[] = {
	{ 0xff20fc00U, 0x04203400U, ORLIX_TCTI_SVE_CRYPTO_XAR, ORLIX_TCTI_SVE_CRYPTO_COND_SVE2_OR_SME, 1 },
	{ 0xffe0fc00U, 0x04203800U, ORLIX_TCTI_SVE_CRYPTO_EOR3, ORLIX_TCTI_SVE_CRYPTO_COND_SVE2_OR_SME, 1 },
	{ 0xffe0fc00U, 0x04603800U, ORLIX_TCTI_SVE_CRYPTO_BCAX, ORLIX_TCTI_SVE_CRYPTO_COND_SVE2_OR_SME, 1 },
	{ 0xffffffe0U, 0x4520e000U, ORLIX_TCTI_SVE_CRYPTO_AESMC, ORLIX_TCTI_SVE_CRYPTO_COND_SVE_AES, 1 },
	{ 0xffffffe0U, 0x4520e400U, ORLIX_TCTI_SVE_CRYPTO_AESIMC, ORLIX_TCTI_SVE_CRYPTO_COND_SVE_AES, 1 },
	{ 0xfffffc00U, 0x4522e000U, ORLIX_TCTI_SVE_CRYPTO_AESE, ORLIX_TCTI_SVE_CRYPTO_COND_SVE_AES, 1 },
	{ 0xfffffc00U, 0x4522e400U, ORLIX_TCTI_SVE_CRYPTO_AESD, ORLIX_TCTI_SVE_CRYPTO_COND_SVE_AES, 1 },
	{ 0xfffffc00U, 0x4523e000U, ORLIX_TCTI_SVE_CRYPTO_SM4E, ORLIX_TCTI_SVE_CRYPTO_COND_SVE_SM4, 1 },
	{ 0xffe7fc01U, 0x4522e800U, ORLIX_TCTI_SVE_CRYPTO_AESE, ORLIX_TCTI_SVE_CRYPTO_COND_SVE_AES2, 2 },
	{ 0xffe7fc01U, 0x4522ec00U, ORLIX_TCTI_SVE_CRYPTO_AESD, ORLIX_TCTI_SVE_CRYPTO_COND_SVE_AES2, 2 },
	{ 0xffe7fc01U, 0x4523e800U, ORLIX_TCTI_SVE_CRYPTO_AESEMC, ORLIX_TCTI_SVE_CRYPTO_COND_SVE_AES2, 2 },
	{ 0xffe7fc01U, 0x4523ec00U, ORLIX_TCTI_SVE_CRYPTO_AESDIMC, ORLIX_TCTI_SVE_CRYPTO_COND_SVE_AES2, 2 },
	{ 0xffe7fc03U, 0x4526e800U, ORLIX_TCTI_SVE_CRYPTO_AESE, ORLIX_TCTI_SVE_CRYPTO_COND_SVE_AES2, 4 },
	{ 0xffe7fc03U, 0x4526ec00U, ORLIX_TCTI_SVE_CRYPTO_AESD, ORLIX_TCTI_SVE_CRYPTO_COND_SVE_AES2, 4 },
	{ 0xffe7fc03U, 0x4527e800U, ORLIX_TCTI_SVE_CRYPTO_AESEMC, ORLIX_TCTI_SVE_CRYPTO_COND_SVE_AES2, 4 },
	{ 0xffe7fc03U, 0x4527ec00U, ORLIX_TCTI_SVE_CRYPTO_AESDIMC, ORLIX_TCTI_SVE_CRYPTO_COND_SVE_AES2, 4 },
	{ 0xffe0fc00U, 0x4520f000U, ORLIX_TCTI_SVE_CRYPTO_SM4EKEY, ORLIX_TCTI_SVE_CRYPTO_COND_SVE_SM4, 1 },
	{ 0xffe0fc00U, 0x4520f400U, ORLIX_TCTI_SVE_CRYPTO_RAX1, ORLIX_TCTI_SVE_CRYPTO_COND_SVE_SHA3, 1 },
};

int orlix_tcti_decode_sve_crypto(u32 instruction,
		struct orlix_tcti_sve_crypto_instruction *decoded)
{
	unsigned int i;

	if (!decoded)
		return -EINVAL;
	for (i = 0; i < ARRAY_SIZE(sve_crypto_encodings); i++) {
		const struct orlix_tcti_sve_crypto_encoding *encoding =
			&sve_crypto_encodings[i];

		if ((instruction & encoding->mask) != encoding->value)
			continue;
		decoded->op = encoding->op;
		decoded->condition = encoding->condition;
		decoded->zd = instruction & 0x1fU;
		decoded->zn = (instruction >> 5) & 0x1fU;
		decoded->zm = (instruction >> 16) & 0x1fU;
		decoded->zk = decoded->zn;
		decoded->index = 0;
		decoded->nregs = encoding->nregs;
		decoded->element_bytes = 16;
		if (encoding->op == ORLIX_TCTI_SVE_CRYPTO_AESMC ||
		    encoding->op == ORLIX_TCTI_SVE_CRYPTO_AESIMC ||
		    encoding->op == ORLIX_TCTI_SVE_CRYPTO_AESE ||
		    encoding->op == ORLIX_TCTI_SVE_CRYPTO_AESD ||
		    encoding->op == ORLIX_TCTI_SVE_CRYPTO_AESEMC ||
		    encoding->op == ORLIX_TCTI_SVE_CRYPTO_AESDIMC ||
		    encoding->op == ORLIX_TCTI_SVE_CRYPTO_SM4E) {
			decoded->zm = decoded->zn;
			decoded->zn = decoded->zd;
		}
		if (encoding->op == ORLIX_TCTI_SVE_CRYPTO_EOR3 ||
		    encoding->op == ORLIX_TCTI_SVE_CRYPTO_BCAX)
			decoded->zn = decoded->zd;
	if (encoding->nregs > 1)
			decoded->index = (instruction >> 19) & 0x3U;
		if (encoding->op == ORLIX_TCTI_SVE_CRYPTO_XAR) {
			u8 tsize = ((instruction >> 20) & 0xcU) |
				((instruction >> 19) & 0x3U);
			u8 imm3 = (instruction >> 16) & 0x7U;
			u8 esize;

			if (!tsize)
				return -EINVAL;
			esize = 8U << (fls(tsize) - 1U);
			decoded->element_bytes = esize / 8U;
			decoded->index = 2U * esize - ((tsize << 3) | imm3);
			decoded->zm = decoded->zn;
			decoded->zn = decoded->zd;
		}
		if (encoding->nregs > 1 &&
		    (decoded->zd & (encoding->nregs - 1U)))
			return -EINVAL;
		return 0;
	}
	return -ENOENT;
}

static bool sve_crypto_is_unary(enum orlix_tcti_sve_crypto_op op)
{
	return op == ORLIX_TCTI_SVE_CRYPTO_AESMC ||
		op == ORLIX_TCTI_SVE_CRYPTO_AESIMC;
}

static u8 orlix_tcti_aes_rotate_left(u8 value, u8 amount)
{
	return (value << amount) | (value >> (8 - amount));
}

static u8 orlix_tcti_aes_gf_multiply(u8 left, u8 right)
{
	u8 result = 0;
	u8 bit;

	for (bit = 0; bit < 8; bit++) {
		bool high = left & BIT(7);

		if (right & BIT(0))
			result ^= left;
		left <<= 1;
		if (high)
			left ^= 0x1bU;
		right >>= 1;
	}
	return result;
}

static u8 orlix_tcti_aes_gf_inverse(u8 value)
{
	u8 result = 1;
	u8 base = value;
	u8 exponent = 254;

	if (!value)
		return 0;
	while (exponent) {
		if (exponent & 1U)
			result = orlix_tcti_aes_gf_multiply(result, base);
		base = orlix_tcti_aes_gf_multiply(base, base);
		exponent >>= 1;
	}
	return result;
}

static u8 orlix_tcti_aes_substitute(u8 value)
{
	u8 inverse = orlix_tcti_aes_gf_inverse(value);

	return inverse ^ orlix_tcti_aes_rotate_left(inverse, 1) ^
	       orlix_tcti_aes_rotate_left(inverse, 2) ^
	       orlix_tcti_aes_rotate_left(inverse, 3) ^
	       orlix_tcti_aes_rotate_left(inverse, 4) ^ 0x63U;
}

static u8 orlix_tcti_aes_inverse_substitute(u8 value)
{
	u8 inverse_affine = orlix_tcti_aes_rotate_left(value, 1) ^
				    orlix_tcti_aes_rotate_left(value, 3) ^
				    orlix_tcti_aes_rotate_left(value, 6) ^ 0x05U;

	return orlix_tcti_aes_gf_inverse(inverse_affine);
}

static void orlix_tcti_aes_shift_rows(u8 state[16], bool inverse)
{
	u8 source[16];
	u8 row;
	u8 column;

	memcpy(source, state, sizeof(source));
	for (row = 0; row < 4; row++) {
		for (column = 0; column < 4; column++) {
			u8 source_column = inverse ?
				(column + 4 - row) % 4 : (column + row) % 4;

			state[row + column * 4] =
				source[row + source_column * 4];
		}
	}
}

static void orlix_tcti_aes_mix_columns(u8 state[16], bool inverse)
{
	u8 column;

	for (column = 0; column < 4; column++) {
		u8 *value = &state[column * 4];
		u8 source[4] = { value[0], value[1], value[2], value[3] };

		if (inverse) {
			value[0] = orlix_tcti_aes_gf_multiply(source[0], 0x0e) ^
				orlix_tcti_aes_gf_multiply(source[1], 0x0b) ^
				orlix_tcti_aes_gf_multiply(source[2], 0x0d) ^
				orlix_tcti_aes_gf_multiply(source[3], 0x09);
			value[1] = orlix_tcti_aes_gf_multiply(source[0], 0x09) ^
				orlix_tcti_aes_gf_multiply(source[1], 0x0e) ^
				orlix_tcti_aes_gf_multiply(source[2], 0x0b) ^
				orlix_tcti_aes_gf_multiply(source[3], 0x0d);
			value[2] = orlix_tcti_aes_gf_multiply(source[0], 0x0d) ^
				orlix_tcti_aes_gf_multiply(source[1], 0x09) ^
				orlix_tcti_aes_gf_multiply(source[2], 0x0e) ^
				orlix_tcti_aes_gf_multiply(source[3], 0x0b);
			value[3] = orlix_tcti_aes_gf_multiply(source[0], 0x0b) ^
				orlix_tcti_aes_gf_multiply(source[1], 0x0d) ^
				orlix_tcti_aes_gf_multiply(source[2], 0x09) ^
				orlix_tcti_aes_gf_multiply(source[3], 0x0e);
		} else {
			value[0] = orlix_tcti_aes_gf_multiply(source[0], 2) ^
				orlix_tcti_aes_gf_multiply(source[1], 3) ^
				source[2] ^ source[3];
			value[1] = source[0] ^
				orlix_tcti_aes_gf_multiply(source[1], 2) ^
				orlix_tcti_aes_gf_multiply(source[2], 3) ^ source[3];
			value[2] = source[0] ^ source[1] ^
				orlix_tcti_aes_gf_multiply(source[2], 2) ^
				orlix_tcti_aes_gf_multiply(source[3], 3);
			value[3] = orlix_tcti_aes_gf_multiply(source[0], 3) ^
				source[1] ^ source[2] ^
				orlix_tcti_aes_gf_multiply(source[3], 2);
		}
	}
}

/* GB/T 32907-2016 S-box, matching upstream Linux crypto/sm4.c. */
static const u8 orlix_tcti_sm4_sbox[256] = {
	0xd6, 0x90, 0xe9, 0xfe, 0xcc, 0xe1, 0x3d, 0xb7,
	0x16, 0xb6, 0x14, 0xc2, 0x28, 0xfb, 0x2c, 0x05,
	0x2b, 0x67, 0x9a, 0x76, 0x2a, 0xbe, 0x04, 0xc3,
	0xaa, 0x44, 0x13, 0x26, 0x49, 0x86, 0x06, 0x99,
	0x9c, 0x42, 0x50, 0xf4, 0x91, 0xef, 0x98, 0x7a,
	0x33, 0x54, 0x0b, 0x43, 0xed, 0xcf, 0xac, 0x62,
	0xe4, 0xb3, 0x1c, 0xa9, 0xc9, 0x08, 0xe8, 0x95,
	0x80, 0xdf, 0x94, 0xfa, 0x75, 0x8f, 0x3f, 0xa6,
	0x47, 0x07, 0xa7, 0xfc, 0xf3, 0x73, 0x17, 0xba,
	0x83, 0x59, 0x3c, 0x19, 0xe6, 0x85, 0x4f, 0xa8,
	0x68, 0x6b, 0x81, 0xb2, 0x71, 0x64, 0xda, 0x8b,
	0xf8, 0xeb, 0x0f, 0x4b, 0x70, 0x56, 0x9d, 0x35,
	0x1e, 0x24, 0x0e, 0x5e, 0x63, 0x58, 0xd1, 0xa2,
	0x25, 0x22, 0x7c, 0x3b, 0x01, 0x21, 0x78, 0x87,
	0xd4, 0x00, 0x46, 0x57, 0x9f, 0xd3, 0x27, 0x52,
	0x4c, 0x36, 0x02, 0xe7, 0xa0, 0xc4, 0xc8, 0x9e,
	0xea, 0xbf, 0x8a, 0xd2, 0x40, 0xc7, 0x38, 0xb5,
	0xa3, 0xf7, 0xf2, 0xce, 0xf9, 0x61, 0x15, 0xa1,
	0xe0, 0xae, 0x5d, 0xa4, 0x9b, 0x34, 0x1a, 0x55,
	0xad, 0x93, 0x32, 0x30, 0xf5, 0x8c, 0xb1, 0xe3,
	0x1d, 0xf6, 0xe2, 0x2e, 0x82, 0x66, 0xca, 0x60,
	0xc0, 0x29, 0x23, 0xab, 0x0d, 0x53, 0x4e, 0x6f,
	0xd5, 0xdb, 0x37, 0x45, 0xde, 0xfd, 0x8e, 0x2f,
	0x03, 0xff, 0x6a, 0x72, 0x6d, 0x6c, 0x5b, 0x51,
	0x8d, 0x1b, 0xaf, 0x92, 0xbb, 0xdd, 0xbc, 0x7f,
	0x11, 0xd9, 0x5c, 0x41, 0x1f, 0x10, 0x5a, 0xd8,
	0x0a, 0xc1, 0x31, 0x88, 0xa5, 0xcd, 0x7b, 0xbd,
	0x2d, 0x74, 0xd0, 0x12, 0xb8, 0xe5, 0xb4, 0xb0,
	0x89, 0x69, 0x97, 0x4a, 0x0c, 0x96, 0x77, 0x7e,
	0x65, 0xb9, 0xf1, 0x09, 0xc5, 0x6e, 0xc6, 0x84,
	0x18, 0xf0, 0x7d, 0xec, 0x3a, 0xdc, 0x4d, 0x20,
	0x79, 0xee, 0x5f, 0x3e, 0xd7, 0xcb, 0x39, 0x48,
};

static u32 orlix_tcti_sm4_substitute(u32 value)
{
	return (u32)orlix_tcti_sm4_sbox[value & 0xffU] |
	       (u32)orlix_tcti_sm4_sbox[(value >> 8) & 0xffU] << 8 |
	       (u32)orlix_tcti_sm4_sbox[(value >> 16) & 0xffU] << 16 |
	       (u32)orlix_tcti_sm4_sbox[value >> 24] << 24;
}

static void sve_crypto_sync_v_to_z(struct orlix_tcti_sve_state *state,
		unsigned long *user_simd, u8 reg)
{
	put_unaligned_le64(user_simd[reg * 2], &state->z[reg][0]);
	put_unaligned_le64(user_simd[reg * 2 + 1], &state->z[reg][8]);
}

static void sve_crypto_sync_z_to_v(const struct orlix_tcti_sve_state *state,
		unsigned long *user_simd, u8 reg)
{
	user_simd[reg * 2] = get_unaligned_le64(&state->z[reg][0]);
	user_simd[reg * 2 + 1] = get_unaligned_le64(&state->z[reg][8]);
}

static void sve_crypto_aes_round(u8 result[16], const u8 left[16],
		const u8 right[16], bool inverse, bool mix)
{
	u8 state[16];
	u8 round_key[16];
	u8 index;

	memcpy(state, left, sizeof(state));
	memcpy(round_key, right, sizeof(round_key));
	for (index = 0; index < 16; index++)
		state[index] ^= round_key[index];
	for (index = 0; index < 16; index++)
		state[index] = inverse ?
			orlix_tcti_aes_inverse_substitute(state[index]) :
			orlix_tcti_aes_substitute(state[index]);
	orlix_tcti_aes_shift_rows(state, inverse);
	if (mix)
		orlix_tcti_aes_mix_columns(state, inverse);
	memcpy(result, state, sizeof(state));
}

static void sve_crypto_sm4_round(u8 result[16], const u8 left[16],
		const u8 right[16], bool key)
{
	u32 state[4];
	u32 source[4];
	u8 round;

	memcpy(state, left, sizeof(state));
	memcpy(source, right, sizeof(source));
	for (round = 0; round < 4; round++) {
		u32 value = orlix_tcti_sm4_substitute(state[1] ^ state[2] ^
			state[3] ^ source[round]);

		value ^= key ? rol32(value, 13) ^ rol32(value, 23) :
			rol32(value, 2) ^ rol32(value, 10) ^ rol32(value, 18) ^
			rol32(value, 24);
		value ^= state[0];
		state[0] = state[1];
		state[1] = state[2];
		state[2] = state[3];
		state[3] = value;
	}
	memcpy(result, state, sizeof(state));
}

static void sve_crypto_aes_multi(struct orlix_tcti_sve_state *state,
		const struct orlix_tcti_sve_crypto_instruction *decoded)
{
	u8 source[4][ORLIX_TCTI_SVE_MAX_VL_BYTES];
	u8 round_key[ORLIX_TCTI_SVE_MAX_VL_BYTES];
	u16 offset;
	u8 reg;

	for (reg = 0; reg < decoded->nregs; reg++)
		memcpy(source[reg], state->z[decoded->zd + reg], state->vl_bytes);
	memcpy(round_key, state->z[decoded->zm], state->vl_bytes);
	for (reg = 0; reg < decoded->nregs; reg++) {
		for (offset = 0; offset < state->vl_bytes; offset += 16) {
			u8 index = decoded->index;
			u16 key_segment;
			u8 *destination = &state->z[decoded->zd + reg][offset];
			const u8 *key;

			if (state->vl_bytes == 16)
				index = 0;
			else if (state->vl_bytes < 64)
				/* DDI0602: a sub-512-bit VL ignores index[1]. */
				index &= 1U;
			key_segment = (offset / 16U & ~3U) + index;
			key = &round_key[key_segment * 16U];

			sve_crypto_aes_round(destination, &source[reg][offset], key,
				decoded->op == ORLIX_TCTI_SVE_CRYPTO_AESD ||
				decoded->op == ORLIX_TCTI_SVE_CRYPTO_AESDIMC,
				decoded->op == ORLIX_TCTI_SVE_CRYPTO_AESEMC ||
				decoded->op == ORLIX_TCTI_SVE_CRYPTO_AESDIMC);
		}
	}
}

int orlix_tcti_execute_sve_crypto(struct orlix_tcti_sve_state *state,
		struct pt_regs *regs, unsigned long *user_simd,
		const struct orlix_tcti_sve_crypto_instruction *decoded)
{
	u16 offset;
	u8 reg;

	if (!state || !regs || !user_simd || !decoded ||
	    state->vl_bytes < 16 || state->vl_bytes > ORLIX_TCTI_SVE_MAX_VL_BYTES ||
	    (state->vl_bytes % 16) || decoded->zd >= ORLIX_TCTI_SVE_ZREG_COUNT ||
	    decoded->zn >= ORLIX_TCTI_SVE_ZREG_COUNT ||
	    decoded->zm >= ORLIX_TCTI_SVE_ZREG_COUNT || !decoded->nregs ||
	    decoded->zd + decoded->nregs > ORLIX_TCTI_SVE_ZREG_COUNT)
		return -EINVAL;
	if (!state->valid)
		return -EOPNOTSUPP;
	for (reg = 0; reg < decoded->nregs; reg++)
		sve_crypto_sync_v_to_z(state, user_simd, decoded->zd + reg);
	if (decoded->zn != decoded->zd)
		sve_crypto_sync_v_to_z(state, user_simd, decoded->zn);
	if (!sve_crypto_is_unary(decoded->op) && decoded->zm != decoded->zd &&
	    decoded->zm != decoded->zn)
		sve_crypto_sync_v_to_z(state, user_simd, decoded->zm);
	if (decoded->zk != decoded->zd && decoded->zk != decoded->zn &&
	    decoded->zk != decoded->zm)
		sve_crypto_sync_v_to_z(state, user_simd, decoded->zk);
	if (decoded->nregs > 1) {
		sve_crypto_aes_multi(state, decoded);
		for (reg = 0; reg < decoded->nregs; reg++)
			sve_crypto_sync_z_to_v(state, user_simd, decoded->zd + reg);
		regs->pc += sizeof(u32);
		return 0;
	}

	for (offset = 0; offset < state->vl_bytes; offset += 16) {
		u8 *destination = &state->z[decoded->zd][offset];
		const u8 *left = &state->z[decoded->zn][offset];
		const u8 *right = &state->z[decoded->zm][offset];
		u8 index;

		switch (decoded->op) {
		case ORLIX_TCTI_SVE_CRYPTO_EOR3:
			for (index = 0; index < 16; index++)
				destination[index] = state->z[decoded->zd][offset + index] ^
					right[index] ^ state->z[decoded->zk][offset + index];
			break;
		case ORLIX_TCTI_SVE_CRYPTO_BCAX:
			for (index = 0; index < 16; index++)
				destination[index] = state->z[decoded->zd][offset + index] ^
					(right[index] &
					 ~state->z[decoded->zk][offset + index]);
			break;
		case ORLIX_TCTI_SVE_CRYPTO_AESMC:
			orlix_tcti_aes_mix_columns(destination, false);
			break;
		case ORLIX_TCTI_SVE_CRYPTO_AESIMC:
			orlix_tcti_aes_mix_columns(destination, true);
			break;
		case ORLIX_TCTI_SVE_CRYPTO_AESE:
		case ORLIX_TCTI_SVE_CRYPTO_AESD:
		case ORLIX_TCTI_SVE_CRYPTO_AESEMC:
		case ORLIX_TCTI_SVE_CRYPTO_AESDIMC:
				sve_crypto_aes_round(destination, left, right,
				decoded->op == ORLIX_TCTI_SVE_CRYPTO_AESD ||
				decoded->op == ORLIX_TCTI_SVE_CRYPTO_AESDIMC,
				decoded->op == ORLIX_TCTI_SVE_CRYPTO_AESEMC ||
				decoded->op == ORLIX_TCTI_SVE_CRYPTO_AESDIMC);
			break;
		case ORLIX_TCTI_SVE_CRYPTO_SM4E:
			sve_crypto_sm4_round(destination, left, right, false);
			break;
		case ORLIX_TCTI_SVE_CRYPTO_SM4EKEY:
			sve_crypto_sm4_round(destination, left, right, true);
			break;
		case ORLIX_TCTI_SVE_CRYPTO_RAX1:
			for (index = 0; index < 2; index++) {
				u64 left64 = get_unaligned_le64(left + index * sizeof(u64));
				u64 right64 = get_unaligned_le64(right + index * sizeof(u64));

				put_unaligned_le64(left64 ^ rol64(right64, 1),
					destination + index * sizeof(u64));
			}
			break;
		case ORLIX_TCTI_SVE_CRYPTO_XAR:
			for (index = 0; index < 16; index += decoded->element_bytes) {
				u64 value = 0;
				u8 bits = decoded->element_bytes * 8U;
				u8 rotation = decoded->index % bits;
				u8 byte;

				for (byte = 0; byte < decoded->element_bytes; byte++)
					value |= (u64)(left[index + byte] ^ right[index + byte]) <<
						(byte * 8);
				if (rotation && decoded->element_bytes == 8)
					value = ror64(value, rotation);
				else if (rotation) {
					u64 mask = GENMASK_ULL(bits - 1, 0);

					value &= mask;
					value = (value >> rotation) |
						(value << (bits - rotation));
				}
				for (byte = 0; byte < decoded->element_bytes; byte++)
					destination[index + byte] = value >> (byte * 8);
			}
			break;
		}
	}
	for (reg = 0; reg < decoded->nregs; reg++)
		sve_crypto_sync_z_to_v(state, user_simd, decoded->zd + reg);
	regs->pc += sizeof(u32);
	return 0;
}
