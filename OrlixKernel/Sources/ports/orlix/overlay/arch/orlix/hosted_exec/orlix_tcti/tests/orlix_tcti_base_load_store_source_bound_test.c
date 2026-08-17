// SPDX-License-Identifier: GPL-2.0-only
/*
 * Source-bound production-path coverage for the 209 BASE_LOAD_STORE leaves
 * owned by GitHub #134. Executed observations bind through the #120 contract.
 */
#include <asm/ptrace.h>
#include <asm/orlix_tcti.h>
#include <kunit/test.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/string.h>
#include <linux/syscalls.h>

#include "../decode_aarch64.h"
#include "target_execution_slice_map.h"

#define BLS_SVC 0xd4000001U

struct bls_source_leaf {
	u32 ordinal;
	const char *name;
	u32 mask;
	u32 pattern;
};

#define ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE(...)
#define ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW(ordinal, id, mnemonic, operation, mask, \
					     pattern, feature_predicate, offset, length) \
	{ ordinal, id, mask, pattern },
static const struct bls_source_leaf bls_source_leaves[] = {
#include "../isa/source_manifest.def"
};
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE

static const struct bls_source_leaf *bls_source_leaf(u32 ordinal)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(bls_source_leaves); index++)
		if (bls_source_leaves[index].ordinal == ordinal)
			return &bls_source_leaves[index];
	return NULL;
}

static bool bls_is_prefetch(const char *name)
{
	return !strncmp(name, "PRFM", 4) || !strncmp(name, "PRFUM", 5) ||
		!strncmp(name, "RPRFM", 5);
}

static bool bls_is_pair(const char *name)
{
	return strstr(name, "ldstpair") || strstr(name, "ldstnapair");
}

static bool bls_is_regoff(const char *name)
{
	return strstr(name, "ldst_regoff") != NULL;
}

static bool bls_is_loadlit(const char *name)
{
	return strstr(name, "loadlit") != NULL;
}

static u32 bls_test_instruction(const struct bls_source_leaf *leaf)
{
	u32 instruction = leaf->pattern;

	instruction |= 0x0U; /* Rt = x0 / v0 */
	instruction |= 1U << 5; /* Rn = x1 */
	if (bls_is_pair(leaf->name))
		instruction |= 2U << 10; /* Rt2 = x2 / v2 */
	if (bls_is_regoff(leaf->name)) {
		instruction |= 3U << 16; /* Rm = x3 */
		instruction |= 3U << 13; /* option = LSL/UXTX */
		if (strstr(leaf->name, "32BL") || strstr(leaf->name, "_BL_"))
			instruction |= 3U << 13;
		else if (strstr(leaf->name, "32B_") || strstr(leaf->name, "_B_ldst_regoff"))
			instruction |= 2U << 13; /* UXTW */
	}
	if (bls_is_loadlit(leaf->name))
		instruction = (leaf->pattern & leaf->mask) | (2U << 5);
	return instruction;
}

static bool bls_decode_is_memory(enum orlix_tcti_decode_class decode_class)
{
	return decode_class == ORLIX_TCTI_DECODE_LOAD_LITERAL ||
		decode_class == ORLIX_TCTI_DECODE_LOAD_STORE_PAIR ||
		decode_class == ORLIX_TCTI_DECODE_LOAD_STORE_UNSIGNED_IMMEDIATE ||
		decode_class == ORLIX_TCTI_DECODE_LOAD_STORE_SIGNED_IMMEDIATE ||
		decode_class == ORLIX_TCTI_DECODE_LOAD_STORE_REGISTER_OFFSET ||
		decode_class == ORLIX_TCTI_DECODE_HINT;
}

static int bls_test_init(struct kunit *test)
{
	struct mm_struct *mm = mm_alloc();

	if (!mm)
		return -ENOMEM;
	kthread_use_mm(mm);
	test->priv = mm;
	return 0;
}

static void bls_test_exit(struct kunit *test)
{
	struct mm_struct *mm = test->priv;

	if (!mm)
		return;
	kthread_unuse_mm(mm);
	mmput(mm);
}

static void bls_source_decode(struct kunit *test)
{
	const struct orlix_tcti_execution_slice_map *map =
		orlix_tcti_execution_slice_map_canonical();
	size_t index;
	size_t seen = 0;

	KUNIT_ASSERT_NOT_NULL(test, map);
	for (index = 0; index < map->counts.leaf_count; index++) {
		const struct orlix_tcti_execution_slice_member *member =
			&map->members[index];
		const struct orlix_tcti_execution_slice_family *family;
		const struct bls_source_leaf *leaf;
		struct orlix_tcti_decoded_instruction decoded;

		family = &map->families[member->family_index];
		if (family->issue_id != 134U)
			continue;
		seen++;
		leaf = bls_source_leaf(member->ordinal);
		KUNIT_ASSERT_NOT_NULL(test, leaf);
		decoded = orlix_tcti_decode_aarch64(bls_test_instruction(leaf));
		KUNIT_EXPECT_TRUE_MSG(test, bls_decode_is_memory(decoded.decode_class),
			"%s ordinal %u class %u insn %#x", leaf->name,
			leaf->ordinal, decoded.decode_class,
			bls_test_instruction(leaf));
	}
	KUNIT_EXPECT_EQ(test, 209U, seen);
}

static unsigned long bls_map_program(struct kunit *test, u32 instruction)
{
	u32 program[4] = { instruction, BLS_SVC, 0x11223344U, 0x55667788U };
	unsigned long mapped;
	int ret;

	mapped = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));
	ret = orlix_tcti_write_user_data(current->mm, mapped, program,
					 sizeof(program));
	KUNIT_ASSERT_EQ(test, 0, ret);
	ret = sys_mprotect(mapped, PAGE_SIZE, PROT_READ | PROT_EXEC);
	KUNIT_ASSERT_EQ(test, 0, ret);
	return mapped;
}

static unsigned long bls_map_data(struct kunit *test)
{
	unsigned long mapped;
	u8 bytes[64];
	size_t index;

	mapped = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));
	for (index = 0; index < sizeof(bytes); index++)
		bytes[index] = (u8)(0x80U + index);
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_write_user_data(current->mm, mapped,
		bytes, sizeof(bytes)));
	return mapped;
}

static void bls_production_resume(struct kunit *test)
{
	const struct orlix_tcti_execution_slice_map *map =
		orlix_tcti_execution_slice_map_canonical();
	size_t index;
	size_t seen = 0;

	KUNIT_ASSERT_NOT_NULL(test, map);
	for (index = 0; index < map->counts.leaf_count; index++) {
		const struct orlix_tcti_execution_slice_member *member =
			&map->members[index];
		const struct orlix_tcti_execution_slice_family *family;
		const struct bls_source_leaf *leaf;
		struct orlix_tcti_decoded_instruction decoded;
		struct orlix_tcti_result result;
		struct pt_regs regs = {};
		unsigned long code;
		unsigned long data = 0;
		u32 instruction;

		family = &map->families[member->family_index];
		if (family->issue_id != 134U)
			continue;
		seen++;
		leaf = bls_source_leaf(member->ordinal);
		KUNIT_ASSERT_NOT_NULL(test, leaf);
		instruction = bls_test_instruction(leaf);
		decoded = orlix_tcti_decode_aarch64(instruction);
		KUNIT_ASSERT_TRUE_MSG(test, bls_decode_is_memory(decoded.decode_class),
			"%s ordinal %u class %u", leaf->name, leaf->ordinal,
			decoded.decode_class);

		if (!bls_is_prefetch(leaf->name) && !bls_is_loadlit(leaf->name))
			data = bls_map_data(test);
		code = bls_map_program(test, instruction);
		regs.pc = code;
		regs.pstate = PSR_MODE_EL0t;
		regs.syscallno = NO_SYSCALL;
		regs.regs[0] = 0x0102030405060708ULL;
		regs.regs[1] = data ? data + 16 : 0;
		regs.regs[2] = 0x1112131415161718ULL;
		regs.regs[3] = 0;
		current->thread.user_simd_valid = 1;

		result = orlix_tcti_resume_user(current, &regs, current->mm);
		if (bls_is_prefetch(leaf->name)) {
			KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_SYSCALL,
				result.reason, "%s ordinal %u", leaf->name,
				leaf->ordinal);
		} else {
			KUNIT_EXPECT_TRUE_MSG(test,
				result.reason == ORLIX_TCTI_EXIT_SYSCALL ||
				result.reason == ORLIX_TCTI_EXIT_USER_FAULT ||
				result.reason == ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
				"%s ordinal %u reason %u status %ld",
				leaf->name, leaf->ordinal, result.reason,
				result.status);
		}
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
		if (data)
			KUNIT_EXPECT_EQ(test, 0, vm_munmap(data, PAGE_SIZE));
	}
	KUNIT_EXPECT_EQ(test, 209U, seen);
}

static struct kunit_case bls_cases[] = {
	KUNIT_CASE(bls_source_decode),
	KUNIT_CASE(bls_production_resume),
	{}
};

static struct kunit_suite orlix_tcti_base_load_store_source_bound_test_suite = {
	.name = "orlix-tcti-base-load-store-source-bound",
	.init = bls_test_init,
	.exit = bls_test_exit,
	.test_cases = bls_cases,
};
kunit_test_suite(orlix_tcti_base_load_store_source_bound_test_suite);
