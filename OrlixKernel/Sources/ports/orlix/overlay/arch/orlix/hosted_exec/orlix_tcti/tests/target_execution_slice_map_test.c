/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_execution_slice_map.h"

#ifdef __KERNEL__
#include <kunit/test.h>
#include <linux/gfp.h>
#include <linux/string.h>
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#endif

struct mutable_map {
	struct orlix_tcti_execution_slice_map map;
	struct orlix_tcti_execution_slice_family *families;
	struct orlix_tcti_execution_slice_member *members;
};

#ifdef __KERNEL__
static void *test_alloc(struct kunit *test, size_t size)
{
	return kunit_kmalloc(test, size, GFP_KERNEL);
}

static void test_free(void *value)
{
	(void)value;
}
#else
static void *test_alloc(void *test, size_t size)
{
	(void)test;
	return malloc(size);
}

static void test_free(void *value)
{
	free(value);
}
#endif

static int mutable_map_init(
#ifdef __KERNEL__
	struct kunit *test,
#else
	void *test,
#endif
	struct mutable_map *mutable)
{
	const struct orlix_tcti_execution_slice_map *canonical =
		orlix_tcti_execution_slice_map_canonical();
	size_t family_bytes = canonical->counts.family_count *
		sizeof(*mutable->families);
	size_t member_bytes = canonical->counts.leaf_count *
		sizeof(*mutable->members);

	mutable->families = test_alloc(test, family_bytes);
	mutable->members = test_alloc(test, member_bytes);
	if (!mutable->families || !mutable->members) {
		test_free(mutable->members);
		test_free(mutable->families);
		return -1;
	}
	memcpy(mutable->families, canonical->families, family_bytes);
	memcpy(mutable->members, canonical->members, member_bytes);
	mutable->map = *canonical;
	mutable->map.families = mutable->families;
	mutable->map.members = mutable->members;
	return 0;
}

static void mutable_map_destroy(struct mutable_map *mutable)
{
	test_free(mutable->members);
	test_free(mutable->families);
}

static int expect_error(const struct orlix_tcti_execution_slice_map *map,
			 enum orlix_tcti_execution_slice_map_error expected)
{
	struct orlix_tcti_execution_slice_map_validation_result result;

	return orlix_tcti_execution_slice_map_validate(map, &result) < 0 &&
		result.error == expected ? 0 : -1;
}

static int canonical_succeeds(void)
{
	struct orlix_tcti_execution_slice_map_validation_result result;
	const struct orlix_tcti_execution_slice_map *canonical =
		orlix_tcti_execution_slice_map_canonical();

	if (orlix_tcti_execution_slice_map_validate(canonical, &result))
		return -1;
	return result.error == ORLIX_TCTI_EXECUTION_SLICE_MAP_VALID &&
		canonical->counts.leaf_count ==
			ORLIX_TCTI_EXECUTION_SLICE_MAP_LEAF_COUNT ? 0 : -1;
}

static int provenance_and_count_mutations(
#ifdef __KERNEL__
	struct kunit *test
#else
	void *test
#endif
)
{
	struct mutable_map mutable;
	const struct orlix_tcti_execution_slice_map *canonical =
		orlix_tcti_execution_slice_map_canonical();

	if (mutable_map_init(test, &mutable))
		return -1;
	mutable.map.source.sha256 = "stale-source-sha256";
	if (expect_error(&mutable.map,
			 ORLIX_TCTI_EXECUTION_SLICE_MAP_PROVENANCE_MISMATCH))
		goto fail;
	mutable.map.source = canonical->source;
	mutable.map.counts.leaf_count--;
	if (expect_error(&mutable.map,
			 ORLIX_TCTI_EXECUTION_SLICE_MAP_COUNT_MISMATCH))
		goto fail;
	mutable_map_destroy(&mutable);
	return 0;
fail:
	mutable_map_destroy(&mutable);
	return -1;
}

static int ordinal_and_source_mutations(
#ifdef __KERNEL__
	struct kunit *test
#else
	void *test
#endif
)
{
	struct mutable_map mutable;
	const struct orlix_tcti_execution_slice_map *canonical =
		orlix_tcti_execution_slice_map_canonical();

	if (mutable_map_init(test, &mutable))
		return -1;
	mutable.members[1] = mutable.members[0];
	if (expect_error(&mutable.map,
			 ORLIX_TCTI_EXECUTION_SLICE_MAP_ORDINAL_MISMATCH))
		goto fail;
	mutable.members[1] = canonical->members[1];
	mutable.members[0].ordinal = 1U;
	if (expect_error(&mutable.map,
			 ORLIX_TCTI_EXECUTION_SLICE_MAP_ORDINAL_MISMATCH))
		goto fail;
	mutable.members[0] = canonical->members[0];
	mutable.members[0].ordinal = ORLIX_TCTI_EXECUTION_SLICE_MAP_LEAF_COUNT;
	if (expect_error(&mutable.map,
			 ORLIX_TCTI_EXECUTION_SLICE_MAP_ORDINAL_MISMATCH))
		goto fail;
	mutable.members[0] = canonical->members[0];
	mutable.members[0].source_name = "stale-source-name";
	if (expect_error(&mutable.map,
			 ORLIX_TCTI_EXECUTION_SLICE_MAP_SOURCE_NAME_MISMATCH))
		goto fail;
	mutable.members[0] = canonical->members[0];
	mutable.members[0].condition_tcnd_hex = "54434e4401";
	if (expect_error(&mutable.map,
			 ORLIX_TCTI_EXECUTION_SLICE_MAP_CONDITION_MISMATCH))
		goto fail;
	mutable_map_destroy(&mutable);
	return 0;
fail:
	mutable_map_destroy(&mutable);
	return -1;
}

static int family_mutations(
#ifdef __KERNEL__
	struct kunit *test
#else
	void *test
#endif
)
{
	struct mutable_map mutable;
	const struct orlix_tcti_execution_slice_map *canonical =
		orlix_tcti_execution_slice_map_canonical();
	u32 original_family;

	if (mutable_map_init(test, &mutable) || mutable.map.counts.family_count < 2U)
		return -1;
	mutable.members[0].family_index = (u32)mutable.map.counts.family_count;
	if (expect_error(&mutable.map,
			 ORLIX_TCTI_EXECUTION_SLICE_MAP_UNKNOWN_FAMILY))
		goto fail;
	mutable.members[0] = canonical->members[0];
	mutable.families[1].issue_id = mutable.families[0].issue_id;
	if (expect_error(&mutable.map,
			 ORLIX_TCTI_EXECUTION_SLICE_MAP_FAMILY_DUPLICATE))
		goto fail;
	mutable.families[1] = canonical->families[1];
	mutable.families[1].stable_id = mutable.families[0].stable_id;
	if (expect_error(&mutable.map,
			 ORLIX_TCTI_EXECUTION_SLICE_MAP_FAMILY_DUPLICATE))
		goto fail;
	mutable.families[1] = canonical->families[1];
	mutable.families[0].issue_id = 0;
	if (expect_error(&mutable.map,
			 ORLIX_TCTI_EXECUTION_SLICE_MAP_FAMILY_INVALID))
		goto fail;
	mutable.families[0] = canonical->families[0];
	mutable.families[0].stable_id = "";
	if (expect_error(&mutable.map,
			 ORLIX_TCTI_EXECUTION_SLICE_MAP_FAMILY_INVALID))
		goto fail;
	mutable.families[0] = canonical->families[0];
	mutable.families[0].declared_member_count++;
	if (expect_error(&mutable.map,
			 ORLIX_TCTI_EXECUTION_SLICE_MAP_COUNT_MISMATCH))
		goto fail;
	mutable.families[0] = canonical->families[0];
	original_family = mutable.members[0].family_index;
	mutable.members[0].family_index = original_family == 0U ? 1U : 0U;
	if (expect_error(&mutable.map,
			 ORLIX_TCTI_EXECUTION_SLICE_MAP_MEMBERSHIP_MISMATCH))
		goto fail;
	mutable_map_destroy(&mutable);
	return 0;
fail:
	mutable_map_destroy(&mutable);
	return -1;
}

#ifdef __KERNEL__
static void orlix_tcti_execution_slice_map_canonical_test(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, 0, canonical_succeeds());
}

static void orlix_tcti_execution_slice_map_provenance_test(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, 0, provenance_and_count_mutations(test));
}

static void orlix_tcti_execution_slice_map_source_test(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, 0, ordinal_and_source_mutations(test));
}

static void orlix_tcti_execution_slice_map_family_test(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, 0, family_mutations(test));
}

static struct kunit_case orlix_tcti_execution_slice_map_cases[] = {
	KUNIT_CASE(orlix_tcti_execution_slice_map_canonical_test),
	KUNIT_CASE(orlix_tcti_execution_slice_map_provenance_test),
	KUNIT_CASE(orlix_tcti_execution_slice_map_source_test),
	KUNIT_CASE(orlix_tcti_execution_slice_map_family_test),
	{}
};

static struct kunit_suite orlix_tcti_execution_slice_map_suite = {
	.name = "orlix_tcti_execution_slice_map",
	.test_cases = orlix_tcti_execution_slice_map_cases,
};
kunit_test_suite(orlix_tcti_execution_slice_map_suite);
#else
int main(void)
{
	if (canonical_succeeds() ||
	    provenance_and_count_mutations(NULL) ||
	    ordinal_and_source_mutations(NULL) || family_mutations(NULL))
		return EXIT_FAILURE;
	puts("target execution slice map tests: passed");
	return EXIT_SUCCESS;
}
#endif
