// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/bitops.h>
#include <asm/ptrace.h>

#include "../decode_aarch64.h"
#include "../switch_debug.h"
#include "crypto_sve_sme_target_contract.h"
#include "target_instruction_artifact.h"

static const char *crypto_artifact_string(
	const struct tcti_target_instruction_artifact *artifact, u32 offset)
{
	if (offset >= artifact->string_pool_size)
		return NULL;
	return (const char *)artifact->string_pool + offset;
}

struct crypto_leaf {
	u32 source_ordinal;
	const char *source_id;
	const char *name;
	u32 mask;
	u32 value;
	enum tcti_simd_vector_arithmetic_op op;
	bool rm;
	bool ra;
	u8 imm_lsb;
	u8 imm_width;
};

static const struct crypto_leaf crypto_leaves[] = {
	{ 4071U, "SHA512H_QQV_cryptosha512_3", "SHA512H", 0xffe0fc00U, 0xce608000U, TCTI_SIMD_ARITH_SHA512H, true, false, 0, 0 },
	{ 4072U, "SHA512H2_QQV_cryptosha512_3", "SHA512H2", 0xffe0fc00U, 0xce608400U, TCTI_SIMD_ARITH_SHA512H2, true, false, 0, 0 },
	{ 4073U, "SHA512SU1_VVV2_cryptosha512_3", "SHA512SU1", 0xffe0fc00U, 0xce608800U, TCTI_SIMD_ARITH_SHA512SU1, true, false, 0, 0 },
	{ 4082U, "SHA512SU0_VV2_cryptosha512_2", "SHA512SU0", 0xfffffc00U, 0xcec08000U, TCTI_SIMD_ARITH_SHA512SU0, false, false, 0, 0 },
	{ 4078U, "EOR3_VVV16_crypto4", "EOR3", 0xffe08000U, 0xce000000U, TCTI_SIMD_ARITH_EOR3, true, true, 0, 0 },
	{ 4074U, "RAX1_VVV2_cryptosha512_3", "RAX1", 0xffe0fc00U, 0xce608c00U, TCTI_SIMD_ARITH_RAX1, true, false, 0, 0 },
	{ 4081U, "XAR_VVV2_crypto3_imm6", "XAR", 0xffe00000U, 0xce800000U, TCTI_SIMD_ARITH_XAR, true, false, 10, 6 },
	{ 4079U, "BCAX_VVV16_crypto4", "BCAX", 0xffe08000U, 0xce200000U, TCTI_SIMD_ARITH_BCAX, true, true, 0, 0 },
	{ 4080U, "SM3SS1_VVV4_crypto4", "SM3SS1", 0xffe08000U, 0xce400000U, TCTI_SIMD_ARITH_SM3SS1, true, true, 0, 0 },
	{ 4067U, "SM3TT1A_VVV4_crypto3_imm2", "SM3TT1A", 0xffe0cc00U, 0xce408000U, TCTI_SIMD_ARITH_SM3TT1A, true, false, 12, 2 },
	{ 4068U, "SM3TT1B_VVV4_crypto3_imm2", "SM3TT1B", 0xffe0cc00U, 0xce408400U, TCTI_SIMD_ARITH_SM3TT1B, true, false, 12, 2 },
	{ 4069U, "SM3TT2A_VVV4_crypto3_imm2", "SM3TT2A", 0xffe0cc00U, 0xce408800U, TCTI_SIMD_ARITH_SM3TT2A, true, false, 12, 2 },
	{ 4070U, "SM3TT2B_VVV_crypto3_imm2", "SM3TT2B", 0xffe0cc00U, 0xce408c00U, TCTI_SIMD_ARITH_SM3TT2B, true, false, 12, 2 },
	{ 4075U, "SM3PARTW1_VVV4_cryptosha512_3", "SM3PARTW1", 0xffe0fc00U, 0xce60c000U, TCTI_SIMD_ARITH_SM3PARTW1, true, false, 0, 0 },
	{ 4076U, "SM3PARTW2_VVV4_cryptosha512_3", "SM3PARTW2", 0xffe0fc00U, 0xce60c400U, TCTI_SIMD_ARITH_SM3PARTW2, true, false, 0, 0 },
	{ 4083U, "SM4E_VV4_cryptosha512_2", "SM4E", 0xfffffc00U, 0xcec08400U, TCTI_SIMD_ARITH_SM4E, false, false, 0, 0 },
	{ 4077U, "SM4EKEY_VVV4_cryptosha512_3", "SM4EKEY", 0xffe0fc00U, 0xce60c800U, TCTI_SIMD_ARITH_SM4EKEY, true, false, 0, 0 },
};

static u32 crypto_instruction(const struct crypto_leaf *leaf, u32 value,
				u8 rd, u8 rn, u8 rm, u8 ra, u8 imm)
{
	value |= rd | ((u32)rn << 5);
	if (leaf->rm)
		value |= (u32)rm << 16;
	if (leaf->ra)
		value |= (u32)ra << 10;
	if (leaf->imm_width)
		value |= (u32)imm << leaf->imm_lsb;
	return value;
}

static const struct crypto_leaf *crypto_leaf_for(u32 instruction)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(crypto_leaves); i++)
		if ((instruction & crypto_leaves[i].mask) == crypto_leaves[i].value)
			return &crypto_leaves[i];
	return NULL;
}

static void crypto_expect_decode(struct kunit *test,
		const struct crypto_leaf *leaf, u32 instruction, u8 imm)
{
	struct tcti_decoded_instruction decoded = tcti_decode_aarch64(instruction);

	KUNIT_EXPECT_EQ_MSG(test, TCTI_DECODE_SIMD_VECTOR_ARITHMETIC,
			decoded.decode_class, "%s (%u, %s) %#x", leaf->name,
			leaf->source_ordinal, leaf->source_id, instruction);
	KUNIT_EXPECT_EQ_MSG(test, leaf->op, decoded.simd_arithmetic_op,
			"%s (%u, %s) %#x", leaf->name, leaf->source_ordinal,
			leaf->source_id, instruction);
	KUNIT_EXPECT_EQ(test, 17U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 9U, decoded.rn);
	KUNIT_EXPECT_EQ(test, leaf->rm ? 3U : 0U, decoded.rm);
	KUNIT_EXPECT_EQ(test, leaf->ra ? 7U : 0U, decoded.ra);
	KUNIT_EXPECT_EQ(test, imm, decoded.shift_amount);
	KUNIT_EXPECT_EQ(test, 16U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 16U, decoded.result_size);
	KUNIT_EXPECT_TRUE(test, decoded.simd_fp);
	KUNIT_EXPECT_FALSE(test, decoded.simd_scalar);
}

static void crypto_source_leaf_provenance_is_complete(struct kunit *test)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(crypto_leaves); i++) {
		const struct crypto_leaf *leaf = &crypto_leaves[i];

		KUNIT_EXPECT_NE_MSG(test, 0U, leaf->source_ordinal,
			"%s has no pinned source ordinal", leaf->name);
		KUNIT_EXPECT_NOT_NULL(test, leaf->source_id);
		if (!leaf->source_id)
			continue;
		KUNIT_EXPECT_NE_MSG(test, '\0', leaf->source_id[0],
			"%s has an empty pinned source identifier", leaf->name);
	}
}

static void crypto_sve_sme_target_contract_is_source_bound(struct kunit *test)
{
	const struct tcti_target_instruction_artifact *artifact =
		tcti_target_instruction_artifact_canonical();
	unsigned int i;

	KUNIT_ASSERT_NOT_NULL(test, artifact);
	for (i = 0; i < ARRAY_SIZE(tcti_crypto_sve_sme_target_contract); i++) {
		const struct tcti_crypto_target_contract_row *row =
			&tcti_crypto_sve_sme_target_contract[i];
		const struct tcti_target_instruction_artifact_leaf *leaf;
		const char *source_id;
		const char *mnemonic;

		KUNIT_ASSERT_LT_MSG(test, row->source_ordinal, artifact->leaf_count,
			"contract row %u source=%s", i, row->source_id);
		leaf = &artifact->leaves[row->source_ordinal];
		source_id = crypto_artifact_string(artifact, leaf->name_offset);
		mnemonic = crypto_artifact_string(artifact, leaf->mnemonic_offset);
		KUNIT_ASSERT_NOT_NULL_MSG(test, source_id, "ordinal=%u",
			row->source_ordinal);
		KUNIT_ASSERT_NOT_NULL_MSG(test, mnemonic, "ordinal=%u",
			row->source_ordinal);
		KUNIT_EXPECT_STREQ_MSG(test, row->source_id, source_id,
			"source ordinal=%u", row->source_ordinal);
		KUNIT_EXPECT_STREQ_MSG(test, row->mnemonic, mnemonic,
			"source ordinal=%u", row->source_ordinal);
		KUNIT_EXPECT_GT_MSG(test, leaf->condition_length, 0U,
			"source ordinal=%u must retain its pinned feature predicate",
			row->source_ordinal);
		KUNIT_EXPECT_NOT_NULL_MSG(test, row->asl_operation,
			"source ordinal=%u must retain its ASL locator",
			row->source_ordinal);
		KUNIT_EXPECT_NE_MSG(test, '\0', row->asl_operation[0],
			"source ordinal=%u must retain its ASL locator",
			row->source_ordinal);
		if (row->family == TCTI_CRYPTO_TARGET_ADVSIMD)
			KUNIT_EXPECT_EQ_MSG(test,
				TCTI_CRYPTO_TARGET_IMPLEMENTED_ASL_BLOCKED, row->status,
				"classic leaf %s must remain ASL-blocked", row->source_id);
		else
			KUNIT_EXPECT_EQ_MSG(test,
				TCTI_CRYPTO_TARGET_REQUIRED_UNIMPLEMENTED, row->status,
				"vector/matrix leaf %s must remain visible", row->source_id);
	}
}

static void crypto_decode_legal_and_immediate_variants(struct kunit *test)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(crypto_leaves); i++) {
		const struct crypto_leaf *leaf = &crypto_leaves[i];
		u8 count = leaf->imm_width ? BIT(leaf->imm_width) : 1;
		u8 imm;

		for (imm = 0; imm < count; imm++)
			crypto_expect_decode(test, leaf,
				crypto_instruction(leaf, leaf->value, 17, 9, 3, 7, imm), imm);
	}
}

static void crypto_decode_rejects_fixed_neighbours(struct kunit *test)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(crypto_leaves); i++) {
		const struct crypto_leaf *leaf = &crypto_leaves[i];
		u8 bit;

		for (bit = 0; bit < 32; bit++) {
			u32 instruction;
			const struct crypto_leaf *peer;
			struct tcti_decoded_instruction decoded;

			if (!(leaf->mask & BIT(bit)))
				continue;
			instruction = crypto_instruction(leaf, leaf->value ^ BIT(bit),
				17, 9, 3, 7, 0);
			peer = crypto_leaf_for(instruction);
			decoded = tcti_decode_aarch64(instruction);
			if (peer) {
				KUNIT_EXPECT_EQ(test, TCTI_DECODE_SIMD_VECTOR_ARITHMETIC,
					decoded.decode_class);
				KUNIT_EXPECT_EQ(test, peer->op, decoded.simd_arithmetic_op);
			} else {
				KUNIT_EXPECT_EQ_MSG(test, TCTI_DECODE_UNSUPPORTED,
					decoded.decode_class, "reserved neighbour %#x of %s",
					instruction, leaf->name);
			}
		}
	}
}

enum crypto_vector_family { CRYPTO_SHA512, CRYPTO_SHA3, CRYPTO_SM3, CRYPTO_SM4 };
struct crypto_vector {
	u32 instruction;
	u64 low;
	u64 high;
	u64 sm4_low;
	u64 sm4_high;
	enum crypto_vector_family family;
};

static const struct crypto_vector crypto_vectors[] = {
	{ 0xce628020U, 0xf0f47a5b26e1bdb2ULL, 0xb4206fda37564a94ULL, 0, 0, CRYPTO_SHA512 },
	{ 0xce628420U, 0xbfa4f12061b7ee1cULL, 0x5adebc996c2bcde4ULL, 0, 0, CRYPTO_SHA512 },
	{ 0xce628820U, 0xa1a1d975587d114fULL, 0x1cacc4a736af70aaULL, 0, 0, CRYPTO_SHA512 },
	{ 0xcec08020U, 0x6f907deb1d5cb34dULL, 0xe87aefdc69ad2195ULL, 0, 0, CRYPTO_SHA512 },
	{ 0xce020c20U, 0x10cd67baba678954ULL, 0x548923fefe234598ULL, 0, 0, CRYPTO_SHA3 },
	{ 0xce628c20U, 0x09abd47690326fcdULL, 0xd47609ab4deff654ULL, 0, 0, CRYPTO_SHA3 },
	{ 0xce823420U, 0x4d5881933a2dd4c4ULL, 0xd4c55c4ee7f0091dULL, 0, 0, CRYPTO_SHA3 },
	{ 0xce220c20U, 0x10236767baab89efULL, 0xfe89bafe76233298ULL, 0, 0, CRYPTO_SHA3 },
	{ 0xce420c20U, 0, 0x09854c3300000000ULL, 0, 0, CRYPTO_SM3 },
	{ 0xce428020U, 0xb97531fd89abcdefULL, 0xb0e4556976543210ULL, 0, 0, CRYPTO_SM3 },
	{ 0xce429420U, 0xb97531fd89abcdefULL, 0xbfaedbab76543210ULL, 0, 0, CRYPTO_SM3 },
	{ 0xce42a820U, 0xd4c7f6e589abcdefULL, 0xa33eb49676543210ULL, 0, 0, CRYPTO_SM3 },
	{ 0xce42bc20U, 0xd4c7f6e589abcdefULL, 0x9fb632fb76543210ULL, 0, 0, CRYPTO_SM3 },
	{ 0xce62c020U, 0x030bcfc73030fcfcULL, 0x6f1f3c6e38e32aaaULL, 0, 0, CRYPTO_SM3 },
	{ 0xce62c420U, 0xbcd2701e9afc5630ULL, 0x124372016ce8d30cULL, 0, 0, CRYPTO_SM3 },
	{ 0xcec08420U, 0xb349299b5c86dacaULL, 0xee5ae58e5f9dc081ULL, 0x2468ace013579bdfULL, 0xdeadbeef0badf00dULL, CRYPTO_SM4 },
	{ 0xce62c820U, 0x103236586cf1cdedULL, 0x5fca364b15f83d25ULL, 0x89abcdef01234567ULL, 0x76543210fedcba98ULL, CRYPTO_SM4 },
};

static void crypto_vector_state(const struct crypto_vector *vector,
		struct tcti_decoded_instruction *decoded)
{
	current->thread.user_simd[0] = 0x89abcdef01234567ULL;
	current->thread.user_simd[1] = 0x76543210fedcba98ULL;
	if (vector->family == CRYPTO_SHA512) {
		current->thread.user_simd[2] = 0x13579bdf2468ace0ULL;
		current->thread.user_simd[3] = 0x0badf00ddeadbeefULL;
		current->thread.user_simd[4] = 0x1111111122222222ULL;
		current->thread.user_simd[5] = 0x3333333344444444ULL;
	} else if (vector->family == CRYPTO_SHA3) {
		current->thread.user_simd[2] = 0x0123456789abcdefULL;
		current->thread.user_simd[3] = 0xfedcba9876543210ULL;
		current->thread.user_simd[4] = 0x1111222233334444ULL;
		current->thread.user_simd[5] = 0x5555666677778888ULL;
		current->thread.user_simd[6] = 0x00ff00ff00ff00ffULL;
		current->thread.user_simd[7] = 0xff00ff00ff00ff00ULL;
	} else if (vector->family == CRYPTO_SM3) {
		bool ss1 = decoded->simd_arithmetic_op == TCTI_SIMD_ARITH_SM3SS1;

		current->thread.user_simd[2] = ss1 ? 0x89abcdef01234567ULL : 0x2468ace013579bdfULL;
		current->thread.user_simd[3] = ss1 ? 0x76543210fedcba98ULL : 0xdeadbeef0badf00dULL;
		current->thread.user_simd[4] = ss1 ? 0x2468ace013579bdfULL : 0x2222222211111111ULL;
		current->thread.user_simd[5] = ss1 ? 0xdeadbeef0badf00dULL : 0x4444444433333333ULL;
		current->thread.user_simd[6] = 0x2222222211111111ULL;
		current->thread.user_simd[7] = 0x4444444433333333ULL;
	} else {
		current->thread.user_simd[2] = vector->sm4_low;
		current->thread.user_simd[3] = vector->sm4_high;
		current->thread.user_simd[4] = 0x2468ace013579bdfULL;
		current->thread.user_simd[5] = 0xdeadbeef0badf00dULL;
	}
}

static void crypto_executor_known_vectors(struct kunit *test)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(crypto_vectors); i++) {
		const struct crypto_vector *vector = &crypto_vectors[i];
		struct tcti_decoded_instruction decoded = tcti_decode_aarch64(vector->instruction);
		struct pt_regs regs = { .pc = 0x8000, .sp = 0x12345000,
			.pstate = PSR_N_BIT | PSR_Z_BIT | 0x155UL };
		struct pt_regs before = regs;

		crypto_vector_state(vector, &decoded);
		current->thread.user_simd[62] = 0x1122334455667788ULL;
		current->thread.user_simd[63] = 0x8877665544332211ULL;
		current->thread.user_fpsr = BIT(5);
		KUNIT_ASSERT_EQ(test, 0,
			tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL));
		KUNIT_EXPECT_EQ(test, vector->low, current->thread.user_simd[0]);
		KUNIT_EXPECT_EQ(test, vector->high, current->thread.user_simd[1]);
		KUNIT_EXPECT_EQ(test, 0x1122334455667788ULL, current->thread.user_simd[62]);
		KUNIT_EXPECT_EQ(test, 0x8877665544332211ULL, current->thread.user_simd[63]);
		KUNIT_EXPECT_EQ(test, BIT(5), current->thread.user_fpsr);
		KUNIT_EXPECT_MEMEQ(test, before.regs, regs.regs, sizeof(regs.regs));
		KUNIT_EXPECT_EQ(test, before.pc + sizeof(u32), regs.pc);
		KUNIT_EXPECT_EQ(test, before.sp, regs.sp);
		KUNIT_EXPECT_EQ(test, before.pstate, regs.pstate);
	}
}

static void crypto_executor_aliasing(struct kunit *test)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(crypto_leaves); i++) {
		const struct crypto_leaf *leaf = &crypto_leaves[i];
		struct pt_regs baseline = { .pc = 0x9000, .sp = 0x12345000,
			.pstate = 0x40000000 };
		struct pt_regs alias = baseline;
		struct tcti_decoded_instruction first = tcti_decode_aarch64(
			crypto_instruction(leaf, leaf->value, 0, 1, 2, 3, 1));
		struct tcti_decoded_instruction second = tcti_decode_aarch64(
			crypto_instruction(leaf, leaf->value, 8, 8, 8, 8, 1));
		u64 low;
		u64 high;
		u8 reg;

		for (reg = 0; reg < 4; reg++) {
			current->thread.user_simd[reg * 2] = 0x0123456789abcdefULL;
			current->thread.user_simd[reg * 2 + 1] = 0xfedcba9876543210ULL;
		}
		KUNIT_ASSERT_EQ(test, 0, tcti_switch_debug_execute_decoded(NULL, &baseline, &first, NULL));
		low = current->thread.user_simd[0];
		high = current->thread.user_simd[1];
		current->thread.user_simd[16] = 0x0123456789abcdefULL;
		current->thread.user_simd[17] = 0xfedcba9876543210ULL;
		current->thread.user_simd[62] = 0x1122334455667788ULL;
		current->thread.user_simd[63] = 0x8877665544332211ULL;
		current->thread.user_fpsr = BIT(5);
		KUNIT_ASSERT_EQ(test, 0, tcti_switch_debug_execute_decoded(NULL, &alias, &second, NULL));
		KUNIT_EXPECT_EQ(test, low, current->thread.user_simd[16]);
		KUNIT_EXPECT_EQ(test, high, current->thread.user_simd[17]);
		KUNIT_EXPECT_EQ(test, 0x1122334455667788ULL, current->thread.user_simd[62]);
		KUNIT_EXPECT_EQ(test, 0x8877665544332211ULL, current->thread.user_simd[63]);
		KUNIT_EXPECT_EQ(test, BIT(5), current->thread.user_fpsr);
		KUNIT_EXPECT_EQ(test, 0x9004ULL, baseline.pc);
		KUNIT_EXPECT_EQ(test, 0x9004ULL, alias.pc);
		KUNIT_EXPECT_EQ(test, 0x12345000ULL, alias.sp);
		KUNIT_EXPECT_EQ(test, 0x40000000ULL, alias.pstate);
	}
}

static struct kunit_case crypto_extension_cases[] = {
	KUNIT_CASE(crypto_source_leaf_provenance_is_complete),
	KUNIT_CASE(crypto_sve_sme_target_contract_is_source_bound),
	KUNIT_CASE(crypto_decode_legal_and_immediate_variants),
	KUNIT_CASE(crypto_decode_rejects_fixed_neighbours),
	KUNIT_CASE(crypto_executor_known_vectors),
	KUNIT_CASE(crypto_executor_aliasing),
	{}
};

struct kunit_suite tcti_crypto_extension_decode_boundary_test_suite = {
	.name = "orlix-tcti-crypto-extension-decode-boundary",
	.test_cases = crypto_extension_cases,
};

kunit_test_suite(tcti_crypto_extension_decode_boundary_test_suite);
