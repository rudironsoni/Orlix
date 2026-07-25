/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_typed_expression.h"
#include <stdio.h>

static int lit(struct tcti_typed_expression *x, enum tcti_typed_type type,
	       uint8_t width, uint32_t *out)
{
	enum tcti_typed_error error;
	struct tcti_typed_value value;

	if (tcti_typed_value_from_u64(&value, 0, type == TCTI_TYPED_BOOL ? 1 :
				      width, &error))
		return -1;
	return tcti_typed_literal(x, type, width, &value, 0, 0,
				  (struct tcti_typed_provenance){9, 2}, out, &error);
}

static int value_parse_tests(void)
{
	struct tcti_typed_value value;
	enum tcti_typed_error error;
	int failed = 0;

	if (tcti_typed_value_parse(&value,
			"'1x000000000000000000000000000000"
			"00000000000000000000000000000000"
			"00000000000000000000000000000000"
			"00000000000000000000000000000001'",
			&error) || value.width != 128 || value.value[1] != UINT64_C(1) << 63 ||
	    value.value[0] != 1U ||
	    value.known_mask[1] != ~(UINT64_C(1) << 62) ||
	    value.known_mask[0] != UINT64_MAX || error != TCTI_TYPED_OK)
		failed = 1;
	if (tcti_typed_value_parse(&value, "'10xX'", &error) ||
	    error != TCTI_TYPED_OK || value.width != 4 || value.value[0] != 8U ||
	    value.known_mask[0] != 12U)
		failed = 1;
	if (!tcti_typed_value_parse(&value, "''", &error) ||
	    error != TCTI_TYPED_VALUE_INVALID)
		failed = 1;
	if (!tcti_typed_value_parse(&value, "'102'", &error) ||
	    error != TCTI_TYPED_VALUE_INVALID)
		failed = 1;
	if (!tcti_typed_value_parse(&value,
			"'00000000000000000000000000000000"
			"00000000000000000000000000000000"
			"00000000000000000000000000000000"
			"00000000000000000000000000000000"
			"0'",
			&error) || error != TCTI_TYPED_VALUE_INVALID)
		failed = 1;
	if (!tcti_typed_value_from_u64(&value, 1U, 0, &error) ||
	    error != TCTI_TYPED_VALUE_INVALID)
		failed = 1;
	return failed;
}

int main(void)
{
	struct tcti_typed_expression expression = {0};
	struct tcti_typed_value value;
	enum tcti_typed_error error;
	uint32_t a, b, c, d, set, set2, result, parameter, version;
	int failed = 0;

	failed |= lit(&expression, TCTI_TYPED_UNSIGNED, 8, &a);
	failed |= lit(&expression, TCTI_TYPED_UNSIGNED, 8, &b);
	failed |= lit(&expression, TCTI_TYPED_UNSIGNED, 8, &c);
	failed |= lit(&expression, TCTI_TYPED_SIGNED, 8, &d);
	failed |= tcti_typed_value_from_u64(&value, 0, 0, &error);
	failed |= tcti_typed_literal(&expression, TCTI_TYPED_SET, 0, &value,
				      TCTI_TYPED_UNSIGNED, 8,
				      (struct tcti_typed_provenance){9, 2}, &set, &error);
	failed |= tcti_typed_literal(&expression, TCTI_TYPED_SET, 0, &value,
				      TCTI_TYPED_UNSIGNED, 8,
				      (struct tcti_typed_provenance){9, 2}, &set2, &error);
	failed |= tcti_typed_apply(&expression, TCTI_TYPED_LT,
				   (uint32_t[]){a, b}, 2,
				   (struct tcti_typed_provenance){9, 2}, &result, &error);
	if (expression.nodes[result].type != TCTI_TYPED_BOOL ||
	    expression.nodes[result].provenance.offset != 9)
		failed = 1;
	failed |= tcti_typed_apply(&expression, TCTI_TYPED_IN,
				   (uint32_t[]){c, set}, 2,
				   (struct tcti_typed_provenance){9, 2}, &result, &error);
	if (!tcti_typed_apply(&expression, TCTI_TYPED_EQ,
				      (uint32_t[]){a, d}, 2,
				      (struct tcti_typed_provenance){9, 2}, &result, &error) ||
	    error != TCTI_TYPED_OWNED)
		failed = 1;
	if (!tcti_typed_apply(&expression, TCTI_TYPED_IN,
				      (uint32_t[]){d, set2}, 2,
				      (struct tcti_typed_provenance){9, 2}, &result, &error) ||
	    error != TCTI_TYPED_TYPE)
		failed = 1;
	tcti_typed_destroy(&expression);
	failed |= value_parse_tests();
	failed |= tcti_typed_boolean_atom(&expression, "FEAT_A", 1,
					 (struct tcti_typed_provenance){1, 6},
					 &parameter, &error);
	failed |= tcti_typed_boolean_atom(&expression, "v9Ap3", 0,
					 (struct tcti_typed_provenance){8, 5},
					 &version, &error);
	if (!expression.nodes[parameter].identity.parameter ||
	    expression.nodes[version].identity.parameter ||
	    expression.nodes[version].type != TCTI_TYPED_UNRESOLVED_BOOLEAN ||
	    !expression.nodes[parameter].identity.text ||
	    !expression.nodes[version].identity.text)
		failed = 1;
	tcti_typed_destroy(&expression);
	failed |= tcti_typed_value_atom(&expression, "'10xX'",
				       (struct tcti_typed_provenance){3, 6}, &a, &error);
	failed |= tcti_typed_apply(&expression, TCTI_TYPED_UINT,
				   (uint32_t[]){a}, 1,
				   (struct tcti_typed_provenance){3, 6}, &result, &error);
	if (expression.nodes[result].width != 4 ||
	    expression.nodes[result].value.value[0] != 8U ||
	    expression.nodes[result].value.known_mask[0] != 12U)
		failed = 1;
	tcti_typed_destroy(&expression);
	failed |= tcti_typed_atom(&expression, TCTI_TYPED_FIELD, NULL, "AArch64",
				  "ID_AA64PFR0_EL1", "SVE",
				  (struct tcti_typed_provenance){4, 1}, &a, &error);
	failed |= tcti_typed_apply(&expression, TCTI_TYPED_UINT,
				   (uint32_t[]){a}, 1,
				   (struct tcti_typed_provenance){4, 1}, &result, &error);
	if (expression.nodes[result].width != 0 ||
	    expression.nodes[result].value.width != 0)
		failed = 1;
	tcti_typed_destroy(&expression);
	if (failed)
		fprintf(stderr, "typed IR failure\n");
	return !!failed;
}
