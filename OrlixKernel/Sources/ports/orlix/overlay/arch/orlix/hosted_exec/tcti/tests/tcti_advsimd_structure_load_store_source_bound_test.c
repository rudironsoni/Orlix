// SPDX-License-Identifier: GPL-2.0-only
/*
 * Pinned AARCHMRS 2026-06 direct leaves 2353 through 2504 are the complete
 * classic AdvSIMD structure load/store interval.  It contains LD1-4/ST1-4
 * multiple structures, lane structures, LD1R-4R replication, and immediate
 * and register post-index forms.  The authoritative checked C artifact is
 * consumed directly so no hand-maintained opcode subset can hide a leaf.
 */
#include <kunit/test.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/string.h>
#include <linux/syscalls.h>
#include <asm/hosted_exec.h>
#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/tcti.h>

#include "target_instruction_artifact.h"
#include "../decode_aarch64.h"

#define TCTI_ADVSIMD_STRUCTURE_FIRST_SOURCE 2353U
#define TCTI_ADVSIMD_STRUCTURE_LAST_SOURCE  2504U
#define TCTI_ADVSIMD_STRUCTURE_SOURCE_COUNT \
	(TCTI_ADVSIMD_STRUCTURE_LAST_SOURCE - \
	 TCTI_ADVSIMD_STRUCTURE_FIRST_SOURCE + 1U)
#define TCTI_ADVSIMD_STRUCTURE_SVC 0xd4000001U

struct tcti_advsimd_structure_context {
	u64 simd[ARRAY_SIZE(current->thread.user_simd)];
	unsigned long simd_valid;
};

static int tcti_advsimd_structure_test_init(struct kunit *test)
{
	struct tcti_advsimd_structure_context *context;

	context = kunit_kzalloc(test, sizeof(*context), GFP_KERNEL);
	if (!context)
		return -ENOMEM;
	memcpy(context->simd, current->thread.user_simd, sizeof(context->simd));
	context->simd_valid = current->thread.user_simd_valid;
	test->priv = context;
	return 0;
}

static void tcti_advsimd_structure_test_exit(struct kunit *test)
{
	struct tcti_advsimd_structure_context *context = test->priv;

	memcpy(current->thread.user_simd, context->simd, sizeof(context->simd));
	current->thread.user_simd_valid = context->simd_valid;
}

static const char *tcti_advsimd_structure_artifact_string(
	const struct tcti_target_instruction_artifact *artifact, u32 offset)
{
	if (offset >= artifact->string_pool_size)
		return NULL;
	return (const char *)artifact->string_pool + offset;
}

static enum tcti_decode_class tcti_advsimd_structure_expected_class(
	const char *operation)
{
	if (strstr(operation, "_advsimd_mult"))
		return TCTI_DECODE_SIMD_LOAD_STORE_MULTIPLE_STRUCTURE;
	if (strstr(operation, "R_advsimd"))
		return TCTI_DECODE_SIMD_LOAD_REPLICATE;
	if (strstr(operation, "_advsimd_sngl"))
		return TCTI_DECODE_SIMD_LOAD_STORE_SINGLE_STRUCTURE;
	return TCTI_DECODE_UNSUPPORTED;
}

/*
 * Every execution claim below enters through the same EL0 resume path as an
 * ordinary hosted guest.  The terminating SVC makes the structured result
 * distinguish successful execution from a decoder-only observation.
 */
static unsigned long tcti_advsimd_structure_map_instruction(struct kunit *test,
							      u32 instruction)
{
	const u32 program[] = { instruction, TCTI_ADVSIMD_STRUCTURE_SVC };
	unsigned long mapped;
	int ret;

	mapped = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));
	ret = tcti_write_user_data(current->mm, mapped, program, sizeof(program));
	KUNIT_ASSERT_EQ(test, 0, ret);
	ret = sys_mprotect(mapped, PAGE_SIZE, PROT_READ | PROT_EXEC);
	KUNIT_ASSERT_EQ(test, 0, ret);
	return mapped;
}

static struct tcti_result tcti_advsimd_structure_resume(
	struct kunit *test, u32 instruction, struct pt_regs *regs,
	unsigned long *mapped)
{
	struct tcti_result result;

	*mapped = tcti_advsimd_structure_map_instruction(test, instruction);
	regs->pc = *mapped;
	regs->pstate = PSR_MODE_EL0t;
	regs->syscallno = NO_SYSCALL;
	result = tcti_resume_user(current, regs, current->mm);
	KUNIT_EXPECT_EQ(test, TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, 0L, result.status);
	KUNIT_EXPECT_EQ(test, TCTI_ADVSIMD_STRUCTURE_SVC, result.instruction);
	KUNIT_EXPECT_EQ(test, *mapped + 2 * sizeof(u32), result.pc);
	KUNIT_EXPECT_EQ(test, *mapped + 2 * sizeof(u32), regs->pc);
	return result;
}

static void tcti_advsimd_structure_every_source_leaf_reaches_decoder(
	struct kunit *test)
{
	const struct tcti_target_instruction_artifact *artifact =
		tcti_target_instruction_artifact_canonical();
	u32 ordinal;
	u32 observed = 0;

	KUNIT_ASSERT_NOT_NULL(test, artifact);
	KUNIT_ASSERT_GT(test, artifact->leaf_count,
			TCTI_ADVSIMD_STRUCTURE_LAST_SOURCE);

	for (ordinal = TCTI_ADVSIMD_STRUCTURE_FIRST_SOURCE;
	     ordinal <= TCTI_ADVSIMD_STRUCTURE_LAST_SOURCE; ordinal++) {
		const struct tcti_target_instruction_artifact_leaf *leaf =
			&artifact->leaves[ordinal];
		const char *name = tcti_advsimd_structure_artifact_string(
			artifact, leaf->name_offset);
		const char *operation = tcti_advsimd_structure_artifact_string(
			artifact, leaf->operation_offset);
		enum tcti_decode_class expected;
		struct tcti_decoded_instruction decoded;

		KUNIT_ASSERT_NOT_NULL_MSG(test, name, "ordinal=%u", ordinal);
		KUNIT_ASSERT_NOT_NULL_MSG(test, operation, "ordinal=%u", ordinal);
		expected = tcti_advsimd_structure_expected_class(operation);
		KUNIT_ASSERT_NE_MSG(test, TCTI_DECODE_UNSUPPORTED, expected,
			"ordinal=%u leaf=%s operation=%s", ordinal, name, operation);

		decoded = tcti_decode_aarch64(leaf->encoding_pattern);
		KUNIT_EXPECT_EQ_MSG(test, expected, decoded.decode_class,
			"ordinal=%u leaf=%s operation=%s instruction=%08x",
			ordinal, name, operation, leaf->encoding_pattern);
		KUNIT_EXPECT_EQ_MSG(test, leaf->encoding_pattern,
			decoded.instruction, "ordinal=%u leaf=%s", ordinal, name);
		KUNIT_EXPECT_TRUE_MSG(test,
			(decoded.instruction & leaf->encoding_mask) ==
			leaf->encoding_pattern,
			"ordinal=%u leaf=%s", ordinal, name);
		observed++;
	}

	KUNIT_EXPECT_EQ(test, TCTI_ADVSIMD_STRUCTURE_SOURCE_COUNT, observed);
}

static void tcti_advsimd_structure_single_lane_writeback_and_pc(
	struct kunit *test)
{
	const u8 source[] = { 0x5a, 0x6b, 0x7c, 0x8d };
	struct pt_regs regs = {};
	struct tcti_decoded_instruction decoded;
	unsigned long address;
	unsigned long mapped;
	int ret;

	KUNIT_ASSERT_NOT_NULL(test, current->mm);
	address = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(address));
	KUNIT_ASSERT_EQ(test, 0,
		tcti_write_user_data(current->mm, address, source, sizeof(source)));

	/* LD4 {v31.b-v2.b}[8], [x8], #4. */
	decoded = tcti_decode_aarch64(0x4d60211fU);
	KUNIT_ASSERT_EQ(test, TCTI_DECODE_SIMD_LOAD_STORE_SINGLE_STRUCTURE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 4, decoded.simd_structure_count);
	KUNIT_EXPECT_EQ(test, TCTI_MEMORY_INDEX_POST,
			decoded.memory_index_mode);
	current->thread.user_simd[31 * 2 + 1] = 0;
	current->thread.user_simd[0] = 0;
	current->thread.user_simd[1] = 0;
	current->thread.user_simd[2] = 0;
	regs.regs[8] = address;

	tcti_advsimd_structure_resume(test, 0x4d60211fU, &regs, &mapped);
	KUNIT_EXPECT_EQ(test, address + sizeof(source), regs.regs[8]);
	KUNIT_EXPECT_EQ(test, (u64)source[0],
			current->thread.user_simd[31 * 2 + 1] & 0xffULL);
	KUNIT_EXPECT_EQ(test, (u64)source[1],
			current->thread.user_simd[0] & 0xffULL);
	KUNIT_EXPECT_EQ(test, (u64)source[2],
			current->thread.user_simd[1] & 0xffULL);
	KUNIT_EXPECT_EQ(test, (u64)source[3],
			current->thread.user_simd[2] & 0xffULL);

	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(address, PAGE_SIZE));
}

static void tcti_advsimd_structure_replicate_and_pc(struct kunit *test)
{
	const u8 source[] = { 0x19, 0x2a, 0x3b, 0x4c };
	struct pt_regs regs = {};
	struct tcti_decoded_instruction decoded;
	unsigned long address;
	unsigned long mapped;
	int ret;

	KUNIT_ASSERT_NOT_NULL(test, current->mm);
	address = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(address));
	KUNIT_ASSERT_EQ(test, 0,
		tcti_write_user_data(current->mm, address, source, sizeof(source)));

	/* LD4R {v30.16b-v1.16b}, [x8]. */
	decoded = tcti_decode_aarch64(0x4d60e11eU);
	KUNIT_ASSERT_EQ(test, TCTI_DECODE_SIMD_LOAD_REPLICATE,
			decoded.decode_class);
	KUNIT_EXPECT_EQ(test, 4, decoded.simd_structure_count);
	regs.regs[8] = address;

	tcti_advsimd_structure_resume(test, 0x4d60e11eU, &regs, &mapped);
	KUNIT_EXPECT_EQ(test, address, regs.regs[8]);
	KUNIT_EXPECT_EQ(test, 0x1919191919191919ULL,
			current->thread.user_simd[30 * 2]);
	KUNIT_EXPECT_EQ(test, 0x2a2a2a2a2a2a2a2aULL,
			current->thread.user_simd[31 * 2]);
	KUNIT_EXPECT_EQ(test, 0x3b3b3b3b3b3b3b3bULL,
			current->thread.user_simd[0]);
	KUNIT_EXPECT_EQ(test, 0x4c4c4c4c4c4c4c4cULL,
			current->thread.user_simd[1]);

	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(address, PAGE_SIZE));
}

static void tcti_advsimd_structure_fault_preserves_pc_and_writeback(
	struct kunit *test)
{
	struct pt_regs regs = {};
	struct tcti_decoded_instruction decoded;
	struct tcti_result result;
	unsigned long address;
	unsigned long mapped;
	u64 source = 0x1716151413121110ULL;
	int ret;

	KUNIT_ASSERT_NOT_NULL(test, current->mm);
	address = ksys_mmap_pgoff(0, 2 * PAGE_SIZE, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(address));
	KUNIT_ASSERT_EQ(test, 0,
		tcti_write_user_data(current->mm,
			address + PAGE_SIZE - sizeof(source), &source, sizeof(source)));
	KUNIT_ASSERT_EQ(test, 0,
		sys_mprotect(address + PAGE_SIZE, PAGE_SIZE, PROT_NONE));

	/* LD2 {v3.8h, v4.8h}, [x10].  The second transfer faults. */
	decoded = tcti_decode_aarch64(0x0c408543U);
	KUNIT_ASSERT_EQ(test, TCTI_DECODE_SIMD_LOAD_STORE_MULTIPLE_STRUCTURE,
			decoded.decode_class);
	regs.regs[10] = address + PAGE_SIZE - sizeof(source);

	mapped = tcti_advsimd_structure_map_instruction(test, 0x0c408543U);
	regs.pc = mapped;
	regs.pstate = PSR_MODE_EL0t;
	regs.syscallno = NO_SYSCALL;
	result = tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, TCTI_EXIT_USER_FAULT, result.reason);
	KUNIT_EXPECT_EQ(test, -EFAULT, result.status);
	KUNIT_EXPECT_EQ(test, address + PAGE_SIZE, result.fault_address);
	KUNIT_EXPECT_EQ(test, TCTI_ACCESS_READ, result.fault_access);
	KUNIT_EXPECT_EQ(test, mapped, result.pc);
	KUNIT_EXPECT_EQ(test, address + PAGE_SIZE - sizeof(source), regs.regs[10]);
	KUNIT_EXPECT_EQ(test, mapped, regs.pc);

	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(address, 2 * PAGE_SIZE));
}

static void tcti_advsimd_structure_reserved_encodings_reject(
	struct kunit *test)
{
	const u32 reserved[] = { 0x0d008800U, 0x0c001000U };
	size_t index;

	/* Single-structure 32-bit lane form with size=2 is reserved. */
	KUNIT_EXPECT_EQ(test, TCTI_DECODE_UNSUPPORTED,
		tcti_decode_aarch64(reserved[0]).decode_class);
	/* The multiple-structure opcode holes are not decoded as structures. */
	KUNIT_EXPECT_EQ(test, TCTI_DECODE_UNSUPPORTED,
		tcti_decode_aarch64(reserved[1]).decode_class);

	for (index = 0; index < ARRAY_SIZE(reserved); index++) {
		struct pt_regs regs = {};
		struct tcti_result result;
		unsigned long mapped;

		mapped = tcti_advsimd_structure_map_instruction(test,
								reserved[index]);
		regs.pc = mapped;
		regs.pstate = PSR_MODE_EL0t;
		regs.syscallno = NO_SYSCALL;
		result = tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ(test, TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
				result.reason);
		KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, result.status);
		KUNIT_EXPECT_EQ(test, reserved[index], result.instruction);
		KUNIT_EXPECT_EQ(test, mapped, result.pc);
		KUNIT_EXPECT_EQ(test, mapped, regs.pc);
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
	}
}

static struct kunit_case tcti_advsimd_structure_cases[] = {
	KUNIT_CASE(tcti_advsimd_structure_every_source_leaf_reaches_decoder),
	KUNIT_CASE(tcti_advsimd_structure_single_lane_writeback_and_pc),
	KUNIT_CASE(tcti_advsimd_structure_replicate_and_pc),
	KUNIT_CASE(tcti_advsimd_structure_fault_preserves_pc_and_writeback),
	KUNIT_CASE(tcti_advsimd_structure_reserved_encodings_reject),
	{}
};

static struct kunit_suite tcti_advsimd_structure_suite = {
	.name = "orlix-tcti-advsimd-structure-source-bound",
	.init = tcti_advsimd_structure_test_init,
	.exit = tcti_advsimd_structure_test_exit,
	.test_cases = tcti_advsimd_structure_cases,
};

kunit_test_suite(tcti_advsimd_structure_suite);
