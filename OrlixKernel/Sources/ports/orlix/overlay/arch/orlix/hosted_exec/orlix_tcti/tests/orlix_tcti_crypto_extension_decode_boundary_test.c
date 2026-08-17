// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched/mm.h>
#include <linux/string.h>
#include <linux/syscalls.h>
#include <asm/ptrace.h>
#include <asm/orlix_tcti.h>

#include "../decode_aarch64.h"
#include "../switch_debug.h"
#include "crypto_sve_sme_target_contract.h"
#include "target_completion_audit.h"
#include "target_instruction_artifact.h"

static const char *crypto_artifact_string(
	const struct orlix_tcti_target_instruction_artifact *artifact, u32 offset)
{
	if (offset >= artifact->string_pool_size)
		return NULL;
	return (const char *)artifact->string_pool + offset;
}

struct crypto_semantic_provenance_row {
	u32 source_ordinal;
	const char *leaf_name;
	enum orlix_tcti_a64_semantic_provenance_disposition disposition;
	const char *relative_file;
	const char *decode_locator;
	const char *decode_digest;
	u32 decode_section_count;
	u32 decode_helper_count;
	const char *execute_locator;
	const char *execute_digest;
	u32 execute_section_count;
	u32 execute_helper_count;
};

#define ORLIX_TCTI_A64_SEMANTIC_PROVENANCE_SOURCE(...)
#define ORLIX_TCTI_A64_DDI0602_PROVENANCE_ROW(ordinal, leaf_name, relative_file, \
		decode_locator, decode_digest, decode_sections, decode_helpers, \
		decode_helper_digest, execute_locator, execute_digest, execute_sections, \
		execute_helpers, execute_helper_digest) \
	{ ordinal, leaf_name, ORLIX_TCTI_A64_SEMANTIC_PROVENANCE_EXTERNAL_DDI0602, \
	  relative_file, decode_locator, decode_digest, decode_sections, decode_helpers, \
	  execute_locator, execute_digest, execute_sections, execute_helpers },
#define ORLIX_TCTI_A64_OFFICIAL_SEMANTICS_NOT_SPECIFIED_ROW(ordinal, leaf_name, \
		operation_locator, operation_offset, operation_length, operation_digest) \
	{ ordinal, leaf_name, \
	  ORLIX_TCTI_A64_SEMANTIC_PROVENANCE_OFFICIAL_SEMANTICS_NOT_SPECIFIED, \
	  NULL, NULL, NULL, 0U, 0U, operation_locator, operation_digest, 0U, 0U },
static const struct crypto_semantic_provenance_row crypto_semantic_provenance[] = {
#include "../isa/target_asl_availability.def"
};
#undef ORLIX_TCTI_A64_OFFICIAL_SEMANTICS_NOT_SPECIFIED_ROW
#undef ORLIX_TCTI_A64_DDI0602_PROVENANCE_ROW
#undef ORLIX_TCTI_A64_SEMANTIC_PROVENANCE_SOURCE

static const struct orlix_tcti_crypto_target_contract_row *
crypto_target_contract_row(u32 source_ordinal)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(orlix_tcti_crypto_sve_sme_target_contract); i++)
		if (orlix_tcti_crypto_sve_sme_target_contract[i].source_ordinal ==
		    source_ordinal)
			return &orlix_tcti_crypto_sve_sme_target_contract[i];

	return NULL;
}

static const struct crypto_semantic_provenance_row *
crypto_semantic_provenance_row(u32 source_ordinal)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(crypto_semantic_provenance); i++)
		if (crypto_semantic_provenance[i].source_ordinal == source_ordinal)
			return &crypto_semantic_provenance[i];

	return NULL;
}

struct crypto_leaf {
	u32 source_ordinal;
	const char *source_id;
	const char *name;
	u32 mask;
	u32 value;
	enum orlix_tcti_simd_vector_arithmetic_op op;
	bool rm;
	bool ra;
	u8 imm_lsb;
	u8 imm_width;
};

static const struct crypto_leaf crypto_leaves[] = {
	{ 4071U, "SHA512H_QQV_cryptosha512_3", "SHA512H", 0xffe0fc00U, 0xce608000U, ORLIX_TCTI_SIMD_ARITH_SHA512H, true, false, 0, 0 },
	{ 4072U, "SHA512H2_QQV_cryptosha512_3", "SHA512H2", 0xffe0fc00U, 0xce608400U, ORLIX_TCTI_SIMD_ARITH_SHA512H2, true, false, 0, 0 },
	{ 4073U, "SHA512SU1_VVV2_cryptosha512_3", "SHA512SU1", 0xffe0fc00U, 0xce608800U, ORLIX_TCTI_SIMD_ARITH_SHA512SU1, true, false, 0, 0 },
	{ 4082U, "SHA512SU0_VV2_cryptosha512_2", "SHA512SU0", 0xfffffc00U, 0xcec08000U, ORLIX_TCTI_SIMD_ARITH_SHA512SU0, false, false, 0, 0 },
	{ 4078U, "EOR3_VVV16_crypto4", "EOR3", 0xffe08000U, 0xce000000U, ORLIX_TCTI_SIMD_ARITH_EOR3, true, true, 0, 0 },
	{ 4074U, "RAX1_VVV2_cryptosha512_3", "RAX1", 0xffe0fc00U, 0xce608c00U, ORLIX_TCTI_SIMD_ARITH_RAX1, true, false, 0, 0 },
	{ 4081U, "XAR_VVV2_crypto3_imm6", "XAR", 0xffe00000U, 0xce800000U, ORLIX_TCTI_SIMD_ARITH_XAR, true, false, 10, 6 },
	{ 4079U, "BCAX_VVV16_crypto4", "BCAX", 0xffe08000U, 0xce200000U, ORLIX_TCTI_SIMD_ARITH_BCAX, true, true, 0, 0 },
	{ 4080U, "SM3SS1_VVV4_crypto4", "SM3SS1", 0xffe08000U, 0xce400000U, ORLIX_TCTI_SIMD_ARITH_SM3SS1, true, true, 0, 0 },
	{ 4067U, "SM3TT1A_VVV4_crypto3_imm2", "SM3TT1A", 0xffe0cc00U, 0xce408000U, ORLIX_TCTI_SIMD_ARITH_SM3TT1A, true, false, 12, 2 },
	{ 4068U, "SM3TT1B_VVV4_crypto3_imm2", "SM3TT1B", 0xffe0cc00U, 0xce408400U, ORLIX_TCTI_SIMD_ARITH_SM3TT1B, true, false, 12, 2 },
	{ 4069U, "SM3TT2A_VVV4_crypto3_imm2", "SM3TT2A", 0xffe0cc00U, 0xce408800U, ORLIX_TCTI_SIMD_ARITH_SM3TT2A, true, false, 12, 2 },
	{ 4070U, "SM3TT2B_VVV_crypto3_imm2", "SM3TT2B", 0xffe0cc00U, 0xce408c00U, ORLIX_TCTI_SIMD_ARITH_SM3TT2B, true, false, 12, 2 },
	{ 4075U, "SM3PARTW1_VVV4_cryptosha512_3", "SM3PARTW1", 0xffe0fc00U, 0xce60c000U, ORLIX_TCTI_SIMD_ARITH_SM3PARTW1, true, false, 0, 0 },
	{ 4076U, "SM3PARTW2_VVV4_cryptosha512_3", "SM3PARTW2", 0xffe0fc00U, 0xce60c400U, ORLIX_TCTI_SIMD_ARITH_SM3PARTW2, true, false, 0, 0 },
	{ 4083U, "SM4E_VV4_cryptosha512_2", "SM4E", 0xfffffc00U, 0xcec08400U, ORLIX_TCTI_SIMD_ARITH_SM4E, false, false, 0, 0 },
	{ 4077U, "SM4EKEY_VVV4_cryptosha512_3", "SM4EKEY", 0xffe0fc00U, 0xce60c800U, ORLIX_TCTI_SIMD_ARITH_SM4EKEY, true, false, 0, 0 },
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
	struct orlix_tcti_decoded_instruction decoded = orlix_tcti_decode_aarch64(instruction);

	KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC,
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
	const struct orlix_tcti_target_instruction_artifact *artifact =
		orlix_tcti_target_instruction_artifact_canonical();
	unsigned int i;

	KUNIT_ASSERT_NOT_NULL(test, artifact);
	for (i = 0; i < ARRAY_SIZE(orlix_tcti_crypto_sve_sme_target_contract); i++) {
		const struct orlix_tcti_crypto_target_contract_row *row =
			&orlix_tcti_crypto_sve_sme_target_contract[i];
		const struct orlix_tcti_target_instruction_artifact_leaf *leaf;
		const struct crypto_semantic_provenance_row *provenance;
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
			"source ordinal=%u must retain its semantic locator",
			row->source_ordinal);
		KUNIT_EXPECT_NE_MSG(test, '\0', row->asl_operation[0],
			"source ordinal=%u must retain its semantic locator",
			row->source_ordinal);
		provenance = crypto_semantic_provenance_row(row->source_ordinal);
		KUNIT_ASSERT_NOT_NULL_MSG(test, provenance, "source ordinal=%u",
			row->source_ordinal);
		KUNIT_EXPECT_STREQ_MSG(test, row->source_id, provenance->leaf_name,
			"source ordinal=%u", row->source_ordinal);
		KUNIT_EXPECT_EQ_MSG(test,
			ORLIX_TCTI_A64_SEMANTIC_PROVENANCE_EXTERNAL_DDI0602,
			provenance->disposition, "source ordinal=%u",
			row->source_ordinal);
		if (row->family == ORLIX_TCTI_CRYPTO_TARGET_ADVSIMD)
			KUNIT_EXPECT_EQ_MSG(test,
				ORLIX_TCTI_CRYPTO_TARGET_IMPLEMENTED_PROOF_PENDING, row->status,
				"classic leaf %s must remain proof-pending", row->source_id);
		else
			KUNIT_EXPECT_EQ_MSG(test,
				ORLIX_TCTI_CRYPTO_TARGET_REQUIRED_UNIMPLEMENTED, row->status,
				"vector/matrix leaf %s must remain visible", row->source_id);
	}
}

/*
 * Reachability only. External DDI0602 provenance for PMUL is independent from
 * this production implementation, so this establishes no proof credit.
 */
static void crypto_pmul_source_contract_and_decoder_reachability(
	struct kunit *test)
{
	static const u8 expected_condition[] =
		"\x54\x43\x4e\x44\x01\x07\x00\x00\x00\x31\x07\x00"
		"\x00\x00\x17\x07\x00\x00\x00\x0c\x01\x00\x00\x00"
		"\x01\x01\x01\x00\x00\x00\x01\x01\x01\x00\x00\x00"
		"\x01\x01\x02\x00\x00\x00\x10\x00\x00\x00\x0c\x46"
		"\x45\x41\x54\x5f\x41\x64\x76\x53\x49\x4d\x44";
	const struct orlix_tcti_target_instruction_artifact *artifact =
		orlix_tcti_target_instruction_artifact_canonical();
	const struct orlix_tcti_target_instruction_artifact_leaf *leaf;
	const struct orlix_tcti_crypto_target_contract_row *contract;
	const struct crypto_semantic_provenance_row *provenance;
	struct orlix_tcti_decoded_instruction decoded;
	struct pt_regs regs = { .pc = 0x4000 };
	const char *source_id;
	const char *mnemonic;
	const char *operation;

	KUNIT_ASSERT_NOT_NULL(test, artifact);
	KUNIT_ASSERT_LT(test, 3955U, artifact->leaf_count);
	leaf = &artifact->leaves[3955U];
	source_id = crypto_artifact_string(artifact, leaf->name_offset);
	mnemonic = crypto_artifact_string(artifact, leaf->mnemonic_offset);
	operation = crypto_artifact_string(artifact, leaf->operation_offset);
	KUNIT_ASSERT_NOT_NULL(test, source_id);
	KUNIT_ASSERT_NOT_NULL(test, mnemonic);
	KUNIT_ASSERT_NOT_NULL(test, operation);
	KUNIT_EXPECT_STREQ(test, "PMUL_asimdsame_only", source_id);
	KUNIT_EXPECT_STREQ(test, "PMUL", mnemonic);
	KUNIT_EXPECT_STREQ(test, "PMUL_advsimd", operation);
	KUNIT_EXPECT_EQ(test, 0xbf20fc00U, leaf->encoding_mask);
	KUNIT_EXPECT_EQ(test, 0x2e209c00U, leaf->encoding_pattern);
	KUNIT_EXPECT_EQ(test, sizeof(expected_condition) - 1U,
			leaf->condition_length);
	KUNIT_ASSERT_LE(test, leaf->condition_offset + leaf->condition_length,
			artifact->condition_pool_size);
	KUNIT_EXPECT_MEMEQ(test,
		artifact->condition_pool + leaf->condition_offset,
		expected_condition, sizeof(expected_condition) - 1U);

	contract = crypto_target_contract_row(3955U);
	KUNIT_ASSERT_NOT_NULL_MSG(test, contract,
		"PMUL must remain a durable crypto target-contract obligation");
	KUNIT_EXPECT_STREQ(test, "PMUL_asimdsame_only", contract->source_id);
	KUNIT_EXPECT_STREQ(test, "PMUL", contract->mnemonic);
	KUNIT_EXPECT_STREQ(test, "operations/PMUL_advsimd",
		contract->asl_operation);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_CRYPTO_TARGET_ADVSIMD, contract->family);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_CRYPTO_TARGET_IMPLEMENTED_PROOF_PENDING,
		contract->status);

	provenance = crypto_semantic_provenance_row(3955U);
	KUNIT_ASSERT_NOT_NULL(test, provenance);
	KUNIT_EXPECT_STREQ(test, "PMUL_asimdsame_only", provenance->leaf_name);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_A64_SEMANTIC_PROVENANCE_EXTERNAL_DDI0602,
			provenance->disposition);
	KUNIT_ASSERT_NOT_NULL(test, provenance->relative_file);
	KUNIT_ASSERT_NOT_NULL(test, provenance->decode_locator);
	KUNIT_ASSERT_NOT_NULL(test, provenance->decode_digest);
	KUNIT_ASSERT_NOT_NULL(test, provenance->execute_locator);
	KUNIT_ASSERT_NOT_NULL(test, provenance->execute_digest);
	KUNIT_EXPECT_GT(test, provenance->decode_section_count, 0U);
	KUNIT_EXPECT_GT(test, provenance->execute_section_count, 0U);

	decoded = orlix_tcti_decode_aarch64(0x2e239d31U);
	KUNIT_ASSERT_EQ(test, ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC,
		decoded.decode_class);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_SIMD_ARITH_PMUL,
		decoded.simd_arithmetic_op);
	KUNIT_EXPECT_EQ(test, 17U, decoded.rd);
	KUNIT_EXPECT_EQ(test, 9U, decoded.rn);
	KUNIT_EXPECT_EQ(test, 3U, decoded.rm);
	KUNIT_EXPECT_EQ(test, 1U, decoded.access_size);
	KUNIT_EXPECT_EQ(test, 8U, decoded.result_size);
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL));
	KUNIT_EXPECT_EQ(test, 0x4004ULL, regs.pc);
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
			struct orlix_tcti_decoded_instruction decoded;

			if (!(leaf->mask & BIT(bit)))
				continue;
			instruction = crypto_instruction(leaf, leaf->value ^ BIT(bit),
				17, 9, 3, 7, 0);
			peer = crypto_leaf_for(instruction);
			decoded = orlix_tcti_decode_aarch64(instruction);
			if (peer) {
				KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC,
					decoded.decode_class);
				KUNIT_EXPECT_EQ(test, peer->op, decoded.simd_arithmetic_op);
			} else {
				KUNIT_EXPECT_TRUE_MSG(test,
					decoded.decode_class !=
						ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC ||
					decoded.simd_arithmetic_op != leaf->op,
					"retained %s after reserved mutation %#x",
					leaf->name, instruction);
			}
		}
	}
}

enum crypto_vector_family { CRYPTO_SHA512, CRYPTO_SHA3, CRYPTO_SM3, CRYPTO_SM4 };
#define CRYPTO_RESUME_SVC 0xd4000001U

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
		struct orlix_tcti_decoded_instruction *decoded)
{
	current->thread.user_simd[0] = 0x89abcdef01234567ULL;
	current->thread.user_simd[1] = 0x76543210fedcba98ULL;
	if (vector->family == CRYPTO_SHA512) {
		current->thread.user_simd[0] = 0x0123456789abcdefULL;
		current->thread.user_simd[1] = 0xfedcba9876543210ULL;
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
		bool ss1 = decoded->simd_arithmetic_op == ORLIX_TCTI_SIMD_ARITH_SM3SS1;

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
		struct orlix_tcti_decoded_instruction decoded = orlix_tcti_decode_aarch64(vector->instruction);
		struct pt_regs regs = { .pc = 0x8000, .sp = 0x12345000,
			.pstate = PSR_N_BIT | PSR_Z_BIT | 0x155UL };
		struct pt_regs before = regs;

		crypto_vector_state(vector, &decoded);
		current->thread.user_simd[62] = 0x1122334455667788ULL;
		current->thread.user_simd[63] = 0x8877665544332211ULL;
		current->thread.user_fpsr = BIT(5);
		KUNIT_ASSERT_EQ(test, 0,
			orlix_tcti_switch_debug_execute_decoded(NULL, &regs, &decoded, NULL));
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
		struct orlix_tcti_decoded_instruction first = orlix_tcti_decode_aarch64(
			crypto_instruction(leaf, leaf->value, 0, 1, 2, 3, 1));
		struct orlix_tcti_decoded_instruction second = orlix_tcti_decode_aarch64(
			crypto_instruction(leaf, leaf->value, 8, 8, 8, 8, 1));
		u64 low;
		u64 high;
		u8 reg;

		for (reg = 0; reg < 4; reg++) {
			current->thread.user_simd[reg * 2] = 0x0123456789abcdefULL;
			current->thread.user_simd[reg * 2 + 1] = 0xfedcba9876543210ULL;
		}
		KUNIT_ASSERT_EQ(test, 0, orlix_tcti_switch_debug_execute_decoded(NULL, &baseline, &first, NULL));
		low = current->thread.user_simd[0];
		high = current->thread.user_simd[1];
		current->thread.user_simd[16] = 0x0123456789abcdefULL;
		current->thread.user_simd[17] = 0xfedcba9876543210ULL;
		current->thread.user_simd[62] = 0x1122334455667788ULL;
		current->thread.user_simd[63] = 0x8877665544332211ULL;
		current->thread.user_fpsr = BIT(5);
		KUNIT_ASSERT_EQ(test, 0, orlix_tcti_switch_debug_execute_decoded(NULL, &alias, &second, NULL));
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

static int crypto_resume_test_init(struct kunit *test)
{
	struct mm_struct *mm = mm_alloc();

	if (!mm)
		return -ENOMEM;
	kthread_use_mm(mm);
	test->priv = mm;
	return 0;
}

static void crypto_resume_test_exit(struct kunit *test)
{
	struct mm_struct *mm = test->priv;

	if (!mm)
		return;
	kthread_unuse_mm(mm);
	mmput(mm);
}

static int crypto_resume_map_text(unsigned long *text)
{
	unsigned long mapped;

	mapped = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
				  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (IS_ERR_VALUE(mapped))
		return (long)mapped;
	*text = mapped;
	return 0;
}

static int crypto_resume_write_program(unsigned long text, u32 instruction)
{
	const u32 program[] = { instruction, CRYPTO_RESUME_SVC };
	int ret;

	ret = orlix_tcti_write_user_data(current->mm, text, program, sizeof(program));
	if (ret)
		return ret;
	return sys_mprotect(text, PAGE_SIZE, PROT_READ | PROT_EXEC);
}

/*
 * Production-path regression coverage only. External DDI0602 provenance is
 * independent from implementation evidence. These vectors must not discharge
 * source-bound proof or
 * enable runtime capability advertisement, and every exercised leaf remains
 * ORLIX_TCTI_CRYPTO_TARGET_IMPLEMENTED_PROOF_PENDING.
 */
static void crypto_resume_production_path_regression(struct kunit *test)
{
	unsigned long text = 0;
	u32 seen = 0;
	unsigned int i;
	int ret;

	ret = crypto_resume_map_text(&text);
	KUNIT_EXPECT_EQ(test, 0, ret);
	if (ret)
		return;

	for (i = 0; i < ARRAY_SIZE(crypto_vectors); i++) {
		const struct crypto_vector *vector = &crypto_vectors[i];
		const struct crypto_leaf *leaf = crypto_leaf_for(vector->instruction);
		const struct orlix_tcti_crypto_target_contract_row *contract;
		struct orlix_tcti_decoded_instruction decoded =
			orlix_tcti_decode_aarch64(vector->instruction);
		struct pt_regs regs = {
			.sp = 0x12345000,
			.pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT,
			.syscallno = NO_SYSCALL,
		};
		struct pt_regs expected;
		struct orlix_tcti_result result;
		u64 expected_simd[ARRAY_SIZE(current->thread.user_simd)];
		u64 expected_fpcr = 0x04000000ULL;
		u64 expected_fpsr = BIT(5);

		KUNIT_ASSERT_NOT_NULL(test, leaf);
		KUNIT_ASSERT_GE(test, leaf->source_ordinal, 4067U);
		KUNIT_ASSERT_LE(test, leaf->source_ordinal, 4083U);
		seen |= BIT(leaf->source_ordinal - 4067U);
		contract = crypto_target_contract_row(leaf->source_ordinal);
		KUNIT_ASSERT_NOT_NULL(test, contract);
		KUNIT_EXPECT_EQ_MSG(test,
				    ORLIX_TCTI_CRYPTO_TARGET_IMPLEMENTED_PROOF_PENDING,
				    contract->status, "%s source=%u must remain proof-pending",
				    leaf->name, leaf->source_ordinal);
		KUNIT_ASSERT_EQ(test, ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC,
				decoded.decode_class);

		ret = crypto_resume_write_program(text, vector->instruction);
		KUNIT_EXPECT_EQ_MSG(test, 0, ret, "%s source=%u", leaf->name,
				    leaf->source_ordinal);
		if (ret)
			goto unmap;
		regs.regs[0] = 0x0123456789abcdefULL;
		regs.regs[8] = 0xfedcba9876543210ULL;
		regs.regs[30] = 0x8877665544332211ULL;
		regs.orig_x0 = 0xfeedfaceULL;
		regs.pc = text;
		crypto_vector_state(vector, &decoded);
		current->thread.user_simd[62] = 0x1122334455667788ULL;
		current->thread.user_simd[63] = 0x8877665544332211ULL;
		current->thread.user_simd_valid = 0;
		current->thread.user_fpcr = expected_fpcr;
		current->thread.user_fpsr = expected_fpsr;
		memcpy(expected_simd, current->thread.user_simd,
		       sizeof(expected_simd));
		expected_simd[0] = vector->low;
		expected_simd[1] = vector->high;
		expected = regs;
		expected.pc = text + sizeof(u32);

		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason,
				    "%s source=%u", leaf->name, leaf->source_ordinal);
		KUNIT_EXPECT_EQ_MSG(test, 0L, result.status, "%s source=%u",
				    leaf->name, leaf->source_ordinal);
		KUNIT_EXPECT_EQ_MSG(test, text + sizeof(u32), result.pc,
				    "%s source=%u", leaf->name, leaf->source_ordinal);
		KUNIT_EXPECT_EQ_MSG(test, CRYPTO_RESUME_SVC, result.instruction,
				    "%s source=%u", leaf->name, leaf->source_ordinal);
		KUNIT_EXPECT_MEMEQ_MSG(test, &expected, &regs, sizeof(regs),
				       "%s source=%u", leaf->name, leaf->source_ordinal);
		KUNIT_EXPECT_MEMEQ_MSG(test, expected_simd, current->thread.user_simd,
				       sizeof(expected_simd), "%s source=%u", leaf->name,
				       leaf->source_ordinal);
		KUNIT_EXPECT_EQ_MSG(test, 1UL, current->thread.user_simd_valid,
				    "%s source=%u", leaf->name, leaf->source_ordinal);
		KUNIT_EXPECT_EQ_MSG(test, expected_fpcr, current->thread.user_fpcr,
				    "%s source=%u", leaf->name, leaf->source_ordinal);
		KUNIT_EXPECT_EQ_MSG(test, expected_fpsr, current->thread.user_fpsr,
				    "%s source=%u", leaf->name, leaf->source_ordinal);
		ret = sys_mprotect(text, PAGE_SIZE, PROT_READ | PROT_WRITE);
		KUNIT_EXPECT_EQ_MSG(test, 0, ret, "%s source=%u", leaf->name,
				    leaf->source_ordinal);
		if (ret)
			goto unmap;
	}
	KUNIT_EXPECT_EQ(test, GENMASK(16, 0), seen);

unmap:
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(text, PAGE_SIZE));
}

#define CRYPTO_EOR3_SOURCE_ORDINAL 4078U

struct crypto_eor3_registers {
	u8 rd;
	u8 rn;
	u8 rm;
	u8 ra;
};

static const struct crypto_eor3_registers crypto_eor3_overlap_cases[] = {
	{ 31U, 0U, 15U, 30U },
	{ 9U, 9U, 15U, 30U },
	{ 9U, 0U, 9U, 30U },
	{ 9U, 0U, 15U, 9U },
	{ 9U, 15U, 15U, 30U },
	{ 9U, 15U, 30U, 15U },
	{ 9U, 15U, 30U, 30U },
	{ 31U, 31U, 31U, 31U },
};

static const struct crypto_leaf *crypto_eor3_leaf(void)
{
	unsigned int index;

	for (index = 0; index < ARRAY_SIZE(crypto_leaves); index++)
		if (crypto_leaves[index].source_ordinal ==
		    CRYPTO_EOR3_SOURCE_ORDINAL)
			return &crypto_leaves[index];
	return NULL;
}

static void crypto_eor3_seed_simd_state(u64 simd[], u32 instruction)
{
	unsigned int index;

	for (index = 0; index < ARRAY_SIZE(current->thread.user_simd); index++)
		simd[index] = 0x9e3779b97f4a7c15ULL ^
			((u64)instruction << (index & 31U)) ^
			rol64(0xd1b54a32d192ed03ULL, index) ^ index;
}

static void crypto_eor3_source_binding_and_decode_fields(struct kunit *test)
{
	const struct orlix_tcti_target_instruction_artifact *artifact =
		orlix_tcti_target_instruction_artifact_canonical();
	const struct orlix_tcti_target_instruction_artifact_leaf *source;
	const struct crypto_leaf *leaf = crypto_eor3_leaf();
	const char *name;
	const char *mnemonic;
	const char *operation;
	u8 field;

	KUNIT_ASSERT_NOT_NULL(test, artifact);
	KUNIT_ASSERT_NOT_NULL(test, leaf);
	KUNIT_ASSERT_LT(test, CRYPTO_EOR3_SOURCE_ORDINAL, artifact->leaf_count);
	source = &artifact->leaves[CRYPTO_EOR3_SOURCE_ORDINAL];
	name = crypto_artifact_string(artifact, source->name_offset);
	mnemonic = crypto_artifact_string(artifact, source->mnemonic_offset);
	operation = crypto_artifact_string(artifact, source->operation_offset);
	KUNIT_ASSERT_NOT_NULL(test, name);
	KUNIT_ASSERT_NOT_NULL(test, mnemonic);
	KUNIT_ASSERT_NOT_NULL(test, operation);
	KUNIT_EXPECT_STREQ(test, "EOR3_VVV16_crypto4", name);
	KUNIT_EXPECT_STREQ(test, "EOR3", mnemonic);
	KUNIT_EXPECT_STREQ(test, "EOR3_advsimd", operation);
	KUNIT_EXPECT_EQ(test, leaf->mask, source->encoding_mask);
	KUNIT_EXPECT_EQ(test, leaf->value, source->encoding_pattern);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_SIMD_ARITH_EOR3, leaf->op);

	for (field = 0; field < 32; field++) {
		struct orlix_tcti_decoded_instruction decoded;
		u32 instruction;

		instruction = crypto_instruction(leaf, leaf->value,
			field, 9U, 3U, 7U, 0U);
		decoded = orlix_tcti_decode_aarch64(instruction);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC,
			decoded.decode_class);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_SIMD_ARITH_EOR3,
			decoded.simd_arithmetic_op);
		KUNIT_EXPECT_EQ(test, field, decoded.rd);

		instruction = crypto_instruction(leaf, leaf->value,
			17U, field, 3U, 7U, 0U);
		decoded = orlix_tcti_decode_aarch64(instruction);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC,
			decoded.decode_class);
		KUNIT_EXPECT_EQ(test, field, decoded.rn);

		instruction = crypto_instruction(leaf, leaf->value,
			17U, 9U, field, 7U, 0U);
		decoded = orlix_tcti_decode_aarch64(instruction);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC,
			decoded.decode_class);
		KUNIT_EXPECT_EQ(test, field, decoded.rm);

		instruction = crypto_instruction(leaf, leaf->value,
			17U, 9U, 3U, field, 0U);
		decoded = orlix_tcti_decode_aarch64(instruction);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC,
			decoded.decode_class);
		KUNIT_EXPECT_EQ(test, field, decoded.ra);
	}
}

static void crypto_eor3_resume_mapped_rx_with_register_aliases(
	struct kunit *test)
{
	const struct crypto_leaf *leaf = crypto_eor3_leaf();
	u64 saved_simd[ARRAY_SIZE(current->thread.user_simd)];
	u64 saved_fpcr = current->thread.user_fpcr;
	u64 saved_fpsr = current->thread.user_fpsr;
	unsigned long saved_valid = current->thread.user_simd_valid;
	unsigned long text = 0;
	unsigned int case_index;
	int ret;

	KUNIT_ASSERT_NOT_NULL(test, leaf);
	ret = crypto_resume_map_text(&text);
	KUNIT_ASSERT_EQ(test, 0, ret);
	memcpy(saved_simd, current->thread.user_simd, sizeof(saved_simd));

	for (case_index = 0;
	     case_index < ARRAY_SIZE(crypto_eor3_overlap_cases); case_index++) {
		const struct crypto_eor3_registers *registers =
			&crypto_eor3_overlap_cases[case_index];
		u32 instruction = crypto_instruction(leaf, leaf->value,
			registers->rd, registers->rn, registers->rm, registers->ra,
			0U);
		const u32 expected_program[] = { instruction, CRYPTO_RESUME_SVC };
		u32 before_program[ARRAY_SIZE(expected_program)];
		u32 after_program[ARRAY_SIZE(expected_program)];
		u64 expected_simd[ARRAY_SIZE(current->thread.user_simd)];
		u64 low;
		u64 high;
		struct pt_regs regs = {
			.sp = 0x12345000,
			.pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_Z_BIT |
				PSR_C_BIT | PSR_V_BIT,
			.syscallno = NO_SYSCALL,
		};
		struct pt_regs expected_regs;
		struct orlix_tcti_result result;

		ret = crypto_resume_write_program(text, instruction);
		KUNIT_EXPECT_EQ(test, 0, ret);
		if (ret)
			goto restore;
		ret = orlix_tcti_read_user_data(current->mm, text, before_program,
					  sizeof(before_program));
		KUNIT_EXPECT_EQ(test, 0, ret);
		KUNIT_EXPECT_MEMEQ(test, expected_program, before_program,
				   sizeof(expected_program));
		crypto_eor3_seed_simd_state(expected_simd, instruction);
		memcpy(current->thread.user_simd, expected_simd,
		       sizeof(expected_simd));
		low = expected_simd[registers->rn * 2] ^
			expected_simd[registers->rm * 2] ^
			expected_simd[registers->ra * 2];
		high = expected_simd[registers->rn * 2 + 1] ^
			expected_simd[registers->rm * 2 + 1] ^
			expected_simd[registers->ra * 2 + 1];
		expected_simd[registers->rd * 2] = low;
		expected_simd[registers->rd * 2 + 1] = high;
		current->thread.user_simd_valid = 0;
		current->thread.user_fpcr = 0x04000000ULL;
		current->thread.user_fpsr = BIT(5);
		regs.regs[0] = 0x0123456789abcdefULL;
		regs.regs[8] = 0xfedcba9876543210ULL;
		regs.regs[30] = 0x8877665544332211ULL;
		regs.orig_x0 = 0xfeedfaceULL;
		regs.pc = text;
		expected_regs = regs;
		expected_regs.pc += sizeof(u32);

		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason,
			"EOR3 overlap case=%u", case_index);
		KUNIT_EXPECT_EQ(test, 0L, result.status);
		KUNIT_EXPECT_EQ(test, 0UL, result.fault_address);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_ACCESS_FETCH, result.fault_access);
		KUNIT_EXPECT_EQ(test, text + sizeof(u32), result.pc);
		KUNIT_EXPECT_EQ(test, CRYPTO_RESUME_SVC, result.instruction);
		KUNIT_EXPECT_MEMEQ(test, &expected_regs, &regs, sizeof(regs));
		KUNIT_EXPECT_MEMEQ(test, expected_simd, current->thread.user_simd,
				   sizeof(expected_simd));
		KUNIT_EXPECT_EQ(test, 1UL, current->thread.user_simd_valid);
		KUNIT_EXPECT_EQ(test, 0x04000000ULL, current->thread.user_fpcr);
		KUNIT_EXPECT_EQ(test, BIT(5), current->thread.user_fpsr);
		ret = orlix_tcti_read_user_data(current->mm, text, after_program,
					  sizeof(after_program));
		KUNIT_EXPECT_EQ(test, 0, ret);
		KUNIT_EXPECT_MEMEQ(test, expected_program, after_program,
				   sizeof(expected_program));
		ret = sys_mprotect(text, PAGE_SIZE, PROT_READ | PROT_WRITE);
		KUNIT_EXPECT_EQ(test, 0, ret);
		if (ret)
			goto restore;
	}

restore:
	memcpy(current->thread.user_simd, saved_simd, sizeof(saved_simd));
	current->thread.user_fpcr = saved_fpcr;
	current->thread.user_fpsr = saved_fpsr;
	current->thread.user_simd_valid = saved_valid;
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(text, PAGE_SIZE));
}

static struct kunit_case crypto_extension_cases[] = {
	KUNIT_CASE(crypto_source_leaf_provenance_is_complete),
	KUNIT_CASE(crypto_sve_sme_target_contract_is_source_bound),
	KUNIT_CASE(crypto_pmul_source_contract_and_decoder_reachability),
	KUNIT_CASE(crypto_decode_legal_and_immediate_variants),
	KUNIT_CASE(crypto_decode_rejects_fixed_neighbours),
	KUNIT_CASE(crypto_executor_known_vectors),
	KUNIT_CASE(crypto_executor_aliasing),
	KUNIT_CASE(crypto_resume_production_path_regression),
	KUNIT_CASE(crypto_eor3_source_binding_and_decode_fields),
	KUNIT_CASE(crypto_eor3_resume_mapped_rx_with_register_aliases),
	{}
};

struct kunit_suite orlix_tcti_crypto_extension_decode_boundary_test_suite = {
	.name = "orlix-tcti-crypto-extension-decode-boundary",
	.init = crypto_resume_test_init,
	.exit = crypto_resume_test_exit,
	.test_cases = crypto_extension_cases,
};

kunit_test_suite(orlix_tcti_crypto_extension_decode_boundary_test_suite);
