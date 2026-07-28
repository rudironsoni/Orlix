// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/bitops.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/syscalls.h>
#include <linux/utsname.h>
#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/orlix_tcti.h>

#include "../block_cache.h"
#include "../decode_aarch64.h"
#include "orlix_tcti_test_suites.h"
#include "orlix_tcti_native_observation.h"
#include "target_proof_ingestion.h"
#include "target_proof_registry.h"

#define ADD_SUB_IMMEDIATE_SOURCE_MASK 0xff800000U
#define ADD_SUB_IMMEDIATE_VARIABLE_MASK (~ADD_SUB_IMMEDIATE_SOURCE_MASK)
#define ADD_SUB_IMMEDIATE_NZCV \
	(PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT | PSR_V_BIT)
#define ADD_SUB_IMMEDIATE_PSTATE_SEED \
	((0x155UL & ~PSR_MODE_MASK) | ADD_SUB_IMMEDIATE_NZCV)
#define ADD_SUB_IMMEDIATE_SVC 0xd4000001U

struct orlix_tcti_add_sub_immediate_leaf {
	u16 source_ordinal;
	const char *source_name;
	const char *operation_id;
	u32 source_pattern;
	bool is_64bit;
	bool subtract;
	bool set_flags;
};

/*
 * Exact direct leaves from the pinned Arm AARCHMRS 2026-06 source manifest.
 * CMN and CMP are operand aliases of the flag-setting leaves.
 */
static const struct orlix_tcti_add_sub_immediate_leaf
orlix_tcti_add_sub_immediate_leaves[] = {
	{ 2173, "ADD_32_addsub_imm", "ADD_addsub_imm", 0x11000000U,
	  false, false, false },
	{ 2174, "ADDS_32S_addsub_imm", "ADDS_addsub_imm", 0x31000000U,
	  false, false, true },
	{ 2175, "SUB_32_addsub_imm", "SUB_addsub_imm", 0x51000000U,
	  false, true, false },
	{ 2176, "SUBS_32S_addsub_imm", "SUBS_addsub_imm", 0x71000000U,
	  false, true, true },
	{ 2177, "ADD_64_addsub_imm", "ADD_addsub_imm", 0x91000000U,
	  true, false, false },
	{ 2178, "ADDS_64S_addsub_imm", "ADDS_addsub_imm", 0xb1000000U,
	  true, false, true },
	{ 2179, "SUB_64_addsub_imm", "SUB_addsub_imm", 0xd1000000U,
	  true, true, false },
	{ 2180, "SUBS_64S_addsub_imm", "SUBS_addsub_imm", 0xf1000000U,
	  true, true, true },
};

static u32 orlix_tcti_add_sub_immediate_instruction(
	const struct orlix_tcti_add_sub_immediate_leaf *leaf, bool shift,
	u16 immediate, u8 rn, u8 rd)
{
	return leaf->source_pattern | (shift ? BIT(22) : 0) |
		((u32)immediate << 10) | ((u32)rn << 5) | rd;
}

static const struct orlix_tcti_add_sub_immediate_leaf *
orlix_tcti_add_sub_immediate_source_leaf(u32 instruction)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_add_sub_immediate_leaves);
	     index++) {
		const struct orlix_tcti_add_sub_immediate_leaf *leaf =
			&orlix_tcti_add_sub_immediate_leaves[index];

		if ((instruction & ADD_SUB_IMMEDIATE_SOURCE_MASK) ==
		    leaf->source_pattern)
			return leaf;
	}
	return NULL;
}

static void orlix_tcti_add_sub_immediate_source_bindings(struct kunit *test)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_add_sub_immediate_leaves);
	     index++) {
		const struct orlix_tcti_add_sub_immediate_leaf *leaf =
			&orlix_tcti_add_sub_immediate_leaves[index];
		struct orlix_tcti_decoded_instruction decoded =
			orlix_tcti_decode_aarch64(leaf->source_pattern);

		KUNIT_EXPECT_EQ_MSG(test, leaf->source_pattern,
				    leaf->source_pattern &
					    ADD_SUB_IMMEDIATE_SOURCE_MASK,
				    "%s source fingerprint", leaf->source_name);
		KUNIT_ASSERT_EQ_MSG(test, ORLIX_TCTI_DECODE_ADD_SUB_IMMEDIATE,
				    decoded.decode_class, "%s", leaf->source_name);
		KUNIT_EXPECT_EQ(test, leaf->is_64bit, decoded.is_64bit);
		KUNIT_EXPECT_EQ(test, leaf->subtract, decoded.subtract);
		KUNIT_EXPECT_EQ(test, leaf->set_flags, decoded.set_flags);
		KUNIT_EXPECT_GT(test, leaf->source_ordinal, (u16)0);
	}
}

static void orlix_tcti_add_sub_immediate_all_legal_encodings(struct kunit *test)
{
	size_t leaf_index;

	for (leaf_index = 0;
	     leaf_index < ARRAY_SIZE(orlix_tcti_add_sub_immediate_leaves);
	     leaf_index++) {
		const struct orlix_tcti_add_sub_immediate_leaf *leaf =
			&orlix_tcti_add_sub_immediate_leaves[leaf_index];
		u32 variable;

		for (variable = 0;
		     variable <= ADD_SUB_IMMEDIATE_VARIABLE_MASK; variable++) {
			u32 instruction = leaf->source_pattern | variable;
			struct orlix_tcti_decoded_instruction decoded =
				orlix_tcti_decode_aarch64(instruction);

			if (decoded.decode_class != ORLIX_TCTI_DECODE_ADD_SUB_IMMEDIATE ||
			    decoded.is_64bit != leaf->is_64bit ||
			    decoded.subtract != leaf->subtract ||
			    decoded.set_flags != leaf->set_flags ||
			    decoded.shift != !!(variable & BIT(22)) ||
			    decoded.imm12 != ((variable >> 10) & 0xfffU) ||
			    decoded.rn != ((variable >> 5) & 0x1fU) ||
			    decoded.rd != (variable & 0x1fU)) {
				KUNIT_FAIL(test,
					   "%s legal encoding %#x decoded incorrectly",
					   leaf->source_name, instruction);
				return;
			}
			if ((variable & 0xfffU) == 0xfffU)
				cond_resched();
		}
	}
}

static unsigned long orlix_tcti_add_sub_immediate_map_program(
	struct kunit *test, u32 instruction)
{
	const u32 program[] = { instruction, ADD_SUB_IMMEDIATE_SVC };
	unsigned long mapped;
	int ret;

	KUNIT_ASSERT_NOT_NULL(test, current->mm);
	mapped = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));
	ret = orlix_tcti_write_user_data(current->mm, mapped, program,
				   sizeof(program));
	if (ret) {
		vm_munmap(mapped, PAGE_SIZE);
		KUNIT_FAIL(test, "could not write ADD/SUB program: %d", ret);
		return 0;
	}
	ret = sys_mprotect(mapped, PAGE_SIZE, PROT_READ | PROT_EXEC);
	if (ret) {
		vm_munmap(mapped, PAGE_SIZE);
		KUNIT_FAIL(test, "could not protect ADD/SUB program: %d", ret);
		return 0;
	}
	return mapped;
}

static unsigned long orlix_tcti_add_sub_immediate_expected_pstate(
	const struct orlix_tcti_add_sub_immediate_leaf *leaf, u64 left, u64 right,
	u64 result, unsigned long initial)
{
	u64 mask = leaf->is_64bit ? U64_MAX : U32_MAX;
	u64 sign = leaf->is_64bit ? BIT_ULL(63) : BIT_ULL(31);
	unsigned long flags = 0;

	if (!leaf->set_flags)
		return initial;
	left &= mask;
	right &= mask;
	result &= mask;
	if (result & sign)
		flags |= PSR_N_BIT;
	if (!result)
		flags |= PSR_Z_BIT;
	if (leaf->subtract ? left >= right :
	    ((__uint128_t)left + right) > mask)
		flags |= PSR_C_BIT;
	if (leaf->subtract ?
	    (((left ^ right) & (left ^ result) & sign) != 0) :
	    ((~(left ^ right) & (left ^ result) & sign) != 0))
		flags |= PSR_V_BIT;
	return (initial & ~ADD_SUB_IMMEDIATE_NZCV) | flags;
}

static void orlix_tcti_add_sub_immediate_run_with_expected_nzcv(
	struct kunit *test, const struct orlix_tcti_add_sub_immediate_leaf *leaf,
	bool shift, u16 immediate, u8 rn, u8 rd, u64 source, u64 sp,
	unsigned long pstate, bool has_expected_nzcv,
	unsigned long expected_nzcv)
{
	u32 instruction = orlix_tcti_add_sub_immediate_instruction(
		leaf, shift, immediate, rn, rd);
	unsigned long mapped = orlix_tcti_add_sub_immediate_map_program(
		test, instruction);
	u64 mask = leaf->is_64bit ? U64_MAX : U32_MAX;
	u64 right = (u64)immediate << (shift ? 12 : 0);
	unsigned int pass;

	KUNIT_ASSERT_NE(test, 0UL, mapped);
	for (pass = 0; pass < 2; pass++) {
		struct orlix_tcti_block *block;
		struct pt_regs regs = {};
		struct pt_regs before;
		struct orlix_tcti_result result;
		u64 left;
		u64 expected;
		unsigned long expected_pstate;
		unsigned int reg;

		for (reg = 0; reg < 31; reg++)
			regs.regs[reg] = 0x8100000000000000ULL + reg;
		if (rn < 31)
		regs.regs[rn] = source;
		regs.sp = sp;
		regs.pc = mapped;
		regs.pstate = PSR_MODE_EL0t | (pstate & ~PSR_MODE_MASK);
		regs.syscallno = NO_SYSCALL;
		KUNIT_ASSERT_EQ(test, (unsigned long)PSR_MODE_EL0t,
				regs.pstate & PSR_MODE_MASK);
		before = regs;
		left = (rn == 31 ? before.sp : before.regs[rn]) & mask;
		expected = (leaf->subtract ? left - right : left + right) & mask;
		expected_pstate = orlix_tcti_add_sub_immediate_expected_pstate(
			leaf, left, right, expected, before.pstate);
		if (has_expected_nzcv) {
			KUNIT_EXPECT_EQ(test, expected_nzcv,
					expected_pstate &
						ADD_SUB_IMMEDIATE_NZCV);
			expected_pstate =
				(before.pstate & ~ADD_SUB_IMMEDIATE_NZCV) |
				expected_nzcv;
		}

		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_ASSERT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
		KUNIT_EXPECT_EQ(test, 0L, result.status);
		KUNIT_EXPECT_EQ(test, mapped + sizeof(u32), result.pc);
		KUNIT_EXPECT_EQ(test, ADD_SUB_IMMEDIATE_SVC,
				result.instruction);
		if (!pass) {
			block = orlix_tcti_block_cache_lookup(
				current->mm, mapped,
				orlix_tcti_code_generation(current->mm));
			KUNIT_EXPECT_NOT_NULL(test, block);
			if (block)
				orlix_tcti_block_put(block);
		}
		for (reg = 0; reg < 31; reg++) {
			u64 expected_reg = before.regs[reg];

			if (rd == reg)
				expected_reg = expected;
			KUNIT_EXPECT_EQ_MSG(test, expected_reg, regs.regs[reg],
					    "%s x%u pass %u", leaf->source_name,
					    reg, pass);
		}
		KUNIT_EXPECT_EQ(test,
			(!leaf->set_flags && rd == 31) ? expected : before.sp,
			regs.sp);
		KUNIT_EXPECT_EQ(test, expected_pstate, regs.pstate);
		KUNIT_EXPECT_EQ(test, (unsigned long)PSR_MODE_EL0t,
				regs.pstate & PSR_MODE_MASK);
		KUNIT_EXPECT_EQ(test, mapped + sizeof(u32), regs.pc);
	}
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void orlix_tcti_add_sub_immediate_run(
	struct kunit *test, const struct orlix_tcti_add_sub_immediate_leaf *leaf,
	bool shift, u16 immediate, u8 rn, u8 rd, u64 source, u64 sp,
	unsigned long pstate)
{
	orlix_tcti_add_sub_immediate_run_with_expected_nzcv(
		test, leaf, shift, immediate, rn, rd, source, sp, pstate, false,
		0);
}

static void orlix_tcti_add_sub_immediate_production_path_arithmetic(
	struct kunit *test)
{
	static const struct {
		u64 source;
		u16 immediate;
		bool shift;
	} vectors[] = {
		{ 0, 0, false },
		{ 0, 1, false },
		{ U32_MAX, 1, false },
		{ BIT_ULL(31) - 1, 1, false },
		{ BIT_ULL(31), 1, false },
		{ U64_MAX, 1, false },
		{ BIT_ULL(63) - 1, 1, false },
		{ BIT_ULL(63), 1, false },
		{ 0x123456789abcdef0ULL, 0xfff, true },
	};
	size_t leaf_index;
	size_t vector_index;

	for (leaf_index = 0;
	     leaf_index < ARRAY_SIZE(orlix_tcti_add_sub_immediate_leaves);
	     leaf_index++)
		for (vector_index = 0; vector_index < ARRAY_SIZE(vectors);
		     vector_index++)
			orlix_tcti_add_sub_immediate_run(
				test, &orlix_tcti_add_sub_immediate_leaves[leaf_index],
				vectors[vector_index].shift,
				vectors[vector_index].immediate, 4, 7,
				vectors[vector_index].source,
				0x000000fffffff000ULL,
				ADD_SUB_IMMEDIATE_PSTATE_SEED);
}

static void orlix_tcti_add_sub_immediate_special_register_aliases(
	struct kunit *test)
{
	size_t leaf_index;

	for (leaf_index = 0;
	     leaf_index < ARRAY_SIZE(orlix_tcti_add_sub_immediate_leaves);
	     leaf_index++) {
		const struct orlix_tcti_add_sub_immediate_leaf *leaf =
			&orlix_tcti_add_sub_immediate_leaves[leaf_index];

		orlix_tcti_add_sub_immediate_run(test, leaf, false, 7, 31, 5,
					   0x1111111111111111ULL,
					   0x00000001fffffff0ULL,
					   ADD_SUB_IMMEDIATE_PSTATE_SEED);
		orlix_tcti_add_sub_immediate_run(test, leaf, false, 9, 31, 31,
					   0x2222222222222222ULL,
					   0x00000001fffffff0ULL,
					   ADD_SUB_IMMEDIATE_PSTATE_SEED);
		orlix_tcti_add_sub_immediate_run(test, leaf, true, 1, 6, 6,
					   0x8000000080001000ULL,
					   0x00000001fffffff0ULL,
					   ADD_SUB_IMMEDIATE_PSTATE_SEED);
	}
}

static void orlix_tcti_add_sub_immediate_cmn_cmp_nzcv_boundaries(
	struct kunit *test)
{
	static const struct {
		bool is_64bit;
		u64 source;
		u16 immediate;
		bool subtract;
		unsigned long expected_nzcv;
	} vectors[] = {
		/* CMN carry and signed-overflow boundaries. */
		{ false, U32_MAX, 1, false, PSR_Z_BIT | PSR_C_BIT },
		{ false, BIT_ULL(31) - 1, 1, false,
		  PSR_N_BIT | PSR_V_BIT },
		{ true, U64_MAX, 1, false, PSR_Z_BIT | PSR_C_BIT },
		{ true, BIT_ULL(63) - 1, 1, false,
		  PSR_N_BIT | PSR_V_BIT },
		/* CMP borrow and signed-overflow boundaries. */
		{ false, 0, 1, true, PSR_N_BIT },
		{ false, BIT_ULL(31), 1, true,
		  PSR_C_BIT | PSR_V_BIT },
		{ true, 0, 1, true, PSR_N_BIT },
		{ true, BIT_ULL(63), 1, true,
		  PSR_C_BIT | PSR_V_BIT },
	};
	static const size_t leaf_indices[] = { 1, 3, 5, 7 };
	size_t leaf_index;
	size_t vector_index;

	for (leaf_index = 0; leaf_index < ARRAY_SIZE(leaf_indices);
	     leaf_index++) {
		const struct orlix_tcti_add_sub_immediate_leaf *leaf =
			&orlix_tcti_add_sub_immediate_leaves[leaf_indices[leaf_index]];

		for (vector_index = 0; vector_index < ARRAY_SIZE(vectors);
		     vector_index++) {
			if (leaf->is_64bit != vectors[vector_index].is_64bit ||
			    leaf->subtract != vectors[vector_index].subtract)
				continue;
			/* CMN/CMP discard the result through Rd == 31. */
			orlix_tcti_add_sub_immediate_run_with_expected_nzcv(
				test, leaf, false,
				vectors[vector_index].immediate, 31, 31, 0,
				vectors[vector_index].source,
				ADD_SUB_IMMEDIATE_PSTATE_SEED, true,
				vectors[vector_index].expected_nzcv);
		}
	}
}

static void orlix_tcti_add_sub_immediate_source_mask_boundaries(
	struct kunit *test)
{
	static const u32 cssc_patterns[] = {
		0x11c00000U, 0x11c40000U, 0x11c80000U, 0x11cc0000U,
		0x91c00000U, 0x91c40000U, 0x91c80000U, 0x91cc0000U,
	};
	static const u32 mte_tagged_patterns[] = {
		0x91800000U, 0xd1800000U,
	};
	size_t leaf_index;
	size_t index;

	for (leaf_index = 0;
	     leaf_index < ARRAY_SIZE(orlix_tcti_add_sub_immediate_leaves);
	     leaf_index++) {
		const struct orlix_tcti_add_sub_immediate_leaf *leaf =
			&orlix_tcti_add_sub_immediate_leaves[leaf_index];
		u32 instruction = orlix_tcti_add_sub_immediate_instruction(
			leaf, false, 0x555, 7, 3);
		u8 bit;

		for (bit = 0; bit < 32; bit++) {
			const struct orlix_tcti_add_sub_immediate_leaf *neighbour;
			struct orlix_tcti_decoded_instruction decoded;
			u32 mutated;

			if (!(ADD_SUB_IMMEDIATE_SOURCE_MASK & BIT(bit)))
				continue;
			mutated = instruction ^ BIT(bit);
			neighbour = orlix_tcti_add_sub_immediate_source_leaf(mutated);
			decoded = orlix_tcti_decode_aarch64(mutated);
			if (!neighbour) {
				KUNIT_EXPECT_NE_MSG(test,
					ORLIX_TCTI_DECODE_ADD_SUB_IMMEDIATE,
					decoded.decode_class,
					"%s fixed-bit neighbour %#x rebound",
					leaf->source_name, mutated);
				continue;
			}
			KUNIT_EXPECT_PTR_NE(test, leaf, neighbour);
			KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_DECODE_ADD_SUB_IMMEDIATE,
				decoded.decode_class, "%s neighbour %#x",
				neighbour->source_name, mutated);
			KUNIT_EXPECT_EQ(test, neighbour->is_64bit,
				decoded.is_64bit);
			KUNIT_EXPECT_EQ(test, neighbour->subtract,
				decoded.subtract);
			KUNIT_EXPECT_EQ(test, neighbour->set_flags,
				decoded.set_flags);
		}
	}

	for (index = 0; index < ARRAY_SIZE(cssc_patterns); index++)
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_MIN_MAX_IMMEDIATE,
			orlix_tcti_decode_aarch64(cssc_patterns[index]).decode_class);
	for (index = 0; index < ARRAY_SIZE(mte_tagged_patterns); index++)
		KUNIT_EXPECT_NE(test, ORLIX_TCTI_DECODE_ADD_SUB_IMMEDIATE,
			orlix_tcti_decode_aarch64(mte_tagged_patterns[index]).decode_class);
}

static const char *orlix_tcti_add_sub_immediate_proof_id(
	const struct orlix_tcti_add_sub_immediate_leaf *leaf)
{
	if (!leaf->subtract)
		return leaf->set_flags ? "kunit:add-sub-immediate-adds" :
			"kunit:add-sub-immediate-add";
	return leaf->set_flags ? "kunit:add-sub-immediate-subs" :
		"kunit:add-sub-immediate-sub";
}

static const struct orlix_tcti_target_proof_registry_entry *
orlix_tcti_add_sub_immediate_registry_entry(const char *proof_id)
{
	const struct orlix_tcti_target_proof_registry_entry *entries;
	size_t count;
	size_t index;

	entries = orlix_tcti_target_proof_registry_entries(&count);
	for (index = 0; entries && index < count; index++)
		if (!strcmp(entries[index].id, proof_id))
			return &entries[index];
	return NULL;
}

static void orlix_tcti_add_sub_immediate_ingest(
	struct kunit *test,
	const struct orlix_tcti_add_sub_immediate_leaf *leaf,
	const char *case_name, struct orlix_tcti_native_observation *observation)
{
	const struct orlix_tcti_target_proof_registry_entry *entry =
		orlix_tcti_add_sub_immediate_registry_entry(
			orlix_tcti_add_sub_immediate_proof_id(leaf));
	const struct orlix_tcti_target_proof_binding *binding = NULL;
	struct orlix_tcti_target_kunit_provenance_identity provenance;
	struct orlix_tcti_target_native_ingestion_selector selector = {};
	struct orlix_tcti_target_proof_ingestion_ledger *ledger;
	struct orlix_tcti_target_native_result_record *record = NULL;
	enum orlix_tcti_target_proof_ingestion_error error;
	char kernel_identity[ORLIX_TCTI_TARGET_PROOF_BUILD_ID_MAX];
	size_t index;

	KUNIT_ASSERT_NOT_NULL(test, entry);
	for (index = 0; index < entry->binding_count; index++)
		if (entry->bindings[index].source_ordinal == leaf->source_ordinal) {
			KUNIT_ASSERT_PTR_EQ(test, binding, NULL);
			binding = &entry->bindings[index];
		}
	KUNIT_ASSERT_NOT_NULL(test, binding);
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_target_kunit_provenance_identity(entry, case_name,
			&provenance));
	scnprintf(kernel_identity, sizeof(kernel_identity), "%s|%s|%s",
		 init_utsname()->release, init_utsname()->version,
		 init_utsname()->machine);
	selector = (struct orlix_tcti_target_native_ingestion_selector) {
		.proof_id = entry->id,
		.classification_mask = entry->classification_mask,
		.condition_tcnd_hex = binding->condition_tcnd_hex,
		.kunit_source = provenance.source,
		.kunit_source_sha256 = provenance.source_sha256,
		.kunit_build_source = provenance.build_source,
		.kunit_build_source_sha256 = provenance.build_source_sha256,
		.kunit_suite = provenance.suite,
		.kunit_case = provenance.case_name,
		.executing_kernel_identity = kernel_identity,
	};
	ledger = orlix_tcti_target_proof_ingestion_ledger_create(1);
	KUNIT_ASSERT_NOT_NULL(test, ledger);
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_native_observation_export(observation, &record));
	KUNIT_ASSERT_NOT_NULL(test, record);
	KUNIT_EXPECT_EQ(test, 0, orlix_tcti_target_proof_ingest_native(
		ledger, record, &selector, &error));
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_TARGET_PROOF_INGEST_OK, error);
	orlix_tcti_target_proof_ingestion_ledger_destroy(ledger);
	orlix_tcti_target_native_result_record_destroy(record);
}

static void orlix_tcti_add_sub_immediate_seed_extended_state(void)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(current->thread.user_simd); index++)
		current->thread.user_simd[index] =
			0x2200000000000000ULL + index;
	current->thread.user_fpcr = 0x00400000;
	current->thread.user_fpsr = 0x1a;
	current->thread.user_simd_valid = 1;
	orlix_tcti_sve_state_reset(&current->thread.user_sve,
		current->thread.user_simd, ORLIX_TCTI_SVE_MIN_VL_BYTES);
	for (index = 0; index < sizeof(current->thread.user_sve.z); index++)
		((u8 *)current->thread.user_sve.z)[index] = (u8)(index * 11U + 5U);
	for (index = 0; index < sizeof(current->thread.user_sve.p); index++)
		((u8 *)current->thread.user_sve.p)[index] = (u8)(index * 3U + 7U);
	for (index = 0; index < sizeof(current->thread.user_sve.ffr); index++)
		current->thread.user_sve.ffr[index] = (u8)(index * 13U + 1U);
}

static void orlix_tcti_add_sub_immediate_fp_capture(
	struct orlix_tcti_native_fp_simd_state *state)
{
	memset(state, 0, sizeof(*state));
	memcpy(state->v, current->thread.user_simd, sizeof(state->v));
	state->fpcr = current->thread.user_fpcr;
	state->fpsr = current->thread.user_fpsr;
	state->valid = current->thread.user_simd_valid;
}

static void orlix_tcti_add_sub_immediate_sve_capture(
	struct orlix_tcti_native_sve_state *state)
{
	*state = (struct orlix_tcti_native_sve_state) {
		.vl_bytes = current->thread.user_sve.vl_bytes,
		.z = (const u8 *)current->thread.user_sve.z,
		.p = (const u8 *)current->thread.user_sve.p,
		.ffr = current->thread.user_sve.ffr,
		.valid = current->thread.user_sve.valid,
	};
}

static struct orlix_tcti_native_sme_state
orlix_tcti_add_sub_immediate_sme_absent(void)
{
	return (struct orlix_tcti_native_sme_state) {
		.valid = true,
		.production_available = true,
	};
}

static void orlix_tcti_add_sub_immediate_native_records(
	struct kunit *test, enum orlix_tcti_native_obligation obligation,
	const char *case_name)
{
	struct orlix_tcti_native_observation_spec *spec =
		kunit_kzalloc(test, sizeof(*spec), GFP_KERNEL);
	size_t leaf_index;

	KUNIT_ASSERT_NOT_NULL(test, spec);
	for (leaf_index = 0;
	     leaf_index < ARRAY_SIZE(orlix_tcti_add_sub_immediate_leaves);
	     leaf_index++) {
		const struct orlix_tcti_add_sub_immediate_leaf *leaf =
			&orlix_tcti_add_sub_immediate_leaves[leaf_index];
		struct orlix_tcti_native_observation *observation;
		struct orlix_tcti_native_fp_simd_state fp_simd;
		struct orlix_tcti_native_sve_state sve;
		struct orlix_tcti_native_sme_state sme;
		struct orlix_tcti_native_memory_state memory;
		struct pt_regs regs = {};
		struct pt_regs expected;
		u8 before_memory[sizeof(u32) * 2];
		u8 after_memory[sizeof(before_memory)];
		u32 instruction = orlix_tcti_add_sub_immediate_instruction(
			leaf, false, 1, 4, 7);
		unsigned long mapped =
			orlix_tcti_add_sub_immediate_map_program(test, instruction);
		u64 mask = leaf->is_64bit ? U64_MAX : U32_MAX;
		u64 left = 0x7fffffff;
		u64 value;
		unsigned int reg;

		for (reg = 0; reg < 31; reg++)
			regs.regs[reg] = 0x8200000000000000ULL + reg;
		regs.regs[4] = left;
		regs.sp = 0x00000002fffffff0ULL;
		regs.pc = mapped;
		regs.pstate = PSR_MODE_EL0t | ADD_SUB_IMMEDIATE_NZCV;
		regs.syscallno = NO_SYSCALL;
		expected = regs;
		value = (leaf->subtract ? left - 1 : left + 1) & mask;
		expected.regs[7] = value;
		expected.pc += sizeof(u32);
		expected.pstate = orlix_tcti_add_sub_immediate_expected_pstate(
			leaf, left, 1, value, expected.pstate);
		*spec = (struct orlix_tcti_native_observation_spec) {
			.source_ordinal = leaf->source_ordinal,
			.obligation = obligation,
			.result = {
				.reason = ORLIX_TCTI_EXIT_SYSCALL,
				.status = 0,
				.fault_access = ORLIX_TCTI_ACCESS_FETCH,
				.pc = expected.pc,
				.instruction = ADD_SUB_IMMEDIATE_SVC,
			},
		};
		orlix_tcti_native_gpr_capture(&spec->gpr, &expected);
		orlix_tcti_add_sub_immediate_seed_extended_state();
		KUNIT_ASSERT_EQ(test, 0, orlix_tcti_read_user_data(current->mm,
			mapped, before_memory, sizeof(before_memory)));
		if (obligation == ORLIX_TCTI_NATIVE_OBLIGATION_MEMORY)
			spec->expected.memory =
				(struct orlix_tcti_native_memory_state) {
					.address = mapped,
					.size = sizeof(before_memory),
					.bytes = before_memory,
				};
		else if (obligation == ORLIX_TCTI_NATIVE_OBLIGATION_FP_SIMD) {
			orlix_tcti_add_sub_immediate_fp_capture(&fp_simd);
			spec->expected.fp_simd = fp_simd;
		} else if (obligation == ORLIX_TCTI_NATIVE_OBLIGATION_SVE) {
			orlix_tcti_add_sub_immediate_sve_capture(&sve);
			spec->expected.sve = sve;
		} else if (obligation == ORLIX_TCTI_NATIVE_OBLIGATION_SME) {
			sme = orlix_tcti_add_sub_immediate_sme_absent();
			spec->expected.sme = sme;
		}
		observation = orlix_tcti_native_observation_create(spec);
		KUNIT_ASSERT_NOT_NULL(test, observation);
		KUNIT_ASSERT_EQ(test, 0, orlix_tcti_native_observation_execute(
			observation, current, &regs, current->mm));
		if (obligation == ORLIX_TCTI_NATIVE_OBLIGATION_MEMORY) {
			KUNIT_ASSERT_EQ(test, 0, orlix_tcti_read_user_data(current->mm,
				mapped, after_memory, sizeof(after_memory)));
			memory = (struct orlix_tcti_native_memory_state) {
				.address = mapped,
				.size = sizeof(after_memory),
				.bytes = after_memory,
			};
			KUNIT_ASSERT_EQ(test, 0,
				orlix_tcti_native_observation_add_memory(observation,
					&memory));
		} else if (obligation == ORLIX_TCTI_NATIVE_OBLIGATION_FP_SIMD) {
			orlix_tcti_add_sub_immediate_fp_capture(&fp_simd);
			KUNIT_ASSERT_EQ(test, 0,
				orlix_tcti_native_observation_add_fp_simd(observation,
					&fp_simd));
		} else if (obligation == ORLIX_TCTI_NATIVE_OBLIGATION_SVE) {
			orlix_tcti_add_sub_immediate_sve_capture(&sve);
			KUNIT_ASSERT_EQ(test, 0,
				orlix_tcti_native_observation_add_sve(observation, &sve));
		} else if (obligation == ORLIX_TCTI_NATIVE_OBLIGATION_SME) {
			sme = orlix_tcti_add_sub_immediate_sme_absent();
			KUNIT_ASSERT_EQ(test, 0,
				orlix_tcti_native_observation_add_sme(observation, &sme));
		}
		KUNIT_ASSERT_EQ(test, 0,
			orlix_tcti_native_observation_compare(observation));
		orlix_tcti_add_sub_immediate_ingest(test, leaf, case_name,
			observation);
		orlix_tcti_native_observation_destroy(observation);
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
	}
}

#define ADD_SUB_IMMEDIATE_NATIVE_CASE(name, obligation) \
	static void name(struct kunit *test) \
	{ \
		orlix_tcti_add_sub_immediate_native_records(test, obligation, #name); \
	}
ADD_SUB_IMMEDIATE_NATIVE_CASE(orlix_tcti_add_sub_immediate_native_decode_records,
	ORLIX_TCTI_NATIVE_OBLIGATION_DECODE)
ADD_SUB_IMMEDIATE_NATIVE_CASE(orlix_tcti_add_sub_immediate_native_legal_records,
	ORLIX_TCTI_NATIVE_OBLIGATION_LEGAL_ENCODING)
ADD_SUB_IMMEDIATE_NATIVE_CASE(
	orlix_tcti_add_sub_immediate_native_register_records,
	ORLIX_TCTI_NATIVE_OBLIGATION_GPR)
ADD_SUB_IMMEDIATE_NATIVE_CASE(orlix_tcti_add_sub_immediate_native_pc_records,
	ORLIX_TCTI_NATIVE_OBLIGATION_RESULT)
ADD_SUB_IMMEDIATE_NATIVE_CASE(orlix_tcti_add_sub_immediate_native_flags_records,
	ORLIX_TCTI_NATIVE_OBLIGATION_FLAGS)
ADD_SUB_IMMEDIATE_NATIVE_CASE(orlix_tcti_add_sub_immediate_native_memory_records,
	ORLIX_TCTI_NATIVE_OBLIGATION_MEMORY)
ADD_SUB_IMMEDIATE_NATIVE_CASE(
	orlix_tcti_add_sub_immediate_native_fp_simd_records,
	ORLIX_TCTI_NATIVE_OBLIGATION_FP_SIMD)
ADD_SUB_IMMEDIATE_NATIVE_CASE(orlix_tcti_add_sub_immediate_native_sve_records,
	ORLIX_TCTI_NATIVE_OBLIGATION_SVE)
ADD_SUB_IMMEDIATE_NATIVE_CASE(orlix_tcti_add_sub_immediate_native_sme_records,
	ORLIX_TCTI_NATIVE_OBLIGATION_SME)
#undef ADD_SUB_IMMEDIATE_NATIVE_CASE

static struct kunit_case orlix_tcti_add_sub_immediate_test_cases[] = {
	KUNIT_CASE(orlix_tcti_add_sub_immediate_source_bindings),
	KUNIT_CASE(orlix_tcti_add_sub_immediate_all_legal_encodings),
	KUNIT_CASE(orlix_tcti_add_sub_immediate_production_path_arithmetic),
	KUNIT_CASE(orlix_tcti_add_sub_immediate_special_register_aliases),
	KUNIT_CASE(orlix_tcti_add_sub_immediate_cmn_cmp_nzcv_boundaries),
	KUNIT_CASE(orlix_tcti_add_sub_immediate_source_mask_boundaries),
	KUNIT_CASE(orlix_tcti_add_sub_immediate_native_decode_records),
	KUNIT_CASE(orlix_tcti_add_sub_immediate_native_legal_records),
	KUNIT_CASE(orlix_tcti_add_sub_immediate_native_register_records),
	KUNIT_CASE(orlix_tcti_add_sub_immediate_native_pc_records),
	KUNIT_CASE(orlix_tcti_add_sub_immediate_native_flags_records),
	KUNIT_CASE(orlix_tcti_add_sub_immediate_native_memory_records),
	KUNIT_CASE(orlix_tcti_add_sub_immediate_native_fp_simd_records),
	KUNIT_CASE(orlix_tcti_add_sub_immediate_native_sve_records),
	KUNIT_CASE(orlix_tcti_add_sub_immediate_native_sme_records),
	{}
};

struct kunit_suite orlix_tcti_add_sub_immediate_test_suite = {
	.name = "orlix-tcti-add-sub-immediate",
	.test_cases = orlix_tcti_add_sub_immediate_test_cases,
};

kunit_test_suite(orlix_tcti_add_sub_immediate_test_suite);
