// SPDX-License-Identifier: GPL-2.0-only
/*
 * Production-path proof for the 32 ADVSIMD_CRYPTO leaves owned by GitHub #123.
 * The crypto-decode-boundary and crypto-extension-decode-boundary suites remain
 * regression and do not receive #120 close credit. PMUL, TBL/TBX, and SVE/SME
 * AES stay off this suite. Crypto HWCAP bits are not advertised.
 */
#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/orlix_tcti.h>
#include <kunit/test.h>
#include <linux/bitops.h>
#include <linux/compiler.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/string.h>
#include <linux/syscalls.h>

#include "../decode_aarch64.h"
#include "../switch_debug.h"
#include "orlix_tcti_memory_proof.h"
#include "orlix_tcti_native_observation.h"
#include "orlix_tcti_advsimd_crypto_production_capture.h"
#include "target_native_proof_contract_private.h"
#include "target_proof_ingestion_private.h"

#define CRYPTO_SVC 0xd4000001U
#define CRYPTO_NZCV (PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT | PSR_V_BIT)
#define CRYPTO_FAMILY_COUNT 32U
#define CRYPTO_RD 0U
#define CRYPTO_RN 1U
#define CRYPTO_RM 2U
#define CRYPTO_RA 3U

enum orlix_tcti_test_source_family {
#define ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_SOURCE(...)
#define ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_COUNTS(...)
#define ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_MEMBER(...)
#define ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_FAMILY(symbol, ...) \
	ORLIX_TCTI_TEST_SOURCE_FAMILY_##symbol,
#include "../isa/target_execution_slice_map.def"
#undef ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_SOURCE
#undef ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_COUNTS
#undef ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_MEMBER
#undef ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_FAMILY
};

static const u8 orlix_tcti_test_source_families[] = {
#define ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_SOURCE(...)
#define ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_COUNTS(...)
#define ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_FAMILY(...)
#define ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_MEMBER(i, name, condition, family) \
	[i] = ORLIX_TCTI_TEST_SOURCE_FAMILY_##family,
#include "../isa/target_execution_slice_map.def"
#undef ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_SOURCE
#undef ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_COUNTS
#undef ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_FAMILY
#undef ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_MEMBER
};

struct orlix_tcti_test_crypto_source {
	u32 ordinal;
	const char *name;
	const char *mnemonic;
	const char *operation;
	u32 mask;
	u32 pattern;
	const char *condition_tcnd_hex;
};

#define ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE(...)
#define ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW(i, name, mnemonic, operation, mask, \
	pattern, condition, ...) { i, name, mnemonic, operation, mask, pattern, condition },
static const struct orlix_tcti_test_crypto_source orlix_tcti_test_sources[] = {
#include "../isa/source_manifest.def"
};
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW

static const bool orlix_tcti_test_has_ddi0602_semantics[] = {
#define ORLIX_TCTI_A64_SEMANTIC_PROVENANCE_SOURCE(...)
#define ORLIX_TCTI_A64_DDI0602_PROVENANCE_ROW(i, ...) [i] = true,
#define ORLIX_TCTI_A64_OFFICIAL_SEMANTICS_NOT_SPECIFIED_ROW(i, ...) [i] = false,
#include "../isa/target_asl_availability.def"
#undef ORLIX_TCTI_A64_SEMANTIC_PROVENANCE_SOURCE
#undef ORLIX_TCTI_A64_DDI0602_PROVENANCE_ROW
#undef ORLIX_TCTI_A64_OFFICIAL_SEMANTICS_NOT_SPECIFIED_ROW
};

static bool orlix_tcti_test_is_advsimd_crypto(
	const struct orlix_tcti_test_crypto_source *source)
{
	return source->ordinal < ARRAY_SIZE(orlix_tcti_test_source_families) &&
		orlix_tcti_test_source_families[source->ordinal] ==
			ORLIX_TCTI_TEST_SOURCE_FAMILY_ADVSIMD_CRYPTO;
}

static const struct orlix_tcti_test_crypto_source *
orlix_tcti_test_crypto_name(const char *name)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++)
		if (orlix_tcti_test_is_advsimd_crypto(
			    &orlix_tcti_test_sources[index]) &&
		    !strcmp(orlix_tcti_test_sources[index].name, name))
			return &orlix_tcti_test_sources[index];
	return NULL;
}

static u32 orlix_tcti_advsimd_crypto_legal_instruction(
	const struct orlix_tcti_test_crypto_source *source)
{
	u32 instruction = source->pattern & source->mask;

	instruction |= (CRYPTO_RN << 5) | CRYPTO_RD;
	if ((source->mask & (0x1fU << 16)) == 0)
		instruction |= CRYPTO_RM << 16;
	if ((source->mask & (0x1fU << 10)) == 0)
		instruction |= CRYPTO_RA << 10;
	return instruction;
}

static int orlix_tcti_advsimd_crypto_test_init(struct kunit *test)
{
	struct mm_struct *mm = mm_alloc();

	if (!mm)
		return -ENOMEM;
	kthread_use_mm(mm);
	test->priv = mm;
	return 0;
}

static void orlix_tcti_advsimd_crypto_test_exit(struct kunit *test)
{
	struct mm_struct *mm = test->priv;

	if (!mm)
		return;
	kthread_unuse_mm(mm);
	mmput(mm);
}

static unsigned long orlix_tcti_advsimd_crypto_map(struct kunit *test,
						   u32 instruction)
{
	u8 bytes[16] = {};
	u32 program[2] = { instruction, CRYPTO_SVC };

	memcpy(bytes, program, sizeof(program));
	return orlix_tcti_memory_proof_map_bytes(test, bytes, sizeof(bytes),
						 PROT_READ | PROT_EXEC);
}

static void orlix_tcti_advsimd_crypto_seed_simd(void)
{
	size_t index;

	current->thread.user_simd_valid = 1;
	current->thread.user_fpcr = 0;
	current->thread.user_fpsr = 0;
	for (index = 0; index < ARRAY_SIZE(current->thread.user_simd); index++)
		current->thread.user_simd[index] =
			0x0102030405060708ULL ^ ((u64)index << 32);
}

static void orlix_tcti_advsimd_crypto_seed(struct pt_regs *regs,
					   unsigned long code)
{
	memset(regs, 0, sizeof(*regs));
	regs->pc = code;
	regs->pstate = PSR_MODE_EL0t | PSR_Z_BIT;
	regs->syscallno = NO_SYSCALL;
	regs->regs[0] = 0x1111111111111111ULL;
	regs->regs[1] = 0x2222222222222222ULL;
	regs->regs[2] = 0x3333333333333333ULL;
	regs->regs[30] = 0x4444444444444444ULL;
	orlix_tcti_advsimd_crypto_seed_simd();
}

static void orlix_tcti_advsimd_crypto_expect_success(struct kunit *test,
	const struct orlix_tcti_test_crypto_source *source,
	const struct orlix_tcti_result *result, const struct pt_regs *regs,
	unsigned long code)
{
	KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_SYSCALL, result->reason,
			    "%s ordinal %u reason", source->name, source->ordinal);
	KUNIT_EXPECT_EQ(test, 0L, result->status);
	KUNIT_EXPECT_EQ(test, CRYPTO_SVC, result->instruction);
	KUNIT_EXPECT_EQ_MSG(test, code + sizeof(u32), regs->pc,
			    "%s ordinal %u pc", source->name, source->ordinal);
}

static void orlix_tcti_advsimd_crypto_capture_run(struct kunit *test,
	const struct orlix_tcti_test_crypto_source *source, u32 obligation,
	struct pt_regs *regs, unsigned long code)
{
	const void *token;
	struct orlix_tcti_native_capture_session *capture = NULL;
	struct orlix_tcti_native_wire_record wire = {};
	struct orlix_tcti_target_proof_ingestion_ledger *ledger;
	struct orlix_tcti_result result;
	int ret;

	token = orlix_tcti_advsimd_crypto_production_capture_token(
		source->ordinal, obligation);
	KUNIT_EXPECT_TRUE_MSG(test, token != NULL, "%s token %u", source->name,
			      obligation);
	if (!token)
		return;
	ret = orlix_tcti_native_capture_begin(token, source->ordinal, obligation,
					      &capture);
	KUNIT_EXPECT_EQ_MSG(test, 0, ret, "%s begin %u", source->name, obligation);
	result = orlix_tcti_resume_user(current, regs, current->mm);
	orlix_tcti_advsimd_crypto_expect_success(test, source, &result, regs,
						 code);
	ret = orlix_tcti_native_capture_take_wire(capture, &wire);
	KUNIT_EXPECT_EQ_MSG(test, 0, ret, "%s wire %u", source->name, obligation);
	if (!ret) {
		ledger = orlix_tcti_target_proof_ingestion_ledger_create(1U);
		KUNIT_ASSERT_NOT_NULL(test, ledger);
		KUNIT_EXPECT_EQ_MSG(test, 0,
			orlix_tcti_target_proof_ingest_native(ledger, &wire, NULL),
			"%s ingest %u", source->name, obligation);
		KUNIT_EXPECT_EQ(test, 1U, ledger->native_passed);
		KUNIT_EXPECT_LT_MSG(test,
			orlix_tcti_target_proof_ingest_native(ledger, &wire, NULL),
			0, "%s replay %u", source->name, obligation);
		KUNIT_EXPECT_EQ(test, 1U, ledger->native_passed);
		orlix_tcti_target_proof_ingestion_ledger_destroy(ledger);
	}
	orlix_tcti_native_wire_record_destroy(&wire);
	orlix_tcti_native_capture_destroy(capture);
}

static void orlix_tcti_advsimd_crypto_decodes_exact_source_cohort(
	struct kunit *test)
{
	size_t index;
	size_t count = 0;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++) {
		const struct orlix_tcti_test_crypto_source *source =
			&orlix_tcti_test_sources[index];
		struct orlix_tcti_decoded_instruction decoded;
		u32 instruction;

		if (!orlix_tcti_test_is_advsimd_crypto(source))
			continue;
		instruction = orlix_tcti_advsimd_crypto_legal_instruction(source);
		KUNIT_ASSERT_EQ_MSG(test, source->pattern,
				instruction & source->mask, "%s", source->name);
		decoded = orlix_tcti_decode_aarch64(instruction);
		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC,
			decoded.decode_class, "%s class %u insn %#x",
			source->name, decoded.decode_class, instruction);
		KUNIT_EXPECT_EQ_MSG(test, source->ordinal, decoded.source_ordinal,
				"%s %#x", source->name, instruction);
		count++;
	}
	KUNIT_EXPECT_EQ(test, CRYPTO_FAMILY_COUNT, count);
}

static void orlix_tcti_advsimd_crypto_binds_pinned_ddi0602_semantics(
	struct kunit *test)
{
	size_t index;
	size_t ddi = 0;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++) {
		const struct orlix_tcti_test_crypto_source *source =
			&orlix_tcti_test_sources[index];

		if (!orlix_tcti_test_is_advsimd_crypto(source))
			continue;
		KUNIT_ASSERT_LT_MSG(test, source->ordinal,
			ARRAY_SIZE(orlix_tcti_test_has_ddi0602_semantics), "%s",
			source->name);
		KUNIT_EXPECT_TRUE_MSG(test,
			orlix_tcti_test_has_ddi0602_semantics[source->ordinal], "%s",
			source->name);
		ddi++;
	}
	KUNIT_EXPECT_EQ(test, CRYPTO_FAMILY_COUNT, ddi);
}

static void orlix_tcti_advsimd_crypto_production_resume(struct kunit *test)
{
	size_t index;
	size_t seen = 0;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++) {
		const struct orlix_tcti_test_crypto_source *source =
			&orlix_tcti_test_sources[index];
		u32 instruction;
		struct orlix_tcti_decoded_instruction decoded;
		struct pt_regs regs = {};
		unsigned long code;

		if (!orlix_tcti_test_is_advsimd_crypto(source))
			continue;
		seen++;
		instruction = orlix_tcti_advsimd_crypto_legal_instruction(source);
		decoded = orlix_tcti_decode_aarch64(instruction);
		KUNIT_ASSERT_EQ_MSG(test, ORLIX_TCTI_DECODE_SIMD_VECTOR_ARITHMETIC,
			decoded.decode_class, "%s class %u", source->name,
			decoded.decode_class);
		KUNIT_ASSERT_EQ_MSG(test, source->ordinal, decoded.source_ordinal,
				    "%s ordinal", source->name);
		code = orlix_tcti_advsimd_crypto_map(test, instruction);
		orlix_tcti_advsimd_crypto_seed(&regs, code);
		orlix_tcti_advsimd_crypto_capture_run(test, source,
			ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FP_SIMD, &regs, code);
		orlix_tcti_advsimd_crypto_seed(&regs, code);
		orlix_tcti_advsimd_crypto_capture_run(test, source,
			ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC, &regs, code);
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
	}
	KUNIT_EXPECT_EQ(test, CRYPTO_FAMILY_COUNT, seen);
}

static void orlix_tcti_advsimd_crypto_destructive_alias(struct kunit *test)
{
	const struct orlix_tcti_test_crypto_source *aese =
		orlix_tcti_test_crypto_name("AESE_B_cryptoaes");
	const struct orlix_tcti_test_crypto_source *eor3 =
		orlix_tcti_test_crypto_name("EOR3_VVV16_crypto4");
	u32 insn;
	unsigned long code;
	struct pt_regs regs = {};
	struct orlix_tcti_result result;
	u64 before0;
	u64 before1;

	KUNIT_ASSERT_NOT_NULL(test, aese);
	KUNIT_ASSERT_NOT_NULL(test, eor3);

	insn = (aese->pattern & aese->mask) | (CRYPTO_RD << 5) | CRYPTO_RD;
	code = orlix_tcti_advsimd_crypto_map(test, insn);
	orlix_tcti_advsimd_crypto_seed(&regs, code);
	current->thread.user_simd[CRYPTO_RD * 2U] = 0x0123456789abcdefULL;
	current->thread.user_simd[CRYPTO_RD * 2U + 1U] = 0xfedcba9876543210ULL;
	before0 = current->thread.user_simd[CRYPTO_RD * 2U];
	before1 = current->thread.user_simd[CRYPTO_RD * 2U + 1U];
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_TRUE(test,
		current->thread.user_simd[CRYPTO_RD * 2U] != before0 ||
		current->thread.user_simd[CRYPTO_RD * 2U + 1U] != before1);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));

	insn = (eor3->pattern & eor3->mask) | (CRYPTO_RN << 5) |
		(CRYPTO_RM << 16) | (CRYPTO_RA << 10) | CRYPTO_RD;
	code = orlix_tcti_advsimd_crypto_map(test, insn);
	orlix_tcti_advsimd_crypto_seed(&regs, code);
	current->thread.user_simd[CRYPTO_RD * 2U] = 0;
	current->thread.user_simd[CRYPTO_RN * 2U] = 0x1111111111111111ULL;
	current->thread.user_simd[CRYPTO_RM * 2U] = 0x2222222222222222ULL;
	current->thread.user_simd[CRYPTO_RA * 2U] = 0x4444444444444444ULL;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, 0x7777777777777777ULL,
			current->thread.user_simd[CRYPTO_RD * 2U]);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
}

static void orlix_tcti_advsimd_crypto_pmull_halves_polynomial(struct kunit *test)
{
	const struct orlix_tcti_test_crypto_source *pmull =
		orlix_tcti_test_crypto_name("PMULL_asimddiff_L");
	u32 insn;
	unsigned long code;
	struct pt_regs regs = {};
	struct orlix_tcti_decoded_instruction decoded;
	struct orlix_tcti_result result;
	u64 dest_q0[2];
	u64 dest_q1[2];
	size_t index;

	KUNIT_ASSERT_NOT_NULL(test, pmull);

	insn = (pmull->pattern & pmull->mask) | (CRYPTO_RN << 5) |
		(CRYPTO_RM << 16) | CRYPTO_RD | (3U << 22);
	decoded = orlix_tcti_decode_aarch64(insn);
	KUNIT_EXPECT_EQ(test, pmull->ordinal, decoded.source_ordinal);
	KUNIT_EXPECT_EQ(test, 0U, decoded.simd_source_index);
	code = orlix_tcti_advsimd_crypto_map(test, insn);
	orlix_tcti_advsimd_crypto_seed(&regs, code);
	for (index = 0; index < ARRAY_SIZE(current->thread.user_simd); index++)
		current->thread.user_simd[index] = 0;
	current->thread.user_simd[CRYPTO_RN * 2U] = 0xffffffffffffffffULL;
	current->thread.user_simd[CRYPTO_RM * 2U] = 0xffffffffffffffffULL;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	dest_q0[0] = current->thread.user_simd[CRYPTO_RD * 2U];
	dest_q0[1] = current->thread.user_simd[CRYPTO_RD * 2U + 1U];
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));

	insn |= BIT(30);
	decoded = orlix_tcti_decode_aarch64(insn);
	KUNIT_EXPECT_EQ(test, pmull->ordinal, decoded.source_ordinal);
	KUNIT_EXPECT_EQ(test, 1U, decoded.simd_source_index);
	code = orlix_tcti_advsimd_crypto_map(test, insn);
	orlix_tcti_advsimd_crypto_seed(&regs, code);
	for (index = 0; index < ARRAY_SIZE(current->thread.user_simd); index++)
		current->thread.user_simd[index] = 0;
	current->thread.user_simd[CRYPTO_RN * 2U + 1U] = 0xffffffffffffffffULL;
	current->thread.user_simd[CRYPTO_RM * 2U + 1U] = 0xffffffffffffffffULL;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	dest_q1[0] = current->thread.user_simd[CRYPTO_RD * 2U];
	dest_q1[1] = current->thread.user_simd[CRYPTO_RD * 2U + 1U];
	KUNIT_EXPECT_TRUE(test, dest_q0[0] != 0 || dest_q0[1] != 0);
	KUNIT_EXPECT_TRUE(test, dest_q1[0] != 0 || dest_q1[1] != 0);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));

	insn = (pmull->pattern & pmull->mask) | (CRYPTO_RN << 5) |
		(CRYPTO_RM << 16) | CRYPTO_RD;
	code = orlix_tcti_advsimd_crypto_map(test, insn);
	orlix_tcti_advsimd_crypto_seed(&regs, code);
	for (index = 0; index < ARRAY_SIZE(current->thread.user_simd); index++)
		current->thread.user_simd[index] = 0;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, 0ULL, current->thread.user_simd[CRYPTO_RD * 2U]);
	KUNIT_EXPECT_EQ(test, 0ULL, current->thread.user_simd[CRYPTO_RD * 2U + 1U]);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
}

static void orlix_tcti_advsimd_crypto_aes_round_upper_gprs(struct kunit *test)
{
	const struct orlix_tcti_test_crypto_source *aese =
		orlix_tcti_test_crypto_name("AESE_B_cryptoaes");
	u32 insn;
	unsigned long code;
	struct pt_regs regs = {};
	struct pt_regs before;
	struct orlix_tcti_result result;
	u64 simd_other;
	u64 flags;

	KUNIT_ASSERT_NOT_NULL(test, aese);
	insn = orlix_tcti_advsimd_crypto_legal_instruction(aese);
	code = orlix_tcti_advsimd_crypto_map(test, insn);
	orlix_tcti_advsimd_crypto_seed(&regs, code);
	before = regs;
	simd_other = current->thread.user_simd[4];
	flags = regs.pstate & CRYPTO_NZCV;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, before.regs[0], regs.regs[0]);
	KUNIT_EXPECT_EQ(test, before.regs[1], regs.regs[1]);
	KUNIT_EXPECT_EQ(test, before.regs[30], regs.regs[30]);
	KUNIT_EXPECT_EQ(test, flags, regs.pstate & CRYPTO_NZCV);
	KUNIT_EXPECT_EQ(test, simd_other, current->thread.user_simd[4]);
	KUNIT_EXPECT_EQ(test, 1, current->thread.user_simd_valid);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
}

static void orlix_tcti_advsimd_crypto_reserved_encodings(struct kunit *test)
{
	struct orlix_tcti_decoded_instruction decoded;
	u32 insn;
	unsigned long code;
	struct pt_regs regs = {};
	struct pt_regs before;
	struct orlix_tcti_result result;

	insn = 0x0e20e000U | (1U << 22);
	decoded = orlix_tcti_decode_aarch64(insn);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED, decoded.decode_class);
	code = orlix_tcti_advsimd_crypto_map(test, insn);
	orlix_tcti_advsimd_crypto_seed(&regs, code);
	before = regs;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION, result.reason);
	KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));

	insn = 0x0e209c00U;
	decoded = orlix_tcti_decode_aarch64(insn);
	KUNIT_EXPECT_NE(test, 3883U, decoded.source_ordinal);
}

static struct kunit_case orlix_tcti_advsimd_crypto_source_bound_cases[] = {
	KUNIT_CASE(orlix_tcti_advsimd_crypto_decodes_exact_source_cohort),
	KUNIT_CASE(orlix_tcti_advsimd_crypto_binds_pinned_ddi0602_semantics),
	KUNIT_CASE(orlix_tcti_advsimd_crypto_production_resume),
	KUNIT_CASE(orlix_tcti_advsimd_crypto_destructive_alias),
	KUNIT_CASE(orlix_tcti_advsimd_crypto_pmull_halves_polynomial),
	KUNIT_CASE(orlix_tcti_advsimd_crypto_aes_round_upper_gprs),
	KUNIT_CASE(orlix_tcti_advsimd_crypto_reserved_encodings),
	{}
};

static struct kunit_suite orlix_tcti_advsimd_crypto_source_bound_suite = {
	.name = "orlix-tcti-advsimd-crypto-source-bound",
	.init = orlix_tcti_advsimd_crypto_test_init,
	.exit = orlix_tcti_advsimd_crypto_test_exit,
	.test_cases = orlix_tcti_advsimd_crypto_source_bound_cases,
};

kunit_test_suite(orlix_tcti_advsimd_crypto_source_bound_suite);
