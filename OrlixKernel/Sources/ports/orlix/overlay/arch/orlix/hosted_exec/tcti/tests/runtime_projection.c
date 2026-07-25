// SPDX-License-Identifier: GPL-2.0-only
#ifdef TCTI_RUNTIME_PROJECTION_HOST_TEST
#include <errno.h>
#include <string.h>
#else
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <asm/isa.h>
#include <target_inventory.h>
#endif

#include "runtime_projection.h"

#ifndef TCTI_RUNTIME_PROJECTION_HOST_TEST
struct tcti_runtime_source_bound_proof {
	unsigned int ordinal;
	const char *proof_id;
};

#define TCTI_A64_SOURCE_BOUND_PROOF(ordinal, proof_id) \
	{ ordinal, proof_id },
static const struct tcti_runtime_source_bound_proof
tcti_runtime_source_bound_proofs[] = {
#include "../isa/source_bound_proof.def"
};
#undef TCTI_A64_SOURCE_BOUND_PROOF
#endif

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

#ifndef TCTI_RUNTIME_PROJECTION_HOST_TEST
static bool tcti_runtime_leaf_is_source_bound(unsigned int ordinal,
	const char *proof_id)
{
	size_t index;

	if (!proof_id || !proof_id[0])
		return false;
	for (index = 0; index < sizeof(tcti_runtime_source_bound_proofs) /
				      sizeof(tcti_runtime_source_bound_proofs[0]); index++)
		if (tcti_runtime_source_bound_proofs[index].ordinal == ordinal &&
		    !strcmp(tcti_runtime_source_bound_proofs[index].proof_id,
			    proof_id))
			return true;
	return false;
}
#endif

static int tcti_runtime_ledger_read_leaf(const void *context, size_t index,
	struct tcti_runtime_projection_leaf *leaf)
{
	const struct tcti_runtime_projection_ledger *ledger = context;

	if (!ledger || !leaf || index >= ledger->leaf_count)
		return -EINVAL;
	*leaf = ledger->leaves[index];
	return 0;
}

static bool tcti_runtime_feature_is_proved(
	const struct tcti_runtime_projection_provider *provider,
	const char *feature, bool *present)
{
	bool complete = true;
	size_t i;

	*present = false;
	for (i = 0; i < provider->leaf_count; i++) {
		struct tcti_runtime_projection_leaf leaf;

		if (provider->read_leaf(provider->context, i, &leaf))
			return false;

		if (!tcti_runtime_leaf_has_feature(&leaf, feature))
			continue;
		*present = true;
		if (!tcti_runtime_leaf_is_classified(&leaf) || !leaf.proof ||
		    !leaf.proof[0] || !leaf.source_bound || !leaf.proved)
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

int tcti_runtime_projection_audit_provider(
	const struct tcti_runtime_projection_provider *provider,
	const struct tcti_runtime_projection_profile *profile,
	const struct tcti_runtime_projection_capability *capabilities,
	size_t capability_count,
	struct tcti_runtime_projection_result *result)
{
	bool advertised_feature_missing = false;
	size_t i;

	if (!result || !provider || !provider->read_leaf || !profile ||
	    (!capabilities && capability_count) ||
	    !provider->leaf_count)
		return -EINVAL;
	memset(result, 0, sizeof(*result));
	result->advertised_hwcap = profile->hwcap;
	result->advertised_hwcap2 = profile->hwcap2;
	result->target_leaf_count = provider->leaf_count;
	if (provider->leaf_count != TCTI_RUNTIME_PROJECTION_MAX_TARGET_LEAVES ||
	    !capability_count ||
	    capability_count >
		    TCTI_RUNTIME_PROJECTION_MAX_CAPABILITY_MAPPINGS)
		return -EINVAL;

	for (i = 0; i < provider->leaf_count; i++) {
		struct tcti_runtime_projection_leaf leaf;
		size_t feature;

		if (provider->read_leaf(provider->context, i, &leaf))
			return -EINVAL;
		if (!leaf.name || !leaf.name[0] ||
		    leaf.feature_count >
			    TCTI_RUNTIME_PROJECTION_MAX_FEATURES_PER_LEAF ||
		    (leaf.feature_count && !leaf.features))
			return -EINVAL;
		for (feature = 0; feature < leaf.feature_count; feature++) {
			size_t previous;

			if (!leaf.features[feature] ||
			    !leaf.features[feature][0])
				return -EINVAL;
			for (previous = 0; previous < feature; previous++)
				if (!strcmp(leaf.features[feature],
					    leaf.features[previous]))
					return -EINVAL;
		}
		if (tcti_runtime_leaf_is_classified(&leaf))
			result->classified_leaf_count++;
		if (leaf.source_bound)
			result->source_bound_leaf_count++;
		if (!tcti_runtime_leaf_is_classified(&leaf) || !leaf.proof ||
		    !leaf.proof[0] || !leaf.source_bound || !leaf.proved)
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
		feature_proved = tcti_runtime_feature_is_proved(provider,
						       mapping->feature,
						       &feature_present);
		if (feature_proved)
			*proved |= mapping->bit;
		if (!(advertised & mapping->bit)) {
			result->unadvertised_mapping_count++;
			if (!feature_proved)
				result->unadvertised_incomplete_feature_count++;
		} else if (!feature_present) {
			/*
			 * Keep auditing every advertised bit so the result records the
			 * complete failure mask. A mapped capability without a generated
			 * feature cohort is no more eligible for advertisement than an
			 * incomplete cohort.
			 */
			advertised_feature_missing = true;
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
	    result->advertised_without_proof_hwcap2 ||
	    advertised_feature_missing)
		return -EINVAL;
	return 0;
}

int tcti_runtime_projection_audit_ledger(
	const struct tcti_runtime_projection_ledger *ledger,
	const struct tcti_runtime_projection_profile *profile,
	const struct tcti_runtime_projection_capability *capabilities,
	size_t capability_count,
	struct tcti_runtime_projection_result *result)
{
	const struct tcti_runtime_projection_provider provider = {
		.context = ledger,
		.leaf_count = ledger ? ledger->leaf_count : 0,
		.read_leaf = tcti_runtime_ledger_read_leaf,
	};

	if (!ledger || !ledger->leaves ||
	    ledger->leaf_count != TCTI_RUNTIME_PROJECTION_MAX_TARGET_LEAVES ||
	    ledger->target_leaf_count !=
		TCTI_RUNTIME_PROJECTION_MAX_TARGET_LEAVES)
		return -EINVAL;
	return tcti_runtime_projection_audit_provider(
		&provider, profile, capabilities, capability_count, result);
}

#ifndef TCTI_RUNTIME_PROJECTION_HOST_TEST
static int tcti_runtime_generated_string(u32 offset, const char **text)
{
	const char *value;

	if (!text || offset >= TCTI_A64_GENERATED_STRING_POOL_SIZE)
		return -EINVAL;
	value = (const char *)tcti_a64_generated_strings + offset;
	if (!memchr(value, '\0', TCTI_A64_GENERATED_STRING_POOL_SIZE - offset))
		return -EINVAL;
	*text = value;
	return 0;
}

static int tcti_runtime_generated_read_leaf(const void *context, size_t index,
	struct tcti_runtime_projection_leaf *leaf)
{
	const struct tcti_a64_generated_row *row;
	const char *name;
	const char *condition;
	const char *proof;
	enum tcti_runtime_leaf_classification classification;

	if (!context || !leaf || index >= TCTI_A64_GENERATED_SOURCE_COUNT ||
	    tcti_a64_generated_metadata.source_count !=
		TCTI_A64_GENERATED_SOURCE_COUNT)
		return -EINVAL;
	row = &tcti_a64_generated_rows[index];
	if (row->ordinal != index ||
	    tcti_runtime_generated_string(row->name, &name) ||
	    tcti_runtime_generated_string(row->condition, &condition) ||
	    tcti_runtime_generated_string(row->proof, &proof) ||
	    !condition[0])
		return -EINVAL;
	switch (row->classification) {
	case 0:
		classification = TCTI_RUNTIME_LEAF_UNCLASSIFIED;
		break;
	case 1:
		classification = TCTI_RUNTIME_LEAF_REQUIRED_EL0;
		break;
	case 2:
		classification = TCTI_RUNTIME_LEAF_NON_EL0;
		break;
	case 3:
		classification =
			TCTI_RUNTIME_LEAF_ARCH_UNDEFINED_OR_UNALLOCATED;
		break;
	case 4:
		classification = TCTI_RUNTIME_LEAF_ALIAS_OR_DUPLICATE;
		break;
	default:
		return -EINVAL;
	}
	/* Source binding is necessary for promotion, but it is not execution proof. */
	*leaf = (struct tcti_runtime_projection_leaf) {
		.name = name,
		.proof = proof,
		.classification = classification,
		.source_bound = tcti_runtime_leaf_is_source_bound(row->ordinal,
			proof),
		.proved = false,
	};
	return 0;
}

int tcti_runtime_projection_audit(
	struct tcti_runtime_projection_result *result)
{
	const struct tcti_runtime_projection_provider provider = {
		.context = &tcti_a64_generated_metadata,
		.leaf_count = TCTI_A64_GENERATED_SOURCE_COUNT,
		.read_leaf = tcti_runtime_generated_read_leaf,
	};
	const struct tcti_runtime_projection_profile profile = {
		.hwcap = ORLIX_EL0_HWCAP,
		.hwcap2 = ORLIX_EL0_HWCAP2,
	};

	return tcti_runtime_projection_audit_provider(
		&provider, &profile, tcti_runtime_capability_mappings,
		ARRAY_SIZE(tcti_runtime_capability_mappings), result);
}
#endif
