// SPDX-License-Identifier: GPL-2.0-only
/*
 * Production-path proof for the 25 BASE_BITFIELD_UNARY leaves owned by
 * GitHub #129. The old bitfield-extract and scalar-bitops suites remain
 * regression and do not receive #120 close credit. UDIV/MADD stay on
 * integer-conditional. LSLV stays on variable-shift.
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
#include "orlix_tcti_base_bitfield_unary_production_capture.h"
#include "target_native_proof_contract_private.h"
#include "target_proof_ingestion_private.h"

#define BFU_SVC 0xd4000001U
#define BFU_NZCV (PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT | PSR_V_BIT)
#define BFU_FAMILY_COUNT 25U
#define BFU_EL0_COUNT 19U
#define BFU_NON_EL0_COUNT 6U
#define BFU_RD 0U
#define BFU_RN 1U
#define BFU_RM 2U

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

struct orlix_tcti_test_bitfield_source {
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
static const struct orlix_tcti_test_bitfield_source orlix_tcti_test_sources[] = {
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

static bool orlix_tcti_test_is_base_bitfield_unary(
	const struct orlix_tcti_test_bitfield_source *source)
{
	return source->ordinal < ARRAY_SIZE(orlix_tcti_test_source_families) &&
		orlix_tcti_test_source_families[source->ordinal] ==
			ORLIX_TCTI_TEST_SOURCE_FAMILY_BASE_BITFIELD_UNARY;
}

static bool orlix_tcti_test_is_cssc_unary(
	const struct orlix_tcti_test_bitfield_source *source)
{
	return !strcmp(source->operation, "CTZ") ||
		!strcmp(source->operation, "CNT") ||
		!strcmp(source->operation, "ABS");
}

static bool orlix_tcti_test_is_el0_bitfield(
	const struct orlix_tcti_test_bitfield_source *source)
{
	return orlix_tcti_test_is_base_bitfield_unary(source) &&
		!orlix_tcti_test_is_cssc_unary(source);
}

static const struct orlix_tcti_test_bitfield_source *
orlix_tcti_test_bitfield_name(const char *name)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++)
		if (orlix_tcti_test_is_base_bitfield_unary(
			    &orlix_tcti_test_sources[index]) &&
		    !strcmp(orlix_tcti_test_sources[index].name, name))
			return &orlix_tcti_test_sources[index];
	return NULL;
}

static bool orlix_tcti_test_bitfield_decode_is_family(
	const struct orlix_tcti_test_bitfield_source *source,
	enum orlix_tcti_decode_class decode_class)
{
	if (!strcmp(source->operation, "EXTR"))
		return decode_class == ORLIX_TCTI_DECODE_EXTRACT;
	if (!strcmp(source->operation, "SBFM") ||
	    !strcmp(source->operation, "BFM") ||
	    !strcmp(source->operation, "UBFM"))
		return decode_class == ORLIX_TCTI_DECODE_BITFIELD;
	return decode_class == ORLIX_TCTI_DECODE_DATA_PROCESSING_1SOURCE;
}

static u32 orlix_tcti_base_bitfield_unary_legal_instruction(
	const struct orlix_tcti_test_bitfield_source *source)
{
	u32 instruction = source->pattern & source->mask;

	if (!strcmp(source->operation, "EXTR"))
		return instruction | (BFU_RN << 5) | (BFU_RM << 16) | (8U << 10);
	if (!strcmp(source->operation, "SBFM") ||
	    !strcmp(source->operation, "BFM") ||
	    !strcmp(source->operation, "UBFM"))
		return instruction | (BFU_RN << 5) | (8U << 16) | (15U << 10) |
			BFU_RD;
	return instruction | (BFU_RN << 5) | BFU_RD;
}

static int orlix_tcti_base_bitfield_unary_test_init(struct kunit *test)
{
	struct mm_struct *mm = mm_alloc();

	if (!mm)
		return -ENOMEM;
	kthread_use_mm(mm);
	test->priv = mm;
	return 0;
}

static void orlix_tcti_base_bitfield_unary_test_exit(struct kunit *test)
{
	struct mm_struct *mm = test->priv;

	if (!mm)
		return;
	kthread_unuse_mm(mm);
	mmput(mm);
}

static unsigned long orlix_tcti_base_bitfield_unary_map(struct kunit *test,
							u32 instruction)
{
	u8 bytes[16] = {};
	u32 program[2] = { instruction, BFU_SVC };

	memcpy(bytes, program, sizeof(program));
	return orlix_tcti_memory_proof_map_bytes(test, bytes, sizeof(bytes),
						 PROT_READ | PROT_EXEC);
}

static void orlix_tcti_base_bitfield_unary_seed(struct pt_regs *regs,
						unsigned long code)
{
	memset(regs, 0, sizeof(*regs));
	regs->pc = code;
	regs->pstate = PSR_MODE_EL0t;
	regs->syscallno = NO_SYSCALL;
	regs->regs[0] = 0x1111111111111111ULL;
	regs->regs[1] = 0x8000000180000001ULL;
	regs->regs[2] = 0x0123456789abcdefULL;
	regs->regs[30] = 0x4444444444444444ULL;
	current->thread.user_simd_valid = 1;
	current->thread.user_simd[0] = 0xaaaaaaaaaaaaaaaaULL;
	current->thread.user_simd[1] = 0xbbbbbbbbbbbbbbbbULL;
}

static void orlix_tcti_base_bitfield_unary_expect_success(struct kunit *test,
	const struct orlix_tcti_test_bitfield_source *source,
	const struct orlix_tcti_result *result, const struct pt_regs *regs,
	unsigned long code)
{
	KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_SYSCALL, result->reason,
			    "%s ordinal %u reason", source->name, source->ordinal);
	KUNIT_EXPECT_EQ(test, 0L, result->status);
	KUNIT_EXPECT_EQ(test, BFU_SVC, result->instruction);
	KUNIT_EXPECT_EQ_MSG(test, code + sizeof(u32), regs->pc,
			    "%s ordinal %u pc", source->name, source->ordinal);
}

static void orlix_tcti_base_bitfield_unary_capture_run(struct kunit *test,
	const struct orlix_tcti_test_bitfield_source *source, u32 obligation,
	struct pt_regs *regs, unsigned long code)
{
	const void *token;
	struct orlix_tcti_native_capture_session *capture = NULL;
	struct orlix_tcti_native_wire_record wire = {};
	struct orlix_tcti_target_proof_ingestion_ledger *ledger;
	struct orlix_tcti_result result;
	int ret;

	token = orlix_tcti_base_bitfield_unary_production_capture_token(
		source->ordinal, obligation);
	KUNIT_EXPECT_TRUE_MSG(test, token != NULL, "%s token %u", source->name,
			      obligation);
	if (!token)
		return;
	ret = orlix_tcti_native_capture_begin(token, source->ordinal, obligation,
					      &capture);
	KUNIT_EXPECT_EQ_MSG(test, 0, ret, "%s begin %u", source->name, obligation);
	result = orlix_tcti_resume_user(current, regs, current->mm);
	orlix_tcti_base_bitfield_unary_expect_success(test, source, &result, regs,
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

static void orlix_tcti_base_bitfield_unary_decodes_exact_source_cohort(
	struct kunit *test)
{
	size_t index;
	size_t count = 0;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++) {
		const struct orlix_tcti_test_bitfield_source *source =
			&orlix_tcti_test_sources[index];
		struct orlix_tcti_decoded_instruction decoded;
		u32 instruction;

		if (!orlix_tcti_test_is_base_bitfield_unary(source))
			continue;
		instruction = orlix_tcti_base_bitfield_unary_legal_instruction(source);
		KUNIT_ASSERT_EQ_MSG(test, source->pattern,
				instruction & source->mask, "%s", source->name);
		decoded = orlix_tcti_decode_aarch64(instruction);
		KUNIT_EXPECT_TRUE_MSG(test,
			orlix_tcti_test_bitfield_decode_is_family(source,
				decoded.decode_class),
			"%s class %u insn %#x", source->name, decoded.decode_class,
			instruction);
		KUNIT_EXPECT_EQ_MSG(test, source->ordinal, decoded.source_ordinal,
				"%s %#x", source->name, instruction);
		count++;
	}
	KUNIT_EXPECT_EQ(test, BFU_FAMILY_COUNT, count);
}

static void orlix_tcti_base_bitfield_unary_binds_pinned_ddi0602_semantics(
	struct kunit *test)
{
	size_t index;
	size_t ddi = 0;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++) {
		const struct orlix_tcti_test_bitfield_source *source =
			&orlix_tcti_test_sources[index];

		if (!orlix_tcti_test_is_base_bitfield_unary(source))
			continue;
		KUNIT_ASSERT_LT_MSG(test, source->ordinal,
			ARRAY_SIZE(orlix_tcti_test_has_ddi0602_semantics), "%s",
			source->name);
		KUNIT_EXPECT_TRUE_MSG(test,
			orlix_tcti_test_has_ddi0602_semantics[source->ordinal], "%s",
			source->name);
		ddi++;
	}
	KUNIT_EXPECT_EQ(test, BFU_FAMILY_COUNT, ddi);
}

static void orlix_tcti_base_bitfield_unary_production_resume(struct kunit *test)
{
	size_t index;
	size_t seen = 0;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++) {
		const struct orlix_tcti_test_bitfield_source *source =
			&orlix_tcti_test_sources[index];
		u32 instruction;
		struct orlix_tcti_decoded_instruction decoded;
		struct pt_regs regs = {};
		unsigned long code;

		if (!orlix_tcti_test_is_el0_bitfield(source))
			continue;
		seen++;
		instruction = orlix_tcti_base_bitfield_unary_legal_instruction(source);
		decoded = orlix_tcti_decode_aarch64(instruction);
		KUNIT_ASSERT_TRUE_MSG(test,
			orlix_tcti_test_bitfield_decode_is_family(source,
				decoded.decode_class),
			"%s class %u", source->name, decoded.decode_class);
		KUNIT_ASSERT_EQ_MSG(test, source->ordinal, decoded.source_ordinal,
				    "%s ordinal", source->name);
		code = orlix_tcti_base_bitfield_unary_map(test, instruction);
		orlix_tcti_base_bitfield_unary_seed(&regs, code);
		orlix_tcti_base_bitfield_unary_capture_run(test, source,
			ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS, &regs, code);
		orlix_tcti_base_bitfield_unary_seed(&regs, code);
		orlix_tcti_base_bitfield_unary_capture_run(test, source,
			ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC, &regs, code);
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
	}
	KUNIT_EXPECT_EQ(test, BFU_EL0_COUNT, seen);
}

static u32 orlix_tcti_base_bitfield_unary_encode_bitfield(u32 pattern, u8 immr,
							 u8 imms, u8 rn, u8 rd)
{
	return (pattern & 0xffc00000U) | ((u32)immr << 16) | ((u32)imms << 10) |
		((u32)rn << 5) | rd;
}

static void orlix_tcti_base_bitfield_unary_aliases_and_w_upper(struct kunit *test)
{
	const struct orlix_tcti_test_bitfield_source *ubfm32 =
		orlix_tcti_test_bitfield_name("UBFM_32M_bitfield");
	const struct orlix_tcti_test_bitfield_source *ubfm64 =
		orlix_tcti_test_bitfield_name("UBFM_64M_bitfield");
	const struct orlix_tcti_test_bitfield_source *sbfm32 =
		orlix_tcti_test_bitfield_name("SBFM_32M_bitfield");
	const struct orlix_tcti_test_bitfield_source *sbfm64 =
		orlix_tcti_test_bitfield_name("SBFM_64M_bitfield");
	const struct orlix_tcti_test_bitfield_source *bfm32 =
		orlix_tcti_test_bitfield_name("BFM_32M_bitfield");
	struct {
		const struct orlix_tcti_test_bitfield_source *source;
		u8 immr;
		u8 imms;
	} cases[8];
	size_t index;

	KUNIT_ASSERT_NOT_NULL(test, ubfm32);
	KUNIT_ASSERT_NOT_NULL(test, ubfm64);
	KUNIT_ASSERT_NOT_NULL(test, sbfm32);
	KUNIT_ASSERT_NOT_NULL(test, sbfm64);
	KUNIT_ASSERT_NOT_NULL(test, bfm32);
	cases[0].source = sbfm32;
	cases[0].immr = 5;
	cases[0].imms = 31;
	cases[1].source = sbfm32;
	cases[1].immr = 0;
	cases[1].imms = 7;
	cases[2].source = ubfm32;
	cases[2].immr = 24;
	cases[2].imms = 23;
	cases[3].source = ubfm32;
	cases[3].immr = 0;
	cases[3].imms = 7;
	cases[4].source = sbfm64;
	cases[4].immr = 8;
	cases[4].imms = 15;
	cases[5].source = ubfm64;
	cases[5].immr = 9;
	cases[5].imms = 63;
	cases[6].source = bfm32;
	cases[6].immr = 24;
	cases[6].imms = 7;
	cases[7].source = ubfm64;
	cases[7].immr = 32;
	cases[7].imms = 15;
	for (index = 0; index < ARRAY_SIZE(cases); index++) {
		u32 insn = orlix_tcti_base_bitfield_unary_encode_bitfield(
			cases[index].source->pattern, cases[index].immr,
			cases[index].imms, BFU_RN, BFU_RD);
		unsigned long code;
		struct pt_regs regs = {};
		struct orlix_tcti_result result;

		code = orlix_tcti_base_bitfield_unary_map(test, insn);
		orlix_tcti_base_bitfield_unary_seed(&regs, code);
		regs.regs[1] = 0xa1a1a1a1b2b2b2b2ULL;
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason,
				    "%s alias %u", cases[index].source->name,
				    (unsigned int)index);
		if (strstr(cases[index].source->name, "32"))
			KUNIT_EXPECT_EQ_MSG(test, regs.regs[0] >> 32, 0ULL,
					    "%s W upper", cases[index].source->name);
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
	}
}

static void orlix_tcti_base_bitfield_unary_extract_overlap(struct kunit *test)
{
	const struct orlix_tcti_test_bitfield_source *extr32 =
		orlix_tcti_test_bitfield_name("EXTR_32_extract");
	const struct orlix_tcti_test_bitfield_source *extr64 =
		orlix_tcti_test_bitfield_name("EXTR_64_extract");
	const struct orlix_tcti_test_bitfield_source *sources[2];
	size_t index;
	u8 rd;

	KUNIT_ASSERT_NOT_NULL(test, extr32);
	KUNIT_ASSERT_NOT_NULL(test, extr64);
	sources[0] = extr32;
	sources[1] = extr64;
	for (index = 0; index < ARRAY_SIZE(sources); index++) {
		for (rd = 1; rd <= 2; rd++) {
			u32 insn = (sources[index]->pattern & sources[index]->mask) |
				(1U << 5) | (2U << 16) | (8U << 10) | rd;
			unsigned long code;
			struct pt_regs regs = {};
			struct orlix_tcti_result result;

			code = orlix_tcti_base_bitfield_unary_map(test, insn);
			orlix_tcti_base_bitfield_unary_seed(&regs, code);
			result = orlix_tcti_resume_user(current, &regs, current->mm);
			KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_SYSCALL,
				result.reason, "%s rd %u", sources[index]->name, rd);
			KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
		}
	}
}

static void orlix_tcti_base_bitfield_unary_rbit_rev_clz(struct kunit *test)
{
	static const char *const names[] = {
		"RBIT_32_dp_1src", "REV16_32_dp_1src", "REV_32_dp_1src",
		"CLZ_32_dp_1src", "CLS_32_dp_1src", "RBIT_64_dp_1src",
		"REV16_64_dp_1src", "REV32_64_dp_1src", "REV_64_dp_1src",
		"CLZ_64_dp_1src", "CLS_64_dp_1src",
	};
	size_t index;

	for (index = 0; index < ARRAY_SIZE(names); index++) {
		const struct orlix_tcti_test_bitfield_source *source =
			orlix_tcti_test_bitfield_name(names[index]);
		u32 insn;
		unsigned long code;
		struct pt_regs regs = {};
		struct orlix_tcti_result result;

		KUNIT_ASSERT_NOT_NULL(test, source);
		insn = orlix_tcti_base_bitfield_unary_legal_instruction(source);
		code = orlix_tcti_base_bitfield_unary_map(test, insn);
		orlix_tcti_base_bitfield_unary_seed(&regs, code);
		regs.regs[1] = 0;
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason,
				    "%s zero", source->name);
		if (strstr(source->name, "32"))
			KUNIT_EXPECT_EQ_MSG(test, regs.regs[0] >> 32, 0ULL,
					    "%s W upper", source->name);
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
	}
}

static void orlix_tcti_base_bitfield_unary_simd_and_flags_unchanged(struct kunit *test)
{
	const struct orlix_tcti_test_bitfield_source *source =
		orlix_tcti_test_bitfield_name("UBFM_32M_bitfield");
	u32 insn;
	unsigned long code;
	struct pt_regs regs = {};
	struct orlix_tcti_result result;
	u64 simd0;
	u64 simd1;
	u64 flags;

	KUNIT_ASSERT_NOT_NULL(test, source);
	insn = orlix_tcti_base_bitfield_unary_legal_instruction(source);
	code = orlix_tcti_base_bitfield_unary_map(test, insn);
	orlix_tcti_base_bitfield_unary_seed(&regs, code);
	regs.pstate |= PSR_Z_BIT;
	simd0 = current->thread.user_simd[0];
	simd1 = current->thread.user_simd[1];
	flags = regs.pstate & BFU_NZCV;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, simd0, current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, simd1, current->thread.user_simd[1]);
	KUNIT_EXPECT_EQ(test, flags, regs.pstate & BFU_NZCV);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
}

static void orlix_tcti_base_bitfield_unary_non_el0_rejected(struct kunit *test)
{
	size_t index;
	size_t seen = 0;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++) {
		const struct orlix_tcti_test_bitfield_source *source =
			&orlix_tcti_test_sources[index];
		u32 instruction;
		struct orlix_tcti_decoded_instruction decoded;
		struct pt_regs regs = {};
		struct pt_regs before;
		struct orlix_tcti_result result;
		unsigned long code;

		if (!orlix_tcti_test_is_base_bitfield_unary(source) ||
		    orlix_tcti_test_is_el0_bitfield(source))
			continue;
		seen++;
		instruction = orlix_tcti_base_bitfield_unary_legal_instruction(source);
		decoded = orlix_tcti_decode_aarch64(instruction);
		KUNIT_EXPECT_TRUE_MSG(test,
			orlix_tcti_test_bitfield_decode_is_family(source,
				decoded.decode_class),
			"%s class", source->name);
		KUNIT_EXPECT_EQ_MSG(test, source->ordinal, decoded.source_ordinal,
				    "%s ordinal", source->name);
		code = orlix_tcti_base_bitfield_unary_map(test, instruction);
		orlix_tcti_base_bitfield_unary_seed(&regs, code);
		before = regs;
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
				    result.reason, "%s", source->name);
		KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, result.status);
		KUNIT_EXPECT_EQ(test, instruction, result.instruction);
		KUNIT_EXPECT_EQ(test, before.pc, result.pc);
		KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
	}
	KUNIT_EXPECT_EQ(test, BFU_NON_EL0_COUNT, seen);
}

static void orlix_tcti_base_bitfield_unary_reserved_encodings(struct kunit *test)
{
	struct orlix_tcti_decoded_instruction decoded;
	u32 insn;
	unsigned long code;
	struct pt_regs regs = {};
	struct pt_regs before;
	struct orlix_tcti_result result;

	insn = 0x13000000U | BIT(22);
	decoded = orlix_tcti_decode_aarch64(insn);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED, decoded.decode_class);
	code = orlix_tcti_base_bitfield_unary_map(test, insn);
	orlix_tcti_base_bitfield_unary_seed(&regs, code);
	before = regs;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION, result.reason);
	KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));

	insn = 0x13800000U | (32U << 10);
	decoded = orlix_tcti_decode_aarch64(insn);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED, decoded.decode_class);
}

static struct kunit_case orlix_tcti_base_bitfield_unary_source_bound_cases[] = {
	KUNIT_CASE(orlix_tcti_base_bitfield_unary_decodes_exact_source_cohort),
	KUNIT_CASE(orlix_tcti_base_bitfield_unary_binds_pinned_ddi0602_semantics),
	KUNIT_CASE(orlix_tcti_base_bitfield_unary_production_resume),
	KUNIT_CASE(orlix_tcti_base_bitfield_unary_aliases_and_w_upper),
	KUNIT_CASE(orlix_tcti_base_bitfield_unary_extract_overlap),
	KUNIT_CASE(orlix_tcti_base_bitfield_unary_rbit_rev_clz),
	KUNIT_CASE(orlix_tcti_base_bitfield_unary_simd_and_flags_unchanged),
	KUNIT_CASE(orlix_tcti_base_bitfield_unary_non_el0_rejected),
	KUNIT_CASE(orlix_tcti_base_bitfield_unary_reserved_encodings),
	{}
};

static struct kunit_suite orlix_tcti_base_bitfield_unary_source_bound_suite = {
	.name = "orlix-tcti-base-bitfield-unary-source-bound",
	.init = orlix_tcti_base_bitfield_unary_test_init,
	.exit = orlix_tcti_base_bitfield_unary_test_exit,
	.test_cases = orlix_tcti_base_bitfield_unary_source_bound_cases,
};

kunit_test_suite(orlix_tcti_base_bitfield_unary_source_bound_suite);
