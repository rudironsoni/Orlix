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

static u32 native_capture_wire_get32(const u8 *bytes)
{
	return (u32)bytes[0] | ((u32)bytes[1] << 8) |
		((u32)bytes[2] << 16) | ((u32)bytes[3] << 24);
}

static u64 native_capture_wire_get64(const u8 *bytes)
{
	u64 value = 0;
	size_t index;

	for (index = 0; index < sizeof(value); index++)
		value |= (u64)bytes[index] << (index * 8U);
	return value;
}

static bool native_capture_copy_text(char *destination, size_t capacity,
	const char *source)
{
	if (!destination || !capacity || !source || !source[0])
		return false;
	return strscpy(destination, source, capacity) >= 0;
}

static void native_capture_expect_ledger_state(struct kunit *test,
	const struct orlix_tcti_target_proof_ingestion_ledger *ledger,
	const struct orlix_tcti_target_proof_ingestion_summary *expected,
	const struct orlix_tcti_target_proof_ingestion_slot *expected_slot)
{
	struct orlix_tcti_target_proof_ingestion_summary observed;

	KUNIT_EXPECT_EQ(test, 0,
		orlix_tcti_target_proof_ingestion_summary(ledger, &observed));
	KUNIT_EXPECT_EQ(test, expected->accepted_records, observed.accepted_records);
	KUNIT_EXPECT_EQ(test, expected->native_passed, observed.native_passed);
	KUNIT_EXPECT_EQ(test, expected->kselftest_passed, observed.kselftest_passed);
	KUNIT_EXPECT_EQ(test, expected->rejected, observed.rejected);
	KUNIT_EXPECT_EQ(test, expected->accepted_records, ledger->count);
	KUNIT_EXPECT_EQ(test, expected->native_passed, ledger->native_passed);
	KUNIT_EXPECT_MEMEQ(test, expected_slot, ledger->slots, sizeof(*expected_slot));
}

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

static void capture_ignores_successful_nonmatching_instruction(
	struct kunit *test)
{
	struct orlix_tcti_native_capture_session *session = NULL;
	struct orlix_tcti_native_capture *capture;
	struct orlix_tcti_native_wire_record wire = {};
	struct orlix_tcti_decoded_instruction target = {
		.instruction = NATIVE_CAPTURE_SVC,
		.decode_class = ORLIX_TCTI_DECODE_SVC,
	};
	struct orlix_tcti_decoded_instruction ordinary = {
		.instruction = 0x8b000000U,
		.decode_class = ORLIX_TCTI_DECODE_ADD_SUB_SHIFTED_REGISTER,
	};
	struct pt_regs regs;
	struct orlix_tcti_result result = {
		.reason = ORLIX_TCTI_EXIT_SYSCALL,
		.status = 0,
		.instruction = NATIVE_CAPTURE_SVC,
	};

	native_capture_seed_regs(&regs, 0x1000U);
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_native_capture_begin(
		orlix_tcti_branch_control_production_capture_token(2227U), 2227U,
		ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS, &session));
	capture = orlix_tcti_native_capture_claim_resume(current);
	KUNIT_ASSERT_NOT_NULL(test, capture);
	/* The target is observed first, then an ordinary successful instruction
	 * reaches the same after-callback in the enclosing gadget block. */
	orlix_tcti_native_capture_before_decoded(capture, current->mm, &regs,
		&target);
	orlix_tcti_native_capture_after_decoded(capture, current->mm, &regs,
		&target);
	orlix_tcti_native_capture_before_decoded(capture, current->mm, &regs,
		&ordinary);
	orlix_tcti_native_capture_after_decoded(capture, current->mm, &regs,
		&ordinary);
	orlix_tcti_native_capture_exit(capture, &result, &regs);
	orlix_tcti_native_capture_finalize(capture, &result, &regs);
	KUNIT_EXPECT_EQ(test, 0,
		orlix_tcti_native_capture_take_wire(session, &wire));
	KUNIT_EXPECT_TRUE(test, wire.sealed);
	orlix_tcti_native_wire_record_destroy(&wire);
	orlix_tcti_native_capture_destroy(session);
}

static void capture_ignores_nonmatching_fault_instruction(
	struct kunit *test)
{
	struct orlix_tcti_native_capture_session *session = NULL;
	struct orlix_tcti_native_capture *capture;
	struct orlix_tcti_native_wire_record wire = {};
	struct orlix_tcti_decoded_instruction target = {
		.instruction = NATIVE_CAPTURE_SVC,
		.decode_class = ORLIX_TCTI_DECODE_SVC,
	};
	struct orlix_tcti_decoded_instruction ordinary = {
		.instruction = 0x8b000000U,
		.decode_class = ORLIX_TCTI_DECODE_ADD_SUB_SHIFTED_REGISTER,
	};
	struct pt_regs regs;
	struct orlix_tcti_result result = {
		.reason = ORLIX_TCTI_EXIT_USER_FAULT,
		.status = -EFAULT,
		.instruction = NATIVE_CAPTURE_SVC,
	};

	native_capture_seed_regs(&regs, 0x1000U);
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_native_capture_begin(
		orlix_tcti_branch_control_production_capture_token(2227U), 2227U,
		ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS, &session));
	capture = orlix_tcti_native_capture_claim_resume(current);
	KUNIT_ASSERT_NOT_NULL(test, capture);
	/* A later non-target fault in the same gadget is not target evidence. */
	orlix_tcti_native_capture_before_decoded(capture, current->mm, &regs,
		&target);
	orlix_tcti_native_capture_after_decoded(capture, current->mm, &regs,
		&target);
	orlix_tcti_native_capture_fault(capture, &ordinary, 0x2000U, -EFAULT);
	orlix_tcti_native_capture_exit(capture, &result, &regs);
	orlix_tcti_native_capture_finalize(capture, &result, &regs);
	KUNIT_EXPECT_EQ(test, 0,
		orlix_tcti_native_capture_take_wire(session, &wire));
	KUNIT_EXPECT_TRUE(test, wire.sealed);
	orlix_tcti_native_wire_record_destroy(&wire);
	orlix_tcti_native_capture_destroy(session);
}

static void capture_rejects_mismatched_system_accessor_variant(
	struct kunit *test)
{
	const struct orlix_tcti_native_proof_registry_entry *entries;
	const struct orlix_tcti_native_proof_registry_entry *target = NULL;
	struct orlix_tcti_native_capture_session *session = NULL;
	struct orlix_tcti_native_capture *capture;
	struct orlix_tcti_native_wire_record wire = {};
	struct orlix_tcti_target_proof_ingestion_ledger *ledger;
	struct orlix_tcti_target_proof_ingestion_summary summary = {};
	struct orlix_tcti_target_proof_ingestion_slot slot = {};
	struct orlix_tcti_decoded_instruction wrong;
	struct orlix_tcti_result result = {
		.reason = ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
		.status = -EOPNOTSUPP,
		.instruction = 0xd53bd040U,
	};
	struct pt_regs regs;
	const void *capture_token;
	size_t count;
	size_t index;

	entries = orlix_tcti_native_proof_registry_entries(&count);
	KUNIT_ASSERT_NOT_NULL(test, entries);
	for (index = 0; index < count; index++) {
		if (entries[index].production_capture &&
		    entries[index].source.subject_kind ==
			ORLIX_TCTI_NATIVE_SUBJECT_SEMANTIC_VARIANT &&
		    !strcmp(entries[index].source.semantic_variant, "FPMR")) {
			target = &entries[index];
			break;
		}
	}
	KUNIT_ASSERT_NOT_NULL(test, target);
	capture_token =
		orlix_tcti_native_proof_registry_capture_token_for_entry(target);
	KUNIT_ASSERT_NOT_NULL(test, capture_token);
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_native_capture_begin(capture_token,
		target->source.source_ordinal, target->obligation, &session));
	capture = orlix_tcti_native_capture_claim_resume(current);
	KUNIT_ASSERT_NOT_NULL(test, capture);

	/* TPIDR_EL0 uses the same generic MRS mask as FPMR.  Decode its
	 * generated selector and prove that the producer cannot credit FPMR. */
	wrong = orlix_tcti_decode_aarch64(0xd53bd040U);
	native_capture_seed_regs(&regs, 0x1000U);
	orlix_tcti_native_capture_before_decoded(capture, current->mm, &regs,
		&wrong);
	orlix_tcti_native_capture_after_decoded(capture, current->mm, &regs,
		&wrong);
	orlix_tcti_native_capture_exit(capture, &result, &regs);
	orlix_tcti_native_capture_finalize(capture, &result, &regs);
	KUNIT_EXPECT_LT(test,
		orlix_tcti_native_capture_take_wire(session, &wire), 0);
	KUNIT_EXPECT_FALSE(test, wire.sealed);

	ledger = orlix_tcti_target_proof_ingestion_ledger_create(1U);
	KUNIT_ASSERT_NOT_NULL(test, ledger);
	KUNIT_EXPECT_EQ(test, 0,
		orlix_tcti_target_proof_ingestion_summary(ledger, &summary));
	KUNIT_EXPECT_MEMEQ(test, &slot, ledger->slots, sizeof(slot));
	KUNIT_EXPECT_EQ(test, 0U, ledger->count);
	KUNIT_EXPECT_EQ(test, 0U, ledger->native_passed);
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

static void production_capture_seals_explicit_tls_after_state(struct kunit *test)
{
	struct orlix_tcti_native_capture_session *session = NULL;
	struct orlix_tcti_native_wire_record wire = {};
	struct orlix_tcti_target_proof_ingestion_ledger *ledger = NULL;
	const struct orlix_tcti_native_proof_registry_entry *entry = NULL;
	const void *capture_token =
		orlix_tcti_branch_control_production_capture_token(2227U);
	struct orlix_tcti_target_proof_ingestion_summary empty_summary = {};
	struct orlix_tcti_target_proof_ingestion_summary accepted_summary = {
		.accepted_records = 1U,
		.native_passed = 1U,
	};
	struct orlix_tcti_target_proof_ingestion_slot empty_slot;
	struct orlix_tcti_target_proof_ingestion_slot accepted_slot;
	struct pt_regs regs;
	struct orlix_tcti_result result;
	u8 *bytes;
	const u8 *payload;
	u64 saved_tls = current->thread.user_tls;
	u64 saved_fpmr = current->thread.user_fpmr;
	u64 semantic_variant_identity;
	const u64 tls_after = 0x123456789abcdef0ULL;
	const u64 fpmr_after = 0x0fedcba987654321ULL;
	unsigned long address;
	size_t cursor = 72U;
	size_t index;
	bool found = false;

	address = native_capture_map_svc(test);
	native_capture_seed_regs(&regs, address);
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_native_capture_begin(
		capture_token, 2227U,
		ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS, &session));
	current->thread.user_tls = tls_after;
	current->thread.user_fpmr = fpmr_after;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	if (orlix_tcti_native_capture_take_wire(session, &wire))
		goto out;
	if (orlix_tcti_native_proof_registry_capture_token_semantic_variant_identity(
		capture_token, &semantic_variant_identity) ||
	    orlix_tcti_native_proof_registry_resolve_production(capture_token, 2227U,
		semantic_variant_identity, ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS,
		&entry, NULL) || !entry) {
		KUNIT_FAIL(test, "could not resolve canonical production capture row");
		goto out;
	}
	memset(&accepted_slot, 0, sizeof(accepted_slot));
	accepted_slot.identity = wire.identity;
	accepted_slot.registry_identity =
		orlix_tcti_native_proof_registry_entry_identity(entry);
	accepted_slot.source_ordinal = entry->source.source_ordinal;
	accepted_slot.semantic_variant_identity =
		entry->source.semantic_variant_identity;
	accepted_slot.obligation = entry->obligation;
	accepted_slot.native = true;
	if (!native_capture_copy_text(accepted_slot.proof_id,
		sizeof(accepted_slot.proof_id), entry->proof_id) ||
	    !native_capture_copy_text(accepted_slot.kunit_suite,
		sizeof(accepted_slot.kunit_suite), entry->kunit_suite) ||
	    !native_capture_copy_text(accepted_slot.kunit_case,
		sizeof(accepted_slot.kunit_case), entry->kunit_case)) {
		KUNIT_FAIL(test, "could not build expected accepted ledger slot");
		goto out;
	}
	bytes = (u8 *)wire.bytes;
	for (index = 0; index < native_capture_wire_get32(bytes + 52U); index++) {
		u32 kind = native_capture_wire_get32(bytes + cursor);
		u32 length = native_capture_wire_get32(bytes + cursor + 12U);

		if (kind == ORLIX_TCTI_NATIVE_CAPTURE_SELECTOR_SYSTEM_CONTROL) {
			found = true;
			payload = bytes + cursor + 16U;
			KUNIT_EXPECT_EQ(test, 40U, length);
			KUNIT_EXPECT_EQ(test, 0U, native_capture_wire_get32(bytes + cursor + 16U));
			KUNIT_EXPECT_EQ(test, 0U, native_capture_wire_get32(bytes + cursor + 20U));
			KUNIT_EXPECT_EQ(test, regs.pstate,
				native_capture_wire_get64(payload + 16U));
			KUNIT_EXPECT_EQ(test, tls_after,
				native_capture_wire_get64(payload + 24U));
			KUNIT_EXPECT_EQ(test, fpmr_after,
				native_capture_wire_get64(payload + 32U));
			/* Size, record identity, TLS, and FPMR bytes must fail closed. */
			bytes[cursor + 12U] = 24U;
			ledger = orlix_tcti_target_proof_ingestion_ledger_create(1U);
			if (!ledger)
				break;
			memcpy(&empty_slot, ledger->slots, sizeof(empty_slot));
			KUNIT_EXPECT_LT(test, orlix_tcti_target_proof_ingest_native(ledger,
				&wire, NULL), 0);
			KUNIT_EXPECT_FALSE(test, wire.consumed);
			native_capture_expect_ledger_state(test, ledger, &empty_summary,
				&empty_slot);
			bytes[cursor + 12U] = 40U;
			wire.identity ^= 1U;
			KUNIT_EXPECT_LT(test, orlix_tcti_target_proof_ingest_native(ledger,
				&wire, NULL), 0);
			KUNIT_EXPECT_FALSE(test, wire.consumed);
			native_capture_expect_ledger_state(test, ledger, &empty_summary,
				&empty_slot);
			wire.identity ^= 1U;
			bytes[cursor + 40U] ^= 0x80U;
			KUNIT_EXPECT_LT(test, orlix_tcti_target_proof_ingest_native(ledger,
				&wire, NULL), 0);
			KUNIT_EXPECT_FALSE(test, wire.consumed);
			native_capture_expect_ledger_state(test, ledger, &empty_summary,
				&empty_slot);
			bytes[cursor + 40U] ^= 0x80U;
			bytes[cursor + 48U] ^= 0x80U;
			KUNIT_EXPECT_LT(test, orlix_tcti_target_proof_ingest_native(ledger,
				&wire, NULL), 0);
			KUNIT_EXPECT_FALSE(test, wire.consumed);
			native_capture_expect_ledger_state(test, ledger, &empty_summary,
				&empty_slot);
			bytes[cursor + 48U] ^= 0x80U;
			KUNIT_EXPECT_EQ(test, 0, orlix_tcti_target_proof_ingest_native(ledger,
				&wire, NULL));
			KUNIT_EXPECT_TRUE(test, wire.consumed);
			native_capture_expect_ledger_state(test, ledger, &accepted_summary,
				&accepted_slot);
			KUNIT_EXPECT_LT(test, orlix_tcti_target_proof_ingest_native(ledger,
				&wire, NULL), 0);
			KUNIT_EXPECT_TRUE(test, wire.consumed);
			native_capture_expect_ledger_state(test, ledger, &accepted_summary,
				&accepted_slot);
			orlix_tcti_target_proof_ingestion_ledger_destroy(ledger);
			ledger = NULL;
			break;
		}
		cursor += 16U + length;
	}
	KUNIT_EXPECT_TRUE(test, found);
out:
	if (ledger)
		orlix_tcti_target_proof_ingestion_ledger_destroy(ledger);
	current->thread.user_tls = saved_tls;
	current->thread.user_fpmr = saved_fpmr;
	orlix_tcti_native_wire_record_destroy(&wire);
	orlix_tcti_native_capture_destroy(session);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(address, PAGE_SIZE));
}

static struct kunit_case native_capture_production_test_cases[] = {
	KUNIT_CASE(capture_binds_only_its_registered_row),
	KUNIT_CASE(production_capture_variant_authority_is_exact),
	KUNIT_CASE(generic_capture_events_cannot_credit_a_claimed_session),
	KUNIT_CASE(capture_ignores_successful_nonmatching_instruction),
	KUNIT_CASE(capture_ignores_nonmatching_fault_instruction),
	KUNIT_CASE(capture_rejects_mismatched_system_accessor_variant),
	KUNIT_CASE(pending_capacity_exhaustion_preserves_live_wires),
	KUNIT_CASE(production_capture_seals_explicit_tls_after_state),
	{}
};
static struct kunit_suite native_capture_production_test_suite = {
	.name = "orlix-tcti-native-capture-production",
	.test_cases = native_capture_production_test_cases,
};
kunit_test_suite(native_capture_production_test_suite);
MODULE_LICENSE("GPL");
