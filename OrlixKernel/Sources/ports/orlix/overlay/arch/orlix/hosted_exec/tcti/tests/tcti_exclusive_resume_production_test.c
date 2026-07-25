// SPDX-License-Identifier: GPL-2.0-only
#include <asm/ptrace.h>
#include <asm/tcti.h>
#include <kunit/test.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched/mm.h>
#include <linux/string.h>
#include <linux/syscalls.h>

#include "../decode_aarch64.h"
#include "target_instruction_artifact.h"

/*
 * AARCHMRS 2026-06 base exclusive source leaves 2583 through 2590 and
 * 2599 through 2614.  Every leaf executes from mapped RX guest text via
 * tcti_resume_user().  Store-exclusive leaves are preceded by the matching
 * exclusive load, which establishes the architectural reservation through
 * the same production path.
 */
#define TCTI_EXCLUSIVE_RESUME_SVC 0xd4000001U
#define TCTI_EXCLUSIVE_RN 10U
#define TCTI_EXCLUSIVE_RS 6U
#define TCTI_EXCLUSIVE_RT 8U
#define TCTI_EXCLUSIVE_RT2 9U

struct tcti_exclusive_resume_case {
	const char *name;
	const char *source_name;
	const char *operation;
	u32 source_ordinal;
	u32 encoding;
	u32 setup_encoding;
	bool load;
	bool pair;
	bool acquire;
	bool release;
};

static const struct tcti_exclusive_resume_case tcti_exclusive_resume_cases[] = {
	{"STXP_32", "STXP_SP32_ldstexclp", "STXP", 2583U, 0x88200000U,
	 0x887f0000U, false, true, false, false},
	{"STLXP_32", "STLXP_SP32_ldstexclp", "STLXP", 2584U, 0x88208000U,
	 0x887f0000U, false, true, false, true},
	{"LDXP_32", "LDXP_LP32_ldstexclp", "LDXP", 2585U, 0x887f0000U, 0, true,
	 true, false, false},
	{"LDAXP_32", "LDAXP_LP32_ldstexclp", "LDAXP", 2586U, 0x887f8000U, 0,
	 true, true, true, false},
	{"STXP_64", "STXP_SP64_ldstexclp", "STXP", 2587U, 0xc8200000U,
	 0xc87f0000U, false, true, false, false},
	{"STLXP_64", "STLXP_SP64_ldstexclp", "STLXP", 2588U, 0xc8208000U,
	 0xc87f0000U, false, true, false, true},
	{"LDXP_64", "LDXP_LP64_ldstexclp", "LDXP", 2589U, 0xc87f0000U, 0, true,
	 true, false, false},
	{"LDAXP_64", "LDAXP_LP64_ldstexclp", "LDAXP", 2590U, 0xc87f8000U, 0,
	 true, true, true, false},
	{"STXRB", "STXRB_SR32_ldstexclr", "STXRB", 2599U, 0x08007c00U,
	 0x085f7c00U, false, false, false, false},
	{"STLXRB", "STLXRB_SR32_ldstexclr", "STLXRB", 2600U, 0x0800fc00U,
	 0x085f7c00U, false, false, false, true},
	{"LDXRB", "LDXRB_LR32_ldstexclr", "LDXRB", 2601U, 0x085f7c00U, 0, true,
	 false, false, false},
	{"LDAXRB", "LDAXRB_LR32_ldstexclr", "LDAXRB", 2602U, 0x085ffc00U, 0,
	 true, false, true, false},
	{"STXRH", "STXRH_SR32_ldstexclr", "STXRH", 2603U, 0x48007c00U,
	 0x485f7c00U, false, false, false, false},
	{"STLXRH", "STLXRH_SR32_ldstexclr", "STLXRH", 2604U, 0x4800fc00U,
	 0x485f7c00U, false, false, false, true},
	{"LDXRH", "LDXRH_LR32_ldstexclr", "LDXRH", 2605U, 0x485f7c00U, 0, true,
	 false, false, false},
	{"LDAXRH", "LDAXRH_LR32_ldstexclr", "LDAXRH", 2606U, 0x485ffc00U, 0,
	 true, false, true, false},
	{"STXR_32", "STXR_SR32_ldstexclr", "STXR", 2607U, 0x88007c00U,
	 0x885f7c00U, false, false, false, false},
	{"STLXR_32", "STLXR_SR32_ldstexclr", "STLXR", 2608U, 0x8800fc00U,
	 0x885f7c00U, false, false, false, true},
	{"LDXR_32", "LDXR_LR32_ldstexclr", "LDXR", 2609U, 0x885f7c00U, 0, true,
	 false, false, false},
	{"LDAXR_32", "LDAXR_LR32_ldstexclr", "LDAXR", 2610U, 0x885ffc00U, 0,
	 true, false, true, false},
	{"STXR_64", "STXR_SR64_ldstexclr", "STXR", 2611U, 0xc8007c00U,
	 0xc85f7c00U, false, false, false, false},
	{"STLXR_64", "STLXR_SR64_ldstexclr", "STLXR", 2612U, 0xc800fc00U,
	 0xc85f7c00U, false, false, false, true},
	{"LDXR_64", "LDXR_LR64_ldstexclr", "LDXR", 2613U, 0xc85f7c00U, 0, true,
	 false, false, false},
	{"LDAXR_64", "LDAXR_LR64_ldstexclr", "LDAXR", 2614U, 0xc85ffc00U, 0,
	 true, false, true, false},
};

static const char *tcti_exclusive_resume_artifact_string(
	const struct tcti_target_instruction_artifact *artifact, u32 offset)
{
	if (!artifact || offset >= artifact->string_pool_size)
		return NULL;
	return (const char *)artifact->string_pool + offset;
}

static void tcti_exclusive_resume_assert_source(
	struct kunit *test, const struct tcti_exclusive_resume_case *entry,
	u32 instruction, const struct tcti_decoded_instruction *decoded)
{
	const struct tcti_target_instruction_artifact *artifact =
		tcti_target_instruction_artifact_canonical();
	const struct tcti_target_instruction_artifact_leaf *leaf;
	const char *source_name;
	const char *operation;

	KUNIT_ASSERT_NOT_NULL(test, artifact);
	KUNIT_ASSERT_GT(test, artifact->leaf_count, entry->source_ordinal);
	leaf = &artifact->leaves[entry->source_ordinal];
	source_name = tcti_exclusive_resume_artifact_string(artifact,
							    leaf->name_offset);
	operation = tcti_exclusive_resume_artifact_string(
		artifact, leaf->operation_offset);
	KUNIT_ASSERT_NOT_NULL_MSG(test, source_name, "source=%u",
				  entry->source_ordinal);
	KUNIT_ASSERT_NOT_NULL_MSG(test, operation, "source=%u",
				  entry->source_ordinal);
	KUNIT_EXPECT_STREQ_MSG(test, entry->source_name, source_name,
			       "source=%u", entry->source_ordinal);
	KUNIT_EXPECT_STREQ_MSG(test, entry->operation, operation, "source=%u",
			       entry->source_ordinal);
	KUNIT_EXPECT_EQ_MSG(test, entry->encoding, leaf->encoding_pattern,
			    "%s source=%u", entry->name, entry->source_ordinal);
	KUNIT_EXPECT_EQ_MSG(test, leaf->encoding_pattern,
			    instruction & leaf->encoding_mask, "%s source=%u",
			    entry->name, entry->source_ordinal);
	KUNIT_EXPECT_TRUE_MSG(test, decoded->exclusive, "%s source=%u",
			      entry->name, entry->source_ordinal);
	KUNIT_EXPECT_EQ_MSG(test, entry->acquire, decoded->acquire,
			    "%s source=%u", entry->name, entry->source_ordinal);
	KUNIT_EXPECT_EQ_MSG(test, entry->release, decoded->release,
			    "%s source=%u", entry->name, entry->source_ordinal);
}

static int tcti_exclusive_resume_production_test_init(struct kunit *test)
{
	struct mm_struct *mm = mm_alloc();

	if (!mm)
		return -ENOMEM;
	kthread_use_mm(mm);
	test->priv = mm;
	return 0;
}

static void tcti_exclusive_resume_production_test_exit(struct kunit *test)
{
	struct mm_struct *mm = test->priv;

	if (!mm)
		return;
	kthread_unuse_mm(mm);
	mmput(mm);
}

static unsigned long tcti_exclusive_resume_map(struct kunit *test, int prot)
{
	unsigned long address;

	address = ksys_mmap_pgoff(0, PAGE_SIZE, prot,
				  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_EXPECT_FALSE(test, IS_ERR_VALUE(address));
	return address;
}

static u32 tcti_exclusive_resume_instruction(u32 encoding, bool pair, bool load,
					     u8 rt)
{
	u32 fields = (TCTI_EXCLUSIVE_RN << 5) | rt;

	if (load)
		fields |= 31U << 16;
	else
		fields |= TCTI_EXCLUSIVE_RS << 16;
	if (pair)
		fields |= (rt + 1) << 10;
	else
		fields |= 31U << 10;
	return (encoding &
		~((0x1fU << 16) | (0x1fU << 10) | (0x1fU << 5) | 0x1fU)) |
	       fields;
}

static int tcti_exclusive_resume_write_program(unsigned long address,
					       const u32 *program, size_t words)
{
	int ret;

	ret = sys_mprotect(address, PAGE_SIZE, PROT_READ | PROT_WRITE);
	if (ret)
		return ret;
	ret = tcti_write_user_data(current->mm, address, program,
				   words * sizeof(*program));
	if (ret)
		return ret;
	return sys_mprotect(address, PAGE_SIZE, PROT_READ | PROT_EXEC);
}

static u64 tcti_exclusive_resume_mask(u8 size)
{
	return size == sizeof(u64) ? U64_MAX : (1ULL << (size * 8)) - 1;
}

static void tcti_exclusive_resume_init_regs(struct pt_regs *regs,
					    unsigned long instructions,
					    unsigned long data)
{
	*regs = (struct pt_regs){};
	regs->regs[TCTI_EXCLUSIVE_RN] = data;
	regs->pc = instructions;
	regs->pstate =
		PSR_MODE_EL0t | PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT | PSR_V_BIT;
	regs->syscallno = NO_SYSCALL;
}

static void tcti_exclusive_resume_executes_every_base_leaf(struct kunit *test)
{
	const u64 old[] = {0x8123456789abcdefULL, 0x1020304050607080ULL};
	const u64 desired[] = {0x0fedcba987654321ULL, 0x8877665544332211ULL};
	const u64 expected_pstate =
		PSR_MODE_EL0t | PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT | PSR_V_BIT;
	unsigned long instructions =
		tcti_exclusive_resume_map(test, PROT_READ | PROT_WRITE);
	unsigned long data =
		tcti_exclusive_resume_map(test, PROT_READ | PROT_WRITE);
	size_t index;

	if (IS_ERR_VALUE(instructions) || IS_ERR_VALUE(data))
		return;
	for (index = 0; index < ARRAY_SIZE(tcti_exclusive_resume_cases);
	     index++) {
		const struct tcti_exclusive_resume_case *entry =
			&tcti_exclusive_resume_cases[index];
		u32 program[3];
		struct tcti_decoded_instruction decoded;
		struct pt_regs regs;
		struct tcti_result result;
		u64 observed[2] = {};
		u64 mask;
		size_t words = 0;
		size_t bytes;
		int ret;

		program[words++] = tcti_exclusive_resume_instruction(
			entry->load ? entry->encoding : entry->setup_encoding,
			entry->pair, true, entry->load ? TCTI_EXCLUSIVE_RT : 0);
		if (!entry->load)
			program[words++] = tcti_exclusive_resume_instruction(
				entry->encoding, entry->pair, false,
				TCTI_EXCLUSIVE_RT);
		program[words++] = TCTI_EXCLUSIVE_RESUME_SVC;
		decoded = tcti_decode_aarch64(program[entry->load ? 0 : 1]);
		KUNIT_ASSERT_EQ_MSG(test, TCTI_DECODE_LOAD_STORE_EXCLUSIVE,
				    decoded.decode_class, "%s source=%u",
				    entry->name, entry->source_ordinal);
		KUNIT_ASSERT_EQ_MSG(test, entry->load, decoded.load,
				    "%s source=%u", entry->name,
				    entry->source_ordinal);
		KUNIT_ASSERT_EQ_MSG(test, entry->pair, decoded.pair,
				    "%s source=%u", entry->name,
				    entry->source_ordinal);
		tcti_exclusive_resume_assert_source(
			test, entry, program[entry->load ? 0 : 1], &decoded);
		bytes = decoded.access_size * (decoded.pair ? 2 : 1);
		mask = tcti_exclusive_resume_mask(decoded.access_size);
		ret = tcti_exclusive_resume_write_program(instructions, program,
							  words);
		KUNIT_ASSERT_EQ_MSG(test, 0, ret, "%s source=%u", entry->name,
				    entry->source_ordinal);
		ret = tcti_write_user_data(current->mm, data, old, bytes);
		KUNIT_ASSERT_EQ_MSG(test, 0, ret, "%s source=%u", entry->name,
				    entry->source_ordinal);
		tcti_exclusive_resume_init_regs(&regs, instructions, data);
		regs.regs[TCTI_EXCLUSIVE_RT] = desired[0] & mask;
		regs.regs[TCTI_EXCLUSIVE_RT2] = desired[1] & mask;
		result = tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ_MSG(test, TCTI_EXIT_SYSCALL, result.reason,
				    "%s source=%u", entry->name,
				    entry->source_ordinal);
		KUNIT_EXPECT_EQ_MSG(test, 0L, result.status, "%s source=%u",
				    entry->name, entry->source_ordinal);
		KUNIT_EXPECT_EQ_MSG(test,
				    instructions + (words - 1) * sizeof(u32),
				    result.pc, "%s source=%u", entry->name,
				    entry->source_ordinal);
		KUNIT_EXPECT_EQ_MSG(test, TCTI_EXCLUSIVE_RESUME_SVC,
				    result.instruction, "%s source=%u",
				    entry->name, entry->source_ordinal);
		KUNIT_EXPECT_EQ_MSG(test, expected_pstate, regs.pstate,
				    "%s source=%u", entry->name,
				    entry->source_ordinal);
		KUNIT_EXPECT_EQ_MSG(test, entry->load ? 1 : 0,
				    current->thread.user_exclusive_valid,
				    "%s source=%u", entry->name,
				    entry->source_ordinal);
		ret = tcti_read_user_data(current->mm, data, observed, bytes);
		KUNIT_ASSERT_EQ_MSG(test, 0, ret, "%s source=%u", entry->name,
				    entry->source_ordinal);
		if (entry->load) {
			KUNIT_EXPECT_EQ(test, old[0] & mask,
					regs.regs[TCTI_EXCLUSIVE_RT]);
			if (entry->pair)
				KUNIT_EXPECT_EQ(test, old[1] & mask,
						regs.regs[TCTI_EXCLUSIVE_RT2]);
			KUNIT_EXPECT_EQ(test, old[0] & mask, observed[0]);
			if (entry->pair)
				KUNIT_EXPECT_EQ(test, old[1] & mask,
						observed[1]);
		} else {
			KUNIT_EXPECT_EQ(test, 0ULL,
					regs.regs[TCTI_EXCLUSIVE_RS]);
			KUNIT_EXPECT_EQ(test, desired[0] & mask, observed[0]);
			if (entry->pair)
				KUNIT_EXPECT_EQ(test, desired[1] & mask,
						observed[1]);
		}
		tcti_prepare_signal_delivery();
	}
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(data, PAGE_SIZE));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(instructions, PAGE_SIZE));
}

static void
tcti_exclusive_resume_store_faults_preserve_state(struct kunit *test)
{
	const u64 old[] = {0x8123456789abcdefULL, 0x1020304050607080ULL};
	const u64 desired[] = {0x0fedcba987654321ULL, 0x8877665544332211ULL};
	unsigned long instructions =
		tcti_exclusive_resume_map(test, PROT_READ | PROT_WRITE);
	unsigned long data =
		tcti_exclusive_resume_map(test, PROT_READ | PROT_WRITE);
	bool seen_scalar[sizeof(u64) + 1] = {};
	bool seen_pair[sizeof(u64) + 1] = {};
	size_t index;

	if (IS_ERR_VALUE(instructions) || IS_ERR_VALUE(data))
		return;
	for (index = 0; index < ARRAY_SIZE(tcti_exclusive_resume_cases);
	     index++) {
		const struct tcti_exclusive_resume_case *entry =
			&tcti_exclusive_resume_cases[index];
		u32 setup_program[2];
		u32 store_program[2];
		struct tcti_decoded_instruction decoded;
		struct pt_regs regs;
		struct pt_regs before;
		struct tcti_result result;
		u64 observed[2] = {};
		u64 mask;
		size_t bytes;
		int ret;

		if (entry->load)
			continue;
		decoded = tcti_decode_aarch64(tcti_exclusive_resume_instruction(
			entry->encoding, entry->pair, false,
			TCTI_EXCLUSIVE_RT));
		KUNIT_ASSERT_EQ_MSG(test, TCTI_DECODE_LOAD_STORE_EXCLUSIVE,
				    decoded.decode_class, "%s source=%u",
				    entry->name, entry->source_ordinal);
		tcti_exclusive_resume_assert_source(
			test, entry,
			tcti_exclusive_resume_instruction(entry->encoding,
							  entry->pair, false,
							  TCTI_EXCLUSIVE_RT),
			&decoded);
		if (entry->pair ? seen_pair[decoded.access_size]
				: seen_scalar[decoded.access_size])
			continue;
		if (entry->pair)
			seen_pair[decoded.access_size] = true;
		else
			seen_scalar[decoded.access_size] = true;
		bytes = decoded.access_size * (decoded.pair ? 2 : 1);
		mask = tcti_exclusive_resume_mask(decoded.access_size);
		setup_program[0] = tcti_exclusive_resume_instruction(
			entry->setup_encoding, entry->pair, true, 0);
		setup_program[1] = TCTI_EXCLUSIVE_RESUME_SVC;
		ret = tcti_exclusive_resume_write_program(
			instructions, setup_program, ARRAY_SIZE(setup_program));
		KUNIT_ASSERT_EQ_MSG(test, 0, ret, "%s source=%u", entry->name,
				    entry->source_ordinal);
		ret = tcti_write_user_data(current->mm, data, old, bytes);
		KUNIT_ASSERT_EQ_MSG(test, 0, ret, "%s source=%u", entry->name,
				    entry->source_ordinal);
		tcti_exclusive_resume_init_regs(&regs, instructions, data);
		result = tcti_resume_user(current, &regs, current->mm);
		KUNIT_ASSERT_EQ_MSG(test, TCTI_EXIT_SYSCALL, result.reason,
				    "%s source=%u", entry->name,
				    entry->source_ordinal);
		KUNIT_ASSERT_EQ_MSG(
			test, 1, current->thread.user_exclusive_valid,
			"%s source=%u", entry->name, entry->source_ordinal);

		store_program[0] = tcti_exclusive_resume_instruction(
			entry->encoding, entry->pair, false, TCTI_EXCLUSIVE_RT);
		store_program[1] = TCTI_EXCLUSIVE_RESUME_SVC;
		ret = tcti_exclusive_resume_write_program(
			instructions, store_program, ARRAY_SIZE(store_program));
		KUNIT_ASSERT_EQ_MSG(test, 0, ret, "%s source=%u", entry->name,
				    entry->source_ordinal);
		regs.pc = instructions;
		regs.regs[TCTI_EXCLUSIVE_RT] = desired[0] & mask;
		regs.regs[TCTI_EXCLUSIVE_RT2] = desired[1] & mask;
		before = regs;
		KUNIT_ASSERT_EQ_MSG(
			test, 0, sys_mprotect(data, PAGE_SIZE, PROT_READ),
			"%s source=%u", entry->name, entry->source_ordinal);
		result = tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ_MSG(test, TCTI_EXIT_USER_FAULT, result.reason,
				    "%s source=%u", entry->name,
				    entry->source_ordinal);
		KUNIT_EXPECT_EQ_MSG(test, -EACCES, result.status,
				    "%s source=%u", entry->name,
				    entry->source_ordinal);
		KUNIT_EXPECT_EQ_MSG(test, data, result.fault_address,
				    "%s source=%u", entry->name,
				    entry->source_ordinal);
		KUNIT_EXPECT_EQ_MSG(test, TCTI_ACCESS_WRITE,
				    result.fault_access, "%s source=%u",
				    entry->name, entry->source_ordinal);
		KUNIT_EXPECT_EQ_MSG(test, instructions, result.pc,
				    "%s source=%u", entry->name,
				    entry->source_ordinal);
		KUNIT_EXPECT_EQ_MSG(test, store_program[0], result.instruction,
				    "%s source=%u", entry->name,
				    entry->source_ordinal);
		KUNIT_EXPECT_MEMEQ_MSG(test, &before, &regs, sizeof(regs),
				       "%s source=%u", entry->name,
				       entry->source_ordinal);
		KUNIT_EXPECT_EQ_MSG(
			test, 0, current->thread.user_exclusive_valid,
			"%s source=%u", entry->name, entry->source_ordinal);
		KUNIT_ASSERT_EQ_MSG(
			test, 0,
			sys_mprotect(data, PAGE_SIZE, PROT_READ | PROT_WRITE),
			"%s source=%u", entry->name, entry->source_ordinal);
		ret = tcti_read_user_data(current->mm, data, observed, bytes);
		KUNIT_ASSERT_EQ_MSG(test, 0, ret, "%s source=%u", entry->name,
				    entry->source_ordinal);
		KUNIT_EXPECT_EQ(test, old[0] & mask, observed[0]);
		if (entry->pair)
			KUNIT_EXPECT_EQ(test, old[1] & mask, observed[1]);
	}
	KUNIT_EXPECT_TRUE(test, seen_scalar[sizeof(u8)]);
	KUNIT_EXPECT_TRUE(test, seen_scalar[sizeof(u16)]);
	KUNIT_EXPECT_TRUE(test, seen_scalar[sizeof(u32)]);
	KUNIT_EXPECT_TRUE(test, seen_scalar[sizeof(u64)]);
	KUNIT_EXPECT_TRUE(test, seen_pair[sizeof(u32)]);
	KUNIT_EXPECT_TRUE(test, seen_pair[sizeof(u64)]);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(data, PAGE_SIZE));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(instructions, PAGE_SIZE));
}

static struct kunit_case tcti_exclusive_resume_production_test_cases[] = {
	KUNIT_CASE(tcti_exclusive_resume_executes_every_base_leaf),
	KUNIT_CASE(tcti_exclusive_resume_store_faults_preserve_state),
	{}};

static struct kunit_suite tcti_exclusive_resume_production_test_suite = {
	.name = "orlix-tcti-exclusive-resume-production",
	.init = tcti_exclusive_resume_production_test_init,
	.exit = tcti_exclusive_resume_production_test_exit,
	.test_cases = tcti_exclusive_resume_production_test_cases,
};
kunit_test_suite(tcti_exclusive_resume_production_test_suite);

MODULE_LICENSE("GPL");
