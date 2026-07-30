// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/errno.h>

#include "orlix_tcti_native_observation.h"
#include "target_native_proof_contract_private.h"
#include "target_proof_ingestion.h"

static void capture_requires_a_registry_issued_token(struct kunit *test)
{
	struct orlix_tcti_native_capture_session *session = NULL;

	KUNIT_EXPECT_EQ(test, -EPERM, orlix_tcti_native_capture_begin(NULL,
		2227U, ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS, &session));
	KUNIT_EXPECT_NULL(test, session);
}

static void private_wire_view_cannot_mint_a_pending_capability(struct kunit *test)
{
	struct orlix_tcti_native_wire_record record = {};
	struct orlix_tcti_target_proof_ingestion_ledger *ledger;

	/* Private headers expose only a copyable view and portable validation.
	 * They intentionally provide no builder, pending slot, or credit primitive. */
	ledger = orlix_tcti_target_proof_ingestion_ledger_create(1U);
	KUNIT_ASSERT_NOT_NULL(test, ledger);
	KUNIT_EXPECT_LT(test, orlix_tcti_target_proof_ingest_native(ledger,
		&record, NULL), 0);
	KUNIT_EXPECT_FALSE(test, record.consumed);
	orlix_tcti_target_proof_ingestion_ledger_destroy(ledger);
}

static struct kunit_case orlix_tcti_native_observation_test_cases[] = {
	KUNIT_CASE(capture_requires_a_registry_issued_token),
	KUNIT_CASE(private_wire_view_cannot_mint_a_pending_capability),
	{}
};

static struct kunit_suite orlix_tcti_native_observation_test_suite = {
	.name = "orlix-tcti-native-capture",
	.test_cases = orlix_tcti_native_observation_test_cases,
};
kunit_test_suite(orlix_tcti_native_observation_test_suite);
MODULE_LICENSE("GPL");
