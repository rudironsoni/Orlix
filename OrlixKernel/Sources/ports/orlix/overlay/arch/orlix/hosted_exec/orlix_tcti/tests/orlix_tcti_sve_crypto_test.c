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
	{ 104, 0x04283400U, ORLIX_TCTI_SVE_CRYPTO_XAR, ORLIX_TCTI_SVE_CRYPTO_COND_SVE2_OR_SME, 1, 1, 8 },
	{ 105, 0x04223860U, ORLIX_TCTI_SVE_CRYPTO_EOR3, ORLIX_TCTI_SVE_CRYPTO_COND_SVE2_OR_SME, 1, 16, 0 },
	{ 106, 0x04623860U, ORLIX_TCTI_SVE_CRYPTO_BCAX, ORLIX_TCTI_SVE_CRYPTO_COND_SVE2_OR_SME, 1, 16, 0 },
	{ 682, 0x4520e000U, ORLIX_TCTI_SVE_CRYPTO_AESMC, ORLIX_TCTI_SVE_CRYPTO_COND_SVE_AES, 1, 16, 0 },
	{ 683, 0x4520e400U, ORLIX_TCTI_SVE_CRYPTO_AESIMC, ORLIX_TCTI_SVE_CRYPTO_COND_SVE_AES, 1, 16, 0 },
	{ 684, 0x4522e000U, ORLIX_TCTI_SVE_CRYPTO_AESE, ORLIX_TCTI_SVE_CRYPTO_COND_SVE_AES, 1, 16, 0 },
	{ 685, 0x4522e400U, ORLIX_TCTI_SVE_CRYPTO_AESD, ORLIX_TCTI_SVE_CRYPTO_COND_SVE_AES, 1, 16, 0 },
	{ 686, 0x4523e000U, ORLIX_TCTI_SVE_CRYPTO_SM4E, ORLIX_TCTI_SVE_CRYPTO_COND_SVE_SM4, 1, 16, 0 },
	{ 687, 0x4522e800U, ORLIX_TCTI_SVE_CRYPTO_AESE, ORLIX_TCTI_SVE_CRYPTO_COND_SVE_AES2, 2, 16, 0 },
	{ 688, 0x4522ec00U, ORLIX_TCTI_SVE_CRYPTO_AESD, ORLIX_TCTI_SVE_CRYPTO_COND_SVE_AES2, 2, 16, 0 },
	{ 689, 0x4523e800U, ORLIX_TCTI_SVE_CRYPTO_AESEMC, ORLIX_TCTI_SVE_CRYPTO_COND_SVE_AES2, 2, 16, 0 },
	{ 690, 0x4523ec00U, ORLIX_TCTI_SVE_CRYPTO_AESDIMC, ORLIX_TCTI_SVE_CRYPTO_COND_SVE_AES2, 2, 16, 0 },
	{ 691, 0x4526e800U, ORLIX_TCTI_SVE_CRYPTO_AESE, ORLIX_TCTI_SVE_CRYPTO_COND_SVE_AES2, 4, 16, 0 },
	{ 692, 0x4526ec00U, ORLIX_TCTI_SVE_CRYPTO_AESD, ORLIX_TCTI_SVE_CRYPTO_COND_SVE_AES2, 4, 16, 0 },
	{ 693, 0x4527e800U, ORLIX_TCTI_SVE_CRYPTO_AESEMC, ORLIX_TCTI_SVE_CRYPTO_COND_SVE_AES2, 4, 16, 0 },
	{ 694, 0x4527ec00U, ORLIX_TCTI_SVE_CRYPTO_AESDIMC, ORLIX_TCTI_SVE_CRYPTO_COND_SVE_AES2, 4, 16, 0 },
	{ 695, 0x4522f020U, ORLIX_TCTI_SVE_CRYPTO_SM4EKEY, ORLIX_TCTI_SVE_CRYPTO_COND_SVE_SM4, 1, 16, 0 },
	{ 696, 0x4520f400U, ORLIX_TCTI_SVE_CRYPTO_RAX1, ORLIX_TCTI_SVE_CRYPTO_COND_SVE_SHA3, 1, 16, 0 },
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
	regs->pc = 0x4000;
}

static void sve_crypto_prepare(struct kunit *test,
		struct orlix_tcti_sve_state *state, unsigned long *simd,
		struct pt_regs *regs)
{
	sve_crypto_prepare_vl(test, state, simd, regs, 16);
}

static void sve_crypto_sync_sources(struct orlix_tcti_sve_state *state,
		unsigned long *simd, u8 zd, u8 zn, u8 zm)
{
	sve_crypto_sync_low(state, simd, zd);
	if (zn != zd)
		sve_crypto_sync_low(state, simd, zn);
	if (zm != zd && zm != zn)
		sve_crypto_sync_low(state, simd, zm);
}

static void sve_crypto_exact_leaf_decodes(struct kunit *test)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(sve_crypto_decode_cases); index++) {
		const struct sve_crypto_decode_case *test_case =
			&sve_crypto_decode_cases[index];
		struct orlix_tcti_sve_crypto_instruction decoded;

		KUNIT_ASSERT_EQ_MSG(test, 0,
			orlix_tcti_decode_sve_crypto(test_case->instruction, &decoded),
			"source ordinal %u", test_case->ordinal);
		KUNIT_EXPECT_EQ(test, test_case->op, decoded.op);
		KUNIT_EXPECT_EQ(test, test_case->condition, decoded.condition);
		KUNIT_EXPECT_EQ(test, test_case->nregs, decoded.nregs);
		KUNIT_EXPECT_EQ(test, test_case->element_bytes, decoded.element_bytes);
		KUNIT_EXPECT_EQ(test, test_case->index, decoded.index);
		if (test_case->ordinal == 105U || test_case->ordinal == 106U) {
			KUNIT_EXPECT_EQ(test, 0U, decoded.zn);
			KUNIT_EXPECT_EQ(test, 2U, decoded.zm);
			KUNIT_EXPECT_EQ(test, 3U, decoded.zk);
		}
		if (test_case->ordinal == 695U) {
			KUNIT_EXPECT_EQ(test, 0U, decoded.zd);
			KUNIT_EXPECT_EQ(test, 1U, decoded.zn);
			KUNIT_EXPECT_EQ(test, 2U, decoded.zm);
		}
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_SVE_CRYPTO,
			orlix_tcti_decode_aarch64(test_case->instruction).decode_class);
	}
}

static void sve_crypto_rejects_misaligned_multivector_destination(struct kunit *test)
{
	struct orlix_tcti_sve_crypto_instruction decoded;

	KUNIT_EXPECT_EQ(test, -EINVAL, orlix_tcti_decode_sve_crypto(0x4522e801U,
		&(struct orlix_tcti_sve_crypto_instruction){}));
	KUNIT_EXPECT_EQ(test, -EINVAL, orlix_tcti_decode_sve_crypto(0x4526e802U,
		&(struct orlix_tcti_sve_crypto_instruction){}));
	KUNIT_EXPECT_EQ(test, -ENOENT, orlix_tcti_decode_sve_crypto(0x4520e020U,
		&(struct orlix_tcti_sve_crypto_instruction){}));
	KUNIT_EXPECT_NE(test, ORLIX_TCTI_DECODE_SVE_CRYPTO,
		orlix_tcti_decode_aarch64(0x4520e020U).decode_class);
	KUNIT_EXPECT_EQ(test, -EINVAL, orlix_tcti_decode_sve_crypto(0x04203400U,
		&(struct orlix_tcti_sve_crypto_instruction){}));
	KUNIT_EXPECT_EQ(test, -EINVAL, orlix_tcti_decode_sve_crypto(0x04273400U,
		&(struct orlix_tcti_sve_crypto_instruction){}));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_decode_sve_crypto(0x046f3400U,
		&decoded));
	KUNIT_EXPECT_EQ(test, 4U, decoded.element_bytes);
	KUNIT_EXPECT_EQ(test, 1U, decoded.index);
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
	sve_crypto_sync_sources(&state, simd, 0, 1, 2);
	instruction.op = ORLIX_TCTI_SVE_CRYPTO_EOR3;
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_sve_crypto(&state, &regs, simd, &instruction));
	KUNIT_EXPECT_EQ(test, 0x69U, state.z[0][0]);
	KUNIT_EXPECT_EQ(test, 0x4004UL, regs.pc);

	sve_crypto_prepare(test, &state, simd, &regs);
	memset(state.z[0], 0x55, 16);
	memset(state.z[1], 0x0f, 16);
	memset(state.z[2], 0x33, 16);
	sve_crypto_sync_sources(&state, simd, 0, 1, 2);
	instruction.op = ORLIX_TCTI_SVE_CRYPTO_BCAX;
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_sve_crypto(&state, &regs, simd, &instruction));
	KUNIT_EXPECT_EQ(test, 0x65U, state.z[0][0]);

	sve_crypto_prepare(test, &state, simd, &regs);
	value = 0x0123456789abcdefULL;
	memcpy(state.z[1], &value, sizeof(value));
	memset(state.z[2], 0, 16);
	sve_crypto_sync_sources(&state, simd, 0, 1, 2);
	instruction.op = ORLIX_TCTI_SVE_CRYPTO_XAR;
	instruction.index = 4;
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_sve_crypto(&state, &regs, simd, &instruction));
	KUNIT_EXPECT_EQ(test, ror64(value, 4), get_unaligned_le64(state.z[0]));
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
		.zd = 0, .zn = 1, .zm = 2, .nregs = 1, .element_bytes = 16,
	};
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(round_ops); i++) {
		u8 expected = (round_ops[i] == ORLIX_TCTI_SVE_CRYPTO_AESD ||
			       round_ops[i] == ORLIX_TCTI_SVE_CRYPTO_AESDIMC) ? 0x52 : 0x63;

		sve_crypto_prepare(test, &state, simd, &regs);
		instruction.op = round_ops[i];
		sve_crypto_sync_sources(&state, simd, 0, 1, 2);
		KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_sve_crypto(&state, &regs,
			simd, &instruction));
		KUNIT_EXPECT_EQ_MSG(test, expected, state.z[0][0], "op=%u", round_ops[i]);
		KUNIT_EXPECT_EQ_MSG(test, expected, state.z[0][15], "op=%u", round_ops[i]);
	}

	sve_crypto_prepare(test, &state, simd, &regs);
	memset(state.z[1], 0, 16);
	memset(state.z[2], 1, 16);
	sve_crypto_sync_sources(&state, simd, 0, 1, 2);
	instruction.op = ORLIX_TCTI_SVE_CRYPTO_AESE;
	instruction.nregs = 1;
	instruction.zd = 0;
	instruction.zn = 1;
	instruction.zm = 2;
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_sve_crypto(&state, &regs,
		simd, &instruction));
	KUNIT_EXPECT_EQ(test, 0x7cU, state.z[0][0]);
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
		KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_sve_crypto(&state, &regs,
			simd, &instruction));
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
		instruction.index = 0;
		sve_crypto_sync_low(&state, simd, 2);
		KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_sve_crypto(&state, &regs,
			simd, &instruction));
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
		instruction.index = 0;
		sve_crypto_sync_low(&state, simd, 2);
		KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_sve_crypto(&state, &regs,
			simd, &instruction));
		KUNIT_EXPECT_EQ(test, i ? 0x52U : 0x63U, state.z[4][0]);
		KUNIT_EXPECT_EQ(test, i ? 0x52U : 0x63U, state.z[7][15]);
	}
}

static void sve_crypto_aes_overlap_and_scalable_vectors(struct kunit *test)
{
	static const u16 vls[] = { 32, 64, 256 };
	struct orlix_tcti_sve_state state;
	unsigned long simd[64] = {};
	struct pt_regs regs = {};
	struct orlix_tcti_sve_crypto_instruction instruction = {
		.op = ORLIX_TCTI_SVE_CRYPTO_AESE,
		.condition = ORLIX_TCTI_SVE_CRYPTO_COND_SVE_AES,
		.zd = 0, .zn = 1, .zm = 2, .nregs = 1, .element_bytes = 16,
	};
	unsigned int vector;

	for (vector = 0; vector < ARRAY_SIZE(vls); vector++) {
		u16 offset;
		u8 byte;

		sve_crypto_prepare_vl(test, &state, simd, &regs, vls[vector]);
		memset(state.z[0], 0, state.vl_bytes);
		memset(state.z[2], 0, state.vl_bytes);
		memset(state.z[31], 0xa5, state.vl_bytes);
		sve_crypto_sync_sources(&state, simd, 0, 1, 2);
		KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_sve_crypto(&state, &regs,
			simd, &instruction));
		for (offset = 0; offset < state.vl_bytes; offset += 16)
			for (byte = 0; byte < 16; byte++)
				KUNIT_EXPECT_EQ_MSG(test, 0x63U,
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
		simd, &instruction));
	KUNIT_EXPECT_EQ(test, 0x7dU, state.z[0][0]);
	KUNIT_EXPECT_EQ(test, 0x7dU, state.z[0][15]);

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
		simd, &instruction));
	KUNIT_EXPECT_EQ(test, 0x7dU, state.z[4][0]);
	KUNIT_EXPECT_EQ(test, 0x77U, state.z[5][0]);
	KUNIT_EXPECT_EQ(test, 0x7dU, state.z[4][16]);
	KUNIT_EXPECT_EQ(test, 0x77U, state.z[5][16]);

	/* DDI0602 ignores index[1] below a 512-bit VL, including VL=384. */
	sve_crypto_prepare_vl(test, &state, simd, &regs, 48);
	memset(state.z[2], 0, state.vl_bytes);
	memset(state.z[4], 0, state.vl_bytes);
	memset(state.z[5], 0, state.vl_bytes);
	memset(&state.z[2][0], 0x11, 16);
	memset(&state.z[2][16], 0x22, 16);
	memset(&state.z[2][32], 0x33, 16);
	sve_crypto_sync_low(&state, simd, 2);
	instruction.op = ORLIX_TCTI_SVE_CRYPTO_AESE;
	instruction.nregs = 2;
	instruction.zd = 4;
	instruction.zn = 4;
	instruction.zm = 2;
	instruction.index = 3;
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_sve_crypto(&state, &regs,
		simd, &instruction));
	for (vector = 0; vector < 2; vector++) {
		u16 offset;

		for (offset = 0; offset < state.vl_bytes; offset += 16)
			KUNIT_EXPECT_EQ_MSG(test, 0x93U,
				state.z[4 + vector][offset], "reg=%u offset=%u",
				vector, offset);
	}
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
		.zd = 0, .zn = 1, .zm = 2, .nregs = 1, .element_bytes = 16,
	};
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(sm4_ops); i++) {
		sve_crypto_prepare(test, &state, simd, &regs);
		memset(state.z[2], 0, 16);
		sve_crypto_sync_sources(&state, simd, 0, 1, 2);
		instruction.op = sm4_ops[i];
		KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_sve_crypto(&state, &regs,
			simd, &instruction));
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
		.op = ORLIX_TCTI_SVE_CRYPTO_RAX1, .zd = 0, .zn = 1, .zm = 2,
		.nregs = 1, .element_bytes = 16,
	};

	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sve_state_reset(&state, simd, 16));
	regs.pstate = 0xa0000000UL;
	put_unaligned_le64(0x0123456789abcdefULL, state.z[1]);
	put_unaligned_le64(0x8000000000000001ULL, state.z[2]);
	sve_crypto_sync_low(&state, simd, 1);
	sve_crypto_sync_low(&state, simd, 2);
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_sve_crypto(&state, &regs,
		simd, &instruction));
	KUNIT_EXPECT_EQ(test, 0x4004UL, regs.pc);
	KUNIT_EXPECT_EQ(test, 0xa0000000UL, regs.pstate);
	KUNIT_EXPECT_EQ(test, 0x0123456789abcdecULL,
		get_unaligned_le64(state.z[0]));
}

static void sve_crypto_xar_full_width_rotations_resume(struct kunit *test)
{
	static const struct {
		u32 instruction;
		u8 element_bytes;
		u8 rotation;
	} cases[] = {
		{ 0x04283400U, 1U, 8U },
		{ 0x04303400U, 2U, 16U },
		{ 0x04603400U, 4U, 32U },
		{ 0x04a03400U, 8U, 64U },
	};
	unsigned long text = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	unsigned int i;

	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(text));
	for (i = 0; i < ARRAY_SIZE(cases); i++) {
		const u32 program[] = { cases[i].instruction, SVE_CRYPTO_TEST_SVC };
		struct orlix_tcti_sve_crypto_instruction decoded;
		struct pt_regs regs = { .pc = text, .pstate = PSR_MODE_EL0t,
			.syscallno = NO_SYSCALL };
		struct orlix_tcti_result result;
		u8 byte;

		KUNIT_ASSERT_EQ(test, 0, orlix_tcti_decode_sve_crypto(
			cases[i].instruction, &decoded));
		KUNIT_EXPECT_EQ(test, cases[i].element_bytes, decoded.element_bytes);
		KUNIT_EXPECT_EQ(test, cases[i].rotation, decoded.index);
		KUNIT_ASSERT_EQ(test, 0, sve_crypto_write_program(text, program));
		KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sve_state_reset(
			&current->thread.user_sve, current->thread.user_simd, 16));
		memset(current->thread.user_sve.z[0], 0xa5, 16);
		memset(current->thread.user_sve.z[2], 0x3c, 16);
		sve_crypto_sync_low(&current->thread.user_sve,
			current->thread.user_simd, 0);
		sve_crypto_sync_low(&current->thread.user_sve,
			current->thread.user_simd, 2);
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
		KUNIT_EXPECT_EQ(test, text + sizeof(u32), regs.pc);
		for (byte = 0; byte < 16; byte++)
			KUNIT_EXPECT_EQ_MSG(test, 0x99U,
				current->thread.user_sve.z[0][byte],
				"element_bytes=%u byte=%u", cases[i].element_bytes, byte);
		for (byte = 0; byte < 16; byte++)
			KUNIT_EXPECT_EQ(test, 0x3cU,
				current->thread.user_sve.z[2][byte]);
	}
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(text, PAGE_SIZE));
}

/*
 * DDI0602 2026-06 SVE2 crypto Operation results, written as independent
 * constants.  This fixture must only use the RX resume path.  In particular,
 * it must not call the executor or an implementation helper to derive a
 * result.  The multi-vector AES fixtures use i2 == 2: Z1's selected 128-bit
 * key slot is zero, while its other slots have distinct non-zero bytes.
 */
static void sve_crypto_resume_user_checks_all_leaf_golden_results(struct kunit *test)
{
	static const u8 xar_left[16] = {
		0xef, 0xcd, 0xab, 0x89, 0x67, 0x45, 0x23, 0x01,
		0xef, 0xcd, 0xab, 0x89, 0x67, 0x45, 0x23, 0x01,
	};
	static const u8 xar_right[16] = {
		0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
		0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
	};
	static const u8 xar_expected[16] = {
		0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10, 0xfe,
		0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10, 0xfe,
	};
	static const u8 sm4e_left[16] = {
		0x33, 0x22, 0x11, 0x00, 0x77, 0x66, 0x55, 0x44,
		0xbb, 0xaa, 0x99, 0x88, 0xff, 0xee, 0xdd, 0xcc,
	};
	static const u8 sm4e_right[16] = {
		0x3c, 0x2d, 0x1e, 0x0f, 0x78, 0x69, 0x5a, 0x4b,
		0xb4, 0xa5, 0x96, 0x87, 0xf0, 0xe1, 0xd2, 0xc3,
	};
	static const u8 sm4e_expected[16] = {
		0x27, 0x36, 0x05, 0x14, 0xef, 0xfe, 0xcd, 0xdc,
		0xf0, 0xe1, 0xd2, 0xc3, 0xbb, 0xaa, 0x99, 0x88,
	};
	static const u8 sm4ekey_left[16] = {
		0x43, 0x32, 0x21, 0x10, 0x87, 0x76, 0x65, 0x54,
		0xcb, 0xba, 0xa9, 0x98, 0x0f, 0xfe, 0xed, 0xdc,
	};
	static const u8 sm4ekey_right[16] = {
		0xfe, 0xca, 0xad, 0x0b, 0x78, 0x56, 0x34, 0x12,
		0xf0, 0xde, 0xbc, 0x9a, 0xdf, 0x9b, 0x57, 0x13,
	};
	static const u8 sm4ekey_expected[16] = {
		0x4c, 0xb3, 0xe6, 0xaf, 0x27, 0x90, 0xcb, 0xe8,
		0x3b, 0x66, 0x12, 0x5a, 0xf0, 0x7d, 0xca, 0x16,
	};
	static const u8 rax1_expected[16] = {
		0xec, 0xcd, 0xab, 0x89, 0x67, 0x45, 0x23, 0x01,
		0x68, 0x98, 0x94, 0xe1, 0xb4, 0x49, 0x5a, 0x2d,
	};
	unsigned long text = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	size_t index;

	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(text));
	for (index = 0; index < ARRAY_SIZE(sve_crypto_decode_cases); index++) {
		const struct sve_crypto_decode_case *leaf = &sve_crypto_decode_cases[index];
		u32 instruction = leaf->instruction;
		const u8 *expected = NULL;
		u8 expected_byte = 0;
		u16 vl_bytes = 16;
		u32 program[] = { 0, SVE_CRYPTO_TEST_SVC };
		struct pt_regs regs = { .pc = text, .pstate = PSR_MODE_EL0t,
			.syscallno = NO_SYSCALL };
		struct orlix_tcti_sve_crypto_instruction decoded;
		u8 before[ORLIX_TCTI_SVE_ZREG_COUNT][ORLIX_TCTI_SVE_MAX_VL_BYTES];
		u8 reg;
		u16 offset;
		struct orlix_tcti_result result;

		switch (leaf->ordinal) {
		case 104:
			instruction |= 4U | (1U << 5);
			expected = xar_expected;
			break;
		case 105:
		case 106:
			instruction |= 4U;
			expected_byte = leaf->ordinal == 105U ? 0x69U : 0x65U;
			break;
		case 682:
		case 683:
			instruction |= 4U;
			expected_byte = 0x5aU;
			break;
		case 684:
		case 685:
			instruction |= 4U | (1U << 5);
			expected_byte = leaf->ordinal == 684U ? 0x63U : 0x52U;
			break;
		case 686:
			instruction |= 4U | (1U << 5);
			expected = sm4e_expected;
			break;
		case 687:
		case 688:
		case 689:
		case 690:
		case 691:
		case 692:
		case 693:
		case 694:
			instruction |= 4U | (1U << 5) | (2U << 19);
			vl_bytes = 64;
			expected_byte = leaf->ordinal == 688U || leaf->ordinal == 690U ||
				leaf->ordinal == 692U || leaf->ordinal == 694U ? 0x52U : 0x63U;
			break;
		case 695:
			instruction |= 4U;
			expected = sm4ekey_expected;
			break;
		case 696:
			instruction |= 4U | (1U << 5) | (2U << 16);
			expected = rax1_expected;
			break;
		}
		program[0] = instruction;
		KUNIT_ASSERT_EQ(test, 0, sve_crypto_write_program(text, program));
		KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sve_state_reset(
			&current->thread.user_sve, current->thread.user_simd, vl_bytes));
		for (reg = 0; reg < ORLIX_TCTI_SVE_ZREG_COUNT; reg++) {
			u16 byte;

			for (byte = 0; byte < vl_bytes; byte++)
				current->thread.user_sve.z[reg][byte] = reg * 17U + byte;
			sve_crypto_sync_low(&current->thread.user_sve,
				current->thread.user_simd, reg);
		}
		if (leaf->ordinal == 104U) {
			memcpy(current->thread.user_sve.z[4], xar_left, sizeof(xar_left));
			memcpy(current->thread.user_sve.z[1], xar_right, sizeof(xar_right));
		} else if (leaf->ordinal == 105U || leaf->ordinal == 106U) {
			memset(current->thread.user_sve.z[4], 0x55, 16);
			memset(current->thread.user_sve.z[2], 0x33, 16);
			memset(current->thread.user_sve.z[3], 0x0f, 16);
		} else if (leaf->ordinal == 682U || leaf->ordinal == 683U) {
			memset(current->thread.user_sve.z[4], 0x5a, 16);
		} else if (leaf->ordinal == 684U || leaf->ordinal == 685U) {
			memset(current->thread.user_sve.z[4], 0, 16);
			memset(current->thread.user_sve.z[1], 0, 16);
		} else if (leaf->ordinal == 686U) {
			memcpy(current->thread.user_sve.z[4], sm4e_left, sizeof(sm4e_left));
			memcpy(current->thread.user_sve.z[1], sm4e_right, sizeof(sm4e_right));
		} else if (leaf->ordinal >= 687U && leaf->ordinal <= 694U) {
			for (reg = 4; reg < 4 + leaf->nregs; reg++)
				memset(current->thread.user_sve.z[reg], 0, vl_bytes);
			memset(current->thread.user_sve.z[1], 0x11, 16);
			memset(current->thread.user_sve.z[1] + 16, 0x22, 16);
			memset(current->thread.user_sve.z[1] + 32, 0, 16);
			memset(current->thread.user_sve.z[1] + 48, 0x44, 16);
		} else if (leaf->ordinal == 695U) {
			memcpy(current->thread.user_sve.z[1], sm4ekey_left,
				sizeof(sm4ekey_left));
			memcpy(current->thread.user_sve.z[2], sm4ekey_right,
				sizeof(sm4ekey_right));
		} else if (leaf->ordinal == 696U) {
			put_unaligned_le64(0x0123456789abcdefULL,
				current->thread.user_sve.z[1]);
			put_unaligned_le64(0x0f1e2d3c4b5a6978ULL,
				current->thread.user_sve.z[1] + 8);
			put_unaligned_le64(0x8000000000000001ULL,
				current->thread.user_sve.z[2]);
			put_unaligned_le64(0x1122334455667788ULL,
				current->thread.user_sve.z[2] + 8);
		}
		KUNIT_ASSERT_EQ(test, 0, orlix_tcti_decode_sve_crypto(
			instruction, &decoded));
		for (reg = 0; reg < ORLIX_TCTI_SVE_ZREG_COUNT; reg++)
			sve_crypto_sync_low(&current->thread.user_sve,
				current->thread.user_simd, reg);
		memcpy(before, current->thread.user_sve.z, sizeof(before));
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason,
			"source ordinal %u", leaf->ordinal);
		KUNIT_EXPECT_EQ_MSG(test, text + sizeof(u32), regs.pc,
			"source ordinal %u", leaf->ordinal);
		for (reg = 0; reg < decoded.nregs; reg++) {
			for (offset = 0; offset < vl_bytes; offset += 16) {
				if (expected) {
					KUNIT_EXPECT_MEMEQ(test, expected,
						current->thread.user_sve.z[decoded.zd + reg] + offset,
						16);
				} else {
					u8 byte;

					for (byte = 0; byte < 16; byte++)
						KUNIT_EXPECT_EQ_MSG(test, expected_byte,
							current->thread.user_sve.z[decoded.zd + reg][offset + byte],
							"source ordinal %u destination Z%u byte %u",
							leaf->ordinal, decoded.zd + reg, offset + byte);
				}
			}
			KUNIT_EXPECT_MEMEQ(test,
				&current->thread.user_simd[(decoded.zd + reg) * 2],
				current->thread.user_sve.z[decoded.zd + reg], 16);
		}
		for (reg = 0; reg < ORLIX_TCTI_SVE_ZREG_COUNT; reg++) {
			if (reg >= decoded.zd && reg < decoded.zd + decoded.nregs)
				continue;
			KUNIT_EXPECT_MEMEQ(test, before[reg],
				current->thread.user_sve.z[reg], vl_bytes);
		}
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
	KUNIT_CASE(sve_crypto_xar_full_width_rotations_resume),
	KUNIT_CASE(sve_crypto_resume_user_checks_all_leaf_golden_results),
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
