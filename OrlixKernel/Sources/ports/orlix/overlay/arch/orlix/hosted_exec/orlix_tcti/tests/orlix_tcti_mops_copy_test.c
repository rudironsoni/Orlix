// SPDX-License-Identifier: GPL-2.0-only
/* Production-path proof for issue #173's 96 CPY leaves. */
#include <asm/orlix_tcti.h>
#include <asm/ptrace.h>
#include <kunit/test.h>
#include <linux/err.h>
#include <linux/sched/mm.h>
#include <linux/bitops.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/syscalls.h>

#include "../decode_aarch64.h"
#include "../semantics.h"

#define MOPS_SVC 0xd4000001U

struct mops_leaf {
	u32 ordinal;
	u32 mask;
	u32 pattern;
};

#define ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE(...)
#define ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW(ordinal, id, mnemonic, operation, mask, \
					     pattern, feature_predicate, offset, length) \
	{ ordinal, mask, pattern },
static const struct mops_leaf mops_leaves[] = {
#include "../isa/source_manifest.def"
};
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE

static const struct mops_leaf *mops_leaf(u32 ordinal)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(mops_leaves); index++)
		if (mops_leaves[index].ordinal == ordinal)
			return &mops_leaves[index];
	return NULL;
}

static u32 mops_instruction(struct kunit *test, u32 ordinal)
{
	const struct mops_leaf *leaf = mops_leaf(ordinal);

	if (!leaf) {
		KUNIT_FAIL(test, "missing source ordinal %u", ordinal);
		return 0;
	}
	/* X0 is destination, X1 source, X2 remaining count. */
	return leaf->pattern | (1U << 16) | (2U << 5);
}

static u32 mops_instruction_with_registers(struct kunit *test, u32 ordinal,
					   u8 rd, u8 rm, u8 rn)
{
	const struct mops_leaf *leaf = mops_leaf(ordinal);

	if (!leaf) {
		KUNIT_FAIL(test, "missing source ordinal %u", ordinal);
		return 0;
	}
	return leaf->pattern | rd | ((u32)rn << 5) | ((u32)rm << 16);
}

static unsigned long mops_map(struct kunit *test, const void *data, size_t size)
{
	unsigned long mapped;
	int ret;

	mapped = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));
	if (data) {
		ret = orlix_tcti_write_user_data(current->mm, mapped, data, size);
		KUNIT_ASSERT_EQ(test, 0, ret);
	}
	return mapped;
}

static void mops_decode_all_96_source_leaves(struct kunit *test)
{
	u32 ordinal;

	for (ordinal = 2704U; ordinal <= 2751U; ordinal++) {
		struct orlix_tcti_decoded_instruction decoded =
			orlix_tcti_decode_aarch64(mops_instruction(test, ordinal));

		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_MOPS_COPY,
				decoded.decode_class);
		KUNIT_EXPECT_TRUE(test, decoded.mops_forward_only);
		KUNIT_EXPECT_EQ(test, ordinal, decoded.mops_source_ordinal);
	}
	for (ordinal = 2764U; ordinal <= 2811U; ordinal++) {
		struct orlix_tcti_decoded_instruction decoded =
			orlix_tcti_decode_aarch64(mops_instruction(test, ordinal));

		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_MOPS_COPY,
				decoded.decode_class);
		KUNIT_EXPECT_FALSE(test, decoded.mops_forward_only);
		KUNIT_EXPECT_EQ(test, ordinal, decoded.mops_source_ordinal);
	}
}

static void mops_rejects_reserved_and_constrained_unpredictable_forms(struct kunit *test)
{
	struct orlix_tcti_decoded_instruction decoded;
	u32 instruction = mops_instruction(test, 2704U);

	decoded = orlix_tcti_decode_aarch64(instruction | BIT(30));
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED, decoded.decode_class);
	decoded = orlix_tcti_decode_aarch64(instruction | BIT(31));
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED, decoded.decode_class);
	decoded = orlix_tcti_decode_aarch64((instruction & ~GENMASK(23, 22)) |
					      (3U << 22));
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED, decoded.decode_class);
}

static void mops_production_gadget_forwards_and_writes_back(struct kunit *test)
{
	const u8 initial[] = { 0x10, 0x20, 0x30, 0x40 };
	const u32 program[] = {
		0x19010440U, 0x19410440U, 0x19810440U, MOPS_SVC,
	};
	struct pt_regs regs = {};
	struct orlix_tcti_result result;
	u8 observed[sizeof(initial)] = {};
	unsigned long source = mops_map(test, initial, sizeof(initial));
	unsigned long destination = mops_map(test, NULL, 0);
	unsigned long code = mops_map(test, program, sizeof(program));
	int ret;

	ret = sys_mprotect(code, PAGE_SIZE, PROT_READ | PROT_EXEC);
	KUNIT_ASSERT_EQ(test, 0, ret);
	regs.regs[0] = destination;
	regs.regs[1] = source;
	regs.regs[2] = sizeof(initial);
	regs.pc = code;
	regs.pstate = PSR_MODE_EL0t;
	regs.syscallno = NO_SYSCALL;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, 0L, result.status);
	KUNIT_EXPECT_EQ(test, 0ULL, regs.regs[2]);
	KUNIT_EXPECT_EQ(test, source + sizeof(initial), regs.regs[1]);
	KUNIT_EXPECT_EQ(test, destination + sizeof(initial), regs.regs[0]);
	KUNIT_EXPECT_EQ(test, (u64)PSR_C_BIT,
			regs.pstate & (PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT | PSR_V_BIT));
	ret = orlix_tcti_read_user_data(current->mm, destination, observed,
					 sizeof(observed));
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_MEMEQ(test, initial, observed, sizeof(initial));
}

static void mops_production_gadget_selects_backward_overlap_direction(struct kunit *test)
{
	u8 initial[] = { 0, 1, 2, 3, 4, 5, 6, 7 };
	u8 expected[] = { 0, 1, 0, 1, 2, 3, 4, 5 };
	const u32 program[] = {
		0x1d010440U, 0x1d410440U, 0x1d810440U, MOPS_SVC,
	};
	struct pt_regs regs = {};
	struct orlix_tcti_result result;
	unsigned long data = mops_map(test, initial, sizeof(initial));
	unsigned long code = mops_map(test, program, sizeof(program));
	u8 observed[sizeof(initial)] = {};
	int ret;

	ret = sys_mprotect(code, PAGE_SIZE, PROT_READ | PROT_EXEC);
	KUNIT_ASSERT_EQ(test, 0, ret);
	/* CPYP consumes decrementing end pointers. */
	regs.regs[0] = data + sizeof(initial);
	regs.regs[1] = data + 6;
	regs.regs[2] = 6;
	regs.pc = code;
	regs.pstate = PSR_MODE_EL0t;
	regs.syscallno = NO_SYSCALL;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, 0L, result.status);
	ret = orlix_tcti_read_user_data(current->mm, data, observed, sizeof(observed));
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_EXPECT_MEMEQ(test, expected, observed, sizeof(observed));
	KUNIT_EXPECT_TRUE(test, regs.pstate & PSR_N_BIT);
	KUNIT_EXPECT_TRUE(test, regs.pstate & PSR_C_BIT);
	KUNIT_EXPECT_EQ(test, data, regs.regs[1]);
	KUNIT_EXPECT_EQ(test, data + 2, regs.regs[0]);
	KUNIT_EXPECT_EQ(test, 0ULL, regs.regs[2]);
}

/*
 * All CPY option forms use the same byte-copy route with the held guest
 * configuration: TCTI does not advertise MTE and every guest mapping used by
 * this test is a user mapping. Thus tag, unprivileged, and non-temporal
 * qualifiers have no distinct guest-visible behavior. Exercise every option
 * through resume_user, rather than treating that equivalence as decode-only.
 */
static void mops_production_gadget_executes_all_options(struct kunit *test)
{
	const u8 forward_initial[] = { 0x10, 0x20, 0x30, 0x40 };
	const u8 backward_initial[] = { 0, 1, 2, 3, 4, 5, 6, 7 };
	const u8 backward_expected[] = { 0, 1, 0, 1, 2, 3, 4, 5 };
	u32 option;

	for (option = 0; option < 16; option++) {
		u32 program[] = {
			mops_instruction(test, 2704U + option),
			mops_instruction(test, 2720U + option),
			mops_instruction(test, 2736U + option), MOPS_SVC,
		};
		struct pt_regs regs = {};
		struct orlix_tcti_result result;
		u8 observed[sizeof(forward_initial)] = {};
		unsigned long source = mops_map(test, forward_initial,
						       sizeof(forward_initial));
		unsigned long destination = mops_map(test, NULL, 0);
		unsigned long code = mops_map(test, program, sizeof(program));
		int ret;

		KUNIT_ASSERT_EQ(test, 0, sys_mprotect(code, PAGE_SIZE,
						      PROT_READ | PROT_EXEC));
		regs.regs[0] = destination;
		regs.regs[1] = source;
		regs.regs[2] = sizeof(forward_initial);
		regs.pc = code;
		regs.pstate = PSR_MODE_EL0t;
		regs.syscallno = NO_SYSCALL;
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason,
				    "forward option=%u", option);
		KUNIT_EXPECT_EQ_MSG(test, 0L, result.status, "forward option=%u",
				    option);
		KUNIT_EXPECT_EQ_MSG(test, 0ULL, regs.regs[2], "forward option=%u",
				    option);
		KUNIT_EXPECT_EQ_MSG(test, source + sizeof(forward_initial), regs.regs[1],
				    "forward option=%u", option);
		KUNIT_EXPECT_EQ_MSG(test, destination + sizeof(forward_initial),
				    regs.regs[0], "forward option=%u", option);
		KUNIT_EXPECT_EQ_MSG(test, (u64)PSR_C_BIT,
				    regs.pstate & (PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT |
						   PSR_V_BIT), "forward option=%u", option);
		ret = orlix_tcti_read_user_data(current->mm, destination, observed,
						       sizeof(observed));
		KUNIT_ASSERT_EQ(test, 0, ret);
		KUNIT_EXPECT_MEMEQ_MSG(test, forward_initial, observed,
				       sizeof(observed), "forward option=%u", option);
	}
	for (option = 0; option < 16; option++) {
		u32 program[] = {
			mops_instruction(test, 2764U + option),
			mops_instruction(test, 2780U + option),
			mops_instruction(test, 2796U + option), MOPS_SVC,
		};
		struct pt_regs regs = {};
		struct orlix_tcti_result result;
		u8 observed[sizeof(backward_initial)] = {};
		unsigned long data = mops_map(test, backward_initial,
						     sizeof(backward_initial));
		unsigned long code = mops_map(test, program, sizeof(program));
		int ret;

		KUNIT_ASSERT_EQ(test, 0, sys_mprotect(code, PAGE_SIZE,
						      PROT_READ | PROT_EXEC));
		regs.regs[0] = data + sizeof(backward_initial);
		regs.regs[1] = data + 6;
		regs.regs[2] = 6;
		regs.pc = code;
		regs.pstate = PSR_MODE_EL0t;
		regs.syscallno = NO_SYSCALL;
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason,
				    "backward option=%u", option);
		KUNIT_EXPECT_EQ_MSG(test, 0L, result.status, "backward option=%u",
				    option);
		KUNIT_EXPECT_EQ_MSG(test, 0ULL, regs.regs[2], "backward option=%u",
				    option);
		KUNIT_EXPECT_EQ_MSG(test, data, regs.regs[1], "backward option=%u",
				    option);
		KUNIT_EXPECT_EQ_MSG(test, data + 2, regs.regs[0],
				    "backward option=%u", option);
		KUNIT_EXPECT_EQ_MSG(test, (u64)(PSR_N_BIT | PSR_C_BIT),
				    regs.pstate & (PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT |
						   PSR_V_BIT), "backward option=%u", option);
		ret = orlix_tcti_read_user_data(current->mm, data, observed,
						       sizeof(observed));
		KUNIT_ASSERT_EQ(test, 0, ret);
		KUNIT_EXPECT_MEMEQ_MSG(test, backward_expected, observed,
				       sizeof(observed), "backward option=%u", option);
	}
}

static void mops_production_rejects_constrained_register_forms(struct kunit *test)
{
	const struct { u8 rd; u8 rm; u8 rn; } forms[] = {
		{ 31, 1, 2 }, { 0, 31, 2 }, { 0, 1, 31 },
		{ 0, 0, 2 }, { 0, 1, 0 }, { 0, 1, 1 },
	};
	u32 index;

	for (index = 0; index < ARRAY_SIZE(forms); index++) {
		u32 program[] = {
			mops_instruction_with_registers(test, 2704U, forms[index].rd,
						     forms[index].rm, forms[index].rn), MOPS_SVC,
		};
		struct pt_regs regs = {};
		struct pt_regs before;
		struct orlix_tcti_result result;
		unsigned long code = mops_map(test, program, sizeof(program));

		KUNIT_ASSERT_EQ(test, 0, sys_mprotect(code, PAGE_SIZE,
						      PROT_READ | PROT_EXEC));
		regs.regs[0] = 0x1000;
		regs.regs[1] = 0x2000;
		regs.regs[2] = 4;
		regs.pc = code;
		regs.pstate = PSR_MODE_EL0t;
		regs.syscallno = NO_SYSCALL;
		before = regs;
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
				result.reason);
		KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, result.status);
		KUNIT_EXPECT_EQ(test, code, result.pc);
		KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
	}
	{
		u32 program[] = { mops_instruction(test, 2704U) | BIT(31), MOPS_SVC };
		struct pt_regs regs = {};
		struct pt_regs before;
		struct orlix_tcti_result result;
		unsigned long code = mops_map(test, program, sizeof(program));

		KUNIT_ASSERT_EQ(test, 0, sys_mprotect(code, PAGE_SIZE,
						      PROT_READ | PROT_EXEC));
		regs.regs[0] = 0x1000;
		regs.regs[1] = 0x2000;
		regs.regs[2] = 4;
		regs.pc = code;
		regs.pstate = PSR_MODE_EL0t;
		regs.syscallno = NO_SYSCALL;
		before = regs;
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
				result.reason);
		KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, result.status);
		KUNIT_EXPECT_EQ(test, code, result.pc);
		KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
	}
}

static void mops_epilogue_preserves_partial_guest_progress_on_fault(struct kunit *test)
{
	const u8 byte = 0xa5;
	struct orlix_tcti_decoded_instruction prologue =
		orlix_tcti_decode_aarch64(0x19010440U);
	struct orlix_tcti_decoded_instruction main =
		orlix_tcti_decode_aarch64(0x19410440U);
	struct orlix_tcti_decoded_instruction epilogue =
		orlix_tcti_decode_aarch64(0x19810440U);
	struct pt_regs regs = { .pstate = PSR_MODE_EL0t };
	unsigned long source;
	unsigned long destination;
	unsigned long fault_address = 0;
	int ret;

	source = ksys_mmap_pgoff(0, 2 * PAGE_SIZE, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(source));
	destination = mops_map(test, NULL, 0);
	ret = orlix_tcti_write_user_data(current->mm, source + PAGE_SIZE - 1,
					 &byte, sizeof(byte));
	KUNIT_ASSERT_EQ(test, 0, ret);
	ret = sys_mprotect(source + PAGE_SIZE, PAGE_SIZE, PROT_NONE);
	KUNIT_ASSERT_EQ(test, 0, ret);
	regs.regs[0] = destination;
	regs.regs[1] = source + PAGE_SIZE - 1;
	regs.regs[2] = 2;
	regs.pc = 0x1000;
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_decoded_semantics(
		current->mm, &regs, &prologue, &fault_address));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_decoded_semantics(
		current->mm, &regs, &main, &fault_address));
	ret = orlix_tcti_execute_decoded_semantics(current->mm, &regs, &epilogue,
						      &fault_address);
	KUNIT_EXPECT_LT(test, ret, 0);
	KUNIT_EXPECT_EQ(test, source + PAGE_SIZE, fault_address);
	KUNIT_EXPECT_EQ(test, destination + 1, regs.regs[0]);
	KUNIT_EXPECT_EQ(test, source + PAGE_SIZE, regs.regs[1]);
	KUNIT_EXPECT_EQ(test, 1ULL, regs.regs[2]);
}

static void mops_epilogue_preserves_destination_fault_progress(struct kunit *test)
{
	const u8 bytes[] = { 0xa5, 0x5a };
	struct orlix_tcti_decoded_instruction prologue =
		orlix_tcti_decode_aarch64(0x19010440U);
	struct orlix_tcti_decoded_instruction main =
		orlix_tcti_decode_aarch64(0x19410440U);
	struct orlix_tcti_decoded_instruction epilogue =
		orlix_tcti_decode_aarch64(0x19810440U);
	struct pt_regs regs = { .pstate = PSR_MODE_EL0t };
	unsigned long source = mops_map(test, bytes, sizeof(bytes));
	unsigned long destination;
	unsigned long fault_address = 0;
	u8 observed = 0;
	int ret;

	destination = ksys_mmap_pgoff(0, 2 * PAGE_SIZE, PROT_READ | PROT_WRITE,
					 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(destination));
	KUNIT_ASSERT_EQ(test, 0, sys_mprotect(destination + PAGE_SIZE,
						      PAGE_SIZE, PROT_NONE));
	regs.regs[0] = destination + PAGE_SIZE - 1;
	regs.regs[1] = source;
	regs.regs[2] = 2;
	regs.pc = 0x1000;
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_decoded_semantics(
		current->mm, &regs, &prologue, &fault_address));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_decoded_semantics(
		current->mm, &regs, &main, &fault_address));
	ret = orlix_tcti_execute_decoded_semantics(current->mm, &regs, &epilogue,
						      &fault_address);
	KUNIT_EXPECT_LT(test, ret, 0);
	KUNIT_EXPECT_EQ(test, destination + PAGE_SIZE, fault_address);
	KUNIT_EXPECT_EQ(test, destination + PAGE_SIZE, regs.regs[0]);
	KUNIT_EXPECT_EQ(test, source + 1, regs.regs[1]);
	KUNIT_EXPECT_EQ(test, 1ULL, regs.regs[2]);
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_read_user_data(current->mm,
		destination + PAGE_SIZE - 1, &observed, sizeof(observed)));
	KUNIT_EXPECT_EQ(test, bytes[0], observed);
}

static void mops_epilogue_preserves_backward_fault_progress(struct kunit *test)
{
	const u8 byte = 0x5a;
	struct orlix_tcti_decoded_instruction prologue =
		orlix_tcti_decode_aarch64(0x1d010440U);
	struct orlix_tcti_decoded_instruction main =
		orlix_tcti_decode_aarch64(0x1d410440U);
	struct orlix_tcti_decoded_instruction epilogue =
		orlix_tcti_decode_aarch64(0x1d810440U);
	struct pt_regs regs = { .pstate = PSR_MODE_EL0t };
	unsigned long data;
	unsigned long fault_address = 0;
	u8 observed = 0;
	int ret;

	data = ksys_mmap_pgoff(0, 3 * PAGE_SIZE, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(data));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_write_user_data(current->mm,
		data + PAGE_SIZE, &byte, sizeof(byte)));
	KUNIT_ASSERT_EQ(test, 0, sys_mprotect(data, PAGE_SIZE, PROT_NONE));
	regs.regs[0] = data + 2 * PAGE_SIZE;
	regs.regs[1] = data + PAGE_SIZE + 1;
	regs.regs[2] = 2;
	regs.pc = 0x1000;
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_decoded_semantics(
		current->mm, &regs, &prologue, &fault_address));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_execute_decoded_semantics(
		current->mm, &regs, &main, &fault_address));
	ret = orlix_tcti_execute_decoded_semantics(current->mm, &regs, &epilogue,
						      &fault_address);
	KUNIT_EXPECT_LT(test, ret, 0);
	KUNIT_EXPECT_EQ(test, data + PAGE_SIZE - 1, fault_address);
	KUNIT_EXPECT_EQ(test, data + 2 * PAGE_SIZE - 1, regs.regs[0]);
	KUNIT_EXPECT_EQ(test, data + PAGE_SIZE, regs.regs[1]);
	KUNIT_EXPECT_EQ(test, 1ULL, regs.regs[2]);
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_read_user_data(current->mm,
		data + 2 * PAGE_SIZE - 1, &observed, sizeof(observed)));
	KUNIT_EXPECT_EQ(test, byte, observed);
}

static struct kunit_case orlix_tcti_mops_copy_cases[] = {
	KUNIT_CASE(mops_decode_all_96_source_leaves),
	KUNIT_CASE(mops_rejects_reserved_and_constrained_unpredictable_forms),
	KUNIT_CASE(mops_production_gadget_forwards_and_writes_back),
	KUNIT_CASE(mops_production_gadget_selects_backward_overlap_direction),
	KUNIT_CASE(mops_production_gadget_executes_all_options),
	KUNIT_CASE(mops_production_rejects_constrained_register_forms),
	KUNIT_CASE(mops_epilogue_preserves_partial_guest_progress_on_fault),
	KUNIT_CASE(mops_epilogue_preserves_destination_fault_progress),
	KUNIT_CASE(mops_epilogue_preserves_backward_fault_progress),
	{}
};

static int orlix_tcti_mops_copy_init(struct kunit *test)
{
	struct mm_struct *mm = mm_alloc();

	if (!mm)
		return -ENOMEM;
	kthread_use_mm(mm);
	test->priv = mm;
	return 0;
}

static void orlix_tcti_mops_copy_exit(struct kunit *test)
{
	struct mm_struct *mm = test->priv;

	if (!mm)
		return;
	kthread_unuse_mm(mm);
	mmput(mm);
}

static struct kunit_suite orlix_tcti_mops_copy_suite = {
	.name = "orlix-tcti-mops-copy",
	.init = orlix_tcti_mops_copy_init,
	.exit = orlix_tcti_mops_copy_exit,
	.test_cases = orlix_tcti_mops_copy_cases,
};

kunit_test_suite(orlix_tcti_mops_copy_suite);

MODULE_LICENSE("GPL");
