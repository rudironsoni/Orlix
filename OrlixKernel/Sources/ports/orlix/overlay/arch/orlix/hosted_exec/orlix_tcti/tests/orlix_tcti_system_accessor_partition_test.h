/* SPDX-License-Identifier: GPL-2.0-only */
/* Included by orlix_tcti_source_leaf_classification_test.c. */
#ifndef ORLIX_TCTI_SYSTEM_ACCESSOR_PARTITION_TEST_H
#define ORLIX_TCTI_SYSTEM_ACCESSOR_PARTITION_TEST_H

enum orlix_tcti_system_accessor_direction {
	ORLIX_TCTI_SYSTEM_ACCESSOR_READ,
	ORLIX_TCTI_SYSTEM_ACCESSOR_WRITE,
};

enum orlix_tcti_system_accessor_disposition {
	ORLIX_TCTI_SYSTEM_ACCESSOR_EL0_IMPLEMENTED,
	ORLIX_TCTI_SYSTEM_ACCESSOR_PRIVILEGED_REJECTION,
	ORLIX_TCTI_SYSTEM_ACCESSOR_UNRESOLVED,
};

struct orlix_tcti_system_accessor_partition_row {
	u32 source_leaf_ordinal;
	const char *source_name;
	const char *generic_operation;
	enum orlix_tcti_system_accessor_direction direction;
	u16 selector;
	u32 register_source_identity;
	const char *register_name;
	const char *feature_predicate;
	enum orlix_tcti_system_accessor_disposition disposition;
};

struct orlix_tcti_system_accessor_register_source {
	u32 identity;
	const char *name;
};

#define TREG_SRC(...)
#define TREG_COUNTS(...)
#define TREG_META(...)
#define TREG_REG(identity, name, ...) { identity, name },
#define TREG_MEM(...)
#define TREG_IMPL(...)
#define TREG_FS(...)
#define TREG_F(...)
#define TREG_RANGE(...)
#define TREG_FB(...)
#define TREG_VS(...)
#define TREG_DOM(...)
#define TREG_CON(...)
#define TREG_CI(...)
#define TREG_LINK(...)
#define TREG_ACC(...)
#define TREG_AO(...)
#define TREG_ENC(...)
#define TREG_SEL(...)
#define TREG_LIT(...)
#define TREG_EQ(...)
#define TREG_SLICE(...)
#define TREG_GRP(...)
#define TREG_GF(...)
#define TREG_E(...)
#define TREG_EC(...)
static const struct orlix_tcti_system_accessor_register_source
	orlix_tcti_system_accessor_register_sources[] = {
#include "../isa/target_register_artifact.def"
};
#undef TREG_EC
#undef TREG_E
#undef TREG_GF
#undef TREG_GRP
#undef TREG_SLICE
#undef TREG_EQ
#undef TREG_LIT
#undef TREG_SEL
#undef TREG_ENC
#undef TREG_AO
#undef TREG_ACC
#undef TREG_LINK
#undef TREG_CI
#undef TREG_CON
#undef TREG_DOM
#undef TREG_VS
#undef TREG_FB
#undef TREG_RANGE
#undef TREG_F
#undef TREG_FS
#undef TREG_IMPL
#undef TREG_MEM
#undef TREG_REG
#undef TREG_META
#undef TREG_COUNTS
#undef TREG_SRC

static const char *orlix_tcti_system_accessor_register_source_name(u32 identity)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_system_accessor_register_sources);
	     index++)
		if (orlix_tcti_system_accessor_register_sources[index].identity == identity)
			return orlix_tcti_system_accessor_register_sources[index].name;
	return NULL;
}

#define ORLIX_TCTI_SYSTEM_ACCESSOR_PARTITION(ordinal, source_name, generic, \
		direction, selector, register_identity, register_name, feature, \
		disposition) \
	{ ordinal, source_name, generic, direction, selector, register_identity, \
	  register_name, feature, disposition },
static const struct orlix_tcti_system_accessor_partition_row
	orlix_tcti_system_accessor_partition[] = {
#include "../isa/system_accessor_partition.def"
};
#undef ORLIX_TCTI_SYSTEM_ACCESSOR_PARTITION

static enum orlix_tcti_system_accessor_disposition
orlix_tcti_system_accessor_partition_lookup(bool write, u16 selector)
{
	size_t index;
	enum orlix_tcti_system_accessor_direction direction = write ?
		ORLIX_TCTI_SYSTEM_ACCESSOR_WRITE : ORLIX_TCTI_SYSTEM_ACCESSOR_READ;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_system_accessor_partition); index++) {
		const struct orlix_tcti_system_accessor_partition_row *row =
			&orlix_tcti_system_accessor_partition[index];

		if (row->direction == direction && row->selector == selector)
			return row->disposition;
	}
	return ORLIX_TCTI_SYSTEM_ACCESSOR_UNRESOLVED;
}

static u32 orlix_tcti_system_accessor_partition_encode(bool write, u16 selector)
{
	return (write ? 0xd5100000U : 0xd5300000U) |
		((u32)selector << 5);
}

static void orlix_tcti_system_accessor_partition_binds_source_metadata(
	struct kunit *test)
{
	size_t index;

	KUNIT_ASSERT_EQ(test, 18U, ARRAY_SIZE(orlix_tcti_system_accessor_partition));
	for (index = 0; index < ARRAY_SIZE(orlix_tcti_system_accessor_partition); index++) {
		const struct orlix_tcti_system_accessor_partition_row *row =
			&orlix_tcti_system_accessor_partition[index];

		KUNIT_EXPECT_TRUE_MSG(test, row->source_leaf_ordinal == 2283U ||
				      row->source_leaf_ordinal == 2284U,
				      "%s", row->register_name);
		KUNIT_EXPECT_STREQ(test, "true", row->feature_predicate);
		KUNIT_EXPECT_STREQ_MSG(test,
			row->source_leaf_ordinal == 2283U ? "MSR_reg" : "MRS",
			row->generic_operation, "source ordinal %u",
			row->source_leaf_ordinal);
		KUNIT_EXPECT_NOT_NULL(test, row->source_name);
		KUNIT_EXPECT_NOT_NULL(test, row->generic_operation);
		KUNIT_EXPECT_NOT_NULL(test, row->register_name);
		KUNIT_EXPECT_NE(test, 0U, row->register_source_identity);
		KUNIT_EXPECT_STREQ_MSG(test,
			orlix_tcti_system_accessor_register_source_name(
				row->register_source_identity), row->register_name,
			"register source identity %u", row->register_source_identity);
	}

	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_SYSTEM_ACCESSOR_EL0_IMPLEMENTED,
		orlix_tcti_system_accessor_partition_lookup(false, 0xde82U));
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_SYSTEM_ACCESSOR_PRIVILEGED_REJECTION,
		orlix_tcti_system_accessor_partition_lookup(true, 0xde83U));
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_SYSTEM_ACCESSOR_UNRESOLVED,
		orlix_tcti_system_accessor_partition_lookup(false, 0xdce8U));
}

static void orlix_tcti_system_accessor_partition_matches_decoder_contract(
	struct kunit *test)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_system_accessor_partition); index++) {
		const struct orlix_tcti_system_accessor_partition_row *row =
			&orlix_tcti_system_accessor_partition[index];
		bool write = row->direction == ORLIX_TCTI_SYSTEM_ACCESSOR_WRITE;
		struct orlix_tcti_decoded_instruction decoded = orlix_tcti_decode_aarch64(
			orlix_tcti_system_accessor_partition_encode(write, row->selector));

		KUNIT_EXPECT_EQ_MSG(test,
			row->disposition == ORLIX_TCTI_SYSTEM_ACCESSOR_EL0_IMPLEMENTED ?
				ORLIX_TCTI_DECODE_SYSTEM_REGISTER : ORLIX_TCTI_DECODE_UNSUPPORTED,
			decoded.decode_class, "%s %s", row->source_name,
			row->register_name);
	}

	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
		orlix_tcti_decode_aarch64(orlix_tcti_system_accessor_partition_encode(false,
			0xdce8U)).decode_class);
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
		KUNIT_EXPECT_MEMEQ(test, before.regs, regs.regs,
			sizeof(regs.regs));
		KUNIT_EXPECT_EQ(test, before.pc, regs.pc);
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
	}
}

#endif /* ORLIX_TCTI_SYSTEM_ACCESSOR_PARTITION_TEST_H */
