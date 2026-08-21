// SPDX-License-Identifier: GPL-2.0-only
/*
 * Production-path proof for the 60 BASE_CONDITIONAL leaves owned by GitHub
 * #131. The old branch-control and integer-conditional suites remain
 * regression and do not receive #120 close credit for these ordinals.
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
#include "orlix_tcti_base_conditional_production_capture.h"
#include "target_native_proof_contract_private.h"
#include "target_proof_ingestion_private.h"

#define BCD_SVC_NOT_TAKEN 0xd4000021U
#define BCD_SVC_TAKEN 0xd4000041U
#define BCD_SVC 0xd4000001U
#define BCD_NZCV (PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT | PSR_V_BIT)
#define BCD_FAMILY_COUNT 60U
#define BCD_EL0_COUNT 23U
#define BCD_NON_EL0_COUNT 37U

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

struct orlix_tcti_test_conditional_source {
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
static const struct orlix_tcti_test_conditional_source orlix_tcti_test_sources[] = {
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

static bool orlix_tcti_test_is_base_conditional(
	const struct orlix_tcti_test_conditional_source *source)
{
	return source->ordinal < ARRAY_SIZE(orlix_tcti_test_source_families) &&
		orlix_tcti_test_source_families[source->ordinal] ==
			ORLIX_TCTI_TEST_SOURCE_FAMILY_BASE_CONDITIONAL;
}

static bool orlix_tcti_test_is_el0_conditional(
	const struct orlix_tcti_test_conditional_source *source)
{
	return orlix_tcti_test_is_base_conditional(source) &&
		(!strcmp(source->operation, "B_cond") ||
		 !strcmp(source->operation, "CBZ") ||
		 !strcmp(source->operation, "CBNZ") ||
		 !strcmp(source->operation, "TBZ") ||
		 !strcmp(source->operation, "TBNZ") ||
		 !strcmp(source->operation, "CSEL") ||
		 !strcmp(source->operation, "CSINC") ||
		 !strcmp(source->operation, "CSINV") ||
		 !strcmp(source->operation, "CSNEG") ||
		 !strcmp(source->operation, "CCMN_reg") ||
		 !strcmp(source->operation, "CCMP_reg") ||
		 !strcmp(source->operation, "CCMN_imm") ||
		 !strcmp(source->operation, "CCMP_imm"));
}

static bool orlix_tcti_test_is_branch_conditional(
	const struct orlix_tcti_test_conditional_source *source)
{
	return !strcmp(source->operation, "B_cond") ||
		!strcmp(source->operation, "CBZ") ||
		!strcmp(source->operation, "CBNZ") ||
		!strcmp(source->operation, "TBZ") ||
		!strcmp(source->operation, "TBNZ");
}

static const struct orlix_tcti_test_conditional_source *
orlix_tcti_test_conditional_name(const char *name)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++)
		if (orlix_tcti_test_is_base_conditional(&orlix_tcti_test_sources[index]) &&
		    !strcmp(orlix_tcti_test_sources[index].name, name))
			return &orlix_tcti_test_sources[index];
	return NULL;
}

static bool orlix_tcti_test_conditional_decode_is_family(
	const struct orlix_tcti_test_conditional_source *source,
	enum orlix_tcti_decode_class decode_class)
{
	if (!strcmp(source->operation, "B_cond"))
		return decode_class == ORLIX_TCTI_DECODE_CONDITIONAL_BRANCH_IMMEDIATE;
	if (!strcmp(source->operation, "CBZ") ||
	    !strcmp(source->operation, "CBNZ"))
		return decode_class == ORLIX_TCTI_DECODE_COMPARE_BRANCH_IMMEDIATE;
	if (!strcmp(source->operation, "TBZ") ||
	    !strcmp(source->operation, "TBNZ"))
		return decode_class == ORLIX_TCTI_DECODE_TEST_BRANCH_IMMEDIATE;
	if (!strcmp(source->operation, "CSEL") ||
	    !strcmp(source->operation, "CSINC") ||
	    !strcmp(source->operation, "CSINV") ||
	    !strcmp(source->operation, "CSNEG"))
		return decode_class == ORLIX_TCTI_DECODE_CONDITIONAL_SELECT;
	if (!strcmp(source->operation, "CCMN_reg") ||
	    !strcmp(source->operation, "CCMP_reg") ||
	    !strcmp(source->operation, "CCMN_imm") ||
	    !strcmp(source->operation, "CCMP_imm"))
		return decode_class == ORLIX_TCTI_DECODE_CONDITIONAL_COMPARE;
	return decode_class == ORLIX_TCTI_DECODE_UNSUPPORTED;
}

static u32 orlix_tcti_base_conditional_legal_instruction(
	const struct orlix_tcti_test_conditional_source *source)
{
	u32 instruction = source->pattern & source->mask;

	if (!strcmp(source->operation, "B_cond"))
		return instruction | (2U << 5);
	if (!strcmp(source->operation, "CBZ") ||
	    !strcmp(source->operation, "CBNZ"))
		return instruction | (2U << 5) | 5U;
	if (!strcmp(source->operation, "TBZ") ||
	    !strcmp(source->operation, "TBNZ"))
		return instruction | (2U << 5) | 5U;
	if (!strcmp(source->operation, "CSEL") ||
	    !strcmp(source->operation, "CSINC") ||
	    !strcmp(source->operation, "CSINV") ||
	    !strcmp(source->operation, "CSNEG"))
		return instruction | (1U << 5) | (2U << 16);
	if (!strcmp(source->operation, "CCMN_reg") ||
	    !strcmp(source->operation, "CCMP_reg") ||
	    !strcmp(source->operation, "CCMN_imm") ||
	    !strcmp(source->operation, "CCMP_imm"))
		return instruction | (1U << 5) | (2U << 16);
	return source->pattern;
}

static int orlix_tcti_base_conditional_test_init(struct kunit *test)
{
	struct mm_struct *mm = mm_alloc();

	if (!mm)
		return -ENOMEM;
	kthread_use_mm(mm);
	test->priv = mm;
	return 0;
}

static void orlix_tcti_base_conditional_test_exit(struct kunit *test)
{
	struct mm_struct *mm = test->priv;

	if (!mm)
		return;
	kthread_unuse_mm(mm);
	mmput(mm);
}

static unsigned long orlix_tcti_base_conditional_map_branch(struct kunit *test,
							    u32 instruction)
{
	u8 bytes[16] = {};
	u32 program[3] = { instruction, BCD_SVC_NOT_TAKEN, BCD_SVC_TAKEN };

	memcpy(bytes, program, sizeof(program));
	return orlix_tcti_memory_proof_map_bytes(test, bytes, sizeof(bytes),
						 PROT_READ | PROT_EXEC);
}

static unsigned long orlix_tcti_base_conditional_map_data(struct kunit *test,
							  u32 instruction)
{
	u8 bytes[16] = {};
	u32 program[2] = { instruction, BCD_SVC };

	memcpy(bytes, program, sizeof(program));
	return orlix_tcti_memory_proof_map_bytes(test, bytes, sizeof(bytes),
						 PROT_READ | PROT_EXEC);
}

static void orlix_tcti_base_conditional_seed(struct pt_regs *regs,
					     unsigned long code, u64 pstate)
{
	memset(regs, 0, sizeof(*regs));
	regs->pc = code;
	regs->pstate = PSR_MODE_EL0t | (pstate & BCD_NZCV);
	regs->syscallno = NO_SYSCALL;
	regs->regs[0] = 0x1111111111111111ULL;
	regs->regs[1] = 0x2222222222222222ULL;
	regs->regs[2] = 0x3333333333333333ULL;
	regs->regs[5] = 0;
	regs->regs[30] = 0x4444444444444444ULL;
	current->thread.user_simd_valid = 1;
	current->thread.user_simd[0] = 0xaaaaaaaaaaaaaaaaULL;
	current->thread.user_simd[1] = 0xbbbbbbbbbbbbbbbbULL;
}

static void orlix_tcti_base_conditional_seed_taken(
	struct pt_regs *regs, unsigned long code,
	const struct orlix_tcti_test_conditional_source *source, bool taken)
{
	orlix_tcti_base_conditional_seed(regs, code, taken ? PSR_Z_BIT : 0);
	if (!strcmp(source->operation, "CBZ"))
		regs->regs[5] = taken ? 0 : 1;
	else if (!strcmp(source->operation, "CBNZ"))
		regs->regs[5] = taken ? 1 : 0;
	else if (!strcmp(source->operation, "TBZ"))
		regs->regs[5] = taken ? 0 : BIT_ULL(0);
	else if (!strcmp(source->operation, "TBNZ"))
		regs->regs[5] = taken ? BIT_ULL(0) : 0;
}

static bool orlix_tcti_base_conditional_has_capture(
	const struct orlix_tcti_test_conditional_source *source)
{
	return orlix_tcti_test_is_el0_conditional(source);
}

static void orlix_tcti_base_conditional_expect_success(struct kunit *test,
	const struct orlix_tcti_test_conditional_source *source,
	const struct orlix_tcti_result *result, const struct pt_regs *regs,
	unsigned long code, bool branch)
{
	KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_SYSCALL, result->reason,
			    "%s ordinal %u reason", source->name, source->ordinal);
	KUNIT_EXPECT_EQ(test, 0L, result->status);
	if (branch)
		KUNIT_EXPECT_EQ_MSG(test, code + 2 * sizeof(u32), regs->pc,
				    "%s ordinal %u pc", source->name,
				    source->ordinal);
	else
		KUNIT_EXPECT_EQ_MSG(test, code + sizeof(u32), regs->pc,
				    "%s ordinal %u pc", source->name,
				    source->ordinal);
}

static void orlix_tcti_base_conditional_capture_run(struct kunit *test,
	const struct orlix_tcti_test_conditional_source *source, u32 obligation,
	struct pt_regs *regs, unsigned long code, bool branch)
{
	const void *token;
	struct orlix_tcti_native_capture_session *capture = NULL;
	struct orlix_tcti_native_wire_record wire = {};
	struct orlix_tcti_target_proof_ingestion_ledger *ledger;
	struct orlix_tcti_result result;
	int ret;

	token = orlix_tcti_base_conditional_production_capture_token(
		source->ordinal, obligation);
	KUNIT_EXPECT_TRUE_MSG(test, token != NULL, "%s token %u", source->name,
			      obligation);
	if (!token)
		return;
	ret = orlix_tcti_native_capture_begin(token, source->ordinal, obligation,
					      &capture);
	KUNIT_EXPECT_EQ_MSG(test, 0, ret, "%s begin %u", source->name, obligation);
	result = orlix_tcti_resume_user(current, regs, current->mm);
	orlix_tcti_base_conditional_expect_success(test, source, &result, regs,
						   code, branch);
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

static void orlix_tcti_base_conditional_decodes_exact_source_cohort(
	struct kunit *test)
{
	size_t index;
	size_t count = 0;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++) {
		const struct orlix_tcti_test_conditional_source *source =
			&orlix_tcti_test_sources[index];
		struct orlix_tcti_decoded_instruction decoded;
		u32 instruction;

		if (!orlix_tcti_test_is_base_conditional(source))
			continue;
		instruction = orlix_tcti_base_conditional_legal_instruction(source);
		KUNIT_ASSERT_EQ_MSG(test, source->pattern,
				instruction & source->mask, "%s", source->name);
		decoded = orlix_tcti_decode_aarch64(instruction);
		KUNIT_EXPECT_TRUE_MSG(test,
			orlix_tcti_test_conditional_decode_is_family(source,
				decoded.decode_class),
			"%s class %u insn %#x", source->name, decoded.decode_class,
			instruction);
		KUNIT_EXPECT_EQ_MSG(test, source->ordinal, decoded.source_ordinal,
				"%s %#x", source->name, instruction);
		count++;
	}
	KUNIT_EXPECT_EQ(test, BCD_FAMILY_COUNT, count);
}

static void orlix_tcti_base_conditional_binds_pinned_ddi0602_semantics(
	struct kunit *test)
{
	size_t index;
	size_t ddi = 0;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++) {
		const struct orlix_tcti_test_conditional_source *source =
			&orlix_tcti_test_sources[index];

		if (!orlix_tcti_test_is_base_conditional(source))
			continue;
		KUNIT_ASSERT_LT_MSG(test, source->ordinal,
			ARRAY_SIZE(orlix_tcti_test_has_ddi0602_semantics), "%s",
			source->name);
		KUNIT_EXPECT_TRUE_MSG(test,
			orlix_tcti_test_has_ddi0602_semantics[source->ordinal], "%s",
			source->name);
		ddi++;
	}
	KUNIT_EXPECT_EQ(test, BCD_FAMILY_COUNT, ddi);
}

static void orlix_tcti_base_conditional_production_resume(struct kunit *test)
{
	size_t index;
	size_t seen = 0;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++) {
		const struct orlix_tcti_test_conditional_source *source =
			&orlix_tcti_test_sources[index];
		u32 instruction;
		struct orlix_tcti_decoded_instruction decoded;
		struct pt_regs regs = {};
		unsigned long code;
		bool branch;

		if (!orlix_tcti_test_is_el0_conditional(source))
			continue;
		KUNIT_ASSERT_TRUE_MSG(test,
			orlix_tcti_base_conditional_has_capture(source),
			"%s missing #120 capture", source->name);
		seen++;
		instruction = orlix_tcti_base_conditional_legal_instruction(source);
		decoded = orlix_tcti_decode_aarch64(instruction);
		KUNIT_ASSERT_TRUE_MSG(test,
			orlix_tcti_test_conditional_decode_is_family(source,
				decoded.decode_class),
			"%s class %u", source->name, decoded.decode_class);
		KUNIT_ASSERT_EQ_MSG(test, source->ordinal, decoded.source_ordinal,
				    "%s ordinal", source->name);
		branch = orlix_tcti_test_is_branch_conditional(source);
		code = branch ?
			orlix_tcti_base_conditional_map_branch(test, instruction) :
			orlix_tcti_base_conditional_map_data(test, instruction);
		if (branch)
			orlix_tcti_base_conditional_seed_taken(&regs, code, source,
							       true);
		else
			orlix_tcti_base_conditional_seed(&regs, code, PSR_Z_BIT);
		orlix_tcti_base_conditional_capture_run(test, source,
			ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS, &regs, code,
			branch);
		if (branch)
			orlix_tcti_base_conditional_seed_taken(&regs, code, source,
							       true);
		else
			orlix_tcti_base_conditional_seed(&regs, code, PSR_Z_BIT);
		orlix_tcti_base_conditional_capture_run(test, source,
			ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC, &regs, code, branch);
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
	}
	KUNIT_EXPECT_EQ(test, BCD_EL0_COUNT, seen);
}

static bool orlix_tcti_base_conditional_cond_passed(u8 cond, u64 nzcv)
{
	bool n = !!(nzcv & PSR_N_BIT);
	bool z = !!(nzcv & PSR_Z_BIT);
	bool c = !!(nzcv & PSR_C_BIT);
	bool v = !!(nzcv & PSR_V_BIT);
	bool result;

	switch (cond >> 1) {
	case 0:
		result = z;
		break;
	case 1:
		result = c;
		break;
	case 2:
		result = n;
		break;
	case 3:
		result = v;
		break;
	case 4:
		result = c && !z;
		break;
	case 5:
		result = n == v;
		break;
	case 6:
		result = (n == v) && !z;
		break;
	default:
		result = true;
		break;
	}
	if (cond & 1U && cond != 15U)
		result = !result;
	return result;
}

static void orlix_tcti_base_conditional_taken_and_not_taken(struct kunit *test)
{
	const struct orlix_tcti_test_conditional_source *bcond =
		orlix_tcti_test_conditional_name("B_only_condbranch");
	const struct orlix_tcti_test_conditional_source *cbz32 =
		orlix_tcti_test_conditional_name("CBZ_32_compbranch");
	const struct orlix_tcti_test_conditional_source *cbz64 =
		orlix_tcti_test_conditional_name("CBZ_64_compbranch");
	const struct orlix_tcti_test_conditional_source *cbnz32 =
		orlix_tcti_test_conditional_name("CBNZ_32_compbranch");
	const struct orlix_tcti_test_conditional_source *cbnz64 =
		orlix_tcti_test_conditional_name("CBNZ_64_compbranch");
	const struct orlix_tcti_test_conditional_source *tbz =
		orlix_tcti_test_conditional_name("TBZ_only_testbranch");
	const struct orlix_tcti_test_conditional_source *tbnz =
		orlix_tcti_test_conditional_name("TBNZ_only_testbranch");
	static const u64 nzcv_samples[] = {
		0, PSR_Z_BIT, PSR_C_BIT, PSR_N_BIT, PSR_V_BIT,
		PSR_N_BIT | PSR_V_BIT, PSR_C_BIT | PSR_Z_BIT,
		PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT | PSR_V_BIT,
	};
	u8 cond;
	size_t sample;
	unsigned int taken;

	KUNIT_ASSERT_NOT_NULL(test, bcond);
	KUNIT_ASSERT_NOT_NULL(test, cbz32);
	KUNIT_ASSERT_NOT_NULL(test, cbz64);
	KUNIT_ASSERT_NOT_NULL(test, cbnz32);
	KUNIT_ASSERT_NOT_NULL(test, cbnz64);
	KUNIT_ASSERT_NOT_NULL(test, tbz);
	KUNIT_ASSERT_NOT_NULL(test, tbnz);

	for (cond = 0; cond < 16; cond++) {
		for (sample = 0; sample < ARRAY_SIZE(nzcv_samples); sample++) {
			u32 instruction = (bcond->pattern & bcond->mask) |
				(2U << 5) | cond;
			unsigned long code;
			struct pt_regs regs = {};
			struct orlix_tcti_result result;
			bool expect_taken;

			expect_taken = orlix_tcti_base_conditional_cond_passed(
				cond, nzcv_samples[sample]);
			code = orlix_tcti_base_conditional_map_branch(test,
								     instruction);
			orlix_tcti_base_conditional_seed(&regs, code,
							 nzcv_samples[sample]);
			result = orlix_tcti_resume_user(current, &regs, current->mm);
			KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_SYSCALL,
				result.reason, "B.cond %u nzcv %#llx", cond,
				(unsigned long long)nzcv_samples[sample]);
			KUNIT_EXPECT_EQ_MSG(test,
				expect_taken ? BCD_SVC_TAKEN : BCD_SVC_NOT_TAKEN,
				result.instruction, "B.cond %u", cond);
			KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
		}
	}

	for (taken = 0; taken < 2; taken++) {
		const struct orlix_tcti_test_conditional_source *local[6];
		size_t leaf;

		local[0] = cbz32;
		local[1] = cbz64;
		local[2] = cbnz32;
		local[3] = cbnz64;
		local[4] = tbz;
		local[5] = tbnz;
		for (leaf = 0; leaf < 6; leaf++) {
			u32 instruction;
			unsigned long code;
			struct pt_regs regs = {};
			struct orlix_tcti_result result;

			instruction = orlix_tcti_base_conditional_legal_instruction(
				local[leaf]);
			code = orlix_tcti_base_conditional_map_branch(test,
								     instruction);
			orlix_tcti_base_conditional_seed_taken(&regs, code,
							       local[leaf], taken);
			if (strstr(local[leaf]->name, "32") &&
			    (!strcmp(local[leaf]->operation, "CBZ") ||
			     !strcmp(local[leaf]->operation, "CBNZ")))
				regs.regs[5] |= 0xffffffff00000000ULL;
			result = orlix_tcti_resume_user(current, &regs, current->mm);
			KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_SYSCALL,
				result.reason, "%s taken %u", local[leaf]->name,
				taken);
			KUNIT_EXPECT_EQ_MSG(test,
				taken ? BCD_SVC_TAKEN : BCD_SVC_NOT_TAKEN,
				result.instruction, "%s", local[leaf]->name);
			KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
		}
	}

	{
		static const u8 bits[] = { 0, 31, 63 };
		size_t bit;

		for (bit = 0; bit < ARRAY_SIZE(bits); bit++) {
			u32 instruction = (tbnz->pattern & tbnz->mask) |
				(2U << 5) | 5U |
				(((u32)(bits[bit] & 0x1fU)) << 19) |
				(bits[bit] & 0x20U ? BIT(31) : 0);
			unsigned long code;
			struct pt_regs regs = {};
			struct orlix_tcti_result result;

			code = orlix_tcti_base_conditional_map_branch(test,
								     instruction);
			orlix_tcti_base_conditional_seed(&regs, code, 0);
			regs.regs[5] = BIT_ULL(bits[bit]);
			result = orlix_tcti_resume_user(current, &regs, current->mm);
			KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
			KUNIT_EXPECT_EQ(test, BCD_SVC_TAKEN, result.instruction);
			KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
		}
		for (bit = 0; bit < ARRAY_SIZE(bits); bit++) {
			u32 instruction = (tbz->pattern & tbz->mask) |
				(2U << 5) | 5U |
				(((u32)(bits[bit] & 0x1fU)) << 19) |
				(bits[bit] & 0x20U ? BIT(31) : 0);
			unsigned long code;
			struct pt_regs regs = {};
			struct orlix_tcti_result result;

			code = orlix_tcti_base_conditional_map_branch(test,
								     instruction);
			orlix_tcti_base_conditional_seed(&regs, code, 0);
			regs.regs[5] = 0;
			result = orlix_tcti_resume_user(current, &regs, current->mm);
			KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
			KUNIT_EXPECT_EQ(test, BCD_SVC_TAKEN, result.instruction);
			KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
		}
	}
}

static void orlix_tcti_base_conditional_csel_aliases_and_w_upper(struct kunit *test)
{
	static const char *const names[] = {
		"CSEL_32_condsel", "CSINC_32_condsel", "CSINV_32_condsel",
		"CSNEG_32_condsel", "CSEL_64_condsel", "CSINC_64_condsel",
		"CSINV_64_condsel", "CSNEG_64_condsel",
	};
	size_t index;

	for (index = 0; index < ARRAY_SIZE(names); index++) {
		const struct orlix_tcti_test_conditional_source *source =
			orlix_tcti_test_conditional_name(names[index]);
		u32 true_insn;
		u32 false_insn;
		unsigned long code;
		struct pt_regs regs = {};
		struct orlix_tcti_result result;
		u8 cond;

		KUNIT_ASSERT_NOT_NULL(test, source);
		true_insn = orlix_tcti_base_conditional_legal_instruction(source);
		code = orlix_tcti_base_conditional_map_data(test, true_insn);
		orlix_tcti_base_conditional_seed(&regs, code, PSR_Z_BIT);
		regs.regs[1] = 0xa1a1a1a1b2b2b2b2ULL;
		regs.regs[2] = 0xc3c3c3c3d4d4d4d4ULL;
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason,
				    "%s true", source->name);
		if (strstr(source->name, "32"))
			KUNIT_EXPECT_EQ_MSG(test, 0xb2b2b2b2ULL, regs.regs[0],
					    "%s true W", source->name);
		else
			KUNIT_EXPECT_EQ_MSG(test, 0xa1a1a1a1b2b2b2b2ULL, regs.regs[0],
					    "%s true X", source->name);
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));

		false_insn = (true_insn & ~0xf000U) | (1U << 12);
		code = orlix_tcti_base_conditional_map_data(test, false_insn);
		orlix_tcti_base_conditional_seed(&regs, code, PSR_Z_BIT);
		regs.regs[1] = 0xa1a1a1a1b2b2b2b2ULL;
		regs.regs[2] = 0xc3c3c3c3d4d4d4d4ULL;
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason,
				    "%s false", source->name);
		if (!strcmp(source->operation, "CSEL") && strstr(source->name, "32"))
			KUNIT_EXPECT_EQ(test, 0xd4d4d4d4ULL, regs.regs[0]);
		else if (!strcmp(source->operation, "CSEL"))
			KUNIT_EXPECT_EQ(test, 0xc3c3c3c3d4d4d4d4ULL, regs.regs[0]);
		else if (!strcmp(source->operation, "CSINC") &&
			 strstr(source->name, "32"))
			KUNIT_EXPECT_EQ(test, 0xd4d4d4d5ULL, regs.regs[0]);
		else if (!strcmp(source->operation, "CSINC"))
			KUNIT_EXPECT_EQ(test, 0xc3c3c3c3d4d4d4d5ULL, regs.regs[0]);
		else if (!strcmp(source->operation, "CSINV") &&
			 strstr(source->name, "32"))
			KUNIT_EXPECT_EQ(test, (u64)(u32)~0xd4d4d4d4U, regs.regs[0]);
		else if (!strcmp(source->operation, "CSINV"))
			KUNIT_EXPECT_EQ(test, ~0xc3c3c3c3d4d4d4d4ULL, regs.regs[0]);
		else if (!strcmp(source->operation, "CSNEG") &&
			 strstr(source->name, "32"))
			KUNIT_EXPECT_EQ(test, (u64)(u32)(-(u32)0xd4d4d4d4U),
					regs.regs[0]);
		else if (!strcmp(source->operation, "CSNEG"))
			KUNIT_EXPECT_EQ(test, 0ULL - 0xc3c3c3c3d4d4d4d4ULL,
					regs.regs[0]);
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));

		for (cond = 0; cond < 16 && !strcmp(source->operation, "CSEL");
		     cond++) {
			u32 insn = (source->pattern & source->mask) | (1U << 5) |
				(2U << 16) | ((u32)cond << 12);

			code = orlix_tcti_base_conditional_map_data(test, insn);
			orlix_tcti_base_conditional_seed(&regs, code, PSR_Z_BIT);
			regs.regs[1] = 0x1111;
			regs.regs[2] = 0x2222;
			result = orlix_tcti_resume_user(current, &regs, current->mm);
			KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
			KUNIT_EXPECT_EQ(test,
				orlix_tcti_base_conditional_cond_passed(cond,
					PSR_Z_BIT) ? 0x1111ULL : 0x2222ULL,
				regs.regs[0]);
			KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
		}
	}
}

static u64 orlix_tcti_base_conditional_true_nzcv(
	const struct orlix_tcti_test_conditional_source *source)
{
	if (!strncmp(source->operation, "CCMN", 4))
		return PSR_N_BIT | PSR_V_BIT;
	return PSR_C_BIT | PSR_V_BIT;
}

static void orlix_tcti_base_conditional_seed_true_compare(
	struct pt_regs *regs, unsigned long code,
	const struct orlix_tcti_test_conditional_source *source)
{
	orlix_tcti_base_conditional_seed(regs, code, PSR_Z_BIT);
	if (strstr(source->name, "64")) {
		if (!strncmp(source->operation, "CCMN", 4))
			regs->regs[1] = 0x7fffffffffffffffULL;
		else
			regs->regs[1] = BIT_ULL(63);
	} else if (!strncmp(source->operation, "CCMN", 4)) {
		regs->regs[1] = 0x7fffffffULL;
	} else {
		regs->regs[1] = 0x80000000ULL;
	}
	regs->regs[2] = 1;
}

static void orlix_tcti_base_conditional_ccmp_true_false_nzcv(struct kunit *test)
{
	static const char *const names[] = {
		"CCMP_32_condcmp_reg", "CCMP_64_condcmp_reg",
		"CCMN_32_condcmp_reg", "CCMN_64_condcmp_reg",
		"CCMN_32_condcmp_imm", "CCMN_64_condcmp_imm",
		"CCMP_32_condcmp_imm", "CCMP_64_condcmp_imm",
	};
	size_t index;

	for (index = 0; index < ARRAY_SIZE(names); index++) {
		const struct orlix_tcti_test_conditional_source *source =
			orlix_tcti_test_conditional_name(names[index]);
		u32 insn;
		unsigned long code;
		struct pt_regs regs = {};
		struct orlix_tcti_result result;
		u64 before_flags;
		u32 rm_field;

		KUNIT_ASSERT_NOT_NULL(test, source);
		insn = (source->pattern & source->mask) | (1U << 5) |
			(2U << 16) | 0xaU;
		code = orlix_tcti_base_conditional_map_data(test, insn);
		orlix_tcti_base_conditional_seed(&regs, code, 0);
		regs.regs[1] = 1;
		regs.regs[2] = 1;
		before_flags = regs.pstate & BCD_NZCV;
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason,
				    "%s false", source->name);
		KUNIT_EXPECT_EQ_MSG(test, PSR_N_BIT | PSR_C_BIT,
				    regs.pstate & BCD_NZCV, "%s false nzcv",
				    source->name);
		KUNIT_EXPECT_NE(test, before_flags, regs.pstate & BCD_NZCV);
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));

		rm_field = strstr(source->operation, "imm") ? 1U : 2U;
		insn = (source->pattern & source->mask) | (1U << 5) |
			(rm_field << 16);
		code = orlix_tcti_base_conditional_map_data(test, insn);
		orlix_tcti_base_conditional_seed_true_compare(&regs, code, source);
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason,
				    "%s true", source->name);
		KUNIT_EXPECT_EQ_MSG(test,
				    orlix_tcti_base_conditional_true_nzcv(source),
				    regs.pstate & BCD_NZCV, "%s true nzcv",
				    source->name);
		orlix_tcti_base_conditional_seed_true_compare(&regs, code, source);
		orlix_tcti_base_conditional_capture_run(test, source,
			ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FLAGS, &regs, code,
			false);
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
	}
}

static void orlix_tcti_base_conditional_simd_and_flags_unchanged(struct kunit *test)
{
	const struct orlix_tcti_test_conditional_source *source =
		orlix_tcti_test_conditional_name("CSEL_32_condsel");
	u32 insn;
	unsigned long code;
	struct pt_regs regs = {};
	struct orlix_tcti_result result;
	u64 simd0;
	u64 simd1;
	u64 flags;

	KUNIT_ASSERT_NOT_NULL(test, source);
	insn = orlix_tcti_base_conditional_legal_instruction(source);
	code = orlix_tcti_base_conditional_map_data(test, insn);
	orlix_tcti_base_conditional_seed(&regs, code, PSR_Z_BIT);
	simd0 = current->thread.user_simd[0];
	simd1 = current->thread.user_simd[1];
	flags = regs.pstate & BCD_NZCV;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, simd0, current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, simd1, current->thread.user_simd[1]);
	KUNIT_EXPECT_EQ(test, flags, regs.pstate & BCD_NZCV);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
}

static void orlix_tcti_base_conditional_non_el0_rejected(struct kunit *test)
{
	size_t index;
	size_t seen = 0;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++) {
		const struct orlix_tcti_test_conditional_source *source =
			&orlix_tcti_test_sources[index];
		u32 instruction;
		struct orlix_tcti_decoded_instruction decoded;
		struct pt_regs regs = {};
		struct pt_regs before;
		struct orlix_tcti_result result;
		unsigned long code;

		if (!orlix_tcti_test_is_base_conditional(source) ||
		    orlix_tcti_test_is_el0_conditional(source))
			continue;
		seen++;
		instruction = source->pattern;
		decoded = orlix_tcti_decode_aarch64(instruction);
		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
				    decoded.decode_class, "%s class", source->name);
		KUNIT_EXPECT_EQ_MSG(test, source->ordinal, decoded.source_ordinal,
				    "%s ordinal", source->name);
		code = orlix_tcti_base_conditional_map_data(test, instruction);
		orlix_tcti_base_conditional_seed(&regs, code, BCD_NZCV);
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
	KUNIT_EXPECT_EQ(test, BCD_NON_EL0_COUNT, seen);
}

static void orlix_tcti_base_conditional_reserved_encodings(struct kunit *test)
{
	const struct orlix_tcti_test_conditional_source *bc =
		orlix_tcti_test_conditional_name("BC_only_condbranch");
	struct orlix_tcti_decoded_instruction decoded;

	KUNIT_ASSERT_NOT_NULL(test, bc);
	decoded = orlix_tcti_decode_aarch64(0x54000010U);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED, decoded.decode_class);
	KUNIT_EXPECT_EQ(test, bc->ordinal, decoded.source_ordinal);
	decoded = orlix_tcti_decode_aarch64(0x74004000U);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED, decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 0U, decoded.source_ordinal);
}

static struct kunit_case orlix_tcti_base_conditional_source_bound_cases[] = {
	KUNIT_CASE(orlix_tcti_base_conditional_decodes_exact_source_cohort),
	KUNIT_CASE(orlix_tcti_base_conditional_binds_pinned_ddi0602_semantics),
	KUNIT_CASE(orlix_tcti_base_conditional_production_resume),
	KUNIT_CASE(orlix_tcti_base_conditional_taken_and_not_taken),
	KUNIT_CASE(orlix_tcti_base_conditional_csel_aliases_and_w_upper),
	KUNIT_CASE(orlix_tcti_base_conditional_ccmp_true_false_nzcv),
	KUNIT_CASE(orlix_tcti_base_conditional_simd_and_flags_unchanged),
	KUNIT_CASE(orlix_tcti_base_conditional_non_el0_rejected),
	KUNIT_CASE(orlix_tcti_base_conditional_reserved_encodings),
	{}
};

static struct kunit_suite orlix_tcti_base_conditional_source_bound_suite = {
	.name = "orlix-tcti-base-conditional-source-bound",
	.init = orlix_tcti_base_conditional_test_init,
	.exit = orlix_tcti_base_conditional_test_exit,
	.test_cases = orlix_tcti_base_conditional_source_bound_cases,
};

kunit_test_suite(orlix_tcti_base_conditional_source_bound_suite);
