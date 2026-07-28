// SPDX-License-Identifier: GPL-2.0-only
/* Source-bound production-path proof for AARCHMRS ordinals 2171-2172. */
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
#include "../gadget_program.h"

#define PCREL_SVC 0xd4000001U
#define PCREL_PROGRAM_OFFSET 0xabcU
#define PCREL_FIXED_MASK 0x9f000000U

struct pcrel_manifest_row {
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
static const struct pcrel_manifest_row pcrel_manifest[] = {
#include "../isa/source_manifest.def"
};
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE

struct pcrel_semantic_row {
	u32 ordinal;
	const char *name;
	const char *file;
	const char *decode_locator;
	const char *decode_digest;
	const char *execute_locator;
	const char *execute_digest;
};

#define ORLIX_TCTI_A64_SEMANTIC_PROVENANCE_SOURCE(...)
#define ORLIX_TCTI_A64_DDI0602_PROVENANCE_ROW(ordinal, name, file, decode, \
	decode_digest, decode_present, decode_statements, decode_helpers, execute, \
	execute_digest, execute_present, execute_statements, execute_helpers) \
	{ ordinal, name, file, decode, decode_digest, execute, execute_digest },
#define ORLIX_TCTI_A64_OFFICIAL_SEMANTICS_NOT_SPECIFIED_ROW(...)
static const struct pcrel_semantic_row pcrel_semantics[] = {
#include "../isa/generations/current/target_asl_availability.def"
};
#undef ORLIX_TCTI_A64_OFFICIAL_SEMANTICS_NOT_SPECIFIED_ROW
#undef ORLIX_TCTI_A64_DDI0602_PROVENANCE_ROW
#undef ORLIX_TCTI_A64_SEMANTIC_PROVENANCE_SOURCE

struct pcrel_leaf {
	u16 ordinal;
	const char *name;
	const char *mnemonic;
	const char *operation;
	u32 pattern;
	bool page_relative;
	const char *semantic_file;
	const char *decode_locator;
	const char *decode_digest;
	const char *execute_locator;
	const char *execute_digest;
};

static const struct pcrel_leaf pcrel_leaves[] = {
	{ 2171U, "ADR_only_pcreladdr", "ADR", "ADR", 0x10000000U, false,
	  "adr.xml", "adr.xml#A64.dpimm.pcreladdr.ADR_only_pcreladdr/decode",
	  "23a00a5f2479a041aabe368c66947091c3a819615e5af2b590d31c42e474a435",
	  "adr.xml#A64.dpimm.pcreladdr.ADR_only_pcreladdr/execute",
	  "77985a73784b766599b2c90d3c7b5b0b2cbc2c168c16cd5225e2c0ceb7516673" },
	{ 2172U, "ADRP_only_pcreladdr", "ADRP", "ADRP", 0x90000000U, true,
	  "adrp.xml", "adrp.xml#A64.dpimm.pcreladdr.ADRP_only_pcreladdr/decode",
	  "c4ebb77fdf05d6c1c1fc0ea11ec0ff6b61a5894739f3e258b7c2531f2720aec3",
	  "adrp.xml#A64.dpimm.pcreladdr.ADRP_only_pcreladdr/execute",
	  "c00a3036eec6709c432456fce23f37b10f69bbe365c039feecac7c89f40e1aa3" },
};

static const struct pcrel_manifest_row *pcrel_manifest_for(u32 ordinal)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(pcrel_manifest); index++)
		if (pcrel_manifest[index].ordinal == ordinal)
			return &pcrel_manifest[index];
	return NULL;
}

static const struct pcrel_semantic_row *pcrel_semantics_for(u32 ordinal)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(pcrel_semantics); index++)
		if (pcrel_semantics[index].ordinal == ordinal)
			return &pcrel_semantics[index];
	return NULL;
}

static u32 pcrel_encode(const struct pcrel_leaf *leaf, s32 immediate, u8 rd)
{
	u32 imm21 = (u32)immediate & (BIT(21) - 1);

	return leaf->pattern | ((imm21 & 3U) << 29) |
		((imm21 >> 2) << 5) | rd;
}

static s64 pcrel_byte_offset(const struct pcrel_leaf *leaf, u32 imm21)
{
	s64 immediate = sign_extend64(imm21, 20);

	return leaf->page_relative ? immediate * 4096 : immediate;
}

static void pcrel_seed_regs(struct pt_regs *regs, u64 pc)
{
	u8 index;

	memset(regs, 0, sizeof(*regs));
	for (index = 0; index < 31; index++)
		regs->regs[index] = 0x9e3779b97f4a7c15ULL ^
			((u64)index * 0x100000001b3ULL);
	regs->sp = 0x123456789abcdef0ULL;
	regs->pc = pc;
	regs->pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_C_BIT | PSR_D_BIT;
	regs->orig_x0 = 0xd1b54a32d192ed03ULL;
	regs->syscallno = NO_SYSCALL;
	regs->unused = 0x6d5a56c3U;
}

static void pcrel_expect_decode(struct kunit *test,
	const struct pcrel_leaf *leaf, u32 imm21, u8 rd)
{
	u32 instruction = pcrel_encode(leaf, sign_extend32(imm21, 20), rd);
	struct orlix_tcti_decoded_instruction decoded =
		orlix_tcti_decode_aarch64(instruction);

	KUNIT_ASSERT_EQ_MSG(test, ORLIX_TCTI_DECODE_PC_RELATIVE_ADDRESS,
		decoded.decode_class, "%s imm21=%#x rd=%u", leaf->name, imm21, rd);
	KUNIT_EXPECT_EQ(test, rd, decoded.rd);
	KUNIT_EXPECT_EQ(test, leaf->page_relative, decoded.page_relative);
	KUNIT_EXPECT_EQ(test, pcrel_byte_offset(leaf, imm21),
		decoded.pc_relative_imm);
}

static unsigned long pcrel_map_program(struct kunit *test, u32 instruction)
{
	const u32 program[] = { instruction, PCREL_SVC };
	unsigned long mapping;
	int ret;

	mapping = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapping));
	ret = orlix_tcti_write_user_data(current->mm,
		mapping + PCREL_PROGRAM_OFFSET, program, sizeof(program));
	KUNIT_ASSERT_EQ(test, 0, ret);
	ret = sys_mprotect(mapping, PAGE_SIZE, PROT_READ | PROT_EXEC);
	KUNIT_ASSERT_EQ(test, 0, ret);
	return mapping;
}

static void pcrel_source_provenance_and_classification(struct kunit *test)
{
	size_t index;

	KUNIT_ASSERT_EQ(test, 2U, ARRAY_SIZE(pcrel_leaves));
	for (index = 0; index < ARRAY_SIZE(pcrel_leaves); index++) {
		const struct pcrel_leaf *leaf = &pcrel_leaves[index];
		const struct pcrel_manifest_row *manifest =
			pcrel_manifest_for(leaf->ordinal);
		const struct pcrel_semantic_row *semantic =
			pcrel_semantics_for(leaf->ordinal);

		KUNIT_EXPECT_EQ(test, 2171U + index, leaf->ordinal);
		KUNIT_ASSERT_NOT_NULL(test, manifest);
		KUNIT_EXPECT_STREQ(test, leaf->name, manifest->name);
		KUNIT_EXPECT_STREQ(test, leaf->mnemonic, manifest->mnemonic);
		KUNIT_EXPECT_STREQ(test, leaf->operation, manifest->operation);
		KUNIT_EXPECT_EQ(test, PCREL_FIXED_MASK, manifest->mask);
		KUNIT_EXPECT_EQ(test, leaf->pattern, manifest->pattern);
		KUNIT_EXPECT_TRUE(test, manifest->condition[0]);
		KUNIT_EXPECT_GT(test, manifest->source_offset, 0ULL);
		KUNIT_EXPECT_GT(test, manifest->source_length, 0ULL);
		KUNIT_ASSERT_NOT_NULL(test, semantic);
		KUNIT_EXPECT_STREQ(test, leaf->name, semantic->name);
		KUNIT_EXPECT_STREQ(test, leaf->semantic_file, semantic->file);
		KUNIT_EXPECT_STREQ(test, leaf->decode_locator,
			semantic->decode_locator);
		KUNIT_EXPECT_STREQ(test, leaf->decode_digest,
			semantic->decode_digest);
		KUNIT_EXPECT_STREQ(test, leaf->execute_locator,
			semantic->execute_locator);
		KUNIT_EXPECT_STREQ(test, leaf->execute_digest,
			semantic->execute_digest);
	}
}

static void pcrel_all_immediates_and_destinations_decode(struct kunit *test)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(pcrel_leaves); index++) {
		const struct pcrel_leaf *leaf = &pcrel_leaves[index];
		u32 imm21;
		u8 rd;

		for (imm21 = 0; imm21 < BIT(21); imm21++)
			pcrel_expect_decode(test, leaf, imm21, 17);
		for (rd = 0; rd < 32; rd++) {
			pcrel_expect_decode(test, leaf, 0, rd);
			pcrel_expect_decode(test, leaf, BIT(20), rd);
			pcrel_expect_decode(test, leaf, BIT(20) - 1, rd);
		}
	}
}

static void pcrel_fixed_bit_neighbours_reject_without_state(struct kunit *test)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(pcrel_leaves); index++) {
		const struct pcrel_leaf *leaf = &pcrel_leaves[index];
		bool structured_rejection = false;
		u8 bit;

		for (bit = 0; bit < 32; bit++) {
			u32 instruction;
			struct orlix_tcti_decoded_instruction decoded;

			if (!(PCREL_FIXED_MASK & BIT(bit)))
				continue;
			instruction = pcrel_encode(leaf, -1, 17) ^ BIT(bit);
			decoded = orlix_tcti_decode_aarch64(instruction);
			KUNIT_EXPECT_NE_MSG(test,
				ORLIX_TCTI_DECODE_PC_RELATIVE_ADDRESS,
				decoded.decode_class, "%s bit=%u", leaf->name, bit);
			if (decoded.decode_class == ORLIX_TCTI_DECODE_UNSUPPORTED) {
				struct pt_regs regs;
				struct pt_regs before;
				struct orlix_tcti_result result;
				unsigned long mapping =
					pcrel_map_program(test, instruction);

				pcrel_seed_regs(&regs, mapping + PCREL_PROGRAM_OFFSET);
				before = regs;
				result = orlix_tcti_resume_user(current, &regs, current->mm);
				KUNIT_EXPECT_EQ(test,
					ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
					result.reason);
				KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, result.status);
				KUNIT_EXPECT_EQ(test, before.pc, result.pc);
				KUNIT_EXPECT_EQ(test, instruction, result.instruction);
				KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
				KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapping, PAGE_SIZE));
				structured_rejection = true;
			}
		}
		KUNIT_EXPECT_TRUE_MSG(test, structured_rejection, "%s", leaf->name);
	}
}

static void pcrel_production_resume_pc_page_and_destination(struct kunit *test)
{
	static const s32 immediates[] = {
		-(s32)BIT(20), -1, 0, 1, BIT(20) - 1,
	};
	static const u8 destinations[] = { 0, 17, 30, 31 };
	size_t leaf_index;

	for (leaf_index = 0; leaf_index < ARRAY_SIZE(pcrel_leaves); leaf_index++) {
		const struct pcrel_leaf *leaf = &pcrel_leaves[leaf_index];
		size_t immediate_index;

		for (immediate_index = 0;
		     immediate_index < ARRAY_SIZE(immediates); immediate_index++) {
			size_t destination_index;

			for (destination_index = 0;
			     destination_index < ARRAY_SIZE(destinations);
			     destination_index++) {
				u8 rd = destinations[destination_index];
				u32 instruction = pcrel_encode(
					leaf, immediates[immediate_index], rd);
				unsigned long mapping =
					pcrel_map_program(test, instruction);
				unsigned int pass;

				for (pass = 0; pass < 2; pass++) {
					struct pt_regs regs;
					struct pt_regs expected;
					struct orlix_tcti_result result;
					u64 instruction_pc =
						mapping + PCREL_PROGRAM_OFFSET;
					u64 base = leaf->page_relative ?
						instruction_pc & ~0xfffULL : instruction_pc;
					u64 offset = leaf->page_relative ?
						(s64)immediates[immediate_index] * 4096 :
						(s64)immediates[immediate_index];

					pcrel_seed_regs(&regs, instruction_pc);
					expected = regs;
					if (rd != 31)
						expected.regs[rd] = base + offset;
					expected.pc += sizeof(u32);
					result = orlix_tcti_resume_user(
						current, &regs, current->mm);
					KUNIT_ASSERT_EQ_MSG(test, ORLIX_TCTI_EXIT_SYSCALL,
						result.reason, "%s imm=%d rd=%u pass=%u",
						leaf->name, immediates[immediate_index], rd,
						pass);
					KUNIT_EXPECT_EQ(test, 0L, result.status);
					KUNIT_EXPECT_EQ(test, expected.pc, result.pc);
					KUNIT_EXPECT_EQ(test, PCREL_SVC, result.instruction);
					KUNIT_EXPECT_MEMEQ(test, &expected, &regs, sizeof(regs));
				}
				KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapping, PAGE_SIZE));
			}
		}
	}
}

static void pcrel_lowered_gadget_wraparound_semantics(struct kunit *test)
{
	static const u64 pcs[] = {
		0x123456789abcULL, 0x123456789fffULL,
		0xfffffffffffffff0ULL, 0xffffffffffffffffULL,
	};
	struct orlix_tcti_decoded_instruction generic_decoded =
		orlix_tcti_decode_aarch64(0xd503201fU);
	struct orlix_tcti_gadget_word generic_program[
		ORLIX_TCTI_SINGLE_INSTRUCTION_PROGRAM_WORDS];
	size_t generic_word_count = 0;
	size_t leaf_index;

	KUNIT_ASSERT_EQ(test, ORLIX_TCTI_DECODE_HINT,
		generic_decoded.decode_class);
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_lower_decoded_instruction(&generic_decoded,
			generic_program, ARRAY_SIZE(generic_program),
			&generic_word_count));
	KUNIT_ASSERT_EQ(test,
		(size_t)ORLIX_TCTI_SINGLE_INSTRUCTION_PROGRAM_WORDS,
		generic_word_count);

	for (leaf_index = 0; leaf_index < ARRAY_SIZE(pcrel_leaves); leaf_index++) {
		const struct pcrel_leaf *leaf = &pcrel_leaves[leaf_index];
		size_t pc_index;

		for (pc_index = 0; pc_index < ARRAY_SIZE(pcs); pc_index++) {
			u8 rd;

			for (rd = 30; rd < 32; rd++) {
				u32 instruction = pcrel_encode(leaf, -1, rd);
				struct orlix_tcti_decoded_instruction decoded =
					orlix_tcti_decode_aarch64(instruction);
				struct orlix_tcti_gadget_word program[
					ORLIX_TCTI_SINGLE_INSTRUCTION_PROGRAM_WORDS];
				struct pt_regs regs;
				struct pt_regs expected;
				u64 base = leaf->page_relative ?
					pcs[pc_index] & ~0xfffULL : pcs[pc_index];
				u64 offset = leaf->page_relative ? (u64)-4096 : U64_MAX;
				unsigned long fault_address = 0;
				size_t word_count = 0;

				pcrel_seed_regs(&regs, pcs[pc_index]);
				expected = regs;
				if (rd != 31)
					expected.regs[rd] = base + offset;
				expected.pc += sizeof(u32);
				KUNIT_ASSERT_EQ(test, 0,
					orlix_tcti_lower_decoded_instruction(&decoded,
						program, ARRAY_SIZE(program), &word_count));
				KUNIT_EXPECT_EQ(test,
					(size_t)ORLIX_TCTI_SINGLE_INSTRUCTION_PROGRAM_WORDS,
					word_count);
				KUNIT_EXPECT_NE(test, generic_program[0].value,
					program[0].value);
				KUNIT_ASSERT_EQ(test, 0,
					orlix_tcti_execute_gadget_program(NULL, &regs, program,
						word_count, &fault_address));
				KUNIT_EXPECT_EQ(test, 0UL, fault_address);
				KUNIT_EXPECT_MEMEQ(test, &expected, &regs, sizeof(regs));
			}
		}
	}
}

static struct kunit_case pcrel_cases[] = {
	KUNIT_CASE(pcrel_source_provenance_and_classification),
	KUNIT_CASE(pcrel_all_immediates_and_destinations_decode),
	KUNIT_CASE(pcrel_fixed_bit_neighbours_reject_without_state),
	KUNIT_CASE(pcrel_production_resume_pc_page_and_destination),
	KUNIT_CASE(pcrel_lowered_gadget_wraparound_semantics),
	{}
};

static struct kunit_suite pcrel_suite = {
	.name = "orlix-tcti-pc-relative-source-bound",
	.test_cases = pcrel_cases,
};

kunit_test_suite(pcrel_suite);
