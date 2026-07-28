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
#include <asm/orlix_tcti.h>
#include <kunit/test.h>
#include <linux/err.h>
#include <linux/completion.h>
#include <linux/kthread.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/syscalls.h>

#include "../decode_aarch64.h"
#include "../isa/base_a64_control_flow_semantic_blockers.h"
#include "../switch_debug.h"
#include "orlix_tcti_native_observation.h"
#include "orlix_tcti_branch_control_production_capture.h"
#include "orlix_tcti_source_leaf_rejection_catalog.h"
#include "orlix_tcti_test_suites.h"
#include "target_proof_ingestion_private.h"
#include "target_native_proof_contract_private.h"
#include "target_completion_audit.h"
#include "target_execution_slice_map.h"
#include "target_proof_ingestion.h"
#include "target_proof_registry.h"

struct bcs_ingest_racer {
	struct completion *start;
	struct orlix_tcti_target_proof_ingestion_ledger *ledger;
	struct orlix_tcti_native_wire_record *wire;
	int result;
};

static u32 bcs_wire_get32(const u8 *bytes)
{
	return (u32)bytes[0] | ((u32)bytes[1] << 8) |
		((u32)bytes[2] << 16) | ((u32)bytes[3] << 24);
}

static u64 bcs_wire_get64(const u8 *bytes)
{
	u64 value = 0;
	size_t index;

	for (index = 0; index < 8U; index++)
		value |= (u64)bytes[index] << (index * 8U);
	return value;
}

static const u8 *bcs_wire_selector_payload(const struct orlix_tcti_native_wire_record *wire,
					    u32 wanted_kind, u32 *length)
{
	const u8 *bytes;
	size_t cursor = 72U;
	u32 count;
	u32 index;

	if (!wire || !wire->bytes || !length)
		return NULL;
	bytes = wire->bytes;
	count = bcs_wire_get32(bytes + 52U);
	for (index = 0; index < count; index++) {
		u32 item_length = bcs_wire_get32(bytes + cursor + 12U);

		if (bcs_wire_get32(bytes + cursor) == wanted_kind) {
			*length = item_length;
			return bytes + cursor + 16U;
		}
		cursor += 16U + item_length;
	}
	return NULL;
}

static void bcs_expect_vector_state_is_canonical(struct kunit *test,
					  const struct orlix_tcti_native_wire_record *wire)
{
	const u8 *payload;
	u32 length;
	size_t index;

	payload = bcs_wire_selector_payload(wire,
		ORLIX_TCTI_NATIVE_CAPTURE_SELECTOR_FP_SIMD, &length);
	KUNIT_ASSERT_NOT_NULL(test, payload);
	KUNIT_ASSERT_EQ(test, 24U + 64U * sizeof(u64), length);
	KUNIT_EXPECT_EQ(test, current->thread.user_fpcr, bcs_wire_get64(payload));
	KUNIT_EXPECT_EQ(test, current->thread.user_fpsr, bcs_wire_get64(payload + 8U));
	KUNIT_EXPECT_EQ(test, current->thread.user_simd_valid,
		bcs_wire_get32(payload + 16U));
	KUNIT_EXPECT_EQ(test, 64U * sizeof(u64), bcs_wire_get32(payload + 20U));
	for (index = 0; index < ARRAY_SIZE(current->thread.user_simd); index++)
		KUNIT_EXPECT_EQ(test, current->thread.user_simd[index],
			bcs_wire_get64(payload + 24U + index * sizeof(u64)));

	payload = bcs_wire_selector_payload(wire,
		ORLIX_TCTI_NATIVE_CAPTURE_SELECTOR_SVE, &length);
	KUNIT_ASSERT_NOT_NULL(test, payload);
	KUNIT_EXPECT_EQ(test, 1U, bcs_wire_get32(payload));
	KUNIT_EXPECT_EQ(test, (u32)ORLIX_TCTI_SVE_MAX_VL_BYTES,
		bcs_wire_get32(payload + 4U));
	KUNIT_EXPECT_EQ(test, 32U * ORLIX_TCTI_SVE_MAX_VL_BYTES,
		bcs_wire_get32(payload + 8U));
	KUNIT_EXPECT_EQ(test, 2U * ORLIX_TCTI_SVE_MAX_VL_BYTES,
		bcs_wire_get32(payload + 12U));
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_SVE_MAX_VL_BYTES / 8U,
		bcs_wire_get32(payload + 16U));
	KUNIT_EXPECT_EQ(test, 24U + 32U * ORLIX_TCTI_SVE_MAX_VL_BYTES +
		2U * ORLIX_TCTI_SVE_MAX_VL_BYTES + ORLIX_TCTI_SVE_MAX_VL_BYTES / 8U,
		length);

	payload = bcs_wire_selector_payload(wire,
		ORLIX_TCTI_NATIVE_CAPTURE_SELECTOR_SME, &length);
	KUNIT_ASSERT_NOT_NULL(test, payload);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_NATIVE_STATE_CAPTURED,
		bcs_wire_get32(payload));
	KUNIT_EXPECT_EQ(test, 7U, bcs_wire_get32(payload + 4U));
	KUNIT_EXPECT_EQ(test, (u32)ORLIX_TCTI_SVE_MAX_VL_BYTES,
		bcs_wire_get32(payload + 8U));
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_SVE_MAX_VL_BYTES *
		ORLIX_TCTI_SVE_MAX_VL_BYTES, bcs_wire_get32(payload + 12U));
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_SVE_MAX_VL_BYTES,
		bcs_wire_get32(payload + 16U));
	KUNIT_EXPECT_EQ(test, 24U + ORLIX_TCTI_SVE_MAX_VL_BYTES *
		ORLIX_TCTI_SVE_MAX_VL_BYTES + ORLIX_TCTI_SVE_MAX_VL_BYTES, length);
	KUNIT_EXPECT_EQ(test, current->thread.user_sme.za[0], payload[24U]);
	KUNIT_EXPECT_EQ(test, current->thread.user_sme.za[
		current->thread.user_sme.za_bytes - 1U], payload[24U +
		current->thread.user_sme.za_bytes - 1U]);
	KUNIT_EXPECT_EQ(test, current->thread.user_sme.zt0[0],
		payload[24U + current->thread.user_sme.za_bytes]);
}

static void bcs_expect_all_capture_selectors_tamper_closed(
	struct kunit *test, struct orlix_tcti_target_proof_ingestion_ledger *ledger,
	struct orlix_tcti_native_wire_record *wire)
{
	static const u32 kinds[] = {
		ORLIX_TCTI_NATIVE_CAPTURE_SELECTOR_RESULT,
		ORLIX_TCTI_NATIVE_CAPTURE_SELECTOR_GPR,
		ORLIX_TCTI_NATIVE_CAPTURE_SELECTOR_FP_SIMD,
		ORLIX_TCTI_NATIVE_CAPTURE_SELECTOR_SVE,
		ORLIX_TCTI_NATIVE_CAPTURE_SELECTOR_SME,
		ORLIX_TCTI_NATIVE_CAPTURE_SELECTOR_DECODED_FIELD,
		ORLIX_TCTI_NATIVE_CAPTURE_SELECTOR_MEMORY,
		ORLIX_TCTI_NATIVE_CAPTURE_SELECTOR_FAULT,
		ORLIX_TCTI_NATIVE_CAPTURE_SELECTOR_ARITHMETIC,
		ORLIX_TCTI_NATIVE_CAPTURE_SELECTOR_ATOMICITY,
		ORLIX_TCTI_NATIVE_CAPTURE_SELECTOR_ORDERING,
		ORLIX_TCTI_NATIVE_CAPTURE_SELECTOR_SYSTEM_CONTROL,
		ORLIX_TCTI_NATIVE_CAPTURE_SELECTOR_LINUX_INTERFACE,
	};
	u8 *bytes = (u8 *)wire->bytes;
	size_t cursor = 72U;
	size_t index;

	KUNIT_ASSERT_EQ(test, ARRAY_SIZE(kinds), bcs_wire_get32(bytes + 52U));
	for (index = 0; index < ARRAY_SIZE(kinds); index++) {
		u32 length;

		KUNIT_ASSERT_EQ(test, kinds[index], bcs_wire_get32(bytes + cursor));
		KUNIT_ASSERT_EQ(test, index, bcs_wire_get32(bytes + cursor + 4U));
		length = bcs_wire_get32(bytes + cursor + 12U);
		KUNIT_ASSERT_GT(test, length, 0U);
		bytes[cursor + 16U] ^= 0x80U;
		KUNIT_EXPECT_LT(test,
			orlix_tcti_target_proof_ingest_native(ledger, wire, NULL), 0);
		KUNIT_EXPECT_FALSE(test, wire->consumed);
		bytes[cursor + 16U] ^= 0x80U;
		cursor += 16U + length;
	}
	/* Opaque row binding and finalized-instance nonce are inside the seal. */
	bytes[56U] ^= 0x80U;
	KUNIT_EXPECT_LT(test, orlix_tcti_target_proof_ingest_native(ledger, wire, NULL), 0);
	bytes[56U] ^= 0x80U;
	bytes[64U] ^= 0x80U;
	KUNIT_EXPECT_LT(test, orlix_tcti_target_proof_ingest_native(ledger, wire, NULL), 0);
	bytes[64U] ^= 0x80U;
	KUNIT_EXPECT_EQ(test, wire->length - 36U, cursor);
}

static int bcs_ingest_race(void *argument)
{
	struct bcs_ingest_racer *racer = argument;

	wait_for_completion(racer->start);
	racer->result = orlix_tcti_target_proof_ingest_native(racer->ledger,
		racer->wire, NULL);
	return racer->result;
}

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

enum bcs_issue_130_disposition {
	BCS_ISSUE_130_REQUIRED_EL0,
	BCS_ISSUE_130_NON_EL0,
	BCS_ISSUE_130_SEMANTIC_BLOCKER,
};

struct bcs_issue_130_leaf {
	u16 ordinal;
	const char *name;
	enum bcs_issue_130_disposition disposition;
};

static const struct bcs_issue_130_leaf bcs_issue_130_leaves[] = {
	{ 2288, "BR_64_branch_reg", BCS_ISSUE_130_REQUIRED_EL0 },
	{ 2291, "BLR_64_branch_reg", BCS_ISSUE_130_REQUIRED_EL0 },
	{ 2294, "RET_64R_branch_reg", BCS_ISSUE_130_REQUIRED_EL0 },
	{ 2299, "ERET_64E_branch_reg", BCS_ISSUE_130_NON_EL0 },
	{ 2300, "ERETAA_64E_branch_reg", BCS_ISSUE_130_NON_EL0 },
	{ 2301, "ERETAB_64E_branch_reg", BCS_ISSUE_130_NON_EL0 },
	{ 2302, "TEXIT_te_branch_reg", BCS_ISSUE_130_SEMANTIC_BLOCKER },
	{ 2303, "DRPS_64E_branch_reg", BCS_ISSUE_130_NON_EL0 },
	{ 2312, "B_only_branch_imm", BCS_ISSUE_130_REQUIRED_EL0 },
	{ 2313, "BL_only_branch_imm", BCS_ISSUE_130_REQUIRED_EL0 },
};

struct bcs_semantic_blocker {
	u16 ordinal;
	const char *name;
	enum orlix_tcti_a64_semantic_provenance_disposition disposition;
	const char *operation_locator;
	const char *operation_sha256;
};

#define ORLIX_TCTI_A64_SEMANTIC_PROVENANCE_SOURCE(...)
#define ORLIX_TCTI_A64_DDI0602_PROVENANCE_ROW(...)
#define ORLIX_TCTI_A64_OFFICIAL_SEMANTICS_NOT_SPECIFIED_ROW(ordinal, name, \
		locator, offset, length, digest) \
	{ ordinal, name, \
	  ORLIX_TCTI_A64_SEMANTIC_PROVENANCE_OFFICIAL_SEMANTICS_NOT_SPECIFIED, \
	  locator, digest },
static const struct bcs_semantic_blocker bcs_semantic_blockers[] = {
#include "../isa/target_asl_availability.def"
};
#undef ORLIX_TCTI_A64_OFFICIAL_SEMANTICS_NOT_SPECIFIED_ROW
#undef ORLIX_TCTI_A64_DDI0602_PROVENANCE_ROW
#undef ORLIX_TCTI_A64_SEMANTIC_PROVENANCE_SOURCE

static const struct bcs_semantic_blocker *
bcs_semantic_blocker_for_ordinal(u16 ordinal)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(bcs_semantic_blockers); index++)
		if (bcs_semantic_blockers[index].ordinal == ordinal)
			return &bcs_semantic_blockers[index];
	return NULL;
}

static const struct bcs_leaf *bcs_leaf_for_ordinal(u16 ordinal)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(bcs_leaves); index++)
		if (bcs_leaves[index].ordinal == ordinal)
			return &bcs_leaves[index];
	return NULL;
}

static const struct orlix_tcti_source_leaf_rejection *
bcs_rejection_for_ordinal(u16 ordinal,
			  struct orlix_tcti_source_leaf_rejection *entry)
{
	size_t index;

	for (index = 0; index < orlix_tcti_source_leaf_rejection_count(); index++) {
		const struct orlix_tcti_source_leaf_rejection *leaf =
			orlix_tcti_source_leaf_rejection_at(index, entry);

		if (leaf && leaf->ordinal == ordinal)
			return leaf;
	}
	return NULL;
}

static void bcs_issue_130_exact_cohort_accounting(struct kunit *test)
{
	const struct orlix_tcti_execution_slice_map *map =
		orlix_tcti_execution_slice_map_canonical();
	struct orlix_tcti_execution_slice_map_validation_result validation;
	bool seen[ARRAY_SIZE(bcs_issue_130_leaves)] = { false };
	size_t family_index = map->counts.family_count;
	size_t member_count = 0;
	size_t index;

	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_execution_slice_map_validate(map, &validation));
	for (index = 0; index < map->counts.family_count; index++)
		if (map->families[index].issue_id == 130U) {
			KUNIT_ASSERT_EQ(test, map->counts.family_count, family_index);
			family_index = index;
		}
	KUNIT_ASSERT_LT(test, family_index, map->counts.family_count);
	KUNIT_EXPECT_STREQ(test, "base-a64-control-flow",
			   map->families[family_index].stable_id);
	KUNIT_EXPECT_EQ(test, ARRAY_SIZE(bcs_issue_130_leaves),
			map->families[family_index].declared_member_count);

	for (index = 0; index < map->counts.leaf_count; index++) {
		const struct orlix_tcti_execution_slice_member *member =
			&map->members[index];
		size_t expected_index;

		if (member->family_index != family_index)
			continue;
		member_count++;
		for (expected_index = 0;
		     expected_index < ARRAY_SIZE(bcs_issue_130_leaves);
		     expected_index++)
			if (member->ordinal ==
			    bcs_issue_130_leaves[expected_index].ordinal)
				break;
		KUNIT_ASSERT_LT_MSG(test, expected_index,
			ARRAY_SIZE(bcs_issue_130_leaves),
			"unexpected issue #130 ordinal %u", member->ordinal);
		KUNIT_EXPECT_FALSE_MSG(test, seen[expected_index],
			"duplicate issue #130 ordinal %u", member->ordinal);
		seen[expected_index] = true;
		KUNIT_EXPECT_STREQ(test, bcs_issue_130_leaves[expected_index].name,
				   member->source_name);
	}
	KUNIT_EXPECT_EQ(test, ARRAY_SIZE(bcs_issue_130_leaves), member_count);

	for (index = 0; index < ARRAY_SIZE(bcs_issue_130_leaves); index++) {
		const struct bcs_issue_130_leaf *expected =
			&bcs_issue_130_leaves[index];

		KUNIT_EXPECT_TRUE_MSG(test, seen[index], "missing ordinal %u",
				      expected->ordinal);
		if (expected->disposition == BCS_ISSUE_130_REQUIRED_EL0) {
			const struct bcs_leaf *leaf =
				bcs_leaf_for_ordinal(expected->ordinal);

			KUNIT_ASSERT_NOT_NULL(test, leaf);
			KUNIT_EXPECT_STREQ(test, expected->name, leaf->name);
		} else if (expected->disposition == BCS_ISSUE_130_NON_EL0) {
			struct orlix_tcti_source_leaf_rejection entry;
			const struct orlix_tcti_source_leaf_rejection *leaf =
				bcs_rejection_for_ordinal(expected->ordinal, &entry);

			KUNIT_ASSERT_NOT_NULL(test, leaf);
			KUNIT_EXPECT_STREQ(test, expected->name, leaf->name);
		} else {
			const struct bcs_semantic_blocker *blocker =
				bcs_semantic_blocker_for_ordinal(expected->ordinal);
			struct orlix_tcti_source_leaf_rejection entry;

			KUNIT_ASSERT_NOT_NULL(test, blocker);
			KUNIT_EXPECT_STREQ(test, expected->name, blocker->name);
			KUNIT_EXPECT_EQ(test,
				ORLIX_TCTI_A64_SEMANTIC_PROVENANCE_OFFICIAL_SEMANTICS_NOT_SPECIFIED,
				blocker->disposition);
			KUNIT_EXPECT_STREQ(test,
				"Instructions.json#operations/TEXIT/operation",
				blocker->operation_locator);
			KUNIT_EXPECT_STREQ(test,
				"28fb16d9885379aa6e05267c659d8b7dab31e819051d85e1dbde8347bc2fdce8",
				blocker->operation_sha256);
			KUNIT_EXPECT_PTR_EQ(test, NULL,
				bcs_rejection_for_ordinal(expected->ordinal, &entry));
		}
	}
}

static void bcs_issue_130_bti_variants_remain_source_blocked(struct kunit *test)
{
	u32 required = ORLIX_TCTI_BASE_CONTROL_FLOW_BRANCH_TYPE_STATE |
		ORLIX_TCTI_BASE_CONTROL_FLOW_GUARDED_PAGE |
		ORLIX_TCTI_BASE_CONTROL_FLOW_TARGET_COMPATIBILITY;
	size_t index;

	KUNIT_ASSERT_EQ(test, 3U,
		ARRAY_SIZE(orlix_tcti_base_control_flow_semantic_blockers));
	for (index = 0;
	     index < ARRAY_SIZE(orlix_tcti_base_control_flow_semantic_blockers);
	     index++) {
		const struct orlix_tcti_base_control_flow_semantic_blocker *blocker =
			&orlix_tcti_base_control_flow_semantic_blockers[index];
		const struct bcs_leaf *leaf = bcs_leaf_for_ordinal(
			blocker->source_ordinal);

		KUNIT_ASSERT_NOT_NULL(test, leaf);
		KUNIT_EXPECT_STREQ(test, leaf->name, blocker->source_id);
		KUNIT_EXPECT_STREQ(test, "FEAT_BTI", blocker->feature);
		KUNIT_EXPECT_TRUE(test, blocker->external_locator[0]);
		KUNIT_EXPECT_EQ(test, 64U,
			strlen(blocker->external_execute_sha256));
		KUNIT_EXPECT_EQ(test, required, blocker->obligations);
		KUNIT_EXPECT_EQ(test,
			ORLIX_TCTI_BASE_CONTROL_FLOW_EXTERNAL_SEMANTICS_UNAVAILABLE,
			blocker->status);
	}
}

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
		expected_reason = ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION;
		expected_status = -EOPNOTSUPP;
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

static void bcs_capture_production_wire(struct kunit *test,
					const struct bcs_leaf *leaf)
{
	struct orlix_tcti_native_capture_session *capture = NULL;
	struct orlix_tcti_native_wire_record wire = {};
	struct orlix_tcti_native_wire_record owner_wire = {};
	struct orlix_tcti_native_wire_record replay_wire = {};
	struct orlix_tcti_native_wire_record copied_wire;
	struct orlix_tcti_native_wire_record tampered_copy;
	struct orlix_tcti_target_proof_ingestion_ledger *ledger;
	struct orlix_tcti_target_proof_ingestion_ledger *other_ledger;
	struct orlix_tcti_target_proof_ingestion_ledger *replay_ledger;
	struct orlix_tcti_target_proof_ingestion_ledger *reusable_ledger;
	struct orlix_tcti_target_proof_ingestion_summary summary;
	struct pt_regs regs;
	struct orlix_tcti_result result;
	struct completion start;
	struct bcs_ingest_racer racers[2] = {};
	struct task_struct *workers[2];
	unsigned long address;
	u32 instruction;
	size_t state_index;
	int ret;

	instruction = bcs_instruction(leaf, true);
	address = bcs_map_program(test, instruction);
	bcs_seed_regs(&regs, address, leaf, true);
	ret = orlix_tcti_sme_state_reset(&current->thread.user_sme,
		ORLIX_TCTI_SVE_MAX_VL_BYTES, true, true, true);
	KUNIT_ASSERT_EQ(test, 0, ret);
	for (state_index = 0; state_index < current->thread.user_sme.za_bytes;
	     state_index++)
		current->thread.user_sme.za[state_index] = (u8)state_index;
	for (state_index = 0; state_index < current->thread.user_sme.zt0_bytes;
	     state_index++)
		current->thread.user_sme.zt0[state_index] =
			(u8)(state_index ^ 0xa5U);
	ret = orlix_tcti_native_capture_begin(
		orlix_tcti_branch_control_production_capture_token(leaf->ordinal),
		leaf->ordinal, ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS,
		&capture);
	KUNIT_EXPECT_EQ(test, 0, ret);
	if (!ret)
		ret = orlix_tcti_native_capture_resume(capture, current, &regs,
			current->mm, &result);
	KUNIT_EXPECT_EQ(test, 0, ret);
	if (!ret)
		ret = orlix_tcti_native_capture_take_wire(capture, &wire);
	KUNIT_EXPECT_EQ(test, 0, ret);
	if (!ret)
		KUNIT_EXPECT_TRUE(test, wire.sealed);
	if (!ret)
		bcs_expect_vector_state_is_canonical(test, &wire);
	/* A copied-looking record without capture provenance earns no credit. */
	ledger = orlix_tcti_target_proof_ingestion_ledger_create(2U);
	KUNIT_ASSERT_NOT_NULL(test, ledger);
	other_ledger = orlix_tcti_target_proof_ingestion_ledger_create(2U);
	KUNIT_ASSERT_NOT_NULL(test, other_ledger);
	if (!ret) {
		bcs_expect_all_capture_selectors_tamper_closed(test, ledger, &wire);
		/* An in-tree caller can flip descriptive metadata, not the capability. */
		wire.production_origin = 1U;
		KUNIT_EXPECT_LT(test,
			orlix_tcti_target_proof_ingest_native(ledger, &wire, NULL), 0);
		KUNIT_EXPECT_FALSE(test, wire.consumed);
		KUNIT_EXPECT_EQ(test, 0U, ledger->native_passed);
		/* Test-owned state can parse a sealed record but cannot mint credit. */
		KUNIT_EXPECT_LT(test,
			orlix_tcti_target_proof_ingest_native(ledger, &wire, NULL), 0);
		KUNIT_EXPECT_FALSE(test, wire.consumed);
		KUNIT_EXPECT_EQ(test, 0U, ledger->native_passed);
		KUNIT_EXPECT_EQ(test, 0U, ledger->count);
		KUNIT_EXPECT_EQ(test, 0U,
			orlix_tcti_target_proof_ingestion_summary(ledger, &summary));
		KUNIT_EXPECT_EQ(test, 0U, summary.accepted_records);

		/* Direct capture has no authority.  The ordinary resume owner claims a
		 * pending capture and mints only after actual gadget execution. */
		orlix_tcti_native_capture_destroy(capture);
		capture = NULL;
		bcs_seed_regs(&regs, address, leaf, true);
		ret = orlix_tcti_native_capture_begin(
			orlix_tcti_branch_control_production_capture_token(leaf->ordinal),
			leaf->ordinal, ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS,
			&capture);
		KUNIT_EXPECT_EQ(test, 0, ret);
		if (!ret) {
			result = orlix_tcti_resume_user(current, &regs, current->mm);
			ret = orlix_tcti_native_capture_take_wire(capture, &owner_wire);
			KUNIT_EXPECT_EQ(test, 0, ret);
			/* Pending ownership survives producer teardown. */
			orlix_tcti_native_capture_destroy(capture);
			capture = NULL;
			/* Records are copyable views.  Alias destruction cannot release the
			 * producer-owned bytes or pending capability of the live record. */
			copied_wire = owner_wire;
			tampered_copy = owner_wire;
			tampered_copy.identity ^= 1U;
			orlix_tcti_native_wire_record_destroy(&tampered_copy);
			KUNIT_EXPECT_NOT_NULL(test, owner_wire.bytes);
			KUNIT_EXPECT_FALSE(test, owner_wire.consumed);
			init_completion(&start);
			for (ret = 0; ret < ARRAY_SIZE(racers); ret++) {
				racers[ret] = (struct bcs_ingest_racer) {
					.start = &start,
					.ledger = ret ? other_ledger : ledger,
					.wire = &owner_wire,
				};
				workers[ret] = kthread_run(bcs_ingest_race, &racers[ret],
					"orlix-proof-race/%d", ret);
				KUNIT_ASSERT_FALSE(test, IS_ERR(workers[ret]));
			}
			complete_all(&start);
			for (ret = 0; ret < ARRAY_SIZE(racers); ret++)
				kthread_stop(workers[ret]);
			KUNIT_EXPECT_TRUE(test, (racers[0].result == 0) !=
						(racers[1].result == 0));
			KUNIT_EXPECT_TRUE(test, owner_wire.consumed);
			KUNIT_EXPECT_EQ(test, 1U, ledger->native_passed +
				other_ledger->native_passed);
			KUNIT_EXPECT_EQ(test, 1U, ledger->count + other_ledger->count);
			/* The producer retirement invalidates every copied view before freeing
			 * backing. A retained alias must reject without a second credit or a
			 * read through its stale bytes pointer. */
			KUNIT_EXPECT_LT(test, orlix_tcti_target_proof_ingest_native(
				ledger, &copied_wire, NULL), 0);
			KUNIT_EXPECT_EQ(test, 1U, ledger->native_passed +
				other_ledger->native_passed);
			orlix_tcti_native_wire_record_destroy(&copied_wire);
			orlix_tcti_native_wire_record_destroy(&copied_wire);
			KUNIT_EXPECT_LT(test, orlix_tcti_target_proof_ingest_native(
				ledger, &copied_wire, NULL), 0);

			/* Replay against the credited ledger must roll back its reservation,
			 * allowing the same valid record to credit the other ledger. */
			replay_ledger = racers[0].result == 0 ? ledger : other_ledger;
			reusable_ledger = racers[0].result == 0 ? other_ledger : ledger;
			bcs_seed_regs(&regs, address, leaf, true);
			ret = orlix_tcti_native_capture_begin(
				orlix_tcti_branch_control_production_capture_token(leaf->ordinal),
				leaf->ordinal, ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS,
				&capture);
			KUNIT_ASSERT_EQ(test, 0, ret);
			result = orlix_tcti_resume_user(current, &regs, current->mm);
			ret = orlix_tcti_native_capture_take_wire(capture, &replay_wire);
			KUNIT_ASSERT_EQ(test, 0, ret);
			KUNIT_EXPECT_LT(test, orlix_tcti_target_proof_ingest_native(
				replay_ledger, &replay_wire, NULL), 0);
			KUNIT_EXPECT_FALSE(test, replay_wire.consumed);
			KUNIT_EXPECT_EQ(test, 0, orlix_tcti_target_proof_ingest_native(
				reusable_ledger, &replay_wire, NULL));
		}
	}
	orlix_tcti_target_proof_ingestion_ledger_destroy(ledger);
	orlix_tcti_target_proof_ingestion_ledger_destroy(other_ledger);
	orlix_tcti_native_wire_record_destroy(&wire);
	orlix_tcti_native_wire_record_destroy(&owner_wire);
	orlix_tcti_native_wire_record_destroy(&replay_wire);
	orlix_tcti_native_capture_destroy(capture);
	orlix_tcti_sme_state_release(&current->thread.user_sme);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(address, PAGE_SIZE));
}

static void bcs_issue_130_legal_encoding_boundaries(struct kunit *test)
{
	static const u32 immediate_fields[] = {
		0U, 1U, BIT(24), BIT(25) - 1U, BIT(25),
		BIT(25) + 1U, BIT(26) - 1U,
	};
	size_t index;

	for (index = 0; index < ARRAY_SIZE(bcs_issue_130_leaves); index++) {
		const struct bcs_issue_130_leaf *expected =
			&bcs_issue_130_leaves[index];
		const struct bcs_leaf *leaf;

		if (expected->disposition != BCS_ISSUE_130_REQUIRED_EL0)
			continue;
		leaf = bcs_leaf_for_ordinal(expected->ordinal);
		KUNIT_ASSERT_NOT_NULL(test, leaf);
		if (leaf->kind == BCS_B || leaf->kind == BCS_BL) {
			size_t immediate_index;

			for (immediate_index = 0;
			     immediate_index < ARRAY_SIZE(immediate_fields);
			     immediate_index++) {
				u32 immediate = immediate_fields[immediate_index];
				struct orlix_tcti_decoded_instruction decoded =
					orlix_tcti_decode_aarch64(leaf->pattern |
							   immediate);
				s64 displacement = immediate & BIT(25) ?
					((s64)immediate - BIT_ULL(26)) * sizeof(u32) :
					(s64)immediate * sizeof(u32);

				KUNIT_EXPECT_EQ(test,
					ORLIX_TCTI_DECODE_UNCONDITIONAL_BRANCH_IMMEDIATE,
					decoded.decode_class);
				KUNIT_EXPECT_EQ(test, displacement, decoded.branch_imm);
				KUNIT_EXPECT_EQ(test, leaf->kind == BCS_BL,
						decoded.link);
			}
		} else {
			u8 rn;

			for (rn = 0; rn < 32; rn++) {
				struct orlix_tcti_decoded_instruction decoded =
					orlix_tcti_decode_aarch64(leaf->pattern |
							   ((u32)rn << 5));

				KUNIT_EXPECT_EQ(test,
					ORLIX_TCTI_DECODE_UNCONDITIONAL_BRANCH_REGISTER,
					decoded.decode_class);
				KUNIT_EXPECT_EQ(test, rn, decoded.rn);
				KUNIT_EXPECT_EQ(test, leaf->kind == BCS_BLR,
						decoded.link);
				KUNIT_EXPECT_EQ(test, leaf->kind == BCS_BR ?
					ORLIX_TCTI_BRANCH_REGISTER_BR :
					leaf->kind == BCS_BLR ?
					ORLIX_TCTI_BRANCH_REGISTER_BLR :
					ORLIX_TCTI_BRANCH_REGISTER_RET,
					decoded.branch_register_op);
			}
		}
	}
}

static void bcs_issue_130_reserved_register_encodings_fail_closed(
	struct kunit *test)
{
	static const u32 reserved[] = {
		0xd61f0400U, 0xd63f0400U, 0xd65f0400U,
	};
	size_t index;

	for (index = 0; index < ARRAY_SIZE(reserved); index++) {
		struct pt_regs regs = {};
		struct pt_regs before;
		struct orlix_tcti_result result;
		unsigned long address;

		KUNIT_ASSERT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
			orlix_tcti_decode_aarch64(reserved[index]).decode_class);
		address = bcs_map_program(test, reserved[index]);
		bcs_seed_regs(&regs, address, &bcs_leaves[0], false);
		before = regs;
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
				result.reason);
		KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, result.status);
		KUNIT_EXPECT_EQ(test, address, result.pc);
		KUNIT_EXPECT_EQ(test, reserved[index], result.instruction);
		KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(address, PAGE_SIZE));
	}
}

static void bcs_issue_130_direct_branch_unmapped_target_production(
	struct kunit *test)
{
	static const u32 patterns[] = { 0x14000000U, 0x94000000U };
	size_t index;

	for (index = 0; index < ARRAY_SIZE(patterns); index++) {
		u32 instruction = patterns[index] | (PAGE_SIZE / sizeof(u32));
		unsigned long address = ksys_mmap_pgoff(
			0, 2 * PAGE_SIZE, PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		struct pt_regs regs = {};
		struct pt_regs before;
		struct orlix_tcti_result result;
		unsigned long target;
		unsigned long expected[ARRAY_SIZE(regs.regs)];
		int ret;

		KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(address));
		target = address + PAGE_SIZE;
		ret = orlix_tcti_write_user_data(current->mm, address, &instruction,
						 sizeof(instruction));
		KUNIT_ASSERT_EQ(test, 0, ret);
		KUNIT_ASSERT_EQ(test, 0, vm_munmap(target, PAGE_SIZE));
		KUNIT_ASSERT_EQ(test, 0,
			sys_mprotect(address, PAGE_SIZE, PROT_READ | PROT_EXEC));
		bcs_seed_regs(&regs, address, &bcs_leaves[0], true);
		before = regs;
		result = orlix_tcti_resume_user(current, &regs, current->mm);

		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_USER_FAULT, result.reason);
		KUNIT_EXPECT_EQ(test, -EFAULT, result.status);
		KUNIT_EXPECT_EQ(test, target, result.fault_address);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_ACCESS_FETCH, result.fault_access);
		KUNIT_EXPECT_EQ(test, target, result.pc);
		KUNIT_EXPECT_EQ(test, target, regs.pc);
		KUNIT_EXPECT_EQ(test, 0U, result.instruction);
		KUNIT_EXPECT_EQ(test, before.pstate, regs.pstate);
		memcpy(expected, before.regs, sizeof(expected));
		if (patterns[index] & BIT(31))
			expected[30] = address + sizeof(u32);
		KUNIT_EXPECT_MEMEQ(test, expected, regs.regs, sizeof(expected));
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(address, PAGE_SIZE));
	}
}

static void bcs_issue_130_typed_native_production_records(struct kunit *test)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(bcs_issue_130_leaves); index++) {
		const struct bcs_issue_130_leaf *expected =
			&bcs_issue_130_leaves[index];
		const struct bcs_leaf *leaf;
		struct pt_regs regs;
		struct pt_regs before;
		struct orlix_tcti_result result;
		unsigned long address;

		if (expected->disposition != BCS_ISSUE_130_REQUIRED_EL0)
			continue;
		leaf = bcs_leaf_for_ordinal(expected->ordinal);
		KUNIT_ASSERT_NOT_NULL(test, leaf);
		address = bcs_map_program(test, bcs_instruction(leaf, true));
		bcs_seed_regs(&regs, address, leaf, true);
		before = regs;
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_ASSERT_EQ_MSG(test, ORLIX_TCTI_EXIT_SYSCALL,
			result.reason, "%s ordinal %u", expected->name,
			expected->ordinal);
		KUNIT_EXPECT_EQ(test, BCS_SVC_TAKEN, result.instruction);
		KUNIT_EXPECT_EQ(test, address + 3 * sizeof(u32), regs.pc);
		KUNIT_EXPECT_EQ(test, before.pstate, regs.pstate);
		bcs_expect_register_effects(test, leaf, &before, &regs, address);
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(address, PAGE_SIZE));
	}
}

static void bcs_issue_130_typed_native_fault_records(struct kunit *test)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(bcs_issue_130_leaves); index++) {
		const struct bcs_issue_130_leaf *expected =
			&bcs_issue_130_leaves[index];
		const struct bcs_leaf *leaf;
		struct pt_regs regs;
		struct pt_regs before;
		struct orlix_tcti_result result;
		u32 instruction;
		unsigned long address;
		unsigned long target;
		unsigned long expected_regs[ARRAY_SIZE(regs.regs)];
		int ret;

		if (expected->disposition != BCS_ISSUE_130_REQUIRED_EL0)
			continue;
		leaf = bcs_leaf_for_ordinal(expected->ordinal);
		KUNIT_ASSERT_NOT_NULL(test, leaf);
		address = ksys_mmap_pgoff(
			0, 2 * PAGE_SIZE, PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(address));
		target = address + PAGE_SIZE;
		instruction = leaf->kind == BCS_B || leaf->kind == BCS_BL ?
			leaf->pattern | (PAGE_SIZE / sizeof(u32)) :
			leaf->pattern | (5U << 5);
		ret = orlix_tcti_write_user_data(current->mm, address, &instruction,
						 sizeof(instruction));
		KUNIT_ASSERT_EQ(test, 0, ret);
		KUNIT_ASSERT_EQ(test, 0, vm_munmap(target, PAGE_SIZE));
		KUNIT_ASSERT_EQ(test, 0,
			sys_mprotect(address, PAGE_SIZE, PROT_READ | PROT_EXEC));
		bcs_seed_regs(&regs, address, leaf, true);
		regs.regs[5] = target;
		before = regs;
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_USER_FAULT,
			result.reason, "%s ordinal %u", expected->name,
			expected->ordinal);
		KUNIT_EXPECT_EQ(test, -EFAULT, result.status);
		KUNIT_EXPECT_EQ(test, target, result.fault_address);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_ACCESS_FETCH, result.fault_access);
		KUNIT_EXPECT_EQ(test, target, result.pc);
		KUNIT_EXPECT_EQ(test, target, regs.pc);
		KUNIT_EXPECT_EQ(test, 0U, result.instruction);
		KUNIT_EXPECT_EQ(test, before.pstate, regs.pstate);
		memcpy(expected_regs, before.regs, sizeof(expected_regs));
		if (leaf->kind == BCS_BL || leaf->kind == BCS_BLR)
			expected_regs[30] = address + sizeof(u32);
		KUNIT_EXPECT_MEMEQ(test, expected_regs, regs.regs,
			 sizeof(expected_regs));
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(address, PAGE_SIZE));
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
	bcs_capture_production_wire(test, &bcs_leaves[0]);
}

static void bcs_unsupported_resume_cannot_credit(struct kunit *test)
{
	struct orlix_tcti_native_capture_session *capture = NULL;
	struct orlix_tcti_native_wire_record wire = {};
	struct pt_regs regs = {};
	struct orlix_tcti_result result;
	const struct bcs_leaf *leaf = &bcs_leaves[0];
	unsigned long address;
	int ret;

	/* A normal-resume claim alone is insufficient: the instruction must reach
	 * the successful gadget path before its pending capture becomes creditable. */
	address = bcs_map_program(test, 0x74004000U);
	bcs_seed_regs(&regs, address, leaf, false);
	ret = orlix_tcti_native_capture_begin(
		orlix_tcti_branch_control_production_capture_token(leaf->ordinal),
		leaf->ordinal, ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS,
		&capture);
	KUNIT_ASSERT_EQ(test, 0, ret);
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
		result.reason);
	KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, result.status);
	KUNIT_EXPECT_LT(test,
		orlix_tcti_native_capture_take_wire(capture, &wire), 0);
	KUNIT_EXPECT_FALSE(test, wire.sealed);
	orlix_tcti_native_wire_record_destroy(&wire);
	orlix_tcti_native_capture_destroy(capture);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(address, PAGE_SIZE));
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
	KUNIT_CASE(bcs_issue_130_exact_cohort_accounting),
	KUNIT_CASE(bcs_issue_130_bti_variants_remain_source_blocked),
	KUNIT_CASE(bcs_source_decode),
	KUNIT_CASE(bcs_issue_130_legal_encoding_boundaries),
	KUNIT_CASE(bcs_issue_130_reserved_register_encodings_fail_closed),
	KUNIT_CASE(bcs_production_resume),
	KUNIT_CASE(bcs_unsupported_resume_cannot_credit),
	KUNIT_CASE(bcs_x31_semantics_production),
	KUNIT_CASE(bcs_register_branch_unaligned_target_production),
	KUNIT_CASE(bcs_register_branch_out_of_range_target_production),
	KUNIT_CASE(bcs_issue_130_direct_branch_unmapped_target_production),
	KUNIT_CASE(bcs_issue_130_typed_native_production_records),
	KUNIT_CASE(bcs_issue_130_typed_native_fault_records),
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
