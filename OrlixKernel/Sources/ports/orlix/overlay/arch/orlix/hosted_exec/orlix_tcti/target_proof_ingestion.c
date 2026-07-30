// SPDX-License-Identifier: GPL-2.0-only
/* Durable native-proof ledger implementation. */
#ifdef __KERNEL__
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/string.h>
#define proof_alloc(size) kzalloc((size), GFP_KERNEL)
#define proof_free(pointer) kfree(pointer)
#else
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#define proof_alloc(size) calloc(1, (size))
#define proof_free(pointer) free(pointer)
#endif

#include "target_proof_ingestion.h"
#include "target_proof_ingestion_private.h"

static void reject(enum orlix_tcti_target_proof_ingestion_error value,
			   enum orlix_tcti_target_proof_ingestion_error *error)
{
	if (error)
		*error = value;
}

struct orlix_tcti_target_proof_ingestion_ledger *
orlix_tcti_target_proof_ingestion_ledger_create(size_t capacity)
{
	struct orlix_tcti_target_proof_ingestion_ledger *ledger;

	if (!capacity || capacity > SIZE_MAX / sizeof(*ledger->slots))
		return NULL;
	ledger = proof_alloc(sizeof(*ledger));
	if (!ledger)
		return NULL;
	ledger->slots = proof_alloc(capacity * sizeof(*ledger->slots));
	if (!ledger->slots) {
		proof_free(ledger);
		return NULL;
	}
	ledger->capacity = capacity;
#ifdef __KERNEL__
	mutex_init(&ledger->lock);
#else
	if (pthread_mutex_init(&ledger->lock, NULL)) {
		proof_free(ledger->slots);
		proof_free(ledger);
		return NULL;
	}
#endif
	return ledger;
}

void orlix_tcti_target_proof_ingestion_ledger_destroy(
	struct orlix_tcti_target_proof_ingestion_ledger *ledger)
{
	if (!ledger)
		return;
#ifndef __KERNEL__
	pthread_mutex_destroy(&ledger->lock);
#endif
	proof_free(ledger->slots);
	proof_free(ledger);
}

int orlix_tcti_target_proof_ingestion_summary(
	const struct orlix_tcti_target_proof_ingestion_ledger *ledger,
	struct orlix_tcti_target_proof_ingestion_summary *summary)
{
	if (!ledger || !summary)
		return -1;
	summary->accepted_records = ledger->count;
	summary->native_passed = ledger->native_passed;
	summary->kselftest_passed = ledger->kselftest_passed;
	summary->rejected = ledger->rejected;
	return 0;
}

/* Kselftest ingestion keeps its distinct Linux-owned contract; native wire is
 * deliberately the only native ingestion representation. */
int orlix_tcti_target_proof_ingest_kselftest(
	struct orlix_tcti_target_proof_ingestion_ledger *ledger,
	struct orlix_tcti_target_kselftest_result *result,
	enum orlix_tcti_target_proof_ingestion_error *error)
{
	(void)ledger;
	(void)result;
	reject(ORLIX_TCTI_TARGET_PROOF_INGEST_NON_PRODUCTION, error);
	return -1;
}
