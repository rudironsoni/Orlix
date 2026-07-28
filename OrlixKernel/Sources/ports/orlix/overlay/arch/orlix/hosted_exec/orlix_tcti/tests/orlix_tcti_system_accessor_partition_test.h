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

#define ORLIX_TCTI_SYSTEM_ACCESSOR_PARTITION(accessor, encoding, name, variant, \
		generic, direction, disposition, selectors, condition, access, concrete, \
		applicability, semantics, implementation, proof, selector_identity, \
		condition_identity, access_identity, decoder, executor, suite, test_case, \
		accessor_offset, accessor_length, encoding_offset, encoding_length, \
		condition_offset, condition_length, access_offset, access_length) \
	{ accessor, encoding, name, variant, generic, direction, disposition, selectors, \
	  condition, access, concrete, applicability, semantics, implementation, proof, \
	  selector_identity, condition_identity, access_identity, decoder, executor, \
	  suite, test_case, accessor_offset, accessor_length, encoding_offset, \
	  encoding_length, condition_offset, condition_length, access_offset, \
	  access_length },
static const struct orlix_tcti_system_accessor_partition_row
	orlix_tcti_system_accessor_partition[] = {
#include "../isa/system_accessor_partition.def"
};
#undef ORLIX_TCTI_SYSTEM_ACCESSOR_PARTITION

static u32 orlix_tcti_system_accessor_partition_encode(bool write, u16 selector)
{
	return (write ? 0xd5100000U : 0xd5300000U) | ((u32)selector << 5);
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
	size_t index;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_system_accessor_partition); index++) {
		const struct orlix_tcti_system_accessor_partition_row *row =
			&orlix_tcti_system_accessor_partition[index];
		bool write;
		struct orlix_tcti_decoded_instruction decoded;

		if (row->concrete_selector == UINT_MAX ||
		    (strcmp(row->generic_operation, "MRS_RS_systemmove") &&
		     strcmp(row->generic_operation, "MSR_SR_systemmove")))
			continue;
		write = row->direction == 2U;
		decoded = orlix_tcti_decode_aarch64(
			orlix_tcti_system_accessor_partition_encode(write,
				(u16)row->concrete_selector));
		KUNIT_EXPECT_EQ_MSG(test, row->implementation == 1U ?
			ORLIX_TCTI_DECODE_SYSTEM_REGISTER : ORLIX_TCTI_DECODE_UNSUPPORTED,
			decoded.decode_class, "%s %s", row->source_name, row->variant_name);
	}
}

static void orlix_tcti_system_accessor_partition_rejections_are_structured_el0_exits(
	struct kunit *test)
{
	static const u16 rejected_selectors[] = { 0xde83U, 0xd801U, 0xd807U,
		0xdf00U, 0xdf02U, 0xdce8U };
	size_t index;

	for (index = 0; index < ARRAY_SIZE(rejected_selectors); index++) {
		u32 instruction = orlix_tcti_system_accessor_partition_encode(true,
			rejected_selectors[index]);
		struct pt_regs regs = { };
		struct pt_regs before;
		struct orlix_tcti_result result;
		unsigned long mapped = source_leaf_map(test, instruction);

		regs.pc = mapped;
		regs.sp = STACK_TOP - 16;
		regs.pstate = PSR_MODE_EL0t;
		regs.syscallno = NO_SYSCALL;
		regs.regs[0] = 0x123456789abcdef0ULL;
		before = regs;
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
			result.reason, "selector=%04x", rejected_selectors[index]);
		KUNIT_EXPECT_EQ_MSG(test, -EOPNOTSUPP, result.status,
			"selector=%04x", rejected_selectors[index]);
		KUNIT_EXPECT_EQ_MSG(test, instruction, result.instruction,
			"selector=%04x", rejected_selectors[index]);
		KUNIT_EXPECT_MEMEQ(test, before.regs, regs.regs, sizeof(regs.regs));
		KUNIT_EXPECT_EQ(test, before.pc, regs.pc);
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
	}
}

#endif /* ORLIX_TCTI_SYSTEM_ACCESSOR_PARTITION_TEST_H */
