// SPDX-License-Identifier: GPL-2.0-only
/*
 * Production-path proof for the 16 BASE_MULTIPLY_DIVIDE leaves owned by
 * GitHub #136. The old integer-conditional suite remains regression and
 * does not receive #120 close credit. LSLV stays on variable-shift.
 * FEAT_CPA is not advertised.
 */
#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/orlix_tcti.h>
#include <kunit/test.h>
#include <linux/bitops.h>
#include <linux/compiler.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/limits.h>
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
#include "orlix_tcti_base_multiply_divide_production_capture.h"
#include "target_native_proof_contract_private.h"
#include "target_proof_ingestion_private.h"

#define MDIV_SVC 0xd4000001U
#define MDIV_NZCV (PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT | PSR_V_BIT)
#define MDIV_FAMILY_COUNT 16U
#define MDIV_RD 0U
#define MDIV_RN 1U
#define MDIV_RM 2U
#define MDIV_RA 3U

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

struct orlix_tcti_test_muldiv_source {
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
static const struct orlix_tcti_test_muldiv_source orlix_tcti_test_sources[] = {
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

static bool orlix_tcti_test_is_base_multiply_divide(
	const struct orlix_tcti_test_muldiv_source *source)
{
	return source->ordinal < ARRAY_SIZE(orlix_tcti_test_source_families) &&
		orlix_tcti_test_source_families[source->ordinal] ==
			ORLIX_TCTI_TEST_SOURCE_FAMILY_BASE_MULTIPLY_DIVIDE;
}

static bool orlix_tcti_test_is_divide(
	const struct orlix_tcti_test_muldiv_source *source)
{
	return !strcmp(source->operation, "UDIV") ||
		!strcmp(source->operation, "SDIV");
}

static bool orlix_tcti_test_is_high_half(
	const struct orlix_tcti_test_muldiv_source *source)
{
	return !strcmp(source->operation, "SMULH") ||
		!strcmp(source->operation, "UMULH");
}

static const struct orlix_tcti_test_muldiv_source *
orlix_tcti_test_muldiv_name(const char *name)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++)
		if (orlix_tcti_test_is_base_multiply_divide(
			    &orlix_tcti_test_sources[index]) &&
		    !strcmp(orlix_tcti_test_sources[index].name, name))
			return &orlix_tcti_test_sources[index];
	return NULL;
}

static bool orlix_tcti_test_muldiv_decode_is_family(
	const struct orlix_tcti_test_muldiv_source *source,
	enum orlix_tcti_decode_class decode_class)
{
	if (orlix_tcti_test_is_divide(source))
		return decode_class == ORLIX_TCTI_DECODE_DATA_PROCESSING_2SOURCE;
	return decode_class == ORLIX_TCTI_DECODE_MULTIPLY_ADD_SUB;
}

static u32 orlix_tcti_base_multiply_divide_legal_instruction(
	const struct orlix_tcti_test_muldiv_source *source)
{
	u32 instruction = source->pattern & source->mask;

	instruction |= (MDIV_RN << 5) | (MDIV_RM << 16) | MDIV_RD;
	if (orlix_tcti_test_is_divide(source) ||
	    orlix_tcti_test_is_high_half(source))
		return instruction;
	return instruction | (MDIV_RA << 10);
}

static int orlix_tcti_base_multiply_divide_test_init(struct kunit *test)
{
	struct mm_struct *mm = mm_alloc();

	if (!mm)
		return -ENOMEM;
	kthread_use_mm(mm);
	test->priv = mm;
	return 0;
}

static void orlix_tcti_base_multiply_divide_test_exit(struct kunit *test)
{
	struct mm_struct *mm = test->priv;

	if (!mm)
		return;
	kthread_unuse_mm(mm);
	mmput(mm);
}

static unsigned long orlix_tcti_base_multiply_divide_map(struct kunit *test,
							 u32 instruction)
{
	u8 bytes[16] = {};
	u32 program[2] = { instruction, MDIV_SVC };

	memcpy(bytes, program, sizeof(program));
	return orlix_tcti_memory_proof_map_bytes(test, bytes, sizeof(bytes),
						 PROT_READ | PROT_EXEC);
}

static void orlix_tcti_base_multiply_divide_seed(struct pt_regs *regs,
						 unsigned long code)
{
	memset(regs, 0, sizeof(*regs));
	regs->pc = code;
	regs->pstate = PSR_MODE_EL0t;
	regs->syscallno = NO_SYSCALL;
	regs->regs[0] = 0x1111111111111111ULL;
	regs->regs[1] = 0x0000000700000003ULL;
	regs->regs[2] = 0x0000000500000005ULL;
	regs->regs[3] = 0xaa00000000001000ULL;
	regs->regs[30] = 0x4444444444444444ULL;
	current->thread.user_simd_valid = 1;
	current->thread.user_simd[0] = 0xaaaaaaaaaaaaaaaaULL;
	current->thread.user_simd[1] = 0xbbbbbbbbbbbbbbbbULL;
}

static void orlix_tcti_base_multiply_divide_expect_success(struct kunit *test,
	const struct orlix_tcti_test_muldiv_source *source,
	const struct orlix_tcti_result *result, const struct pt_regs *regs,
	unsigned long code)
{
	KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_SYSCALL, result->reason,
			    "%s ordinal %u reason", source->name, source->ordinal);
	KUNIT_EXPECT_EQ(test, 0L, result->status);
	KUNIT_EXPECT_EQ(test, MDIV_SVC, result->instruction);
	KUNIT_EXPECT_EQ_MSG(test, code + sizeof(u32), regs->pc,
			    "%s ordinal %u pc", source->name, source->ordinal);
}

static void orlix_tcti_base_multiply_divide_capture_run(struct kunit *test,
	const struct orlix_tcti_test_muldiv_source *source, u32 obligation,
	struct pt_regs *regs, unsigned long code)
{
	const void *token;
	struct orlix_tcti_native_capture_session *capture = NULL;
	struct orlix_tcti_native_wire_record wire = {};
	struct orlix_tcti_target_proof_ingestion_ledger *ledger;
	struct orlix_tcti_result result;
	int ret;

	token = orlix_tcti_base_multiply_divide_production_capture_token(
		source->ordinal, obligation);
	KUNIT_EXPECT_TRUE_MSG(test, token != NULL, "%s token %u", source->name,
			      obligation);
	if (!token)
		return;
	ret = orlix_tcti_native_capture_begin(token, source->ordinal, obligation,
					      &capture);
	KUNIT_EXPECT_EQ_MSG(test, 0, ret, "%s begin %u", source->name, obligation);
	result = orlix_tcti_resume_user(current, regs, current->mm);
	orlix_tcti_base_multiply_divide_expect_success(test, source, &result, regs,
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

static void orlix_tcti_base_multiply_divide_decodes_exact_source_cohort(
	struct kunit *test)
{
	size_t index;
	size_t count = 0;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++) {
		const struct orlix_tcti_test_muldiv_source *source =
			&orlix_tcti_test_sources[index];
		struct orlix_tcti_decoded_instruction decoded;
		u32 instruction;

		if (!orlix_tcti_test_is_base_multiply_divide(source))
			continue;
		instruction = orlix_tcti_base_multiply_divide_legal_instruction(source);
		KUNIT_ASSERT_EQ_MSG(test, source->pattern,
				instruction & source->mask, "%s", source->name);
		decoded = orlix_tcti_decode_aarch64(instruction);
		KUNIT_EXPECT_TRUE_MSG(test,
			orlix_tcti_test_muldiv_decode_is_family(source,
				decoded.decode_class),
			"%s class %u insn %#x", source->name, decoded.decode_class,
			instruction);
		KUNIT_EXPECT_EQ_MSG(test, source->ordinal, decoded.source_ordinal,
				"%s %#x", source->name, instruction);
		count++;
	}
	KUNIT_EXPECT_EQ(test, MDIV_FAMILY_COUNT, count);
}

static void orlix_tcti_base_multiply_divide_binds_pinned_ddi0602_semantics(
	struct kunit *test)
{
	size_t index;
	size_t ddi = 0;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++) {
		const struct orlix_tcti_test_muldiv_source *source =
			&orlix_tcti_test_sources[index];

		if (!orlix_tcti_test_is_base_multiply_divide(source))
			continue;
		KUNIT_ASSERT_LT_MSG(test, source->ordinal,
			ARRAY_SIZE(orlix_tcti_test_has_ddi0602_semantics), "%s",
			source->name);
		KUNIT_EXPECT_TRUE_MSG(test,
			orlix_tcti_test_has_ddi0602_semantics[source->ordinal], "%s",
			source->name);
		ddi++;
	}
	KUNIT_EXPECT_EQ(test, MDIV_FAMILY_COUNT, ddi);
}

static void orlix_tcti_base_multiply_divide_production_resume(struct kunit *test)
{
	size_t index;
	size_t seen = 0;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++) {
		const struct orlix_tcti_test_muldiv_source *source =
			&orlix_tcti_test_sources[index];
		u32 instruction;
		struct orlix_tcti_decoded_instruction decoded;
		struct pt_regs regs = {};
		unsigned long code;

		if (!orlix_tcti_test_is_base_multiply_divide(source))
			continue;
		seen++;
		instruction = orlix_tcti_base_multiply_divide_legal_instruction(source);
		decoded = orlix_tcti_decode_aarch64(instruction);
		KUNIT_ASSERT_TRUE_MSG(test,
			orlix_tcti_test_muldiv_decode_is_family(source,
				decoded.decode_class),
			"%s class %u", source->name, decoded.decode_class);
		KUNIT_ASSERT_EQ_MSG(test, source->ordinal, decoded.source_ordinal,
				    "%s ordinal", source->name);
		code = orlix_tcti_base_multiply_divide_map(test, instruction);
		orlix_tcti_base_multiply_divide_seed(&regs, code);
		orlix_tcti_base_multiply_divide_capture_run(test, source,
			ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS, &regs, code);
		orlix_tcti_base_multiply_divide_seed(&regs, code);
		orlix_tcti_base_multiply_divide_capture_run(test, source,
			ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC, &regs, code);
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
	}
	KUNIT_EXPECT_EQ(test, MDIV_FAMILY_COUNT, seen);
}

static void orlix_tcti_base_multiply_divide_divide_zero_signed_min_w_upper(
	struct kunit *test)
{
	const struct orlix_tcti_test_muldiv_source *udiv32 =
		orlix_tcti_test_muldiv_name("UDIV_32_dp_2src");
	const struct orlix_tcti_test_muldiv_source *udiv64 =
		orlix_tcti_test_muldiv_name("UDIV_64_dp_2src");
	const struct orlix_tcti_test_muldiv_source *sdiv32 =
		orlix_tcti_test_muldiv_name("SDIV_32_dp_2src");
	const struct orlix_tcti_test_muldiv_source *sdiv64 =
		orlix_tcti_test_muldiv_name("SDIV_64_dp_2src");
	const struct orlix_tcti_test_muldiv_source *madd32 =
		orlix_tcti_test_muldiv_name("MADD_32A_dp_3src");
	struct {
		const struct orlix_tcti_test_muldiv_source *source;
		u64 rn;
		u64 rm;
		u64 expected;
		bool check_w;
	} cases[6];
	size_t index;

	KUNIT_ASSERT_NOT_NULL(test, udiv32);
	KUNIT_ASSERT_NOT_NULL(test, udiv64);
	KUNIT_ASSERT_NOT_NULL(test, sdiv32);
	KUNIT_ASSERT_NOT_NULL(test, sdiv64);
	KUNIT_ASSERT_NOT_NULL(test, madd32);
	cases[0].source = udiv32;
	cases[0].rn = 0x9abcdef0ULL;
	cases[0].rm = 0;
	cases[0].expected = 0;
	cases[0].check_w = true;
	cases[1].source = udiv64;
	cases[1].rn = 0x9abcdef012345678ULL;
	cases[1].rm = 0;
	cases[1].expected = 0;
	cases[1].check_w = false;
	cases[2].source = sdiv32;
	cases[2].rn = (u32)S32_MIN;
	cases[2].rm = (u32)-1;
	cases[2].expected = (u32)S32_MIN;
	cases[2].check_w = true;
	cases[3].source = sdiv64;
	cases[3].rn = (u64)S64_MIN;
	cases[3].rm = (u64)-1;
	cases[3].expected = (u64)S64_MIN;
	cases[3].check_w = false;
	cases[4].source = sdiv32;
	cases[4].rn = 0xffffffff80000000ULL;
	cases[4].rm = 0;
	cases[4].expected = 0;
	cases[4].check_w = true;
	cases[5].source = madd32;
	cases[5].rn = 0xffffffff00000003ULL;
	cases[5].rm = 0xffffffff00000005ULL;
	cases[5].expected = (u32)(3U * 5U + 0x1000U);
	cases[5].check_w = true;
	for (index = 0; index < ARRAY_SIZE(cases); index++) {
		u32 insn = orlix_tcti_base_multiply_divide_legal_instruction(
			cases[index].source);
		unsigned long code;
		struct pt_regs regs = {};
		struct orlix_tcti_result result;

		code = orlix_tcti_base_multiply_divide_map(test, insn);
		orlix_tcti_base_multiply_divide_seed(&regs, code);
		regs.regs[MDIV_RN] = cases[index].rn;
		regs.regs[MDIV_RM] = cases[index].rm;
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason,
				    "%s vector %u", cases[index].source->name,
				    (unsigned int)index);
		KUNIT_EXPECT_EQ_MSG(test, cases[index].expected, regs.regs[MDIV_RD],
				    "%s vector %u dest", cases[index].source->name,
				    (unsigned int)index);
		if (cases[index].check_w)
			KUNIT_EXPECT_EQ_MSG(test, regs.regs[MDIV_RD] >> 32, 0ULL,
					    "%s W upper", cases[index].source->name);
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
	}
}

static void orlix_tcti_base_multiply_divide_long_form_high_half(struct kunit *test)
{
	const struct orlix_tcti_test_muldiv_source *smaddl =
		orlix_tcti_test_muldiv_name("SMADDL_64WA_dp_3src");
	const struct orlix_tcti_test_muldiv_source *umaddl =
		orlix_tcti_test_muldiv_name("UMADDL_64WA_dp_3src");
	const struct orlix_tcti_test_muldiv_source *smulh =
		orlix_tcti_test_muldiv_name("SMULH_64_dp_3src");
	const struct orlix_tcti_test_muldiv_source *umulh =
		orlix_tcti_test_muldiv_name("UMULH_64_dp_3src");
	u32 insn;
	unsigned long code;
	struct pt_regs regs = {};
	struct orlix_tcti_result result;

	KUNIT_ASSERT_NOT_NULL(test, smaddl);
	KUNIT_ASSERT_NOT_NULL(test, umaddl);
	KUNIT_ASSERT_NOT_NULL(test, smulh);
	KUNIT_ASSERT_NOT_NULL(test, umulh);

	insn = orlix_tcti_base_multiply_divide_legal_instruction(smaddl);
	code = orlix_tcti_base_multiply_divide_map(test, insn);
	orlix_tcti_base_multiply_divide_seed(&regs, code);
	regs.regs[MDIV_RN] = 0xfffffffffffffffeULL;
	regs.regs[MDIV_RM] = 0x0000000000000003ULL;
	regs.regs[MDIV_RA] = 0x10ULL;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, 0x0aULL, regs.regs[MDIV_RD]);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));

	insn = orlix_tcti_base_multiply_divide_legal_instruction(umaddl);
	code = orlix_tcti_base_multiply_divide_map(test, insn);
	orlix_tcti_base_multiply_divide_seed(&regs, code);
	regs.regs[MDIV_RN] = 0xfffffffffffffffeULL;
	regs.regs[MDIV_RM] = 0x0000000000000003ULL;
	regs.regs[MDIV_RA] = 0x10ULL;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, 0x30000000aULL, regs.regs[MDIV_RD]);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));

	insn = orlix_tcti_base_multiply_divide_legal_instruction(smulh);
	code = orlix_tcti_base_multiply_divide_map(test, insn);
	orlix_tcti_base_multiply_divide_seed(&regs, code);
	regs.regs[MDIV_RN] = 0x8000000000000000ULL;
	regs.regs[MDIV_RM] = 0x2ULL;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, 0xffffffffffffffffULL, regs.regs[MDIV_RD]);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));

	insn = orlix_tcti_base_multiply_divide_legal_instruction(umulh);
	code = orlix_tcti_base_multiply_divide_map(test, insn);
	orlix_tcti_base_multiply_divide_seed(&regs, code);
	regs.regs[MDIV_RN] = 0x8000000000000000ULL;
	regs.regs[MDIV_RM] = 0x2ULL;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, 0x1ULL, regs.regs[MDIV_RD]);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
}

static void orlix_tcti_base_multiply_divide_accumulator_alias_xzr(struct kunit *test)
{
	const struct orlix_tcti_test_muldiv_source *madd64 =
		orlix_tcti_test_muldiv_name("MADD_64A_dp_3src");
	u32 insn;
	unsigned long code;
	struct pt_regs regs = {};
	struct orlix_tcti_result result;

	KUNIT_ASSERT_NOT_NULL(test, madd64);

	insn = (madd64->pattern & madd64->mask) | (MDIV_RN << 5) |
		(MDIV_RM << 16) | (MDIV_RD << 10) | MDIV_RD;
	code = orlix_tcti_base_multiply_divide_map(test, insn);
	orlix_tcti_base_multiply_divide_seed(&regs, code);
	regs.regs[MDIV_RD] = 0x8ULL;
	regs.regs[MDIV_RN] = 0x3ULL;
	regs.regs[MDIV_RM] = 0x5ULL;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, 0x17ULL, regs.regs[MDIV_RD]);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));

	insn = (madd64->pattern & madd64->mask) | (31U << 5) | (MDIV_RM << 16) |
		(MDIV_RA << 10) | MDIV_RD;
	code = orlix_tcti_base_multiply_divide_map(test, insn);
	orlix_tcti_base_multiply_divide_seed(&regs, code);
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, 0xaa00000000001000ULL, regs.regs[MDIV_RD]);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));

	insn = (madd64->pattern & madd64->mask) | (MDIV_RN << 5) | (31U << 16) |
		(31U << 10) | MDIV_RD;
	code = orlix_tcti_base_multiply_divide_map(test, insn);
	orlix_tcti_base_multiply_divide_seed(&regs, code);
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, 0ULL, regs.regs[MDIV_RD]);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
}

static void orlix_tcti_base_multiply_divide_simd_and_flags_unchanged(
	struct kunit *test)
{
	const struct orlix_tcti_test_muldiv_source *source =
		orlix_tcti_test_muldiv_name("MADD_32A_dp_3src");
	u32 insn;
	unsigned long code;
	struct pt_regs regs = {};
	struct orlix_tcti_result result;
	u64 simd0;
	u64 simd1;
	u64 flags;

	KUNIT_ASSERT_NOT_NULL(test, source);
	insn = orlix_tcti_base_multiply_divide_legal_instruction(source);
	code = orlix_tcti_base_multiply_divide_map(test, insn);
	orlix_tcti_base_multiply_divide_seed(&regs, code);
	regs.pstate |= PSR_Z_BIT;
	simd0 = current->thread.user_simd[0];
	simd1 = current->thread.user_simd[1];
	flags = regs.pstate & MDIV_NZCV;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, simd0, current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, simd1, current->thread.user_simd[1]);
	KUNIT_EXPECT_EQ(test, flags, regs.pstate & MDIV_NZCV);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
}

static void orlix_tcti_base_multiply_divide_pointer_preserves_tag(struct kunit *test)
{
	const struct orlix_tcti_test_muldiv_source *maddpt =
		orlix_tcti_test_muldiv_name("MADDPT_64A_dp_3src");
	const struct orlix_tcti_test_muldiv_source *msubpt =
		orlix_tcti_test_muldiv_name("MSUBPT_64A_dp_3src");
	u32 insn;
	unsigned long code;
	struct pt_regs regs = {};
	struct orlix_tcti_result result;

	KUNIT_ASSERT_NOT_NULL(test, maddpt);
	KUNIT_ASSERT_NOT_NULL(test, msubpt);

	insn = orlix_tcti_base_multiply_divide_legal_instruction(maddpt);
	code = orlix_tcti_base_multiply_divide_map(test, insn);
	orlix_tcti_base_multiply_divide_seed(&regs, code);
	regs.regs[MDIV_RN] = 0x10ULL;
	regs.regs[MDIV_RM] = 0x2ULL;
	regs.regs[MDIV_RA] = 0xaa00000000001000ULL;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, 0xaa00000000001020ULL, regs.regs[MDIV_RD]);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));

	insn = orlix_tcti_base_multiply_divide_legal_instruction(msubpt);
	code = orlix_tcti_base_multiply_divide_map(test, insn);
	orlix_tcti_base_multiply_divide_seed(&regs, code);
	regs.regs[MDIV_RN] = 0x10ULL;
	regs.regs[MDIV_RM] = 0x2ULL;
	regs.regs[MDIV_RA] = 0xaa00000000001000ULL;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, 0xaa00000000000fe0ULL, regs.regs[MDIV_RD]);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));

	insn = orlix_tcti_base_multiply_divide_legal_instruction(maddpt);
	code = orlix_tcti_base_multiply_divide_map(test, insn);
	orlix_tcti_base_multiply_divide_seed(&regs, code);
	regs.regs[MDIV_RN] = 0x1ULL;
	regs.regs[MDIV_RM] = 0x2ULL;
	regs.regs[MDIV_RA] = 0xaaffffffffffffffULL;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, 0xaa00000000000001ULL, regs.regs[MDIV_RD]);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
}

static void orlix_tcti_base_multiply_divide_reserved_encodings(struct kunit *test)
{
	struct orlix_tcti_decoded_instruction decoded;
	u32 insn;
	unsigned long code;
	struct pt_regs regs = {};
	struct pt_regs before;
	struct orlix_tcti_result result;

	insn = 0x1b600000U;
	decoded = orlix_tcti_decode_aarch64(insn);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED, decoded.decode_class);
	code = orlix_tcti_base_multiply_divide_map(test, insn);
	orlix_tcti_base_multiply_divide_seed(&regs, code);
	before = regs;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION, result.reason);
	KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));

	insn = 0x9b800000U;
	decoded = orlix_tcti_decode_aarch64(insn);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED, decoded.decode_class);

	insn = 0x9b400000U;
	decoded = orlix_tcti_decode_aarch64(insn);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED, decoded.decode_class);
}

static struct kunit_case orlix_tcti_base_multiply_divide_source_bound_cases[] = {
	KUNIT_CASE(orlix_tcti_base_multiply_divide_decodes_exact_source_cohort),
	KUNIT_CASE(orlix_tcti_base_multiply_divide_binds_pinned_ddi0602_semantics),
	KUNIT_CASE(orlix_tcti_base_multiply_divide_production_resume),
	KUNIT_CASE(orlix_tcti_base_multiply_divide_divide_zero_signed_min_w_upper),
	KUNIT_CASE(orlix_tcti_base_multiply_divide_long_form_high_half),
	KUNIT_CASE(orlix_tcti_base_multiply_divide_accumulator_alias_xzr),
	KUNIT_CASE(orlix_tcti_base_multiply_divide_simd_and_flags_unchanged),
	KUNIT_CASE(orlix_tcti_base_multiply_divide_pointer_preserves_tag),
	KUNIT_CASE(orlix_tcti_base_multiply_divide_reserved_encodings),
	{}
};

static struct kunit_suite orlix_tcti_base_multiply_divide_source_bound_suite = {
	.name = "orlix-tcti-base-multiply-divide-source-bound",
	.init = orlix_tcti_base_multiply_divide_test_init,
	.exit = orlix_tcti_base_multiply_divide_test_exit,
	.test_cases = orlix_tcti_base_multiply_divide_source_bound_cases,
};

kunit_test_suite(orlix_tcti_base_multiply_divide_source_bound_suite);
