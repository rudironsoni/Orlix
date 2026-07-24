// SPDX-License-Identifier: GPL-2.0-only
#ifdef TCTI_RUNTIME_PROJECTION_HOST_TEST
#include <errno.h>
#include <string.h>
#else
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <asm/isa.h>
#endif

#include "runtime_projection.h"

#ifndef TCTI_RUNTIME_PROJECTION_HOST_TEST
#define stringify_1(value) #value
#define stringify(value) stringify_1(value)
#define AUXV_HWCAP TCTI_RUNTIME_CAPABILITY_HWCAP
#define AUXV_HWCAP2 TCTI_RUNTIME_CAPABILITY_HWCAP2
#define TCTI_A64_RUNTIME_CAPABILITY(word, bit, feature, extension) \
	{ (word), (bit), stringify(feature) },
static const struct tcti_runtime_projection_capability
tcti_runtime_capability_mappings[] = {
#include "../isa/runtime_profile.def"
};
#undef TCTI_A64_RUNTIME_CAPABILITY
#undef AUXV_HWCAP2
#undef AUXV_HWCAP
#undef stringify
#undef stringify_1
#endif

static bool tcti_runtime_leaf_is_classified(
	const struct tcti_runtime_projection_leaf *leaf)
{
	return leaf->classification > TCTI_RUNTIME_LEAF_UNCLASSIFIED &&
	       leaf->classification <= TCTI_RUNTIME_LEAF_ALIAS_OR_DUPLICATE;
}

static bool tcti_runtime_leaf_has_feature(
	const struct tcti_runtime_projection_leaf *leaf, const char *feature)
{
	size_t i;

	for (i = 0; i < leaf->feature_count; i++)
		if (leaf->features[i] && !strcmp(leaf->features[i], feature))
			return true;
	return false;
}

static bool tcti_runtime_feature_is_proved(
	const struct tcti_runtime_projection_ledger *ledger,
	const char *feature, bool *present)
{
	bool complete = true;
	size_t i;

	*present = false;
	for (i = 0; i < ledger->leaf_count; i++) {
		const struct tcti_runtime_projection_leaf *leaf =
			&ledger->leaves[i];

		if (!tcti_runtime_leaf_has_feature(leaf, feature))
			continue;
		*present = true;
		if (!tcti_runtime_leaf_is_classified(leaf) || !leaf->proof ||
		    !leaf->proof[0] || !leaf->proved)
			complete = false;
	}

	return *present && complete;
}

static unsigned long *tcti_runtime_result_word(
	struct tcti_runtime_projection_result *result,
	enum tcti_runtime_capability_word word, bool proved)
{
	if (word == TCTI_RUNTIME_CAPABILITY_HWCAP)
		return proved ? &result->proved_hwcap : &result->mapped_hwcap;
	if (word == TCTI_RUNTIME_CAPABILITY_HWCAP2)
		return proved ? &result->proved_hwcap2 : &result->mapped_hwcap2;
	return NULL;
}

int tcti_runtime_projection_audit_ledger(
	const struct tcti_runtime_projection_ledger *ledger,
	const struct tcti_runtime_projection_profile *profile,
	const struct tcti_runtime_projection_capability *capabilities,
	size_t capability_count,
	struct tcti_runtime_projection_result *result)
{
	size_t i;

	if (!result || !ledger || !profile ||
	    (!capabilities && capability_count) ||
	    (!ledger->leaves && ledger->leaf_count))
		return -EINVAL;
	memset(result, 0, sizeof(*result));
	result->advertised_hwcap = profile->hwcap;
	result->advertised_hwcap2 = profile->hwcap2;
	result->target_leaf_count = ledger->target_leaf_count;
	if (!ledger->leaf_count ||
	    ledger->leaf_count > TCTI_RUNTIME_PROJECTION_MAX_TARGET_LEAVES ||
	    ledger->target_leaf_count >
		    TCTI_RUNTIME_PROJECTION_MAX_TARGET_LEAVES ||
	    ledger->leaf_count != ledger->target_leaf_count ||
	    !capability_count ||
	    capability_count >
		    TCTI_RUNTIME_PROJECTION_MAX_CAPABILITY_MAPPINGS)
		return -EINVAL;

	for (i = 0; i < ledger->leaf_count; i++) {
		const struct tcti_runtime_projection_leaf *leaf =
			&ledger->leaves[i];
		size_t feature;

		if (!leaf->name || !leaf->name[0] ||
		    leaf->feature_count >
			    TCTI_RUNTIME_PROJECTION_MAX_FEATURES_PER_LEAF ||
		    (leaf->feature_count && !leaf->features))
			return -EINVAL;
		for (feature = 0; feature < leaf->feature_count; feature++) {
			size_t previous;

			if (!leaf->features[feature] ||
			    !leaf->features[feature][0])
				return -EINVAL;
			for (previous = 0; previous < feature; previous++)
				if (!strcmp(leaf->features[feature],
					    leaf->features[previous]))
					return -EINVAL;
		}
		if (tcti_runtime_leaf_is_classified(leaf))
			result->classified_leaf_count++;
		if (!tcti_runtime_leaf_is_classified(leaf) || !leaf->proof ||
		    !leaf->proof[0] || !leaf->proved)
			result->unproved_leaf_count++;
	}

	for (i = 0; i < capability_count; i++) {
		const struct tcti_runtime_projection_capability *mapping =
			&capabilities[i];
		unsigned long *mapped;
		unsigned long *proved;
		unsigned long advertised;
		bool feature_present;
		bool feature_proved;

		if (!mapping->feature || !mapping->feature[0] ||
		    !mapping->bit || (mapping->bit & (mapping->bit - 1)))
			return -EINVAL;
		mapped = tcti_runtime_result_word(result, mapping->word, false);
		proved = tcti_runtime_result_word(result, mapping->word, true);
		if (!mapped || !proved || (*mapped & mapping->bit))
			return -EINVAL;

		*mapped |= mapping->bit;
		advertised =
			mapping->word == TCTI_RUNTIME_CAPABILITY_HWCAP ?
				result->advertised_hwcap :
				result->advertised_hwcap2;
		feature_proved = tcti_runtime_feature_is_proved(ledger,
							       mapping->feature,
							       &feature_present);
		if (feature_proved)
			*proved |= mapping->bit;
		if (!(advertised & mapping->bit)) {
			result->unadvertised_mapping_count++;
			if (!feature_proved)
				result->unadvertised_incomplete_feature_count++;
		} else if (!feature_present) {
			return -EINVAL;
		}
		result->mapping_count++;
	}

	result->unmapped_advertised_hwcap =
		result->advertised_hwcap & ~result->mapped_hwcap;
	result->unmapped_advertised_hwcap2 =
		result->advertised_hwcap2 & ~result->mapped_hwcap2;
	result->advertised_without_proof_hwcap =
		result->advertised_hwcap & ~result->proved_hwcap;
	result->advertised_without_proof_hwcap2 =
		result->advertised_hwcap2 & ~result->proved_hwcap2;

	if (result->unmapped_advertised_hwcap ||
	    result->unmapped_advertised_hwcap2 ||
	    result->advertised_without_proof_hwcap ||
	    result->advertised_without_proof_hwcap2)
		return -EINVAL;
	return 0;
}

#ifndef TCTI_RUNTIME_PROJECTION_HOST_TEST
int tcti_runtime_projection_audit(
	struct tcti_runtime_projection_result *result)
{
	const struct tcti_runtime_projection_ledger ledger = {
		/* The live ledger must enumerate all 4,350 pinned source leaves. */
		.target_leaf_count = 4350,
	};
	const struct tcti_runtime_projection_profile profile = {
		.hwcap = ORLIX_EL0_HWCAP,
		.hwcap2 = ORLIX_EL0_HWCAP2,
	};

	return tcti_runtime_projection_audit_ledger(
		&ledger, &profile, tcti_runtime_capability_mappings,
		ARRAY_SIZE(tcti_runtime_capability_mappings), result);
}
#endif
