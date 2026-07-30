// SPDX-License-Identifier: GPL-2.0-only
#include <stdio.h>

#include "target_proof_ingestion.h"

/*
 * Host code has no OrlixTCTI resume path.  Production-record ingestion is
 * covered by the KUnit fixture that drives orlix_tcti_resume_user_captured().
 * This gate keeps the host ABI honest without recreating a test wire builder.
 */
int main(void)
{
	struct orlix_tcti_target_proof_ingestion_ledger *ledger;
	struct orlix_tcti_target_proof_ingestion_summary summary;

	ledger = orlix_tcti_target_proof_ingestion_ledger_create(1U);
	if (!ledger || orlix_tcti_target_proof_ingestion_summary(ledger, &summary) ||
	    summary.accepted_records || summary.native_passed || summary.rejected) {
		orlix_tcti_target_proof_ingestion_ledger_destroy(ledger);
		return 1;
	}
	orlix_tcti_target_proof_ingestion_ledger_destroy(ledger);
	puts("target proof ingestion host ABI test passed");
	return 0;
}
