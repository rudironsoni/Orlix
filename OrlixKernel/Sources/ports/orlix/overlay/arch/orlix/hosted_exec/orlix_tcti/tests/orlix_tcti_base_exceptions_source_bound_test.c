// SPDX-License-Identifier: GPL-2.0-only
/*
 * Production-path proof for the 10 BASE_EXCEPTIONS leaves owned by GitHub
 * #132. The old branch-control suite remains a regression and does not
 * receive #120 close credit for these ordinals.
 */
#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/orlix_tcti.h>
#include <kunit/test.h>
#include <linux/bitops.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/string.h>
#include <linux/syscalls.h>

#include "../decode_aarch64.h"
#include "../engine.h"
#include "../switch_debug.h"
#include "orlix_tcti_memory_proof.h"
#include "orlix_tcti_native_observation.h"
#include "orlix_tcti_base_exceptions_production_capture.h"
#include "target_native_proof_contract_private.h"
#include "target_proof_ingestion_private.h"

#define BEX_SVC 0xd4000001U
#define BEX_BRK 0xd4200000U
#define BEX_HLT 0xd4400000U
#define BEX_NZCV (PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT | PSR_V_BIT)
#define BEX_FAMILY_COUNT 10U
#define BEX_DDI0602_COUNT 9U
#define BEX_EL0_TRAP_COUNT 4U
#define BEX_NON_EL0_COUNT 6U

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

struct orlix_tcti_test_exceptions_source {
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
static const struct orlix_tcti_test_exceptions_source orlix_tcti_test_sources[] = {
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

static bool orlix_tcti_test_is_base_exceptions(
	const struct orlix_tcti_test_exceptions_source *source)
{
	return source->ordinal < ARRAY_SIZE(orlix_tcti_test_source_families) &&
		orlix_tcti_test_source_families[source->ordinal] ==
			ORLIX_TCTI_TEST_SOURCE_FAMILY_BASE_EXCEPTIONS;
}

static bool orlix_tcti_test_is_el0_trap(
	const struct orlix_tcti_test_exceptions_source *source)
{
	return orlix_tcti_test_is_base_exceptions(source) &&
		(!strcmp(source->operation, "SVC") ||
		 !strcmp(source->operation, "BRK") ||
		 !strcmp(source->operation, "HLT") ||
		 !strcmp(source->mnemonic, "UDF"));
}

static const struct orlix_tcti_test_exceptions_source *
orlix_tcti_test_exceptions_name(const char *name)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++)
		if (orlix_tcti_test_is_base_exceptions(&orlix_tcti_test_sources[index]) &&
		    !strcmp(orlix_tcti_test_sources[index].name, name))
			return &orlix_tcti_test_sources[index];
	return NULL;
}

static bool orlix_tcti_test_exceptions_decode_is_family(
	const struct orlix_tcti_test_exceptions_source *source,
	enum orlix_tcti_decode_class decode_class)
{
	if (!strcmp(source->operation, "SVC"))
		return decode_class == ORLIX_TCTI_DECODE_SVC;
	if (!strcmp(source->operation, "BRK"))
		return decode_class == ORLIX_TCTI_DECODE_BRK;
	if (!strcmp(source->operation, "HLT"))
		return decode_class == ORLIX_TCTI_DECODE_HLT;
	if (!strcmp(source->mnemonic, "UDF") ||
	    !strcmp(source->operation, "HVC") ||
	    !strcmp(source->operation, "SMC") ||
	    !strcmp(source->operation, "DCPS1") ||
	    !strcmp(source->operation, "DCPS2") ||
	    !strcmp(source->operation, "DCPS3"))
		return decode_class == ORLIX_TCTI_DECODE_UNDEFINED;
	return decode_class == ORLIX_TCTI_DECODE_UNSUPPORTED;
}

static u32 orlix_tcti_base_exceptions_legal_instruction(
	const struct orlix_tcti_test_exceptions_source *source)
{
	if (!strcmp(source->operation, "SVC") ||
	    !strcmp(source->operation, "BRK") ||
	    !strcmp(source->operation, "HLT"))
		return (source->pattern & source->mask) | (0x1234U << 5);
	return source->pattern;
}

static int orlix_tcti_base_exceptions_test_init(struct kunit *test)
{
	struct mm_struct *mm = mm_alloc();

	if (!mm)
		return -ENOMEM;
	kthread_use_mm(mm);
	test->priv = mm;
	return 0;
}

static void orlix_tcti_base_exceptions_test_exit(struct kunit *test)
{
	struct mm_struct *mm = test->priv;

	if (!mm)
		return;
	kthread_unuse_mm(mm);
	mmput(mm);
}

static unsigned long orlix_tcti_base_exceptions_map_program(struct kunit *test,
							    u32 instruction)
{
	u8 bytes[16] = {};

	memcpy(bytes, &instruction, sizeof(instruction));
	return orlix_tcti_memory_proof_map_bytes(test, bytes, sizeof(bytes),
						 PROT_READ | PROT_EXEC);
}

static void orlix_tcti_base_exceptions_seed(struct pt_regs *regs,
					    unsigned long code, u64 pstate)
{
	memset(regs, 0, sizeof(*regs));
	regs->pc = code;
	regs->pstate = PSR_MODE_EL0t | (pstate & BEX_NZCV);
	regs->syscallno = NO_SYSCALL;
	regs->regs[0] = 0x1111111111111111ULL;
	regs->regs[1] = 0x2222222222222222ULL;
	regs->regs[8] = 64ULL;
	regs->regs[30] = 0x3333333333333333ULL;
	current->thread.user_simd_valid = 1;
	current->thread.user_simd[0] = 0xaaaaaaaaaaaaaaaaULL;
	current->thread.user_simd[1] = 0xbbbbbbbbbbbbbbbbULL;
}

static void orlix_tcti_base_exceptions_expect_exit(struct kunit *test,
	const struct orlix_tcti_test_exceptions_source *source,
	const struct orlix_tcti_result *result, const struct pt_regs *regs,
	const struct pt_regs *before, u32 instruction)
{
	if (!strcmp(source->operation, "SVC")) {
		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_SYSCALL, result->reason,
				    "%s", source->name);
		KUNIT_EXPECT_EQ(test, 0L, result->status);
	} else if (!strcmp(source->operation, "BRK")) {
		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_BREAKPOINT, result->reason,
				    "%s", source->name);
		KUNIT_EXPECT_EQ(test, 0x1234L, result->status);
	} else if (!strcmp(source->operation, "HLT")) {
		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_UNDEFINED_INSTRUCTION,
				    result->reason, "%s", source->name);
		KUNIT_EXPECT_EQ(test, 0x1234L, result->status);
	} else if (!strcmp(source->operation, "TENTER")) {
		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
				    result->reason, "%s", source->name);
		KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, result->status);
	} else {
		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_UNDEFINED_INSTRUCTION,
				    result->reason, "%s", source->name);
		KUNIT_EXPECT_EQ(test, 0L, result->status);
	}
	KUNIT_EXPECT_EQ(test, instruction, result->instruction);
	KUNIT_EXPECT_EQ(test, before->pc, result->pc);
	KUNIT_EXPECT_EQ(test, before->pc, regs->pc);
	KUNIT_EXPECT_MEMEQ(test, before, regs, sizeof(*regs));
}

static void orlix_tcti_base_exceptions_capture_run(struct kunit *test,
	const struct orlix_tcti_test_exceptions_source *source, u32 obligation,
	struct pt_regs *regs, u32 instruction, const struct pt_regs *before)
{
	const void *token;
	struct orlix_tcti_native_capture_session *capture = NULL;
	struct orlix_tcti_native_wire_record wire = {};
	struct orlix_tcti_target_proof_ingestion_ledger *ledger;
	struct orlix_tcti_result result;
	int ret;

	token = orlix_tcti_base_exceptions_production_capture_token(
		source->ordinal, obligation);
	KUNIT_EXPECT_TRUE_MSG(test, token != NULL, "%s token %u", source->name,
			      obligation);
	if (!token)
		return;
	ret = orlix_tcti_native_capture_begin(token, source->ordinal, obligation,
					      &capture);
	KUNIT_EXPECT_EQ_MSG(test, 0, ret, "%s begin %u", source->name, obligation);
	result = orlix_tcti_resume_user(current, regs, current->mm);
	orlix_tcti_base_exceptions_expect_exit(test, source, &result, regs, before,
					       instruction);
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

static void orlix_tcti_base_exceptions_decodes_exact_source_cohort(
	struct kunit *test)
{
	size_t index;
	size_t count = 0;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++) {
		const struct orlix_tcti_test_exceptions_source *source =
			&orlix_tcti_test_sources[index];
		struct orlix_tcti_decoded_instruction decoded;
		u32 instruction;

		if (!orlix_tcti_test_is_base_exceptions(source))
			continue;
		instruction = orlix_tcti_base_exceptions_legal_instruction(source);
		KUNIT_ASSERT_EQ_MSG(test, source->pattern,
				instruction & source->mask, "%s", source->name);
		decoded = orlix_tcti_decode_aarch64(instruction);
		KUNIT_EXPECT_TRUE_MSG(test,
			orlix_tcti_test_exceptions_decode_is_family(source,
				decoded.decode_class),
			"%s class %u insn %#x", source->name, decoded.decode_class,
			instruction);
		KUNIT_EXPECT_EQ_MSG(test, source->ordinal, decoded.source_ordinal,
				"%s %#x", source->name, instruction);
		count++;
	}
	KUNIT_EXPECT_EQ(test, BEX_FAMILY_COUNT, count);
}

static void orlix_tcti_base_exceptions_binds_pinned_ddi0602_semantics(
	struct kunit *test)
{
	size_t index;
	size_t ddi = 0;
	size_t unspecified = 0;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++) {
		const struct orlix_tcti_test_exceptions_source *source =
			&orlix_tcti_test_sources[index];

		if (!orlix_tcti_test_is_base_exceptions(source))
			continue;
		KUNIT_ASSERT_LT_MSG(test, source->ordinal,
			ARRAY_SIZE(orlix_tcti_test_has_ddi0602_semantics), "%s",
			source->name);
		if (!strcmp(source->name, "TENTER_te_exception")) {
			KUNIT_EXPECT_FALSE_MSG(test,
				orlix_tcti_test_has_ddi0602_semantics[source->ordinal],
				"%s", source->name);
			unspecified++;
			continue;
		}
		KUNIT_EXPECT_TRUE_MSG(test,
			orlix_tcti_test_has_ddi0602_semantics[source->ordinal], "%s",
			source->name);
		ddi++;
	}
	KUNIT_EXPECT_EQ(test, BEX_DDI0602_COUNT, ddi);
	KUNIT_EXPECT_EQ(test, 1U, unspecified);
}

static void orlix_tcti_base_exceptions_production_resume(struct kunit *test)
{
	const struct orlix_tcti_test_exceptions_source *source =
		orlix_tcti_test_exceptions_name("SVC_EX_exception");
	u32 instruction;
	struct orlix_tcti_decoded_instruction decoded;
	struct pt_regs regs = {};
	struct pt_regs before;
	unsigned long code;

	KUNIT_ASSERT_NOT_NULL(test, source);
	instruction = orlix_tcti_base_exceptions_legal_instruction(source);
	decoded = orlix_tcti_decode_aarch64(instruction);
	KUNIT_ASSERT_EQ(test, ORLIX_TCTI_DECODE_SVC, decoded.decode_class);
	KUNIT_ASSERT_EQ(test, source->ordinal, decoded.source_ordinal);
	code = orlix_tcti_base_exceptions_map_program(test, instruction);
	orlix_tcti_base_exceptions_seed(&regs, code, BEX_NZCV);
	before = regs;
	orlix_tcti_base_exceptions_capture_run(test, source,
		ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS, &regs, instruction,
		&before);
	orlix_tcti_base_exceptions_seed(&regs, code, BEX_NZCV);
	before = regs;
	orlix_tcti_base_exceptions_capture_run(test, source,
		ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC, &regs, instruction, &before);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
}

static void orlix_tcti_base_exceptions_el0_traps(struct kunit *test)
{
	size_t index;
	size_t seen = 0;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++) {
		const struct orlix_tcti_test_exceptions_source *source =
			&orlix_tcti_test_sources[index];
		u32 instruction;
		struct orlix_tcti_decoded_instruction decoded;
		struct pt_regs regs = {};
		struct pt_regs before;
		struct orlix_tcti_result result;
		unsigned long code;

		if (!orlix_tcti_test_is_el0_trap(source))
			continue;
		seen++;
		instruction = orlix_tcti_base_exceptions_legal_instruction(source);
		decoded = orlix_tcti_decode_aarch64(instruction);
		KUNIT_ASSERT_TRUE_MSG(test,
			orlix_tcti_test_exceptions_decode_is_family(source,
				decoded.decode_class),
			"%s class %u", source->name, decoded.decode_class);
		code = orlix_tcti_base_exceptions_map_program(test, instruction);
		orlix_tcti_base_exceptions_seed(&regs, code, BEX_NZCV);
		before = regs;
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		orlix_tcti_base_exceptions_expect_exit(test, source, &result, &regs,
						       &before, instruction);
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
	}
	KUNIT_EXPECT_EQ(test, BEX_EL0_TRAP_COUNT, seen);
}

static void orlix_tcti_base_exceptions_syscall_handoff(struct kunit *test)
{
	struct pt_regs regs = {};
	unsigned long code = 0x1000;

	orlix_tcti_base_exceptions_seed(&regs, code, BEX_NZCV);
	regs.regs[8] = 64ULL;
	orlix_tcti_prepare_syscall_handoff(&regs);
	KUNIT_EXPECT_EQ(test, 64, regs.syscallno);
	KUNIT_EXPECT_EQ(test, 0x1111111111111111ULL, regs.orig_x0);
	KUNIT_EXPECT_EQ(test, code + sizeof(u32), regs.pc);
}

static void orlix_tcti_base_exceptions_simd_and_flags_unchanged(struct kunit *test)
{
	const struct orlix_tcti_test_exceptions_source *source =
		orlix_tcti_test_exceptions_name("SVC_EX_exception");
	u32 insn;
	unsigned long code;
	struct pt_regs regs = {};
	struct orlix_tcti_result result;
	u64 simd0;
	u64 simd1;
	u64 flags;

	KUNIT_ASSERT_NOT_NULL(test, source);
	insn = orlix_tcti_base_exceptions_legal_instruction(source);
	code = orlix_tcti_base_exceptions_map_program(test, insn);
	orlix_tcti_base_exceptions_seed(&regs, code, BEX_NZCV);
	simd0 = current->thread.user_simd[0];
	simd1 = current->thread.user_simd[1];
	flags = regs.pstate & BEX_NZCV;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, simd0, current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, simd1, current->thread.user_simd[1]);
	KUNIT_EXPECT_EQ(test, flags, regs.pstate & BEX_NZCV);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
}

static void orlix_tcti_base_exceptions_non_el0_rejected(struct kunit *test)
{
	size_t index;
	size_t seen = 0;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++) {
		const struct orlix_tcti_test_exceptions_source *source =
			&orlix_tcti_test_sources[index];
		u32 instruction;
		struct orlix_tcti_decoded_instruction decoded;
		struct pt_regs regs = {};
		struct pt_regs before;
		struct orlix_tcti_result result;
		unsigned long code;

		if (!orlix_tcti_test_is_base_exceptions(source) ||
		    orlix_tcti_test_is_el0_trap(source))
			continue;
		seen++;
		instruction = source->pattern;
		decoded = orlix_tcti_decode_aarch64(instruction);
		KUNIT_EXPECT_TRUE_MSG(test,
			orlix_tcti_test_exceptions_decode_is_family(source,
				decoded.decode_class),
			"%s class %u", source->name, decoded.decode_class);
		KUNIT_EXPECT_EQ_MSG(test, source->ordinal, decoded.source_ordinal,
				    "%s ordinal", source->name);
		code = orlix_tcti_base_exceptions_map_program(test, instruction);
		orlix_tcti_base_exceptions_seed(&regs, code, BEX_NZCV);
		before = regs;
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		orlix_tcti_base_exceptions_expect_exit(test, source, &result, &regs,
						       &before, instruction);
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
	}
	KUNIT_EXPECT_EQ(test, BEX_NON_EL0_COUNT, seen);
}

static void orlix_tcti_base_exceptions_reserved_encodings(struct kunit *test)
{
	struct orlix_tcti_decoded_instruction decoded;
	const struct orlix_tcti_test_exceptions_source *tenter =
		orlix_tcti_test_exceptions_name("TENTER_te_exception");

	KUNIT_ASSERT_NOT_NULL(test, tenter);
	decoded = orlix_tcti_decode_aarch64(tenter->pattern);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED, decoded.decode_class);
	KUNIT_EXPECT_EQ(test, tenter->ordinal, decoded.source_ordinal);
	decoded = orlix_tcti_decode_aarch64(tenter->pattern | BIT(5));
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED, decoded.decode_class);
	KUNIT_EXPECT_EQ(test, tenter->ordinal, decoded.source_ordinal);
}

static struct kunit_case orlix_tcti_base_exceptions_source_bound_cases[] = {
	KUNIT_CASE(orlix_tcti_base_exceptions_decodes_exact_source_cohort),
	KUNIT_CASE(orlix_tcti_base_exceptions_binds_pinned_ddi0602_semantics),
	KUNIT_CASE(orlix_tcti_base_exceptions_production_resume),
	KUNIT_CASE(orlix_tcti_base_exceptions_el0_traps),
	KUNIT_CASE(orlix_tcti_base_exceptions_syscall_handoff),
	KUNIT_CASE(orlix_tcti_base_exceptions_simd_and_flags_unchanged),
	KUNIT_CASE(orlix_tcti_base_exceptions_non_el0_rejected),
	KUNIT_CASE(orlix_tcti_base_exceptions_reserved_encodings),
	{}
};

static struct kunit_suite orlix_tcti_base_exceptions_source_bound_suite = {
	.name = "orlix-tcti-base-exceptions-source-bound",
	.init = orlix_tcti_base_exceptions_test_init,
	.exit = orlix_tcti_base_exceptions_test_exit,
	.test_cases = orlix_tcti_base_exceptions_source_bound_cases,
};

kunit_test_suite(orlix_tcti_base_exceptions_source_bound_suite);
