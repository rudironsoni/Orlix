/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_condition_serialization.h"

#include <stdio.h>
#include <string.h>

#define EXPECT_TRUE(value) do { \
	if (!(value)) { \
		fprintf(stderr, "%s:%d: expectation failed: %s\n", __FILE__, \
			__LINE__, #value); \
		return -1; \
	} \
} while (0)

static int golden_expression_bytes(void)
{
	static char feature[] = "FEAT_SVE";
	static char value[] = "1";
	static uint32_t items[] = { 3, 4 };
	static struct orlix_tcti_target_expr expressions[] = {
		{ .kind = ORLIX_TCTI_TARGET_EXPR_IN, .left = 1, .right = 2 },
		{ .kind = ORLIX_TCTI_TARGET_EXPR_FEATURE, .text = feature },
		{ .kind = ORLIX_TCTI_TARGET_EXPR_SET, .first_item = 0, .item_count = 2 },
		{ .kind = ORLIX_TCTI_TARGET_EXPR_VALUE, .text = value },
		{ .kind = ORLIX_TCTI_TARGET_EXPR_BOOL, .boolean = true },
	};
	static const uint8_t expected[] = {
		'T', 'C', 'N', 'D', 1,
		0x0b, 0, 0, 0, 42,
		0x02, 0, 0, 0, 12, 0, 0, 0, 8, 'F', 'E', 'A', 'T', '_', 'S', 'V', 'E',
		0x05, 0, 0, 0, 20, 0, 0, 0, 2,
		0x04, 0, 0, 0, 5, 0, 0, 0, 1, '1',
		0x01, 0, 0, 0, 1, 1,
	};
	struct orlix_tcti_target_inventory inventory = {
		.expressions = expressions,
		.expression_count = sizeof(expressions) / sizeof(expressions[0]),
		.set_items = items,
		.set_item_count = sizeof(items) / sizeof(items[0]),
	};
	struct orlix_tcti_target_condition_bytes bytes;

	EXPECT_TRUE(!orlix_tcti_target_condition_serialize(&inventory, 0, &bytes, NULL));
	EXPECT_TRUE(bytes.length == sizeof(expected));
	EXPECT_TRUE(!memcmp(bytes.data, expected, sizeof(expected)));
	orlix_tcti_target_condition_bytes_destroy(&bytes);
	return 0;
}

static int contains(const uint8_t *data, size_t length, const char *needle)
{
	size_t needle_length = strlen(needle);
	size_t index;

	if (needle_length > length)
		return 0;
	for (index = 0; index <= length - needle_length; index++)
		if (!memcmp(data + index, needle, needle_length))
			return 1;
	return 0;
}

static int collisions_remain_distinct(void)
{
	static char a[] = "A";
	static char b[] = "B";
	static struct orlix_tcti_target_expr expressions[] = {
		{ .kind = ORLIX_TCTI_TARGET_EXPR_AND, .left = 1, .right = 2 },
		{ .kind = ORLIX_TCTI_TARGET_EXPR_FEATURE, .text = a },
		{ .kind = ORLIX_TCTI_TARGET_EXPR_FEATURE, .text = b },
		{ .kind = ORLIX_TCTI_TARGET_EXPR_OR, .left = 1, .right = 2 },
	};
	struct orlix_tcti_target_inventory inventory = {
		.expressions = expressions,
		.expression_count = sizeof(expressions) / sizeof(expressions[0]),
	};
	struct orlix_tcti_target_condition_bytes left;
	struct orlix_tcti_target_condition_bytes right;

	EXPECT_TRUE(!orlix_tcti_target_condition_serialize(&inventory, 0, &left, NULL));
	EXPECT_TRUE(!orlix_tcti_target_condition_serialize(&inventory, 3, &right, NULL));
	EXPECT_TRUE(left.length == right.length);
	EXPECT_TRUE(memcmp(left.data, right.data, left.length));
	orlix_tcti_target_condition_bytes_destroy(&left);
	orlix_tcti_target_condition_bytes_destroy(&right);
	return 0;
}

static int inherited_asts_remain_complete(void)
{
	static char parent[] = "FEAT_PARENT";
	static char child[] = "FEAT_CHILD";
	static struct orlix_tcti_target_expr expressions[] = {
		{ .kind = ORLIX_TCTI_TARGET_EXPR_FEATURE, .text = parent },
		{ .kind = ORLIX_TCTI_TARGET_EXPR_FEATURE, .text = child },
		{ .kind = ORLIX_TCTI_TARGET_EXPR_AND, .left = 0, .right = 1 },
	};
	struct orlix_tcti_target_inventory inventory = {
		.expressions = expressions,
		.expression_count = sizeof(expressions) / sizeof(expressions[0]),
	};
	struct orlix_tcti_target_condition_bytes inherited;
	struct orlix_tcti_target_condition_bytes local;

	EXPECT_TRUE(!orlix_tcti_target_condition_serialize(&inventory, 2, &inherited, NULL));
	EXPECT_TRUE(!orlix_tcti_target_condition_serialize(&inventory, 1, &local, NULL));
	EXPECT_TRUE(inherited.length > local.length);
	EXPECT_TRUE(contains(inherited.data, inherited.length, parent));
	EXPECT_TRUE(contains(inherited.data, inherited.length, child));
	orlix_tcti_target_condition_bytes_destroy(&inherited);
	orlix_tcti_target_condition_bytes_destroy(&local);
	return 0;
}

int main(void)
{
	static const struct {
		const char *name;
		int (*run)(void);
	} tests[] = {
		{ "golden_expression_bytes", golden_expression_bytes },
		{ "collisions_remain_distinct", collisions_remain_distinct },
		{ "inherited_asts_remain_complete", inherited_asts_remain_complete },
	};
	size_t index;

	for (index = 0; index < sizeof(tests) / sizeof(tests[0]); index++) {
		if (tests[index].run()) {
			fprintf(stderr, "FAIL %s\n", tests[index].name);
			return 1;
		}
		printf("PASS %s\n", tests[index].name);
	}
	return 0;
}
