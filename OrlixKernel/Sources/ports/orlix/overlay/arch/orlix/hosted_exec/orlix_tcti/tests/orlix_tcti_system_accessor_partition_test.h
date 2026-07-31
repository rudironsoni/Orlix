/* SPDX-License-Identifier: GPL-2.0-only */
/* Included by orlix_tcti_source_leaf_classification_test.c. */
#ifndef ORLIX_TCTI_SYSTEM_ACCESSOR_PARTITION_TEST_H
#define ORLIX_TCTI_SYSTEM_ACCESSOR_PARTITION_TEST_H

struct orlix_tcti_system_accessor_partition_row {
	u32 accessor_index;
	u32 encoding_index;
	const char *source_name;
	const char *variant_name;
	const char *generic_operation;
	u32 direction;
	u32 disposition;
	u32 selector_count;
	u32 condition_expression;
	u32 access_expression;
	u32 concrete_selector;
	u32 applicability;
	u32 semantics;
	u32 implementation;
	u32 proof_state;
	u64 selector_identity;
	u64 condition_identity;
	u64 access_identity;
	u64 decode_key;
	u32 execution_operation;
	const char *decoder_owner;
	const char *execution_owner;
	const char *kunit_suite;
	const char *kunit_case;
	u32 accessor_source_offset;
	u32 accessor_source_length;
	u32 encoding_source_offset;
	u32 encoding_source_length;
	u32 condition_source_offset;
	u32 condition_source_length;
	u32 access_source_offset;
	u32 access_source_length;
};

enum orlix_tcti_system_accessor_partition_decode_route {
	ORLIX_TCTI_SYSTEM_ACCESSOR_PARTITION_MRS_MSR_ROUTE,
	ORLIX_TCTI_SYSTEM_ACCESSOR_PARTITION_MSR_PSTATE_ROUTE,
	ORLIX_TCTI_SYSTEM_ACCESSOR_PARTITION_SYS_ROUTE,
	ORLIX_TCTI_SYSTEM_ACCESSOR_PARTITION_SYSL_ROUTE,
	ORLIX_TCTI_SYSTEM_ACCESSOR_PARTITION_SYSP_ROUTE,
	ORLIX_TCTI_SYSTEM_ACCESSOR_PARTITION_MSRR_ROUTE,
	ORLIX_TCTI_SYSTEM_ACCESSOR_PARTITION_MRRS_ROUTE,
	ORLIX_TCTI_SYSTEM_ACCESSOR_PARTITION_INVALID_ROUTE,
};

#define ORLIX_TCTI_SYSTEM_ACCESSOR_PARTITION(accessor, encoding, name, variant, \
		generic, direction, disposition, selectors, condition, access, concrete, \
		applicability, semantics, implementation, proof, selector_identity, \
		condition_identity, access_identity, decode_key, operation, decoder, executor, suite, test_case, \
		accessor_offset, accessor_length, encoding_offset, encoding_length, \
		condition_offset, condition_length, access_offset, access_length) \
	{ accessor, encoding, name, variant, generic, direction, disposition, selectors, \
	  condition, access, concrete, applicability, semantics, implementation, proof, \
	  selector_identity, condition_identity, access_identity, decode_key, operation, decoder, executor, \
	  suite, test_case, accessor_offset, accessor_length, encoding_offset, \
	  encoding_length, condition_offset, condition_length, access_offset, \
	  access_length },
static const struct orlix_tcti_system_accessor_partition_row
	orlix_tcti_system_accessor_partition[] = {
#include "../isa/system_accessor_partition.def"
};
#undef ORLIX_TCTI_SYSTEM_ACCESSOR_PARTITION

static u32 orlix_tcti_system_accessor_partition_encode(bool write, u16 selector,
							u8 rt)
{
	return (write ? 0xd5100000U : 0xd5300000U) | ((u32)selector << 5) | rt;
}

static u32 orlix_tcti_system_accessor_partition_encode_route(
	const struct orlix_tcti_system_accessor_partition_row *row, u8 rt)
{
	if (!strcmp(row->generic_operation, "SYS_CR_systeminstrs"))
		return 0xd5080000U |
			(((row->concrete_selector >> 11) & 7U) << 16) |
			(((row->concrete_selector >> 7) & 15U) << 12) |
			(((row->concrete_selector >> 3) & 15U) << 8) |
			((row->concrete_selector & 7U) << 5) | rt;
	if (!strcmp(row->generic_operation, "MSRR_SR_systemmovepr"))
		return 0xd5500000U | ((u32)row->concrete_selector << 5) | rt;
	if (!strcmp(row->generic_operation, "MRRS_RS_systemmovepr"))
		return 0xd5700000U | ((u32)row->concrete_selector << 5) | rt;
	return orlix_tcti_system_accessor_partition_encode(row->direction == 2U,
		(u16)row->concrete_selector, rt);
}

static enum orlix_tcti_system_accessor_partition_decode_route
orlix_tcti_system_accessor_partition_decode_route(
	const struct orlix_tcti_system_accessor_partition_row *row)
{
	if (!strcmp(row->generic_operation, "MRS_RS_systemmove") ||
	    !strcmp(row->generic_operation, "MSR_SR_systemmove"))
		return ORLIX_TCTI_SYSTEM_ACCESSOR_PARTITION_MRS_MSR_ROUTE;
	if (!strcmp(row->generic_operation, "MSR_SI_pstate"))
		return ORLIX_TCTI_SYSTEM_ACCESSOR_PARTITION_MSR_PSTATE_ROUTE;
	if (!strcmp(row->generic_operation, "SYS_CR_systeminstrs"))
		return ORLIX_TCTI_SYSTEM_ACCESSOR_PARTITION_SYS_ROUTE;
	if (!strcmp(row->generic_operation, "SYSL_RC_systeminstrs"))
		return ORLIX_TCTI_SYSTEM_ACCESSOR_PARTITION_SYSL_ROUTE;
	if (!strcmp(row->generic_operation, "SYSP_CR_syspairinstrs"))
		return ORLIX_TCTI_SYSTEM_ACCESSOR_PARTITION_SYSP_ROUTE;
	if (!strcmp(row->generic_operation, "MSRR_SR_systemmovepr"))
		return ORLIX_TCTI_SYSTEM_ACCESSOR_PARTITION_MSRR_ROUTE;
	if (!strcmp(row->generic_operation, "MRRS_RS_systemmovepr"))
		return ORLIX_TCTI_SYSTEM_ACCESSOR_PARTITION_MRRS_ROUTE;
	return ORLIX_TCTI_SYSTEM_ACCESSOR_PARTITION_INVALID_ROUTE;
}

static int orlix_tcti_system_accessor_partition_semantic_key_compare(
	const struct orlix_tcti_system_accessor_partition_row *left,
	const struct orlix_tcti_system_accessor_partition_row *right)
{
	if (left->condition_identity != right->condition_identity)
		return left->condition_identity < right->condition_identity ? -1 : 1;
	if (left->access_identity != right->access_identity)
		return left->access_identity < right->access_identity ? -1 : 1;
	return 0;
}

static void orlix_tcti_system_accessor_partition_noncanonical_first_row_is_never_decoded(
	struct kunit *test)
{
	const struct orlix_tcti_system_accessor_partition_row *first = NULL;
	const struct orlix_tcti_system_accessor_partition_row *canonical = NULL;
	struct orlix_tcti_decoded_instruction decoded;
	size_t index;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_system_accessor_partition); index++) {
		const struct orlix_tcti_system_accessor_partition_row *row =
			&orlix_tcti_system_accessor_partition[index];

		if (row->concrete_selector != 24344U || row->direction != 1U ||
		    orlix_tcti_system_accessor_partition_decode_route(row) !=
			ORLIX_TCTI_SYSTEM_ACCESSOR_PARTITION_MRS_MSR_ROUTE)
			continue;
		if (!first)
			first = row;
		if (!canonical ||
		    orlix_tcti_system_accessor_partition_semantic_key_compare(row,
				canonical) < 0)
			canonical = row;
	}

	KUNIT_ASSERT_NOT_NULL(test, first);
	KUNIT_ASSERT_NOT_NULL(test, canonical);
	KUNIT_EXPECT_EQ(test, 660U, first->accessor_index);
	KUNIT_EXPECT_EQ(test, 704U, canonical->accessor_index);
	KUNIT_EXPECT_LT(test, 0,
		orlix_tcti_system_accessor_partition_semantic_key_compare(first,
			canonical));
	decoded = orlix_tcti_decode_aarch64(
		orlix_tcti_system_accessor_partition_encode(false, 24344U, 9U));
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_SYSTEM_REGISTER,
		decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 9U, decoded.rt);
	KUNIT_EXPECT_EQ(test, canonical->accessor_index,
		decoded.system_accessor_id);
	KUNIT_EXPECT_EQ(test, canonical->condition_identity,
		decoded.system_accessor_condition_identity);
	KUNIT_EXPECT_EQ(test, canonical->access_identity,
		decoded.system_accessor_access_identity);
	KUNIT_EXPECT_EQ(test, canonical->decode_key,
		decoded.system_accessor_decode_key);
	KUNIT_EXPECT_EQ(test, canonical->execution_operation,
		decoded.system_accessor_operation);
}

static const char *orlix_tcti_generated_accessor_string(u32 offset)
{
	const char *string;
	size_t remaining;

	if (offset >= ARRAY_SIZE(orlix_tcti_a64_generated_strings))
		return NULL;
	string = (const char *)orlix_tcti_a64_generated_strings + offset;
	remaining = ARRAY_SIZE(orlix_tcti_a64_generated_strings) - offset;
	return memchr(string, '\0', remaining) ? string : NULL;
}

static u64 orlix_tcti_system_accessor_identity_byte(u64 identity, u8 byte)
{
	return (identity ^ byte) * 1099511628211ULL;
}

static u64 orlix_tcti_system_accessor_identity_u64(u64 identity, u64 value)
{
	unsigned int index;

	for (index = 0; index < 8U; index++)
		identity = orlix_tcti_system_accessor_identity_byte(
			identity, (u8)(value >> (index * 8U)));
	return identity;
}

static u64 orlix_tcti_system_accessor_identity_text(u64 identity,
						    const char *text)
{
	size_t length = strlen(text);
	size_t index;

	identity = orlix_tcti_system_accessor_identity_u64(identity, length);
	for (index = 0; index < length; index++)
		identity = orlix_tcti_system_accessor_identity_byte(identity,
							     (u8)text[index]);
	return identity;
}

static void orlix_tcti_system_accessor_partition_binds_source_metadata(
	struct kunit *test)
{
	const struct orlix_tcti_a64_generated_system_accessor_metadata *metadata =
		&orlix_tcti_a64_generated_system_accessor_metadata;
	size_t index;
	u32 implemented = 0, architectural = 0, unimplemented = 0;
	u32 source_semantics = 0, generic_semantics = 0;
	u32 concrete = 0, symbolic = 0, proof_not_observed = 0;
	u32 generic_mrs = 0, generic_msr = 0, generic_sys = 0;
	u32 generic_pstate = 0, generic_msrr = 0, generic_mrrs = 0;
	u32 generic_sysp = 0, generic_sysl = 0, rndr = 0, rndrrs = 0;
	u64 identity = 1469598103934665603ULL;
	const char *metadata_string;

	KUNIT_ASSERT_EQ(test, 2014U, ARRAY_SIZE(orlix_tcti_system_accessor_partition));
	KUNIT_ASSERT_EQ(test, 2014U,
			ARRAY_SIZE(orlix_tcti_a64_generated_system_accessors));
	metadata_string = orlix_tcti_generated_accessor_string(metadata->architecture);
	KUNIT_ASSERT_NOT_NULL(test, metadata_string);
	KUNIT_EXPECT_STREQ(test, "vFATAp1-A", metadata_string);
	metadata_string = orlix_tcti_generated_accessor_string(metadata->build);
	KUNIT_ASSERT_NOT_NULL(test, metadata_string);
	KUNIT_EXPECT_STREQ(test, "818", metadata_string);
	metadata_string = orlix_tcti_generated_accessor_string(metadata->reference);
	KUNIT_ASSERT_NOT_NULL(test, metadata_string);
	KUNIT_EXPECT_STREQ(test, "2026-06_rel", metadata_string);
	metadata_string = orlix_tcti_generated_accessor_string(metadata->schema);
	KUNIT_ASSERT_NOT_NULL(test, metadata_string);
	KUNIT_EXPECT_STREQ(test, "2.9.5", metadata_string);
	metadata_string = orlix_tcti_generated_accessor_string(metadata->timestamp);
	KUNIT_ASSERT_NOT_NULL(test, metadata_string);
	KUNIT_EXPECT_STREQ(test, "2026-06-24 17:12:14", metadata_string);
	metadata_string = orlix_tcti_generated_accessor_string(metadata->registers_sha256);
	KUNIT_ASSERT_NOT_NULL(test, metadata_string);
	KUNIT_EXPECT_STREQ(test,
		"5bd76c3c3ce90322eb4fd179675dafe82df2fd1cb789beee516e5b29c471b874",
		metadata_string);
	KUNIT_EXPECT_EQ(test, 2014U, metadata->accessor_count);
	KUNIT_EXPECT_EQ(test, 2014U, metadata->mapped_count);
	KUNIT_EXPECT_EQ(test, 0U, metadata->reserved_count);
	KUNIT_EXPECT_EQ(test, 0U, metadata->privileged_count);
	KUNIT_EXPECT_EQ(test, 0U, metadata->unsupported_count);
	KUNIT_EXPECT_EQ(test, 0U, metadata->ambiguous_count);
	KUNIT_EXPECT_EQ(test, 0U, metadata->contradictory_count);
	KUNIT_EXPECT_EQ(test, 0U, metadata->invalid_count);
	KUNIT_EXPECT_EQ(test, 2014U, metadata->semantic_count);
	KUNIT_EXPECT_EQ(test, 2001U, metadata->source_access_semantics_count);
	KUNIT_EXPECT_EQ(test, 13U, metadata->generic_leaf_semantics_count);
	KUNIT_EXPECT_EQ(test, 13U, metadata->implemented_count);
	KUNIT_EXPECT_EQ(test, 2U, metadata->architectural_rejection_count);
	KUNIT_EXPECT_EQ(test, 1999U, metadata->unimplemented_rejection_count);
	KUNIT_EXPECT_EQ(test, 1836U, metadata->concrete_selector_count);
	KUNIT_EXPECT_EQ(test, 178U, metadata->symbolic_selector_count);
	KUNIT_EXPECT_EQ(test, 2014U, metadata->proof_not_observed_count);
	KUNIT_EXPECT_EQ(test, 0x84ab1ba211a57499ULL,
			metadata->reconciliation_identity);
	identity = orlix_tcti_system_accessor_identity_u64(identity,
		ARRAY_SIZE(orlix_tcti_a64_generated_system_accessors));
	for (index = 0; index < ARRAY_SIZE(orlix_tcti_system_accessor_partition); index++) {
		const struct orlix_tcti_system_accessor_partition_row *row =
			&orlix_tcti_system_accessor_partition[index];
		const struct orlix_tcti_a64_generated_system_accessor_row *generated =
			&orlix_tcti_a64_generated_system_accessors[index];
		const char *name = orlix_tcti_generated_accessor_string(generated->name);
		const char *variant =
			orlix_tcti_generated_accessor_string(generated->variant_name);
		const char *generic =
			orlix_tcti_generated_accessor_string(generated->generic_leaf);
		const char *decoder =
			orlix_tcti_generated_accessor_string(generated->decoder_owner);
		const char *executor =
			orlix_tcti_generated_accessor_string(generated->execution_owner);
		const char *suite =
			orlix_tcti_generated_accessor_string(generated->kunit_suite);
		const char *test_case =
			orlix_tcti_generated_accessor_string(generated->kunit_case);

		KUNIT_ASSERT_NOT_NULL_MSG(test, name, "row %zu name offset", index);
		KUNIT_ASSERT_NOT_NULL_MSG(test, variant, "row %zu variant offset", index);
		KUNIT_ASSERT_NOT_NULL_MSG(test, generic, "row %zu generic offset", index);
		KUNIT_ASSERT_NOT_NULL_MSG(test, decoder, "row %zu decoder offset", index);
		KUNIT_ASSERT_NOT_NULL_MSG(test, executor, "row %zu executor offset", index);
		KUNIT_ASSERT_NOT_NULL_MSG(test, suite, "row %zu suite offset", index);
		KUNIT_ASSERT_NOT_NULL_MSG(test, test_case, "row %zu case offset", index);
		KUNIT_ASSERT_EQ_MSG(test, row->accessor_index, generated->accessor_index,
				    "row %zu accessor", index);
		KUNIT_ASSERT_EQ_MSG(test, row->encoding_index, generated->encoding_index,
				    "row %zu encoding", index);
		KUNIT_ASSERT_STREQ_MSG(test, row->source_name, name, "row %zu name", index);
		KUNIT_ASSERT_STREQ_MSG(test, row->variant_name, variant,
				       "row %zu variant", index);
		KUNIT_ASSERT_STREQ_MSG(test, row->generic_operation, generic,
				       "row %zu generic", index);
		KUNIT_ASSERT_EQ_MSG(test, row->direction, (u32)generated->direction,
				    "row %zu direction", index);
		KUNIT_ASSERT_EQ_MSG(test, row->disposition, (u32)generated->disposition,
				    "row %zu disposition", index);
		KUNIT_ASSERT_EQ_MSG(test, row->selector_count, generated->selector_count,
				    "row %zu selectors", index);
		KUNIT_ASSERT_EQ_MSG(test, row->condition_expression,
				    generated->condition_expression,
				    "row %zu condition", index);
		KUNIT_ASSERT_EQ_MSG(test, row->access_expression,
				    generated->access_expression, "row %zu access", index);
		KUNIT_ASSERT_EQ_MSG(test, row->concrete_selector,
				    generated->concrete_selector, "row %zu selector", index);
		KUNIT_ASSERT_EQ_MSG(test, row->applicability, (u32)generated->applicability,
				    "row %zu applicability", index);
		KUNIT_ASSERT_EQ_MSG(test, row->semantics, (u32)generated->semantics,
				    "row %zu semantics", index);
		KUNIT_ASSERT_EQ_MSG(test, row->implementation,
				    (u32)generated->implementation,
				    "row %zu implementation", index);
		KUNIT_ASSERT_EQ_MSG(test, row->proof_state, (u32)generated->proof_state,
				    "row %zu proof", index);
		KUNIT_ASSERT_EQ_MSG(test, row->selector_identity,
				    generated->selector_identity,
				    "row %zu selector identity", index);
		KUNIT_ASSERT_EQ_MSG(test, row->condition_identity,
				    generated->condition_identity,
				    "row %zu condition identity", index);
		KUNIT_ASSERT_EQ_MSG(test, row->access_identity, generated->access_identity,
				    "row %zu access identity", index);
		KUNIT_ASSERT_STREQ_MSG(test, row->decoder_owner, decoder,
				       "row %zu decoder", index);
		KUNIT_ASSERT_STREQ_MSG(test, row->execution_owner, executor,
				       "row %zu executor", index);
		KUNIT_ASSERT_STREQ_MSG(test, row->kunit_suite, suite,
				       "row %zu suite", index);
		KUNIT_ASSERT_STREQ_MSG(test, row->kunit_case, test_case,
				       "row %zu case", index);
		KUNIT_ASSERT_EQ_MSG(test, row->accessor_source_offset,
				    generated->accessor_source_offset,
				    "row %zu accessor offset", index);
		KUNIT_ASSERT_EQ_MSG(test, row->accessor_source_length,
				    generated->accessor_source_length,
				    "row %zu accessor length", index);
		KUNIT_ASSERT_EQ_MSG(test, row->encoding_source_offset,
				    generated->encoding_source_offset,
				    "row %zu encoding offset", index);
		KUNIT_ASSERT_EQ_MSG(test, row->encoding_source_length,
				    generated->encoding_source_length,
				    "row %zu encoding length", index);
		KUNIT_ASSERT_EQ_MSG(test, row->condition_source_offset,
				    generated->condition_source_offset,
				    "row %zu condition offset", index);
		KUNIT_ASSERT_EQ_MSG(test, row->condition_source_length,
				    generated->condition_source_length,
				    "row %zu condition length", index);
		KUNIT_ASSERT_EQ_MSG(test, row->access_source_offset,
				    generated->access_source_offset,
				    "row %zu access offset", index);
		KUNIT_ASSERT_EQ_MSG(test, row->access_source_length,
				    generated->access_source_length,
				    "row %zu access length", index);

		identity = orlix_tcti_system_accessor_identity_u64(identity,
			generated->accessor_index);
		identity = orlix_tcti_system_accessor_identity_u64(identity,
			generated->encoding_index);
		identity = orlix_tcti_system_accessor_identity_text(identity, name);
		identity = orlix_tcti_system_accessor_identity_text(identity, variant);
		identity = orlix_tcti_system_accessor_identity_text(identity, generic);
		identity = orlix_tcti_system_accessor_identity_u64(identity,
			generated->direction);
		identity = orlix_tcti_system_accessor_identity_u64(identity,
			generated->disposition);
		identity = orlix_tcti_system_accessor_identity_u64(identity,
			generated->selector_count);
		identity = orlix_tcti_system_accessor_identity_u64(identity,
			generated->condition_expression);
		identity = orlix_tcti_system_accessor_identity_u64(identity,
			generated->access_expression);
		identity = orlix_tcti_system_accessor_identity_u64(identity,
			generated->concrete_selector);
		identity = orlix_tcti_system_accessor_identity_u64(identity,
			generated->selector_identity);
		identity = orlix_tcti_system_accessor_identity_u64(identity,
			generated->condition_identity);
		identity = orlix_tcti_system_accessor_identity_u64(identity,
			generated->access_identity);
		identity = orlix_tcti_system_accessor_identity_u64(identity,
			generated->applicability);
		identity = orlix_tcti_system_accessor_identity_u64(identity,
			generated->semantics);
		identity = orlix_tcti_system_accessor_identity_u64(identity,
			generated->implementation);
		identity = orlix_tcti_system_accessor_identity_u64(identity,
			generated->proof_state);
		identity = orlix_tcti_system_accessor_identity_text(identity, decoder);
		identity = orlix_tcti_system_accessor_identity_text(identity, executor);
		identity = orlix_tcti_system_accessor_identity_text(identity, suite);
		identity = orlix_tcti_system_accessor_identity_text(identity, test_case);
		identity = orlix_tcti_system_accessor_identity_u64(identity,
			generated->accessor_source_offset);
		identity = orlix_tcti_system_accessor_identity_u64(identity,
			generated->accessor_source_length);
		identity = orlix_tcti_system_accessor_identity_u64(identity,
			generated->encoding_source_offset);
		identity = orlix_tcti_system_accessor_identity_u64(identity,
			generated->encoding_source_length);
		identity = orlix_tcti_system_accessor_identity_u64(identity,
			generated->condition_source_offset);
		identity = orlix_tcti_system_accessor_identity_u64(identity,
			generated->condition_source_length);
		identity = orlix_tcti_system_accessor_identity_u64(identity,
			generated->access_source_offset);
		identity = orlix_tcti_system_accessor_identity_u64(identity,
			generated->access_source_length);

		KUNIT_EXPECT_NOT_NULL(test, row->source_name);
		KUNIT_EXPECT_NOT_NULL(test, row->variant_name);
		KUNIT_EXPECT_NOT_NULL(test, row->generic_operation);
		KUNIT_EXPECT_NE(test, 0ULL, row->selector_identity);
		KUNIT_EXPECT_NE(test, 0ULL, row->condition_identity);
		KUNIT_EXPECT_NE(test, 0ULL, row->access_identity);
		KUNIT_EXPECT_EQ(test, 1U, row->applicability);
		KUNIT_EXPECT_EQ(test, 1U, row->proof_state);
		KUNIT_EXPECT_STREQ(test, "orlix_tcti_decode_aarch64", row->decoder_owner);
		KUNIT_EXPECT_STREQ(test, "orlix-tcti-source-leaf-classification",
			row->kunit_suite);
		KUNIT_EXPECT_STREQ(test,
			"orlix_tcti_system_accessor_partition_binds_source_metadata",
			row->kunit_case);
		KUNIT_EXPECT_NE(test, 0U, row->accessor_source_length);
		KUNIT_EXPECT_NE(test, 0U, row->encoding_source_length);
		KUNIT_EXPECT_NE(test, 0U, row->condition_source_length);
		if (row->semantics == 1U) {
			KUNIT_EXPECT_NE(test, 0U, row->access_source_length);
			source_semantics++;
		} else {
			KUNIT_EXPECT_TRUE(test, row->access_expression == UINT_MAX);
			generic_semantics++;
		}
		if (row->concrete_selector == UINT_MAX)
			symbolic++;
		else
			concrete++;
		if (row->proof_state == 1U)
			proof_not_observed++;
		if (row->implementation == 1U)
			implemented++;
		else if (row->implementation == 2U)
			architectural++;
		else
			unimplemented++;
		if (!strcmp(row->generic_operation, "MRS_RS_systemmove")) generic_mrs++;
		else if (!strcmp(row->generic_operation, "MSR_SR_systemmove")) generic_msr++;
		else if (!strcmp(row->generic_operation, "SYS_CR_systeminstrs")) generic_sys++;
		else if (!strcmp(row->generic_operation, "MSR_SI_pstate")) generic_pstate++;
		else if (!strcmp(row->generic_operation, "MSRR_SR_systemmovepr")) generic_msrr++;
		else if (!strcmp(row->generic_operation, "MRRS_RS_systemmovepr")) generic_mrrs++;
		else if (!strcmp(row->generic_operation, "SYSP_CR_syspairinstrs")) generic_sysp++;
		else if (!strcmp(row->generic_operation, "SYSL_RC_systeminstrs")) generic_sysl++;
		if (!strcmp(row->variant_name, "RNDR")) {
			KUNIT_EXPECT_STREQ(test, "MRS_RS_systemmove", row->generic_operation);
			KUNIT_EXPECT_EQ(test, 0x5920U, row->concrete_selector);
			rndr++;
		} else if (!strcmp(row->variant_name, "RNDRRS")) {
			KUNIT_EXPECT_STREQ(test, "MRS_RS_systemmove", row->generic_operation);
			KUNIT_EXPECT_EQ(test, 0x5921U, row->concrete_selector);
			rndrrs++;
		}
	}
	KUNIT_EXPECT_EQ(test, 13U, implemented);
	KUNIT_EXPECT_EQ(test, 2U, architectural);
	KUNIT_EXPECT_EQ(test, 1999U, unimplemented);
	KUNIT_EXPECT_EQ(test, metadata->source_access_semantics_count,
			source_semantics);
	KUNIT_EXPECT_EQ(test, metadata->generic_leaf_semantics_count,
			generic_semantics);
	KUNIT_EXPECT_EQ(test, metadata->implemented_count, implemented);
	KUNIT_EXPECT_EQ(test, metadata->architectural_rejection_count,
			architectural);
	KUNIT_EXPECT_EQ(test, metadata->unimplemented_rejection_count,
			unimplemented);
	KUNIT_EXPECT_EQ(test, metadata->concrete_selector_count, concrete);
	KUNIT_EXPECT_EQ(test, metadata->symbolic_selector_count, symbolic);
	KUNIT_EXPECT_EQ(test, metadata->proof_not_observed_count,
			proof_not_observed);
	KUNIT_EXPECT_EQ(test, metadata->reconciliation_identity, identity);
	KUNIT_EXPECT_EQ(test, 830U, generic_mrs);
	KUNIT_EXPECT_EQ(test, 698U, generic_msr);
	KUNIT_EXPECT_EQ(test, 445U, generic_sys);
	KUNIT_EXPECT_EQ(test, 13U, generic_pstate);
	KUNIT_EXPECT_EQ(test, 13U, generic_msrr);
	KUNIT_EXPECT_EQ(test, 13U, generic_mrrs);
	KUNIT_EXPECT_EQ(test, 1U, generic_sysp);
	KUNIT_EXPECT_EQ(test, 1U, generic_sysl);
	KUNIT_EXPECT_EQ(test, 1U, rndr);
	KUNIT_EXPECT_EQ(test, 1U, rndrrs);
}

static void orlix_tcti_system_accessor_partition_matches_decoder_contract(
	struct kunit *test)
{
	static const u8 rt_values[] = { 0U, 31U };
	size_t index;
	u32 concrete = 0;
	u32 canonical = 0, aliases = 0, duplicate_keys = 0;

	orlix_tcti_system_accessor_partition_noncanonical_first_row_is_never_decoded(test);

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_system_accessor_partition); index++) {
		const struct orlix_tcti_system_accessor_partition_row *row =
			&orlix_tcti_system_accessor_partition[index];
		bool write;
		size_t rt_index;

		const struct orlix_tcti_system_accessor_partition_row *canonical_row = row;
		size_t candidate;

		if (row->concrete_selector == UINT_MAX ||
		    orlix_tcti_system_accessor_partition_decode_route(row) !=
			ORLIX_TCTI_SYSTEM_ACCESSOR_PARTITION_MRS_MSR_ROUTE)
			continue;
		for (candidate = 0;
		     candidate < ARRAY_SIZE(orlix_tcti_system_accessor_partition);
		     candidate++) {
			const struct orlix_tcti_system_accessor_partition_row *other =
				&orlix_tcti_system_accessor_partition[candidate];

			if (other->concrete_selector != row->concrete_selector ||
			    other->direction != row->direction ||
			    orlix_tcti_system_accessor_partition_decode_route(other) !=
				ORLIX_TCTI_SYSTEM_ACCESSOR_PARTITION_MRS_MSR_ROUTE)
				continue;
			if (orlix_tcti_system_accessor_partition_semantic_key_compare(other,
				canonical_row) < 0)
				canonical_row = other;
			else if (other != canonical_row &&
				 !orlix_tcti_system_accessor_partition_semantic_key_compare(other,
					canonical_row))
				KUNIT_FAIL(test, "duplicate generated semantic key");
		}
		write = row->direction == 2U;
		concrete++;
		if (canonical_row == row)
			canonical++;
		else
			aliases++;
		for (rt_index = 0; rt_index < ARRAY_SIZE(rt_values); rt_index++) {
			struct orlix_tcti_decoded_instruction decoded =
				orlix_tcti_decode_aarch64(
					orlix_tcti_system_accessor_partition_encode(write,
						(u16)row->concrete_selector,
						rt_values[rt_index]));

			KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_DECODE_SYSTEM_REGISTER,
				decoded.decode_class, "%s %s", row->source_name,
				row->variant_name);
			KUNIT_EXPECT_EQ_MSG(test, rt_values[rt_index], decoded.rt,
				"%s %s", row->source_name, row->variant_name);
			KUNIT_EXPECT_EQ_MSG(test, write, decoded.system_register_write,
				"%s %s", row->source_name, row->variant_name);
			KUNIT_EXPECT_EQ_MSG(test, row->concrete_selector,
				decoded.system_accessor_selector, "%s %s", row->source_name,
				row->variant_name);
			KUNIT_EXPECT_EQ_MSG(test, canonical_row->accessor_index,
				decoded.system_accessor_id, "%s %s", row->source_name,
				row->variant_name);
			KUNIT_EXPECT_EQ_MSG(test, canonical_row->implementation,
				decoded.system_accessor_implementation, "%s %s",
				row->source_name, row->variant_name);
			KUNIT_EXPECT_EQ_MSG(test, canonical_row->selector_identity,
				decoded.system_accessor_selector_identity, "%s %s",
				row->source_name, row->variant_name);
			KUNIT_EXPECT_EQ_MSG(test, canonical_row->condition_identity,
				decoded.system_accessor_condition_identity, "%s %s",
				row->source_name, row->variant_name);
			KUNIT_EXPECT_EQ_MSG(test, canonical_row->access_identity,
				decoded.system_accessor_access_identity, "%s %s",
				row->source_name, row->variant_name);
		}
	}
	for (index = 0; index < ARRAY_SIZE(orlix_tcti_system_accessor_partition); index++) {
		const struct orlix_tcti_system_accessor_partition_row *row =
			&orlix_tcti_system_accessor_partition[index];
		size_t prior;
		bool has_prior = false;

		if (row->concrete_selector == UINT_MAX ||
		    orlix_tcti_system_accessor_partition_decode_route(row) !=
			ORLIX_TCTI_SYSTEM_ACCESSOR_PARTITION_MRS_MSR_ROUTE)
			continue;
		for (prior = 0; prior < index; prior++) {
			const struct orlix_tcti_system_accessor_partition_row *other =
				&orlix_tcti_system_accessor_partition[prior];

			if (other->concrete_selector == row->concrete_selector &&
			    other->direction == row->direction &&
			    orlix_tcti_system_accessor_partition_decode_route(other) ==
				ORLIX_TCTI_SYSTEM_ACCESSOR_PARTITION_MRS_MSR_ROUTE) {
				KUNIT_EXPECT_NE_MSG(test,
					0, orlix_tcti_system_accessor_partition_semantic_key_compare(other,
						row),
					"duplicate generated semantic key %s %s", row->source_name,
					row->variant_name);
				has_prior = true;
				break;
			}
		}
		if (!has_prior)
			for (prior = index + 1U;
			     prior < ARRAY_SIZE(orlix_tcti_system_accessor_partition); prior++) {
				const struct orlix_tcti_system_accessor_partition_row *other =
					&orlix_tcti_system_accessor_partition[prior];

				if (other->concrete_selector == row->concrete_selector &&
				    other->direction == row->direction &&
				    orlix_tcti_system_accessor_partition_decode_route(other) ==
					ORLIX_TCTI_SYSTEM_ACCESSOR_PARTITION_MRS_MSR_ROUTE) {
					duplicate_keys++;
					break;
				}
			}
	}
	KUNIT_EXPECT_EQ(test, 1368U, concrete);
	KUNIT_EXPECT_EQ(test, 1186U, canonical);
	KUNIT_EXPECT_EQ(test, 182U, aliases);
	KUNIT_EXPECT_EQ(test, 168U, duplicate_keys);
}

static void orlix_tcti_system_accessor_partition_rejections_are_structured_el0_exits(
	struct kunit *test)
{
	size_t index;
	u32 rejected = 0, concrete = 0, symbolic = 0, non_mrs_msr = 0;
	u32 architectural = 0, unimplemented = 0;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_system_accessor_partition); index++) {
		const struct orlix_tcti_system_accessor_partition_row *row =
			&orlix_tcti_system_accessor_partition[index];
		u32 instruction;
		struct pt_regs regs = { };
		struct pt_regs before;
		struct orlix_tcti_result result;
		unsigned long mapped;
		size_t register_index;

		if (row->implementation == 1U)
			continue;
		rejected++;
		if (row->implementation == 2U)
			architectural++;
		else
			unimplemented++;
		if (row->concrete_selector == UINT_MAX) {
			symbolic++;
			continue;
		}
		if (orlix_tcti_system_accessor_partition_decode_route(row) !=
		    ORLIX_TCTI_SYSTEM_ACCESSOR_PARTITION_MRS_MSR_ROUTE)
			non_mrs_msr++;
		concrete++;
		instruction = orlix_tcti_system_accessor_partition_encode_route(row, 31U);
		mapped = source_leaf_map(test, instruction);

		regs.pc = mapped;
		regs.sp = STACK_TOP - 16;
		regs.pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_C_BIT;
		regs.syscallno = NO_SYSCALL;
		for (register_index = 0; register_index < ARRAY_SIZE(regs.regs);
		     register_index++)
			regs.regs[register_index] = 0x123456789abcdef0ULL + register_index;
		before = regs;
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
			result.reason, "%s %s", row->source_name, row->variant_name);
		KUNIT_EXPECT_EQ_MSG(test, -EOPNOTSUPP, result.status,
			"%s %s", row->source_name, row->variant_name);
		KUNIT_EXPECT_EQ_MSG(test, instruction, result.instruction,
			"%s %s", row->source_name, row->variant_name);
		KUNIT_EXPECT_EQ_MSG(test, before.pc, result.pc, "%s %s",
			row->source_name, row->variant_name);
		KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
	}
	KUNIT_EXPECT_EQ(test, 2001U, rejected);
	KUNIT_EXPECT_EQ(test, 2U, architectural);
	KUNIT_EXPECT_EQ(test, 1999U, unimplemented);
	KUNIT_EXPECT_EQ(test, 1823U, concrete);
	KUNIT_EXPECT_EQ(test, 178U, symbolic);
	KUNIT_EXPECT_EQ(test, 468U, non_mrs_msr);
}

static const struct orlix_tcti_native_proof_registry_entry *
orlix_tcti_system_accessor_partition_capture_entry(
	const struct orlix_tcti_system_accessor_partition_row *row)
{
	const struct orlix_tcti_native_proof_registry_entry *entries;
	size_t count;
	size_t index;

	entries = orlix_tcti_native_proof_registry_entries(&count);
	for (index = 0; entries && index < count; index++) {
		const struct orlix_tcti_native_proof_registry_entry *entry =
			&entries[index];

		if (entry->production_capture &&
		    entry->source.source_ordinal == row->encoding_index &&
		    entry->source.semantic_variant_identity == row->selector_identity &&
		    entry->obligation == ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS)
			return entry;
	}
	return NULL;
}

static u32 orlix_tcti_system_accessor_partition_wire_u32(const u8 *bytes)
{
	return (u32)bytes[0] | ((u32)bytes[1] << 8) |
		((u32)bytes[2] << 16) | ((u32)bytes[3] << 24);
}

static u64 orlix_tcti_system_accessor_partition_wire_u64(const u8 *bytes)
{
	u64 value = 0;
	size_t index;

	for (index = 0; index < sizeof(value); index++)
		value |= (u64)bytes[index] << (index * 8U);
	return value;
}

static bool orlix_tcti_system_accessor_partition_assert_tls_wire(
	struct kunit *test, const struct orlix_tcti_native_wire_record *wire,
	u64 expected_tls)
{
	const u8 *bytes = wire->bytes;
	size_t cursor = 72U;
	size_t index;

	KUNIT_EXPECT_NOT_NULL(test, bytes);
	if (!bytes)
		return false;
	for (index = 0; index <
		orlix_tcti_system_accessor_partition_wire_u32(bytes + 52U); index++) {
		u32 kind = orlix_tcti_system_accessor_partition_wire_u32(bytes + cursor);
		u32 length = orlix_tcti_system_accessor_partition_wire_u32(
			bytes + cursor + 12U);

		if (kind == ORLIX_TCTI_NATIVE_CAPTURE_SELECTOR_SYSTEM_CONTROL) {
			KUNIT_EXPECT_EQ(test, 32U, length);
			KUNIT_EXPECT_EQ(test, expected_tls,
				orlix_tcti_system_accessor_partition_wire_u64(
					bytes + cursor + 40U));
			return length == 32U && expected_tls ==
				orlix_tcti_system_accessor_partition_wire_u64(
					bytes + cursor + 40U);
		}
		cursor += 16U + length;
	}
	KUNIT_FAIL(test, "missing SystemAccessor TLS system-control capture");
	return false;
}

static bool orlix_tcti_system_accessor_partition_assert_architectural_effect(
	struct kunit *test, const struct orlix_tcti_system_accessor_partition_row *row,
	const struct pt_regs *regs, u64 input)
{
	if (!strcmp(row->variant_name, "CNTFRQ_EL0")) {
		KUNIT_EXPECT_EQ(test, 1000000000ULL, regs->regs[0]);
		return regs->regs[0] == 1000000000ULL;
	} else if (!strcmp(row->variant_name, "CNTVCT_EL0")) {
		KUNIT_EXPECT_NE(test, 0ULL, regs->regs[0]);
		return regs->regs[0] != 0;
	} else if (!strcmp(row->variant_name, "CTR_EL0")) {
		KUNIT_EXPECT_EQ(test, 0x3004c004ULL, regs->regs[0]);
		return regs->regs[0] == 0x3004c004ULL;
	} else if (!strcmp(row->variant_name, "DCZID_EL0")) {
		KUNIT_EXPECT_EQ(test, 0x10ULL, regs->regs[0]);
		return regs->regs[0] == 0x10ULL;
	} else if (!strcmp(row->variant_name, "TPIDRRO_EL0")) {
		KUNIT_EXPECT_EQ(test, 0ULL, regs->regs[0]);
		return regs->regs[0] == 0;
	} else if (!strcmp(row->variant_name, "TPIDR_EL0")) {
		if (row->direction == 2U)
			KUNIT_EXPECT_EQ(test, input, current->thread.user_tls);
		else
			KUNIT_EXPECT_EQ(test, 0x0123456789abcdefULL, regs->regs[0]);
		return row->direction == 2U ? current->thread.user_tls == input :
			regs->regs[0] == 0x0123456789abcdefULL;
	} else if (!strcmp(row->variant_name, "NZCV")) {
		if (row->direction == 2U)
			KUNIT_EXPECT_EQ(test, input & 0xf0000000ULL,
				regs->pstate & 0xf0000000ULL);
		else
			KUNIT_EXPECT_EQ(test, 0xa0000000ULL, regs->regs[0]);
		return row->direction == 2U ?
			(regs->pstate & 0xf0000000ULL) == (input & 0xf0000000ULL) :
			regs->regs[0] == 0xa0000000ULL;
	} else if (!strcmp(row->variant_name, "FPCR")) {
		if (row->direction == 2U)
			KUNIT_EXPECT_EQ(test, input & 0x07c09f00ULL,
				current->thread.user_fpcr);
		else
			KUNIT_EXPECT_EQ(test, 0x07c09f00ULL, regs->regs[0]);
		return row->direction == 2U ? current->thread.user_fpcr ==
			(input & 0x07c09f00ULL) : regs->regs[0] == 0x07c09f00ULL;
	} else if (!strcmp(row->variant_name, "FPSR")) {
		if (row->direction == 2U)
			KUNIT_EXPECT_EQ(test, input & 0x0800009fULL,
				current->thread.user_fpsr);
		else
			KUNIT_EXPECT_EQ(test, 0x0800009fULL, regs->regs[0]);
		return row->direction == 2U ? current->thread.user_fpsr ==
			(input & 0x0800009fULL) : regs->regs[0] == 0x0800009fULL;
	} else
		KUNIT_FAIL(test, "unexpected implemented SystemAccessor variant %s",
			row->variant_name);
	return false;
}

static void orlix_tcti_system_accessor_partition_implemented_production_observations(
	struct kunit *test)
{
	const struct orlix_tcti_native_proof_registry_entry *entries;
	size_t index;
	size_t entry_count;
	u32 observed = 0;
	u32 static_variants = 0;
	u32 production_variants = 0;
	u32 symbolic_production = 0;

	entries = orlix_tcti_native_proof_registry_entries(&entry_count);
	KUNIT_ASSERT_NOT_NULL(test, entries);
	for (index = 0; index < entry_count; index++) {
		const struct orlix_tcti_native_proof_registry_entry *entry =
			&entries[index];

		if (entry->source.subject_kind !=
		    ORLIX_TCTI_NATIVE_SUBJECT_SEMANTIC_VARIANT)
			continue;
		static_variants += entry->static_obligation;
		production_variants += entry->production_capture;
		if (entry->production_capture &&
		    entry->source.concrete_selector == UINT_MAX)
			symbolic_production++;
	}
	KUNIT_EXPECT_EQ(test, 2014U, static_variants);
	KUNIT_EXPECT_EQ(test, 13U, production_variants);
	KUNIT_EXPECT_EQ(test, 0U, symbolic_production);

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_system_accessor_partition); index++) {
		const struct orlix_tcti_system_accessor_partition_row *row =
			&orlix_tcti_system_accessor_partition[index];
		struct orlix_tcti_native_capture_session *session = NULL;
		struct orlix_tcti_native_wire_record wire = { };
		struct pt_regs regs = { };
		struct orlix_tcti_result result;
		const struct orlix_tcti_native_proof_registry_entry *entry;
		const void *token;
		unsigned long old_tls;
		unsigned long old_fpcr;
		unsigned long old_fpsr;
		unsigned long mapped = 0;
		u32 instruction;
		int capture_status;
		int wire_status;
		bool semantic_ok;
		bool tls_ok = true;
		bool saved_thread_state = false;

		if (row->implementation != 1U)
			continue;
		KUNIT_EXPECT_NE(test, UINT_MAX, row->concrete_selector);
		if (row->concrete_selector == UINT_MAX)
			goto cleanup;
		instruction = orlix_tcti_system_accessor_partition_encode_route(row, 0U);
		mapped = source_leaf_map(test, instruction);
		KUNIT_EXPECT_NE(test, 0UL, mapped);
		if (!mapped)
			goto cleanup;
		old_tls = current->thread.user_tls;
		old_fpcr = current->thread.user_fpcr;
		old_fpsr = current->thread.user_fpsr;
		saved_thread_state = true;
		regs.pc = mapped;
		regs.sp = STACK_TOP - 16;
		regs.pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_C_BIT;
		regs.syscallno = NO_SYSCALL;
		regs.regs[0] = 0xf123456789abcdefULL;
		current->thread.user_tls = 0x0123456789abcdefULL;
		current->thread.user_fpcr = 0x07c09f00ULL;
		current->thread.user_fpsr = 0x0800009fULL;
		entry = orlix_tcti_system_accessor_partition_capture_entry(row);
		KUNIT_EXPECT_NOT_NULL_MSG(test, entry, "%s %s", row->source_name,
			row->variant_name);
		if (!entry)
			goto cleanup;
		token = orlix_tcti_native_proof_registry_capture_token_for_entry(entry);
		KUNIT_EXPECT_NOT_NULL_MSG(test, token, "%s %s", row->source_name,
			row->variant_name);
		if (!token)
			goto cleanup;
		capture_status = orlix_tcti_native_capture_begin(token,
			row->encoding_index, ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS,
			&session);
		KUNIT_EXPECT_EQ(test, 0, capture_status);
		if (capture_status || !session)
			goto cleanup;
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason,
			"%s %s", row->source_name, row->variant_name);
		KUNIT_EXPECT_EQ_MSG(test, mapped + sizeof(instruction), regs.pc,
			"%s %s", row->source_name, row->variant_name);
		semantic_ok = result.reason == ORLIX_TCTI_EXIT_SYSCALL &&
			regs.pc == mapped + sizeof(instruction) &&
			orlix_tcti_system_accessor_partition_assert_architectural_effect(test,
			row, &regs, 0xf123456789abcdefULL);
		if (!semantic_ok)
			goto cleanup;
		wire_status = orlix_tcti_native_capture_take_wire(session, &wire);
		KUNIT_EXPECT_EQ(test, 0, wire_status);
		if (wire_status || !wire.sealed)
			goto cleanup;
		KUNIT_EXPECT_TRUE(test, wire.sealed);
		KUNIT_EXPECT_NE(test, 0U, wire.identity);
		KUNIT_EXPECT_TRUE(test,
			orlix_tcti_native_wire_record_has_production_origin(&wire));
		KUNIT_EXPECT_EQ(test, 0,
			orlix_tcti_native_wire_record_validate(&wire,
				entry->capture_declaration,
				orlix_tcti_native_proof_registry_entry_identity(entry)));
		if (!wire.identity ||
		    !orlix_tcti_native_wire_record_has_production_origin(&wire) ||
		    orlix_tcti_native_wire_record_validate(&wire,
				entry->capture_declaration,
				orlix_tcti_native_proof_registry_entry_identity(entry)))
			goto cleanup;
		if (!strcmp(row->variant_name, "TPIDR_EL0"))
			tls_ok = orlix_tcti_system_accessor_partition_assert_tls_wire(test, &wire,
				row->direction == 2U ? 0xf123456789abcdefULL :
				0x0123456789abcdefULL);
		if (!tls_ok)
			goto cleanup;
		observed++;
	cleanup:
		orlix_tcti_native_wire_record_destroy(&wire);
		orlix_tcti_native_capture_destroy(session);
		if (saved_thread_state) {
			current->thread.user_tls = old_tls;
			current->thread.user_fpcr = old_fpcr;
			current->thread.user_fpsr = old_fpsr;
		}
		if (mapped)
			KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
	}
	KUNIT_EXPECT_EQ(test, 13U, observed);
}

#endif /* ORLIX_TCTI_SYSTEM_ACCESSOR_PARTITION_TEST_H */
