/* SPDX-License-Identifier: GPL-2.0-only */
#include "runtime_projection.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#define TARGET_LEAF_COUNT 4350
#define HWCAP_ALPHA (1UL << 0)
#define HWCAP_BETA (1UL << 1)

static int failures;

#define EXPECT(condition)                                                        \
	do {                                                                      \
		if (!(condition)) {                                                \
			fprintf(stderr, "%s:%d: expectation failed: %s\n",         \
				__FILE__, __LINE__, #condition);                    \
			failures++;                                                 \
		}                                                                 \
	} while (0)

static const char *const alpha_feature[] = { "FEAT_ALPHA" };
static const char *const beta_feature[] = { "FEAT_BETA" };

static const struct tcti_runtime_projection_capability capabilities[] = {
	{ TCTI_RUNTIME_CAPABILITY_HWCAP, HWCAP_ALPHA, "FEAT_ALPHA" },
	{ TCTI_RUNTIME_CAPABILITY_HWCAP, HWCAP_BETA, "FEAT_BETA" },
};

struct live_provider_fixture {
	size_t calls;
	bool fail;
};

static int read_live_leaf(const void *context, size_t index,
	struct tcti_runtime_projection_leaf *leaf)
{
	struct live_provider_fixture *fixture =
		(struct live_provider_fixture *)context;

	if (!fixture || !leaf || fixture->fail || index >= TARGET_LEAF_COUNT)
		return -1;
	fixture->calls++;
	*leaf = (struct tcti_runtime_projection_leaf) {
		.name = "PINNED_SOURCE_LEAF",
		.features = index == 0 ? alpha_feature : NULL,
		.feature_count = index == 0 ? 1 : 0,
		.classification = TCTI_RUNTIME_LEAF_REQUIRED_EL0,
		.proof = "source-bound-proof-record",
		.source_bound = true,
		.proved = index != 0,
	};
	return 0;
}

static void initialize_complete_ledger(
	struct tcti_runtime_projection_leaf leaves[TARGET_LEAF_COUNT])
{
	size_t index;

	memset(leaves, 0, sizeof(*leaves) * TARGET_LEAF_COUNT);
	for (index = 0; index < TARGET_LEAF_COUNT; index++) {
		leaves[index].name = "PINNED_SOURCE_LEAF";
		leaves[index].classification = TCTI_RUNTIME_LEAF_REQUIRED_EL0;
		leaves[index].proof = "resolved-proof";
		leaves[index].source_bound = true;
		leaves[index].proved = true;
	}
	leaves[0].features = alpha_feature;
	leaves[0].feature_count = 1;
	leaves[1].features = alpha_feature;
	leaves[1].feature_count = 1;
	leaves[2].features = beta_feature;
	leaves[2].feature_count = 1;
}

static int audit(
	const struct tcti_runtime_projection_leaf leaves[TARGET_LEAF_COUNT],
	unsigned long hwcap, struct tcti_runtime_projection_result *result)
{
	const struct tcti_runtime_projection_ledger ledger = {
		.leaves = leaves,
		.leaf_count = TARGET_LEAF_COUNT,
		.target_leaf_count = TARGET_LEAF_COUNT,
	};
	const struct tcti_runtime_projection_profile profile = {
		.hwcap = hwcap,
	};

	return tcti_runtime_projection_audit_ledger(
		&ledger, &profile, capabilities,
		sizeof(capabilities) / sizeof(capabilities[0]), result);
}

static int audit_custom(
	const struct tcti_runtime_projection_ledger *ledger,
	const struct tcti_runtime_projection_profile *profile,
	const struct tcti_runtime_projection_capability *mappings,
	size_t mapping_count, struct tcti_runtime_projection_result *result)
{
	return tcti_runtime_projection_audit_ledger(
		ledger, profile, mappings, mapping_count, result);
}

static void advertised_feature_rejects_unclassified_leaf(void)
{
	struct tcti_runtime_projection_leaf leaves[TARGET_LEAF_COUNT];
	struct tcti_runtime_projection_result result;

	initialize_complete_ledger(leaves);
	leaves[1].classification = TCTI_RUNTIME_LEAF_UNCLASSIFIED;

	EXPECT(audit(leaves, HWCAP_ALPHA, &result) == -EINVAL);
	EXPECT(result.target_leaf_count == TARGET_LEAF_COUNT);
	EXPECT(result.classified_leaf_count == TARGET_LEAF_COUNT - 1);
	EXPECT(result.unproved_leaf_count == 1);
	EXPECT(result.advertised_without_proof_hwcap == HWCAP_ALPHA);
}

static void advertised_feature_rejects_unresolved_proof(void)
{
	struct tcti_runtime_projection_leaf leaves[TARGET_LEAF_COUNT];
	struct tcti_runtime_projection_result result;

	initialize_complete_ledger(leaves);
	leaves[1].proof = "";

	EXPECT(audit(leaves, HWCAP_ALPHA, &result) == -EINVAL);
	EXPECT(result.target_leaf_count == TARGET_LEAF_COUNT);
	EXPECT(result.classified_leaf_count == TARGET_LEAF_COUNT);
	EXPECT(result.unproved_leaf_count == 1);
	EXPECT(result.advertised_without_proof_hwcap == HWCAP_ALPHA);
}

static void advertised_feature_rejects_unproved_leaf(void)
{
	struct tcti_runtime_projection_leaf leaves[TARGET_LEAF_COUNT];
	struct tcti_runtime_projection_result result;

	initialize_complete_ledger(leaves);
	leaves[1].proved = false;

	EXPECT(audit(leaves, HWCAP_ALPHA, &result) == -EINVAL);
	EXPECT(result.target_leaf_count == TARGET_LEAF_COUNT);
	EXPECT(result.classified_leaf_count == TARGET_LEAF_COUNT);
	EXPECT(result.unproved_leaf_count == 1);
	EXPECT(result.advertised_without_proof_hwcap == HWCAP_ALPHA);
}

static void advertised_feature_rejects_source_unbound_leaf(void)
{
	struct tcti_runtime_projection_leaf leaves[TARGET_LEAF_COUNT];
	struct tcti_runtime_projection_result result;

	initialize_complete_ledger(leaves);
	leaves[1].source_bound = false;

	EXPECT(audit(leaves, HWCAP_ALPHA, &result) == -EINVAL);
	EXPECT(result.target_leaf_count == TARGET_LEAF_COUNT);
	EXPECT(result.source_bound_leaf_count == TARGET_LEAF_COUNT - 1);
	EXPECT(result.advertised_without_proof_hwcap == HWCAP_ALPHA);
}

static void proved_cohort_cannot_authorize_another_bit(void)
{
	struct tcti_runtime_projection_leaf leaves[TARGET_LEAF_COUNT];
	struct tcti_runtime_projection_result result;

	initialize_complete_ledger(leaves);
	leaves[2].proved = false;

	EXPECT(audit(leaves, HWCAP_BETA, &result) == -EINVAL);
	EXPECT(result.target_leaf_count == TARGET_LEAF_COUNT);
	EXPECT(result.proved_hwcap == HWCAP_ALPHA);
	EXPECT(result.advertised_without_proof_hwcap == HWCAP_BETA);
}

static void zero_advertised_bits_preserves_complete_target(void)
{
	struct tcti_runtime_projection_leaf leaves[TARGET_LEAF_COUNT];
	struct tcti_runtime_projection_result result;

	initialize_complete_ledger(leaves);
	leaves[1].classification = TCTI_RUNTIME_LEAF_UNCLASSIFIED;
	leaves[1].proof = NULL;
	leaves[1].source_bound = false;
	leaves[1].proved = false;
	leaves[2].proof = "";
	leaves[2].proved = false;

	EXPECT(audit(leaves, 0, &result) == 0);
	EXPECT(result.target_leaf_count == TARGET_LEAF_COUNT);
	EXPECT(result.classified_leaf_count == TARGET_LEAF_COUNT - 1);
	EXPECT(result.unproved_leaf_count == 2);
	EXPECT(result.advertised_hwcap == 0);
	EXPECT(result.proved_hwcap == 0);
	EXPECT(result.unadvertised_mapping_count == 2);
	EXPECT(result.unadvertised_incomplete_feature_count == 2);
}

static void empty_and_oversized_counts_fail_before_iteration(void)
{
	struct tcti_runtime_projection_leaf leaf = {
		.name = "LEAF",
		.classification = TCTI_RUNTIME_LEAF_REQUIRED_EL0,
		.proof = "resolved-proof",
		.source_bound = true,
		.proved = true,
	};
	const struct tcti_runtime_projection_profile profile = { 0 };
	struct tcti_runtime_projection_result result;
	struct tcti_runtime_projection_ledger ledger = { 0 };

	EXPECT(audit_custom(&ledger, &profile, capabilities,
			    sizeof(capabilities) / sizeof(capabilities[0]),
			    &result) == -EINVAL);

	ledger.leaves = &leaf;
	ledger.leaf_count =
		TCTI_RUNTIME_PROJECTION_MAX_TARGET_LEAVES + 1U;
	ledger.target_leaf_count = ledger.leaf_count;
	EXPECT(audit_custom(&ledger, &profile, capabilities,
			    sizeof(capabilities) / sizeof(capabilities[0]),
			    &result) == -EINVAL);

	ledger.leaf_count = (size_t)-1;
	ledger.target_leaf_count = (size_t)-1;
	EXPECT(audit_custom(&ledger, &profile, capabilities,
			    sizeof(capabilities) / sizeof(capabilities[0]),
			    &result) == -EINVAL);

	ledger.leaf_count = 1;
	ledger.target_leaf_count = 1;
	EXPECT(audit_custom(&ledger, &profile, NULL, 0, &result) == -EINVAL);
	EXPECT(audit_custom(
		       &ledger, &profile, capabilities,
		       TCTI_RUNTIME_PROJECTION_MAX_CAPABILITY_MAPPINGS + 1U,
		       &result) == -EINVAL);
}

static void malformed_leaf_feature_sets_fail(void)
{
	struct tcti_runtime_projection_leaf leaves[TARGET_LEAF_COUNT];
	struct tcti_runtime_projection_result result;
	static const char *const null_feature[] = { NULL };
	static const char *const empty_feature[] = { "" };
	static const char *const duplicate_features[] = {
		"FEAT_ALPHA", "FEAT_ALPHA"
	};

	initialize_complete_ledger(leaves);
	leaves[0].features = NULL;
	EXPECT(audit(leaves, 0, &result) == -EINVAL);

	initialize_complete_ledger(leaves);
	leaves[0].features = null_feature;
	EXPECT(audit(leaves, 0, &result) == -EINVAL);

	initialize_complete_ledger(leaves);
	leaves[0].features = empty_feature;
	EXPECT(audit(leaves, 0, &result) == -EINVAL);

	initialize_complete_ledger(leaves);
	leaves[0].features = duplicate_features;
	leaves[0].feature_count = 2;
	EXPECT(audit(leaves, 0, &result) == -EINVAL);

	initialize_complete_ledger(leaves);
	leaves[0].feature_count = (size_t)-1;
	EXPECT(audit(leaves, 0, &result) == -EINVAL);
}

static void invalid_capability_mappings_fail(void)
{
	struct tcti_runtime_projection_leaf leaves[TARGET_LEAF_COUNT];
	const struct tcti_runtime_projection_ledger ledger = {
		.leaves = leaves,
		.leaf_count = TARGET_LEAF_COUNT,
		.target_leaf_count = TARGET_LEAF_COUNT,
	};
	const struct tcti_runtime_projection_profile profile = { 0 };
	struct tcti_runtime_projection_result result;
	static const struct tcti_runtime_projection_capability duplicate[] = {
		{ TCTI_RUNTIME_CAPABILITY_HWCAP, HWCAP_ALPHA, "FEAT_ALPHA" },
		{ TCTI_RUNTIME_CAPABILITY_HWCAP, HWCAP_ALPHA, "FEAT_BETA" },
	};
	static const struct tcti_runtime_projection_capability unknown_word[] = {
		{ (enum tcti_runtime_capability_word)99,
		  HWCAP_ALPHA, "FEAT_ALPHA" },
	};

	initialize_complete_ledger(leaves);
	EXPECT(audit_custom(&ledger, &profile, duplicate,
			    sizeof(duplicate) / sizeof(duplicate[0]),
			    &result) == -EINVAL);
	EXPECT(audit_custom(&ledger, &profile, unknown_word,
			    sizeof(unknown_word) / sizeof(unknown_word[0]),
			    &result) == -EINVAL);
}

static void unadvertised_gap_cannot_authorize_or_block_proved_bit(void)
{
	struct tcti_runtime_projection_leaf leaves[TARGET_LEAF_COUNT];
	struct tcti_runtime_projection_result result;

	initialize_complete_ledger(leaves);
	leaves[2].proved = false;

	EXPECT(audit(leaves, HWCAP_ALPHA, &result) == 0);
	EXPECT(result.target_leaf_count == TARGET_LEAF_COUNT);
	EXPECT(result.advertised_hwcap == HWCAP_ALPHA);
	EXPECT(result.proved_hwcap == HWCAP_ALPHA);
	EXPECT((result.proved_hwcap & HWCAP_BETA) == 0);
	EXPECT(result.advertised_without_proof_hwcap == 0);
	EXPECT(result.unadvertised_mapping_count == 1);
	EXPECT(result.unadvertised_incomplete_feature_count == 1);
}

static void unmapped_advertised_capability_is_reported(void)
{
	struct tcti_runtime_projection_leaf leaves[TARGET_LEAF_COUNT];
	struct tcti_runtime_projection_result result;
	const unsigned long unknown_bit = 1UL << 7;

	initialize_complete_ledger(leaves);

	EXPECT(audit(leaves, HWCAP_ALPHA | unknown_bit, &result) == -EINVAL);
	EXPECT(result.target_leaf_count == TARGET_LEAF_COUNT);
	EXPECT(result.unmapped_advertised_hwcap == unknown_bit);
	EXPECT(result.advertised_without_proof_hwcap == unknown_bit);
}

static void live_provider_retains_full_target_and_fails_closed(void)
{
	struct live_provider_fixture fixture = { 0 };
	const struct tcti_runtime_projection_provider provider = {
		.context = &fixture,
		.leaf_count = TARGET_LEAF_COUNT,
		.read_leaf = read_live_leaf,
	};
	const struct tcti_runtime_projection_profile zero_profile = { 0 };
	const struct tcti_runtime_projection_profile advertised_profile = {
		.hwcap = HWCAP_ALPHA,
	};
	const struct tcti_runtime_projection_profile missing_feature_profile = {
		.hwcap = HWCAP_BETA,
	};
	const struct tcti_runtime_projection_capability alpha_capability[] = {
		{ TCTI_RUNTIME_CAPABILITY_HWCAP, HWCAP_ALPHA, "FEAT_ALPHA" },
	};
	struct tcti_runtime_projection_result result;

	EXPECT(tcti_runtime_projection_audit_provider(
		       &provider, &zero_profile, alpha_capability,
		       sizeof(alpha_capability) / sizeof(alpha_capability[0]),
		       &result) == 0);
	EXPECT(result.target_leaf_count == TARGET_LEAF_COUNT);
	EXPECT(result.classified_leaf_count == TARGET_LEAF_COUNT);
	EXPECT(result.unproved_leaf_count == 1);
	EXPECT(result.unadvertised_incomplete_feature_count == 1);
	EXPECT(fixture.calls == TARGET_LEAF_COUNT * 2);

	fixture.calls = 0;
	EXPECT(tcti_runtime_projection_audit_provider(
		       &provider, &advertised_profile, alpha_capability,
		       sizeof(alpha_capability) / sizeof(alpha_capability[0]),
		       &result) == -EINVAL);
	EXPECT(result.target_leaf_count == TARGET_LEAF_COUNT);
	EXPECT(result.advertised_without_proof_hwcap == HWCAP_ALPHA);

	/* A mapped HWCAP bit with no generated feature cohort fails closed too. */
	fixture.calls = 0;
	EXPECT(tcti_runtime_projection_audit_provider(
		       &provider, &missing_feature_profile, capabilities,
		       sizeof(capabilities) / sizeof(capabilities[0]),
		       &result) == -EINVAL);
	EXPECT(result.target_leaf_count == TARGET_LEAF_COUNT);
	EXPECT(result.advertised_without_proof_hwcap == HWCAP_BETA);

	fixture.fail = true;
	EXPECT(tcti_runtime_projection_audit_provider(
		       &provider, &zero_profile, alpha_capability,
		       sizeof(alpha_capability) / sizeof(alpha_capability[0]),
		       &result) == -EINVAL);
}

int main(void)
{
	advertised_feature_rejects_unclassified_leaf();
	advertised_feature_rejects_unresolved_proof();
	advertised_feature_rejects_unproved_leaf();
	advertised_feature_rejects_source_unbound_leaf();
	proved_cohort_cannot_authorize_another_bit();
	zero_advertised_bits_preserves_complete_target();
	empty_and_oversized_counts_fail_before_iteration();
	malformed_leaf_feature_sets_fail();
	invalid_capability_mappings_fail();
	unadvertised_gap_cannot_authorize_or_block_proved_bit();
	unmapped_advertised_capability_is_reported();
	live_provider_retains_full_target_and_fails_closed();

	if (failures) {
		fprintf(stderr, "runtime projection tests: %d failed\n", failures);
		return 1;
	}
	puts("runtime projection tests: 12 passed");
	return 0;
}
