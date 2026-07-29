// SPDX-License-Identifier: GPL-2.0-only
/*
 * Source-bound production-path coverage for base branch and exception-control
 * leaves.
 *
 * The rows below intentionally cover only the pinned AARCHMRS leaves whose
 * decode class and EL0 execution semantics are implemented by the production
 * OrlixTCTI decoder and executor. Pointer-authenticated, guarded-control-stack,
 * and other feature-conditioned branch variants remain outside this table
 * until their distinct architecture-defined semantics have owning evidence.
 */
#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/elf.h>
#include <asm/orlix_tcti.h>
#include <kunit/test.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/syscalls.h>
#include <linux/utsname.h>

#include "../decode_aarch64.h"
#include "../switch_debug.h"
#include "orlix_tcti_test_suites.h"
#include "orlix_tcti_source_leaf_rejection_catalog.h"
#include "orlix_tcti_native_observation.h"
#include "target_feature_applicability_artifact.h"
#include "target_proof_registry.h"
#include "target_execution_slice_map.h"

#define BCS_SVC_NOT_TAKEN 0xd4000021U
#define BCS_SVC_TAKEN 0xd4000041U

enum bcs_kind {
	BCS_SVC,
	BCS_BRK,
	BCS_HLT,
	BCS_B_COND,
	BCS_B,
	BCS_BL,
	BCS_BR,
	BCS_BLR,
	BCS_RET,
	BCS_CBZ,
	BCS_CBNZ,
	BCS_TBZ,
	BCS_TBNZ,
};

struct bcs_leaf {
	u16 ordinal;
	const char *name;
	const char *operation;
	u32 mask;
	u32 pattern;
	enum bcs_kind kind;
	bool wide;
};

struct bcs_semantics_gap {
	u32 ordinal;
	const char *name;
	const char *operation_locator;
	u64 source_offset;
	u64 source_length;
	const char *source_sha256;
};

#define ORLIX_TCTI_A64_SEMANTIC_PROVENANCE_SOURCE(...)
#define ORLIX_TCTI_A64_DDI0602_PROVENANCE_ROW(...)
#define ORLIX_TCTI_A64_OFFICIAL_SEMANTICS_NOT_SPECIFIED_ROW(ordinal, name, \
		locator, offset, length, sha256) \
	{ ordinal, name, locator, offset, length, sha256 },
static const struct bcs_semantics_gap bcs_semantics_gaps[] = {
#include "../isa/generations/current/target_asl_availability.def"
};
#undef ORLIX_TCTI_A64_OFFICIAL_SEMANTICS_NOT_SPECIFIED_ROW
#undef ORLIX_TCTI_A64_DDI0602_PROVENANCE_ROW
#undef ORLIX_TCTI_A64_SEMANTIC_PROVENANCE_SOURCE

/* Pinned source_manifest.def ordinals and source encodings. */
static const struct bcs_leaf bcs_leaves[] = {
	{ 2227, "SVC_EX_exception", "SVC", 0xffe0001fU, 0xd4000001U,
	  BCS_SVC, false },
	{ 2230, "BRK_EX_exception", "BRK", 0xffe0001fU, 0xd4200000U,
	  BCS_BRK, false },
	{ 2231, "HLT_EX_exception", "HLT", 0xffe0001fU, 0xd4400000U,
	  BCS_HLT, false },
	{ 2211, "B_only_condbranch", "B_cond", 0xff000010U, 0x54000000U,
	  BCS_B_COND, false },
	{ 2288, "BR_64_branch_reg", "BR", 0xfffffc1fU, 0xd61f0000U,
	  BCS_BR, true },
	{ 2291, "BLR_64_branch_reg", "BLR", 0xfffffc1fU, 0xd63f0000U,
	  BCS_BLR, true },
	{ 2294, "RET_64R_branch_reg", "RET", 0xfffffc1fU, 0xd65f0000U,
	  BCS_RET, true },
	{ 2312, "B_only_branch_imm", "B_uncond", 0xfc000000U, 0x14000000U,
	  BCS_B, false },
	{ 2313, "BL_only_branch_imm", "BL", 0xfc000000U, 0x94000000U,
	  BCS_BL, false },
	{ 2314, "CBZ_32_compbranch", "CBZ", 0xff000000U, 0x34000000U,
	  BCS_CBZ, false },
	{ 2315, "CBNZ_32_compbranch", "CBNZ", 0xff000000U, 0x35000000U,
	  BCS_CBNZ, false },
	{ 2316, "CBZ_64_compbranch", "CBZ", 0xff000000U, 0xb4000000U,
	  BCS_CBZ, true },
	{ 2317, "CBNZ_64_compbranch", "CBNZ", 0xff000000U, 0xb5000000U,
	  BCS_CBNZ, true },
	{ 2342, "TBZ_only_testbranch", "TBZ", 0x7f000000U, 0x36000000U,
	  BCS_TBZ, false },
	{ 2343, "TBNZ_only_testbranch", "TBNZ", 0x7f000000U, 0x37000000U,
	  BCS_TBNZ, false },
};

static enum orlix_tcti_decode_class bcs_decode_class(const struct bcs_leaf *leaf)
{
	switch (leaf->kind) {
	case BCS_SVC:
		return ORLIX_TCTI_DECODE_SVC;
	case BCS_BRK:
		return ORLIX_TCTI_DECODE_BRK;
	case BCS_HLT:
		return ORLIX_TCTI_DECODE_HLT;
	case BCS_B_COND:
		return ORLIX_TCTI_DECODE_CONDITIONAL_BRANCH_IMMEDIATE;
	case BCS_B:
	case BCS_BL:
		return ORLIX_TCTI_DECODE_UNCONDITIONAL_BRANCH_IMMEDIATE;
	case BCS_BR:
	case BCS_BLR:
	case BCS_RET:
		return ORLIX_TCTI_DECODE_UNCONDITIONAL_BRANCH_REGISTER;
	case BCS_CBZ:
	case BCS_CBNZ:
		return ORLIX_TCTI_DECODE_COMPARE_BRANCH_IMMEDIATE;
	case BCS_TBZ:
	case BCS_TBNZ:
		return ORLIX_TCTI_DECODE_TEST_BRANCH_IMMEDIATE;
	}

	return ORLIX_TCTI_DECODE_UNSUPPORTED;
}

static void bcs_issue_132_exact_source_cohort(struct kunit *test)
{
	static const u32 expected_ordinals[] = {
		2166U, 2227U, 2228U, 2229U, 2230U,
		2231U, 2232U, 2233U, 2234U, 2235U,
	};
	const struct orlix_tcti_execution_slice_map *map =
		orlix_tcti_execution_slice_map_canonical();
	const struct orlix_tcti_source_leaf_manifest_row *tenter;
	const struct bcs_semantics_gap *tenter_semantics = NULL;
	const struct orlix_tcti_target_feature_applicability_artifact *applicability;
	const struct orlix_tcti_target_feature_applicability_row *tenter_feature;
	size_t family_index;
	size_t expected_index = 0;
	size_t gap_index;
	bool found_gap = false;

	for (family_index = 0; family_index < map->counts.family_count;
	     family_index++)
		if (map->families[family_index].issue_id == 132U)
			break;
	KUNIT_ASSERT_LT(test, family_index, map->counts.family_count);
	KUNIT_ASSERT_STREQ(test, "BASE_EXCEPTIONS",
			   map->families[family_index].stable_id);
	KUNIT_ASSERT_EQ(test, ARRAY_SIZE(expected_ordinals),
			map->families[family_index].declared_member_count);

	for (gap_index = 0; gap_index < map->counts.leaf_count; gap_index++) {
		const struct orlix_tcti_execution_slice_member *member =
			&map->members[gap_index];

		if (member->family_index != family_index)
			continue;
		KUNIT_ASSERT_LT(test, expected_index,
				ARRAY_SIZE(expected_ordinals));
		KUNIT_EXPECT_EQ(test, expected_ordinals[expected_index],
				member->ordinal);
		expected_index++;
	}
	KUNIT_EXPECT_EQ(test, ARRAY_SIZE(expected_ordinals), expected_index);

	for (gap_index = 0; gap_index < ARRAY_SIZE(bcs_semantics_gaps);
	     gap_index++)
		if (bcs_semantics_gaps[gap_index].ordinal == 2235U &&
		    !strcmp(bcs_semantics_gaps[gap_index].name,
			    "TENTER_te_exception")) {
			found_gap = true;
			tenter_semantics = &bcs_semantics_gaps[gap_index];
		}
	KUNIT_EXPECT_TRUE(test, found_gap);
	KUNIT_ASSERT_NOT_NULL(test, tenter_semantics);
	KUNIT_EXPECT_STREQ(test, "Instructions.json#operations/TENTER/operation",
			   tenter_semantics->operation_locator);
	KUNIT_EXPECT_EQ(test, 115113790ULL, tenter_semantics->source_offset);
	KUNIT_EXPECT_EQ(test, 16ULL, tenter_semantics->source_length);
	KUNIT_EXPECT_STREQ(test,
		"28fb16d9885379aa6e05267c659d8b7dab31e819051d85e1dbde8347bc2fdce8",
		tenter_semantics->source_sha256);

	tenter = orlix_tcti_source_leaf_manifest_row(2235U);
	KUNIT_ASSERT_NOT_NULL(test, tenter);
	KUNIT_EXPECT_STREQ(test, "TENTER_te_exception", tenter->name);
	KUNIT_EXPECT_STREQ(test, "TENTER", tenter->operation);

	applicability = orlix_tcti_target_feature_applicability_artifact();
	KUNIT_ASSERT_NOT_NULL(test, applicability);
	KUNIT_ASSERT_LT(test, 2235U, applicability->row_count);
	tenter_feature = &applicability->rows[2235U];
	KUNIT_EXPECT_EQ(test, 2235U, tenter_feature->ordinal);
	KUNIT_EXPECT_STREQ(test, "TENTER_te_exception", tenter_feature->name);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_TARGET_FEATURE_APPLICABLE,
			tenter_feature->status);
	KUNIT_EXPECT_STREQ(test,
		"54434e4401070000002d0700000017070000000c010000000101010000000101010000000101020000000c00000008464541545f544556",
		tenter_feature->condition_tcnd_hex);
	KUNIT_EXPECT_EQ(test, 0x5ec8d296a01919edULL,
			tenter_feature->formula_identity);
}

static u32 bcs_instruction(const struct bcs_leaf *leaf, bool taken)
{
	/* Both paths are valid, with the taken target at instruction index two. */
	u32 instruction = leaf->pattern;

	switch (leaf->kind) {
	case BCS_SVC:
	case BCS_BRK:
	case BCS_HLT:
		return instruction | (0x1234U << 5);
	case BCS_B_COND:
		return instruction | (2U << 5);
	case BCS_B:
	case BCS_BL:
		return instruction | 2U;
	case BCS_BR:
	case BCS_BLR:
	case BCS_RET:
		return instruction | (5U << 5);
	case BCS_CBZ:
	case BCS_CBNZ:
		return instruction | (2U << 5) | 5U;
	case BCS_TBZ:
	case BCS_TBNZ:
		return instruction | (2U << 5) | (3U << 19) | 5U;
	}

	return 0;
}

static unsigned long bcs_map_program(struct kunit *test, u32 instruction)
{
	u32 program[] = { instruction, BCS_SVC_NOT_TAKEN, BCS_SVC_TAKEN };
	unsigned long address;
	int ret;

	address = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(address));
	ret = orlix_tcti_write_user_data(current->mm, address, program,
				   sizeof(program));
	KUNIT_ASSERT_EQ(test, 0, ret);
	ret = sys_mprotect(address, PAGE_SIZE, PROT_READ | PROT_EXEC);
	KUNIT_ASSERT_EQ(test, 0, ret);
	return address;
}

static void bcs_seed_regs(struct pt_regs *regs, unsigned long address,
			  const struct bcs_leaf *leaf, bool taken)
{
	unsigned int index;

	memset(regs, 0, sizeof(*regs));
	for (index = 0; index < ARRAY_SIZE(regs->regs); index++)
		regs->regs[index] = 0x0102030405060708ULL + index;
	regs->pc = address;
	regs->pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_C_BIT | PSR_V_BIT;
	regs->syscallno = NO_SYSCALL;
	regs->regs[5] = address + (taken ? 2 * sizeof(u32) : sizeof(u32));

	if (leaf->kind == BCS_B_COND) {
		/* EQ has condition code zero, so Z selects the taken path. */
		if (taken)
			regs->pstate |= PSR_Z_BIT;
		return;
	}
	if (leaf->kind == BCS_CBZ)
		regs->regs[5] = taken ? 0 : 1;
	else if (leaf->kind == BCS_CBNZ)
		regs->regs[5] = taken ? 1 : 0;
	else if (leaf->kind == BCS_TBZ)
		regs->regs[5] = taken ? 0 : BIT_ULL(3);
	else if (leaf->kind == BCS_TBNZ)
		regs->regs[5] = taken ? BIT_ULL(3) : 0;
}

static void bcs_expect_register_effects(struct kunit *test,
					const struct bcs_leaf *leaf,
					const struct pt_regs *before,
					const struct pt_regs *regs,
					unsigned long address)
{
	unsigned long expected[ARRAY_SIZE(regs->regs)];

	memcpy(expected, before->regs, sizeof(expected));
	if (leaf->kind == BCS_BL || leaf->kind == BCS_BLR)
		expected[30] = address + sizeof(u32);
	KUNIT_EXPECT_MEMEQ(test, expected, regs->regs, sizeof(regs->regs));
	KUNIT_EXPECT_EQ(test, before->pstate, regs->pstate);
}

static bool bcs_can_fall_through(const struct bcs_leaf *leaf)
{
	return leaf->kind == BCS_B_COND || leaf->kind == BCS_CBZ ||
	       leaf->kind == BCS_CBNZ || leaf->kind == BCS_TBZ ||
	       leaf->kind == BCS_TBNZ;
}

static bool bcs_is_exception_control(const struct bcs_leaf *leaf)
{
	return leaf->kind == BCS_SVC || leaf->kind == BCS_BRK ||
	       leaf->kind == BCS_HLT;
}

static const char *bcs_issue_132_proof_id(const struct bcs_leaf *leaf)
{
	switch (leaf->kind) {
	case BCS_SVC:
		return "kunit:branch-control-svc";
	case BCS_BRK:
		return "kunit:branch-control-brk";
	case BCS_HLT:
		return "kunit:branch-control-hlt";
	default:
		return NULL;
	}
}

static void bcs_capture_fp_simd(struct orlix_tcti_native_fp_simd_state *state)
{
	memset(state, 0, sizeof(*state));
	memcpy(state->v, current->thread.user_simd, sizeof(state->v));
	state->fpcr = current->thread.user_fpcr;
	state->fpsr = current->thread.user_fpsr;
	state->valid = true;
}

static void bcs_capture_sve(struct orlix_tcti_native_sve_state *state)
{
	const struct orlix_tcti_sve_state *sve = &current->thread.user_sve;

	*state = (struct orlix_tcti_native_sve_state) {
		.vl_bytes = sve->vl_bytes,
		.z = &sve->z[0][0],
		.p = &sve->p[0][0],
		.ffr = sve->ffr,
		.valid = sve->valid,
	};
}

static void bcs_ingest_observation(
	struct kunit *test, const char *proof_id, u32 ordinal,
	struct orlix_tcti_target_native_result_record *record, bool unavailable)
{
	const char *case_name =
		"bcs_issue_132_exceptions_emit_typed_observations";
	struct orlix_tcti_target_kunit_provenance_identity provenance;
	struct orlix_tcti_target_native_ingestion_selector selector = {};
	struct orlix_tcti_target_proof_ingestion_ledger *ledger;
	struct orlix_tcti_target_proof_ingestion_summary summary;
	const struct orlix_tcti_target_proof_registry_entry *entries;
	const struct orlix_tcti_target_proof_registry_entry *entry = NULL;
	const struct orlix_tcti_target_proof_binding *binding = NULL;
	enum orlix_tcti_target_proof_ingestion_error error;
	char kernel_identity[ORLIX_TCTI_TARGET_PROOF_BUILD_ID_MAX];
	size_t entry_count;
	size_t entry_index;
	size_t binding_index;

	entries = orlix_tcti_target_proof_registry_entries(&entry_count);
	KUNIT_ASSERT_NOT_NULL(test, entries);
	for (entry_index = 0; entry_index < entry_count; entry_index++)
		if (!strcmp(entries[entry_index].id, proof_id)) {
			entry = &entries[entry_index];
			break;
		}
	KUNIT_ASSERT_NOT_NULL(test, entry);
	for (binding_index = 0; binding_index < entry->binding_count;
	     binding_index++)
		if (entry->bindings[binding_index].source_ordinal == ordinal) {
			binding = &entry->bindings[binding_index];
			break;
		}
	KUNIT_ASSERT_NOT_NULL(test, binding);
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_target_kunit_provenance_identity(
			entry, case_name, &provenance));
	scnprintf(kernel_identity, sizeof(kernel_identity), "%s|%s|%s",
		 init_utsname()->release, init_utsname()->version,
		 init_utsname()->machine);
	selector = (struct orlix_tcti_target_native_ingestion_selector) {
		.proof_id = proof_id,
		.classification_mask = entry->classification_mask,
		.condition_tcnd_hex = binding->condition_tcnd_hex,
		.kunit_source = provenance.source,
		.kunit_source_sha256 = provenance.source_sha256,
		.kunit_build_source = provenance.build_source,
		.kunit_build_source_sha256 = provenance.build_source_sha256,
		.kunit_suite = provenance.suite,
		.kunit_case = provenance.case_name,
		.executing_kernel_identity = kernel_identity,
	};
	ledger = orlix_tcti_target_proof_ingestion_ledger_create(1);
	KUNIT_ASSERT_NOT_NULL(test, ledger);
	KUNIT_EXPECT_EQ(test, unavailable ? -1 : 0,
		orlix_tcti_target_proof_ingest_native(
			ledger, record, &selector, &error));
	KUNIT_EXPECT_EQ(test, unavailable ?
		ORLIX_TCTI_TARGET_PROOF_INGEST_NOT_APPLICABLE :
		ORLIX_TCTI_TARGET_PROOF_INGEST_OK, error);
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_target_proof_ingestion_summary(ledger, &summary));
	KUNIT_EXPECT_EQ(test, unavailable ? 0UL : 1UL, summary.accepted_records);
	KUNIT_EXPECT_EQ(test, unavailable ? 0UL : 1UL, summary.native_passed);
	KUNIT_EXPECT_EQ(test, unavailable ? 1UL : 0UL, summary.rejected);
	orlix_tcti_target_proof_ingestion_ledger_destroy(ledger);
}

static void bcs_emit_observation(
	struct kunit *test, const struct bcs_leaf *leaf,
	enum orlix_tcti_native_obligation obligation)
{
	struct orlix_tcti_native_observation_spec spec = {};
	struct orlix_tcti_native_observation *observation;
	struct orlix_tcti_target_native_result_record *record = NULL;
	struct orlix_tcti_native_fp_simd_state fp_simd;
	struct orlix_tcti_native_sve_state sve;
	struct orlix_tcti_native_sme_state sme = {
		.valid = true,
		.production_available = false,
	};
	struct orlix_tcti_native_memory_state memory;
	struct pt_regs regs;
	u32 instruction = bcs_instruction(leaf, true);
	u32 observed_instruction = 0;
	unsigned long mapped = bcs_map_program(test, instruction);
	const char *proof_id = bcs_issue_132_proof_id(leaf);

	bcs_seed_regs(&regs, mapped, leaf, true);
	spec.source_ordinal = leaf->ordinal;
	spec.obligation = obligation;
	spec.expected_decode_class = bcs_decode_class(leaf);
	spec.expected_decode_class_valid = true;
	spec.result.reason = leaf->kind == BCS_SVC ? ORLIX_TCTI_EXIT_SYSCALL :
		(leaf->kind == BCS_BRK ? ORLIX_TCTI_EXIT_BREAKPOINT :
		 ORLIX_TCTI_EXIT_UNDEFINED_INSTRUCTION);
	spec.result.status = leaf->kind == BCS_BRK ? 0x1234 : 0;
	spec.result.fault_access = ORLIX_TCTI_ACCESS_FETCH;
	spec.result.pc = mapped;
	spec.result.instruction = instruction;
	orlix_tcti_native_gpr_capture(&spec.gpr, &regs);
	if (obligation == ORLIX_TCTI_NATIVE_OBLIGATION_MEMORY) {
		spec.expected.memory.address = mapped;
		spec.expected.memory.size = sizeof(instruction);
		spec.expected.memory.bytes = (const u8 *)&instruction;
	} else if (obligation == ORLIX_TCTI_NATIVE_OBLIGATION_FP_SIMD) {
		bcs_capture_fp_simd(&spec.expected.fp_simd);
	} else if (obligation == ORLIX_TCTI_NATIVE_OBLIGATION_SVE) {
		bcs_capture_sve(&spec.expected.sve);
	} else if (obligation == ORLIX_TCTI_NATIVE_OBLIGATION_SME) {
		spec.expected.sme = sme;
	}
	observation = orlix_tcti_native_observation_create(&spec);
	KUNIT_ASSERT_NOT_NULL(test, observation);
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_native_observation_execute(
		observation, current, &regs, current->mm));
	if (obligation == ORLIX_TCTI_NATIVE_OBLIGATION_DECODE ||
	    obligation == ORLIX_TCTI_NATIVE_OBLIGATION_LEGAL_ENCODINGS ||
	    obligation == ORLIX_TCTI_NATIVE_OBLIGATION_REJECTED_ENCODINGS) {
		KUNIT_ASSERT_EQ(test, 0,
			orlix_tcti_native_observation_add_encoding_domain(observation));
	} else if (obligation == ORLIX_TCTI_NATIVE_OBLIGATION_MEMORY) {
		KUNIT_ASSERT_EQ(test, 0, orlix_tcti_read_user_data(
			current->mm, mapped, &observed_instruction,
			sizeof(observed_instruction)));
		memory = (struct orlix_tcti_native_memory_state) {
			.address = mapped,
			.size = sizeof(observed_instruction),
			.bytes = (const u8 *)&observed_instruction,
		};
		KUNIT_ASSERT_EQ(test, 0,
			orlix_tcti_native_observation_add_memory(observation, &memory));
	} else if (obligation == ORLIX_TCTI_NATIVE_OBLIGATION_FP_SIMD) {
		bcs_capture_fp_simd(&fp_simd);
		KUNIT_ASSERT_EQ(test, 0,
			orlix_tcti_native_observation_add_fp_simd(observation,
				&fp_simd));
	} else if (obligation == ORLIX_TCTI_NATIVE_OBLIGATION_SVE) {
		bcs_capture_sve(&sve);
		KUNIT_ASSERT_EQ(test, 0,
			orlix_tcti_native_observation_add_sve(observation, &sve));
	} else if (obligation == ORLIX_TCTI_NATIVE_OBLIGATION_SME) {
		KUNIT_ASSERT_EQ(test, 0,
			orlix_tcti_native_observation_add_sme(observation, &sme));
	}
	KUNIT_ASSERT_EQ(test, obligation == ORLIX_TCTI_NATIVE_OBLIGATION_SME ?
		-EOPNOTSUPP : 0,
		orlix_tcti_native_observation_compare(observation));
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_native_observation_export(observation, &record));
	KUNIT_ASSERT_NOT_NULL(test, record);
	KUNIT_ASSERT_NOT_NULL(test, proof_id);
	bcs_ingest_observation(test, proof_id, leaf->ordinal, record,
		obligation == ORLIX_TCTI_NATIVE_OBLIGATION_SME);
	orlix_tcti_target_native_result_record_destroy(record);
	orlix_tcti_native_observation_destroy(observation);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void bcs_issue_132_exceptions_emit_typed_observations(
	struct kunit *test)
{
	static const enum orlix_tcti_native_obligation obligations[] = {
		ORLIX_TCTI_NATIVE_OBLIGATION_DECODE,
		ORLIX_TCTI_NATIVE_OBLIGATION_LEGAL_ENCODINGS,
		ORLIX_TCTI_NATIVE_OBLIGATION_REJECTED_ENCODINGS,
		ORLIX_TCTI_NATIVE_OBLIGATION_GPR,
		ORLIX_TCTI_NATIVE_OBLIGATION_FLAGS,
		ORLIX_TCTI_NATIVE_OBLIGATION_RESULT,
		ORLIX_TCTI_NATIVE_OBLIGATION_MEMORY,
		ORLIX_TCTI_NATIVE_OBLIGATION_FAULT,
		ORLIX_TCTI_NATIVE_OBLIGATION_FP_SIMD,
		ORLIX_TCTI_NATIVE_OBLIGATION_SVE,
		ORLIX_TCTI_NATIVE_OBLIGATION_SME,
	};
	struct orlix_tcti_sve_state *saved_sve;
	unsigned long *saved_simd;
	unsigned long saved_fpcr = current->thread.user_fpcr;
	unsigned long saved_fpsr = current->thread.user_fpsr;
	unsigned long saved_simd_valid = current->thread.user_simd_valid;
	size_t leaf_index;
	size_t obligation_index;

	KUNIT_ASSERT_EQ(test, 0UL, (unsigned long)ELF_HWCAP);
	KUNIT_ASSERT_EQ(test, 0UL, (unsigned long)ELF_HWCAP2);
	saved_sve = kmemdup(&current->thread.user_sve,
			    sizeof(current->thread.user_sve), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, saved_sve);
	saved_simd = kmemdup(current->thread.user_simd,
			     sizeof(current->thread.user_simd), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, saved_simd);
	for (leaf_index = 0; leaf_index < ARRAY_SIZE(current->thread.user_simd);
	     leaf_index++)
		current->thread.user_simd[leaf_index] =
			0x5a5a000000000000ULL ^ leaf_index;
	current->thread.user_simd_valid = 1;
	current->thread.user_fpcr = BIT(22) | BIT(24);
	current->thread.user_fpsr = BIT(27) | BIT(4);
	memset(&current->thread.user_sve, 0, sizeof(current->thread.user_sve));
	current->thread.user_sve.vl_bytes = ORLIX_TCTI_SVE_MIN_VL_BYTES;
	current->thread.user_sve.valid = true;
	current->thread.user_sve.z[31][ORLIX_TCTI_SVE_MIN_VL_BYTES - 1] = 0xa5;
	current->thread.user_sve.p[15][1] = 0x5a;
	current->thread.user_sve.ffr[1] = 0x3c;
	for (leaf_index = 0; leaf_index < 3; leaf_index++)
		for (obligation_index = 0;
		     obligation_index < ARRAY_SIZE(obligations); obligation_index++) {
			if (bcs_leaves[leaf_index].kind == BCS_SVC &&
			    obligations[obligation_index] ==
				ORLIX_TCTI_NATIVE_OBLIGATION_FAULT)
				continue;
			bcs_emit_observation(test, &bcs_leaves[leaf_index],
				obligations[obligation_index]);
		}
	memcpy(&current->thread.user_sve, saved_sve,
	       sizeof(current->thread.user_sve));
	memcpy(current->thread.user_simd, saved_simd,
	       sizeof(current->thread.user_simd));
	current->thread.user_simd_valid = saved_simd_valid;
	current->thread.user_fpcr = saved_fpcr;
	current->thread.user_fpsr = saved_fpsr;
	kfree(saved_simd);
	kfree(saved_sve);
}

static void bcs_expect_exception_control(struct kunit *test,
					 const struct bcs_leaf *leaf,
					 const struct pt_regs *before,
					 const struct pt_regs *regs,
					 const struct orlix_tcti_result *result,
					 u32 instruction)
{
	enum orlix_tcti_exit_reason expected_reason;
	long expected_status;

	switch (leaf->kind) {
	case BCS_SVC:
		expected_reason = ORLIX_TCTI_EXIT_SYSCALL;
		expected_status = 0;
		break;
	case BCS_BRK:
		expected_reason = ORLIX_TCTI_EXIT_BREAKPOINT;
		expected_status = 0x1234;
		break;
	case BCS_HLT:
		expected_reason = ORLIX_TCTI_EXIT_UNDEFINED_INSTRUCTION;
		expected_status = (instruction >> 5) & 0xffffU;
		break;
	default:
		return;
	}

	KUNIT_EXPECT_EQ(test, expected_reason, result->reason);
	KUNIT_EXPECT_EQ(test, expected_status, result->status);
	KUNIT_EXPECT_EQ(test, before->pc, result->pc);
	KUNIT_EXPECT_EQ(test, instruction, result->instruction);
	KUNIT_EXPECT_MEMEQ(test, before, regs, sizeof(*regs));
	KUNIT_EXPECT_EQ(test, before->pstate, regs->pstate);
	KUNIT_EXPECT_EQ(test, before->pc, regs->pc);
}

static void bcs_source_decode(struct kunit *test)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(bcs_leaves); index++) {
		const struct bcs_leaf *leaf = &bcs_leaves[index];
		struct orlix_tcti_decoded_instruction decoded =
			orlix_tcti_decode_aarch64(bcs_instruction(leaf, true));

		KUNIT_EXPECT_TRUE_MSG(test, leaf->pattern ==
				      (leaf->pattern & leaf->mask), "%s", leaf->name);
		KUNIT_ASSERT_EQ_MSG(test, bcs_decode_class(leaf),
				    decoded.decode_class, "%s ordinal %u", leaf->name,
				    leaf->ordinal);
		if (leaf->kind == BCS_CBZ || leaf->kind == BCS_CBNZ)
			KUNIT_EXPECT_EQ(test, leaf->wide, decoded.is_64bit);
	}
}

static void bcs_production_resume(struct kunit *test)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(bcs_leaves); index++) {
		const struct bcs_leaf *leaf = &bcs_leaves[index];
		unsigned int taken;

		for (taken = 0; taken < 2; taken++) {
			struct pt_regs regs, before;
			struct orlix_tcti_result result;
			unsigned long address;
			u32 instruction;
			u32 expected_svc;

			if (!taken && !bcs_can_fall_through(leaf))
				continue;
			instruction = bcs_instruction(leaf, taken);
			address = bcs_map_program(test, instruction);
			bcs_seed_regs(&regs, address, leaf, taken);
			before = regs;
			result = orlix_tcti_resume_user(current, &regs, current->mm);
			if (bcs_is_exception_control(leaf)) {
				bcs_expect_exception_control(test, leaf, &before, &regs,
							     &result, instruction);
				KUNIT_EXPECT_EQ(test, 0, vm_munmap(address, PAGE_SIZE));
				continue;
			}
			expected_svc = taken ? BCS_SVC_TAKEN : BCS_SVC_NOT_TAKEN;

			KUNIT_ASSERT_EQ_MSG(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason,
				    "%s ordinal %u taken=%u", leaf->name, leaf->ordinal,
				    taken);
			KUNIT_EXPECT_EQ(test, expected_svc, result.instruction);
			KUNIT_EXPECT_EQ(test, address +
					(taken ? 3 : 2) * sizeof(u32), regs.pc);
			KUNIT_EXPECT_EQ(test, before.pstate, regs.pstate);
			if (leaf->kind == BCS_BL || leaf->kind == BCS_BLR)
				KUNIT_EXPECT_EQ(test, address + sizeof(u32),
						regs.regs[30]);
			else
				KUNIT_EXPECT_EQ(test, before.regs[30], regs.regs[30]);
			bcs_expect_register_effects(test, leaf, &before, &regs, address);
			KUNIT_EXPECT_EQ(test, 0, vm_munmap(address, PAGE_SIZE));
		}
	}
}

static void bcs_x31_semantics_production(struct kunit *test)
{
	static const struct {
		u32 instruction;
		bool taken;
	} cases[] = {
		/* CBZ wzr always branches. CBNZ wzr always falls through. */
		{ 0x3400005fU, true },
		{ 0x3500005fU, false },
		/* TBZ xzr, #3 branches. TBNZ xzr, #3 falls through. */
		{ 0x3618005fU, true },
		{ 0x3718005fU, false },
	};
	size_t index;

	for (index = 0; index < ARRAY_SIZE(cases); index++) {
		struct pt_regs regs = {};
		struct pt_regs before;
		struct orlix_tcti_result result;
		unsigned long address;

		address = bcs_map_program(test, cases[index].instruction);
		bcs_seed_regs(&regs, address, &bcs_leaves[0], cases[index].taken);
		before = regs;
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
		KUNIT_EXPECT_EQ(test, cases[index].taken ? BCS_SVC_TAKEN :
				BCS_SVC_NOT_TAKEN, result.instruction);
		KUNIT_EXPECT_EQ(test, address +
				(cases[index].taken ? 3 : 2) * sizeof(u32), regs.pc);
		KUNIT_EXPECT_MEMEQ(test, before.regs, regs.regs, sizeof(regs.regs));
		KUNIT_EXPECT_EQ(test, before.pstate, regs.pstate);
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(address, PAGE_SIZE));
	}
}

static void bcs_register_branch_unaligned_target_production(struct kunit *test)
{
	static const struct {
		enum bcs_kind kind;
		u32 instruction;
	} cases[] = {
		{ BCS_BR, 0xd61f00a0U },
		{ BCS_BLR, 0xd63f00a0U },
		{ BCS_RET, 0xd65f00a0U },
	};
	size_t index;

	for (index = 0; index < ARRAY_SIZE(cases); index++) {
		struct pt_regs regs = {};
		struct pt_regs debug_regs;
		struct pt_regs before;
		struct orlix_tcti_result result;
		struct orlix_tcti_result debug_result;
		unsigned long address;
		unsigned long target;
		unsigned long expected[ARRAY_SIZE(regs.regs)];

		address = bcs_map_program(test, cases[index].instruction);
		bcs_seed_regs(&regs, address, &bcs_leaves[0], true);
		target = address + 1;
		regs.regs[5] = target;
		before = regs;
		debug_regs = regs;
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		debug_result = orlix_tcti_switch_debug_resume_user(current, &debug_regs,
							     current->mm);

		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_ALIGNMENT_FAULT, result.reason,
				    "kind=%u", cases[index].kind);
		KUNIT_EXPECT_EQ(test, -EFAULT, result.status);
		KUNIT_EXPECT_EQ(test, target, result.fault_address);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_ACCESS_FETCH, result.fault_access);
		KUNIT_EXPECT_EQ(test, target, result.pc);
		KUNIT_EXPECT_EQ(test, target, regs.pc);
		KUNIT_EXPECT_EQ(test, 0U, result.instruction);
		KUNIT_EXPECT_EQ(test, before.pstate, regs.pstate);
		memcpy(expected, before.regs, sizeof(expected));
		if (cases[index].kind == BCS_BLR)
			expected[30] = address + sizeof(u32);
		KUNIT_EXPECT_MEMEQ(test, expected, regs.regs, sizeof(expected));
		KUNIT_EXPECT_EQ(test, result.reason, debug_result.reason);
		KUNIT_EXPECT_EQ(test, result.status, debug_result.status);
		KUNIT_EXPECT_EQ(test, result.fault_address,
				debug_result.fault_address);
		KUNIT_EXPECT_EQ(test, result.fault_access,
				debug_result.fault_access);
		KUNIT_EXPECT_EQ(test, result.pc, debug_result.pc);
		KUNIT_EXPECT_EQ(test, result.instruction, debug_result.instruction);
		KUNIT_EXPECT_EQ(test, target, debug_regs.pc);
		KUNIT_EXPECT_MEMEQ(test, expected, debug_regs.regs,
				   sizeof(expected));
		KUNIT_EXPECT_EQ(test, before.pstate, debug_regs.pstate);
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(address, PAGE_SIZE));
	}
}

static void bcs_register_branch_out_of_range_target_production(struct kunit *test)
{
	static const struct {
		enum bcs_kind kind;
		u32 instruction;
	} cases[] = {
		{ BCS_BR, 0xd61f00a0U },
		{ BCS_BLR, 0xd63f00a0U },
		{ BCS_RET, 0xd65f00a0U },
	};
	size_t index;

	for (index = 0; index < ARRAY_SIZE(cases); index++) {
		struct pt_regs regs = {};
		struct pt_regs debug_regs;
		struct pt_regs before;
		struct orlix_tcti_result result;
		struct orlix_tcti_result debug_result;
		unsigned long address;
		unsigned long expected[ARRAY_SIZE(regs.regs)];

		address = bcs_map_program(test, cases[index].instruction);
		bcs_seed_regs(&regs, address, &bcs_leaves[0], true);
		regs.regs[5] = TASK_SIZE;
		before = regs;
		debug_regs = regs;
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		debug_result = orlix_tcti_switch_debug_resume_user(current, &debug_regs,
							     current->mm);

		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_USER_FAULT, result.reason,
				    "kind=%u", cases[index].kind);
		KUNIT_EXPECT_EQ(test, -EFAULT, result.status);
		KUNIT_EXPECT_EQ(test, TASK_SIZE, result.fault_address);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_ACCESS_FETCH, result.fault_access);
		KUNIT_EXPECT_EQ(test, TASK_SIZE, result.pc);
		KUNIT_EXPECT_EQ(test, TASK_SIZE, regs.pc);
		KUNIT_EXPECT_EQ(test, 0U, result.instruction);
		KUNIT_EXPECT_EQ(test, before.pstate, regs.pstate);
		memcpy(expected, before.regs, sizeof(expected));
		if (cases[index].kind == BCS_BLR)
			expected[30] = address + sizeof(u32);
		KUNIT_EXPECT_MEMEQ(test, expected, regs.regs, sizeof(expected));
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_USER_FAULT, debug_result.reason);
		KUNIT_EXPECT_EQ(test, -EFAULT, debug_result.status);
		KUNIT_EXPECT_EQ(test, TASK_SIZE, debug_result.fault_address);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_ACCESS_FETCH,
				debug_result.fault_access);
		KUNIT_EXPECT_EQ(test, TASK_SIZE, debug_result.pc);
		KUNIT_EXPECT_EQ(test, 0U, debug_result.instruction);
		KUNIT_EXPECT_EQ(test, TASK_SIZE, debug_regs.pc);
		KUNIT_EXPECT_MEMEQ(test, expected, debug_regs.regs,
				   sizeof(expected));
		KUNIT_EXPECT_EQ(test, before.pstate, debug_regs.pstate);
		KUNIT_EXPECT_EQ(test, result.reason, debug_result.reason);
		KUNIT_EXPECT_EQ(test, result.status, debug_result.status);
		KUNIT_EXPECT_EQ(test, result.fault_address,
				debug_result.fault_address);
		KUNIT_EXPECT_EQ(test, result.fault_access,
				debug_result.fault_access);
		KUNIT_EXPECT_EQ(test, result.pc, debug_result.pc);
		KUNIT_EXPECT_EQ(test, result.instruction, debug_result.instruction);
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(address, PAGE_SIZE));
	}
}

static void bcs_al_nv_condition_production(struct kunit *test)
{
	static const u8 conditions[] = { 14, 15 };
	size_t index;

	for (index = 0; index < ARRAY_SIZE(conditions); index++) {
		u32 instruction = 0x54000040U | conditions[index];
		struct pt_regs regs = {};
		struct pt_regs before;
		struct orlix_tcti_result result;
		unsigned long address;

		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_CONDITIONAL_BRANCH_IMMEDIATE,
			orlix_tcti_decode_aarch64(instruction).decode_class);
		address = bcs_map_program(test, instruction);
		bcs_seed_regs(&regs, address, &bcs_leaves[0], false);
		before = regs;
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
		KUNIT_EXPECT_EQ(test, BCS_SVC_TAKEN, result.instruction);
		KUNIT_EXPECT_EQ(test, address + 3 * sizeof(u32), regs.pc);
		KUNIT_EXPECT_MEMEQ(test, before.regs, regs.regs, sizeof(regs.regs));
		KUNIT_EXPECT_EQ(test, before.pstate, regs.pstate);
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(address, PAGE_SIZE));
	}
}

enum cbe_condition {
	CBE_GT,
	CBE_GE,
	CBE_HI,
	CBE_HS,
	CBE_EQ,
	CBE_NE,
	CBE_LT,
	CBE_LO,
};

struct cbe_leaf {
	u16 ordinal;
	const char *name;
	u32 mask;
	u32 pattern;
	enum cbe_condition condition;
	u8 access_size;
	bool immediate;
};

/* Complete FEAT_CMPBR source inventory, ordinals 2215-2226 and 2318-2341. */
static const struct cbe_leaf cbe_leaves[] = {
	{ 2215, "CBBGT_8_regs", 0xffe0c000U, 0x74008000U, CBE_GT, 1, false },
	{ 2216, "CBBGE_8_regs", 0xffe0c000U, 0x74208000U, CBE_GE, 1, false },
	{ 2217, "CBBHI_8_regs", 0xffe0c000U, 0x74408000U, CBE_HI, 1, false },
	{ 2218, "CBBHS_8_regs", 0xffe0c000U, 0x74608000U, CBE_HS, 1, false },
	{ 2219, "CBBEQ_8_regs", 0xffe0c000U, 0x74c08000U, CBE_EQ, 1, false },
	{ 2220, "CBBNE_8_regs", 0xffe0c000U, 0x74e08000U, CBE_NE, 1, false },
	{ 2221, "CBHGT_16_regs", 0xffe0c000U, 0x7400c000U, CBE_GT, 2, false },
	{ 2222, "CBHGE_16_regs", 0xffe0c000U, 0x7420c000U, CBE_GE, 2, false },
	{ 2223, "CBHHI_16_regs", 0xffe0c000U, 0x7440c000U, CBE_HI, 2, false },
	{ 2224, "CBHHS_16_regs", 0xffe0c000U, 0x7460c000U, CBE_HS, 2, false },
	{ 2225, "CBHEQ_16_regs", 0xffe0c000U, 0x74c0c000U, CBE_EQ, 2, false },
	{ 2226, "CBHNE_16_regs", 0xffe0c000U, 0x74e0c000U, CBE_NE, 2, false },
	{ 2318, "CBGT_32_regs", 0xffe0c000U, 0x74000000U, CBE_GT, 4, false },
	{ 2319, "CBGE_32_regs", 0xffe0c000U, 0x74200000U, CBE_GE, 4, false },
	{ 2320, "CBHI_32_regs", 0xffe0c000U, 0x74400000U, CBE_HI, 4, false },
	{ 2321, "CBHS_32_regs", 0xffe0c000U, 0x74600000U, CBE_HS, 4, false },
	{ 2322, "CBEQ_32_regs", 0xffe0c000U, 0x74c00000U, CBE_EQ, 4, false },
	{ 2323, "CBNE_32_regs", 0xffe0c000U, 0x74e00000U, CBE_NE, 4, false },
	{ 2324, "CBGT_64_regs", 0xffe0c000U, 0xf4000000U, CBE_GT, 8, false },
	{ 2325, "CBGE_64_regs", 0xffe0c000U, 0xf4200000U, CBE_GE, 8, false },
	{ 2326, "CBHI_64_regs", 0xffe0c000U, 0xf4400000U, CBE_HI, 8, false },
	{ 2327, "CBHS_64_regs", 0xffe0c000U, 0xf4600000U, CBE_HS, 8, false },
	{ 2328, "CBEQ_64_regs", 0xffe0c000U, 0xf4c00000U, CBE_EQ, 8, false },
	{ 2329, "CBNE_64_regs", 0xffe0c000U, 0xf4e00000U, CBE_NE, 8, false },
	{ 2330, "CBGT_32_imm", 0xffe04000U, 0x75000000U, CBE_GT, 4, true },
	{ 2331, "CBLT_32_imm", 0xffe04000U, 0x75200000U, CBE_LT, 4, true },
	{ 2332, "CBHI_32_imm", 0xffe04000U, 0x75400000U, CBE_HI, 4, true },
	{ 2333, "CBLO_32_imm", 0xffe04000U, 0x75600000U, CBE_LO, 4, true },
	{ 2334, "CBEQ_32_imm", 0xffe04000U, 0x75c00000U, CBE_EQ, 4, true },
	{ 2335, "CBNE_32_imm", 0xffe04000U, 0x75e00000U, CBE_NE, 4, true },
	{ 2336, "CBGT_64_imm", 0xffe04000U, 0xf5000000U, CBE_GT, 8, true },
	{ 2337, "CBLT_64_imm", 0xffe04000U, 0xf5200000U, CBE_LT, 8, true },
	{ 2338, "CBHI_64_imm", 0xffe04000U, 0xf5400000U, CBE_HI, 8, true },
	{ 2339, "CBLO_64_imm", 0xffe04000U, 0xf5600000U, CBE_LO, 8, true },
	{ 2340, "CBEQ_64_imm", 0xffe04000U, 0xf5c00000U, CBE_EQ, 8, true },
	{ 2341, "CBNE_64_imm", 0xffe04000U, 0xf5e00000U, CBE_NE, 8, true },
};

static u32 cbe_instruction(const struct cbe_leaf *leaf, u8 rt, u8 rm,
			   u8 immediate)
{
	u32 instruction = leaf->pattern | (2U << 5) | rt;

	if (leaf->immediate)
		return instruction | ((u32)(immediate & 0x1fU) << 16) |
			       ((u32)(immediate & 0x20U) << 10);
	return instruction | ((u32)rm << 16);
}

static bool cbe_taken(enum cbe_condition condition, u64 left, u64 right,
		      u8 access_size)
{
	switch (access_size) {
	case 1:
		left = (u8)left;
		right = (u8)right;
		break;
	case 2:
		left = (u16)left;
		right = (u16)right;
		break;
	case 4:
		left = (u32)left;
		right = (u32)right;
		break;
	default:
		break;
	}

	switch (condition) {
	case CBE_GT:
		return access_size == 1 ? (s8)left > (s8)right :
		       access_size == 2 ? (s16)left > (s16)right :
		       access_size == 4 ? (s32)left > (s32)right :
					 (s64)left > (s64)right;
	case CBE_GE:
		return access_size == 1 ? (s8)left >= (s8)right :
		       access_size == 2 ? (s16)left >= (s16)right :
		       access_size == 4 ? (s32)left >= (s32)right :
					 (s64)left >= (s64)right;
	case CBE_HI:
		return left > right;
	case CBE_HS:
		return left >= right;
	case CBE_EQ:
		return left == right;
	case CBE_NE:
		return left != right;
	case CBE_LT:
		return access_size == 4 ? (s32)left < (s32)right :
					 (s64)left < (s64)right;
	case CBE_LO:
		return left < right;
	}

	return false;
}

static u64 cbe_left_for_path(const struct cbe_leaf *leaf, u8 immediate,
			     bool taken)
{
	u64 right = leaf->immediate ? immediate : 0;

	switch (leaf->condition) {
	case CBE_GT:
	case CBE_HI:
	case CBE_NE:
		return taken ? right + 1 : right;
	case CBE_GE:
	case CBE_HS:
	case CBE_EQ:
		return taken ? right : right - 1;
	case CBE_LT:
	case CBE_LO:
		return taken ? right - 1 : right;
	}

	return 0;
}

static void cbe_source_decode(struct kunit *test)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(cbe_leaves); index++) {
		const struct cbe_leaf *leaf = &cbe_leaves[index];
		static const u16 branch_immediates[] = { 0, 1, 0x100, 0x1ff };
		u8 rt, rm_or_imm;

		KUNIT_EXPECT_EQ_MSG(test, leaf->pattern, leaf->pattern & leaf->mask,
				    "%s ordinal %u", leaf->name, leaf->ordinal);
		for (rt = 0; rt < 32; rt++) {
			for (rm_or_imm = 0; rm_or_imm < (leaf->immediate ? 64 : 32);
			     rm_or_imm++) {
				struct orlix_tcti_decoded_instruction decoded =
					orlix_tcti_decode_aarch64(cbe_instruction(leaf, rt,
								    rm_or_imm, rm_or_imm));

				KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_DECODE_COMPARE_BRANCH_EXTENSION,
						    decoded.decode_class, "%s", leaf->name);
				KUNIT_EXPECT_EQ_MSG(test, rt, decoded.rt, "%s", leaf->name);
				if (leaf->immediate)
					KUNIT_EXPECT_EQ_MSG(test, rm_or_imm, decoded.imm6,
							    "%s", leaf->name);
				else
					KUNIT_EXPECT_EQ_MSG(test, rm_or_imm, decoded.rm,
							    "%s", leaf->name);
			}
		}
		for (rm_or_imm = 0; rm_or_imm < ARRAY_SIZE(branch_immediates);
		     rm_or_imm++) {
			u16 branch = branch_immediates[rm_or_imm];
			struct orlix_tcti_decoded_instruction decoded = orlix_tcti_decode_aarch64(
				(cbe_instruction(leaf, 0, 0, 0) & ~0x3fe0U) |
				((u32)branch << 5));
			s64 expected = branch & BIT(8) ?
				((s64)branch - 0x200) * sizeof(u32) :
				(s64)branch * sizeof(u32);

			KUNIT_EXPECT_EQ_MSG(test, expected, decoded.branch_imm, "%s",
					    leaf->name);
		}
	}
}

static void cbe_resume_case(struct kunit *test, const struct cbe_leaf *leaf,
			    u64 left, u64 right, u8 immediate, bool requested_taken)
{
	struct pt_regs regs = {}, before;
	struct orlix_tcti_result result;
	unsigned long address;
	bool taken;

	address = bcs_map_program(test, cbe_instruction(leaf, 1, 2, immediate));
	bcs_seed_regs(&regs, address, &bcs_leaves[0], false);
	regs.regs[1] = left;
	regs.regs[2] = right;
	before = regs;
	taken = cbe_taken(leaf->condition, left,
		leaf->immediate ? immediate : right, leaf->access_size);
	KUNIT_ASSERT_EQ_MSG(test, requested_taken, taken,
			    "%s ordinal %u did not construct requested path", leaf->name,
			    leaf->ordinal);
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_ASSERT_EQ_MSG(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason,
			    "%s ordinal %u", leaf->name, leaf->ordinal);
	KUNIT_EXPECT_EQ(test, taken ? BCS_SVC_TAKEN : BCS_SVC_NOT_TAKEN,
			result.instruction);
	KUNIT_EXPECT_EQ(test, address + (taken ? 3 : 2) * sizeof(u32), regs.pc);
	KUNIT_EXPECT_MEMEQ(test, before.regs, regs.regs, sizeof(regs.regs));
	KUNIT_EXPECT_EQ(test, before.pstate, regs.pstate);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(address, PAGE_SIZE));
}

static void cbe_production_resume(struct kunit *test)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(cbe_leaves); index++) {
		const struct cbe_leaf *leaf = &cbe_leaves[index];
		unsigned int requested_taken;

		for (requested_taken = 0; requested_taken < 2; requested_taken++) {
			u8 immediate = leaf->immediate ? 1 : 0;
			u64 left = cbe_left_for_path(leaf, immediate,
						     requested_taken);
			u64 right = leaf->immediate ? immediate : 0;

			if (!leaf->immediate && leaf->condition == CBE_HS &&
			    !requested_taken) {
				left = 0;
				right = 1;
			}
			if (leaf->condition == CBE_LO) {
				left = requested_taken ? 0 : 1;
				right = leaf->immediate ? immediate :
					(requested_taken ? 1 : 0);
			}
			if (leaf->access_size < sizeof(u64)) {
				left |= 0xa5a5a5a500000000ULL;
				right |= 0x5a5a5a5a00000000ULL;
			}
			cbe_resume_case(test, leaf, left, right, immediate,
					requested_taken);
		}
		if (leaf->immediate) {
			bool zero_taken = leaf->condition != CBE_LO;

			cbe_resume_case(test, leaf,
				cbe_left_for_path(leaf, 0, zero_taken), 0, 0,
				zero_taken);
			cbe_resume_case(test, leaf,
				cbe_left_for_path(leaf, 63, true), 0, 63, true);
		}
	}
}

static void cbe_extrema_production(struct kunit *test)
{
	static const struct {
		u8 leaf;
		u64 left;
		u64 right;
		u8 immediate;
		bool taken;
	} cases[] = {
		{ 0, 0x7f, 0x80, 0, true }, { 1, 0x80, 0x7f, 0, false },
		{ 6, 0x7fff, 0x8000, 0, true }, { 7, 0x8000, 0x7fff, 0, false },
		{ 12, 0x7fffffff, 0x80000000, 0, true },
		{ 13, 0x80000000, 0x7fffffff, 0, false },
		{ 18, S64_MAX, S64_MIN, 0, true },
		{ 19, S64_MIN, S64_MAX, 0, false },
		{ 24, S32_MAX, 0, 63, true }, { 25, S32_MIN, 0, 0, true },
		{ 30, S64_MAX, 0, 63, true }, { 31, S64_MIN, 0, 0, true },
		{ 2, 0xff, 0, 0, true }, { 3, 0, 0xff, 0, false },
		{ 8, 0xffff, 0, 0, true }, { 9, 0, 0xffff, 0, false },
		{ 14, U32_MAX, 0, 0, true }, { 15, 0, U32_MAX, 0, false },
		{ 20, U64_MAX, 0, 0, true }, { 21, 0, U64_MAX, 0, false },
		{ 26, U32_MAX, 0, 63, true }, { 27, 0, 0, 63, true },
		{ 32, U64_MAX, 0, 63, true }, { 33, 0, 0, 63, true },
	};
	size_t index;

	for (index = 0; index < ARRAY_SIZE(cases); index++)
		cbe_resume_case(test, &cbe_leaves[cases[index].leaf],
				cases[index].left, cases[index].right,
				cases[index].immediate, cases[index].taken);
}

static void cbe_invalid_encodings_reject(struct kunit *test)
{
	static const u32 invalid[] = {
		0x74004000U, 0x74800000U, 0x74a00000U, 0x75004000U,
	};
	size_t index;

	for (index = 0; index < ARRAY_SIZE(invalid); index++)
		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
				orlix_tcti_decode_aarch64(invalid[index]).decode_class,
				"invalid FEAT_CMPBR encoding %#x", invalid[index]);
}

static void cbe_invalid_encodings_fail_closed_in_production(struct kunit *test)
{
	static const u32 invalid[] = {
		0x74004000U, 0x74800000U, 0x74a00000U, 0x75004000U,
	};
	size_t index;

	for (index = 0; index < ARRAY_SIZE(invalid); index++) {
		struct pt_regs regs = {};
		struct orlix_tcti_result result;
		unsigned long address = bcs_map_program(test, invalid[index]);

		bcs_seed_regs(&regs, address, &bcs_leaves[0], false);
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION, result.reason);
		KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, result.status);
		KUNIT_EXPECT_EQ(test, address, result.pc);
		KUNIT_EXPECT_EQ(test, invalid[index], result.instruction);
		KUNIT_EXPECT_EQ(test, address, regs.pc);
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(address, PAGE_SIZE));
	}
}

static void cbe_narrow_sf_encodings_fail_closed(struct kunit *test)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(cbe_leaves); index++) {
		const struct cbe_leaf *leaf = &cbe_leaves[index];
		struct pt_regs regs = {};
		struct orlix_tcti_result result;
		u32 instruction;
		unsigned long address;

		if (leaf->immediate || leaf->access_size >= sizeof(u32))
			continue;
		instruction = cbe_instruction(leaf, 1, 2, 0) | BIT(31);
		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
			orlix_tcti_decode_aarch64(instruction).decode_class,
			"%s ordinal %u", leaf->name, leaf->ordinal);
		address = bcs_map_program(test, instruction);
		bcs_seed_regs(&regs, address, &bcs_leaves[0], false);
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
			result.reason, "%s ordinal %u", leaf->name, leaf->ordinal);
		KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, result.status);
		KUNIT_EXPECT_EQ(test, address, result.pc);
		KUNIT_EXPECT_EQ(test, instruction, result.instruction);
		KUNIT_EXPECT_EQ(test, address, regs.pc);
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(address, PAGE_SIZE));
	}
}

static struct kunit_case bcs_cases[] = {
	KUNIT_CASE(bcs_issue_132_exact_source_cohort),
	KUNIT_CASE(bcs_source_decode),
	KUNIT_CASE(bcs_production_resume),
	KUNIT_CASE(bcs_issue_132_exceptions_emit_typed_observations),
	KUNIT_CASE(bcs_x31_semantics_production),
	KUNIT_CASE(bcs_register_branch_unaligned_target_production),
	KUNIT_CASE(bcs_register_branch_out_of_range_target_production),
	KUNIT_CASE(bcs_al_nv_condition_production),
	KUNIT_CASE(cbe_source_decode),
	KUNIT_CASE(cbe_production_resume),
	KUNIT_CASE(cbe_extrema_production),
	KUNIT_CASE(cbe_invalid_encodings_reject),
	KUNIT_CASE(cbe_invalid_encodings_fail_closed_in_production),
	KUNIT_CASE(cbe_narrow_sf_encodings_fail_closed),
	{}
};

static struct kunit_suite orlix_tcti_branch_control_source_bound_test_suite = {
	.name = "orlix-tcti-branch-control-source-bound",
	.test_cases = bcs_cases,
};

kunit_test_suite(orlix_tcti_branch_control_source_bound_test_suite);
