// SPDX-License-Identifier: GPL-2.0-only
/*
 * Production-path proof for the 34 BASE_ADD_SUBTRACT leaves owned by GitHub
 * #133. Old immediate and register suites remain regressions and do not
 * receive #120 close credit.
 */
#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/orlix_tcti.h>
#include <kunit/test.h>
#include <linux/bitops.h>
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
#include "orlix_tcti_base_add_sub_production_capture.h"
#include "target_native_proof_contract_private.h"
#include "target_proof_ingestion_private.h"

#define BAS_SVC 0xd4000001U
#define BAS_RD 0U
#define BAS_RN 1U
#define BAS_RM 2U
#define BAS_NZCV (PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT | PSR_V_BIT)

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

struct orlix_tcti_test_add_sub_source {
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
static const struct orlix_tcti_test_add_sub_source orlix_tcti_test_sources[] = {
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

static bool orlix_tcti_test_is_base_add_sub(
	const struct orlix_tcti_test_add_sub_source *source)
{
	return source->ordinal < ARRAY_SIZE(orlix_tcti_test_source_families) &&
		orlix_tcti_test_source_families[source->ordinal] ==
			ORLIX_TCTI_TEST_SOURCE_FAMILY_BASE_ADD_SUBTRACT;
}

static const struct orlix_tcti_test_add_sub_source *
orlix_tcti_test_add_sub_name(const char *name)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++)
		if (orlix_tcti_test_is_base_add_sub(&orlix_tcti_test_sources[index]) &&
		    !strcmp(orlix_tcti_test_sources[index].name, name))
			return &orlix_tcti_test_sources[index];
	return NULL;
}

static bool orlix_tcti_test_add_sub_sets_flags(
	const struct orlix_tcti_test_add_sub_source *source)
{
	return source->pattern & BIT(29);
}

static bool orlix_tcti_test_add_sub_decode_is_family(
	enum orlix_tcti_decode_class decode_class)
{
	return decode_class == ORLIX_TCTI_DECODE_ADD_SUB_IMMEDIATE ||
		decode_class == ORLIX_TCTI_DECODE_ADD_SUB_SHIFTED_REGISTER ||
		decode_class == ORLIX_TCTI_DECODE_ADD_SUB_EXTENDED_REGISTER ||
		decode_class == ORLIX_TCTI_DECODE_ADD_SUB_WITH_CARRY ||
		decode_class == ORLIX_TCTI_DECODE_ADD_SUB_POINTER;
}

static u32 orlix_tcti_test_set_variable_field(u32 instruction, u32 mask,
					      u8 shift, u8 bits, u8 value)
{
	u32 field = ((1U << bits) - 1U) << shift;
	u32 variable = field & ~mask;

	return (instruction & ~variable) | ((u32)value << shift & variable);
}

static u32 orlix_tcti_base_add_sub_legal_instruction(
	const struct orlix_tcti_test_add_sub_source *source)
{
	u32 instruction = source->pattern;

	instruction = orlix_tcti_test_set_variable_field(instruction, source->mask,
							 0, 5, BAS_RD);
	instruction = orlix_tcti_test_set_variable_field(instruction, source->mask,
							 5, 5, BAS_RN);
	instruction = orlix_tcti_test_set_variable_field(instruction, source->mask,
							 16, 5, BAS_RM);
	if (!(source->mask & (0xfffU << 10)) &&
	    strstr(source->name, "addsub_imm"))
		instruction = orlix_tcti_test_set_variable_field(instruction,
			source->mask, 10, 12, 1);
	return instruction;
}

static int orlix_tcti_base_add_sub_test_init(struct kunit *test)
{
	struct mm_struct *mm = mm_alloc();

	if (!mm)
		return -ENOMEM;
	kthread_use_mm(mm);
	test->priv = mm;
	return 0;
}

static void orlix_tcti_base_add_sub_test_exit(struct kunit *test)
{
	struct mm_struct *mm = test->priv;

	if (!mm)
		return;
	kthread_unuse_mm(mm);
	mmput(mm);
}

static unsigned long orlix_tcti_base_add_sub_map_program(struct kunit *test,
							 u32 instruction)
{
	u8 bytes[16] = {};
	u32 program[2] = { instruction, BAS_SVC };

	memcpy(bytes, program, sizeof(program));
	return orlix_tcti_memory_proof_map_bytes(test, bytes, sizeof(bytes),
						 PROT_READ | PROT_EXEC);
}

static void orlix_tcti_base_add_sub_seed(struct pt_regs *regs, unsigned long code,
					 u64 left, u64 right, u64 sp, u64 pstate)
{
	memset(regs, 0, sizeof(*regs));
	regs->pc = code;
	regs->pstate = PSR_MODE_EL0t | (pstate & BAS_NZCV);
	regs->syscallno = NO_SYSCALL;
	regs->regs[BAS_RN] = left;
	regs->regs[BAS_RM] = right;
	regs->sp = sp;
	current->thread.user_simd_valid = 1;
	current->thread.user_simd[0] = 0x1111111111111111ULL;
	current->thread.user_simd[1] = 0x2222222222222222ULL;
}

static void orlix_tcti_base_add_sub_expect_success(struct kunit *test,
	const struct orlix_tcti_test_add_sub_source *source,
	const struct orlix_tcti_result *result, const struct pt_regs *regs,
	unsigned long code)
{
	KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_SYSCALL, result->reason,
			    "%s ordinal %u reason", source->name, source->ordinal);
	KUNIT_EXPECT_EQ(test, 0L, result->status);
	KUNIT_EXPECT_EQ(test, BAS_SVC, result->instruction);
	KUNIT_EXPECT_EQ_MSG(test, code + sizeof(u32), regs->pc,
			    "%s ordinal %u pc", source->name, source->ordinal);
}

static void orlix_tcti_base_add_sub_capture_run(struct kunit *test,
	const struct orlix_tcti_test_add_sub_source *source, u32 obligation,
	struct pt_regs *regs, unsigned long code)
{
	const void *token;
	struct orlix_tcti_native_capture_session *capture = NULL;
	struct orlix_tcti_native_wire_record wire = {};
	struct orlix_tcti_target_proof_ingestion_ledger *ledger;
	struct orlix_tcti_result result;
	int ret;

	token = orlix_tcti_base_add_sub_production_capture_token(source->ordinal,
								 obligation);
	KUNIT_EXPECT_TRUE_MSG(test, token != NULL, "%s token %u", source->name,
			      obligation);
	if (!token)
		return;
	ret = orlix_tcti_native_capture_begin(token, source->ordinal, obligation,
					      &capture);
	KUNIT_EXPECT_EQ_MSG(test, 0, ret, "%s begin %u", source->name, obligation);
	result = orlix_tcti_resume_user(current, regs, current->mm);
	orlix_tcti_base_add_sub_expect_success(test, source, &result, regs, code);
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

static void orlix_tcti_base_add_sub_decodes_exact_source_cohort(struct kunit *test)
{
	size_t index;
	size_t count = 0;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++) {
		const struct orlix_tcti_test_add_sub_source *source =
			&orlix_tcti_test_sources[index];
		struct orlix_tcti_decoded_instruction decoded;
		u32 instruction;

		if (!orlix_tcti_test_is_base_add_sub(source))
			continue;
		instruction = orlix_tcti_base_add_sub_legal_instruction(source);
		KUNIT_ASSERT_EQ_MSG(test, source->pattern,
				instruction & source->mask, "%s", source->name);
		decoded = orlix_tcti_decode_aarch64(instruction);
		KUNIT_EXPECT_TRUE_MSG(test,
			orlix_tcti_test_add_sub_decode_is_family(decoded.decode_class),
			"%s class %u insn %#x", source->name, decoded.decode_class,
			instruction);
		KUNIT_EXPECT_EQ_MSG(test, source->ordinal, decoded.source_ordinal,
				"%s %#x", source->name, instruction);
		count++;
	}
	KUNIT_EXPECT_EQ(test, 34U, count);
}

static void orlix_tcti_base_add_sub_binds_pinned_ddi0602_semantics(struct kunit *test)
{
	size_t index;
	size_t count = 0;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++) {
		const struct orlix_tcti_test_add_sub_source *source =
			&orlix_tcti_test_sources[index];

		if (!orlix_tcti_test_is_base_add_sub(source))
			continue;
		KUNIT_ASSERT_LT_MSG(test, source->ordinal,
			ARRAY_SIZE(orlix_tcti_test_has_ddi0602_semantics), "%s",
			source->name);
		KUNIT_EXPECT_TRUE_MSG(test,
			orlix_tcti_test_has_ddi0602_semantics[source->ordinal], "%s",
			source->name);
		count++;
	}
	KUNIT_EXPECT_EQ(test, 34U, count);
}

static void orlix_tcti_base_add_sub_production_resume(struct kunit *test)
{
	size_t index;
	size_t seen = 0;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++) {
		const struct orlix_tcti_test_add_sub_source *source =
			&orlix_tcti_test_sources[index];
		u32 instruction;
		struct orlix_tcti_decoded_instruction decoded;
		struct pt_regs regs = {};
		unsigned long code;
		u64 left = 0x0102030480000001ULL;
		u64 right = 0x0000000000000010ULL;
		u64 sp = 0x000000010000fff0ULL;

		if (!orlix_tcti_test_is_base_add_sub(source))
			continue;
		seen++;
		instruction = orlix_tcti_base_add_sub_legal_instruction(source);
		decoded = orlix_tcti_decode_aarch64(instruction);
		KUNIT_ASSERT_TRUE_MSG(test,
			orlix_tcti_test_add_sub_decode_is_family(decoded.decode_class),
			"%s ordinal %u class %u", source->name, source->ordinal,
			decoded.decode_class);
		code = orlix_tcti_base_add_sub_map_program(test, instruction);
		orlix_tcti_base_add_sub_seed(&regs, code, left, right, sp,
					     BAS_NZCV);
		orlix_tcti_base_add_sub_capture_run(test, source,
			ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS, &regs, code);
		orlix_tcti_base_add_sub_seed(&regs, code, left, right, sp,
					     BAS_NZCV);
		orlix_tcti_base_add_sub_capture_run(test, source,
			ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC, &regs, code);
		orlix_tcti_base_add_sub_seed(&regs, code, left, right, sp,
					     BAS_NZCV);
		orlix_tcti_base_add_sub_capture_run(test, source,
			ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS, &regs, code);
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
	}
	KUNIT_EXPECT_EQ(test, 34U, seen);
}

static void orlix_tcti_base_add_sub_reserved_encodings(struct kunit *test)
{
	u32 reserved_imm = 0x11000000U | BIT(23);
	u32 reserved_shift = 0x0b000000U | (3U << 22);
	u32 reserved_ext = 0x0b200000U | (5U << 10);
	struct orlix_tcti_decoded_instruction decoded;

	decoded = orlix_tcti_decode_aarch64(reserved_imm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED, decoded.decode_class);
	decoded = orlix_tcti_decode_aarch64(reserved_shift);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED, decoded.decode_class);
	decoded = orlix_tcti_decode_aarch64(reserved_ext);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED, decoded.decode_class);
	decoded = orlix_tcti_decode_aarch64(0x0b000000U | BIT(15));
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED, decoded.decode_class);
}

static void orlix_tcti_base_add_sub_sp_zr_and_w_upper_zero(struct kunit *test)
{
	const struct orlix_tcti_test_add_sub_source *add64 =
		orlix_tcti_test_add_sub_name("ADD_64_addsub_imm");
	const struct orlix_tcti_test_add_sub_source *add32 =
		orlix_tcti_test_add_sub_name("ADD_32_addsub_imm");
	const struct orlix_tcti_test_add_sub_source *adds32 =
		orlix_tcti_test_add_sub_name("ADDS_32S_addsub_imm");
	u32 add_sp;
	u32 add32_insn;
	u32 cmp_xzr;
	unsigned long code;
	struct pt_regs regs = {};
	struct orlix_tcti_result result;

	KUNIT_ASSERT_NOT_NULL(test, add64);
	KUNIT_ASSERT_NOT_NULL(test, add32);
	KUNIT_ASSERT_NOT_NULL(test, adds32);
	add_sp = (add64->pattern & add64->mask) | (31U << 5) | 31U | (1U << 10);
	code = orlix_tcti_base_add_sub_map_program(test, add_sp);
	orlix_tcti_base_add_sub_seed(&regs, code, 0, 0, 0x1000, 0);
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, 0x1000ULL + 1ULL, regs.sp);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));

	add32_insn = orlix_tcti_base_add_sub_legal_instruction(add32);
	code = orlix_tcti_base_add_sub_map_program(test, add32_insn);
	orlix_tcti_base_add_sub_seed(&regs, code, 0xffffffff00000002ULL, 0, 0, 0);
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, 0x00000003ULL, regs.regs[BAS_RD]);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));

	cmp_xzr = (adds32->pattern & adds32->mask) | 31U | (31U << 5);
	code = orlix_tcti_base_add_sub_map_program(test, cmp_xzr);
	orlix_tcti_base_add_sub_seed(&regs, code, 1, 2, 0, PSR_C_BIT);
	regs.regs[0] = 0xaaaaaaaaaaaaaaaaULL;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, 0xaaaaaaaaaaaaaaaaULL, regs.regs[0]);
	KUNIT_EXPECT_TRUE(test, regs.pstate & PSR_Z_BIT);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
}

static void orlix_tcti_base_add_sub_extend_shift_and_carry(struct kunit *test)
{
	const struct orlix_tcti_test_add_sub_source *add_ext =
		orlix_tcti_test_add_sub_name("ADD_64_addsub_ext");
	const struct orlix_tcti_test_add_sub_source *adcs =
		orlix_tcti_test_add_sub_name("ADCS_64_addsub_carry");
	const struct orlix_tcti_test_add_sub_source *add_shift =
		orlix_tcti_test_add_sub_name("ADD_64_addsub_shift");
	u32 insn;
	unsigned long code;
	struct pt_regs regs = {};
	struct orlix_tcti_result result;
	u8 option;

	KUNIT_ASSERT_NOT_NULL(test, add_ext);
	KUNIT_ASSERT_NOT_NULL(test, adcs);
	KUNIT_ASSERT_NOT_NULL(test, add_shift);
	for (option = 0; option < 8; option++) {
		insn = orlix_tcti_base_add_sub_legal_instruction(add_ext);
		insn &= ~(0x7U << 13);
		insn |= (u32)option << 13;
		code = orlix_tcti_base_add_sub_map_program(test, insn);
		orlix_tcti_base_add_sub_seed(&regs, code, 0x10, 0xff, 0, 0);
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason,
				    "extend %u", option);
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
	}

	insn = orlix_tcti_base_add_sub_legal_instruction(add_shift);
	insn |= 12U << 10;
	code = orlix_tcti_base_add_sub_map_program(test, insn);
	orlix_tcti_base_add_sub_seed(&regs, code, 1, 1, 0, 0);
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, 1ULL + (1ULL << 12), regs.regs[BAS_RD]);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));

	insn = orlix_tcti_base_add_sub_legal_instruction(adcs);
	code = orlix_tcti_base_add_sub_map_program(test, insn);
	orlix_tcti_base_add_sub_seed(&regs, code, U64_MAX, 0, 0, PSR_C_BIT);
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, 0ULL, regs.regs[BAS_RD]);
	KUNIT_EXPECT_TRUE(test, regs.pstate & PSR_C_BIT);
	KUNIT_EXPECT_TRUE(test, regs.pstate & PSR_Z_BIT);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
}

static void orlix_tcti_base_add_sub_simd_state_unchanged(struct kunit *test)
{
	const struct orlix_tcti_test_add_sub_source *add64 =
		orlix_tcti_test_add_sub_name("ADD_64_addsub_imm");
	u32 insn;
	unsigned long code;
	struct pt_regs regs = {};
	struct orlix_tcti_result result;
	u64 simd0;
	u64 simd1;

	KUNIT_ASSERT_NOT_NULL(test, add64);
	insn = orlix_tcti_base_add_sub_legal_instruction(add64);
	code = orlix_tcti_base_add_sub_map_program(test, insn);
	orlix_tcti_base_add_sub_seed(&regs, code, 4, 0, 0, 0);
	simd0 = current->thread.user_simd[0];
	simd1 = current->thread.user_simd[1];
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, simd0, current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, simd1, current->thread.user_simd[1]);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
}

static void orlix_tcti_base_add_sub_pointer_preserves_tag(struct kunit *test)
{
	const struct orlix_tcti_test_add_sub_source *addpt =
		orlix_tcti_test_add_sub_name("ADDPT_64_addsub_pt");
	const struct orlix_tcti_test_add_sub_source *subpt =
		orlix_tcti_test_add_sub_name("SUBPT_64_addsub_pt");
	u32 insn;
	unsigned long code;
	struct pt_regs regs = {};
	struct orlix_tcti_result result;
	u64 base = 0xaa00000000001000ULL;
	u64 addend = 0x20;

	KUNIT_ASSERT_NOT_NULL(test, addpt);
	KUNIT_ASSERT_NOT_NULL(test, subpt);
	insn = orlix_tcti_base_add_sub_legal_instruction(addpt);
	code = orlix_tcti_base_add_sub_map_program(test, insn);
	orlix_tcti_base_add_sub_seed(&regs, code, base, addend, 0, 0);
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, 0xaa00000000001020ULL, regs.regs[BAS_RD]);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));

	insn = orlix_tcti_base_add_sub_legal_instruction(addpt) | (3U << 10);
	code = orlix_tcti_base_add_sub_map_program(test, insn);
	orlix_tcti_base_add_sub_seed(&regs, code, base, addend, 0, 0);
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, 0xaa00000000001100ULL, regs.regs[BAS_RD]);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));

	insn = orlix_tcti_base_add_sub_legal_instruction(subpt);
	code = orlix_tcti_base_add_sub_map_program(test, insn);
	orlix_tcti_base_add_sub_seed(&regs, code, base, addend, 0, 0);
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, 0xaa00000000000fe0ULL, regs.regs[BAS_RD]);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
}

static struct kunit_case orlix_tcti_base_add_sub_source_bound_cases[] = {
	KUNIT_CASE(orlix_tcti_base_add_sub_decodes_exact_source_cohort),
	KUNIT_CASE(orlix_tcti_base_add_sub_binds_pinned_ddi0602_semantics),
	KUNIT_CASE(orlix_tcti_base_add_sub_production_resume),
	KUNIT_CASE(orlix_tcti_base_add_sub_reserved_encodings),
	KUNIT_CASE(orlix_tcti_base_add_sub_sp_zr_and_w_upper_zero),
	KUNIT_CASE(orlix_tcti_base_add_sub_extend_shift_and_carry),
	KUNIT_CASE(orlix_tcti_base_add_sub_simd_state_unchanged),
	KUNIT_CASE(orlix_tcti_base_add_sub_pointer_preserves_tag),
	{}
};

static struct kunit_suite orlix_tcti_base_add_sub_source_bound_suite = {
	.name = "orlix-tcti-base-add-sub-source-bound",
	.init = orlix_tcti_base_add_sub_test_init,
	.exit = orlix_tcti_base_add_sub_test_exit,
	.test_cases = orlix_tcti_base_add_sub_source_bound_cases,
};

kunit_test_suite(orlix_tcti_base_add_sub_source_bound_suite);
