// SPDX-License-Identifier: GPL-2.0-only
/*
 * Production-path proof for the 10 BASE_CONTROL_FLOW leaves owned by GitHub
 * #130. The old branch-control suite remains a regression and does not
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
#include "../switch_debug.h"
#include "orlix_tcti_memory_proof.h"
#include "orlix_tcti_native_observation.h"
#include "orlix_tcti_base_control_flow_production_capture.h"
#include "target_native_proof_contract_private.h"
#include "target_proof_ingestion_private.h"

#define BCF_SVC 0xd4000001U
#define BCF_BRK 0xd4200000U
#define BCF_NZCV (PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT | PSR_V_BIT)
#define BCF_EL0_COUNT 5U
#define BCF_FAMILY_COUNT 10U
#define BCF_DDI0602_COUNT 9U

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

struct orlix_tcti_test_control_flow_source {
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
static const struct orlix_tcti_test_control_flow_source orlix_tcti_test_sources[] = {
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

static bool orlix_tcti_test_is_base_control_flow(
	const struct orlix_tcti_test_control_flow_source *source)
{
	return source->ordinal < ARRAY_SIZE(orlix_tcti_test_source_families) &&
		orlix_tcti_test_source_families[source->ordinal] ==
			ORLIX_TCTI_TEST_SOURCE_FAMILY_BASE_CONTROL_FLOW;
}

static bool orlix_tcti_test_is_el0_control_flow(
	const struct orlix_tcti_test_control_flow_source *source)
{
	return orlix_tcti_test_is_base_control_flow(source) &&
		(!strcmp(source->operation, "BR") ||
		 !strcmp(source->operation, "BLR") ||
		 !strcmp(source->operation, "RET") ||
		 !strcmp(source->operation, "B_uncond") ||
		 !strcmp(source->operation, "BL"));
}

static const struct orlix_tcti_test_control_flow_source *
orlix_tcti_test_control_flow_name(const char *name)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++)
		if (orlix_tcti_test_is_base_control_flow(&orlix_tcti_test_sources[index]) &&
		    !strcmp(orlix_tcti_test_sources[index].name, name))
			return &orlix_tcti_test_sources[index];
	return NULL;
}

static bool orlix_tcti_test_control_flow_decode_is_family(
	const struct orlix_tcti_test_control_flow_source *source,
	enum orlix_tcti_decode_class decode_class)
{
	if (!strcmp(source->operation, "B_uncond") ||
	    !strcmp(source->operation, "BL"))
		return decode_class == ORLIX_TCTI_DECODE_UNCONDITIONAL_BRANCH_IMMEDIATE;
	if (!strcmp(source->operation, "BR") ||
	    !strcmp(source->operation, "BLR") ||
	    !strcmp(source->operation, "RET"))
		return decode_class == ORLIX_TCTI_DECODE_UNCONDITIONAL_BRANCH_REGISTER;
	return decode_class == ORLIX_TCTI_DECODE_UNSUPPORTED;
}

static u32 orlix_tcti_base_control_flow_imm_insn(
	const struct orlix_tcti_test_control_flow_source *source, s32 insns)
{
	return (source->pattern & source->mask) | ((u32)insns & 0x03ffffffU);
}

static u32 orlix_tcti_base_control_flow_reg_insn(
	const struct orlix_tcti_test_control_flow_source *source, u8 rn)
{
	return (source->pattern & source->mask) | ((u32)rn << 5);
}

static u32 orlix_tcti_base_control_flow_legal_instruction(
	const struct orlix_tcti_test_control_flow_source *source)
{
	if (!strcmp(source->operation, "B_uncond") ||
	    !strcmp(source->operation, "BL"))
		return orlix_tcti_base_control_flow_imm_insn(source, 1);
	if (!strcmp(source->operation, "RET"))
		return orlix_tcti_base_control_flow_reg_insn(source, 30);
	if (!strcmp(source->operation, "BR") ||
	    !strcmp(source->operation, "BLR"))
		return orlix_tcti_base_control_flow_reg_insn(source, 0);
	return source->pattern;
}

static int orlix_tcti_base_control_flow_test_init(struct kunit *test)
{
	struct mm_struct *mm = mm_alloc();

	if (!mm)
		return -ENOMEM;
	kthread_use_mm(mm);
	test->priv = mm;
	return 0;
}

static void orlix_tcti_base_control_flow_test_exit(struct kunit *test)
{
	struct mm_struct *mm = test->priv;

	if (!mm)
		return;
	kthread_unuse_mm(mm);
	mmput(mm);
}

static unsigned long orlix_tcti_base_control_flow_map_program(struct kunit *test,
							      u32 instruction)
{
	u8 bytes[16] = {};
	u32 program[2] = { instruction, BCF_SVC };

	memcpy(bytes, program, sizeof(program));
	return orlix_tcti_memory_proof_map_bytes(test, bytes, sizeof(bytes),
						 PROT_READ | PROT_EXEC);
}

static void orlix_tcti_base_control_flow_seed(struct pt_regs *regs,
					      unsigned long code, u64 pstate)
{
	memset(regs, 0, sizeof(*regs));
	regs->pc = code;
	regs->pstate = PSR_MODE_EL0t | (pstate & BCF_NZCV);
	regs->syscallno = NO_SYSCALL;
	regs->regs[0] = 0x1111111111111111ULL;
	regs->regs[1] = 0x2222222222222222ULL;
	regs->regs[30] = 0x3333333333333333ULL;
	current->thread.user_simd_valid = 1;
	current->thread.user_simd[0] = 0xaaaaaaaaaaaaaaaaULL;
	current->thread.user_simd[1] = 0xbbbbbbbbbbbbbbbbULL;
}

static void orlix_tcti_base_control_flow_seed_target(
	struct pt_regs *regs, unsigned long code, u64 pstate,
	const struct orlix_tcti_test_control_flow_source *source)
{
	orlix_tcti_base_control_flow_seed(regs, code, pstate);
	if (!strcmp(source->operation, "BR") ||
	    !strcmp(source->operation, "BLR"))
		regs->regs[0] = code + sizeof(u32);
	if (!strcmp(source->operation, "RET"))
		regs->regs[30] = code + sizeof(u32);
}

static void orlix_tcti_base_control_flow_expect_success(struct kunit *test,
	const struct orlix_tcti_test_control_flow_source *source,
	const struct orlix_tcti_result *result, const struct pt_regs *regs,
	unsigned long code, u64 link_before)
{
	KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_SYSCALL, result->reason,
			    "%s ordinal %u reason", source->name, source->ordinal);
	KUNIT_EXPECT_EQ(test, 0L, result->status);
	KUNIT_EXPECT_EQ(test, BCF_SVC, result->instruction);
	KUNIT_EXPECT_EQ_MSG(test, code + sizeof(u32), regs->pc,
			    "%s ordinal %u pc", source->name, source->ordinal);
	if (!strcmp(source->operation, "BL") ||
	    !strcmp(source->operation, "BLR"))
		KUNIT_EXPECT_EQ_MSG(test, code + sizeof(u32), regs->regs[30],
				    "%s ordinal %u x30", source->name,
				    source->ordinal);
	else if (strcmp(source->operation, "RET"))
		KUNIT_EXPECT_EQ_MSG(test, link_before, regs->regs[30],
				    "%s ordinal %u x30 preserved", source->name,
				    source->ordinal);
}

static void orlix_tcti_base_control_flow_capture_run(struct kunit *test,
	const struct orlix_tcti_test_control_flow_source *source, u32 obligation,
	struct pt_regs *regs, unsigned long code, u64 link_before)
{
	const void *token;
	struct orlix_tcti_native_capture_session *capture = NULL;
	struct orlix_tcti_native_wire_record wire = {};
	struct orlix_tcti_target_proof_ingestion_ledger *ledger;
	struct orlix_tcti_result result;
	int ret;

	token = orlix_tcti_base_control_flow_production_capture_token(
		source->ordinal, obligation);
	KUNIT_EXPECT_TRUE_MSG(test, token != NULL, "%s token %u", source->name,
			      obligation);
	if (!token)
		return;
	ret = orlix_tcti_native_capture_begin(token, source->ordinal, obligation,
					      &capture);
	KUNIT_EXPECT_EQ_MSG(test, 0, ret, "%s begin %u", source->name, obligation);
	result = orlix_tcti_resume_user(current, regs, current->mm);
	orlix_tcti_base_control_flow_expect_success(test, source, &result, regs,
						    code, link_before);
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

static void orlix_tcti_base_control_flow_decodes_exact_source_cohort(
	struct kunit *test)
{
	size_t index;
	size_t count = 0;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++) {
		const struct orlix_tcti_test_control_flow_source *source =
			&orlix_tcti_test_sources[index];
		struct orlix_tcti_decoded_instruction decoded;
		u32 instruction;

		if (!orlix_tcti_test_is_base_control_flow(source))
			continue;
		instruction = orlix_tcti_base_control_flow_legal_instruction(source);
		KUNIT_ASSERT_EQ_MSG(test, source->pattern,
				instruction & source->mask, "%s", source->name);
		decoded = orlix_tcti_decode_aarch64(instruction);
		KUNIT_EXPECT_TRUE_MSG(test,
			orlix_tcti_test_control_flow_decode_is_family(source,
				decoded.decode_class),
			"%s class %u insn %#x", source->name, decoded.decode_class,
			instruction);
		KUNIT_EXPECT_EQ_MSG(test, source->ordinal, decoded.source_ordinal,
				"%s %#x", source->name, instruction);
		count++;
	}
	KUNIT_EXPECT_EQ(test, BCF_FAMILY_COUNT, count);
}

static void orlix_tcti_base_control_flow_binds_pinned_ddi0602_semantics(
	struct kunit *test)
{
	size_t index;
	size_t ddi = 0;
	size_t unspecified = 0;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++) {
		const struct orlix_tcti_test_control_flow_source *source =
			&orlix_tcti_test_sources[index];

		if (!orlix_tcti_test_is_base_control_flow(source))
			continue;
		KUNIT_ASSERT_LT_MSG(test, source->ordinal,
			ARRAY_SIZE(orlix_tcti_test_has_ddi0602_semantics), "%s",
			source->name);
		if (!strcmp(source->name, "TEXIT_te_branch_reg")) {
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
	KUNIT_EXPECT_EQ(test, BCF_DDI0602_COUNT, ddi);
	KUNIT_EXPECT_EQ(test, 1U, unspecified);
}

static void orlix_tcti_base_control_flow_production_resume(struct kunit *test)
{
	size_t index;
	size_t seen = 0;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++) {
		const struct orlix_tcti_test_control_flow_source *source =
			&orlix_tcti_test_sources[index];
		u32 instruction;
		struct orlix_tcti_decoded_instruction decoded;
		struct pt_regs regs = {};
		unsigned long code;
		u64 link_before;

		if (!orlix_tcti_test_is_el0_control_flow(source))
			continue;
		seen++;
		instruction = orlix_tcti_base_control_flow_legal_instruction(source);
		decoded = orlix_tcti_decode_aarch64(instruction);
		KUNIT_ASSERT_TRUE_MSG(test,
			orlix_tcti_test_control_flow_decode_is_family(source,
				decoded.decode_class),
			"%s ordinal %u class %u", source->name, source->ordinal,
			decoded.decode_class);
		KUNIT_ASSERT_EQ_MSG(test, source->ordinal, decoded.source_ordinal,
			"%s ordinal", source->name);
		code = orlix_tcti_base_control_flow_map_program(test, instruction);
		orlix_tcti_base_control_flow_seed_target(&regs, code, BCF_NZCV,
							 source);
		link_before = regs.regs[30];
		orlix_tcti_base_control_flow_capture_run(test, source,
			ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS, &regs, code,
			link_before);
		orlix_tcti_base_control_flow_seed_target(&regs, code, BCF_NZCV,
							 source);
		link_before = regs.regs[30];
		orlix_tcti_base_control_flow_capture_run(test, source,
			ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC, &regs, code,
			link_before);
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
	}
	KUNIT_EXPECT_EQ(test, BCF_EL0_COUNT, seen);
}

static void orlix_tcti_base_control_flow_link_and_xzr(struct kunit *test)
{
	const struct orlix_tcti_test_control_flow_source *blr =
		orlix_tcti_test_control_flow_name("BLR_64_branch_reg");
	const struct orlix_tcti_test_control_flow_source *ret =
		orlix_tcti_test_control_flow_name("RET_64R_branch_reg");
	const struct orlix_tcti_test_control_flow_source *br =
		orlix_tcti_test_control_flow_name("BR_64_branch_reg");
	u32 insn;
	unsigned long code;
	struct pt_regs regs = {};
	struct orlix_tcti_result result;

	KUNIT_ASSERT_NOT_NULL(test, blr);
	KUNIT_ASSERT_NOT_NULL(test, ret);
	KUNIT_ASSERT_NOT_NULL(test, br);

	insn = orlix_tcti_base_control_flow_reg_insn(blr, 30);
	code = orlix_tcti_base_control_flow_map_program(test, insn);
	orlix_tcti_base_control_flow_seed(&regs, code, 0);
	regs.regs[30] = code + sizeof(u32);
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, code + sizeof(u32), regs.regs[30]);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));

	insn = orlix_tcti_base_control_flow_reg_insn(ret, 0);
	code = orlix_tcti_base_control_flow_map_program(test, insn);
	orlix_tcti_base_control_flow_seed(&regs, code, 0);
	regs.regs[0] = code + sizeof(u32);
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, code + sizeof(u32), regs.pc);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));

	insn = orlix_tcti_base_control_flow_reg_insn(br, 31);
	code = orlix_tcti_base_control_flow_map_program(test, insn);
	orlix_tcti_base_control_flow_seed(&regs, code, 0);
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_TRUE(test,
		result.reason == ORLIX_TCTI_EXIT_USER_FAULT ||
		result.reason == ORLIX_TCTI_EXIT_ALIGNMENT_FAULT);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
}

static void orlix_tcti_base_control_flow_offsets_and_alignment(struct kunit *test)
{
	const struct orlix_tcti_test_control_flow_source *b =
		orlix_tcti_test_control_flow_name("B_only_branch_imm");
	const struct orlix_tcti_test_control_flow_source *br =
		orlix_tcti_test_control_flow_name("BR_64_branch_reg");
	u32 program[3];
	u8 bytes[16] = {};
	u32 insn;
	unsigned long code;
	struct pt_regs regs = {};
	struct orlix_tcti_result result;
	struct orlix_tcti_decoded_instruction decoded;

	KUNIT_ASSERT_NOT_NULL(test, b);
	KUNIT_ASSERT_NOT_NULL(test, br);

	program[0] = orlix_tcti_base_control_flow_imm_insn(b, 2);
	program[1] = BCF_BRK;
	program[2] = BCF_SVC;
	memcpy(bytes, program, sizeof(program));
	code = orlix_tcti_memory_proof_map_bytes(test, bytes, sizeof(bytes),
						 PROT_READ | PROT_EXEC);
	orlix_tcti_base_control_flow_seed(&regs, code, 0);
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, code + 2U * sizeof(u32), regs.pc);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));

	program[0] = BCF_SVC;
	program[1] = BCF_BRK;
	program[2] = orlix_tcti_base_control_flow_imm_insn(b, -2);
	memcpy(bytes, program, sizeof(program));
	code = orlix_tcti_memory_proof_map_bytes(test, bytes, sizeof(bytes),
						 PROT_READ | PROT_EXEC);
	orlix_tcti_base_control_flow_seed(&regs, code + 2U * sizeof(u32), 0);
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, code, regs.pc);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));

	insn = orlix_tcti_base_control_flow_imm_insn(b, 0x01ffffff);
	decoded = orlix_tcti_decode_aarch64(insn);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNCONDITIONAL_BRANCH_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 0x7fffffcLL, decoded.branch_imm);
	insn = orlix_tcti_base_control_flow_imm_insn(b, (s32)0x02000000);
	decoded = orlix_tcti_decode_aarch64(insn);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNCONDITIONAL_BRANCH_IMMEDIATE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, -0x8000000LL, decoded.branch_imm);

	insn = orlix_tcti_base_control_flow_reg_insn(br, 0);
	code = orlix_tcti_base_control_flow_map_program(test, insn);
	orlix_tcti_base_control_flow_seed(&regs, code, 0);
	regs.regs[0] = code + 2U;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_ALIGNMENT_FAULT, result.reason);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
}

static void orlix_tcti_base_control_flow_simd_and_flags_unchanged(struct kunit *test)
{
	const struct orlix_tcti_test_control_flow_source *bl =
		orlix_tcti_test_control_flow_name("BL_only_branch_imm");
	u32 insn;
	unsigned long code;
	struct pt_regs regs = {};
	struct orlix_tcti_result result;
	u64 simd0;
	u64 simd1;
	u64 flags;

	KUNIT_ASSERT_NOT_NULL(test, bl);
	insn = orlix_tcti_base_control_flow_legal_instruction(bl);
	code = orlix_tcti_base_control_flow_map_program(test, insn);
	orlix_tcti_base_control_flow_seed(&regs, code, BCF_NZCV);
	simd0 = current->thread.user_simd[0];
	simd1 = current->thread.user_simd[1];
	flags = regs.pstate & BCF_NZCV;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, simd0, current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, simd1, current->thread.user_simd[1]);
	KUNIT_EXPECT_EQ(test, flags, regs.pstate & BCF_NZCV);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
}

static void orlix_tcti_base_control_flow_non_el0_rejected(struct kunit *test)
{
	size_t index;
	size_t seen = 0;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++) {
		const struct orlix_tcti_test_control_flow_source *source =
			&orlix_tcti_test_sources[index];
		u32 instruction;
		struct orlix_tcti_decoded_instruction decoded;
		struct pt_regs regs = {};
		struct pt_regs before;
		struct orlix_tcti_result result;
		unsigned long code;

		if (!orlix_tcti_test_is_base_control_flow(source) ||
		    orlix_tcti_test_is_el0_control_flow(source))
			continue;
		seen++;
		instruction = source->pattern;
		decoded = orlix_tcti_decode_aarch64(instruction);
		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
				    decoded.decode_class, "%s", source->name);
		KUNIT_EXPECT_EQ_MSG(test, source->ordinal, decoded.source_ordinal,
				    "%s ordinal", source->name);
		code = orlix_tcti_base_control_flow_map_program(test, instruction);
		orlix_tcti_base_control_flow_seed(&regs, code, BCF_NZCV);
		before = regs;
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
				    result.reason, "%s", source->name);
		KUNIT_EXPECT_EQ_MSG(test, -EOPNOTSUPP, result.status, "%s",
				    source->name);
		KUNIT_EXPECT_EQ(test, instruction, result.instruction);
		KUNIT_EXPECT_EQ(test, before.pc, result.pc);
		KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
	}
	KUNIT_EXPECT_EQ(test, 5U, seen);
}

static void orlix_tcti_base_control_flow_reserved_encodings(struct kunit *test)
{
	struct orlix_tcti_decoded_instruction decoded;
	const struct orlix_tcti_test_control_flow_source *texit =
		orlix_tcti_test_control_flow_name("TEXIT_te_branch_reg");

	decoded = orlix_tcti_decode_aarch64(0xd67f0000U);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED, decoded.decode_class);
	decoded = orlix_tcti_decode_aarch64(0xd6df03e0U);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED, decoded.decode_class);
	KUNIT_ASSERT_NOT_NULL(test, texit);
	decoded = orlix_tcti_decode_aarch64(texit->pattern | BIT(10));
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED, decoded.decode_class);
	KUNIT_EXPECT_EQ(test, texit->ordinal, decoded.source_ordinal);
}

static struct kunit_case orlix_tcti_base_control_flow_source_bound_cases[] = {
	KUNIT_CASE(orlix_tcti_base_control_flow_decodes_exact_source_cohort),
	KUNIT_CASE(orlix_tcti_base_control_flow_binds_pinned_ddi0602_semantics),
	KUNIT_CASE(orlix_tcti_base_control_flow_production_resume),
	KUNIT_CASE(orlix_tcti_base_control_flow_link_and_xzr),
	KUNIT_CASE(orlix_tcti_base_control_flow_offsets_and_alignment),
	KUNIT_CASE(orlix_tcti_base_control_flow_simd_and_flags_unchanged),
	KUNIT_CASE(orlix_tcti_base_control_flow_non_el0_rejected),
	KUNIT_CASE(orlix_tcti_base_control_flow_reserved_encodings),
	{}
};

static struct kunit_suite orlix_tcti_base_control_flow_source_bound_suite = {
	.name = "orlix-tcti-base-control-flow-source-bound",
	.init = orlix_tcti_base_control_flow_test_init,
	.exit = orlix_tcti_base_control_flow_test_exit,
	.test_cases = orlix_tcti_base_control_flow_source_bound_cases,
};

kunit_test_suite(orlix_tcti_base_control_flow_source_bound_suite);
