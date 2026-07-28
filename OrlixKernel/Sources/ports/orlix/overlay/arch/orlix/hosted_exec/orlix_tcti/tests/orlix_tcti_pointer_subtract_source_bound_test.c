// SPDX-License-Identifier: GPL-2.0-only
/* Source-bound production proof for the pinned SUBP and SUBPS leaves. */
#include <asm/orlix_tcti.h>
#include <asm/ptrace.h>
#include <kunit/test.h>
#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/string.h>
#include <linux/syscalls.h>

#include "../decode_aarch64.h"
#include "../gadget_program.h"
#include "orlix_tcti_native_observation.h"

#define POINTER_SUBTRACT_SVC 0xd4000001U
#define POINTER_SUBTRACT_NZCV \
	(PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT | PSR_V_BIT)
#define POINTER_SUBTRACT_FEAT_MTE_CONDITION \
	"54434e4401070000002d0700000017070000000c010000000101010000000101" \
	"010000000101020000000c00000008464541545f4d5445"

enum pointer_subtract_architectural_classification {
	POINTER_SUBTRACT_REQUIRED_EL0_FEAT_MTE,
};

enum pointer_subtract_linux_interface_disposition {
	POINTER_SUBTRACT_KSELFTEST_NOT_APPLICABLE_REGISTER_ONLY,
};

enum pointer_subtract_semantic_provenance {
	POINTER_SUBTRACT_PROVENANCE_EXTERNAL_DDI0602_2026_06,
};

struct pointer_subtract_manifest_leaf {
	u32 ordinal;
	const char *name;
	const char *mnemonic;
	const char *operation;
	u32 mask;
	u32 pattern;
	const char *feature_condition;
};

#define ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE(...)
#define ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW(ordinal, name, mnemonic, operation, \
	mask, pattern, feature, offset, length) \
	{ ordinal, name, mnemonic, operation, mask, pattern, feature },
static const struct pointer_subtract_manifest_leaf pointer_subtract_manifest[] = {
#include "../isa/source_manifest.def"
};
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE

struct pointer_subtract_provenance_leaf {
	u32 ordinal;
	const char *name;
	const char *relative_file;
	const char *decode_locator;
	const char *execute_locator;
	enum pointer_subtract_semantic_provenance provenance;
};

#define ORLIX_TCTI_A64_SEMANTIC_PROVENANCE_SOURCE(...)
#define ORLIX_TCTI_A64_DDI0602_PROVENANCE_ROW(ordinal, name, relative_file, \
	decode_locator, decode_digest, decode_present, decode_helpers, \
	decode_helpers_digest, execute_locator, execute_digest, execute_present, \
	execute_helpers, execute_helpers_digest) \
	{ ordinal, name, relative_file, decode_locator, execute_locator, \
	  POINTER_SUBTRACT_PROVENANCE_EXTERNAL_DDI0602_2026_06 },
#define ORLIX_TCTI_A64_OFFICIAL_SEMANTICS_NOT_SPECIFIED_ROW(...)
static const struct pointer_subtract_provenance_leaf pointer_subtract_provenance[] = {
#include "../isa/target_asl_availability.def"
};
#undef ORLIX_TCTI_A64_OFFICIAL_SEMANTICS_NOT_SPECIFIED_ROW
#undef ORLIX_TCTI_A64_DDI0602_PROVENANCE_ROW
#undef ORLIX_TCTI_A64_SEMANTIC_PROVENANCE_SOURCE

struct pointer_subtract_leaf {
	u32 ordinal;
	const char *name;
	const char *mnemonic;
	u32 pattern;
	bool set_flags;
	enum pointer_subtract_architectural_classification classification;
	enum pointer_subtract_linux_interface_disposition kselftest;
};

static const struct pointer_subtract_leaf pointer_subtract_leaves[] = {
	{ 3372U, "SUBP_64S_dp_2src", "SUBP", 0x9ac00000U, false,
	  POINTER_SUBTRACT_REQUIRED_EL0_FEAT_MTE,
	  POINTER_SUBTRACT_KSELFTEST_NOT_APPLICABLE_REGISTER_ONLY },
	{ 3388U, "SUBPS_64S_dp_2src", "SUBPS", 0xbac00000U, true,
	  POINTER_SUBTRACT_REQUIRED_EL0_FEAT_MTE,
	  POINTER_SUBTRACT_KSELFTEST_NOT_APPLICABLE_REGISTER_ONLY },
};

static const struct pointer_subtract_manifest_leaf *
pointer_subtract_manifest_for_ordinal(u32 ordinal)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(pointer_subtract_manifest); index++)
		if (pointer_subtract_manifest[index].ordinal == ordinal)
			return &pointer_subtract_manifest[index];
	return NULL;
}

static const struct pointer_subtract_provenance_leaf *
pointer_subtract_provenance_for_ordinal(u32 ordinal)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(pointer_subtract_provenance); index++)
		if (pointer_subtract_provenance[index].ordinal == ordinal)
			return &pointer_subtract_provenance[index];
	return NULL;
}

static const struct pointer_subtract_leaf *
pointer_subtract_leaf_for_instruction(u32 instruction)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(pointer_subtract_leaves); index++)
		if ((instruction & 0xffe0fc00U) ==
		    pointer_subtract_leaves[index].pattern)
			return &pointer_subtract_leaves[index];
	return NULL;
}

static u32 pointer_subtract_instruction(const struct pointer_subtract_leaf *leaf,
					u8 rd, u8 rn, u8 rm)
{
	return leaf->pattern | ((u32)rm << 16) | ((u32)rn << 5) | rd;
}

static int pointer_subtract_suite_init(struct kunit *test)
{
	struct mm_struct *mm = mm_alloc();

	if (!mm)
		return -ENOMEM;
	test->priv = mm;
	return 0;
}

static void pointer_subtract_suite_exit(struct kunit *test)
{
	struct mm_struct *mm = test->priv;

	if (mm)
		mmput(mm);
}

static bool pointer_subtract_attach_mm(struct kunit *test)
{
	struct mm_struct *mm = test->priv;

	KUNIT_EXPECT_NOT_NULL(test, mm);
	KUNIT_EXPECT_NULL(test, current->mm);
	if (!mm || current->mm)
		return false;
	kthread_use_mm(mm);
	return true;
}

static void pointer_subtract_detach_mm(struct kunit *test)
{
	struct mm_struct *mm = test->priv;

	KUNIT_EXPECT_PTR_EQ(test, current->mm, mm);
	if (current->mm == mm)
		kthread_unuse_mm(mm);
}

static int pointer_subtract_map_program(struct kunit *test, u32 instruction,
					unsigned long *address)
{
	u32 program[] = { instruction, POINTER_SUBTRACT_SVC };
	unsigned long mapped;
	int ret;

	mapped = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_EXPECT_FALSE(test, IS_ERR_VALUE(mapped));
	if (IS_ERR_VALUE(mapped))
		return (int)(long)mapped;
	ret = orlix_tcti_write_user_data(current->mm, mapped, program,
					 sizeof(program));
	KUNIT_EXPECT_EQ(test, 0, ret);
	if (ret)
		goto unmap;
	ret = sys_mprotect(mapped, PAGE_SIZE, PROT_READ | PROT_EXEC);
	KUNIT_EXPECT_EQ(test, 0, ret);
	if (ret)
		goto unmap;
	*address = mapped;
	return 0;

unmap:
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
	return ret;
}

static u64 pointer_subtract_operand(u64 value)
{
	return sign_extend64(value, 55);
}

static unsigned long pointer_subtract_expected_nzcv(u64 left, u64 right,
						     u64 result)
{
	unsigned long nzcv = 0;

	if (result & BIT_ULL(63))
		nzcv |= PSR_N_BIT;
	if (!result)
		nzcv |= PSR_Z_BIT;
	if (left >= right)
		nzcv |= PSR_C_BIT;
	if (((left ^ right) & (left ^ result) & BIT_ULL(63)) != 0)
		nzcv |= PSR_V_BIT;
	return nzcv;
}

static void pointer_subtract_source_decode_and_lowering(struct kunit *test)
{
	size_t index;

	KUNIT_ASSERT_EQ(test, 2U, ARRAY_SIZE(pointer_subtract_leaves));
	for (index = 0; index < ARRAY_SIZE(pointer_subtract_leaves); index++) {
		const struct pointer_subtract_leaf *leaf =
			&pointer_subtract_leaves[index];
		const struct pointer_subtract_manifest_leaf *source =
			pointer_subtract_manifest_for_ordinal(leaf->ordinal);
		const struct pointer_subtract_provenance_leaf *provenance =
			pointer_subtract_provenance_for_ordinal(leaf->ordinal);
		u8 rd;
		u8 rn;
		u8 rm;

		KUNIT_ASSERT_NOT_NULL(test, source);
		KUNIT_ASSERT_NOT_NULL(test, provenance);
		KUNIT_EXPECT_STREQ(test, leaf->name, source->name);
		KUNIT_EXPECT_STREQ(test, leaf->mnemonic, source->mnemonic);
		KUNIT_EXPECT_STREQ(test, leaf->mnemonic, source->operation);
		KUNIT_EXPECT_EQ(test, 0xffe0fc00U, source->mask);
		KUNIT_EXPECT_EQ(test, leaf->pattern, source->pattern);
		KUNIT_EXPECT_STREQ(test, POINTER_SUBTRACT_FEAT_MTE_CONDITION,
				   source->feature_condition);
		KUNIT_EXPECT_STREQ(test, leaf->name, provenance->name);
		KUNIT_EXPECT_NOT_NULL(test, strstr(provenance->relative_file,
						       leaf->set_flags ? "subps.xml" :
								 "subp.xml"));
		KUNIT_EXPECT_NOT_NULL(test, strstr(provenance->decode_locator,
						       "/decode"));
		KUNIT_EXPECT_NOT_NULL(test, strstr(provenance->execute_locator,
						       "/execute"));
		KUNIT_EXPECT_EQ(test,
			POINTER_SUBTRACT_PROVENANCE_EXTERNAL_DDI0602_2026_06,
			provenance->provenance);
		KUNIT_EXPECT_EQ(test, POINTER_SUBTRACT_REQUIRED_EL0_FEAT_MTE,
				leaf->classification);
		KUNIT_EXPECT_EQ(test,
			POINTER_SUBTRACT_KSELFTEST_NOT_APPLICABLE_REGISTER_ONLY,
			leaf->kselftest);

		for (rd = 0; rd < 32; rd++)
			for (rn = 0; rn < 32; rn++)
				for (rm = 0; rm < 32; rm++) {
					struct orlix_tcti_decoded_instruction decoded =
						orlix_tcti_decode_aarch64(
							pointer_subtract_instruction(
								leaf, rd, rn, rm));
					struct orlix_tcti_gadget_word program[
						ORLIX_TCTI_SINGLE_INSTRUCTION_PROGRAM_WORDS];
					size_t word_count = 0;

					KUNIT_ASSERT_EQ(test,
						ORLIX_TCTI_DECODE_DATA_PROCESSING_2SOURCE,
						decoded.decode_class);
					KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DP2_SUBP,
							decoded.dp2_op);
					KUNIT_EXPECT_TRUE(test, decoded.is_64bit);
					KUNIT_EXPECT_EQ(test, leaf->set_flags,
							decoded.set_flags);
					KUNIT_EXPECT_EQ(test, rd, decoded.rd);
					KUNIT_EXPECT_EQ(test, rn, decoded.rn);
					KUNIT_EXPECT_EQ(test, rm, decoded.rm);
					KUNIT_ASSERT_EQ(test, 0,
						orlix_tcti_lower_decoded_instruction(
							&decoded, program,
							ARRAY_SIZE(program), &word_count));
					KUNIT_EXPECT_EQ(test,
						1U + ORLIX_TCTI_DECODED_INSTRUCTION_WORDS,
						word_count);
				}
	}
}

static void pointer_subtract_fixed_neighbours_reject(struct kunit *test)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(pointer_subtract_leaves); index++) {
		const struct pointer_subtract_leaf *leaf =
			&pointer_subtract_leaves[index];
		u8 bit;
		unsigned int rejected = 0;

		for (bit = 0; bit < 32; bit++) {
			u32 instruction;

			if (!(0xffe0fc00U & BIT(bit)))
				continue;
			instruction = pointer_subtract_instruction(leaf, 3, 5, 7) ^
				BIT(bit);
			if (pointer_subtract_leaf_for_instruction(instruction))
				continue;
			if (orlix_tcti_decode_aarch64(instruction).decode_class ==
			    ORLIX_TCTI_DECODE_UNSUPPORTED)
				rejected++;
		}
		KUNIT_EXPECT_GT_MSG(test, rejected, 0U, "%s", leaf->name);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
			orlix_tcti_decode_aarch64(
				pointer_subtract_instruction(leaf, 3, 5, 7) ^ BIT(10))
				.decode_class);
	}
}

struct pointer_subtract_vector {
	u8 rd;
	u8 rn;
	u8 rm;
	u64 rn_value;
	u64 rm_value;
	u64 sp_value;
};

static const struct pointer_subtract_vector pointer_subtract_vectors[] = {
	{ 3, 5, 7, 0xab00000000000120ULL, 0xcd00000000000020ULL,
	  0x0000000000003000ULL },
	{ 4, 31, 8, 0, 0xfe00000000000040ULL, 0x0100000000000020ULL },
	{ 6, 9, 31, 0x007ffffffffffff0ULL, 0, 0xff80000000000010ULL },
	{ 31, 10, 11, 0x0080000000000010ULL, 0x007ffffffffffff0ULL,
	  0x0000000000004000ULL },
};

static void pointer_subtract_native_production_observations(struct kunit *test)
{
	size_t leaf_index;

	if (!pointer_subtract_attach_mm(test))
		return;
	for (leaf_index = 0; leaf_index < ARRAY_SIZE(pointer_subtract_leaves);
	     leaf_index++) {
		const struct pointer_subtract_leaf *leaf =
			&pointer_subtract_leaves[leaf_index];
		size_t vector_index;

		for (vector_index = 0;
		     vector_index < ARRAY_SIZE(pointer_subtract_vectors);
		     vector_index++) {
			const struct pointer_subtract_vector *vector =
				&pointer_subtract_vectors[vector_index];
			struct orlix_tcti_native_observation_spec spec = {};
			struct orlix_tcti_native_observation *observation;
			struct pt_regs regs = {};
			struct pt_regs expected;
			unsigned long address = 0;
			u64 left;
			u64 right;
			u64 result;
			int ret;

			ret = pointer_subtract_map_program(test,
				pointer_subtract_instruction(leaf, vector->rd,
					vector->rn, vector->rm), &address);
			if (ret)
				goto detach;
			regs.regs[vector->rn == 31 ? 0 : vector->rn] =
				vector->rn_value;
			regs.regs[vector->rm == 31 ? 1 : vector->rm] =
				vector->rm_value;
			regs.regs[3] ^= 0x123456789abcdef0ULL;
			regs.sp = vector->sp_value;
			regs.pc = address;
			regs.pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_C_BIT;
			regs.syscallno = NO_SYSCALL;
			expected = regs;
			left = pointer_subtract_operand(vector->rn == 31 ? regs.sp :
						regs.regs[vector->rn]);
			right = pointer_subtract_operand(vector->rm == 31 ? regs.sp :
						 regs.regs[vector->rm]);
			result = left - right;
			if (vector->rd != 31)
				expected.regs[vector->rd] = result;
			if (leaf->set_flags)
				expected.pstate =
					(expected.pstate & ~POINTER_SUBTRACT_NZCV) |
					pointer_subtract_expected_nzcv(left, right, result);
			expected.pc = address + sizeof(u32);
			spec.source_ordinal = leaf->ordinal;
			spec.obligation = ORLIX_TCTI_NATIVE_OBLIGATION_GPR;
			spec.result = (struct orlix_tcti_result) {
				.reason = ORLIX_TCTI_EXIT_SYSCALL,
				.status = 0,
				.fault_access = ORLIX_TCTI_ACCESS_FETCH,
				.pc = address + sizeof(u32),
				.instruction = POINTER_SUBTRACT_SVC,
			};
			orlix_tcti_native_gpr_capture(&spec.gpr, &expected);
			observation = orlix_tcti_native_observation_create(&spec);
			if (!observation) {
				KUNIT_FAIL(test, "%s observation allocation", leaf->name);
				KUNIT_EXPECT_EQ(test, 0,
					vm_munmap(address, PAGE_SIZE));
				goto detach;
			}
			ret = orlix_tcti_native_observation_execute(
				observation, current, &regs, current->mm);
			KUNIT_EXPECT_EQ(test, 0, ret);
			if (!ret)
				KUNIT_EXPECT_EQ(test, 0,
					orlix_tcti_native_observation_compare(observation));
			orlix_tcti_native_observation_destroy(observation);
			KUNIT_EXPECT_EQ(test, 0, vm_munmap(address, PAGE_SIZE));
			if (ret)
				goto detach;
		}
	}

detach:
	pointer_subtract_detach_mm(test);
}

static void pointer_subtract_rejection_exits_production_path(struct kunit *test)
{
	size_t index;

	if (!pointer_subtract_attach_mm(test))
		return;
	for (index = 0; index < ARRAY_SIZE(pointer_subtract_leaves); index++) {
		const struct pointer_subtract_leaf *leaf =
			&pointer_subtract_leaves[index];
		struct pt_regs regs = {};
		struct pt_regs before;
		struct orlix_tcti_result result;
		unsigned long address = 0;
		u32 instruction = pointer_subtract_instruction(leaf, 3, 5, 7) ^
			BIT(10);
		int ret = pointer_subtract_map_program(test, instruction, &address);

		if (ret)
			goto detach;
		regs.regs[3] = 0x1111222233334444ULL;
		regs.regs[5] = 0xab00000000000030ULL;
		regs.regs[7] = 0xcd00000000000010ULL;
		regs.sp = 0x20000;
		regs.pc = address;
		regs.pstate = PSR_MODE_EL0t | PSR_Z_BIT | PSR_V_BIT;
		regs.syscallno = NO_SYSCALL;
		before = regs;
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
				result.reason);
		KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, result.status);
		KUNIT_EXPECT_EQ(test, address, result.pc);
		KUNIT_EXPECT_EQ(test, instruction, result.instruction);
		KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(address, PAGE_SIZE));
	}

detach:
	pointer_subtract_detach_mm(test);
}

static struct kunit_case pointer_subtract_source_bound_cases[] = {
	KUNIT_CASE(pointer_subtract_source_decode_and_lowering),
	KUNIT_CASE(pointer_subtract_fixed_neighbours_reject),
	KUNIT_CASE(pointer_subtract_native_production_observations),
	KUNIT_CASE(pointer_subtract_rejection_exits_production_path),
	{}
};

static struct kunit_suite pointer_subtract_source_bound_suite = {
	.name = "orlix-tcti-pointer-subtract-source-bound",
	.init = pointer_subtract_suite_init,
	.exit = pointer_subtract_suite_exit,
	.test_cases = pointer_subtract_source_bound_cases,
};

kunit_test_suite(pointer_subtract_source_bound_suite);
