// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/syscalls.h>

#include <asm/ptrace.h>
#include <asm/processor.h>

#include "../decode_aarch64.h"
#include "../native_capture.h"
#include "orlix_tcti_branch_control_production_capture.h"
#include "orlix_tcti_native_observation.h"
#include "target_native_proof_contract_private.h"
#include "target_native_proof_registry_private.h"
#include "target_proof_ingestion_private.h"

#define NATIVE_CAPTURE_SVC 0xd4024681U
#define NATIVE_CAPTURE_PENDING_CAPACITY 128U

static unsigned long native_capture_map_svc(struct kunit *test)
{
	unsigned long address;
	u32 instruction = NATIVE_CAPTURE_SVC;

	address = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(address));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_write_user_data(current->mm, address,
		&instruction, sizeof(instruction)));
	KUNIT_ASSERT_EQ(test, 0, sys_mprotect(address, PAGE_SIZE,
		PROT_READ | PROT_EXEC));
	return address;
}

static void native_capture_seed_regs(struct pt_regs *regs, unsigned long address)
{
	memset(regs, 0, sizeof(*regs));
	regs->pc = address;
	regs->pstate = PSR_MODE_EL0t;
	regs->syscallno = NO_SYSCALL;
}

static void capture_binds_only_its_registered_row(struct kunit *test)
{
	struct orlix_tcti_native_capture_session *session = NULL;
	const void *token =
		orlix_tcti_branch_control_production_capture_token(2227U);

	KUNIT_ASSERT_NOT_NULL(test, token);
	KUNIT_EXPECT_EQ(test, 0, orlix_tcti_native_capture_begin(token, 2227U,
		ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS, &session));
	KUNIT_ASSERT_NOT_NULL(test, session);
	KUNIT_EXPECT_EQ(test, -EPERM, orlix_tcti_native_capture_begin(token, 2230U,
		ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS, &session));
	orlix_tcti_native_capture_destroy(session);
}

static void production_capture_variant_authority_is_exact(struct kunit *test)
{
	static const struct orlix_tcti_native_proof_registry_entry variants[] = {
		{ .source = { .source_ordinal = 77U,
			.semantic_variant_identity = 0x1111111111111111ULL },
		  .obligation = ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS,
		  .production_capture = 1U },
		{ .source = { .source_ordinal = 77U,
			.semantic_variant_identity = 0x2222222222222222ULL },
		  .obligation = ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS,
		  .production_capture = 1U },
		{ .source = { .source_ordinal = 78U,
			.semantic_variant_identity = 0U },
		  .obligation = ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC,
		  .production_capture = 1U },
	};
	struct orlix_tcti_native_proof_registry_entry copied = variants[0];
	const struct orlix_tcti_native_proof_registry_entry *entry = NULL;
	enum orlix_tcti_native_contract_error error;

	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_native_proof_registry_resolve_production_entries(variants,
			ARRAY_SIZE(variants), &variants[0], 77U,
			0x1111111111111111ULL,
			ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS, &entry, &error));
	KUNIT_EXPECT_PTR_EQ(test, &variants[0], entry);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_NATIVE_CONTRACT_OK, error);
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_native_proof_registry_resolve_production_entries(variants,
			ARRAY_SIZE(variants), &variants[1], 77U,
			0x2222222222222222ULL,
			ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS, &entry, &error));
	KUNIT_EXPECT_PTR_EQ(test, &variants[1], entry);

	KUNIT_EXPECT_LT(test,
		orlix_tcti_native_proof_registry_resolve_production_entries(variants,
			ARRAY_SIZE(variants), &variants[0], 77U,
			0x2222222222222222ULL,
			ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS, &entry, &error), 0);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_NATIVE_CONTRACT_UNKNOWN, error);
	KUNIT_EXPECT_LT(test,
		orlix_tcti_native_proof_registry_resolve_production_entries(variants,
			ARRAY_SIZE(variants), &variants[0], 77U, 0U,
			ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS, &entry, &error), 0);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_NATIVE_CONTRACT_UNKNOWN, error);
	KUNIT_EXPECT_LT(test,
		orlix_tcti_native_proof_registry_resolve_production_entries(variants,
			ARRAY_SIZE(variants), &copied, 77U,
			0x1111111111111111ULL,
			ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS, &entry, &error), 0);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_NATIVE_CONTRACT_UNKNOWN, error);
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_native_proof_registry_resolve_production_entries(variants,
			ARRAY_SIZE(variants), &variants[2], 78U, 0U,
			ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC, &entry, &error));
	KUNIT_EXPECT_PTR_EQ(test, &variants[2], entry);
}

static void generic_capture_events_cannot_credit_a_claimed_session(
	struct kunit *test)
{
	struct orlix_tcti_native_capture_session *session = NULL;
	struct orlix_tcti_native_capture *capture;
	struct orlix_tcti_native_wire_record wire = {};
	struct orlix_tcti_target_proof_ingestion_ledger *ledger;
	struct orlix_tcti_decoded_instruction decoded = {
		.instruction = NATIVE_CAPTURE_SVC,
		.decode_class = ORLIX_TCTI_DECODE_HINT,
	};
	struct pt_regs regs;
	struct orlix_tcti_result result = {
		.reason = ORLIX_TCTI_EXIT_SYSCALL,
		.status = 0,
	};

	native_capture_seed_regs(&regs, 0x1000U);
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_native_capture_begin(
		orlix_tcti_branch_control_production_capture_token(2227U), 2227U,
		ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS, &session));
	capture = orlix_tcti_native_capture_claim_resume(current);
	KUNIT_ASSERT_NOT_NULL(test, capture);
	/* This is every generic event available to a caller that claims capture.
	 * No gadget ran, so their observational effects cannot credit the wire. */
	orlix_tcti_native_capture_before_decoded(capture, current->mm, &regs,
		&decoded);
	orlix_tcti_native_capture_after_decoded(capture, current->mm, &regs,
		&decoded);
	orlix_tcti_native_capture_fault(capture, &decoded, 0, 0);
	orlix_tcti_native_capture_exit(capture, &result, &regs);
	orlix_tcti_native_capture_execution_succeeded(capture);
	orlix_tcti_native_capture_finalize(capture, &result, &regs);
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_native_capture_take_wire(session,
		&wire));
	ledger = orlix_tcti_target_proof_ingestion_ledger_create(1U);
	KUNIT_ASSERT_NOT_NULL(test, ledger);
	KUNIT_EXPECT_LT(test, orlix_tcti_target_proof_ingest_native(ledger,
		&wire, NULL), 0);
	orlix_tcti_target_proof_ingestion_ledger_destroy(ledger);
	orlix_tcti_native_wire_record_destroy(&wire);
	orlix_tcti_native_capture_destroy(session);
}

static void pending_capacity_exhaustion_preserves_live_wires(
	struct kunit *test)
{
	struct orlix_tcti_native_capture_session *session = NULL;
	struct orlix_tcti_native_wire_record *wires;
	struct orlix_tcti_native_wire_record copied_wire;
	struct orlix_tcti_native_wire_record failed_wire = {};
	struct orlix_tcti_target_proof_ingestion_ledger *ledger;
	struct pt_regs regs;
	struct orlix_tcti_result result;
	unsigned long address;
	size_t index;
	int ret;

	address = native_capture_map_svc(test);
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_sme_state_reset(&current->thread.user_sme,
		ORLIX_TCTI_SVE_MAX_VL_BYTES, true, true, true));
	wires = kcalloc(NATIVE_CAPTURE_PENDING_CAPACITY, sizeof(*wires), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, wires);
	for (index = 0; index < NATIVE_CAPTURE_PENDING_CAPACITY; index++) {
		native_capture_seed_regs(&regs, address);
		ret = orlix_tcti_native_capture_begin(
			orlix_tcti_branch_control_production_capture_token(2227U), 2227U,
			ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS, &session);
		KUNIT_ASSERT_EQ(test, 0, ret);
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_ASSERT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
		KUNIT_ASSERT_EQ(test, 0,
			orlix_tcti_native_capture_take_wire(session, &wires[index]));
		KUNIT_ASSERT_TRUE(test, wires[index].sealed);
		orlix_tcti_native_capture_destroy(session);
		session = NULL;
	}
	ledger = orlix_tcti_target_proof_ingestion_ledger_create(1U);
	KUNIT_ASSERT_NOT_NULL(test, ledger);
	/* The 129th ordinary capture is rejected before it can replace a live
	 * pending owner. Its caller receives no sealed wire and no ledger changes. */
	native_capture_seed_regs(&regs, address);
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_native_capture_begin(
		orlix_tcti_branch_control_production_capture_token(2227U), 2227U,
		ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS, &session));
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_LT(test, orlix_tcti_native_capture_take_wire(session,
		&failed_wire), 0);
	KUNIT_EXPECT_FALSE(test, failed_wire.sealed);
	KUNIT_EXPECT_EQ(test, 0U, ledger->count);
	KUNIT_EXPECT_EQ(test, 0U, ledger->native_passed);
	orlix_tcti_native_wire_record_destroy(&failed_wire);
	orlix_tcti_native_capture_destroy(session);
	session = NULL;

	/* A copied view cannot revoke or free the first live record. The owner
	 * record still credits exactly once after table exhaustion. */
	copied_wire = wires[0];
	orlix_tcti_native_wire_record_destroy(&copied_wire);
	KUNIT_EXPECT_NOT_NULL(test, wires[0].bytes);
	KUNIT_EXPECT_EQ(test, 0, orlix_tcti_target_proof_ingest_native(ledger,
		&wires[0], NULL));
	KUNIT_EXPECT_TRUE(test, wires[0].consumed);
	KUNIT_EXPECT_EQ(test, 1U, ledger->count);
	orlix_tcti_target_proof_ingestion_ledger_destroy(ledger);

	/* A different retained slot remains ingestible. Consume each remaining
	 * slot into its own ledger so this test leaves no global pending state. */
	for (index = 1U; index < NATIVE_CAPTURE_PENDING_CAPACITY; index++) {
		ledger = orlix_tcti_target_proof_ingestion_ledger_create(1U);
		KUNIT_ASSERT_NOT_NULL(test, ledger);
		KUNIT_EXPECT_EQ(test, 0, orlix_tcti_target_proof_ingest_native(ledger,
			&wires[index], NULL));
		KUNIT_EXPECT_TRUE(test, wires[index].consumed);
		orlix_tcti_target_proof_ingestion_ledger_destroy(ledger);
	}
	for (index = 0; index < NATIVE_CAPTURE_PENDING_CAPACITY; index++)
		orlix_tcti_native_wire_record_destroy(&wires[index]);
	kfree(wires);
	orlix_tcti_sme_state_release(&current->thread.user_sme);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(address, PAGE_SIZE));
}

static struct kunit_case native_capture_production_test_cases[] = {
	KUNIT_CASE(capture_binds_only_its_registered_row),
	KUNIT_CASE(production_capture_variant_authority_is_exact),
	KUNIT_CASE(generic_capture_events_cannot_credit_a_claimed_session),
	KUNIT_CASE(pending_capacity_exhaustion_preserves_live_wires),
	{}
};
static struct kunit_suite native_capture_production_test_suite = {
	.name = "orlix-tcti-native-capture-production",
	.test_cases = native_capture_production_test_cases,
};
kunit_test_suite(native_capture_production_test_suite);
MODULE_LICENSE("GPL");
