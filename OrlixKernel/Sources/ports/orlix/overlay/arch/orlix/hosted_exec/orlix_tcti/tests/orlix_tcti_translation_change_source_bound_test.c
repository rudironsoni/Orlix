// SPDX-License-Identifier: GPL-2.0-only
/* Production EL0-rejection proof for AARCHMRS 2026-06 ordinals 2308-2311. */
#include <asm/orlix_tcti.h>
#include <asm/processor.h>
#include <asm/ptrace.h>
#include <kunit/test.h>
#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/syscalls.h>

#include "../decode_aarch64.h"

#define TCHANGE_SVC 0xd4000001U
#define TCHANGE_FEATURE_CONDITION \
	"54434e440107000000300700000017070000000c010000000101010000000101010000000101020000000f0000000b464541545f5331504f4532"

struct tchange_manifest_row {
	u32 ordinal;
	const char *name;
	const char *mnemonic;
	const char *operation;
	u32 mask;
	u32 pattern;
	const char *condition;
	u64 source_offset;
	u64 source_length;
};

#define ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE(...)
#define ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW(ordinal, name, mnemonic, operation, \
	mask, pattern, condition, offset, length) \
	{ ordinal, name, mnemonic, operation, mask, pattern, condition, offset, length },
static const struct tchange_manifest_row tchange_manifest[] = {
#include "../isa/source_manifest.def"
};
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE

struct tchange_semantics_absence_row {
	u32 ordinal;
	const char *name;
	const char *locator;
	u64 offset;
	u64 length;
	const char *digest;
};

#define ORLIX_TCTI_A64_SEMANTIC_PROVENANCE_SOURCE(...)
#define ORLIX_TCTI_A64_DDI0602_PROVENANCE_ROW(...)
#define ORLIX_TCTI_A64_ASL_AVAILABILITY_ROW(...)
#define ORLIX_TCTI_A64_ASL_XML_ROW(...)
#define ORLIX_TCTI_A64_OFFICIAL_SEMANTICS_NOT_SPECIFIED_ROW(ordinal, name, \
	locator, offset, length, digest) \
	{ ordinal, name, locator, offset, length, digest },
static const struct tchange_semantics_absence_row tchange_semantics_absence[] = {
#include "../isa/generations/current/target_asl_availability.def"
};
#undef ORLIX_TCTI_A64_OFFICIAL_SEMANTICS_NOT_SPECIFIED_ROW
#undef ORLIX_TCTI_A64_ASL_XML_ROW
#undef ORLIX_TCTI_A64_ASL_AVAILABILITY_ROW
#undef ORLIX_TCTI_A64_DDI0602_PROVENANCE_ROW
#undef ORLIX_TCTI_A64_SEMANTIC_PROVENANCE_SOURCE

struct tchange_leaf {
	u32 ordinal;
	const char *name;
	const char *mnemonic;
	const char *operation;
	u32 mask;
	u32 pattern;
	bool immediate;
};

static const struct tchange_leaf tchange_leaves[] = {
	{ 2308U, "TCHANGEB_tc_reg", "TCHANGEB", "TCHANGEB_reg",
	  0xfffdfc00U, 0xd5840000U, false },
	{ 2309U, "TCHANGEF_tc_reg", "TCHANGEF", "TCHANGEF_reg",
	  0xfffdfc00U, 0xd5800000U, false },
	{ 2310U, "TCHANGEB_tc_imm", "TCHANGEB", "TCHANGEB_imm",
	  0xfffdf000U, 0xd5940000U, true },
	{ 2311U, "TCHANGEF_tc_imm", "TCHANGEF", "TCHANGEF_imm",
	  0xfffdf000U, 0xd5900000U, true },
};

static const struct tchange_manifest_row *tchange_manifest_for(u32 ordinal)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(tchange_manifest); index++)
		if (tchange_manifest[index].ordinal == ordinal)
			return &tchange_manifest[index];
	return NULL;
}

static const struct tchange_semantics_absence_row *
tchange_semantics_absence_for(u32 ordinal)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(tchange_semantics_absence); index++)
		if (tchange_semantics_absence[index].ordinal == ordinal)
			return &tchange_semantics_absence[index];
	return NULL;
}

static unsigned long tchange_map_program(struct kunit *test, u32 instruction)
{
	const u32 program[] = { instruction, TCHANGE_SVC };
	unsigned long mapping;
	int ret;

	mapping = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapping));
	ret = orlix_tcti_write_user_data(current->mm, mapping, program,
					  sizeof(program));
	KUNIT_ASSERT_EQ(test, 0, ret);
	ret = sys_mprotect(mapping, PAGE_SIZE, PROT_READ | PROT_EXEC);
	KUNIT_ASSERT_EQ(test, 0, ret);
	return mapping;
}

static void tchange_seed_regs(struct pt_regs *regs, unsigned long pc)
{
	u8 index;

	memset(regs, 0, sizeof(*regs));
	for (index = 0; index < 31; index++)
		regs->regs[index] = 0x9e3779b97f4a7c15ULL + index;
	regs->sp = 0x123456789abcdef0ULL;
	regs->pc = pc;
	regs->pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_C_BIT | PSR_D_BIT;
	regs->syscallno = NO_SYSCALL;
}

static void tchange_source_and_feature_domain(struct kunit *test)
{
	size_t index;

	KUNIT_ASSERT_EQ(test, 4U, ARRAY_SIZE(tchange_leaves));
	for (index = 0; index < ARRAY_SIZE(tchange_leaves); index++) {
		const struct tchange_leaf *leaf = &tchange_leaves[index];
		const struct tchange_manifest_row *manifest =
			tchange_manifest_for(leaf->ordinal);
		const struct tchange_semantics_absence_row *absence =
			tchange_semantics_absence_for(leaf->ordinal);

		KUNIT_EXPECT_EQ(test, 2308U + index, leaf->ordinal);
		KUNIT_ASSERT_NOT_NULL(test, manifest);
		KUNIT_EXPECT_STREQ(test, leaf->name, manifest->name);
		KUNIT_EXPECT_STREQ(test, leaf->mnemonic, manifest->mnemonic);
		KUNIT_EXPECT_STREQ(test, leaf->operation, manifest->operation);
		KUNIT_EXPECT_EQ(test, leaf->mask, manifest->mask);
		KUNIT_EXPECT_EQ(test, leaf->pattern, manifest->pattern);
		KUNIT_EXPECT_STREQ(test, TCHANGE_FEATURE_CONDITION,
				   manifest->condition);
		KUNIT_EXPECT_GT(test, manifest->source_offset, 0ULL);
		KUNIT_EXPECT_GT(test, manifest->source_length, 0ULL);
		KUNIT_ASSERT_NOT_NULL(test, absence);
		KUNIT_EXPECT_STREQ(test, leaf->name, absence->name);
		KUNIT_EXPECT_TRUE(test, absence->locator[0]);
		KUNIT_EXPECT_GT(test, absence->offset, 0ULL);
		KUNIT_EXPECT_GT(test, absence->length, 0ULL);
		KUNIT_EXPECT_TRUE(test, absence->digest[0]);
	}
}

static void tchange_all_source_encodings_decode_unsupported(struct kunit *test)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(tchange_leaves); index++) {
		const struct tchange_leaf *leaf = &tchange_leaves[index];
		u32 variable_mask = ~leaf->mask;
		u32 variable_fields = 0;
		u32 variable_count = 0;

		do {
			u32 instruction = leaf->pattern | variable_fields;
			struct orlix_tcti_decoded_instruction decoded =
				orlix_tcti_decode_aarch64(instruction);

			KUNIT_EXPECT_EQ_MSG(test, leaf->pattern,
					instruction & leaf->mask,
					"%s variable fields %#x", leaf->name,
					variable_fields);
			KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
					decoded.decode_class,
					"%s accepted encoding %#x", leaf->name,
					instruction);
			variable_count++;
			variable_fields =
				(variable_fields - variable_mask) & variable_mask;
		} while (variable_fields);
		KUNIT_EXPECT_EQ(test, 1U << hweight32(variable_mask), variable_count);
	}
}

static u32 tchange_encode(const struct tchange_leaf *leaf, u8 rd, u8 operand,
			  bool not_balanced)
{
	u32 instruction = leaf->pattern | rd | (not_balanced ? BIT(17) : 0);

	return instruction | ((u32)operand << 5);
}

static void tchange_production_el0_rejection_preserves_state(struct kunit *test)
{
	static const u8 registers[] = { 0U, 30U };
	static const u8 register_operands[] = { 0U, 30U };
	static const u8 immediate_operands[] = { 0U, 127U };
	size_t leaf_index;

	for (leaf_index = 0; leaf_index < ARRAY_SIZE(tchange_leaves); leaf_index++) {
		const struct tchange_leaf *leaf = &tchange_leaves[leaf_index];
		size_t rd_index;

		for (rd_index = 0; rd_index < ARRAY_SIZE(registers); rd_index++) {
			size_t operand_index;
			size_t operand_count = leaf->immediate ?
				ARRAY_SIZE(immediate_operands) :
				ARRAY_SIZE(register_operands);

			for (operand_index = 0; operand_index < operand_count;
			     operand_index++) {
				unsigned int not_balanced;
				u8 operand = leaf->immediate ?
					immediate_operands[operand_index] :
					register_operands[operand_index];

				for (not_balanced = 0; not_balanced < 2;
				     not_balanced++) {
					u32 instruction = tchange_encode(leaf, registers[rd_index],
						operand, not_balanced);
					unsigned long mapping = tchange_map_program(test,
						instruction);
					struct pt_regs regs;
					struct pt_regs before;
					struct orlix_tcti_result result;

					tchange_seed_regs(&regs, mapping);
					before = regs;
					result = orlix_tcti_resume_user(current, &regs,
									 current->mm);
					KUNIT_EXPECT_EQ_MSG(test,
						ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
						result.reason, "%s rd=%u operand=%u nb=%u",
						leaf->name, registers[rd_index],
						operand, not_balanced);
					KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, result.status);
					KUNIT_EXPECT_EQ(test, instruction, result.instruction);
					KUNIT_EXPECT_EQ(test, before.pc, result.pc);
					KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
					KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapping, PAGE_SIZE));
				}
			}
		}
	}
}

static struct kunit_case tchange_cases[] = {
	KUNIT_CASE(tchange_source_and_feature_domain),
	KUNIT_CASE(tchange_all_source_encodings_decode_unsupported),
	KUNIT_CASE(tchange_production_el0_rejection_preserves_state),
	{}
};

static struct kunit_suite tchange_suite = {
	.name = "orlix-tcti-translation-change-source-bound",
	.test_cases = tchange_cases,
};

kunit_test_suite(tchange_suite);
