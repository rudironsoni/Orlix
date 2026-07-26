/* SPDX-License-Identifier: GPL-2.0-only */
#include <kunit/test.h>

#include "../decode_aarch64.h"
#include "../sme_state_contract.h"

static void orlix_tcti_sme_pstate_contract_covers_each_decoder_alias(struct kunit *test)
{
	size_t index;

	KUNIT_EXPECT_EQ(test, 6UL, ARRAY_SIZE(orlix_tcti_sme_pstate_alias_contracts));
	for (index = 0; index < ARRAY_SIZE(orlix_tcti_sme_pstate_alias_contracts);
	     index++) {
		const struct orlix_tcti_sme_pstate_alias_contract *contract =
			&orlix_tcti_sme_pstate_alias_contracts[index];
		struct orlix_tcti_decoded_instruction decoded =
			orlix_tcti_decode_aarch64(contract->instruction);

		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_SME_PSTATE_IMMEDIATE,
				decoded.decode_class);
		KUNIT_EXPECT_EQ(test, contract->operation,
				decoded.sme_pstate_operation);
		KUNIT_EXPECT_EQ(test, contract->streaming_mode,
				decoded.sme_streaming_mode);
		KUNIT_EXPECT_EQ(test, contract->za, decoded.sme_za);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_SME_STATE_EXECUTION_PREREQUISITES,
				contract->prerequisites);
	}
}

static void orlix_tcti_sme_pstate_contract_keeps_execution_fail_closed(
	struct kunit *test)
{
	/*
	 * The executor's -EOPNOTSUPP and register-preservation proof remains in
	 * orlix_tcti_decode_test.c.  This ledger makes clear why accepting a decoded alias
	 * cannot be mistaken for available SME state or semantics.
	 */
	KUNIT_EXPECT_NE(test, 0UL, ORLIX_TCTI_SME_STATE_EXECUTION_PREREQUISITES);
	KUNIT_EXPECT_TRUE(test, ORLIX_TCTI_SME_STATE_EXECUTION_PREREQUISITES &
			  ORLIX_TCTI_SME_STATE_PSTATE);
	KUNIT_EXPECT_TRUE(test, ORLIX_TCTI_SME_STATE_EXECUTION_PREREQUISITES &
			  ORLIX_TCTI_SME_STATE_VL_AND_SVL);
	KUNIT_EXPECT_TRUE(test, ORLIX_TCTI_SME_STATE_EXECUTION_PREREQUISITES &
			  ORLIX_TCTI_SME_STATE_CONTEXT_SWITCH);
}

static struct kunit_case orlix_tcti_sme_state_contract_test_cases[] = {
	KUNIT_CASE(orlix_tcti_sme_pstate_contract_covers_each_decoder_alias),
	KUNIT_CASE(orlix_tcti_sme_pstate_contract_keeps_execution_fail_closed),
	{}
};

static struct kunit_suite orlix_tcti_sme_state_contract_test_suite = {
	.name = "orlix-tcti-sme-state-contract",
	.test_cases = orlix_tcti_sme_state_contract_test_cases,
};

kunit_test_suite(orlix_tcti_sme_state_contract_test_suite);
