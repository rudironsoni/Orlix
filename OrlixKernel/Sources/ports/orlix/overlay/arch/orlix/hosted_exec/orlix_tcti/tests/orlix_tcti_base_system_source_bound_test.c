// SPDX-License-Identifier: GPL-2.0-only
/*
 * Production-path proof for the 39 BASE_SYSTEM leaves owned by GitHub
 * #139. PAuth hint encodings stay on #137 and decode as unsupported.
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
#include "orlix_tcti_base_system_production_capture.h"
#include "target_native_proof_contract_private.h"
#include "target_proof_ingestion_private.h"

#define BSYS_SVC 0xd4000001U
#define BSYS_NZCV (PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT | PSR_V_BIT)
#define BSYS_FAMILY_COUNT 39U
#define BSYS_EL0_COUNT 26U
#define BSYS_REJECT_COUNT 13U
#define BSYS_MRS_TPIDR 0xd53bd040U
#define BSYS_MSR_TPIDR 0xd51bd040U
#define BSYS_SYS_IC_IVAU 0xd50b7520U
#define BSYS_HINT_GENERIC 0xd5032e1fU

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

struct orlix_tcti_test_system_source {
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
static const struct orlix_tcti_test_system_source orlix_tcti_test_sources[] = {
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

static bool orlix_tcti_test_is_base_system(
	const struct orlix_tcti_test_system_source *source)
{
	return source->ordinal < ARRAY_SIZE(orlix_tcti_test_source_families) &&
		orlix_tcti_test_source_families[source->ordinal] ==
			ORLIX_TCTI_TEST_SOURCE_FAMILY_BASE_SYSTEM;
}

static bool orlix_tcti_test_is_el0_system(
	const struct orlix_tcti_test_system_source *source)
{
	if (!orlix_tcti_test_is_base_system(source))
		return false;
	switch (source->ordinal) {
	case 2236U:
	case 2237U:
	case 2275U:
	case 2276U:
	case 2277U:
	case 2278U:
	case 2279U:
	case 2280U:
	case 2282U:
	case 2285U:
	case 2286U:
	case 2287U:
	case 2344U:
		return false;
	default:
		return true;
	}
}

static bool orlix_tcti_test_is_yield_hint(
	const struct orlix_tcti_test_system_source *source)
{
	return !strcmp(source->operation, "YIELD") ||
		!strcmp(source->operation, "WFE") ||
		!strcmp(source->operation, "WFI");
}

static u32 orlix_tcti_base_system_legal_instruction(
	const struct orlix_tcti_test_system_source *source)
{
	if (!strcmp(source->operation, "HINT"))
		return BSYS_HINT_GENERIC;
	if (!strcmp(source->operation, "DSB") && source->ordinal == 2272U)
		return 0xd5033f9fU;
	if (!strcmp(source->operation, "DMB"))
		return 0xd5033fbfU;
	if (!strcmp(source->operation, "ISB"))
		return 0xd5033fdfU;
	if (!strcmp(source->operation, "MRS"))
		return BSYS_MRS_TPIDR;
	if (!strcmp(source->operation, "MSR_reg"))
		return BSYS_MSR_TPIDR;
	if (!strcmp(source->operation, "SYS"))
		return BSYS_SYS_IC_IVAU;
	return source->pattern;
}

static bool orlix_tcti_test_system_decode_is_family(
	const struct orlix_tcti_test_system_source *source,
	enum orlix_tcti_decode_class decode_class)
{
	if (!orlix_tcti_test_is_el0_system(source))
		return decode_class == ORLIX_TCTI_DECODE_UNSUPPORTED ||
			decode_class == ORLIX_TCTI_DECODE_SYSTEM_REGISTER ||
			(source->ordinal == 2276U &&
			 decode_class == ORLIX_TCTI_DECODE_BARRIER);
	if (!strcmp(source->operation, "CLREX"))
		return decode_class == ORLIX_TCTI_DECODE_EXCLUSIVE_MONITOR_CLEAR;
	if (!strcmp(source->operation, "DSB") ||
	    !strcmp(source->operation, "DMB") ||
	    !strcmp(source->operation, "ISB"))
		return decode_class == ORLIX_TCTI_DECODE_BARRIER;
	if (!strcmp(source->operation, "SYS"))
		return decode_class == ORLIX_TCTI_DECODE_CACHE_MAINTENANCE ||
			decode_class == ORLIX_TCTI_DECODE_SYSTEM_REGISTER;
	if (!strcmp(source->operation, "MRS") ||
	    !strcmp(source->operation, "MSR_reg"))
		return decode_class == ORLIX_TCTI_DECODE_SYSTEM_REGISTER;
	return decode_class == ORLIX_TCTI_DECODE_HINT;
}

static int orlix_tcti_base_system_test_init(struct kunit *test)
{
	struct mm_struct *mm = mm_alloc();

	if (!mm)
		return -ENOMEM;
	kthread_use_mm(mm);
	test->priv = mm;
	return 0;
}

static void orlix_tcti_base_system_test_exit(struct kunit *test)
{
	struct mm_struct *mm = test->priv;

	if (!mm)
		return;
	kthread_unuse_mm(mm);
	mmput(mm);
}

static unsigned long orlix_tcti_base_system_map_program(struct kunit *test,
							u32 instruction,
							bool append_svc)
{
	u8 bytes[16] = {};
	u32 program[2] = { instruction, BSYS_SVC };

	memcpy(bytes, program, append_svc ? sizeof(program) : sizeof(instruction));
	return orlix_tcti_memory_proof_map_bytes(test, bytes,
						 append_svc ? sizeof(program) :
						 sizeof(instruction),
						 PROT_READ | PROT_EXEC);
}

static void orlix_tcti_base_system_seed(struct pt_regs *regs, unsigned long code)
{
	memset(regs, 0, sizeof(*regs));
	regs->pc = code;
	regs->pstate = PSR_MODE_EL0t | BSYS_NZCV;
	regs->syscallno = NO_SYSCALL;
	regs->regs[0] = 0x1111111111111111ULL;
	regs->sp = 0x2000;
	current->thread.user_simd_valid = 1;
	current->thread.user_simd[0] = 0xaaaaaaaaaaaaaaaaULL;
	current->thread.user_simd[1] = 0xbbbbbbbbbbbbbbbbULL;
}

static void orlix_tcti_base_system_capture_run(struct kunit *test,
	const struct orlix_tcti_test_system_source *source, u32 obligation,
	struct pt_regs *regs, unsigned long code, bool yield_hint)
{
	const void *token;
	struct orlix_tcti_native_capture_session *capture = NULL;
	struct orlix_tcti_native_wire_record wire = {};
	struct orlix_tcti_target_proof_ingestion_ledger *ledger;
	struct orlix_tcti_result result;
	int ret;

	token = orlix_tcti_base_system_production_capture_token(source->ordinal,
							       obligation);
	KUNIT_EXPECT_TRUE_MSG(test, token != NULL, "%s token %u", source->name,
			      obligation);
	if (!token)
		return;
	ret = orlix_tcti_native_capture_begin(token, source->ordinal, obligation,
					      &capture);
	KUNIT_EXPECT_EQ_MSG(test, 0, ret, "%s begin %u", source->name, obligation);
	result = orlix_tcti_resume_user(current, regs, current->mm);
	if (yield_hint) {
		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_YIELD, result.reason,
				    "%s", source->name);
		KUNIT_EXPECT_EQ_MSG(test, code + sizeof(u32), regs->pc, "%s pc",
				    source->name);
	} else {
		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason,
				    "%s", source->name);
		KUNIT_EXPECT_EQ_MSG(test, code + sizeof(u32), regs->pc, "%s pc",
				    source->name);
	}
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
		orlix_tcti_target_proof_ingestion_ledger_destroy(ledger);
	}
	orlix_tcti_native_wire_record_destroy(&wire);
	orlix_tcti_native_capture_destroy(capture);
}

static void orlix_tcti_base_system_decodes_exact_source_cohort(struct kunit *test)
{
	size_t index;
	size_t count = 0;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++) {
		const struct orlix_tcti_test_system_source *source =
			&orlix_tcti_test_sources[index];
		struct orlix_tcti_decoded_instruction decoded;
		u32 instruction;

		if (!orlix_tcti_test_is_base_system(source))
			continue;
		instruction = orlix_tcti_base_system_legal_instruction(source);
		KUNIT_ASSERT_EQ_MSG(test, source->pattern,
				instruction & source->mask, "%s", source->name);
		decoded = orlix_tcti_decode_aarch64(instruction);
		KUNIT_EXPECT_TRUE_MSG(test,
			orlix_tcti_test_system_decode_is_family(source,
				decoded.decode_class),
			"%s class %u insn %#x", source->name, decoded.decode_class,
			instruction);
		KUNIT_EXPECT_EQ_MSG(test, source->ordinal, decoded.source_ordinal,
				"%s %#x", source->name, instruction);
		count++;
	}
	KUNIT_EXPECT_EQ(test, BSYS_FAMILY_COUNT, count);
}

static void orlix_tcti_base_system_binds_pinned_ddi0602_semantics(struct kunit *test)
{
	size_t index;
	size_t ddi = 0;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++) {
		const struct orlix_tcti_test_system_source *source =
			&orlix_tcti_test_sources[index];

		if (!orlix_tcti_test_is_base_system(source))
			continue;
		KUNIT_ASSERT_LT_MSG(test, source->ordinal,
			ARRAY_SIZE(orlix_tcti_test_has_ddi0602_semantics), "%s",
			source->name);
		KUNIT_EXPECT_TRUE_MSG(test,
			orlix_tcti_test_has_ddi0602_semantics[source->ordinal],
			"%s", source->name);
		ddi++;
	}
	KUNIT_EXPECT_EQ(test, BSYS_FAMILY_COUNT, ddi);
}

static void orlix_tcti_base_system_production_resume(struct kunit *test)
{
	size_t index;
	size_t seen = 0;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++) {
		const struct orlix_tcti_test_system_source *source =
			&orlix_tcti_test_sources[index];
		u32 instruction;
		struct orlix_tcti_decoded_instruction decoded;
		struct pt_regs regs = {};
		unsigned long code;
		bool yield_hint;

		if (!orlix_tcti_test_is_el0_system(source))
			continue;
		seen++;
		instruction = orlix_tcti_base_system_legal_instruction(source);
		decoded = orlix_tcti_decode_aarch64(instruction);
		KUNIT_ASSERT_TRUE_MSG(test,
			orlix_tcti_test_system_decode_is_family(source,
				decoded.decode_class),
			"%s class %u", source->name, decoded.decode_class);
		KUNIT_ASSERT_EQ_MSG(test, source->ordinal, decoded.source_ordinal,
				    "%s ordinal", source->name);
		yield_hint = orlix_tcti_test_is_yield_hint(source);
		code = orlix_tcti_base_system_map_program(test, instruction,
							  !yield_hint);
		orlix_tcti_base_system_seed(&regs, code);
		orlix_tcti_base_system_capture_run(test, source,
			ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS, &regs, code,
			yield_hint);
		orlix_tcti_base_system_seed(&regs, code);
		orlix_tcti_base_system_capture_run(test, source,
			ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC, &regs, code,
			yield_hint);
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
	}
	KUNIT_EXPECT_EQ(test, BSYS_EL0_COUNT, seen);
}

static void orlix_tcti_base_system_simd_and_flags_unchanged(struct kunit *test)
{
	u32 insn = 0xd503201fU;
	unsigned long code;
	struct pt_regs regs = {};
	struct orlix_tcti_result result;
	u64 simd0;
	u64 simd1;
	u64 flags;

	code = orlix_tcti_base_system_map_program(test, insn, true);
	orlix_tcti_base_system_seed(&regs, code);
	simd0 = current->thread.user_simd[0];
	simd1 = current->thread.user_simd[1];
	flags = regs.pstate & BSYS_NZCV;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, simd0, current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, simd1, current->thread.user_simd[1]);
	KUNIT_EXPECT_EQ(test, flags, regs.pstate & BSYS_NZCV);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
}

static void orlix_tcti_base_system_non_el0_rejected(struct kunit *test)
{
	size_t index;
	size_t seen = 0;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++) {
		const struct orlix_tcti_test_system_source *source =
			&orlix_tcti_test_sources[index];
		u32 instruction;
		struct orlix_tcti_decoded_instruction decoded;
		struct pt_regs regs = {};
		struct pt_regs before;
		struct orlix_tcti_result result;
		unsigned long code;

		if (!orlix_tcti_test_is_base_system(source) ||
		    orlix_tcti_test_is_el0_system(source))
			continue;
		seen++;
		instruction = orlix_tcti_base_system_legal_instruction(source);
		decoded = orlix_tcti_decode_aarch64(instruction);
		KUNIT_EXPECT_TRUE_MSG(test,
			orlix_tcti_test_system_decode_is_family(source,
				decoded.decode_class),
			"%s class", source->name);
		KUNIT_EXPECT_EQ_MSG(test, source->ordinal, decoded.source_ordinal,
				    "%s ordinal", source->name);
		code = orlix_tcti_base_system_map_program(test, instruction, true);
		orlix_tcti_base_system_seed(&regs, code);
		before = regs;
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
				    result.reason, "%s", source->name);
		KUNIT_EXPECT_EQ(test, instruction, result.instruction);
		KUNIT_EXPECT_EQ(test, before.pc, result.pc);
		KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
	}
	KUNIT_EXPECT_EQ(test, BSYS_REJECT_COUNT, seen);
}

static void orlix_tcti_base_system_reserved_encodings(struct kunit *test)
{
	struct orlix_tcti_decoded_instruction decoded;

	decoded = orlix_tcti_decode_aarch64(0xd503309fU);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED, decoded.decode_class);
	decoded = orlix_tcti_decode_aarch64(0xd50330dfU);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED, decoded.decode_class);
	decoded = orlix_tcti_decode_aarch64(0xd50b7820U);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED, decoded.decode_class);
}

static struct kunit_case orlix_tcti_base_system_source_bound_cases[] = {
	KUNIT_CASE(orlix_tcti_base_system_decodes_exact_source_cohort),
	KUNIT_CASE(orlix_tcti_base_system_binds_pinned_ddi0602_semantics),
	KUNIT_CASE(orlix_tcti_base_system_production_resume),
	KUNIT_CASE(orlix_tcti_base_system_simd_and_flags_unchanged),
	KUNIT_CASE(orlix_tcti_base_system_non_el0_rejected),
	KUNIT_CASE(orlix_tcti_base_system_reserved_encodings),
	{}
};

static struct kunit_suite orlix_tcti_base_system_source_bound_suite = {
	.name = "orlix-tcti-base-system-source-bound",
	.init = orlix_tcti_base_system_test_init,
	.exit = orlix_tcti_base_system_test_exit,
	.test_cases = orlix_tcti_base_system_source_bound_cases,
};

kunit_test_suite(orlix_tcti_base_system_source_bound_suite);
