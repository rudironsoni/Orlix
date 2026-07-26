// SPDX-License-Identifier: GPL-2.0-only
/*
 * Source-bound production-path coverage for the eight CRC32 and CRC32C
 * leaves in the pinned AARCHMRS source.
 */
#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/orlix_tcti.h>
#include <kunit/test.h>
#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/syscalls.h>

#include "../decode_aarch64.h"

#define CRC_SOURCE_SVC 0xd4000001U
#define CRC_SOURCE_RD 17U
#define CRC_SOURCE_RN 9U
#define CRC_SOURCE_RM 3U

struct crc_source_manifest_leaf {
	u32 ordinal;
	const char *name;
	const char *mnemonic;
	const char *operation;
	u32 mask;
	u32 pattern;
};

#define ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE(...)
#define ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW(ordinal, name, mnemonic, operation, \
	mask, pattern, feature, offset, length) \
	{ ordinal, name, mnemonic, operation, mask, pattern },
static const struct crc_source_manifest_leaf crc_source_manifest[] = {
#include "../isa/source_manifest.def"
};
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE

struct crc_source_leaf {
	u16 ordinal;
	const char *name;
	const char *mnemonic;
	const char *operation;
	u32 mask;
	u32 pattern;
	u8 access_size;
	bool source_is_64bit;
	bool castagnoli;
};

static const struct crc_source_leaf crc_source_leaves[] = {
	{ 3362U, "CRC32B_32C_dp_2src", "CRC32B", "CRC32",
	  0xffe0fc00U, 0x1ac04000U, 1, false, false },
	{ 3363U, "CRC32H_32C_dp_2src", "CRC32H", "CRC32",
	  0xffe0fc00U, 0x1ac04400U, 2, false, false },
	{ 3364U, "CRC32W_32C_dp_2src", "CRC32W", "CRC32",
	  0xffe0fc00U, 0x1ac04800U, 4, false, false },
	{ 3382U, "CRC32X_64C_dp_2src", "CRC32X", "CRC32",
	  0xffe0fc00U, 0x9ac04c00U, 8, true, false },
	{ 3365U, "CRC32CB_32C_dp_2src", "CRC32CB", "CRC32C",
	  0xffe0fc00U, 0x1ac05000U, 1, false, true },
	{ 3366U, "CRC32CH_32C_dp_2src", "CRC32CH", "CRC32C",
	  0xffe0fc00U, 0x1ac05400U, 2, false, true },
	{ 3367U, "CRC32CW_32C_dp_2src", "CRC32CW", "CRC32C",
	  0xffe0fc00U, 0x1ac05800U, 4, false, true },
	{ 3383U, "CRC32CX_64C_dp_2src", "CRC32CX", "CRC32C",
	  0xffe0fc00U, 0x9ac05c00U, 8, true, true },
};

static const struct crc_source_manifest_leaf *
crc_source_manifest_leaf(u16 ordinal)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(crc_source_manifest); index++)
		if (crc_source_manifest[index].ordinal == ordinal)
			return &crc_source_manifest[index];
	return NULL;
}

static const struct crc_source_leaf *crc_source_leaf_for_instruction(u32 instruction)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(crc_source_leaves); index++)
		if ((instruction & crc_source_leaves[index].mask) ==
		    crc_source_leaves[index].pattern)
			return &crc_source_leaves[index];
	return NULL;
}

static u32 crc_source_instruction(const struct crc_source_leaf *leaf)
{
	return leaf->pattern | ((u32)CRC_SOURCE_RM << 16) |
	       ((u32)CRC_SOURCE_RN << 5) | CRC_SOURCE_RD;
}

static unsigned long crc_source_map_program(struct kunit *test, u32 instruction)
{
	u32 program[] = { instruction, CRC_SOURCE_SVC };
	unsigned long address;
	int ret;

	address = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(address));
	ret = orlix_tcti_write_user_data(current->mm, address, program, sizeof(program));
	KUNIT_ASSERT_EQ(test, 0, ret);
	ret = sys_mprotect(address, PAGE_SIZE, PROT_READ | PROT_EXEC);
	KUNIT_ASSERT_EQ(test, 0, ret);
	return address;
}

/* Independent reflected CRC reference for the Arm CRC32 and CRC32C polynomials. */
static u32 crc_source_expected(u32 accumulator, u64 value, u8 access_size,
			       bool castagnoli)
{
	u32 polynomial = castagnoli ? 0x82f63b78U : 0xedb88320U;
	u8 byte;

	for (byte = 0; byte < access_size; byte++) {
		u8 bit;

		accumulator ^= (u8)(value >> (byte * 8));
		for (bit = 0; bit < 8; bit++)
			accumulator = (accumulator >> 1) ^
				((accumulator & 1U) ? polynomial : 0);
	}
	return accumulator;
}

static void crc_source_seed_regs(struct pt_regs *regs, unsigned long address)
{
	unsigned int index;

	memset(regs, 0, sizeof(*regs));
	for (index = 0; index < ARRAY_SIZE(regs->regs); index++)
		regs->regs[index] = 0x8877665544332211ULL ^ ((u64)index << 40);
	regs->regs[CRC_SOURCE_RN] = 0xfeedface12345678ULL;
	regs->regs[CRC_SOURCE_RM] = 0x8877665544332211ULL;
	regs->regs[CRC_SOURCE_RD] = 0xa5a5a5a5a5a5a5a5ULL;
	regs->pc = address;
	regs->sp = 0x00000001fffffff0ULL;
	regs->pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_C_BIT | PSR_V_BIT;
	regs->orig_x0 = 0xd1b54a32d192ed03ULL;
	regs->syscallno = NO_SYSCALL;
	regs->unused = 0x6d5a56c3U;
}

static void crc_source_leaves_bind_canonical_artifacts(struct kunit *test)
{
	size_t index;

	KUNIT_ASSERT_EQ(test, 8U, ARRAY_SIZE(crc_source_leaves));
	for (index = 0; index < ARRAY_SIZE(crc_source_leaves); index++) {
		const struct crc_source_leaf *leaf = &crc_source_leaves[index];
		const struct crc_source_manifest_leaf *source =
			crc_source_manifest_leaf(leaf->ordinal);
		struct orlix_tcti_decoded_instruction decoded =
			orlix_tcti_decode_aarch64(crc_source_instruction(leaf));

		KUNIT_ASSERT_NOT_NULL(test, source);
		KUNIT_EXPECT_STREQ(test, leaf->name, source->name);
		KUNIT_EXPECT_STREQ(test, leaf->mnemonic, source->mnemonic);
		KUNIT_EXPECT_STREQ(test, leaf->operation, source->operation);
		KUNIT_EXPECT_EQ(test, leaf->mask, source->mask);
		KUNIT_EXPECT_EQ(test, leaf->pattern, source->pattern);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_DATA_PROCESSING_2SOURCE,
				decoded.decode_class);
		KUNIT_EXPECT_EQ(test, leaf->castagnoli ? ORLIX_TCTI_DP2_CRC32C :
				ORLIX_TCTI_DP2_CRC32, decoded.dp2_op);
		KUNIT_EXPECT_EQ(test, CRC_SOURCE_RD, decoded.rd);
		KUNIT_EXPECT_EQ(test, CRC_SOURCE_RN, decoded.rn);
		KUNIT_EXPECT_EQ(test, CRC_SOURCE_RM, decoded.rm);
		KUNIT_EXPECT_EQ(test, leaf->access_size, decoded.access_size);
		KUNIT_EXPECT_EQ(test, sizeof(u32), decoded.result_size);
		KUNIT_EXPECT_EQ(test, leaf->source_is_64bit, decoded.is_64bit);
	}
}

static void crc_source_leaves_execute_from_mapped_rx(struct kunit *test)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(crc_source_leaves); index++) {
		const struct crc_source_leaf *leaf = &crc_source_leaves[index];
		struct pt_regs regs;
		struct pt_regs expected;
		struct orlix_tcti_result result;
		unsigned long address;
		u32 expected_crc;

		address = crc_source_map_program(test, crc_source_instruction(leaf));
		crc_source_seed_regs(&regs, address);
		expected = regs;
		expected_crc = crc_source_expected((u32)regs.regs[CRC_SOURCE_RN],
			regs.regs[CRC_SOURCE_RM], leaf->access_size, leaf->castagnoli);
		expected.regs[CRC_SOURCE_RD] = expected_crc;
		expected.pc = address + sizeof(u32);
		result = orlix_tcti_resume_user(current, &regs, current->mm);

		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason,
				    "%s", leaf->name);
		KUNIT_EXPECT_EQ(test, 0L, result.status);
		KUNIT_EXPECT_EQ(test, 0UL, result.fault_address);
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_ACCESS_FETCH, result.fault_access);
		KUNIT_EXPECT_EQ(test, address + sizeof(u32), result.pc);
		KUNIT_EXPECT_EQ(test, CRC_SOURCE_SVC, result.instruction);
		KUNIT_EXPECT_EQ(test, (u64)expected_crc,
				regs.regs[CRC_SOURCE_RD]);
		KUNIT_EXPECT_MEMEQ(test, &expected, &regs, sizeof(regs));
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(address, PAGE_SIZE));
	}
}

static void crc_source_reserved_neighbours_reject_from_mapped_rx(
	struct kunit *test)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(crc_source_leaves); index++) {
		const struct crc_source_leaf *leaf = &crc_source_leaves[index];
		bool rejected = false;
		u8 bit;

		for (bit = 0; bit < 32; bit++) {
			struct pt_regs regs;
			struct pt_regs before;
			struct orlix_tcti_result result;
			unsigned long address;
			u32 instruction;

			if (!(leaf->mask & BIT(bit)))
				continue;
			instruction = crc_source_instruction(leaf) ^ BIT(bit);
			if (crc_source_leaf_for_instruction(instruction))
				continue;
			if (orlix_tcti_decode_aarch64(instruction).decode_class !=
			    ORLIX_TCTI_DECODE_UNSUPPORTED)
				continue;
			address = crc_source_map_program(test, instruction);
			crc_source_seed_regs(&regs, address);
			before = regs;
			result = orlix_tcti_resume_user(current, &regs, current->mm);

			KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
					    result.reason, "%s bit=%u", leaf->name, bit);
			KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, result.status);
			KUNIT_EXPECT_EQ(test, 0UL, result.fault_address);
			KUNIT_EXPECT_EQ(test, ORLIX_TCTI_ACCESS_FETCH,
					result.fault_access);
			KUNIT_EXPECT_EQ(test, address, result.pc);
			KUNIT_EXPECT_EQ(test, instruction, result.instruction);
			KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
			KUNIT_EXPECT_EQ(test, 0, vm_munmap(address, PAGE_SIZE));
			rejected = true;
			break;
		}
		KUNIT_EXPECT_TRUE_MSG(test, rejected, "%s", leaf->name);
	}
}

static struct kunit_case crc_source_bound_cases[] = {
	KUNIT_CASE(crc_source_leaves_bind_canonical_artifacts),
	KUNIT_CASE(crc_source_leaves_execute_from_mapped_rx),
	KUNIT_CASE(crc_source_reserved_neighbours_reject_from_mapped_rx),
	{}
};

static struct kunit_suite crc_source_bound_test_suite = {
	.name = "orlix-tcti-crc32-source-bound",
	.test_cases = crc_source_bound_cases,
};

kunit_test_suite(crc_source_bound_test_suite);
