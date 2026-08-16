// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/bitops.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched/mm.h>
#include <linux/string.h>
#include <linux/syscalls.h>
#include <linux/unaligned.h>

#include <asm/ptrace.h>
#include <asm/orlix_tcti.h>

#include "../decode_aarch64.h"
#include "../sve_crypto.h"
#include "crypto_sve_sme_target_contract.h"

struct sve_crypto_decode_case {
	u32 ordinal;
	u32 instruction;
	enum orlix_tcti_sve_crypto_op op;
	enum orlix_tcti_sve_crypto_condition condition;
	u8 nregs;
	u8 element_bytes;
	u8 index;
};

#define SVE_CRYPTO_TEST_SVC 0xd4000001U

struct sve_crypto_resume_context {
	struct mm_struct *mm;
	struct orlix_tcti_sve_state sve;
	unsigned long simd[ARRAY_SIZE(current->thread.user_simd)];
};

static int sve_crypto_resume_init(struct kunit *test)
{
	struct sve_crypto_resume_context *context;

	context = kunit_kzalloc(test, sizeof(*context), GFP_KERNEL);
	if (!context)
		return -ENOMEM;
	context->mm = mm_alloc();
	if (!context->mm)
		return -ENOMEM;
	memcpy(&context->sve, &current->thread.user_sve, sizeof(context->sve));
	memcpy(context->simd, current->thread.user_simd, sizeof(context->simd));
	kthread_use_mm(context->mm);
	test->priv = context;
	return 0;
}

static void sve_crypto_resume_exit(struct kunit *test)
{
	struct sve_crypto_resume_context *context = test->priv;

	if (!context)
		return;
	memcpy(&current->thread.user_sve, &context->sve, sizeof(context->sve));
	memcpy(current->thread.user_simd, context->simd, sizeof(context->simd));
	kthread_unuse_mm(context->mm);
	mmput(context->mm);
}

static int sve_crypto_write_program(unsigned long address, const u32 *program)
{
	int ret;

	ret = sys_mprotect(address, PAGE_SIZE, PROT_READ | PROT_WRITE);
	if (ret)
		return ret;
	ret = orlix_tcti_write_user_data(current->mm, address, program,
		2 * sizeof(*program));
	if (ret)
		return ret;
	return sys_mprotect(address, PAGE_SIZE, PROT_READ | PROT_EXEC);
}

/* One legal canonical encoding per exact #147 source leaf. */
static const struct sve_crypto_decode_case sve_crypto_decode_cases[] = {
	{ 104, 0x04283464U, ORLIX_TCTI_SVE_CRYPTO_XAR, ORLIX_TCTI_SVE_CRYPTO_COND_SVE2_OR_SME, 1, 1, 8 },
	{ 105, 0x04223864U, ORLIX_TCTI_SVE_CRYPTO_EOR3, ORLIX_TCTI_SVE_CRYPTO_COND_SVE2_OR_SME, 1, 16, 0 },
	{ 106, 0x04623864U, ORLIX_TCTI_SVE_CRYPTO_BCAX, ORLIX_TCTI_SVE_CRYPTO_COND_SVE2_OR_SME, 1, 16, 0 },
	{ 682, 0x4520e004U, ORLIX_TCTI_SVE_CRYPTO_AESMC, ORLIX_TCTI_SVE_CRYPTO_COND_SVE_AES, 1, 16, 0 },
	{ 683, 0x4520e404U, ORLIX_TCTI_SVE_CRYPTO_AESIMC, ORLIX_TCTI_SVE_CRYPTO_COND_SVE_AES, 1, 16, 0 },
	{ 684, 0x4522e064U, ORLIX_TCTI_SVE_CRYPTO_AESE, ORLIX_TCTI_SVE_CRYPTO_COND_SVE_AES, 1, 16, 0 },
	{ 685, 0x4522e464U, ORLIX_TCTI_SVE_CRYPTO_AESD, ORLIX_TCTI_SVE_CRYPTO_COND_SVE_AES, 1, 16, 0 },
	{ 686, 0x4523e064U, ORLIX_TCTI_SVE_CRYPTO_SM4E, ORLIX_TCTI_SVE_CRYPTO_COND_SVE_SM4, 1, 16, 0 },
	{ 687, 0x4532e864U, ORLIX_TCTI_SVE_CRYPTO_AESE, ORLIX_TCTI_SVE_CRYPTO_COND_SVE_AES2, 2, 16, 2 },
	{ 688, 0x4532ec64U, ORLIX_TCTI_SVE_CRYPTO_AESD, ORLIX_TCTI_SVE_CRYPTO_COND_SVE_AES2, 2, 16, 2 },
	{ 689, 0x4533e864U, ORLIX_TCTI_SVE_CRYPTO_AESEMC, ORLIX_TCTI_SVE_CRYPTO_COND_SVE_AES2, 2, 16, 2 },
	{ 690, 0x4533ec64U, ORLIX_TCTI_SVE_CRYPTO_AESDIMC, ORLIX_TCTI_SVE_CRYPTO_COND_SVE_AES2, 2, 16, 2 },
	{ 691, 0x4536e864U, ORLIX_TCTI_SVE_CRYPTO_AESE, ORLIX_TCTI_SVE_CRYPTO_COND_SVE_AES2, 4, 16, 2 },
	{ 692, 0x4536ec64U, ORLIX_TCTI_SVE_CRYPTO_AESD, ORLIX_TCTI_SVE_CRYPTO_COND_SVE_AES2, 4, 16, 2 },
	{ 693, 0x4537e864U, ORLIX_TCTI_SVE_CRYPTO_AESEMC, ORLIX_TCTI_SVE_CRYPTO_COND_SVE_AES2, 4, 16, 2 },
	{ 694, 0x4537ec64U, ORLIX_TCTI_SVE_CRYPTO_AESDIMC, ORLIX_TCTI_SVE_CRYPTO_COND_SVE_AES2, 4, 16, 2 },
	{ 695, 0x4522f064U, ORLIX_TCTI_SVE_CRYPTO_SM4EKEY, ORLIX_TCTI_SVE_CRYPTO_COND_SVE_SM4, 1, 16, 0 },
	{ 696, 0x4522f464U, ORLIX_TCTI_SVE_CRYPTO_RAX1, ORLIX_TCTI_SVE_CRYPTO_COND_SVE_SHA3, 1, 16, 0 },
};

static void sve_crypto_sync_low(struct orlix_tcti_sve_state *state,
		unsigned long *simd, u8 reg)
{
	memcpy(&simd[reg * 2], state->z[reg], 16);
}

static void sve_crypto_prepare_vl(struct kunit *test,
		struct orlix_tcti_sve_state *state, unsigned long *simd,
		struct pt_regs *regs, u16 vl_bytes)
{
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sve_state_reset(state, simd, vl_bytes));
	state->crypto_feature_state = ORLIX_TCTI_SVE_CRYPTO_FEATURES_ACTIVE_PROFILE;
	state->crypto_features = ORLIX_TCTI_SVE_CRYPTO_FEAT_SVE2 |
		ORLIX_TCTI_SVE_CRYPTO_FEAT_SME |
		ORLIX_TCTI_SVE_CRYPTO_FEAT_SVE_AES |
		ORLIX_TCTI_SVE_CRYPTO_FEAT_SVE_AES2 |
		ORLIX_TCTI_SVE_CRYPTO_FEAT_SVE_SM4 |
		ORLIX_TCTI_SVE_CRYPTO_FEAT_SVE_SHA3;
	regs->pc = 0x4000;
}

static void sve_crypto_prepare(struct kunit *test,
		struct orlix_tcti_sve_state *state, unsigned long *simd,
		struct pt_regs *regs)
{
	sve_crypto_prepare_vl(test, state, simd, regs, 16);
}

static void sve_crypto_sync_sources(struct orlix_tcti_sve_state *state,
		unsigned long *simd, u8 zd, u8 zn, u8 zm, u8 zk)
{
	sve_crypto_sync_low(state, simd, zd);
	if (zn != zd)
		sve_crypto_sync_low(state, simd, zn);
	if (zm != zd && zm != zn)
		sve_crypto_sync_low(state, simd, zm);
	if (zk != zd && zk != zn && zk != zm)
		sve_crypto_sync_low(state, simd, zk);
}

static void sve_crypto_exact_leaf_decodes(struct kunit *test)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(sve_crypto_decode_cases); index++) {
		const struct sve_crypto_decode_case *test_case =
			&sve_crypto_decode_cases[index];
		struct orlix_tcti_sve_crypto_instruction decoded;
		struct orlix_tcti_decoded_instruction aarch64;

		KUNIT_ASSERT_EQ_MSG(test, 0,
			orlix_tcti_decode_sve_crypto(test_case->instruction, &decoded),
			"source ordinal %u", test_case->ordinal);
		KUNIT_EXPECT_EQ(test, test_case->op, decoded.op);
		KUNIT_EXPECT_EQ(test, test_case->condition, decoded.condition);
		KUNIT_EXPECT_EQ(test, test_case->nregs, decoded.nregs);
		KUNIT_EXPECT_EQ(test, test_case->element_bytes, decoded.element_bytes);
		KUNIT_EXPECT_EQ(test, test_case->index, decoded.index);
		switch (test_case->ordinal) {
		case 104U:
			KUNIT_EXPECT_EQ(test, 4U, decoded.zd);
			KUNIT_EXPECT_EQ(test, 4U, decoded.zn);
			KUNIT_EXPECT_EQ(test, 3U, decoded.zm);
			break;
		case 105U:
		case 106U:
			KUNIT_EXPECT_EQ(test, 4U, decoded.zd);
			KUNIT_EXPECT_EQ(test, 4U, decoded.zn);
			KUNIT_EXPECT_EQ(test, 2U, decoded.zm);
			KUNIT_EXPECT_EQ(test, 3U, decoded.zk);
			break;
		case 682U:
		case 683U:
			KUNIT_EXPECT_EQ(test, 4U, decoded.zd);
			KUNIT_EXPECT_EQ(test, 4U, decoded.zn);
			KUNIT_EXPECT_EQ(test, 0U, decoded.zm);
			break;
		case 684U:
		case 685U:
		case 686U:
		case 687U:
		case 688U:
		case 689U:
		case 690U:
		case 691U:
		case 692U:
		case 693U:
		case 694U:
			KUNIT_EXPECT_EQ(test, 4U, decoded.zd);
			KUNIT_EXPECT_EQ(test, 4U, decoded.zn);
			KUNIT_EXPECT_EQ(test, 3U, decoded.zm);
			break;
		case 695U:
			KUNIT_EXPECT_EQ(test, 4U, decoded.zd);
			KUNIT_EXPECT_EQ(test, 3U, decoded.zn);
			KUNIT_EXPECT_EQ(test, 2U, decoded.zm);
			break;
		case 696U:
			KUNIT_EXPECT_EQ(test, 4U, decoded.zd);
			KUNIT_EXPECT_EQ(test, 3U, decoded.zn);
			KUNIT_EXPECT_EQ(test, 2U, decoded.zm);
			break;
		}
		aarch64 = orlix_tcti_decode_aarch64(test_case->instruction);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_SVE_CRYPTO,
			aarch64.decode_class);
		KUNIT_EXPECT_EQ(test, decoded.zd, aarch64.rd);
		KUNIT_EXPECT_EQ(test, decoded.zn, aarch64.rn);
		KUNIT_EXPECT_EQ(test, decoded.zm, aarch64.rm);
		KUNIT_EXPECT_EQ(test, decoded.zk, aarch64.sve_crypto_zk);
	}
}

static void sve_crypto_rejects_misaligned_multivector_destination(struct kunit *test)
{
	struct orlix_tcti_sve_crypto_instruction decoded;

	KUNIT_EXPECT_EQ(test, -ENOENT, orlix_tcti_decode_sve_crypto(0x4522e801U,
		&(struct orlix_tcti_sve_crypto_instruction){}));
	KUNIT_EXPECT_EQ(test, -ENOENT, orlix_tcti_decode_sve_crypto(0x4526e802U,
		&(struct orlix_tcti_sve_crypto_instruction){}));
	KUNIT_EXPECT_EQ(test, -ENOENT, orlix_tcti_decode_sve_crypto(0x4520e020U,
		&(struct orlix_tcti_sve_crypto_instruction){}));
	KUNIT_EXPECT_NE(test, ORLIX_TCTI_DECODE_SVE_CRYPTO,
		orlix_tcti_decode_aarch64(0x4520e020U).decode_class);
	KUNIT_EXPECT_EQ(test, -EINVAL, orlix_tcti_decode_sve_crypto(0x04203400U,
		&(struct orlix_tcti_sve_crypto_instruction){}));
	KUNIT_EXPECT_EQ(test, -EINVAL, orlix_tcti_decode_sve_crypto(0x04273400U,
		&(struct orlix_tcti_sve_crypto_instruction){}));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_decode_sve_crypto(0x043f3400U,
		&decoded));
	KUNIT_EXPECT_EQ(test, 2U, decoded.element_bytes);
	KUNIT_EXPECT_EQ(test, 1U, decoded.index);
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_decode_sve_crypto(0x04a03400U,
		&decoded));
	KUNIT_EXPECT_EQ(test, 8U, decoded.element_bytes);
	KUNIT_EXPECT_EQ(test, 64U, decoded.index);
}

static void sve_crypto_contract_is_exact_sve_147(struct kunit *test)
{
	static const u32 expected_ordinals[] = {
		104U, 105U, 106U, 682U, 683U, 684U, 685U, 686U, 687U,
		688U, 689U, 690U, 691U, 692U, 693U, 694U, 695U, 696U,
	};
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(expected_ordinals); i++) {
		const struct orlix_tcti_crypto_target_contract_row *row = NULL;
		unsigned int row_index;

		for (row_index = 0;
		     row_index < ARRAY_SIZE(orlix_tcti_crypto_sve_sme_target_contract);
		     row_index++)
			if (orlix_tcti_crypto_sve_sme_target_contract[row_index].source_ordinal ==
			    expected_ordinals[i]) {
				row = &orlix_tcti_crypto_sve_sme_target_contract[row_index];
				break;
			}
		KUNIT_ASSERT_NOT_NULL_MSG(test, row, "ordinal=%u", expected_ordinals[i]);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_CRYPTO_TARGET_SVE, row->family);
		KUNIT_EXPECT_EQ(test,
			ORLIX_TCTI_CRYPTO_TARGET_IMPLEMENTED_PROOF_PENDING, row->status);
		KUNIT_EXPECT_NOT_NULL(test, row->asl_operation);
		KUNIT_EXPECT_NE(test, '\0', row->asl_operation[0]);
	}
}

static void sve_crypto_logical_and_rotate_vectors(struct kunit *test)
{
	struct orlix_tcti_sve_state state;
	unsigned long simd[64] = {};
	struct pt_regs regs = {};
	struct orlix_tcti_sve_crypto_instruction instruction = {
		.zd = 0, .zn = 0, .zm = 2, .zk = 1, .nregs = 1, .element_bytes = 8,
	};
	u64 value;

	sve_crypto_prepare(test, &state, simd, &regs);
	memset(state.z[0], 0x55, 16);
	memset(state.z[1], 0x0f, 16);
	memset(state.z[2], 0x33, 16);
	sve_crypto_sync_sources(&state, simd, 0, 0, 2, 1);
	instruction.op = ORLIX_TCTI_SVE_CRYPTO_EOR3;
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_sve_crypto(&state, &regs, simd,
		true, &instruction));
	KUNIT_EXPECT_EQ(test, 0x69U, state.z[0][0]);
	KUNIT_EXPECT_EQ(test, 0x4004UL, regs.pc);

	sve_crypto_prepare(test, &state, simd, &regs);
	memset(state.z[0], 0x55, 16);
	memset(state.z[1], 0x0f, 16);
	memset(state.z[2], 0x33, 16);
	sve_crypto_sync_sources(&state, simd, 0, 0, 2, 1);
	instruction.op = ORLIX_TCTI_SVE_CRYPTO_BCAX;
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_sve_crypto(&state, &regs, simd,
		true, &instruction));
	KUNIT_EXPECT_EQ(test, 0x65U, state.z[0][0]);

	sve_crypto_prepare(test, &state, simd, &regs);
	value = 0x0123456789abcdefULL;
	memcpy(state.z[1], &value, sizeof(value));
	memset(state.z[2], 0, 16);
	instruction.zm = 1;
	sve_crypto_sync_sources(&state, simd, 0, 0, 1, 1);
	instruction.op = ORLIX_TCTI_SVE_CRYPTO_XAR;
	instruction.index = 4;
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_sve_crypto(&state, &regs, simd,
		true, &instruction));
	KUNIT_EXPECT_EQ(test, ror64(value, 4), get_unaligned_le64(state.z[0]));

	sve_crypto_prepare(test, &state, simd, &regs);
	memcpy(state.z[1], &value, sizeof(value));
	instruction.index = 64;
	instruction.element_bytes = 8;
	sve_crypto_sync_sources(&state, simd, 0, 0, 1, 1);
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_sve_crypto(&state, &regs, simd,
		true, &instruction));
	KUNIT_EXPECT_EQ(test, value, get_unaligned_le64(state.z[0]));
}

static void sve_crypto_aes_and_multivector_vectors(struct kunit *test)
{
	static const enum orlix_tcti_sve_crypto_op round_ops[] = {
		ORLIX_TCTI_SVE_CRYPTO_AESE, ORLIX_TCTI_SVE_CRYPTO_AESD,
		ORLIX_TCTI_SVE_CRYPTO_AESEMC, ORLIX_TCTI_SVE_CRYPTO_AESDIMC,
	};
	struct orlix_tcti_sve_state state;
	unsigned long simd[64] = {};
	struct pt_regs regs = {};
	struct orlix_tcti_sve_crypto_instruction instruction = {
		.zd = 0, .zn = 0, .zm = 2, .zk = 2, .nregs = 1, .element_bytes = 16,
	};
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(round_ops); i++) {
		u8 expected = (round_ops[i] == ORLIX_TCTI_SVE_CRYPTO_AESD ||
			       round_ops[i] == ORLIX_TCTI_SVE_CRYPTO_AESDIMC) ? 0x52 : 0x63;

		sve_crypto_prepare(test, &state, simd, &regs);
		instruction.op = round_ops[i];
		sve_crypto_sync_sources(&state, simd, 0, 0, 2, 2);
		KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_sve_crypto(&state, &regs,
			simd, true, &instruction));
		KUNIT_EXPECT_EQ_MSG(test, expected, state.z[0][0], "op=%u", round_ops[i]);
		KUNIT_EXPECT_EQ_MSG(test, expected, state.z[0][15], "op=%u", round_ops[i]);
	}
	for (i = 0; i < 2; i++) {
		sve_crypto_prepare(test, &state, simd, &regs);
		memset(state.z[0], 0x5a, 16);
		sve_crypto_sync_low(&state, simd, 0);
		instruction.op = i ? ORLIX_TCTI_SVE_CRYPTO_AESIMC :
			ORLIX_TCTI_SVE_CRYPTO_AESMC;
		instruction.nregs = 1;
		instruction.zd = 0;
		instruction.zn = 0;
		instruction.zm = 0;
		instruction.zk = 0;
		KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_sve_crypto(&state, &regs,
			simd, true, &instruction));
		KUNIT_EXPECT_EQ(test, 0x5aU, state.z[0][0]);
		KUNIT_EXPECT_EQ(test, 0x5aU, state.z[0][15]);
	}

	for (i = 0; i < ARRAY_SIZE(round_ops); i++) {
		u8 expected = (round_ops[i] == ORLIX_TCTI_SVE_CRYPTO_AESD ||
			       round_ops[i] == ORLIX_TCTI_SVE_CRYPTO_AESDIMC) ? 0x52 : 0x63;

		sve_crypto_prepare(test, &state, simd, &regs);
		instruction.op = round_ops[i];
		instruction.nregs = 2;
		instruction.zd = 4;
		instruction.zn = 4;
		instruction.zm = 2;
		instruction.zk = 2;
		instruction.index = 0;
		sve_crypto_sync_low(&state, simd, 2);
		KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_sve_crypto(&state, &regs,
			simd, true, &instruction));
		KUNIT_EXPECT_EQ_MSG(test, expected, state.z[4][0], "op=%u", round_ops[i]);
		KUNIT_EXPECT_EQ_MSG(test, expected, state.z[5][15], "op=%u", round_ops[i]);
	}

	for (i = 0; i < 2; i++) {
		sve_crypto_prepare(test, &state, simd, &regs);
		instruction.op = i ? ORLIX_TCTI_SVE_CRYPTO_AESDIMC :
			ORLIX_TCTI_SVE_CRYPTO_AESEMC;
		instruction.nregs = 4;
		instruction.zd = 4;
		instruction.zn = 4;
		instruction.zm = 2;
		instruction.zk = 2;
		instruction.index = 0;
		sve_crypto_sync_low(&state, simd, 2);
		KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_sve_crypto(&state, &regs,
			simd, true, &instruction));
		KUNIT_EXPECT_EQ(test, i ? 0x52U : 0x63U, state.z[4][0]);
		KUNIT_EXPECT_EQ(test, i ? 0x52U : 0x63U, state.z[7][15]);
	}
}

static void sve_crypto_aes_overlap_and_scalable_vectors(struct kunit *test)
{
	static const u16 vls[] = { 32, 64, 256 };
	static const u8 aes_sbox_prefix[] = {
		0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5,
		0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
	};
	struct orlix_tcti_sve_state state;
	unsigned long simd[64] = {};
	struct pt_regs regs = {};
	struct orlix_tcti_sve_crypto_instruction instruction = {
		.op = ORLIX_TCTI_SVE_CRYPTO_AESE,
		.condition = ORLIX_TCTI_SVE_CRYPTO_COND_SVE_AES,
		.zd = 0, .zn = 0, .zm = 2, .zk = 2, .nregs = 1, .element_bytes = 16,
	};
	unsigned int vector;

	for (vector = 0; vector < ARRAY_SIZE(vls); vector++) {
		u16 offset;
		u8 byte;

		sve_crypto_prepare_vl(test, &state, simd, &regs, vls[vector]);
		memset(state.z[0], 0, state.vl_bytes);
		for (offset = 0; offset < state.vl_bytes; offset++)
			state.z[2][offset] = offset / 16;
		memset(state.z[31], 0xa5, state.vl_bytes);
		sve_crypto_sync_sources(&state, simd, 0, 0, 2, 2);
		KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_sve_crypto(&state, &regs,
			simd, true, &instruction));
		for (offset = 0; offset < state.vl_bytes; offset += 16)
			for (byte = 0; byte < 16; byte++)
				KUNIT_EXPECT_EQ_MSG(test, aes_sbox_prefix[offset / 16],
					state.z[0][offset + byte], "vl=%u byte=%u",
					state.vl_bytes, offset + byte);
		for (offset = 0; offset < state.vl_bytes; offset++)
			KUNIT_EXPECT_EQ(test, 0xa5U, state.z[31][offset]);
	}

	sve_crypto_prepare(test, &state, simd, &regs);
	memset(state.z[0], 1, 16);
	instruction.zn = 0;
	instruction.zm = 0;
	sve_crypto_sync_low(&state, simd, 0);
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_sve_crypto(&state, &regs,
		simd, true, &instruction));
	KUNIT_EXPECT_EQ(test, 0x63U, state.z[0][0]);
	KUNIT_EXPECT_EQ(test, 0x63U, state.z[0][15]);

	sve_crypto_prepare_vl(test, &state, simd, &regs, 32);
	memset(state.z[4], 1, state.vl_bytes);
	memset(state.z[5], 2, state.vl_bytes);
	sve_crypto_sync_low(&state, simd, 4);
	sve_crypto_sync_low(&state, simd, 5);
	instruction.condition = ORLIX_TCTI_SVE_CRYPTO_COND_SVE_AES2;
	instruction.zd = 4;
	instruction.zn = 4;
	instruction.zm = 4;
	instruction.nregs = 2;
	instruction.index = 0;
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_sve_crypto(&state, &regs,
		simd, true, &instruction));
	KUNIT_EXPECT_EQ(test, 0x63U, state.z[4][0]);
	KUNIT_EXPECT_EQ(test, 0x7bU, state.z[5][0]);
	KUNIT_EXPECT_EQ(test, 0x63U, state.z[4][16]);
	KUNIT_EXPECT_EQ(test, 0x7bU, state.z[5][16]);
}

static void sve_crypto_sm4_vectors(struct kunit *test)
{
	static const enum orlix_tcti_sve_crypto_op sm4_ops[] = {
		ORLIX_TCTI_SVE_CRYPTO_SM4E, ORLIX_TCTI_SVE_CRYPTO_SM4EKEY,
	};
	struct orlix_tcti_sve_state state;
	unsigned long simd[64] = {};
	struct pt_regs regs = {};
	struct orlix_tcti_sve_crypto_instruction instruction = {
		.zd = 0, .zn = 0, .zm = 2, .zk = 1, .nregs = 1, .element_bytes = 16,
	};
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(sm4_ops); i++) {
		sve_crypto_prepare(test, &state, simd, &regs);
		memset(state.z[2], 0, 16);
		instruction.op = sm4_ops[i];
		instruction.zn = i ? 1 : 0;
		instruction.zk = i ? 0 : 1;
		sve_crypto_sync_sources(&state, simd, 0, instruction.zn,
			instruction.zm, instruction.zk);
		KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_sve_crypto(&state, &regs,
			simd, true, &instruction));
		KUNIT_EXPECT_NE_MSG(test, 0U, get_unaligned_le32(state.z[0]),
			"op=%u must apply its four source-bound SM4 rounds", sm4_ops[i]);
		KUNIT_EXPECT_EQ(test, 0x4004UL, regs.pc);
	}
}

static void sve_crypto_rax1_updates_state_and_pc(struct kunit *test)
{
	struct orlix_tcti_sve_state state;
	unsigned long simd[64];
	struct pt_regs regs = { .pc = 0x4000 };
	struct orlix_tcti_sve_crypto_instruction instruction = {
		.op = ORLIX_TCTI_SVE_CRYPTO_RAX1,
		.condition = ORLIX_TCTI_SVE_CRYPTO_COND_SVE_SHA3,
		.zd = 0, .zn = 1, .zm = 2,
		.zk = 0, .nregs = 1, .element_bytes = 16,
	};

	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sve_state_reset(&state, simd, 16));
	regs.pstate = 0xa0000000UL;
	put_unaligned_le64(0x0123456789abcdefULL, state.z[1]);
	put_unaligned_le64(0x8000000000000001ULL, state.z[2]);
	sve_crypto_sync_low(&state, simd, 1);
	sve_crypto_sync_low(&state, simd, 2);
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_sve_crypto(&state, &regs,
		simd, true, &instruction));
	KUNIT_EXPECT_EQ(test, 0x4004UL, regs.pc);
	KUNIT_EXPECT_EQ(test, 0xa0000000UL, regs.pstate);
	KUNIT_EXPECT_EQ(test, 0x0123456789abcdecULL,
		get_unaligned_le64(state.z[0]));
}

static void sve_crypto_feature_off_preserves_state_and_pc(struct kunit *test)
{
	struct orlix_tcti_sve_state state;
	struct orlix_tcti_sve_state before;
	unsigned long simd[64];
	unsigned long simd_before[64];
	size_t index;

	for (index = 0; index < ARRAY_SIZE(sve_crypto_decode_cases); index++) {
		struct pt_regs regs = { .pc = 0x4000 };
		struct orlix_tcti_sve_crypto_instruction instruction;

		KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sve_state_reset(&state, simd, 16));
		KUNIT_ASSERT_EQ(test, 0, orlix_tcti_decode_sve_crypto(
			sve_crypto_decode_cases[index].instruction, &instruction));
		memset(state.z, 0x5a, sizeof(state.z));
		before = state;
		memcpy(simd_before, simd, sizeof(simd));
		KUNIT_EXPECT_EQ_MSG(test, -EOPNOTSUPP,
			orlix_tcti_execute_sve_crypto(&state, &regs, simd, false,
				&instruction), "ordinal=%u",
			sve_crypto_decode_cases[index].ordinal);
		KUNIT_EXPECT_EQ(test, 0x4000UL, regs.pc);
		KUNIT_EXPECT_MEMEQ(test, &before, &state, sizeof(state));
		KUNIT_EXPECT_MEMEQ(test, simd_before, simd, sizeof(simd));
	}
}

static void sve_crypto_resume_user_executes_and_rejects_all_leaves(struct kunit *test)
{
	unsigned long text = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	size_t index;

	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(text));
	for (index = 0; index < ARRAY_SIZE(sve_crypto_decode_cases); index++) {
		const struct sve_crypto_decode_case *leaf = &sve_crypto_decode_cases[index];
		const u32 program[] = { leaf->instruction, SVE_CRYPTO_TEST_SVC };
		struct pt_regs regs = { .pc = text, .pstate = PSR_MODE_EL0t,
			.syscallno = NO_SYSCALL };
		struct pt_regs before;
		struct orlix_tcti_sve_state sve_before;
		unsigned long simd_before[ARRAY_SIZE(current->thread.user_simd)];
		struct orlix_tcti_result result;

		KUNIT_ASSERT_EQ(test, 0, sve_crypto_write_program(text, program));
		KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sve_state_reset(
			&current->thread.user_sve, current->thread.user_simd, 16));
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason,
			"source ordinal %u", leaf->ordinal);

		KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sve_state_reset(
			&current->thread.user_sve, current->thread.user_simd, 16));
		regs.pc = text;
		before = regs;
		sve_before = current->thread.user_sve;
		memcpy(simd_before, current->thread.user_simd, sizeof(simd_before));
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
			result.reason, "source ordinal %u", leaf->ordinal);
		KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, result.status);
		KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
		KUNIT_EXPECT_MEMEQ(test, &sve_before, &current->thread.user_sve,
			sizeof(sve_before));
		KUNIT_EXPECT_MEMEQ(test, simd_before, current->thread.user_simd,
			sizeof(simd_before));
	}
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(text, PAGE_SIZE));
}

static struct kunit_case sve_crypto_test_cases[] = {
	KUNIT_CASE(sve_crypto_exact_leaf_decodes),
	KUNIT_CASE(sve_crypto_rejects_misaligned_multivector_destination),
	KUNIT_CASE(sve_crypto_contract_is_exact_sve_147),
	KUNIT_CASE(sve_crypto_logical_and_rotate_vectors),
	KUNIT_CASE(sve_crypto_aes_and_multivector_vectors),
	KUNIT_CASE(sve_crypto_aes_overlap_and_scalable_vectors),
	KUNIT_CASE(sve_crypto_sm4_vectors),
	KUNIT_CASE(sve_crypto_rax1_updates_state_and_pc),
	KUNIT_CASE(sve_crypto_feature_off_preserves_state_and_pc),
	KUNIT_CASE(sve_crypto_resume_user_executes_and_rejects_all_leaves),
	{}
};

static struct kunit_suite sve_crypto_test_suite = {
	.name = "orlix-tcti-sve-crypto",
	.init = sve_crypto_resume_init,
	.exit = sve_crypto_resume_exit,
	.test_cases = sve_crypto_test_cases,
};
kunit_test_suite(sve_crypto_test_suite);
MODULE_LICENSE("GPL");
