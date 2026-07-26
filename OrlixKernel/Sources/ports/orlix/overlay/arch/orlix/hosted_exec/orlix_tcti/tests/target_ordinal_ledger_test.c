// SPDX-License-Identifier: GPL-2.0-only
#include "../isa/target_ordinal_ledger.h"

#ifdef __KERNEL__
#include <kunit/test.h>
#include <linux/errno.h>

static const struct orlix_tcti_a64_ordinal_range target_ranges[] = {
	{ 0U, 1099U },
	{ 1100U, 2226U },
	{ 2227U, 3299U },
	{ 3300U, 4349U },
};

static void orlix_tcti_target_ordinal_ledger_covers_every_source_leaf(struct kunit *test)
{
	struct orlix_tcti_a64_ordinal_ledger_result result;
	u32 index;
	u32 total = 0;

	for (index = 0; index < ARRAY_SIZE(target_ranges); index++) {
		if (index)
			KUNIT_EXPECT_EQ(test, target_ranges[index - 1U].last + 1U,
				target_ranges[index].first);
		KUNIT_ASSERT_EQ(test, 0, orlix_tcti_a64_ordinal_ledger_validate(
			&target_ranges[index], &result));
		KUNIT_EXPECT_EQ(test, target_ranges[index].first, result.first_ordinal);
		KUNIT_EXPECT_EQ(test, target_ranges[index].last, result.last_ordinal);
		KUNIT_EXPECT_EQ(test, target_ranges[index].last -
			target_ranges[index].first + 1U, result.rows);
		total += result.rows;
	}
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_A64_TARGET_ORDINAL_LEAF_COUNT, total);
}

static void orlix_tcti_target_ordinal_ledger_rejects_invalid_ranges(struct kunit *test)
{
	struct orlix_tcti_a64_ordinal_ledger_result result;
	const struct orlix_tcti_a64_ordinal_range reversed = { 12U, 11U };
	const struct orlix_tcti_a64_ordinal_range out_of_bounds = { 0U, 4350U };

	KUNIT_EXPECT_EQ(test, -ERANGE,
		orlix_tcti_a64_ordinal_ledger_validate(&reversed, &result));
	KUNIT_EXPECT_EQ(test, -ERANGE,
		orlix_tcti_a64_ordinal_ledger_validate(&out_of_bounds, &result));
	KUNIT_EXPECT_EQ(test, -EINVAL,
		orlix_tcti_a64_ordinal_ledger_validate(NULL, &result));
}

static struct kunit_case orlix_tcti_target_ordinal_ledger_cases[] = {
	KUNIT_CASE(orlix_tcti_target_ordinal_ledger_covers_every_source_leaf),
	KUNIT_CASE(orlix_tcti_target_ordinal_ledger_rejects_invalid_ranges),
	{}
};

static struct kunit_suite orlix_tcti_target_ordinal_ledger_test_suite = {
	.name = "orlix_tcti_target_ordinal_ledger",
	.test_cases = orlix_tcti_target_ordinal_ledger_cases,
};
kunit_test_suite(orlix_tcti_target_ordinal_ledger_test_suite);
#else
#include <errno.h>
#include <stdio.h>

#define EXPECT(condition) \
	do { \
		if (!(condition)) { \
			fprintf(stderr, "%s:%d: EXPECT(%s) failed\n", \
				__FILE__, __LINE__, #condition); \
			return 1; \
		} \
	} while (0)

int main(void)
{
	static const struct orlix_tcti_a64_ordinal_range ranges[] = {
		{ 0U, 1099U }, { 1100U, 2226U },
		{ 2227U, 3299U }, { 3300U, 4349U },
	};
	struct orlix_tcti_a64_ordinal_ledger_result result;
	u32 total = 0;
	u32 index;

	for (index = 0; index < sizeof(ranges) / sizeof(ranges[0]); index++) {
		if (index)
			EXPECT(ranges[index - 1U].last + 1U == ranges[index].first);
		EXPECT(orlix_tcti_a64_ordinal_ledger_validate(&ranges[index], &result) == 0);
		EXPECT(result.rows == ranges[index].last - ranges[index].first + 1U);
		total += result.rows;
	}
	EXPECT(total == ORLIX_TCTI_A64_TARGET_ORDINAL_LEAF_COUNT);
	return 0;
}
#endif
