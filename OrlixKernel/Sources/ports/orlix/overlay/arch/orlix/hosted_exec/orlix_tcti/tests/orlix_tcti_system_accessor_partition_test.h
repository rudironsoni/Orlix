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
	u32 accessor_source_length;
	u32 encoding_source_length;
	u32 condition_source_length;
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
	  suite, test_case, accessor_length, encoding_length, condition_length, \
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

static void orlix_tcti_system_accessor_partition_binds_source_metadata(
	struct kunit *test)
{
	size_t index;
	u32 implemented = 0, architectural = 0, unimplemented = 0;
	u32 generic_mrs = 0, generic_msr = 0, generic_sys = 0;
	u32 generic_pstate = 0, generic_msrr = 0, generic_mrrs = 0;
	u32 generic_sysp = 0, generic_sysl = 0, rndr = 0, rndrrs = 0;

	KUNIT_ASSERT_EQ(test, 2014U, ARRAY_SIZE(orlix_tcti_system_accessor_partition));
	for (index = 0; index < ARRAY_SIZE(orlix_tcti_system_accessor_partition); index++) {
		const struct orlix_tcti_system_accessor_partition_row *row =
			&orlix_tcti_system_accessor_partition[index];

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
		if (row->semantics == 1U)
			KUNIT_EXPECT_NE(test, 0U, row->access_source_length);
		else
			KUNIT_EXPECT_TRUE(test, row->access_expression == UINT_MAX);
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
